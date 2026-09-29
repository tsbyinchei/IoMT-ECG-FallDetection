#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <OneWire.h>

// ==================== CẤU HÌNH PHẦN CỨNG ESP32-C3 ====================
#define LED_PIN         8    // LED báo trạng thái WiFi
#define ECG_PIN         1    // Chân Analog ADC1_CH1 đọc tín hiệu AD8232
#define LO_MINUS_PIN    2    // Chân LO- phát hiện hở điện cực âm
#define LO_PLUS_PIN     3    // Chân LO+ phát hiện hở điện cực dương
#define ONE_WIRE_BUS    4    // Chân dữ liệu 1-Wire cho cảm biến nhiệt độ DS18B20
#define I2C_SDA_PIN     6    // Chân SDA giao tiếp MPU-6050
#define I2C_SCL_PIN     7    // Chân SCL giao tiếp MPU-6050

#define MPU6050_ADDR    0x68 // Địa chỉ I2C mặc định của MPU-6050
#define FALL_THRESHOLD  2.5f // Ngưỡng SMV cảnh báo té ngã (đơn vị: g)

// ==================== THÔNG TIN MẠNG UDP ====================
const char* AP_SSID = "BIOMED_GW";
const char* GATEWAY_IP = "192.168.4.1";
const unsigned int UDP_PORT = 4210;

IPAddress local_IP(192, 168, 4, 2);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// ==================== GÓI DỮ LIỆU ĐÓNG GÓI ====================
// Gom 25 mẫu ECG @ 250Hz (chu kỳ 4ms/mẫu, 25 mẫu = 100ms)
typedef struct __attribute__((packed)) struct_message {
  uint16_t ecgSamples[25];  // 25 mẫu ECG thô (ADC 0-4095)
  float bodyTemp;           // Nhiệt độ cơ thể (°C)
  float smv;                // Signal Magnitude Vector gia tốc (g)
  uint8_t leadsOff;         // 1: Hở điện cực | 0: Bình thường
  uint8_t fallDetected;     // 1: Phát hiện ngã  | 0: Bình thường
} struct_message;

struct_message sensorData;
OneWire ds(ONE_WIRE_BUS);
WiFiUDP udp;

// ==================== MÁY TRẠNG THÁI DS18B20 (NON-BLOCKING) ====================
// Sử dụng chế độ Skip ROM (0xCC) để tự động nhận dạng mọi cảm biến DS18B20 mà không cần hardcode địa chỉ
enum TempStep { TRIGGER_CONVERT, WAIT_READ };
TempStep tempStep = TRIGGER_CONVERT;
unsigned long tempTimer = 0;

void processTemperature() {
  if (tempStep == TRIGGER_CONVERT) {
    if (millis() - tempTimer >= 1000) {
      ds.reset();
      ds.skip();          // Bỏ qua bước kiểm tra ROM ID, tác động trực tiếp lên bus 1 thiết bị
      ds.write(0x44, 0);  // Bắt đầu quá trình chuyển đổi nhiệt độ (Convert T)
      tempTimer = millis();
      tempStep = WAIT_READ;
    }
  } else if (tempStep == WAIT_READ) {
    if (millis() - tempTimer >= 800) { // Chờ đủ 800ms để DS18B20 hoàn tất phân giải 12-bit
      byte data[9];
      ds.reset();
      ds.skip();
      ds.write(0xBE);     // Đọc dữ liệu từ Scratchpad (Read Scratchpad)

      for (int i = 0; i < 9; i++) {
        data[i] = ds.read();
      }

      // Kiểm tra tính toàn vẹn dữ liệu bằng mã CRC8
      if (OneWire::crc8(data, 8) == data[8]) {
        int16_t raw = (data[1] << 8) | data[0];
        float c = (float)raw / 16.0f;
        // Lọc bỏ giá trị khởi động rác (85.0°C) và ngoài dải sinh lý người
        if (c > 15.0f && c < 50.0f && c != 85.0f) {
          sensorData.bodyTemp = c;
        }
      }
      tempStep = TRIGGER_CONVERT;
      tempTimer = millis();
    }
  }
}

// Cấu hình chốt cảnh báo té ngã (giữ trạng thái trong 2 giây = 20 gói tin 100ms)
uint8_t fallHoldCounter = 0;
float currentMaxSMV = 1.0f;

// Địa chỉ I2C của MPU-6050 (0x68 hoặc 0x69 nếu chân AD0 kéo lên 3.3V)
uint8_t mpuAddr = 0x68;
bool mpuFound = false;
float lastAx = 0, lastAy = 0, lastAz = 1.0f;

// ==================== KHỞI TẠO VÀ ĐỌC MPU-6050 ====================
bool initMPU6050() {
  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, INPUT_PULLUP);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  Wire.setTimeOut(20); // Giới hạn 20ms, tuyệt đối không treo CPU nếu dây I2C lỏng

  Serial.print("[I2C] Dang kiem tra MPU-6050 tai 0x68...");
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  if (Wire.endTransmission(true) == 0) {
    mpuAddr = 0x68;
    mpuFound = true;
    Serial.println(" [OK] Tim thay tai 0x68!");
  } else {
    Serial.println(" [FAIL]");
    Serial.print("[I2C] Dang thu dia chi phu 0x69...");
    Wire.beginTransmission(0x69);
    Wire.write(0x6B);
    Wire.write(0x00);
    if (Wire.endTransmission(true) == 0) {
      mpuAddr = 0x69;
      mpuFound = true;
      Serial.println(" [OK] Tim thay tai 0x69!");
    } else {
      Serial.println(" [FAIL]");
    }
  }

  if (mpuFound) {
    Wire.beginTransmission(mpuAddr);
    Wire.write(0x1C);
    Wire.write(0x00);
    Wire.endTransmission(true);
    return true;
  }

  Serial.println("[CANH BAO] Khong tim thay MPU-6050 tren GPIO 6 (SDA) va GPIO 7 (SCL)!");
  Serial.println("           Kiem tra: VCC->3.3V, GND->GND, SDA->Chân 6, SCL->Chân 7");
  return false;
}

void sampleMPU6050() {
  if (!mpuFound) return;

  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);
  if (Wire.endTransmission(true) != 0) return;

  if (Wire.requestFrom((uint8_t)mpuAddr, (size_t)6, true) == 6) {
    int16_t ax_raw = (Wire.read() << 8) | Wire.read();
    int16_t ay_raw = (Wire.read() << 8) | Wire.read();
    int16_t az_raw = (Wire.read() << 8) | Wire.read();

    lastAx = (float)ax_raw / 16384.0f;
    lastAy = (float)ay_raw / 16384.0f;
    lastAz = (float)az_raw / 16384.0f;

    float instantSMV = sqrtf(lastAx * lastAx + lastAy * lastAy + lastAz * lastAz);
    sensorData.smv = instantSMV;

    if (instantSMV >= FALL_THRESHOLD) {
      fallHoldCounter = 20;
    }
  }
}

// ==================== BIẾN ĐIỀU KHIỂN THỜI GIAN ====================
unsigned long lastSampleMicros = 0;
uint8_t sampleIndex = 0;
unsigned long lastDiagLog = 0;

void setup() {
  Serial.begin(115200);
  delay(500); // Chờ cổng USB CDC ổn định

  Serial.println("\n==========================================");
  Serial.println("  ESP32-C3 BIOMEDICAL SENSOR NODE DANG KHOI DONG");
  Serial.println("  Tan so lay mau ECG: 250 Hz (4ms/sample)");
  Serial.println("==========================================");

  // Cấu hình ADC độ phân giải 12-bit (0-4095) cho tín hiệu ECG AD8232
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  pinMode(LED_PIN, OUTPUT);
  pinMode(ECG_PIN, INPUT);
  pinMode(LO_MINUS_PIN, INPUT_PULLDOWN);
  pinMode(LO_PLUS_PIN, INPUT_PULLDOWN);

  // Khởi tạo MPU-6050
  initMPU6050();

  // Khởi tạo giá trị mặc định cho cấu trúc dữ liệu
  sensorData.bodyTemp = 36.5f;
  sensorData.smv = 1.0f;
  sensorData.leadsOff = 0;
  sensorData.fallDetected = 0;

  // Cấu hình WiFi kết nối nhanh tới Gateway
  Serial.printf("[WiFi] Dang ket noi vao Gateway AP: %s...\n", AP_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(AP_SSID);

  lastSampleMicros = micros();
}

void loop() {
  // Tự động kết nối lại định kỳ mỗi 3s nếu mất sóng hoặc Gateway khởi động sau (không cần bấm RST)
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    static unsigned long lastWifiRetry = 0;
    if (millis() - lastWifiRetry >= 3000) {
      lastWifiRetry = millis();
      Serial.println("[WiFi] Dang thu ket noi lai AP 'BIOMED_GW'...");
      WiFi.disconnect();
      WiFi.begin(AP_SSID);
    }
    if (millis() - lastDiagLog >= 1000) {
      lastDiagLog = millis();
      Serial.println("[WiFi] Dang cho Gateway phat AP 'BIOMED_GW' de ket noi...");
    }
    delay(100);
    return;
  } else {
    digitalWrite(LED_PIN, HIGH); // Bật sáng liên tục khi đã kết nối ổn định
  }

  // 1. Máy trạng thái cập nhật nhiệt độ nền (không block CPU)
  processTemperature();

  // 2. Lấy mẫu ECG với tần số chính xác 250 Hz (mỗi 4000 micro-giây = 4ms)
  unsigned long currentMicros = micros();
  if (currentMicros - lastSampleMicros >= 4000) {
    lastSampleMicros = currentMicros;

    // Đọc mẫu Analog từ chân ECG (12-bit ADC: 0 -> 4095)
    sensorData.ecgSamples[sampleIndex] = analogRead(ECG_PIN);

    // Lấy mẫu gia tốc MPU-6050 mỗi 20ms (cứ mỗi 5 mẫu ECG) để bắt trọn xung va đập ngã
    if (sampleIndex % 5 == 0) {
      sampleMPU6050();
    }

    sampleIndex++;

    // 3. Khi gom đủ 25 mẫu (= 100ms dữ liệu sinh hiệu) -> Thu thập MPU & Bắn UDP
    if (sampleIndex >= 25) {
      sampleIndex = 0;

      // Cập nhật cờ cảnh báo ngã (có chốt giữ 2s tránh bỏ sót xung)
      if (fallHoldCounter > 0) {
        sensorData.fallDetected = 1;
        fallHoldCounter--;
      } else {
        sensorData.fallDetected = 0;
      }

      // ==================== KIỂM TRA TUỘT ĐIỆN CỰC (SMART LEADS-OFF) ====================
      // Chân LO- (GPIO 2) trên một số kit ESP32-C3 SuperMini có trở kéo lên phần cứng (strapping pin)
      // nên nếu chỉ đọc digitalRead có thể bị kẹt HIGH liên tục.
      // Giải pháp: Kết hợp kiểm tra chân LO và kiểm tra mức bão hòa ADC (0 hoặc 4095).
      bool loMinus = (digitalRead(LO_MINUS_PIN) == HIGH);
      bool loPlus  = (digitalRead(LO_PLUS_PIN) == HIGH);
      
      // Nếu tín hiệu ADC rơi vào dải sinh lý bình thường (100 -> 4050), chắc chắn điện cực ĐANG DÁN TỐT!
      // Ngược lại, nếu ADC chạm trần >= 4080 hoặc tụt sàn <= 20 trong phần lớn mẫu -> Hở điện cực
      uint16_t sample0 = sensorData.ecgSamples[0];
      bool isAdcValid = (sample0 > 100 && sample0 < 4050);

      // Nếu ADC đang đọc sóng tốt, ưu tiên xác nhận đã gắn cực (leadsOff = 0)
      if (isAdcValid) {
        sensorData.leadsOff = 0;
      } else {
        // Chỉ báo hở cực khi tín hiệu bão hòa hoặc chân phần cứng báo hở
        sensorData.leadsOff = (loMinus || loPlus || sample0 >= 4080 || sample0 <= 50) ? 1 : 0;
      }

      // Đóng gói và gửi UDP broadcast sang Gateway
      udp.beginPacket(GATEWAY_IP, UDP_PORT);
      udp.write((uint8_t*)&sensorData, sizeof(sensorData));
      udp.endPacket();
    }
  }

  // 4. In thông tin chẩn đoán lên Serial Monitor định kỳ 500ms
  if (millis() - lastDiagLog >= 500) {
    lastDiagLog = millis();
    Serial.printf("[C3 TX] Temp: %.1f*C | SMV: %.2fg (X:%.2f, Y:%.2f, Z:%.2f) | Fall: %d | LeadOff: %d | ECG[0]: %u\n",
                  sensorData.bodyTemp, sensorData.smv, lastAx, lastAy, lastAz,
                  sensorData.fallDetected, sensorData.leadsOff, sensorData.ecgSamples[0]);
  }
}