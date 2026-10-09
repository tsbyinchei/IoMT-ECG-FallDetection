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
#define FALL_THRESHOLD  2.2f // Ngưỡng SMV cảnh báo va đập té ngã (đơn vị: g, dải đo ±8g)

// ==================== THÔNG TIN MẠNG UDP & MULTI-WIFI ROAMING ====================
// 1. Mạng Gateway SoftAP (Mặc định khi ở gần Gateway bàn làm việc)
const char* AP_SSID = "BIOMED_GW";
const unsigned int UDP_PORT = 4210;

IPAddress local_IP(192, 168, 4, 2);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress gwDirectIP(192, 168, 4, 1);
IPAddress currentTargetIP(192, 168, 4, 1);

// 2. Mạng Router Wi-Fi gia đình (Phủ sóng toàn bộ ngôi nhà khi đi xa khỏi Gateway)
#ifndef SECRET_ROUTER_SSID
#define SECRET_ROUTER_SSID "YOUR_WIFI_SSID"
#endif
#ifndef SECRET_ROUTER_PASS
#define SECRET_ROUTER_PASS "YOUR_WIFI_PASSWORD"
#endif
const char* HOME_SSID = SECRET_ROUTER_SSID;
const char* HOME_PASS = SECRET_ROUTER_PASS;

enum WifiNetType { NET_GATEWAY_AP, NET_HOME_WIFI };
WifiNetType currentNet = NET_GATEWAY_AP;

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
float windowPeakSMV = 1.0f;

// ==================== KHỞI TẠO VÀ ĐỌC MPU-6050 ====================
bool initMPU6050() {
  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, INPUT_PULLUP);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 400000); // 400kHz Fast-mode I2C
  Wire.setTimeOut(20); // Giới hạn 20ms, tuyệt đối không treo CPU nếu dây I2C lỏng

  Serial.print("[I2C] Dang kiem tra MPU-6050 tai 0x68...");
  Wire.beginTransmission(0x68);
  Wire.write(0x6B); // PWR_MGMT_1
  Wire.write(0x00); // Đánh thức MPU-6050
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
    // 1. Cấu hình dải đo gia tốc: ±8g (AFS_SEL = 2, độ nhạy 4096 LSB/g)
    // Tham khảo từ Elderly-Fall-Detection-System để không bị bão hòa trần ở mức 2.0g
    Wire.beginTransmission(mpuAddr);
    Wire.write(0x1C); // ACCEL_CONFIG
    Wire.write(0x10); // 0x10 = AFS_SEL 2 (±8g)
    Wire.endTransmission(true);

    // 2. Cấu hình bộ lọc DLPF = 260Hz để bắt trọn xung va đập tức thời
    Wire.beginTransmission(mpuAddr);
    Wire.write(0x1A); // CONFIG
    Wire.write(0x00); // DLPF_CFG = 0 (260Hz bandwidth)
    Wire.endTransmission(true);

    Serial.println("[MPU] Da cau hinh dai do Gia toc: +/-8G (4096 LSB/g), DLPF: 260Hz");
    return true;
  }

  Serial.println("[CANH BAO] Khong tim thay MPU-6050 tren GPIO 6 (SDA) va GPIO 7 (SCL)!");
  Serial.println("           Kiem tra: VCC->3.3V, GND->GND, SDA->Chân 6, SCL->Chân 7");
  return false;
}

// ── Thuật toán Kiểm định Té ngã 2 giai đoạn (Post-Fall Inactivity Verification) ──
bool candidateFall = false;
unsigned long candidateFallTime = 0;
float candidatePeakSMV = 1.0f;
float lastTiltAngle = 0.0f;
float lastTotalGyro = 0.0f;

// Vector trọng trường tham chiếu thích ứng động (Cho phép đặt MPU ở BẤT KỲ GÓC NÀO)
float baseGx = 0.0f, baseGy = 0.0f, baseGz = 1.0f;
bool baseGInitialized = false;

void sampleMPU6050() {
  if (!mpuFound) return;

  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0 && Wire.endTransmission(true) != 0) return;

  // Đọc trọn vẹn 14 thanh ghi: Accel (6B) + Temp (2B) + Gyro (6B)
  if (Wire.requestFrom((uint8_t)mpuAddr, (size_t)14, true) == 14) {
    int16_t ax_raw = (Wire.read() << 8) | Wire.read();
    int16_t ay_raw = (Wire.read() << 8) | Wire.read();
    int16_t az_raw = (Wire.read() << 8) | Wire.read();
    (void)((Wire.read() << 8) | Wire.read()); // Bỏ qua nhiệt độ nội MPU
    int16_t gx_raw = (Wire.read() << 8) | Wire.read();
    int16_t gy_raw = (Wire.read() << 8) | Wire.read();
    int16_t gz_raw = (Wire.read() << 8) | Wire.read();

    // Với dải ±8g, hệ số quy đổi độ nhạy là 4096 LSB/g
    lastAx = (float)ax_raw / 4096.0f;
    lastAy = (float)ay_raw / 4096.0f;
    lastAz = (float)az_raw / 4096.0f;

    // Chuyển đổi vận tốc góc Gyroscope (đơn vị: độ/giây - dps)
    float gx_dps = (float)gx_raw / 131.0f;
    float gy_dps = (float)gy_raw / 131.0f;
    float gz_dps = (float)gz_raw / 131.0f;
    lastTotalGyro = fabsf(gx_dps) + fabsf(gy_dps) + fabsf(gz_dps);

    float instantSMV = sqrtf(lastAx * lastAx + lastAy * lastAy + lastAz * lastAz);

    // Chốt giữ đỉnh va đập trong cửa sổ 100ms gửi UDP
    if (instantSMV > windowPeakSMV) {
      windowPeakSMV = instantSMV;
    }
    sensorData.smv = windowPeakSMV;

    // Tự động nhận diện tư thế ban đầu bất kể hướng đặt MPU-6050
    if (!baseGInitialized && instantSMV > 0.5f) {
      baseGx = lastAx / instantSMV;
      baseGy = lastAy / instantSMV;
      baseGz = lastAz / instantSMV;
      baseGInitialized = true;
    }

    // Tự động cập nhật thích nghi trọng trường khi cơ thể ở trạng thái tĩnh ổn định (0.85g - 1.15g)
    if (!candidateFall && fallHoldCounter == 0 && instantSMV >= 0.85f && instantSMV <= 1.15f && lastTotalGyro < 45.0f) {
      baseGx = 0.98f * baseGx + 0.02f * (lastAx / instantSMV);
      baseGy = 0.98f * baseGy + 0.02f * (lastAy / instantSMV);
      baseGz = 0.98f * baseGz + 0.02f * (lastAz / instantSMV);
      float baseLen = sqrtf(baseGx * baseGx + baseGy * baseGy + baseGz * baseGz);
      if (baseLen > 0.01f) {
        baseGx /= baseLen;
        baseGy /= baseLen;
        baseGz /= baseLen;
      }
    }

    // Tính góc biến thiên tư thế (Tilt Angle) so với tư thế ban đầu (Tự thích ứng mọi hướng gắn)
    if (instantSMV > 0.2f) {
      float dotProd = (lastAx * baseGx + lastAy * baseGy + lastAz * baseGz) / instantSMV;
      if (dotProd > 1.0f) dotProd = 1.0f;
      if (dotProd < -1.0f) dotProd = -1.0f;
      lastTiltAngle = acosf(fabsf(dotProd)) * (180.0f / (float)M_PI);
    }

    // ── GIAI ĐOẠN 1: PHÁT HIỆN XUNG VA ĐẬP (Stage 1: Impact Acceleration Peak) ──
    if (instantSMV >= FALL_THRESHOLD && !candidateFall && fallHoldCounter == 0) {
      candidateFall = true;
      candidateFallTime = millis();
      candidatePeakSMV = instantSMV;
      fallHoldCounter = 30; // Kích hoạt chốt giữ cảnh báo ngã 3 giây (30 gói UDP) ngay lập tức!
      sensorData.fallDetected = 1;
      Serial.printf("[STAGE 1: VA ĐẬP] SMV Đỉnh: %.2fg >= %.1fg -> KÍCH HOẠT CÒI HÚ LIÊN TỤC & Bắt đầu thẩm định 2.0s...\n",
                    instantSMV, FALL_THRESHOLD);
    }

    // ── GIAI ĐOẠN 2: THẨM ĐỊNH BẤT ĐỘNG & TƯ THẾ NẰM (Stage 2: Post-Fall Inactivity & Posture Verification) ──
    if (candidateFall) {
      if (instantSMV > candidatePeakSMV) {
        candidatePeakSMV = instantSMV;
      }

      unsigned long elapsed = millis() - candidateFallTime;

      // Kịch bản A - Tự phục hồi / Báo động giả (Self-Recovery):
      // Người dùng vẫn đứng thẳng (Tilt < 30°) và tiếp tục vận động (Gyro > 80°/s) -> Hủy báo động
      if (elapsed < 2000) {
        if (lastTiltAngle < 30.0f && lastTotalGyro > 80.0f) {
          candidateFall = false;
          Serial.println("[TỰ HỦY BÁO ĐỘNG] Người dùng đã đứng thẳng và di chuyển bình thường!");
        }
      }
      // Kịch bản B - Hết cửa sổ thẩm định (>= 2.0s):
      // Kiểm tra 2 tiêu chí y sinh bắt buộc:
      // 1. Tư thế nằm sàn: Góc nghiêng cơ thể Tilt >= 40°
      // 2. Trạng thái bất động: Vận tốc góc Gyro < 70°/s và độ biến thiên gia tốc tĩnh ổn định
      else if (elapsed >= 2000) {
        bool isLyingDown = (lastTiltAngle >= 40.0f);
        bool isImmobile  = (lastTotalGyro < 70.0f && fabsf(instantSMV - 1.0f) < 0.50f);

        if (isLyingDown && isImmobile) {
          fallHoldCounter = 30; // Chốt giữ cảnh báo ngã trong 3 giây (30 gói tin UDP 100ms)
          Serial.println("\n***************************************************");
          Serial.printf(">>> [XÁC NHẬN TÉ NGÃ 2 GIAI ĐOẠN (POST-FALL VERIFIED)] <<<\n");
          Serial.printf("    - Va đập SMV cực đại: %.2fg (Ngưỡng: %.1fg)\n", candidatePeakSMV, FALL_THRESHOLD);
          Serial.printf("    - Góc nghiêng nằm sàn: %.1f° (Chuẩn >= 40°)\n", lastTiltAngle);
          Serial.printf("    - Độ bất động sau ngã: Gyro = %.1f°/s (< 70°/s)\n", lastTotalGyro);
          Serial.println("***************************************************\n");
        } else {
          Serial.printf("[HỦY BÁO ĐỘNG] Không thỏa mãn tiêu chuẩn ngã thật: Góc=%.1f° (cần >=40°), Gyro=%.1f°/s\n",
                        lastTiltAngle, lastTotalGyro);
        }
        candidateFall = false;
      }
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

  // Cấu hình WiFi kết nối nhanh tới Gateway AP (nguyên bản hoạt động ổn định)
  Serial.printf("[WiFi] Dang ket noi vao Gateway AP: %s...\n", AP_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(AP_SSID);
  currentNet = NET_GATEWAY_AP;
  currentTargetIP = gwDirectIP;

  lastSampleMicros = micros();
}

void loop() {
  // Quản lý kết nối Wi-Fi & Tự động kết nối lại / Roaming dự phòng sang Wi-Fi nhà
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Nháy LED khi mất sóng
    static unsigned long lastWifiRetry = 0;
    static uint8_t failCount = 0;

    if (millis() - lastWifiRetry >= 3000) {
      lastWifiRetry = millis();
      failCount++;

      // Sau 3 lần thử (9 giây) nếu không thấy Gateway AP -> Thử chuyển sang Wi-Fi nhà
      if (failCount >= 3 && currentNet == NET_GATEWAY_AP) {
        Serial.printf("[WiFi] Gateway AP khong phan hoi -> Chuyen sang thu Wi-Fi nha '%s'...\n", HOME_SSID);
        WiFi.disconnect();
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // Bật lại DHCP cho Router
        WiFi.begin(HOME_SSID, HOME_PASS);
        currentNet = NET_HOME_WIFI;
        currentTargetIP = IPAddress(255, 255, 255, 255);
        failCount = 0;
      } 
      // Nếu đang thử Wi-Fi nhà mà 3 lần (9 giây) không được -> Thử quay lại Gateway AP
      else if (failCount >= 3 && currentNet == NET_HOME_WIFI) {
        Serial.printf("[WiFi] Wi-Fi nha khong phan hoi -> Quay lai thu Gateway AP '%s'...\n", AP_SSID);
        WiFi.disconnect();
        WiFi.config(local_IP, gateway, subnet);
        WiFi.begin(AP_SSID);
        currentNet = NET_GATEWAY_AP;
        currentTargetIP = gwDirectIP;
        failCount = 0;
      } 
      else {
        Serial.printf("[WiFi] Dang thu lai ket noi %s...\n", 
                      (currentNet == NET_GATEWAY_AP) ? AP_SSID : HOME_SSID);
        WiFi.disconnect();
        if (currentNet == NET_GATEWAY_AP) {
          WiFi.config(local_IP, gateway, subnet);
          WiFi.begin(AP_SSID);
        } else {
          WiFi.begin(HOME_SSID, HOME_PASS);
        }
      }
    }

    if (millis() - lastDiagLog >= 1000) {
      lastDiagLog = millis();
      Serial.printf("[WiFi] Dang cho ket noi mang %s...\n", 
                    (currentNet == NET_GATEWAY_AP) ? AP_SSID : HOME_SSID);
    }
    delay(100);
    return;
  } else {
    digitalWrite(LED_PIN, HIGH); // Bật sáng liên tục khi đã kết nối ổn định
    static bool loggedConnect = false;
    if (!loggedConnect) {
      loggedConnect = true;
      Serial.printf("\n[WiFi OK] DA KET NOI THANH CONG VAO %s! IP: %s (Target UDP: %s)\n",
                    (currentNet == NET_GATEWAY_AP) ? AP_SSID : HOME_SSID,
                    WiFi.localIP().toString().c_str(),
                    currentTargetIP.toString().c_str());
    }

    // Nếu đang kết nối Gateway AP mà sóng quá yếu (< -84 dBm, bệnh nhân đi xa khỏi bàn):
    // Tự động chuyển vùng Roaming sang Wi-Fi nhà:
    static unsigned long lastRssiCheck = 0;
    if (millis() - lastRssiCheck >= 5000) {
      lastRssiCheck = millis();
      if (currentNet == NET_GATEWAY_AP && WiFi.RSSI() < -84) {
        Serial.printf("[WiFi ROAMING] Song Gateway yeu (%d dBm < -84 dBm) -> Chuyen sang Wi-Fi nha '%s'...\n",
                      WiFi.RSSI(), HOME_SSID);
        WiFi.disconnect();
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
        WiFi.begin(HOME_SSID, HOME_PASS);
        currentNet = NET_HOME_WIFI;
        currentTargetIP = IPAddress(255, 255, 255, 255);
        loggedConnect = false;
      }
    }
  }

  // 1. Máy trạng thái cập nhật nhiệt độ nền (không block CPU)
  processTemperature();

  // 2. Lấy mẫu ECG với tần số chính xác 250 Hz (mỗi 4000 micro-giây = 4ms)
  unsigned long currentMicros = micros();
  if (currentMicros - lastSampleMicros >= 4000) {
    lastSampleMicros = currentMicros;

    // Đọc mẫu Analog từ chân ECG (12-bit ADC: 0 -> 4095)
    sensorData.ecgSamples[sampleIndex] = analogRead(ECG_PIN);

    // Lấy mẫu gia tốc MPU-6050 mỗi 8ms (cứ mỗi 2 mẫu ECG) để bắt trọn xung va đập ngã
    if (sampleIndex % 2 == 0) {
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

      // Đóng gói và gửi UDP sang Gateway (Unicast 192.168.4.1 hoặc Broadcast LAN 255.255.255.255)
      udp.beginPacket(currentTargetIP, UDP_PORT);
      udp.write((uint8_t*)&sensorData, sizeof(sensorData));
      udp.endPacket();

      // Đặt lại đỉnh va chạm cho cửa sổ 100ms tiếp theo về mức gia tốc hiện tại
      windowPeakSMV = sqrtf(lastAx * lastAx + lastAy * lastAy + lastAz * lastAz);
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
