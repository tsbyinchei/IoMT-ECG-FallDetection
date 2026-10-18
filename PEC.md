# 📋 ĐẶC TẢ KỸ THUẬT VÀ TIÊU CHUẨN LÂM SÀNG (PEC)
## ĐỀ TÀI: "THIẾT KẾ HỆ THỐNG IoT THEO DÕI ĐIỆN TIM, THÂN NHIỆT VÀ CẢNH BÁO TÉ NGÃ TỪ XA CHO NGƯỜI CAO TUỔI TẠI NHÀ"
**Tác giả:** TsByinChei ([@tsbyinchei](https://github.com/tsbyinchei)) • [contact@tsbyin.dev](mailto:contact@tsbyin.dev)  
**Đề tài:** Thiết kế hệ thống IoT theo dõi điện tim, thân nhiệt và cảnh báo té ngã từ xa cho người cao tuổi tại nhà  
**Tiêu chuẩn áp dụng:** ITU-T Rec. Y.2060 (IoT), ANSI/AAMI EC13 (ECG Standard), IEC 60601-2-47  

---

## 🎯 1. MỤC TIÊU DỰ ÁN (PROJECT OBJECTIVES)

### 1.1. Mục tiêu Kỹ thuật (Engineering Goals)
1. **Thiết kế Node đeo gọn nhẹ:** Tích hợp khối đo điện tim ECG (AD8232), cảm biến gia tốc 3 trục (MPU-6050) và cảm biến thân nhiệt (DS18B20) trên vi điều khiển ESP32-C3 SuperMini với công suất tiêu thụ $< 150\text{mA}$.
2. **Lấy mẫu chính xác thời gian thực:** Tần số lấy mẫu ECG đạt chuẩn y tế **$250\text{ Hz} \pm 0.5\%$** (chu kỳ $4.0\text{ ms}$ cố định bằng Hardware Timer / non-blocking micros).
3. **Độ trễ truyền nhận thấp:** Đóng gói khung 25 mẫu ($100\text{ ms}$) truyền qua Wi-Fi UDP Socket nội bộ với độ trễ truyền dẫn $< 5\text{ ms}$, tỷ lệ mất gói $< 0.1\%$.
4. **Phân tầng tính toán biên (Edge Computing):** Gateway ESP32 tự động tính toán nhịp tim BPM tức thời, hiển thị trực quan trên màn hình OLED SH1106 và kích hoạt còi cảnh báo tại chỗ $< 200\text{ ms}$ khi xảy ra biến cố.

### 1.2. Mục tiêu Y sinh & Lâm sàng (Clinical Goals)
1. **Bóc tách đỉnh R chính xác:** Độ nhạy ($Se$) và độ đặc hiệu ($Sp$) bóc tách phức bộ QRS bằng thuật toán Pan-Tompkins đạt $> 95\%$ trên dữ liệu thực nghiệm.
2. **Khử nhiễu co cơ & Trôi đường đẳng điện:** Lọc bỏ hoàn toàn trôi baseline do hô hấp ($< 0.5\text{Hz}$) và nhiễu xoay chiều mạng điện 50Hz, nhiễu co cơ EMG ($> 30\text{Hz}$).
3. **Cảnh báo té ngã tin cậy:** Nhận dạng chính xác cú va đập té ngã ($SMV \ge 2.5g$) kết hợp kiểm định bất động sau ngã để giảm thiểu tối đa báo động giả (False Alarm) do ngồi nhanh hoặc nằm phịch xuống giường.

---

## ⚙️ 2. BẢNG THÔNG SỐ KỸ THUẬT CHI TIẾT (TECHNICAL SPECIFICATIONS)

| Tham số kỹ thuật | Yêu cầu thiết kế | Cấu hình định mức / Giải pháp | Tiêu chuẩn áp dụng |
| :--- | :--- | :--- | :--- |
| **Vi điều khiển Node** | Gọn nhẹ, hỗ trợ Wi-Fi & Timer phần cứng | ESP32-C3 SuperMini (RISC-V 160MHz / 80MHz) | Chuẩn nhúng IoT |
| **Vi điều khiển Gateway** | Đa nhân, xử lý song song SoftAP & MQTT | ESP32-WROOM-32 (Dual-core 240MHz) | FreeRTOS đa luồng |
| **Tần số lấy mẫu ECG ($F_s$)** | $250\text{ Hz}$ ($4.0\text{ ms}$/mẫu) | Định thời vi sai `micros()` chính xác cao | ANSI/AAMI EC13 |
| **Độ phân giải ADC** | $\ge 10\text{-bit}$ | $12\text{-bit}$ (0 – 4095 mức lượng tử) | ADC SAR nội chip (ADC1) |
| **Dải điện áp sinh lý** | $0\text{V} - 3.3\text{V}$ ($V_{REF} \approx 1.65\text{V}$) | Mạch tiền khuếch đại vi sai AD8232 | Chuẩn Analog Front-End |
| **Băng thông lọc số ECG** | $5\text{ Hz} - 15\text{ Hz}$ (Bandpass) | Lọc IIR Butterworth dải thông bậc 2 | Khử trôi đường đẳng điện |
| **Thời gian trơ sinh lý** | $300\text{ms} - 400\text{ms}$ | $380\text{ms}$ (95 mẫu @ 250Hz) | Chuẩn Pan-Tompkins 1985 |
| **Dải đo gia tốc MPU-6050** | $\pm 8g$ (bắt trọn xung va đập) | Cấu hình AFS_SEL = 2 ($\pm 8g$, 4096 LSB/g) | Giao tiếp I2C 400kHz |
| **Tần số quét MPU-6050** | $\ge 100\text{ Hz}$ | $125\text{ Hz}$ ($8.0\text{ ms}$, đồng bộ 2 mẫu ECG) | Bắt trọn xung lực tiếp đất |
| **Thuật toán nhận diện ngã** | Kiểm định 2 giai đoạn (2-Stage Verification) | Giai đoạn 1: $SMV \ge 2.8g$<br/>Giai đoạn 2: $Tilt \ge 40^\circ$ & $Gyro < 45^\circ/\text{s}$ | Chống báo động giả ADL |
| **Thời gian thẩm định ngã** | $2.0\text{s} - 3.0\text{s}$ | Cửa sổ phân tích bất động $2.5\text{ giây}$ | Động lực học ngã người già |
| **Dải đo thân nhiệt DS18B20** | $+15^\circ\text{C}$ đến $+50^\circ\text{C}$ | Cảm biến kỹ thuật số 12-bit (FSM 800ms non-blocking) | Giao thức 1-Wire (CRC8) |
| **Khung gói tin UDP (Cơ bản)** | $\le 100\text{ bytes}$ | Khung nhị phân Packed Struct 60 bytes | Tối ưu hóa băng thông |
| **Khung gói tin UDP (Mở rộng)**| Toàn vẹn tầng ứng dụng (End-to-End) | Khung nhị phân Packed Struct 62 bytes (+CRC-16) | IEC 62304 / ISO 13485 |
| **Độ trễ truyền dẫn nội bộ** | $< 10\text{ ms}$ | Wi-Fi UDP Socket điểm-điểm | Thời gian thực (< 5ms) |

---

## 📡 3. ĐẶC TẢ GIAO THỨC TRUYỀN THÔNG (COMMUNICATION PROTOCOLS)

### 3.1. Giao thức Tầng 1: Wi-Fi UDP Socket (Node $\to$ Gateway)
* **Chế độ Wi-Fi:** SoftAP trên Gateway (`BIOMED_GW`), Node ESP32-C3 kết nối kiểu Station (STA) với IP tĩnh:
  * Gateway IP: `192.168.4.1`
  * Node C3 IP: `192.168.4.2`
  * Subnet Mask: `255.255.255.0`
  * Cổng UDP Port: `4210`
* **Cấu trúc gói tin nhị phân chuẩn định mức (60 Bytes):**

| Trường dữ liệu | Kiểu dữ liệu | Kích thước | Mô tả |
| :--- | :--- | :--- | :--- |
| `ecgSamples[25]` | `uint16_t` | 50 Bytes | 25 mẫu ECG 12-bit liên tiếp ($4\text{ms} \times 25 = 100\text{ms}$) |
| `bodyTemp` | `float` | 4 Bytes | Thân nhiệt bề mặt cơ thể (°C) |
| `smv` | `float` | 4 Bytes | Vector độ lớn gia tốc $SMV$ ($g$) |
| `leadsOff` | `uint8_t` | 1 Byte | $0$: Tiếp xúc tốt \| $1$: Hở/tuột điện cực |
| `fallDetected` | `uint8_t` | 1 Byte | $0$: Bình thường \| $1$: Cảnh báo ngã |
| **TỔNG CỘNG** | | **60 Bytes** | Truyền nguyên khối bằng `udp.write()` |

* **Cấu trúc gói tin mở rộng chuẩn y tế (62 Bytes - Tích hợp CRC-16):**
  Bổ sung trường `crc16` (2 bytes, đa thức CRC-16-CCITT `0x1021`) vào cuối struct để thực hiện **End-to-End Application Integrity Check** theo tiêu chuẩn thiết bị y tế IEC 62304:

| Trường dữ liệu | Kiểu dữ liệu | Kích thước | Mô tả |
| :--- | :--- | :--- | :--- |
| `ecgSamples[25]` | `uint16_t` | 50 Bytes | 25 mẫu ECG 12-bit liên tiếp ($100\text{ms}$) |
| `bodyTemp` | `float` | 4 Bytes | Thân nhiệt bề mặt cơ thể (°C) |
| `smv` | `float` | 4 Bytes | Vector gia tốc toàn phần cực đại ($g$) |
| `leadsOff` | `uint8_t` | 1 Byte | Trạng thái tuột điện cực Smart Leads-off |
| `fallDetected` | `uint8_t` | 1 Byte | Cờ xác nhận té ngã 2 giai đoạn |
| `crc16` | `uint16_t` | 2 Bytes | Mã kiểm tra toàn vẹn CRC-16 (Đa thức 0x1021) |
| **TỔNG CỘNG** | | **62 Bytes** | Chuẩn hóa toàn vẹn cấu trúc bộ nhớ |

### 3.2. Giao thức Tầng 2: USB Serial Streaming (Gateway $\to$ MATLAB)
* **Baudrate:** `115200 bps`, 8-N-1.
* **Cú pháp bản tin ASCII:**
  ```text
  $DATA,<Temp>,<SMV>,<LeadsOff>,<FallDetected>,<EdgeBPM>,<S0>,<S1>,...,<S24>\n
  ```

### 3.3. Giao thức Tầng 3: Cloudflare Tunnel MQTT WSS Stream (Gateway $\to$ Cloud Broker)
* **Kiến trúc Broker:** EMQX v5 triển khai trên Ubuntu Server 1Panel (IP nội bộ `192.168.1.x`, cổng WebSocket `8083`).
* **Định tuyến toàn cầu:** Cloudflare Zero Trust Tunnel chuyển tiếp SSL/TLS an toàn qua Public Hostname:
  * **URI WSS:** `wss://mqtt.yourdomain.com/mqtt` (Port 443 WSS)
  * **Client ESP32:** Thư viện `esp_mqtt_client` (FreeRTOS Native Task).
  * **Xác thực an toàn:** Password-based Authentication (Tài khoản xác thực trên EMQX Broker của bạn).
* **Topic dữ liệu sinh hiệu:** `biomed/patient/data` (Publish chu kỳ 100ms)
* **Topic cảnh báo khẩn cấp:** `biomed/patient/alert` (Publish tức thời khi té ngã)
* **Định dạng tải trọng JSON:**
  ```json
  {
    "temp": 32.5,
    "smv": 1.02,
    "leadsOff": 0,
    "fall": 0,
    "bpm": 82,
    "ecg": [2580, 2605, 3350, 2710, 2590, ...]
  }
  ```

### 3.4. Giao thức Tầng 4: Cảnh báo Tức thời Đa Kênh (Telegram & Zalo Bot API)
* **Kênh Telegram Bot:**
  * **Endpoint:** `https://api.telegram.org/bot<TOKEN>/sendMessage`
  * **Bot định danh:** `@your_telegram_bot`
* **Kênh Zalo Bot Platform:**
  * **Endpoint:** `https://bot-api.zaloplatforms.com/bot<TOKEN>/sendMessage`
  * **Bot định danh:** `Bot loMT Assistant`
* **Cơ chế kích hoạt:** Bắn HTTP GET / POST bất đồng bộ qua `WiFiClientSecure` (bỏ qua check SSL cert để gửi song song trong $< 400\text{ms}$) khi cờ ngã `fallDetected` chuyển trạng thái $0 \to 1$.
* **Nội dung bản tin cảnh báo:** Bao gồm Lực va đập SMV ($g$), Nhịp tim hiện tại (BPM), Thân nhiệt ($^\circ\text{C}$) và định danh thiết bị IoMT Gateway.

## 🏥 4. TIÊU CHUẨN LÂM SÀNG & TIÊU CHÍ ĐÁNH GIÁ (V&V CRITERIA)

### 4.1. Tiêu chuẩn Nhịp tim Lâm sàng (AHA Standard)
* **Dải nhịp nghỉ ngơi bình thường:** $60 - 100\text{ BPM}$.
* **Cảnh báo nhịp chậm (Bradycardia):** $< 50\text{ BPM}$ (Còi bíp nhịp đôi).
* **Cảnh báo nhịp nhanh (Tachycardia):** $> 120\text{ BPM}$ (Còi bíp nhịp đôi).

### 4.2. Tiêu chuẩn Thân nhiệt Lâm sàng
* **Thân nhiệt bề mặt da (Skin Temperature):** $31.5^\circ\text{C} - 34.5^\circ\text{C}$ (Điều kiện phòng $28^\circ\text{C}$).
* **Nhiệt độ lõi ước lượng (Core Temperature Estimate):** Thân nhiệt da $+ 3.0^\circ\text{C} \approx 36.5^\circ\text{C} - 37.0^\circ\text{C}$.

### 4.3. Tiêu chuẩn Biến thiên Nhịp tim (HRV Analysis Metrics)
* **RMSSD (Root Mean Square of Successive Differences):** Đo lường hoạt động của hệ phó giao cảm tim mạch qua khoảng RR.
* **Poincaré Plot:**
  * $SD_1$: Độ phân tán ngắn hạn (Short-term variability - nhịp theo nhịp).
  * $SD_2$: Độ phân tán dài hạn (Long-term variability).
  * Tỷ số $SD_1 / SD_2$: Chỉ số cân bằng thần kinh tự chủ (Autonomic balance).

---

## 🔋 5. PHÂN TÍCH NĂNG LƯỢNG & DỰ TOÁN PIN (POWER BUDGET)

### 5.1. Bảng Dòng Điện Tiêu Thụ Từng Khối Phần Cứng
* **Điện áp danh định:** $3.3\text{V}$ (hoặc Pin Li-Po 1S $3.7\text{V} - 4.2\text{V}$ qua IC ổn áp LDO ME6211 dung sai thấp).
* **Chi tiết tiêu thụ dòng điện thành phần (Node C3):**
  * MPU-6050 (Chế độ đo gia tốc liên tục $125\text{ Hz}$): $\approx 3.8\text{ mA}$.
  * AD8232 (IC khuếch đại điện sinh học AFE): $\approx 0.17\text{ mA}$.
  * DS18B20 (Chuyển đổi nhiệt độ định kỳ, FSM non-blocking): $\approx 1.0\text{ mA}$.
  * ESP32-C3 (Xung nhịp $160\text{ MHz}$, phát Wi-Fi liên tục $100\text{ ms}$/lần): $\approx 85\text{ mA}$.

### 5.2. So Sánh Thời Lượng Vận Hành Theo 2 Cấu Hình Kỹ Thuật

| Cấu hình kỹ thuật | Xung nhịp CPU | Chế độ Wi-Fi | Dòng TB ($I_{avg}$) | Thời lượng pin Li-Po 1000mAh |
| :--- | :---: | :---: | :---: | :---: |
| **Cấu hình 1 (Mặc định)** | $160\text{ MHz}$ | Wi-Fi Active liên tục | $\approx 90\text{ mA}$ | **$8.8 - 10.0\text{ giờ}$** |
| **Cấu hình 2 (Tối ưu hóa)** | **$80\text{ MHz}$** | **Modem-sleep + Tắt BT** | **$\approx 55\text{ mA}$** | **$\approx 14.5\text{ giờ}$** |

* **Công thức tính thời lượng pin thực tế:**
  Áp dụng hệ số xả an toàn của pin Li-Po ($k_{safe} = 0.8$ nhằm bảo vệ tế bào pin không bị kiệt dưới ngưỡng $3.2\text{V}$):
  $$T_{life} = \frac{C_{pin} \times k_{safe}}{I_{avg}} = \frac{1000\text{ mAh} \times 0.8}{55\text{ mA}} \approx 14.55\text{ giờ}$$
  * **Ý nghĩa thực tiễn:** Thời lượng $14.5\text{ giờ}$ đáp ứng hoàn hảo chu kỳ sinh hoạt trọn vẹn 1 ngày của người cao tuổi (đeo ban ngày từ 6:00 sáng đến 20:30 tối, ban đêm đặt lên dock sạc nam châm).
  * **Định hướng thế hệ kế tiếp (Next-gen BLE 5.0):** Để nâng tuổi thọ pin lên nhiều ngày ($> 3 - 5\text{ ngày}$), hệ thống cần chuyển tầng vật lý từ Wi-Fi sang **Bluetooth Low Energy (BLE 5.0 Coded PHY)** với mức tiêu thụ trung bình chỉ $5 - 8\text{ mA}$.

---

## 🎯 6. TIÊU CHÍ NGHIỆM THU HỆ THỐNG (SYSTEM ACCEPTANCE CRITERIA)

Để nghiệm thu và đánh giá mức độ hoàn thiện của hệ thống, các tiêu chí kỹ thuật cần đạt bao gồm:

| Hạng mục kiểm thử | Tiêu chí nghiệm thu kỹ thuật | Phương pháp kiểm tra |
| :--- | :--- | :--- |
| **Độ ổn định chu kỳ lấy mẫu** | Tần số lấy mẫu ECG đạt $250\text{ Hz} \pm 1\%$ (chu kỳ $4.0\text{ ms}$) | Đo kiểm định thời vi sai `micros()` qua Serial Monitor |
| **Hiệu năng bóc tách đỉnh R** | Thuật toán Pan-Tompkins nhận diện chính xác đỉnh R, không đếm lặp sóng T | Đối chiếu biểu đồ nhãn đỉnh R trên MATLAB và OLED |
| **Kiểm định lọc nhiễu ECG** | Triệt tiêu hoàn toàn trôi baseline do hô hấp và nhiễu xoay chiều 50Hz | Phân tích bộ lọc số IIR Biquad Notch 50Hz + Bandpass |
| **Độ nhạy phát hiện té ngã** | Kích hoạt cảnh báo khi $SMV \ge 2.8g$ và góc nghiêng nằm sàn $Tilt \ge 40^\circ$ | Thử nghiệm mô phỏng va chạm và kiểm tra chốt 2.5s |
| **Khả năng chống báo động giả** | Tự động hủy cảnh báo nếu người dùng cử động bình thường ($Gyro \ge 45^\circ/\text{s}$) | Kiểm tra ngưỡng triệt tiêu báo động giả ADL |
| **Độ trễ kênh cảnh báo khẩn** | Tin nhắn khẩn cấp gửi đến Telegram Bot & Zalo Bot trong vòng $< 1.0\text{s}$ | Ghi log thời gian gửi HTTP POST trên Gateway |
| **Độ tin cậy truyền nhận UDP** | Hàng đợi LwIP được rút cạn liên tục, tỷ lệ mất gói tin $< 0.5\%$ | Vòng lặp `while (udp.parsePacket() > 0)` trên Gateway |
