/**
 * ====================================================================================================
 * ĐỀ TÀI: THIẾT KẾ HỆ THỐNG IOT THEO DÕI ĐIỆN TIM, THÂN NHIỆT VÀ CẢNH BÁO TÉ NGÃ TỪ XA CHO NGƯỜI CAO TUỔI TẠI NHÀ
 * HỆ THỐNG: PHÂN HỆ NODE CẢM BIẾN ĐEO NGƯỜI (WEARABLE SENSOR NODE - BẢN EXAMPLE MẪU)
 * VI ĐIỀU KHIỂN: ESP32-C3 SuperMini (Kiến trúc RISC-V 32-bit, xung nhịp 160MHz)
 * TÁC GIẢ: TsByinChei (contact@tsbyin.dev) • GITHUB: @tsbyinchei
 * ====================================================================================================
 * NGUYÊN LÝ HOẠT ĐỘNG:
 * 1. Thu thập tín hiệu Điện tâm đồ (ECG) qua cảm biến AD8232 với tần số Fs = 250 Hz (chu kỳ 4.0ms).
 *    - Sử dụng cơ chế định thời non-blocking micros() chính xác cao, không chặn CPU.
 *    - Gom 25 mẫu ECG liên tiếp (= 100ms) vào khung nhị phân chuẩn Packed Struct 60 bytes.
 * 2. Thu thập gia tốc 3 trục từ MPU-6050 qua giao tiếp I2C Fast-mode (400kHz) mỗi 8ms (125Hz):
 *    - Vector gia tốc toàn phần: SMV = sqrt(ax^2 + ay^2 + az^2).
 *    - Thuật toán kiểm định té ngã 2 giai đoạn: Xung va đập (SMV >= 2.8g) kết hợp cửa sổ thẩm định 2.5s
 *      đánh giá góc nghiêng cơ thể (Tilt >= 40°) và trạng thái bất động sau ngã (Gyro < 45°/s).
 * 3. Thu thập thân nhiệt bề mặt từ DS18B20 qua giao tiếp 1-Wire (GPIO 4):
 *    - Máy trạng thái non-blocking 2 bước (Trigger Convert 0x44 -> Chờ 800ms -> Đọc Scratchpad 0xBE).
 *    - Kiểm tra mã kiểm dư toàn vẹn CRC8 và lọc dải sinh lý 15°C - 50°C.
 * 4. Truyền thông Wi-Fi UDP Socket điểm-điểm (Port 4210, 60 bytes/packet):
 *    - Đóng gói struct 60 bytes bắn sang Gateway ESP32 với độ trễ cực thấp (< 5ms).
 *    - Hỗ trợ chuyển vùng Roaming tự động giữa SoftAP của Gateway (BIOMED_GW) và Wi-Fi Router gia đình.
 *    - Cơ chế an toàn Fail-safe: Đo đạc và phân tích ngã luôn chạy 100% thời gian độc lập khi mất mạng.
 * ====================================================================================================
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <OneWire.h>

// ==================== CẤU HÌNH PHẦN CỨNG & SƠ ĐỒ CHÂN ESP32-C3 SUPERMINI ====================
// ESP32-C3 SuperMini sử dụng nhân RISC-V 32-bit (Single-core, 160MHz), tích hợp Wi-Fi 4 & BLE 5.
// Lưu ý phần cứng: Chân GPIO 2 và GPIO 8 là Strapping Pin (kéo trở trên board), cần xử lý cẩn trọng.
#define LED_PIN         8    // LED hiển thị trạng thái WiFi & hoạt động hệ thống (Active Low / High tuỳ kit)
#define ECG_PIN         1    // Kênh Analog ADC1_CH1 đọc tín hiệu tương tự từ ngõ ra OUTPUT của AD8232
#define LO_MINUS_PIN    2    // Ngõ ra LO- (Leads-off Negative) phát hiện hở điện cực âm trên cơ thể
#define LO_PLUS_PIN     3    // Ngõ ra LO+ (Leads-off Positive) phát hiện hở điện cực dương trên cơ thể
#define ONE_WIRE_BUS    4    // Bus giao tiếp 1-Wire cho cảm biến nhiệt độ Dallas DS18B20 (cần trở kéo 4.7kΩ)
#define I2C_SDA_PIN     6    // Tuyến truyền dữ liệu SDA bus I2C cho MPU-6050 (Fast-mode 400kHz)
#define I2C_SCL_PIN     7    // Tuyến xung nhịp SCL bus I2C cho MPU-6050 (Fast-mode 400kHz)

#define MPU6050_ADDR    0x68 // Địa chỉ I2C mặc định của MPU-6050 khi chân AD0 nối GND (hoặc 0x69 nếu AD0 lên 3.3V)
#define FALL_THRESHOLD  2.8f // Ngưỡng gia tốc va đập SMV kích hoạt giai đoạn 1 (2.8g nhằm loại bỏ hoạt động ADL thường ngày)

// ==================== THÔNG TIN MẠNG UDP & MULTI-WIFI ROAMING ====================
// Phân hệ hỗ trợ chuyển vùng tự động giữa mạng Gateway AP nội bộ và Router Wi-Fi gia đình:
// 1. Mạng Gateway SoftAP (Mặc định khi người bệnh ở trong phòng có Gateway bàn làm việc)
const char* AP_SSID = "BIOMED_GW";
const unsigned int UDP_PORT = 4210;

IPAddress local_IP(192, 168, 4, 2);      // Địa chỉ IP tĩnh của Node C3 trong mạng Gateway AP
IPAddress gateway(192, 168, 4, 1);       // Địa chỉ IP của Gateway trung tâm
IPAddress subnet(255, 255, 255, 0);     // Mặt nạ mạng con Subnet Mask chuẩn lớp C
IPAddress gwDirectIP(192, 168, 4, 1);    // Địa chỉ đích Unicast gửi gói tin trực tiếp tới Gateway
IPAddress currentTargetIP(192, 168, 4, 1);

// 2. Mạng Router Wi-Fi gia đình (Phủ sóng toàn bộ nhà khi người bệnh di chuyển xa khỏi Gateway)
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

// ==================== KHUNG TRUYỀN DỮ LIỆU ĐỒNG BỘ UDP (PACKED STRUCT) ====================
// Sử dụng chỉ thị thuộc tính __attribute__((packed)) của trình biên dịch GCC để:
// 1. Triệt tiêu hoàn toàn padding bytes do căn chỉnh bộ nhớ 32-bit (đảm bảo cấu trúc nhị phân 100% đồng nhất).
// 2. Kích thước khung cố định chính xác 60 bytes:
//    - ecgSamples[25]: 25 mẫu * 2 bytes = 50 bytes (tương ứng 100ms tín hiệu tại Fs = 250Hz, ADC 12-bit 0-4095).
//    - bodyTemp: float 4 bytes (IEEE 754 đơn chính xác, phân giải 0.0625°C).
//    - smv: float 4 bytes (Vector gia tốc toàn phần cực đại trong cửa sổ 100ms).
//    - leadsOff: uint8_t 1 byte (0: tiếp xúc tốt, 1: phát hiện tuột điện cực).
//    - fallDetected: uint8_t 1 byte (0: bình thường, 1: xác nhận biến cố té ngã sau 2 giai đoạn).
//    Tổng kích thước gói UDP Payload = 50 + 4 + 4 + 1 + 1 = 60 bytes.
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

// ==================== MÁY TRẠNG THÁI NHIỆT ĐỘ NON-BLOCKING (DS18B20) ====================
// Cảm biến Dallas DS18B20 ở độ phân giải 12-bit yêu cầu thời gian chuyển đổi t_conv tối đa lên tới 750ms.
// Nếu gọi hàm delay(750) truyền thống sẽ làm gián đoạn chu kỳ lấy mẫu ECG 250Hz và MPU-6050 125Hz.
// Do đó, thiết kế Máy trạng thái hữu hạn (FSM) 2 bước định thời qua millis():
// - Bước 1 (TRIGGER_CONVERT): Gửi lệnh Skip ROM (0xCC) và Convert T (0x44) định kỳ mỗi 1000ms.
// - Bước 2 (WAIT_READ): Sau 800ms chờ đợi (không khóa CPU), gửi lệnh Read Scratchpad (0xBE) đọc 9 bytes.
// - Xác thực tính toàn vẹn: Kiểm tra mã CRC8 đa thức X^8 + X^5 + X^4 + 1 trên 8 bytes dữ liệu đầu so với byte 8.
// - Lọc nhiễu sinh lý: Loại bỏ giá trị reset khởi động 85.0°C và giới hạn trong dải nhiệt độ người 15.0°C - 50.0°C.
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

      // Kiểm tra tính toàn vẹn dữ liệu bằng mã kiểm dư CRC8
      if (OneWire::crc8(data, 8) == data[8]) {
        int16_t raw = (data[1] << 8) | data[0];
        float c = (float)raw / 16.0f; // Độ phân giải 12-bit: 1 LSB = 1/16 = 0.0625°C
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

// Cấu hình chốt cảnh báo té ngã (giữ trạng thái cảnh báo trong 4 giây = 40 gói tin UDP 100ms)
uint8_t fallHoldCounter = 0;
float currentMaxSMV = 1.0f;

// Địa chỉ I2C của MPU-6050 (0x68 hoặc 0x69 nếu chân AD0 kéo lên 3.3V)
uint8_t mpuAddr = 0x68;
bool mpuFound = false;
float lastAx = 0, lastAy = 0, lastAz = 1.0f;
float windowPeakSMV = 1.0f;

// ==================== KHỞI TẠO VÀ CẤU HÌNH CẢM BIẾN MPU-6050 ====================
// Sử dụng bus I2C Fast-mode (400kHz) trên GPIO 6 (SDA) và GPIO 7 (SCL):
// - Đánh thức cảm biến qua thanh ghi PWR_MGMT_1 (0x6B).
// - Cấu hình dải đo gia tốc ±8g (AFS_SEL = 2, 4096 LSB/g) để bắt trọn xung va đập mà không bị cắt đỉnh.
// - Cấu hình bộ lọc thông thấp kỹ thuật số nội DLPF = 260Hz nhằm thu thập các biến thiên động lực học nhanh.
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

// ==================== THUẬT TOÁN ĐỌC MPU-6050 & PHÂN TÍCH TÉ NGÃ 2 GIAI ĐOẠN ====================
// Chu kỳ thực thi: Mỗi 8ms (125Hz) - đồng bộ với luồng lấy mẫu ECG (cứ mỗi 2 mẫu ECG đọc 1 lần MPU).
//
// 1. Vector gia tốc toàn phần (Signal Magnitude Vector - SMV):
//    SMV = sqrt(Ax^2 + Ay^2 + Az^2)  [đơn vị: g, 1g = 9.80665 m/s^2]
//
// 2. Góc nghiêng cơ thể (Tilt Angle):
//    Tính qua tích vô hướng (Dot Product) giữa vector gia tốc tức thời và vector trọng lực tham chiếu:
//    cos(θ) = (Ax*Gx + Ay*Gy + Az*Gz) / (|A| * |G|)
//    θ = arccos(cos(θ)) * (180 / π)  [đơn vị: độ]
//
// 3. Quy trình kiểm định 2 giai đoạn (Two-Stage Verification):
//    - Giai đoạn 1: SMV >= 2.8g (Xung lực va chạm tiếp đất) -> Mở cửa sổ thẩm định 2.5s (chưa báo động).
//    - Giai đoạn 2 (sau 2.5s): Đánh giá đồng thời Tư thế nằm (Tilt >= 40°) và Bất động (Gyro < 45°/s).
void sampleMPU6050() {
  if (!mpuFound) return;

  // Trỏ thanh ghi bắt đầu đọc từ 0x3B (ACCEL_XOUT_H)
  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0 && Wire.endTransmission(true) != 0) return;

  // Đọc liên tục 14 thanh ghi nội của MPU-6050 qua Burst Read I2C:
  // - 0x3B..0x40: Gia tốc 3 trục Ax, Ay, Az (6 bytes)
  // - 0x41..0x42: Nhiệt độ bán dẫn nội die chip (2 bytes - bỏ qua)
  // - 0x43..0x48: Vận tốc góc con quay hồi chuyển Gx, Gy, Gz (6 bytes)
  if (Wire.requestFrom((uint8_t)mpuAddr, (size_t)14, true) == 14) {
    int16_t ax_raw = (Wire.read() << 8) | Wire.read();
    int16_t ay_raw = (Wire.read() << 8) | Wire.read();
    int16_t az_raw = (Wire.read() << 8) | Wire.read();
    (void)((Wire.read() << 8) | Wire.read()); // Bỏ qua cảm biến nhiệt độ nội MPU
    int16_t gx_raw = (Wire.read() << 8) | Wire.read();
    int16_t gy_raw = (Wire.read() << 8) | Wire.read();
    int16_t gz_raw = (Wire.read() << 8) | Wire.read();

    // Chuyển đổi gia tốc số sang đơn vị hấp dẫn chuẩn (g):
    // Cấu hình AFS_SEL = 2 (±8g) -> Hệ số tỷ lệ độ nhạy là 4096 LSB/g
    lastAx = (float)ax_raw / 4096.0f;
    lastAy = (float)ay_raw / 4096.0f;
    lastAz = (float)az_raw / 4096.0f;

    // Chuyển đổi vận tốc góc sang đơn vị độ/giây (dps - degrees per second):
    // Cấu hình FS_SEL = 0 (±250 dps) -> Hệ số tỷ lệ độ nhạy là 131 LSB/(°/s)
    float gx_dps = (float)gx_raw / 131.0f;
    float gy_dps = (float)gy_raw / 131.0f;
    float gz_dps = (float)gz_raw / 131.0f;
    lastTotalGyro = fabsf(gx_dps) + fabsf(gy_dps) + fabsf(gz_dps);

    // Tính độ lớn gia tốc tổng hợp tức thời SMV
    float instantSMV = sqrtf(lastAx * lastAx + lastAy * lastAy + lastAz * lastAz);

    // Chốt giữ đỉnh va đập cao nhất trong cửa sổ 100ms gửi UDP sang Gateway
    if (instantSMV > windowPeakSMV) {
      windowPeakSMV = instantSMV;
    }
    sensorData.smv = windowPeakSMV;

    // Tự động nhận diện vector trọng trường tham chiếu ban đầu (Adaptive Gravity Reference):
    // Giúp thiết bị tự thích nghi với BẤT KỲ hướng đeo nào (ngực, thắt lưng, túi áo)
    if (!baseGInitialized && instantSMV > 0.5f) {
      baseGx = lastAx / instantSMV;
      baseGy = lastAy / instantSMV;
      baseGz = lastAz / instantSMV;
      baseGInitialized = true;
    }

    // Tự động cập nhật thích nghi trọng trường khi cơ thể ở trạng thái tĩnh ổn định (0.85g <= SMV <= 1.15g & Gyro < 45°/s):
    // Sử dụng bộ lọc thông thấp IIR (98% lịch sử + 2% mẫu mới) để tránh trôi do cử động chậm
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

    // Tính góc nghiêng cơ thể (Tilt Angle) so với tư thế đứng ban đầu qua tích vô hướng:
    if (instantSMV > 0.2f) {
      float dotProd = (lastAx * baseGx + lastAy * baseGy + lastAz * baseGz) / instantSMV;
      if (dotProd > 1.0f) dotProd = 1.0f;
      if (dotProd < -1.0f) dotProd = -1.0f;
      lastTiltAngle = acosf(dotProd) * (180.0f / (float)M_PI);
    }

    // ── GIAI ĐOẠN 1: PHÁT HIỆN XUNG VA ĐẬP KHẢ NGHI (Stage 1: Candidate Impact Peak) ──
    if (instantSMV >= FALL_THRESHOLD && !candidateFall && fallHoldCounter == 0) {
      candidateFall = true;
      candidateFallTime = millis();
      candidatePeakSMV = instantSMV;
      // QUAN TRỌNG: TUYỆT ĐỐI KHÔNG kích hoạt còi hay đặt fallDetected ở đây!
      // Bắt đầu cửa sổ phân tích tĩnh 2.5 giây.
      Serial.printf("\n[GIAI ĐOẠN 1] Phát hiện xung va đập SMV: %.2fg >= %.1fg! Bắt đầu phân tích bất động & tư thế trong 2.5s...\n",
                    instantSMV, FALL_THRESHOLD);
    }

    // ── GIAI ĐOẠN 2: THẨM ĐỊNH BẤT ĐỘNG & TƯ THẾ NẰM (Stage 2: Post-Fall Inactivity & Posture Verification) ──
    if (candidateFall) {
      if (instantSMV > candidatePeakSMV) {
        candidatePeakSMV = instantSMV;
      }

      unsigned long elapsed = millis() - candidateFallTime;

      // Hết cửa sổ thẩm định 2.5 giây kể từ lúc va đập:
      // Phải thỏa mãn ĐỒNG THỜI 2 điều kiện y sinh của một ca ngã thật:
      // 1. Góc nghiêng cơ thể Tilt >= 40° (đang nằm ngang sàn so với tư thế đứng ban đầu)
      // 2. Trạng thái bất động sau ngã: Vận tốc góc Gyro < 45°/s và lực gia tốc ổn định quanh trọng lực 1g
      if (elapsed >= 2500) {
        bool isLyingDown = (lastTiltAngle >= 40.0f);
        bool isImmobile  = (lastTotalGyro < 45.0f && fabsf(instantSMV - 1.0f) < 0.35f);

        if (isLyingDown && isImmobile) {
          // CHÍNH THỨC XÁC NHẬN NGÃ THẬT SAU 2.5 GIÂY THẨM ĐỊNH!
          fallHoldCounter = 40; // Chốt giữ cảnh báo ngã 4.0 giây (40 gói tin UDP 100ms)
          sensorData.fallDetected = 1;
          Serial.println("\n***************************************************");
          Serial.println("🚨 >>> [XÁC NHẬN TÉ NGÃ 2 GIAI ĐOẠN HOÀN TẤT (VERIFIED FALL)] <<< 🚨");
          Serial.printf("    - Va đập SMV cực đại: %.2fg (Ngưỡng: %.1fg)\n", candidatePeakSMV, FALL_THRESHOLD);
          Serial.printf("    - Góc nghiêng nằm sàn: %.1f° (Chuẩn >= 40°)\n", lastTiltAngle);
          Serial.printf("    - Độ bất động sau ngã: Gyro = %.1f°/s (< 45°/s)\n", lastTotalGyro);
          Serial.println("***************************************************\n");
        } else {
          Serial.printf("[HỦY BÁO ĐỘNG] Cử động sinh hoạt bình thường: Góc=%.1f° (cần >=40°), Gyro=%.1f°/s (cần <45°/s)\n",
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

    if (millis() - lastDiagLog >= 2000) {
      lastDiagLog = millis();
      Serial.printf("[WiFi] Dang cho ket noi mang %s (Luong do ECG & Nga van hoat dong doc lap)...\n", 
                    (currentNet == NET_GATEWAY_AP) ? AP_SSID : HOME_SSID);
    }
    // ĐẶC TẢ AN TOÀN FAIL-SAFE: KHÔNG gọi delay() hay return; ở đây!
    // Vi điều khiển tiếp tục chạy xuống bên dưới để lấy mẫu ECG 250Hz và tính toán té ngã
    // liên tục 24/7, chỉ tạm dừng gửi gói UDP cho đến khi sóng mạng được kết nối lại.
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

  // 1. Máy trạng thái cập nhật nhiệt độ nền DS18B20 (hoàn toàn non-blocking, không trễ CPU)
  processTemperature();

  // 2. Lấy mẫu ECG với tần số chuẩn y sinh Fs = 250 Hz (chu kỳ chính xác 4000 micro-giây = 4ms):
  //    Sử dụng micros() định thời vi sai để triệt tiêu hiện tượng trôi pha (Accumulated Timing Jitter).
  //    Tín hiệu đọc từ chân GPIO 1 (kênh ADC1_CH1) - an toàn tuyệt đối, không bị xung đột với driver Wi-Fi (vốn chiếm ADC2).
  unsigned long currentMicros = micros();
  if (currentMicros - lastSampleMicros >= 4000) {
    lastSampleMicros = currentMicros;

    // Đọc mẫu tương tự từ ngõ ra AD8232 (ADC 12-bit: dải giá trị số 0 -> 4095 tương ứng 0V -> 3.3V)
    sensorData.ecgSamples[sampleIndex] = analogRead(ECG_PIN);

    // Thu thập gia tốc MPU-6050 mỗi 8ms (125Hz, cứ 2 mẫu ECG đọc 1 lần MPU):
    // Đảm bảo bắt trọn đỉnh va đập tức thời của các ca té ngã vốn diễn ra rất nhanh (< 50ms)
    if (sampleIndex % 2 == 0) {
      sampleMPU6050();
    }

    sampleIndex++;

    // 3. Khi gom đủ 25 mẫu ECG (= 100ms cửa sổ dữ liệu sinh hiệu) -> Kiểm tra cực và phát gói tin UDP
    if (sampleIndex >= 25) {
      sampleIndex = 0;

      // Cập nhật cờ cảnh báo té ngã (bộ đếm chốt giữ 40 chu kỳ = 4.0 giây để Gateway & Web bắt trọn)
      if (fallHoldCounter > 0) {
        sensorData.fallDetected = 1;
        fallHoldCounter--;
      } else {
        sensorData.fallDetected = 0;
      }

      // ==================== THUẬT TOÁN PHÁT HIỆN TUỘT ĐIỆN CỰC THÔNG MINH (SMART LEADS-OFF) ====================
      // Hiện tượng phần cứng: Cảm biến AD8232 xuất mức HIGH trên chân LO- hoặc LO+ khi điện cực bị hở.
      // Tuy nhiên, chân LO- (GPIO 2) trên kit ESP32-C3 SuperMini là Strapping Pin (MTMS) có gắn sẵn trở kéo lên 3.3V
      // trên bo mạch, khiến digitalRead(LO_MINUS_PIN) luôn trả về HIGH ngay cả khi đang dán điện cực bình thường!
      //
      // GIẢI PHÁP ĐỘT PHÁ: Hợp nhất logic kiểm tra chân phần cứng với điện áp đường ray ADC (Rail Saturation Check):
      // - Khi điện cực dán tốt: Tín hiệu ECG dao động quanh đường đẳng điện ~ 1.65V (khoảng 1500 - 2500 LSB).
      //   Nếu mẫu đầu tiên rơi vào dải sinh lý hợp lệ (100 < ADC < 4050) -> Chắc chắn điện cực ĐANG DÁN TỐT (leadsOff = 0).
      // - Khi điện cực tuột hẳn: Ngõ ra Op-Amp của AD8232 sẽ bị bão hòa chạm trần nguồn (ADC >= 4080) hoặc tụt sàn GND (ADC <= 50).
      //   Khi đó mới kết hợp cờ LO để xác nhận tuột dây (leadsOff = 1).
      bool loMinus = (digitalRead(LO_MINUS_PIN) == HIGH);
      bool loPlus  = (digitalRead(LO_PLUS_PIN) == HIGH);
      
      uint16_t sample0 = sensorData.ecgSamples[0];
      bool isAdcValid = (sample0 > 100 && sample0 < 4050);

      if (isAdcValid) {
        sensorData.leadsOff = 0; // Ưu tiên hàng đầu cho tín hiệu sinh lý thực tế
      } else {
        sensorData.leadsOff = (loMinus || loPlus || sample0 >= 4080 || sample0 <= 50) ? 1 : 0;
      }

      // Đóng gói và phát gói tin UDP Socket 60 bytes sang Gateway ESP32:
      // Chỉ gửi khi Wi-Fi đã kết nối ổn định, hàm sendto của LwIP chạy hoàn toàn non-blocking (< 1ms).
      if (WiFi.status() == WL_CONNECTED) {
        udp.beginPacket(currentTargetIP, UDP_PORT);
        udp.write((uint8_t*)&sensorData, sizeof(sensorData));
        udp.endPacket();
      }

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
