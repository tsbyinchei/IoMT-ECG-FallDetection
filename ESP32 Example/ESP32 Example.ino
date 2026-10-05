#include "esp_idf_version.h"
#include "mqtt_client.h"
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
const char *MQTT_CLIENT_ID = "ESP32_Biomed_Gateway";

const char *TOPIC_DATA =
    "biomed/patient/data"; // Dữ liệu đo đạc (ECG, Temp, SMV, BPM)
const char *TOPIC_ALERT =
    "biomed/patient/alert"; // Cảnh báo khẩn (Fall, LeadOff)

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
      currentPage = (currentPage + 1) % TOTAL_PAGES;
      Serial.printf("[OLED] Chuyen sang Trang %d/%d\n", currentPage + 1,
                    TOTAL_PAGES);
    }
  }
  lastButtonState = reading;
}

// ==================== THUẬT TOÁN ĐO NHỊP TIM TẠI BIÊN (EDGE BPM)
// ==================== Tần số lấy mẫu 250Hz -> Mỗi mẫu = 4ms
unsigned long edgeSampleCounter = 0;
unsigned long lastPeakSample = 0;
uint16_t prevSample1 = 3400;
uint16_t prevSample2 = 3400;
int slopeThreshold = 80;

void processEdgeBPM(uint16_t sample) {
  edgeSampleCounter++;

  // Tính độ dốc đạo hàm bậc 1 của sóng QRS: slope = x[n] - x[n-2]
  int slope = (int)sample - (int)prevSample2;
  prevSample2 = prevSample1;
  prevSample1 = sample;

  // Thích ứng ngưỡng độ dốc (Slope Adaptive Threshold)
  if (slope > slopeThreshold) {
    slopeThreshold = (slopeThreshold * 7 + slope) / 8;
  } else {
    slopeThreshold =
        (slopeThreshold * 511 + 60) / 512; // Hạ dần về ngưỡng sàn 60
  }

  // Phát hiện đỉnh sóng R:
  // - Vượt ngưỡng độ dốc sườn QRS nhọn
  // - Thời gian trơ sinh lý (Refractory Period): 380ms = 95 mẫu @ 250Hz để LOẠI
  // BỎ HOÀN TOÀN SÓNG T
  if (slope > slopeThreshold && (edgeSampleCounter - lastPeakSample > 95)) {
    unsigned long rrSamples = edgeSampleCounter - lastPeakSample;
    lastPeakSample = edgeSampleCounter;

    // Khoảng thời gian RR tính bằng mili-giây (mỗi mẫu = 4ms)
    unsigned long rrMs = rrSamples * 4;

    // Lọc dải sinh lý người bình thường: 45 BPM (1333ms) đến 140 BPM (428ms)
    if (rrMs >= 428 && rrMs <= 1333) {
      int calculatedBpm = 60000 / rrMs;
      if (edgeBpm == 0) {
        edgeBpm = calculatedBpm;
      } else {
        // Thuật toán chống nhảy vọt (Outlier Rejection):
        // Nếu nhịp tính toán lệch quá 20 BPM so với nhịp nền (thường do co
        // cơ/rung lắc MPU tạo đỉnh giả)
        // -> Lọc bỏ đỉnh giả đó, không cho nhịp tim nhảy vọt
        if (abs(calculatedBpm - edgeBpm) <= 18) {
          edgeBpm = (edgeBpm * 7 + calculatedBpm) / 8;
        } else {
          // Nếu lệch nhiều, chỉ dịch chuyển rất từ từ (1/16) để bám theo nhịp
          // tim thực
          edgeBpm = (edgeBpm * 15 + calculatedBpm) / 16;
        }
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
void updateWaveformBuffer(uint16_t rawSample) {
  // Tự động thích ứng với dải tín hiệu thực tế (Ví dụ: 3200 - 4095)
  static uint16_t sigMin = 3200;
  static uint16_t sigMax = 3800;

  if (rawSample < sigMin && rawSample > 200)
    sigMin = rawSample;
  if (rawSample > sigMax && rawSample <= 4095)
    sigMax = rawSample;

  // Co giãn ngưỡng từ từ để bám sát nhịp thở và đường đẳng điện
  sigMin = (sigMin * 63 + 3200) / 64;
  sigMax = (sigMax * 63 + 3900) / 64;

  int span = sigMax - sigMin;
  if (span < 150)
    span = 150; // Bảo vệ chống chia cho 0

  // Ánh xạ tín hiệu vừa vặn vào khung đồ thị OLED (Y: 54 ở dưới, 16 ở trên)
  int mappedY =
      54 -
      (int)((long)(constrain(rawSample, sigMin, sigMax) - sigMin) * 38 / span);
  waveBuffer[waveWriteIdx] = (uint8_t)constrain(mappedY, 16, 56);
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

    // Vạch đánh dấu ngưỡng ngã 2.5g
    int threshX = 6 + (int)(116.0 * 2.5 / 4.0);
    u8g2.drawVLine(threshX, 26, 14);

    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawStr(5, 45, "0g");
    u8g2.drawStr(threshX - 8, 45, "2.5g (Nga)");
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
    u8g2.drawHLine(0, 12, 128);

    u8g2.setFont(u8g2_font_6x10_tf);
#if ENABLE_MQTT
    u8g2.drawStr(0, 24,
                 mqtt_connected ? "Cloud: WSS ONLINE" : "Cloud: CONNECTING...");
#else
    u8g2.drawStr(0, 24, "Che do: LOCAL OFFLINE");
#endif

    char pkgStr[30];
    snprintf(pkgStr, sizeof(pkgStr), "Goi tin RX: %lu", totalPacketsReceived);
    u8g2.drawStr(0, 37, pkgStr);

    u8g2.drawStr(0, 50, "SoftAP: BIOMED_GW");
    u8g2.drawStr(0, 63, "Node C3: 192.168.4.2");
  }

  // ==================== POPUP CẢNH BÁO KHẨN CẤP (HIỂN THỊ TRÊN MỌI TRANG)
  // ====================
  if (incomingData.fallDetected) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(4, 18, 120, 28);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(4, 18, 120, 28);
    u8g2.drawFrame(6, 20, 116, 24);
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(10, 36, "! FALL DETECTED !");
  }

  u8g2.sendBuffer();
}

// ==================== ĐIỀU KHIỂN CÒI BÁO ĐỘNG BUZZER (GPIO 23)
// ====================
void handleBuzzer() {
  static unsigned long lastBeep = 0;
  static bool beepState = false;

  if (incomingData.fallDetected) {
    // Báo động ngã khẩn cấp: Kêu dồn dập (100ms ON / 100ms OFF)
    if (millis() - lastBeep >= 100) {
      lastBeep = millis();
      beepState = !beepState;
      digitalWrite(BUZZER_PIN, beepState ? HIGH : LOW);
    }
  } else if (incomingData.leadsOff) {
    // Cảnh báo tuột điện cực: Bíp ngắt quãng nhẹ (80ms ON mỗi 1.2s)
    unsigned long cycle = millis() % 1200;
    if (cycle < 80) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else if (edgeBpm > 0 && (edgeBpm > 125 || edgeBpm < 45)) {
    // Cảnh báo nhịp tim bất thường: Bíp kép chu kỳ 1s
    unsigned long cycle = millis() % 1000;
    if (cycle < 100 || (cycle > 200 && cycle < 300)) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ==================== KẾT NỐI VÀ XỬ LÝ SỰ KIỆN MQTT CLOUD (WSS)
// ====================
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    mqtt_connected = true;
    Serial.println(
        "\n[MQTT] >>> KET NOI THANH CONG TOI BROKER CLOUD (WSS)! <<<");
    esp_mqtt_client_publish(mqtt_client, "biomed/status",
                            "ESP32 Gateway Online", 0, 1, 0);
    break;
  case MQTT_EVENT_DISCONNECTED:
    mqtt_connected = false;
    Serial.println("[MQTT] Mat ket noi Broker. Dang tu dong ket noi lai...");
    break;
  case MQTT_EVENT_ERROR:
    Serial.println("[MQTT] Bao loi ket noi MQTT WSS");
    break;
  default:
    break;
  }
}

void initAndStartMQTT() {
#if ENABLE_MQTT
  if (mqtt_client != NULL)
    return; // Đã khởi chạy rồi

  esp_mqtt_client_config_t mqtt_cfg = {};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  mqtt_cfg.broker.address.uri = MQTT_URI;
  mqtt_cfg.broker.verification.skip_cert_common_name_check = true;
  if (strlen(MQTT_USER) > 0) {
    mqtt_cfg.credentials.username = MQTT_USER;
    mqtt_cfg.credentials.authentication.password = MQTT_PASS;
  }
  mqtt_cfg.credentials.client_id = MQTT_CLIENT_ID;
  mqtt_cfg.network.disable_auto_reconnect = false;
#else
  mqtt_cfg.uri = MQTT_URI;
  mqtt_cfg.skip_cert_common_name_check = true;
  if (strlen(MQTT_USER) > 0) {
    mqtt_cfg.username = MQTT_USER;
    mqtt_cfg.password = MQTT_PASS;
  }
  mqtt_cfg.client_id = MQTT_CLIENT_ID;
  mqtt_cfg.disable_auto_reconnect = false;
#endif

  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client,
                                 (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqtt_client);
  Serial.printf("[MQTT] Khoi chay client ket noi Cloud: %s\n", MQTT_URI);
#endif
}

// ==================== XUẤT BẢN DỮ LIỆU SANG MQTT ====================
void publishDataMQTT() {
#if ENABLE_MQTT
  if (!mqtt_client || !mqtt_connected)
    return;

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
           "Cần kiểm tra người bệnh ngay lập tức!",
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
           "Cần kiểm tra người bệnh ngay lập tức!",
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
// ====================
void outputSerialForMatlab() {
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
  Serial.printf("[WiFi] Dang ket noi vao Router: %s...\n", ROUTER_SSID);
  WiFi.begin(ROUTER_SSID, ROUTER_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 15) {
    delay(300);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Da ket noi Router! IP: %s\n",
                  WiFi.localIP().toString().c_str());
    initAndStartMQTT();
  }
#else
  // Chế độ Local: Chỉ phát SoftAP, không mất thời gian tìm Router
  WiFi.mode(WIFI_AP);
  Serial.println(
      "\n[WiFi] Dang chay che do LOCAL TEST (Tien trinh MQTT da tam tat)");
#endif

  // Phát SoftAP cho Node ESP32-C3
  WiFi.softAP(AP_SSID);
  Serial.printf("[AP] Gateway phat AP: %s (IP: 192.168.4.1)\n", AP_SSID);

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

  // 2. Quản lý MQTT (chỉ kích hoạt khi ENABLE_MQTT = true)
#if ENABLE_MQTT
  if (WiFi.status() == WL_CONNECTED && mqtt_client == NULL) {
    initAndStartMQTT();
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
    prevFallState = incomingData.fallDetected;

    // Xử lý từng mẫu trong 25 mẫu ECG
    for (int i = 0; i < 25; i++) {
      if (!incomingData.leadsOff) {
        processEdgeBPM(incomingData.ecgSamples[i]);
      } else {
        edgeBpm = 0; // Khi hở dây, nhịp tim về 0
      }
      if (i % 3 == 0) {
        updateWaveformBuffer(incomingData.ecgSamples[i]);
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
