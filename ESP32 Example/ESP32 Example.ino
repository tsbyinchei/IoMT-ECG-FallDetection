#include "esp_idf_version.h"
#include "mqtt_client.h"
#if __has_include("esp_crt_bundle.h")
#include "esp_crt_bundle.h"
#define HAS_ESP_CRT_BUNDLE 1
#else
#define HAS_ESP_CRT_BUNDLE 0
#endif
#include <U8g2lib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <Wire.h>

// Cấu hình Telegram Bot bảo mật (ưu tiên đọc secrets.h)
#if __has_include("secrets.h")
#include "secrets.h"
#elif __has_include("secrets_example.h")
#include "secrets_example.h"
#endif

// ==================== CẤU HÌNH PHẦN CỨNG GATEWAY ESP32 ====================
#define BUZZER_PIN 23   // Active Buzzer cảnh báo biến cố y sinh
#define OLED_SDA_PIN 21 // Chân I2C SDA cho màn hình OLED SH1106
#define OLED_SCL_PIN 22 // Chân I2C SCL cho màn hình OLED SH1106
#define BUTTON_PIN 0 // Nút BOOT có sẵn trên kit ESP32 (GPIO 0) bấm chuyển trang

// Khởi tạo màn hình OLED SH1106 I2C 128x64
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE,
                                        /* clock=*/OLED_SCL_PIN,
                                        /* data=*/OLED_SDA_PIN);

// ==================== CẤU HÌNH CHẾ ĐỘ HOẠT ĐỘNG ====================
// Đặt 'false' khi test Local tại bàn (không cần kết nối máy chủ 1Panel, tránh
// báo lỗi rc=-2) Đặt 'true' khi chạy chính thức đẩy dữ liệu lên Ubuntu Server
// (1Panel: EMQX Broker)
#define ENABLE_MQTT true

// 1. Wi-Fi Router kết nối ra Internet / Mạng nội bộ tới Ubuntu Server
#ifndef SECRET_ROUTER_SSID
#define SECRET_ROUTER_SSID "YOUR_WIFI_SSID"
#endif
#ifndef SECRET_ROUTER_PASS
#define SECRET_ROUTER_PASS "YOUR_WIFI_PASSWORD"
#endif
const char *ROUTER_SSID = SECRET_ROUTER_SSID;
const char *ROUTER_PASS = SECRET_ROUTER_PASS;

// 2. Mạng SoftAP phát riêng cho Node cảm biến ESP32-C3
const char *AP_SSID = "BIOMED_GW";
const unsigned int UDP_PORT = 4210;

// 3. MQTT Broker qua Cloudflare Tunnel (Toàn cầu) hoặc IP LAN
#ifndef SECRET_MQTT_URI
#define SECRET_MQTT_URI "wss://mqtt.yourdomain.com/mqtt"
#endif
#ifndef SECRET_MQTT_USER
#define SECRET_MQTT_USER "YOUR_MQTT_USERNAME"
#endif
#ifndef SECRET_MQTT_PASS
#define SECRET_MQTT_PASS "YOUR_MQTT_PASSWORD"
#endif

const char *MQTT_URI = SECRET_MQTT_URI;
const char *MQTT_USER = SECRET_MQTT_USER; // Điền nếu broker yêu cầu xác thực
const char *MQTT_PASS = SECRET_MQTT_PASS; // Điền mật khẩu nếu có
// Chứng chỉ gốc Let's Encrypt (ISRG Root X1 & X2) dùng cho Cloudflare WSS (mqtt.yourdomain.com)
static const char *ISRG_ROOT_CA =
// ISRG Root X1 (RSA)
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n"
// ISRG Root X2 (ECDSA)
"-----BEGIN CERTIFICATE-----\n"
"MIIEcDCCAligAwIBAgIQbI8dxyfHEX97r4U6yYD5zTANBgkqhkiG9w0BAQsFADBP\n"
"MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy\n"
"Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNjA1MTMwMDAwMDBa\n"
"Fw0zMjA5MDIyMzU5NTlaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5l\n"
"dCBTZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgy\n"
"MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0H\n"
"ttwW+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7\n"
"AlF9ItgKbppbd9/w+kHsOdx1ymgHDB/qo4H1MIHyMA4GA1UdDwEB/wQEAwIBBjAd\n"
"BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd\n"
"BgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwHwYDVR0jBBgwFoAUebRZ5nu2\n"
"5eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAChhZodHRw\n"
"Oi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcGA1UdHwQg\n"
"MB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcNAQELBQAD\n"
"ggIBAD2/e9frmMxNpCV03qUHegg+MV2wz9644YoXdqtH8RyWYcBO7xfjjGEXdU1e\n"
"/o0OkEFiynUCOSIk/vLLo7ttz6CPAeNlWfC0XNkoGeWgK6jjXvozBaGuGH5n0Ufo\n"
"shMeWTuURqNN5G00sSXDTBrpp2+mgvdZQjb8K11TYMA25QA+YHNfbIEL0BniAhKS\n"
"2gsnJjSzrdZLI+EZ7SEyqdR2rkjd1KutLDU+n3TFyxjniZVGur4YlhMP3mY/dV95\n"
"IruAkkjOZier6hGBdEgZXXvaCz9u9iVEadsIE75pAGL8oHV5vxdARDiotRpul1IN\n"
"/UZwzAbrfUFcw1HkAcYD/mlZfnQ2ieCF2MS7j3Vhv7JPDKp45fmykmzYNSrumRW0\n"
"upFFKDBOoF7hsOb7oLyHS+Uft6jOUfOrogj8YUx38hKb2K20r42OgsSdDdxdeYWc\n"
"MS3Sb6mwJeSZEYxJ2gaXnDSPaKhhrNkYwljyVQyr4Nq+MEJytXNTnHqaAcrNwZlV\n"
"pcJL1KBnMrMjP7eanvUwL3FYj3cF17jtboLt7gLoi4+2rWZFvn+w54jmd/FIuhhZ\n"
"cEaU/wvU6BUNMtcVquVGHp7itQeDth5j+XL3j4WJ2SABwzUl6OeYdgpIt/ITZa+p\n"
"TT0mQ/r5XyA4MEAiabn7XJjvCERlF2dcn2wqJw+CreTkkQ2R\n"
"-----END CERTIFICATE-----\n";

const char *MQTT_CLIENT_ID = "ESP32_Biomed_Gateway";

const char *TOPIC_DATA =
    "biomed/patient/data"; // Dữ liệu đo đạc (ECG, Temp, SMV, BPM)
const char *TOPIC_ALERT =
    "biomed/patient/alert"; // Cảnh báo khẩn (Fall, LeadOff)
const char *TOPIC_CMD =
    "biomed/gateway/cmd";   // Lệnh điều khiển Gateway từ xa (Mute/Unmute/Test)
const char *TOPIC_STATUS =
    "biomed/gateway/status"; // Trạng thái Gateway (Online/Muted/Active)

// 4. Telegram Bot API
#ifndef TELEGRAM_BOT_TOKEN
#define TELEGRAM_BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
#endif
#ifndef TELEGRAM_CHAT_ID
#define TELEGRAM_CHAT_ID "YOUR_TELEGRAM_CHAT_ID"
#endif

// 5. Zalo Bot API (Zalo Bot Platform HTTP API)
#ifndef ZALO_BOT_TOKEN
#define ZALO_BOT_TOKEN "YOUR_ZALO_BOT_TOKEN"
#endif
#ifndef ZALO_CHAT_ID
#define ZALO_CHAT_ID "YOUR_ZALO_CHAT_ID"
#endif

// ==================== ĐỊNH NGHĨA GÓI TIN ĐỒNG BỘ TỪ ESP32-C3
// ====================
typedef struct __attribute__((packed)) struct_message {
  uint16_t ecgSamples[25]; // 25 mẫu ECG thô (ADC 12-bit: 0-4095 @ 250Hz)
  float bodyTemp;          // Nhiệt độ cơ thể (°C)
  float smv;               // Signal Magnitude Vector gia tốc (g)
  uint8_t leadsOff;        // 1: Hở điện cực | 0: Bình thường
  uint8_t fallDetected;    // 1: Phát hiện ngã  | 0: Bình thường
} struct_message;

struct_message incomingData;
WiFiUDP udp;

esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;

// ==================== BIẾN BỘ ĐỆM ĐỒ THỊ & TÍNH BPM TẠI BIÊN (EDGE)
// ====================
#define WAVE_WIDTH 128
uint8_t waveBuffer[WAVE_WIDTH]; // Bộ đệm vẽ đồ thị sóng ECG mini cuộn trên OLED
uint8_t waveWriteIdx = 0;

int edgeBpm = 0;
unsigned long lastPeakTime = 0;
uint16_t dynamicMax = 2500;
uint16_t dynamicMin = 1800;
unsigned long lastOledRefresh = 0;
unsigned long lastMqttReconnectAttempt = 0;
unsigned long totalPacketsReceived = 0;
uint16_t totalFallsCount = 0;
bool prevFallState = false;
bool buzzerMuted = false; // Cờ tắt còi báo động từ xa hoặc tại chỗ
bool fallAlarmActive = false; // Chốt giữ cảnh báo ngã khẩn cấp: Hú liên tục cho đến khi người dùng bấm tắt

// ==================== QUẢN LÝ CHUYỂN TRANG MÀN HÌNH ====================
#define TOTAL_PAGES 4
uint8_t currentPage = 0;
// Trang 0: Đồ thị sóng ECG & Nhịp tim (ECG Waveform Focus)
// Trang 1: Tổng quan chỉ số Sinh hiệu (Clinical Vitals: BPM, Temp, Status)
// Trang 2: Giám sát Gia tốc & Té ngã (Motion & Fall Level Bar)
// Trang 3: Chẩn đoán Mạng & Hệ thống (System & Network Diagnostics)

void handleButton() {
  static unsigned long lastDebounceTime = 0;
  static int lastButtonState = HIGH;
  int reading = digitalRead(BUTTON_PIN);

  if (reading == LOW && lastButtonState == HIGH) {
    if (millis() - lastDebounceTime > 200) {
      lastDebounceTime = millis();
      // Nếu đang trong trạng thái báo động ngã hoặc còi đang kêu -> Bấm nút BOOT để tắt còi tại chỗ
      if (fallAlarmActive || (!buzzerMuted && (incomingData.fallDetected || incomingData.leadsOff))) {
        fallAlarmActive = false;
        buzzerMuted = true;
        digitalWrite(BUZZER_PIN, LOW);
        Serial.println("\n[LOCAL] >>> Da bam nut BOOT xac nhan bien co & tat coi bao dong tai cho! <<<");
        if (mqtt_connected) {
          esp_mqtt_client_publish(mqtt_client, TOPIC_STATUS, "{\"status\":\"ok\",\"buzzer\":\"muted\"}", 0, 1, 0);
        }
      } else {
        currentPage = (currentPage + 1) % TOTAL_PAGES;
        Serial.printf("[OLED] Chuyen sang Trang %d/%d\n", currentPage + 1,
                      TOTAL_PAGES);
      }
    }
  }
  lastButtonState = reading;
}

// ==================== BỘ LỌC SỐ ECG 50Hz NOTCH + PAN-TOMPKINS ====================
struct EcgFilterBiquad {
  float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
  float x1 = 2048.0f, x2 = 2048.0f, y1 = 2048.0f, y2 = 2048.0f;

  void init(float f0, float fs, float Q) {
    float w0 = 2.0f * 3.14159265f * f0 / fs;
    float c = cosf(w0);
    float s = sinf(w0);
    float alpha = s / (2.0f * Q);
    float a0 = 1.0f + alpha;
    b0 = 1.0f / a0;
    b1 = -2.0f * c / a0;
    b2 = 1.0f / a0;
    a1 = -2.0f * c / a0;
    a2 = (1.0f - alpha) / a0;
  }

  float step(float x) {
    float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x;
    y2 = y1; y1 = y;
    return y;
  }

  void reset(float val = 2048.0f) {
    x1 = val; x2 = val; y1 = val; y2 = val;
  }
};

static EcgFilterBiquad gwNotch;
static bool gwFilterInited = false;
static float gwBaseline = 2048.0f;
static float gwLp = 0.0f;

float filterGatewayEcg(uint16_t raw, bool leadsOff) {
  if (!gwFilterInited) {
    gwNotch.init(50.0f, 250.0f, 7.0f);
    gwFilterInited = true;
  }
  if (leadsOff || raw < 50 || raw >= 4080) {
    gwNotch.reset(2048.0f);
    gwBaseline = 2048.0f;
    gwLp = 0.0f;
    return 0.0f;
  }
  // 1. Notch filter 50Hz (Khử triệt để nhiễu điện lưới 50Hz)
  float n = gwNotch.step((float)raw);
  // 2. High-pass filter 0.5Hz (Khử trôi đường đẳng điện do hô hấp)
  gwBaseline += (n - gwBaseline) * 0.005f;
  float hp = n - gwBaseline;
  // 3. Low-pass filter 35Hz (Khử nhiễu rung cơ EMG & gai ADC)
  gwLp += (hp - gwLp) * 0.38f;
  return gwLp;
}

// ==================== THUẬT TOÁN ĐO NHỊP TIM TẠI BIÊN (EDGE BPM - PAN-TOMPKINS)
// ==================== Tần số lấy mẫu 250Hz -> Mỗi mẫu = 4ms
unsigned long edgeSampleCounter = 0;
unsigned long lastPeakSample = 0;
static float ptHp1 = 0, ptHp2 = 0, ptHp3 = 0, ptHp4 = 0;
#define MWI_LEN 25
static float mwiBuf[MWI_LEN] = {0};
static int mwiIdx = 0;
static float mwiSum = 0;
static float mwiPeak = 200.0f;
static bool qrsAbove = false;

void processEdgeBPM(float hp) {
  edgeSampleCounter++;

  // 1. Đạo hàm bậc 1 5 điểm nhấn mạnh sườn dốc QRS: deriv = (2*x[n] + x[n-1] - x[n-3] - 2*x[n-4]) / 8
  float deriv = (2.0f * hp + ptHp1 - ptHp3 - 2.0f * ptHp4) * 0.125f;
  ptHp4 = ptHp3; ptHp3 = ptHp2; ptHp2 = ptHp1; ptHp1 = hp;
  float dsq = deriv * deriv;

  // 2. Tích phân cửa sổ động (Moving Window Integration - 100ms)
  mwiSum += dsq - mwiBuf[mwiIdx];
  mwiBuf[mwiIdx] = dsq;
  mwiIdx = (mwiIdx + 1) % MWI_LEN;
  float mwi = mwiSum / (float)MWI_LEN;

  // 3. Ngưỡng thích ứng biên độ đỉnh QRS
  mwiPeak *= 0.999f;
  if (mwi > mwiPeak) mwiPeak = mwi;

  float thr = 0.35f * mwiPeak;
  bool isAbove = (mwi > thr);
  bool rising = isAbove && !qrsAbove;
  qrsAbove = isAbove;

  // 4. Phát hiện đỉnh R với thời gian trơ sinh lý 380ms (95 mẫu @ 250Hz chuẩn Pan-Tompkins & MATLAB)
  if (rising && (edgeSampleCounter - lastPeakSample > 95) && edgeSampleCounter > 250) {
    unsigned long rrSamples = edgeSampleCounter - lastPeakSample;
    lastPeakSample = edgeSampleCounter;

    // Khoảng thời gian RR tính bằng mili-giây (mỗi mẫu = 4ms)
    unsigned long rrMs = rrSamples * 4;

    // Lọc dải sinh lý chuẩn: 400ms đến 1400ms (43 đến 150 BPM)
    if (rrMs >= 400 && rrMs <= 1400) {
      static uint16_t recentRRs[7];
      static uint8_t rrCount = 0;
      static uint8_t rrIdx = 0;

      recentRRs[rrIdx] = (uint16_t)rrMs;
      rrIdx = (rrIdx + 1) % 7;
      if (rrCount < 7) rrCount++;

      // Bộ lọc trung vị (Median Filter) chuẩn thuật toán MATLAB
      uint16_t sorted[7];
      for (uint8_t k = 0; k < rrCount; k++) sorted[k] = recentRRs[k];
      for (uint8_t i = 0; i < rrCount - 1; i++) {
        for (uint8_t j = i + 1; j < rrCount; j++) {
          if (sorted[j] < sorted[i]) {
            uint16_t temp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = temp;
          }
        }
      }
      uint16_t medianRR = sorted[rrCount / 2];
      int calculatedBpm = 60000 / medianRR;

      if (edgeBpm == 0) {
        edgeBpm = calculatedBpm;
      } else {
        // Làm mịn nhẹ 80% lịch sử, 20% giá trị mới để nhịp tim không nhảy loạn
        edgeBpm = (edgeBpm * 13 + calculatedBpm * 3) / 16;
      }
    }
  }

  // Quá 3.5 giây không có nhịp tim (875 mẫu) -> nhịp tim về 0
  if (edgeSampleCounter - lastPeakSample > 875) {
    edgeBpm = 0;
  }
}

// ==================== CẬP NHẬT ĐỒ THỊ SÓNG OLED MINI (DYNAMIC AUTO-RANGING)
// ====================
void updateWaveformBuffer(float filteredSample) {
  static float oledPeak = 300.0f;
  float a = fabsf(filteredSample);
  if (a > oledPeak) oledPeak = a;
  oledPeak *= 0.998f;
  if (oledPeak < 80.0f) oledPeak = 80.0f; // Ngưỡng sàn

  // Đường đẳng điện OLED ở Y = 36 (giữa khung đồ thị 16 - 56)
  int y = 36 - (int)(filteredSample * 18.0f / oledPeak);
  waveBuffer[waveWriteIdx] = (uint8_t)constrain(y, 16, 56);
  waveWriteIdx = (waveWriteIdx + 1) % WAVE_WIDTH;
}

// ==================== HIỂN THỊ MÀN HÌNH OLED THEO TỪNG TRANG
// ====================
void drawOLED() {
  u8g2.clearBuffer();

  // ==================== TRANG 0: ĐỒ THỊ SÓNG ECG CHUYÊN SÂU
  // ====================
  if (currentPage == 0) {
    u8g2.setFont(u8g2_font_6x10_tf);

    // Dòng thông tin trên cùng (BPM | Temp | SMV)
    if (incomingData.leadsOff) {
      u8g2.drawStr(0, 9, "HR: --");
      u8g2.drawStr(50, 9, "[LEAD OFF]");
    } else if (edgeBpm > 0) {
      char hrStr[16];
      snprintf(hrStr, sizeof(hrStr), "%dBPM", edgeBpm);
      u8g2.drawStr(0, 9, hrStr);
      char tempStr[16];
      snprintf(tempStr, sizeof(tempStr), "%.1f*C", incomingData.bodyTemp);
      u8g2.drawStr(50, 9, tempStr);
    } else {
      u8g2.drawStr(0, 9, "HR: DETECT...");
    }

    char smvStr[10];
    snprintf(smvStr, sizeof(smvStr), "%.1fg", incomingData.smv);
    u8g2.drawStr(102, 9, smvStr);

    u8g2.drawHLine(0, 12, 128);

    // Vẽ đồ thị sóng ECG dạng cuộn
    for (int x = 1; x < WAVE_WIDTH; x++) {
      int idx1 = (waveWriteIdx + x - 1) % WAVE_WIDTH;
      int idx2 = (waveWriteIdx + x) % WAVE_WIDTH;
      u8g2.drawLine(x - 1, waveBuffer[idx1], x, waveBuffer[idx2]);
    }

    // Dòng chân trang
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(0, 63, "P1:ECG WAVE (Bam BOOT chuyen)");
  }

  // ==================== TRANG 1: TỔNG QUAN CHỈ SỐ SINH HIỆU
  // ====================
  else if (currentPage == 1) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 9, "CLINICAL VITALS [P2/4]");
    u8g2.drawHLine(0, 12, 128);

    // Nhịp tim
    u8g2.setFont(u8g2_font_7x14B_tf);
    if (incomingData.leadsOff) {
      u8g2.drawStr(5, 27, "HR  : -- BPM");
    } else if (edgeBpm > 0) {
      char hrBuf[24];
      snprintf(hrBuf, sizeof(hrBuf), "HR  : %d BPM", edgeBpm);
      u8g2.drawStr(5, 27, hrBuf);
    } else {
      u8g2.drawStr(5, 27, "HR  : CHO DO...");
    }

    // Thân nhiệt
    char tBuf[24];
    snprintf(tBuf, sizeof(tBuf), "TEMP: %.1f *C", incomingData.bodyTemp);
    u8g2.drawStr(5, 43, tBuf);

    // Đánh giá tình trạng lâm sàng
    u8g2.setFont(u8g2_font_6x10_tf);
    if (incomingData.leadsOff) {
      u8g2.drawStr(5, 58, "TT: HO DIEN CUC!");
    } else if (incomingData.bodyTemp >= 38.0f) {
      u8g2.drawStr(5, 58, "TT: SOT CAO (>38*C)");
    } else if (incomingData.bodyTemp >= 37.5f) {
      u8g2.drawStr(5, 58, "TT: SOT NHE");
    } else if (edgeBpm > 120) {
      u8g2.drawStr(5, 58, "TT: NHIP NHANH!");
    } else if (edgeBpm > 0 && edgeBpm < 50) {
      u8g2.drawStr(5, 58, "TT: NHIP CHAM!");
    } else {
      u8g2.drawStr(5, 58, "TT: ON DINH (NORMAL)");
    }
  }

  // ==================== TRANG 2: GIÁM SÁT GIA TỐC & TÉ NGÃ
  // ====================
  else if (currentPage == 2) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 9, "MOTION & FALL [P3/4]");
    u8g2.drawHLine(0, 12, 128);

    char smvFull[24];
    snprintf(smvFull, sizeof(smvFull), "Gia toc SMV: %.2f g", incomingData.smv);
    u8g2.drawStr(5, 24, smvFull);

    // Thanh đo lực gia tốc đồ họa (Level Bar từ 0 đến 4.0g)
    u8g2.drawFrame(5, 28, 118, 10);
    int barWidth =
        map(constrain((int)(incomingData.smv * 100), 0, 400), 0, 400, 0, 116);
    u8g2.drawBox(6, 29, barWidth, 8);

    // Vạch đánh dấu ngưỡng ngã 2.2g
    int threshX = 6 + (int)(116.0 * 2.2 / 4.0);
    u8g2.drawVLine(threshX, 26, 14);

    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(5, 45, "0g");
    u8g2.drawStr(threshX - 8, 45, "2.2g (Nga)");
    u8g2.drawStr(108, 45, "4.0g");

    // Thống kê số lần ngã
    u8g2.setFont(u8g2_font_6x10_tf);
    char fallCountBuf[30];
    snprintf(fallCountBuf, sizeof(fallCountBuf), "So lan nga: %u lan",
             totalFallsCount);
    u8g2.drawStr(5, 59, fallCountBuf);
  }

  // ==================== TRANG 3: CHẨN ĐOÁN HỆ THỐNG IOT ====================
  else if (currentPage == 3) {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 9, "SYSTEM DIAG [P4/4]");
    u8g2.drawHLine(0, 11, 128);

    u8g2.setFont(u8g2_font_5x8_tf);
    char wifiBuf[32];
    if (WiFi.status() == WL_CONNECTED) {
      snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: %s (%s)", ROUTER_SSID, WiFi.localIP().toString().c_str());
    } else {
      snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: MAT KET NOI (%s)", ROUTER_SSID);
    }
    u8g2.drawStr(0, 22, wifiBuf);

#if ENABLE_MQTT
    u8g2.drawStr(0, 33,
                 mqtt_connected ? "Cloud: WSS ONLINE [OK]" : "Cloud: CONNECTING...");
#else
    u8g2.drawStr(0, 33, "Cloud: LOCAL OFFLINE");
#endif

    char pkgStr[32];
    snprintf(pkgStr, sizeof(pkgStr), "RX: %lu | Nga: %u lan", totalPacketsReceived, totalFallsCount);
    u8g2.drawStr(0, 44, pkgStr);

    u8g2.drawStr(0, 55, "SoftAP: BIOMED_GW:4210");
    u8g2.drawStr(0, 64, "Node C3: 192.168.4.2");
  }

  // ==================== POPUP CẢNH BÁO KHẨN CẤP (HIỂN THỊ TRÊN MỌI TRANG)
  // ====================
  if (fallAlarmActive || incomingData.fallDetected) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(4, 14, 120, 36);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(4, 14, 120, 36);
    u8g2.drawFrame(6, 16, 116, 32);
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(12, 30, "! CANH BAO NGA !");
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(16, 42, "Bam BOOT de tat coi");
  }

  u8g2.sendBuffer();
}

// ==================== ĐIỀU KHIỂN CÒI BÁO ĐỘNG BUZZER (GPIO 23)
// ====================
void handleBuzzer() {
  // Nếu người dùng đã kích hoạt tắt còi (tại chỗ qua BOOT hoặc từ xa qua Web) -> Im lặng
  if (buzzerMuted) {
    digitalWrite(BUZZER_PIN, LOW);
    return;
  }

  // 1. CẢNH BÁO TÉ NGÃ KHẨN CẤP (ƯU TIÊN CAO NHẤT): HÚ LIÊN TỤC KHÔNG DỪNG CHO ĐẾN KHI BẤM NÚT
  if (fallAlarmActive || incomingData.fallDetected) {
    digitalWrite(BUZZER_PIN, HIGH);
    return;
  }

  // 2. CẢNH BÁO NHỊP TIM BẤT THƯỜNG: Bíp kép chu kỳ 1s
  if (edgeBpm > 0 && (edgeBpm > 125 || edgeBpm < 45)) {
    unsigned long cycle = millis() % 1000;
    if (cycle < 100 || (cycle > 200 && cycle < 300)) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
    return;
  }

  // 3. CẢNH BÁO TUỘT ĐIỆN CỰC: Bíp ngắt quãng nhẹ (80ms ON mỗi 1.5s)
  if (incomingData.leadsOff) {
    unsigned long cycle = millis() % 1500;
    if (cycle < 80) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
    return;
  }

  // Bình thường: Tắt còi
  digitalWrite(BUZZER_PIN, LOW);
}

// ==================== KẾT NỐI VÀ XỬ LÝ SỰ KIỆN MQTT CLOUD (WSS)
// ====================
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_BEFORE_CONNECT:
    Serial.println("\n[MQTT] >> Dang thiet lap bat tay TLS va ket noi Broker Cloud...");
    break;
  case MQTT_EVENT_CONNECTED:
    mqtt_connected = true;
    Serial.println("\n=======================================================");
    Serial.println("[MQTT] >>> KET NOI THANH CONG TOI BROKER CLOUD (WSS)! <<<");
    Serial.println("=======================================================\n");
    esp_mqtt_client_publish(mqtt_client, TOPIC_STATUS,
                            "{\"status\":\"online\",\"buzzer\":\"active\"}", 0, 1, 0);
    esp_mqtt_client_subscribe(mqtt_client, TOPIC_CMD, 1);
    Serial.printf("[MQTT] Da subscribe lang nghe lenh Gateway tai: %s\n", TOPIC_CMD);
    break;
  case MQTT_EVENT_DISCONNECTED:
    mqtt_connected = false;
    Serial.println("\n[MQTT] Mat ket noi Broker. Dang tu dong ket noi lai...");
    break;
  case MQTT_EVENT_DATA: {
    char topicBuf[64] = {0};
    int tLen = event->topic_len < (int)sizeof(topicBuf) - 1 ? event->topic_len : (int)sizeof(topicBuf) - 1;
    strncpy(topicBuf, event->topic, tLen);

    char dataBuf[128] = {0};
    int dLen = event->data_len < (int)sizeof(dataBuf) - 1 ? event->data_len : (int)sizeof(dataBuf) - 1;
    strncpy(dataBuf, event->data, dLen);

    Serial.printf("\n[MQTT CMD] Nhan lenh dieu khien tai [%s]: %s\n", topicBuf, dataBuf);

    if (strstr(dataBuf, "mute") != NULL && strstr(dataBuf, "unmute") == NULL) {
      fallAlarmActive = false;
      buzzerMuted = true;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("[REMOTE] >>> DA TAT COI BAO DONG (BUZZER MUTED) TU XA! <<<");
      esp_mqtt_client_publish(mqtt_client, TOPIC_STATUS, "{\"status\":\"ok\",\"buzzer\":\"muted\"}", 0, 1, 0);
    } else if (strstr(dataBuf, "unmute") != NULL) {
      buzzerMuted = false;
      Serial.println("[REMOTE] >>> DA BAT LAI COI BAO DONG (BUZZER ACTIVE)! <<<");
      esp_mqtt_client_publish(mqtt_client, TOPIC_STATUS, "{\"status\":\"ok\",\"buzzer\":\"active\"}", 0, 1, 0);
    } else if (strstr(dataBuf, "beep") != NULL || strstr(dataBuf, "test") != NULL) {
      Serial.println("[REMOTE] >>> TEST COI CHIP 150ms! <<<");
      digitalWrite(BUZZER_PIN, HIGH);
      delay(150);
      digitalWrite(BUZZER_PIN, LOW);
      esp_mqtt_client_publish(mqtt_client, TOPIC_STATUS, "{\"status\":\"ok\",\"buzzer\":\"tested\"}", 0, 1, 0);
    }
    break;
  }
  case MQTT_EVENT_ERROR:
    Serial.println("\n----------------- [MQTT CHI TIET LOI] -----------------");
    if (event->error_handle) {
      Serial.printf(" - Error Type: %d (1: TCP/TLS Transport, 2: Refused, 3: Protocol, 4: Connack)\n", event->error_handle->error_type);
      Serial.printf(" - Connect Return Code: %d\n", event->error_handle->connect_return_code);
      Serial.printf(" - ESP-TLS Last Error: 0x%x\n", event->error_handle->esp_tls_last_esp_err);
      Serial.printf(" - TLS Stack Error: 0x%x\n", event->error_handle->esp_tls_stack_err);
      Serial.printf(" - TLS Cert Verify Flags: 0x%x\n", event->error_handle->esp_tls_cert_verify_flags);
      Serial.printf(" - Socket Errno: %d\n", event->error_handle->esp_transport_sock_errno);
      if (event->error_handle->esp_tls_cert_verify_flags != 0) {
        Serial.println("   [GIAI MA LOI CHUNG CHI TLS]:");
        if (event->error_handle->esp_tls_cert_verify_flags & 0x01) Serial.println("    * MBEDTLS_X509_BADCERT_EXPIRED: Chung chi het han");
        if (event->error_handle->esp_tls_cert_verify_flags & 0x02) Serial.println("    * MBEDTLS_X509_BADCERT_REVOKED: Chung chi bi thu hoi");
        if (event->error_handle->esp_tls_cert_verify_flags & 0x04) Serial.println("    * MBEDTLS_X509_BADCERT_CN_MISMATCH: Ten mien SNI khong khop");
        if (event->error_handle->esp_tls_cert_verify_flags & 0x08) Serial.println("    * MBEDTLS_X509_BADCERT_NOT_TRUSTED: Root CA khong tin cay");
        if (event->error_handle->esp_tls_cert_verify_flags & 0x20) Serial.println("    * MBEDTLS_X509_BADCERT_FUTURE: Gio he thong ESP32 nho hon ngay cap chung chi!");
      }
    }
    Serial.println("-------------------------------------------------------");
    break;
  default:
    break;
  }
}

void initAndStartMQTT() {
#if ENABLE_MQTT
  if (mqtt_client != NULL)
    return; // Đã khởi chạy rồi

  Serial.println("\n[MQTT DIAGNOSTIC] Bat dau khoi tao client...");
  Serial.printf(" - Free Heap: %d bytes (Max Alloc: %d bytes)\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  time_t now = time(nullptr);
  Serial.printf(" - System Time Epoch: %ld\n", (long)now);

  esp_mqtt_client_config_t mqtt_cfg = {};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  mqtt_cfg.broker.address.uri = MQTT_URI;
  mqtt_cfg.broker.verification.certificate = ISRG_ROOT_CA;
  mqtt_cfg.broker.verification.common_name = "mqtt.tsbyin.dev";
  mqtt_cfg.broker.verification.skip_cert_common_name_check = false;
  if (strlen(MQTT_USER) > 0) {
    mqtt_cfg.credentials.username = MQTT_USER;
    mqtt_cfg.credentials.authentication.password = MQTT_PASS;
  }
  mqtt_cfg.credentials.client_id = MQTT_CLIENT_ID;
  mqtt_cfg.network.disable_auto_reconnect = false;
  mqtt_cfg.network.timeout_ms = 15000;
  mqtt_cfg.buffer.size = 2048;
  mqtt_cfg.buffer.out_size = 2048;
#else
  mqtt_cfg.uri = MQTT_URI;
  mqtt_cfg.cert_pem = ISRG_ROOT_CA;
  mqtt_cfg.skip_cert_common_name_check = false;
  if (strlen(MQTT_USER) > 0) {
    mqtt_cfg.username = MQTT_USER;
    mqtt_cfg.password = MQTT_PASS;
  }
  mqtt_cfg.client_id = MQTT_CLIENT_ID;
  mqtt_cfg.disable_auto_reconnect = false;
  mqtt_cfg.network_timeout_ms = 15000;
  mqtt_cfg.buffer_size = 2048;
  mqtt_cfg.out_buffer_size = 2048;
#endif

  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  if (!mqtt_client) {
    Serial.println("\n[MQTT LOI] esp_mqtt_client_init that bai!\n");
    return;
  }
  esp_mqtt_client_register_event(mqtt_client,
                                 MQTT_EVENT_ANY,
                                 mqtt_event_handler, NULL);
  esp_err_t startErr = esp_mqtt_client_start(mqtt_client);
  Serial.printf("[MQTT] Khoi chay tien trinh ket noi Cloud Broker: %s (Status: %d)\n", MQTT_URI, startErr);
#endif
}

// ==================== XUẤT BẢN DỮ LIỆU SANG MQTT ====================
void publishDataMQTT() {
#if ENABLE_MQTT
  static unsigned long lastWarnTime = 0;
  if (!mqtt_client || !mqtt_connected) {
    if (millis() - lastWarnTime >= 5000) {
      lastWarnTime = millis();
      if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("\n[MQTT CHUA DAY] Wi-Fi chua ket noi Router '%s' (Trang thai: %d). Dang doi...\n", ROUTER_SSID, WiFi.status());
      } else {
        Serial.printf("\n[MQTT CHUA DAY] Wi-Fi da ket noi (IP: %s) nhung Broker Cloud (%s) chua Connected! Dang ket noi...\n",
                      WiFi.localIP().toString().c_str(), MQTT_URI);
      }
    }
    return;
  }

  char jsonBuffer[512];
  int offset =
      snprintf(jsonBuffer, sizeof(jsonBuffer),
               "{\"temp\":%.1f,\"smv\":%.2f,\"leadsOff\":%d,\"fall\":%d,"
               "\"bpm\":%d,\"ecg\":[",
               incomingData.bodyTemp, incomingData.smv, incomingData.leadsOff,
               incomingData.fallDetected, edgeBpm);

  for (int i = 0; i < 25; i++) {
    offset += snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset,
                       (i == 24) ? "%u" : "%u,", incomingData.ecgSamples[i]);
  }
  snprintf(jsonBuffer + offset, sizeof(jsonBuffer) - offset, "]}");

  esp_mqtt_client_publish(mqtt_client, TOPIC_DATA, jsonBuffer, 0, 0, 0);

  static unsigned long totalPubCount = 0;
  totalPubCount++;
  if (totalPubCount == 1 || totalPubCount % 50 == 0) {
    Serial.printf("\n[MQTT] >>> Da day %lu goi tin sinh hieu len Web Dashboard Cloud! <<<\n", totalPubCount);
  }

  if (incomingData.fallDetected || incomingData.leadsOff) {
    char alertBuf[128];
    snprintf(alertBuf, sizeof(alertBuf),
             "{\"alert\":\"%s\",\"smv\":%.2f,\"temp\":%.1f,\"time\":%lu}",
             incomingData.fallDetected ? "FALL_DETECTED" : "LEADS_OFF",
             incomingData.smv, incomingData.bodyTemp, millis());
    esp_mqtt_client_publish(mqtt_client, TOPIC_ALERT, alertBuf, 0, 1, 0);
  }
#endif
}

// ==================== GỬI CẢNH BÁO TÉ NGÃ QUA TELEGRAM BOT
// ====================
String urlEncodeString(const char *msg) {
  String encoded = "";
  char c;
  while ((c = *msg++) != 0) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

void sendTelegramFallAlert(float smv, float temp, int bpm) {
  // Chỉ gửi qua Internet nếu kit Gateway đang kết nối Router Wi-Fi
  if (WiFi.status() != WL_CONNECTED)
    return;

  WiFiClientSecure client;
  client
      .setInsecure(); // Bỏ qua chứng chỉ SSL để gửi tin nhắn nhanh không bị trễ
  client.setTimeout(4000);

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("[TELEGRAM] Khong the ket noi api.telegram.org");
    return;
  }

  char textBuf[256];
  snprintf(textBuf, sizeof(textBuf),
           "🚨 [CẢNH BÁO TÉ NGÃ KHẨN CẤP - GATEWAY ESP32]!\n"
           "Bệnh nhân vừa bị té ngã!\n"
           "- Lực va đập SMV: %.2fg\n"
           "- Nhịp tim hiện tại: %d BPM\n"
           "- Thân nhiệt: %.1f*C\n"
           "- Thiết bị: IoMT Gateway\n"
           "Cần kiểm tra ngay lập tức!",
           smv, bpm, temp);

  String url = "/bot" + String(TELEGRAM_BOT_TOKEN) +
               "/sendMessage?chat_id=" + String(TELEGRAM_CHAT_ID) +
               "&text=" + urlEncodeString(textBuf);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: api.telegram.org\r\n" + "Connection: close\r\n\r\n");

  Serial.println(
      "[TELEGRAM] >>> Da gui tin nhan canh bao te nga thanh cong! <<<");
  client.stop();
}

void sendZaloFallAlert(float smv, float temp, int bpm) {
  // Chỉ gửi qua Internet nếu kit Gateway đang kết nối Router Wi-Fi
  if (WiFi.status() != WL_CONNECTED)
    return;
  if (strlen(ZALO_BOT_TOKEN) == 0 || strlen(ZALO_CHAT_ID) == 0 ||
      strcmp(ZALO_BOT_TOKEN, "YOUR_ZALO_BOT_TOKEN") == 0) {
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Bỏ qua chứng chỉ SSL để gửi tin nhắn nhanh không trễ
  client.setTimeout(4000);

  if (!client.connect("bot-api.zaloplatforms.com", 443)) {
    Serial.println("[ZALO] Khong the ket noi bot-api.zaloplatforms.com");
    return;
  }

  char textBuf[256];
  snprintf(textBuf, sizeof(textBuf),
           "🚨 [CẢNH BÁO TÉ NGÃ KHẨN CẤP - GATEWAY ESP32]!\n"
           "Bệnh nhân vừa bị té ngã!\n"
           "- Lực va đập SMV: %.2fg\n"
           "- Nhịp tim hiện tại: %d BPM\n"
           "- Thân nhiệt: %.1f*C\n"
           "- Thiết bị: IoMT Gateway\n"
           "Cần kiểm tra ngay lập tức!",
           smv, bpm, temp);

  String url = "/bot" + String(ZALO_BOT_TOKEN) +
               "/sendMessage?chat_id=" + String(ZALO_CHAT_ID) +
               "&text=" + urlEncodeString(textBuf);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: bot-api.zaloplatforms.com\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("[ZALO] >>> Da gui tin nhan canh bao te nga qua Zalo Bot! <<<");
  client.stop();
}

// ==================== IN DỮ LIỆU SERIAL (CHO MATLAB OFFLINE & DEBUG)
// ==================== IN DỮ LIỆU SERIAL (CHO MATLAB OFFLINE & DEBUG) ====================
void outputSerialForMatlab() {
#if DEBUG_MUTE_DATA_STREAM
  // Tắt 10 dòng $DATA/giây để Serial Monitor không bị tràn trôi log
  // Chỉ in 1 dòng tóm tắt trạng thái cảm biến mỗi 5 giây
  static unsigned long lastSensorLog = 0;
  if (millis() - lastSensorLog >= 5000) {
    lastSensorLog = millis();
    Serial.printf("[C3 SENSOR OK] Temp=%.1f*C | HR=%d bpm | SMV=%.2fg | Fall=%d | LeadsOff=%d (Goi #%lu)\n",
                  incomingData.bodyTemp, edgeBpm, incomingData.smv,
                  incomingData.fallDetected, incomingData.leadsOff, totalPacketsReceived);
  }
  return;
#endif

  Serial.print("$DATA,");
  Serial.print(incomingData.bodyTemp, 1);
  Serial.print(",");
  Serial.print(incomingData.smv, 2);
  Serial.print(",");
  Serial.print(incomingData.leadsOff);
  Serial.print(",");
  Serial.print(incomingData.fallDetected);
  Serial.print(",");
  Serial.print(edgeBpm);
  for (int i = 0; i < 25; i++) {
    Serial.print(",");
    Serial.print(incomingData.ecgSamples[i]);
  }
  Serial.println();
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Thiết lập mức độ log ESP-IDF chi tiết để hiển thị chi tiết tiến trình bắt tay TLS/MQTT
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
  esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
  esp_log_level_set("esp-tls-mbedtls", ESP_LOG_VERBOSE);
  esp_log_level_set("mbedtls", ESP_LOG_VERBOSE);

  Serial.println("\n==========================================");
  Serial.println("  ESP32 BIOMEDICAL GATEWAY DANG KHOI DONG");
  Serial.println("==========================================");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(BUZZER_PIN, LOW);

  // Khởi động màn hình OLED SH1106
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(10, 25, "BIOMED GATEWAY");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 45, "Khoi dong he thong...");
  u8g2.sendBuffer();

  // Khởi tạo bộ đệm đồ thị sóng ECG
  for (int i = 0; i < WAVE_WIDTH; i++) {
    waveBuffer[i] = 36;
  }

  // Cấu hình Wi-Fi kép (WIFI_AP_STA): vừa thu UDP vừa đẩy Internet
#if ENABLE_MQTT
  WiFi.mode(WIFI_AP_STA);
  Serial.printf("\n[WiFi] Dang ket noi Router: %s", ROUTER_SSID);
  WiFi.begin(ROUTER_SSID, ROUTER_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 25) {
    delay(300);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Da ket noi Router! IP: %s (Kenh: %d | RSSI: %d dBm)\n",
                  WiFi.localIP().toString().c_str(), WiFi.channel(), WiFi.RSSI());
    WiFi.setDNS(IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
    IPAddress resolvedIP;
    if (WiFi.hostByName("mqtt.tsbyin.dev", resolvedIP)) {
      Serial.printf("[DNS] Giai ma mqtt.tsbyin.dev -> %s\n", resolvedIP.toString().c_str());
    } else {
      Serial.println("[DNS CANH BAO] Chua giai ma duoc domain mqtt.tsbyin.dev");
    }
    // Cài đặt mốc thời gian hệ thống chuẩn xác (Tháng 10/2026 ~ 1791558000)
    // Chứng chỉ SSL của domain có hiệu lực từ 21/08/2026 đến 19/11/2026.
    struct timeval tv = { .tv_sec = 1791558000 };
    settimeofday(&tv, NULL);
    configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
    Serial.print("[TIME] Dang dong bo thoi gian thuc tu NTP");
    time_t now = time(nullptr);
    int ntpWait = 0;
    while (now < 1791500000 && ntpWait < 10) {
      delay(200);
      Serial.print(".");
      now = time(nullptr);
      ntpWait++;
    }
    Serial.printf("\n[TIME] Thoi gian he thong hien tai Epoch: %ld\n", (long)now);
    initAndStartMQTT();
  } else {
    Serial.printf("\n[WiFi CANH BAO] Chua ket noi duoc Router '%s'. Gateway se tiep tuc thu lai tu dong trong loop()!\n", ROUTER_SSID);
  }
#else
  // Chế độ Local: Chỉ phát SoftAP, không mất thời gian tìm Router
  WiFi.mode(WIFI_AP);
  Serial.println(
      "\n[WiFi] Dang chay che do LOCAL TEST (Tien trinh MQTT da tam tat)");
#endif

  // Phát SoftAP cho Node ESP32-C3
  // Dùng kênh sóng của Router nếu đã kết nối để tránh xung đột RF radio
  int apChannel = (WiFi.status() == WL_CONNECTED) ? WiFi.channel() : 1;
  WiFi.softAP(AP_SSID, NULL, apChannel);
  Serial.printf("[AP] Gateway phat SoftAP: %s (Kenh: %d | IP: %s)\n",
                AP_SSID, apChannel, WiFi.softAPIP().toString().c_str());

  // Mở cổng UDP lắng nghe gói tin từ ESP32-C3
  udp.begin(UDP_PORT);
  Serial.printf("[UDP] Dang lang nghe goi tin tai cong %u...\n", UDP_PORT);

  u8g2.clearBuffer();
  u8g2.drawStr(12, 35, "Cho tin hieu C3...");
  u8g2.sendBuffer();
}

// ==================== LOOP ====================
void loop() {
  // 1. Kiểm tra nút nhấn BOOT để chuyển trang OLED
  handleButton();

  // 2. Quản lý kết nối Wi-Fi & MQTT (chỉ kích hoạt khi ENABLE_MQTT = true)
#if ENABLE_MQTT
  static unsigned long lastWifiRetry = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetry >= 10000) {
      lastWifiRetry = millis();
      Serial.printf("[WiFi RETRY] Dang ket noi lai Router Wi-Fi '%s' ...\n", ROUTER_SSID);
      WiFi.begin(ROUTER_SSID, ROUTER_PASS);
    }
  } else {
    if (mqtt_client == NULL) {
      Serial.printf("[WiFi] Da ket noi Router! IP: %s. Bat dau khoi chay MQTT...\n",
                    WiFi.localIP().toString().c_str());
      struct timeval tv = { .tv_sec = 1791558000 };
      settimeofday(&tv, NULL);
      configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
      initAndStartMQTT();
    }
  }
#endif

  // 3. Lắng nghe và đọc gói tin UDP từ ESP32-C3
  int packetSize = udp.parsePacket();
  if (packetSize >= sizeof(incomingData)) {
    udp.read((char *)&incomingData, sizeof(incomingData));
    totalPacketsReceived++;

    // Đếm số lần ngã và gửi cảnh báo Telegram khẩn cấp (chỉ tăng khi cờ chuyển
    // từ 0 lên 1)
    if (incomingData.fallDetected && !prevFallState) {
      totalFallsCount++;
      sendTelegramFallAlert(incomingData.smv, incomingData.bodyTemp, edgeBpm);
      sendZaloFallAlert(incomingData.smv, incomingData.bodyTemp, edgeBpm);
    }

    // Khi người bệnh đã ổn định (hết ngã), tự động nhả cờ buzzerMuted để sẵn sàng cho lần sau
    if (!incomingData.fallDetected && prevFallState && buzzerMuted) {
      buzzerMuted = false;
      Serial.println("[BUZZER] Tu dong tai kich hoat coi sau khi ket thuc bien co.");
      if (mqtt_connected) {
        esp_mqtt_client_publish(mqtt_client, TOPIC_STATUS,
                                "{\"status\":\"ok\",\"buzzer\":\"active\"}", 0, 1, 0);
      }
    }
    prevFallState = incomingData.fallDetected;

    // Xử lý từng mẫu trong 25 mẫu ECG qua bộ lọc số 50Hz Notch + Pan-Tompkins
    for (int i = 0; i < 25; i++) {
      float filtered = filterGatewayEcg(incomingData.ecgSamples[i], incomingData.leadsOff);
      if (!incomingData.leadsOff) {
        processEdgeBPM(filtered);
      } else {
        edgeBpm = 0; // Khi hở dây, nhịp tim về 0
      }
      if (i % 3 == 0) {
        updateWaveformBuffer(filtered);
      }
    }

    // Xuất dữ liệu ra Serial phục vụ MATLAB và quan sát
    outputSerialForMatlab();

    // Đẩy MQTT (nếu bật)
    publishDataMQTT();
  }

  // 4. Cập nhật màn hình OLED định kỳ (50ms = 20 FPS)
  if (millis() - lastOledRefresh >= 50) {
    lastOledRefresh = millis();
    drawOLED();
  }

  // 5. Điều khiển còi chíp cảnh báo Buzzer
  handleBuzzer();
}
