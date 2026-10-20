# 🫀 Thiết kế Hệ thống IoT Theo dõi Điện tim, Thân nhiệt và Cảnh báo Té ngã Từ xa cho Người cao tuổi tại Nhà
### Dual-MCU IoMT Wearable System: Remote Monitoring of ECG, Body Temperature & Fall Detection for the Elderly at Home

[![Platform](https://img.shields.io/badge/Platform-ESP32%20%7C%20ESP32--C3-blue.svg)](https://www.espressif.com/)
[![Language](https://img.shields.io/badge/Language-C%2B%2B%20%7C%20MATLAB-orange.svg)](https://mathworks.com/)
[![Protocol](https://img.shields.io/badge/Protocols-Wi--Fi%20UDP%20%7C%20MQTT%20%7C%20Serial-green.svg)]()
[![Standard](https://img.shields.io/badge/Standard-ITU--T%20Y.2060%20IoMT-purple.svg)]()
[![Algorithm](https://img.shields.io/badge/Algorithm-Pan--Tompkins%20QRS-red.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## 📌 1. TỔNG QUAN HỆ THỐNG (SYSTEM OVERVIEW)

Hệ thống được thiết kế phục vụ đề tài **"Thiết kế hệ thống IoT theo dõi điện tim, thân nhiệt và cảnh báo té ngã từ xa cho người cao tuổi tại nhà"**, kết hợp kiến trúc **Internet of Medical Things (IoMT)** và **Xử lý Tín hiệu Y sinh (Biomedical Signal Processing - BDSP)**:

* **Node cảm biến đeo người (Wearable Sensor Node):** Sử dụng chip vi điều khiển **ESP32-C3 SuperMini (RISC-V 80MHz tối ưu năng lượng)** nhỏ gọn đeo trên ngực người cao tuổi, lấy mẫu điện tim ECG **250 Hz (chu kỳ 4ms)** qua cảm biến sinh học **AD8232**, đọc gia tốc 3 trục từ **MPU-6050** để tính vector gia tốc toàn phần $SMV = \sqrt{a_x^2 + a_y^2 + a_z^2}$, và theo dõi thân nhiệt qua **DS18B20**.
* **Đóng gói & Truyền thông không dây:** Dữ liệu được gom theo khung **25 mẫu ECG/100ms** thành struct nhị phân 62-byte (kèm CRC-16-CCITT bảo vệ toàn vẹn), truyền không dây qua **Wi-Fi UDP Socket** nội bộ tới Gateway với độ trễ cực thấp ($< 5\text{ms}$).
* **Gateway trung tâm (IoT Edge Gateway):** Sử dụng **ESP32 Dev Module (Dual-core Xtensa)** đặt tại nhà, phát mạng SoftAP riêng biệt, chạy thuật toán bóc tách đỉnh R thời gian thực (Edge BPM), hiển thị đa trang trên màn hình **OLED SH1106 1.3"** qua nút nhấn chuyển trang BOOT, còi chíp **Active Buzzer** báo động té ngã và tuột điện cực.
* **Xử lý tín hiệu chuyên sâu trên MATLAB (BDSP Center):** Thuật toán **Pan-Tompkins** hoàn chỉnh (Lọc dải thông số IIR 5–15Hz $\to$ Đạo hàm $\to$ Bình phương $\to$ Tích phân cửa sổ trượt MWI $\to$ Ngưỡng thích ứng kép với thời gian trơ sinh lý 380ms), bộ lọc trung vị (Median Filter) chống nhiễu co cơ (EMG), phân tích biến thiên nhịp tim HRV (Poincaré Plot, RMSSD, SD1, SD2), mô hình động học té ngã 3 pha và tự động xuất hồ sơ bệnh án chuẩn hóa (`.mat`).

---

## 🏛️ 2. KIẾN TRÚC HỆ THỐNG (SYSTEM ARCHITECTURE)

Hệ thống hỗ trợ 2 chế độ vận hành linh hoạt:

### 🔹 Chế độ 1: Mô hình Nội bộ Offline (Direct Local Demo)
Hoạt động độc lập không cần Internet hay Router ngoài, phục vụ kiểm thử nhanh tại phòng thí nghiệm hoặc báo cáo trực tiếp tại lớp.

```
       [ AD8232 ] (ECG - GPIO 1)
       [ DS18B20 ] (Nhiệt - GPIO 4)  
       [ MPU-6050 ] (Gia tốc - GPIO 6, 7)
                  | (Cáp tín hiệu)
                  v
       +----------------------+
       |  NODE: ESP32-C3      | (IP: 192.168.4.2)
       |  Lấy mẫu 250Hz (4ms) |
       +----------------------+
                  |
                  | Wi-Fi UDP Socket (Port 4210)
                  | (Kết nối AP: BIOMED_GW)
                  v
       +----------------------+
       |  GATEWAY: ESP32      | (IP: 192.168.4.1 - SoftAP)
       |  - Bóc tách gói UDP  |
       |  - Edge BPM & Filter |
       +----------+-----------+
                  |
        +---------+---------+
        |                   |
        v                   v
+---------------+   +---------------+
| OLED SH1106   |   | ACTIVE BUZZER | (Còi hú cảnh báo ngã & tuột cực)
| (I2C 128x64)  |   | (GPIO 23)     |
+---------------+   +---------------+
        |
        | UART USB Serial (115200 Baud)
        v
+------------------------------------+
| TRẠM XỬ LÝ MATLAB (OFFLINE)        |
| - Đọc luồng bản tin $DATA          |
| - Thuật toán Pan-Tompkins R-Peaks  |
| - Phát hiện té ngã ngưỡng 2.5g     |
| - Xuất file ECG_Patient_Record.mat |
+------------------------------------+
```

---

### 🔹 Chế độ 2: Kiến trúc IoMT Chuẩn ITU-T Rec. Y.2060 (Server / Cloud)
```
+===================================================================================+
| 4. TẦNG ỨNG DỤNG (Application Layer)                                              |
|    - Web Dashboard PWA: IoMT Telehealth Monitor (https://iomt.tsbyin.dev)         |
|    - Quản trị Broker: EMQX Console Dashboard                                      |
|    - Cảnh báo khẩn cấp tức thời: Telegram Bot (@your_telegram_bot) & Zalo Bot     |
|    - Phân tích chuyên sâu: MATLAB Biomedical Signal Processing Toolbox            |
+===================================================================================+
                                      ▲
                                      │ WSS / HTTPS (Port 443 - Cloudflare Tunnel)
+===================================================================================+
| 3. TẦNG HỖ TRỢ DỊCH VỤ & ỨNG DỤNG (Service & Application Support Layer)           |
|    - Hạ tầng: Ubuntu Server cá nhân quản lý qua 1Panel (192.168.1.36)             |
|    - MQTT Broker: EMQX (Cổng WebSocket 8083 -> Domain: wss://mqtt.tsbyin.dev/mqtt)|
|    - Cầu nối dữ liệu: tools/mqtt_to_influxdb.py (Docker Bridge Network)           |
|    - Cơ sở dữ liệu chuỗi thời gian: InfluxDB v2 (Lưu ECG 250Hz, Temp, SMV, Replay)|
|    - Bảo mật & Chứng chỉ số: Cloudflare Zero Trust SSL/TLS 1.3 Tunnel             |
+===================================================================================+
                                      ▲
                                      │ MQTT WSS / TCP (Wi-Fi / 4G Hotspot)
+===================================================================================+
| 2. TẦNG MẠNG (Network Layer)                                                      |
|    - Gateway: ESP32-WROOM-32 (Chế độ Wi-Fi kép AP + STA)                          |
|      + AP Mode: BIOMED_GW (Giao tiếp UDP nội bộ 192.168.4.1)                      |
|      + STA Mode: Đẩy dữ liệu lên Cloud qua wss://mqtt.tsbyin.dev/mqtt             |
|      + Khách thể ngầm: Native esp_mqtt_client FreeRTOS đa luồng                   |
|    - Giao thức nội bộ: Wi-Fi UDP Socket (Port 4210, 62 bytes/packet)              |
+===================================================================================+
                                      ▲
                                      │ Gói tin UDP (25 mẫu/packet @ 250Hz)
+===================================================================================+
| 1. TẦNG THIẾT BỊ (Device Layer)                                                   |
|    - Node thu thập: ESP32-C3 SuperMini (Hardware Timer lấy mẫu 4ms)               |
|    - Cảm biến sinh học (Sensors):                                                 |
|      + AD8232: Điện tâm đồ ECG Analog (ADC 12-bit)                                |
|      + DS18B20: Thân nhiệt bề mặt (1-Wire Protocol)                               |
|      + MPU-6050: Gia tốc 3 trục phát hiện té ngã (I2C Standard 100kHz)            |
|    - Cơ cấu chấp hành (Actuators):                                                |
|      + Màn hình OLED SH1106: Giao diện 4 trang lâm sàng (Báo Cloud WSS)           |
|      + Active Buzzer: Báo động âm thanh tần số cao                                |
+===================================================================================+
```

---

## 🔌 3. SƠ ĐỒ ĐẤU NỐI PHẦN CỨNG (PINOUT MAPPING)

### 📌 Khối 1: Node cảm biến đeo người (ESP32-C3 SuperMini)

| Module cảm biến | Chân cảm biến | Chân ESP32-C3 SuperMini | Ghi chú kỹ thuật |
| :--- | :--- | :--- | :--- |
| **Nguồn chung** | VCC / 3.3V | **3.3** | Nguồn 3.3V ổn định từ IC nguồn onboard |
| **Mass chung** | GND | **G (GND)** | Mass chung cho toàn bộ cảm biến |
| **AD8232 (ECG)** | OUTPUT | **GPIO 1 (A1)** | Tín hiệu điện tim Analog (ADC 12-bit: 0–4095) |
| | SDN | **3.3** | Kéo mức cao (HIGH) để kích hoạt IC chạy |
| | LO- | **GPIO 2** | Phát hiện hở điện cực âm |
| | LO+ | **GPIO 3** | Phát hiện hở điện cực dương |
| **DS18B20 (Nhiệt)** | DATA | **GPIO 4** | Đường truyền 1-Wire (Kèm trở kéo $4.7\text{k}\Omega$ lên 3.3V) |
| **MPU-6050 (Gia tốc)** | SDA | **GPIO 6** | Dữ liệu $I^2C$ Data (Tần số 100kHz) |
| | SCL | **GPIO 7** | Xung nhịp $I^2C$ Clock |

> ⚠️ **Lưu ý vị trí chân ESP32-C3 SuperMini:** Chân `1 (A1)` nằm ở **cột bên phải, chân thứ 2 từ dưới đếm lên** (ngay trên chân 0).

---

### 📌 Khối 2: Gateway trung tâm (ESP32 Dev Module)

| Thiết bị ngoại vi | Chân thiết bị | Chân ESP32 Gateway | Chức năng |
| :--- | :--- | :--- | :--- |
| **Nguồn & Mass** | VCC / GND | **3.3V / GND** | Nguồn nuôi mạch |
| **OLED SH1106 1.3"** | SDA | **GPIO 21** | Giao tiếp $I^2C$ Data mặc định |
| | SCL | **GPIO 22** | Giao tiếp $I^2C$ Clock mặc định |
| **Active Buzzer** | Cực dương (+) | **GPIO 23** | Kích hoạt còi hú cảnh báo ($V_{out} = 3.3\text{V}$) |
| | Cực âm (-) | **GND** | Mass còi |
| **Nút bấm BOOT** | Onboard | **GPIO 0** | Nút bấm chuyển đổi vòng lặp 4 trang OLED |

---

## 📦 4. CẤU TRÚC GÓI TIN TRUYỀN THÔNG (DATA PACKET FRAME)

Dữ liệu truyền qua Wi-Fi UDP Socket sử dụng cấu trúc `struct` nhị phân packed 62 bytes (kèm CRC-16 bảo vệ toàn vẹn y tế):

```cpp
typedef struct __attribute__((packed)) struct_message {
  uint16_t ecgSamples[25];  // 50 bytes: 25 mẫu ECG 12-bit (250Hz, 100ms dữ liệu)
  float bodyTemp;           // 4 bytes: Nhiệt độ cơ thể (°C)
  float smv;                // 4 bytes: Signal Magnitude Vector gia tốc (g)
  uint8_t leadsOff;         // 1 byte: 1 = Hở điện cực | 0 = Bình thường
  uint8_t fallDetected;     // 1 byte: 1 = Phát hiện té ngã | 0 = Bình thường
  uint16_t crc16;           // 2 bytes: CRC-16-CCITT kiểm tra toàn vẹn gói tin
} struct_message;           // Tổng kích thước: Đúng 62 bytes
```

Định dạng bản tin xuất qua cổng Serial cho MATLAB:
```
$DATA,<Temp>,<SMV>,<LeadsOff>,<FallDetected>,<EdgeBPM>,<ECG_0>,...,<ECG_24>\n
```

---

## 🔬 5. THUẬT TOÁN XỬ LÝ TÍN HIỆU (ALGORITHMS)

### 1. Thuật toán Pan-Tompkins bóc tách đỉnh R (QRS Detection)
* **Bộ lọc thông dải (Bandpass Filter 5–15Hz):** Lọc bỏ trôi đường đẳng điện do nhịp thở ($< 0.5\text{Hz}$) và nhiễu xoay chiều 50Hz, co cơ EMG ($> 30\text{Hz}$). Sử dụng bộ nhớ trạng thái `z_bp` trong MATLAB triệt tiêu hoàn toàn hiện tượng rung cạnh khi ghép khối 25 mẫu.
* **Đạo hàm bậc 1 5-điểm (Five-Point Derivative):** Làm nổi bật sườn dốc đứng của phức bộ QRS:
  $$y[n] = \frac{1}{8T} \left( 2x[n] + x[n-1] - x[n-3] - 2x[n-4] \right)$$
* **Bình phương (Squaring):** Khuếch đại phi tuyến tính năng lượng QRS, chuyển toàn bộ biên độ âm thành dương: $y[n] = (x[n])^2$.
* **Tích phân cửa sổ trượt (Moving Window Integrator - MWI):** Cửa sổ tích phân 37.5ms (38 mẫu) trích xuất năng lượng thực tế của phức bộ sóng.
* **Thời gian trơ sinh lý (Refractory Period):** Khóa nhận đỉnh trong **380ms** (95 mẫu @ 250Hz) sau mỗi đỉnh R để loại bỏ 100% hiện tượng đếm đúp sóng T (T-wave oversensing).
* **Bộ lọc trung vị (Median Filter) RR:** Tính nhịp tim BPM từ trung vị các khoảng RR hợp lệ ($0.4\text{s} \le RR \le 1.4\text{s}$), loại bỏ hoàn toàn các ngoại tâm thu hoặc đỉnh nhiễu co cơ đột ngột.

### 2. Thuật toán Phát hiện Té ngã Động lực học 3 Pha
* **Vector độ lớn gia tốc (SMV):**
  $$SMV = \sqrt{a_x^2 + a_y^2 + a_z^2}$$
* **Mô hình 3 pha Y sinh:**
  1. *Pha rơi tự do (Free-fall):* $SMV < 0.6g$ trong khoảng $100 - 250\text{ms}$.
  2. *Pha va đập (Impact):* Xung va đập chạm sàn vượt ngưỡng $SMV \ge 2.5g$.
  3. *Pha bất động & Đổi tư thế (Post-fall Inactivity):* Thân người nằm im trên sàn $\ge 2\text{s}$ ($\sigma(SMV) < 0.15g$) và góc nghiêng trọng lực lệch $> 60^\circ$ so với phương thẳng đứng trước va chạm.

---

## 🖥️ 6. GIAO DIỆN MÀN HÌNH OLED 4 TRANG (OLED MULTI-PAGE UI)

Bấm nút **BOOT (GPIO 0)** trên Gateway ESP32 để chuyển tuần tự 4 trang thông tin:

* **Trang 1 (ECG Realtime Wave):** Vẽ sóng điện tim ECG chạy thời gian thực với cơ chế tự co giãn biên độ động (Dynamic Auto-ranging), hiển thị BPM và cảnh báo `[LEAD OFF]`.
* **Trang 2 (Clinical Vitals):** Bảng tổng kết sinh hiệu lâm sàng (Nhịp tim, Thân nhiệt, Cảnh báo sốt/nhịp chậm).
* **Trang 3 (Motion & Fall Monitor):** Thanh đo gia tốc SMV trực quan, đếm tổng số lần té ngã ghi nhận được.
* **Trang 4 (IoMT System Stats):** Thống kê số lượng gói UDP thu nhận, địa chỉ IP SoftAP và trạng thái kết nối Node.
* **Popup Khẩn cấp:** Khi có biến cố té ngã, khung cảnh báo **`! FALL DETECTED !`** sẽ chớp sáng đè lên mọi trang kèm còi hú tần số cao.

---

## 🌐 7. GIAO DIỆN WEB PWA & GIÁM SÁT TỪ XA (TELEHEALTH DASHBOARD)

Hệ thống cung cấp giao diện Web PWA chuẩn y tế tại địa chỉ: **[https://iomt.tsbyin.dev](https://iomt.tsbyin.dev)**

* **Đồ thị Điện tim Oscilloscope 60 FPS:** Bộ quét phosphor y tế khử giật lag, tia quét laser dẫn đầu, hỗ trợ tự động co giãn biên độ động (Dynamic Auto-Gain).
* **Dải HUD Sinh hiệu Lâm sàng (Clinical Telemetry HUD Strip):** Tích hợp đèn xung nhịp tim Visual Pulse LED `#heartPulse`, ruy-băng nhiệt độ lâm sàng `#tempNeedle`, và đồng hồ gia tốc 2 phân tầng `#smvGauge` có vạch báo động té ngã $2.5g$.
* **Hệ thống Thanh bên Di động (Mobile Drawer System):** Tự động chuyển đổi mượt mà giữa thanh ngang trên Desktop và Drawer trượt trên Mobile ($\le 1024\text{px}$). Cơ chế cô lập Stacking Context (`z-index: 100000 / 100001 / 99990`) và lớp nền phẳng không mờ đảm bảo hiển thị sắc nét tuyệt đối.
* **Tự động đóng và đồng bộ co giãn (Adaptive Resize Sync):** Tự động giải phóng trạng thái mở khi chuyển đổi màn hình di động $\to$ máy tính và đồng bộ canvas qua `requestAnimationFrame`.
* **Phát lại Biến cố InfluxDB (60s Fall Event Replay):** Tải và phát lại dải sóng 60 giây quanh thời điểm té ngã với tần số thực $250\text{ Hz}$.
* **Điều khiển Còi hú từ xa (Remote Mute):** Gửi lệnh MQTT tới topic `biomed/gateway/cmd` dập tắt còi Active Buzzer trên kit Gateway ngay tức thì.
* **Cài đặt Ứng dụng PWA (v1.9):** Hỗ trợ Add to Home Screen và vận hành offline qua Service Worker `sw.js`.

---

## 🚀 8. HƯỚNG DẪN CÀI ĐẶT VÀ CHẠY THỬ (STEP-BY-STEP)

### Bước 1: Nạp Node Cảm Biến (ESP32-C3)
1. Mở Arduino IDE, cắm ESP32-C3 vào cổng COM.
2. Chọn Board: **ESP32C3 Dev Module** (hoặc ESP32-C3 SuperMini).
3. Cấu hình Tools: `USB CDC On Boot: Enabled`.
4. Mở file [ESP32C3 Example/ESP32C3 Example.ino](ESP32C3%20Example/ESP32C3%20Example.ino) (hoặc thư mục nạp trực tiếp `ESP32C3/ESP32C3.ino` trên máy phát triển) và nhấn **Upload**.

### Bước 2: Nạp Gateway Trung Tâm (ESP32)
1. Mở file [ESP32 Example/ESP32 Example.ino](ESP32%20Example/ESP32%20Example.ino) (hoặc cấu hình `secrets.h` từ file mẫu [ESP32 Example/secrets_example.h](ESP32%20Example/secrets_example.h)):
   ```c
   #define SECRET_ROUTER_SSID "Tên_WiFi_Nhà_Bạn"
   #define SECRET_ROUTER_PASS "Mật_Khẩu_WiFi"
   #define SECRET_MQTT_URI    "wss://mqtt.tsbyin.dev/mqtt"
   #define SECRET_MQTT_USER   "YOUR_MQTT_USERNAME"
   #define SECRET_MQTT_PASS   "YOUR_MQTT_PASSWORD"
   #define TELEGRAM_BOT_TOKEN "Token_Telegram_Bot_Của_Bạn"
   #define TELEGRAM_CHAT_ID   "Chat_ID_Của_Bạn"
   #define ZALO_BOT_TOKEN     "Token_Zalo_Bot_Của_Bạn"
   #define ZALO_CHAT_ID       "Chat_ID_Zalo_Của_Bạn"
   ```
2. Cắm kit ESP32 Gateway vào cổng COM (ví dụ COM10).
3. Chọn Board: **ESP32 Dev Module**.
4. Cài đặt thư viện: `U8g2` (Thư viện `esp_mqtt_client` đã tích hợp sẵn trong ESP32 Core).
5. Nhấn **Upload** để nạp code vào Gateway (Thư mục nạp đầy đủ thông tin chuẩn `ESP32/` đã được `.gitignore` bảo vệ độc lập).
6. Màn hình OLED sẽ sáng lên, phát SoftAP `BIOMED_GW`, kết nối Wi-Fi Router, đồng bộ WSS với Cloudflare Tunnel và gửi cảnh báo tự động qua Telegram & Zalo Bot khi phát hiện té ngã.

### Bước 3: Chạy MATLAB Giám Sát & Phân Tích Thực Nghiệm
1. Tạo file cấu hình bảo mật `MATLAB/config_private.m` từ file mẫu [MATLAB/config_example.m](MATLAB/config_example.m).
2. **Chế độ 1 (Qua cổng USB COM):** Đóng Serial Monitor trong Arduino IDE, mở MATLAB và chạy:
   ```matlab
   run('MATLAB/matlab_serial_ecg_processor.m')
   ```
3. **Chế độ 2 (Qua Cloud MQTT không dây):** Chạy script nhận dữ liệu từ xa:
   ```matlab
   run('MATLAB/matlab_mqtt_ecg_processor.m')
   ```
4. Bấm nút đỏ **"DỪNG & XUẤT FILE .MAT"** để lưu hồ sơ `ECG_Patient_Record.mat`.
5. Chạy tiếp file phân tích chuyên sâu:
   ```matlab
   run('MATLAB/analyze_patient_record.m')
   ```
   MATLAB sẽ tự động tính toán các chỉ số lâm sàng (HRV, SDNN, RMSSD, SD1/SD2) và **tự động xuất 4 đồ thị 300 DPI** vào thư mục `MATLAB/figures/` để dán vào báo cáo!

### Bước 4: Chạy Cầu nối InfluxDB & Mở Web Dashboard
1. Chạy dịch vụ cầu nối MQTT sang InfluxDB chuỗi thời gian:
   ```bash
   python tools/mqtt_to_influxdb.py
   # hoặc chạy qua Docker:
   docker compose -f tools/docker/docker-compose-bridge.yml up -d
   ```
2. Mở trình duyệt truy cập: **[https://iomt.tsbyin.dev](https://iomt.tsbyin.dev)** để theo dõi trực tiếp dạng sóng ECG 60 FPS, sinh hiệu và nhận cảnh báo té ngã thời gian thực.

---

## 📑 HỆ THỐNG TÀI LIỆU DỰ ÁN (DOCUMENTATION DIRECTORY)

> 💡 **Khám phá tài liệu chuyên sâu:** Bấm vào các liên kết bên dưới để xem trực tiếp các tài liệu kỹ thuật, thuật toán và hướng dẫn làm báo cáo của dự án:

| Tài liệu | Phân loại | Mô tả nội dung chính | Liên kết trực tiếp |
| :--- | :--- | :--- | :--- |
| 📋 **PEC.md** | **Đặc tả Kỹ thuật & Lâm sàng** | Tiêu chuẩn ITU-T Y.2060, ANSI/AAMI EC13, bảng thông số ADC/Fs, giao thức UDP/MQTT, dự toán nguồn pin Li-Po. | 👉 **[Xem tài liệu PEC.md](PEC.md)** |
| 📖 **SYSTEM_ARCHITECTURE.md** | **Kiến trúc Kỹ thuật Hệ thống** | Đặc tả kiến trúc kỹ thuật hệ thống IoT theo dõi điện tim, thân nhiệt và cảnh báo té ngã từ xa cho người cao tuổi tại nhà. | 👉 **[Xem tài liệu SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md)** |
| 📐 **ALGORITHM.md** | **Cơ sở Toán học & Thuật toán** | Chi tiết giải thuật Pan-Tompkins 1985 (hàm Z, vi phân, bình phương, MWI) & Động lực học kiểm định té ngã 3 pha. | 👉 **[Xem tài liệu ALGORITHM.md](ALGORITHM.md)** |
| 📖 **REPORT_GUIDE.md** | **Khung Báo cáo Môn học** | Khung sườn 6 chương chuẩn hóa cho bài báo cáo 25–30 trang môn Thiết kế Hệ thống IoT & Xử lý Tín hiệu Y sinh. | 👉 **[Xem tài liệu REPORT_GUIDE.md](REPORT_GUIDE.md)** |
| 📜 **LICENSE** | **Giấy phép Bản quyền** | Giấy phép mã nguồn mở MIT License (cho phép tự do sử dụng, chỉnh sửa và phân phối). | 👉 **[Xem giấy phép MIT](LICENSE)** |
| 🛡️ **SECURITY.md** | **Chính sách An toàn** | Khuyến cáo an toàn điện học y sinh IEC 60601-1, cách ly nguồn pin và bảo mật dữ liệu MQTT qua Cloudflare Tunnel. | 👉 **[Xem tài liệu SECURITY.md](SECURITY.md)** |
| 🤝 **CONTRIBUTING.md** | **Hướng dẫn Đóng góp** | Quy chuẩn đóng gói code Arduino, tiêu chuẩn xử lý tín hiệu MATLAB và quy trình Pull Request. | 👉 **[Xem tài liệu CONTRIBUTING.md](CONTRIBUTING.md)** |
| 📜 **CODE_OF_CONDUCT.md** | **Quy tắc Ứng xử** | Bộ quy tắc ứng xử chuẩn Contributor Covenant v2.1 cho cộng đồng nghiên cứu khoa học. | 👉 **[Xem tài liệu CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)** |
| 💻 **ESP32C3 Example** | **Mã nguồn Node Cảm biến** | Lấy mẫu ECG 250Hz, MPU-6050 SMV, DS18B20 1-Wire, đóng gói UDP 62-byte (CRC-16), hạ xung 80MHz tiết kiệm pin. | 👉 **[Xem code ESP32-C3](ESP32C3%20Example/ESP32C3%20Example.ino)** |
| 📟 **ESP32 Example** | **Mã nguồn Gateway** | SoftAP UDP Server, giao diện OLED 4 trang, nút BOOT chuyển trang, còi Buzzer GPIO 23, lọc nhịp tim. | 👉 **[Xem code Gateway](ESP32%20Example/ESP32%20Example.ino)** |
| 📊 **MATLAB Scripts** | **Trạm Xử lý Tín hiệu** | Pan-Tompkins thời gian thực với chấm đỏ đỉnh R, bộ lọc trung vị Median, xuất file .MAT và phân tích HRV. | 👉 **[Xem thư mục MATLAB](MATLAB/)** |
| 🌐 **Web Dashboard** | **Giao diện Theo dõi Realtime** | Màn hình PWA v1.9 60 FPS, dải HUD Sinh hiệu, InfluxDB Replay 60s & còi báo Web Audio API ([https://iomt.tsbyin.dev](https://iomt.tsbyin.dev)). | 👉 **[Xem Web Dashboard](web/)** |

---

## 👥 TÁC GIẢ VÀ BẢN QUYỀN (AUTHOR & COPYRIGHT)
* **Tác giả / Nghiên cứu phát triển:** **TsByinChei**
* **GitHub:** [@tsbyinchei](https://github.com/tsbyinchei)
* **Email liên hệ:** [contact@tsbyin.dev](mailto:contact@tsbyin.dev) • [tsbyinchei@gmail.com](mailto:tsbyinchei@gmail.com)
* **Đề tài:** Thiết kế hệ thống IoT theo dõi điện tim, thân nhiệt và cảnh báo té ngã từ xa cho người cao tuổi tại nhà.
* **Môn học:** Thiết kế Hệ thống IoT & Xử lý Tín hiệu Y sinh (Biomedical Signal Processing - BDSP).
* **Giấy phép:** Toàn bộ mã nguồn và tài liệu được phát hành theo giấy phép mã nguồn mở **[MIT License](LICENSE)** (Copyright © 2026 TsByinChei).

---

## 📚 TÀI LIỆU THAM KHẢO & LỜI CẢM ƠN (ACKNOWLEDGEMENTS & REFERENCES)
1. **Thuật toán Pan-Tompkins QRS Detection:**
   * J. Pan and W. J. Tompkins, *"A Real-Time QRS Detection Algorithm,"* IEEE Transactions on Biomedical Engineering, vol. BME-32, no. 3, pp. 230-236, 1985.
2. **Thuật toán Phát hiện Té ngã & Góc nghiêng MPU-6050:**
   * Tham khảo kiến trúc kiểm định động học té ngã (Góc nghiêng Tilt Angle và độ bất động sau ngã Gyroscope) từ dự án nguồn mở: [Circuit-Digest/Elderly-Fall-Detection-System](https://github.com/Circuit-Digest/Elderly-Fall-Detection-System).
3. **Tiêu chuẩn Kiến trúc Mạng IoMT:**
   * ITU-T Recommendation Y.2060, *"Overview of the Internet of Things,"* International Telecommunication Union.
   * ANSI/AAMI EC13:2002, *"Cardiac monitors, heart rate meters, and alarms,"* Association for the Advancement of Medical Instrumentation.
