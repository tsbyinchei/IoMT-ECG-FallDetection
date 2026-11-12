# CẨM NANG 60 CÂU HỎI VẤN ĐÁP BẢO VỆ ĐỒ ÁN TỐT NGHIỆP
## HỆ THỐNG IOT THEO DÕI ĐIỆN TIM, THÂN NHIỆT VÀ CẢNH BÁO TÉ NGÃ TỪ XA CHO NGƯỜI CAO TUỔI TẠI NHÀ
**Tác giả:** TsByinChei ([@tsbyinchei](https://github.com/tsbyinchei)) • [contact@tsbyin.dev](mailto:contact@tsbyin.dev)  
**Hệ thống thực nghiệm:** [https://iomt.tsbyin.dev](https://iomt.tsbyin.dev) • **Mã nguồn:** ESP32-C3 Sensor Node, ESP32 Gateway, Web PWA Canvas 2D

---

## 🎯 PHẦN ĐẶC BIỆT: GIẢI MÃ NHẬN XÉT CỦA THẦY & CHIẾN LƯỢC BẢO VỆ
> **Lời nhận xét của Thầy:** *"Hệ thống khá phức tạp có EN, GW, EMQX, WebDashboard (Canvas 2D), Cloudflare Tunnel, InfluxDB, Telegram Bot và Zalo OA. Demo Breadboard."*

### 1. Ý nghĩa thực sự đằng sau nhận xét của Thầy
* **Mặt tích cực (Thầy đánh giá đề tài điểm A):** Thầy công nhận khối lượng công việc Full-stack đồ sộ từ Thiết bị biên (EN) $\rightarrow$ Gateway (GW) $\rightarrow$ Mạng nội bộ $\rightarrow$ Đám mây $\rightarrow$ Web thời gian thực $\rightarrow$ Di động.
* **Mặt nghi ngờ (Thầy chuẩn bị chất vấn):**
  1. *Liệu có bị over-engineering (làm màu/vẽ việc) không?* Nếu Cloudflare sập hay mất mạng Internet thì người già ngã có được cứu không?
  2. *Liệu có tự code từ đầu không, hay chỉ copy các thư viện ráp nối lại?*
  3. *Tại sao lại demo Breadboard mà không làm mạch in PCB?* Dây cắm lỏng lẻo có làm sai lệch tín hiệu điện tim y tế không?

### 2. Chiến lược trả lời "Ăn điểm tuyệt đối"
* **Khi thầy hỏi về "Hệ thống quá phức tạp / Nhiều tầng":**
  > *"Thưa thầy, hệ thống được thiết kế nghiêm ngặt theo **triết lý phân tầng độc lập (Decoupling Architecture) chuẩn ITU-T Y.2060** nhằm đảm bảo an toàn y sinh cao nhất:*  
  > * **Tầng biên (EN + GW) hoàn toàn ĐỘC LẬP:** Mạch đeo EN và trạm GW giao tiếp qua Wi-Fi nội bộ bằng UDP. Kể cả khi đứt cáp quang Internet, sập Cloudflare hay mất mạng toàn nhà, hệ thống đo đạc ECG và còi hú báo ngã tại chỗ **vẫn hoạt động 100% thời gian thực**.*  
  > * **Tầng đám mây (EMQX, InfluxDB, Web, Telegram/Zalo) chỉ đóng vai trò VIỄN TRẮC (Telemetry):** Phục vụ người thân theo dõi từ xa khi đi làm và lưu trữ lịch sử y tế. Sự cố mạng ngoài đường tuyệt đối không ảnh hưởng đến tính năng cứu sinh tại chỗ."*
* **Khi thầy hỏi về "Demo Breadboard":**
  > *"Thưa thầy, việc triển khai trên Breadboard là **bước đi bắt buộc trong giai đoạn thử nghiệm kiểm chứng nguyên lý (Proof-of-Concept - PoC)**:*  
  > * Giúp nhóm em dễ dàng đo kiểm dòng tiêu thụ thực tế của từng linh kiện (đo được dòng tĩnh $22\text{ mA}$, dòng phát Wi-Fi $44\text{ mA}$), từ đó tính toán chính xác dung lượng pin $1000\text{ mAh}$ dùng được $14.2\text{ tiếng}$.*  
  > * Giúp đo kiểm nhiễu điện từ thực tế (EMI/RF) giữa ăng-ten Wi-Fi của ESP32-C3 và bộ khuếch đại tương tự AD8232 để tối ưu hóa bộ lọc số IIR $5 - 15\text{ Hz}$ trong code.*  
  > * Đây là bước chuẩn bị kỹ thuật để hoàn thiện sơ đồ nguyên lý trước khi đặt gia công mạch in PCB 2 lớp SMD và in vỏ hộp 3D y tế trong giai đoạn thương mại hóa tiếp theo."*

---

## 🔍 BẢNG TỪ KHÓA TRA CỨU NHANH BẰNG CTRL + F (TIẾNG VIỆT KHÔNG DẤU)
> *Mẹo thực chiến: Khi thầy hỏi vào chức năng nào, mở file code tương ứng, bấm `Ctrl + F`, gõ từ khóa không dấu là con trỏ nhảy ngay đến dòng code!*

| Chức năng thầy hỏi | Từ khóa Ctrl + F (Không dấu) | Mở file nào? | Ý nghĩa tóm tắt |
| :--- | :--- | :---: | :--- |
| **Định thời lấy mẫu ECG 250Hz** | `TAG_LAY_MAU_250HZ` hoặc `LAY_MAU` | `ESP32C3.ino` | Dùng `micros()` vi sai chu kỳ 4.0ms, không trôi pha |
| **Bắt xung va chạm ngã 2.8g** | `TAG_VA_DAP_NGA` hoặc `2.8G` | `ESP32C3.ino` | Giai đoạn 1: $SMV \ge 2.8g$, mở cửa sổ thẩm định 2.5s |
| **Thẩm định nằm im sau ngã** | `TAG_THAM_DINH_NGA` hoặc `THAM_DINH` | `ESP32C3.ino` | Giai đoạn 2: $Tilt \ge 40^\circ$ và $Gyro < 45^\circ/\text{s}$ |
| **Chống tuột điện cực giả** | `TAG_TUOT_DIEN_CUC` hoặc `TUOT_DIEN_CUC` | `ESP32C3.ino` | Xử lý kẹt chân Strapping Pin GPIO 2 và bão hòa ADC |
| **Đo nhiệt độ DS18B20 1-Wire** | `TAG_NHIET_DO_FSM` hoặc `NHIET_DO` | `ESP32C3.ino` | FSM 2 bước non-blocking, chờ 800ms không khóa CPU |
| **Đóng gói UDP nhị phân 62B** | `TAG_DONG_GOI_UDP` hoặc `62BYTES` | `ESP32C3.ino` | Packed struct gom 25 mẫu ECG + thân nhiệt + CRC16 |
| **Mã kiểm tra toàn vẹn CRC-16** | `TAG_CRC16` hoặc `CRC16` | Cả 2 file | Đa thức 0x1021 bảo vệ 100% lỗi sai lệch bit |
| **Hạ xung CPU 80MHz tiết kiệm pin** | `TAG_HA_XUNG` hoặc `80MHZ` | `ESP32C3.ino` | Hạ xung 80MHz và tắt Bluetooth tiết kiệm 35% pin |
| **Tự động chuyển mạng Wi-Fi** | `TAG_ROAMING_WIFI` hoặc `ROAMING` | `ESP32C3.ino` | Tự đổi giữa SoftAP Gateway và Wi-Fi gia đình |
| **Bóc tách đỉnh R Pan-Tompkins**| `TAG_PAN_TOMPKINS` hoặc `PAN_TOMPKINS`| `ESP32.ino` | Đạo hàm 5 điểm, bình phương phi tuyến, MWI |
| **Khóa trơ sinh lý chống đúp sóng T** | `TAG_KHOA_TRO` hoặc `380MS` | `ESP32.ino` | Khóa trơ 380ms (95 mẫu @ 250Hz) chuẩn tim mạch |
| **Lọc trung vị loại ngoại tâm thu** | `TAG_LOC_TRUNG_VI` hoặc `TRUNG_VI` | `ESP32.ino` | Lọc trung vị 7 khoảng RR + trung bình trượt EMA |
| **Bộ lọc số IIR thông dải 5-15Hz**| `TAG_LOC_IIR` hoặc `5_15HZ` | `ESP32.ino` | Triệt hoàn toàn nhiễu thở 0.5Hz và điện lưới 50Hz |
| **Điều khiển còi hú khẩn cấp** | `TAG_COI_BUZZER` hoặc `COI` | `ESP32.ino` | Ưu tiên còi ngã cao nhất, chốt giữ liên tục |
| **Nút bấm BOOT tắt còi tại chỗ** | `TAG_NUT_BOOT` hoặc `NUT_BOOT` | `ESP32.ino` | Nút GPIO 0 giải tỏa còi sau khi đã sơ cứu |
| **Gửi cảnh báo Telegram Bot** | `TAG_TELEGRAM` hoặc `TELEGRAM` | `ESP32.ino` | HTTPS GET/POST API gửi báo động đỏ kèm nhịp tim |
| **Gửi thông báo Zalo OA** | `TAG_ZALO` hoặc `ZALO` | `ESP32.ino` | Webhook gửi Zalo Notification Service |
| **Đồng bộ hóa Telegram & Zalo** | `TAG_DONG_BO_NOTIFY` hoặc `DONG_BO` | `ESP32.ino` | Broadcast song song 100% nội dung giống nhau |
| **Chống rớt gói tin TLS/HTTPS** | `TAG_CHONG_ROT_TLS` hoặc `ROT_TLS` | `ESP32.ino` | Chờ server ACK drain buffer 1.5s + đệm delay 200ms |
| **Báo cáo định kỳ chuẩn AHA** | `TAG_BAO_CAO_DINH_KY` hoặc `DINH_KY` | `ESP32.ino` | Định kỳ 30p, phân loại >105 BPM nhịp nhanh, <50 chậm |

---

# BỘ 60 CÂU HỎI VẤN ĐÁP KỸ THUẬT & TRUY VẾT MÃ NGUỒN

## PHẦN 1: PHẦN CỨNG & SƠ ĐỒ NGUYÊN LÝ VI MẠCH (CÂU 1 – 10)

### Câu 1: Tại sao khối Node đeo người lại chọn ESP32-C3 SuperMini mà không dùng ESP32 thường, ESP8266 hay Arduino Nano?
* **Bản chất câu hỏi:** Khả năng lựa chọn linh kiện phù hợp với bài toán công thái học và năng lượng y tế.
* **Trả lời:**
  1. *Kích thước siêu nhỏ:* Bản SuperMini chỉ bằng 2 đốt ngón tay ($22.5 \times 18\text{ mm}$), siêu nhẹ để đeo trước ngực.
  2. *Kiến trúc RISC-V 32-bit tiết kiệm điện:* Cho phép hạ xung xuống `80MHz` (`setCpuFrequencyMhz(80)` tại dòng 382 của `ESP32C3.ino`) giảm nhiệt độ chip và tiết kiệm $35\%$ dòng tĩnh.
  3. *Tích hợp sẵn Wi-Fi/BLE:* Arduino Nano hay STM32 phải câu thêm module ngoài cồng kềnh; ESP8266 thì CPU yếu, ADC chỉ có 10-bit và thiếu các tập lệnh xử lý số DSP.

### Câu 2: Chân ngõ ra tín hiệu ECG của AD8232 cắm vào chân nào của ESP32-C3? Tại sao cắm chân đó?
* **Bản chất câu hỏi:** Hiểu biết về phần cứng ADC và xung đột ngoại vi trên ESP32.
* **Trả lời:** Cắm vào **GPIO 1** (`ECG_PIN = 1`, kênh `ADC1_CH1`). Tuyệt đối không cắm vào các chân thuộc `ADC2` (như GPIO 0, 2, 3, 4, 5) vì khi bật Wi-Fi lên, driver Wi-Fi của ESP32 sẽ độc chiếm toàn bộ bộ chuyển đổi `ADC2`, gây ra lỗi không đọc được analog hoặc đọc ra giá trị rác.

### Câu 3: Hãy giải thích hiện tượng "kẹt chân" Strapping Pin GPIO 2 và GPIO 8 trên ESP32-C3?
* **Bản chất câu hỏi:** Khả năng làm chủ sơ đồ chân vi điều khiển thực tế.
* **Trả lời:**
  * **GPIO 8:** Là chân Strapping Pin và gắn sẵn LED trên bo. Khi boot phải ở mức logic HIGH để chip vào chế độ SPI Boot bình thường.
  * **GPIO 2 (MTMS):** Trên kit SuperMini đã có sẵn điện trở kéo lên $3.3\text{V}$. Khi nối chân `LO-` của AD8232 vào GPIO 2, hàm `digitalRead(LO_MINUS_PIN)` sẽ luôn trả về `HIGH` ngay cả khi đang dán điện cực bình thường. Trong code em đã xử lý bằng thuật toán **Rail Saturation Check** (dòng 557 của `ESP32C3.ino`) kiểm tra dải điện áp ADC thực tế ($100 < ADC < 4050$) để không bị báo động hở mạch giả.

### Câu 4: Cảm biến thân nhiệt DS18B20 giao tiếp theo chuẩn gì? Cần linh kiện phụ trợ nào trên mạch?
* **Bản chất câu hỏi:** Kiến thức bus giao tiếp 1-Wire.
* **Trả lời:** Chuẩn giao tiếp **Dallas 1-Wire** trên chân **GPIO 4**. Bus này có cấu trúc cực thu hở (Open-Drain), bắt buộc phải có một điện trở kéo lên **Pull-up $4.7\text{ k}\Omega$** nối từ chân dữ liệu `DQ` lên nguồn $3.3\text{V}$ để giữ đường truyền ở mức cao khi các thiết bị nhả bus.

### Câu 5: Cảm biến MPU-6050 kết nối với ESP32-C3 qua bus gì? Địa chỉ I2C là bao nhiêu?
* **Bản chất câu hỏi:** Giao tiếp I2C và định địa chỉ phần cứng.
* **Trả lời:** Giao tiếp qua **I2C Fast-mode $400\text{ kHz}$** trên chân **GPIO 6 (SDA)** và **GPIO 7 (SCL)**. Địa chỉ I2C mặc định là `0x68` (khi chân `AD0` nối GND). Trong code hàm `initMPU6050()` (dòng 180 của `ESP32C3.ino`), có cơ chế tự động quét cả địa chỉ dự phòng `0x69` nếu chân `AD0` bị treo lên $3.3\text{V}$.

### Câu 6: Tại sao còi Buzzer ở trạm Gateway không nối thẳng vào chân GPIO của ESP32 mà phải qua Transistor?
* **Bản chất câu hỏi:** An toàn dòng điện và bảo vệ cổng vi điều khiển.
* **Trả lời:** Chân GPIO của ESP32 chỉ cấp dòng tối đa $12 - 20\text{ mA}$. Còi Active Buzzer khi kêu hết công suất cần dòng từ $30 - 50\text{ mA}$. Nếu kích trực tiếp sẽ gây sụt áp nguồn vi điều khiển, làm chip reset hoặc cháy cổng GPIO. Do đó phải dùng transistor NPN (như S8050) làm khóa chuyển mạch đệm dòng.

### Câu 7: Mạch nguồn của Node đeo người được thiết kế như thế nào? Dùng pin gì?
* **Bản chất câu hỏi:** Khối quản lý nguồn (Power Management).
* **Trả lời:** Dùng **pin sạc Li-Po 3.7V dung lượng 1000mAh**, tích hợp mạch sạc và bảo vệ qua cổng Type-C (module TP4056 có chức năng bảo vệ quá nạp $4.2\text{V}$, quá xả $< 2.5\text{V}$ và ngắn mạch). Nguồn $3.7\text{V}$ sau đó đi qua IC ổn áp LDO ME6211 trên kit SuperMini để hạ áp sạch xuống $3.3\text{V}$ cấp cho vi điều khiển và các cảm biến.

### Câu 8: Màn hình OLED SH1106 ở Gateway giao tiếp thế nào? Khác gì với màn hình SSD1306?
* **Bản chất câu hỏi:** Phân biệt ngoại vi hiển thị.
* **Trả lời:** Giao tiếp I2C tại địa chỉ `0x3C` qua chân **GPIO 21 (SDA)** và **GPIO 22 (SCL)** của ESP32 Gateway. Màn SH1106 có kích thước $1.3\text{ inch}$ (lớn hơn màn SSD1306 $0.96\text{ inch}$), bộ đệm RAM bên trong là $132 \times 64$ điểm ảnh (SSD1306 là $128 \times 64$), do đó khi khởi tạo thư viện `U8g2` phải cấu hình đúng driver `U8G2_SH1106_128X64_NONAME_F_HW_I2C` để không bị lệch 2 pixel viền màn hình.

### Câu 9: Nút bấm trên Gateway dùng để làm gì? Được nối vào chân nào?
* **Bản chất câu hỏi:** Cơ cấu giải tỏa còi (Fail-safe reset).
* **Trả lời:** Nút bấm dùng để **tắt còi khẩn cấp tại chỗ** sau khi người nhà đã tiếp cận hỗ trợ người già bị ngã. Em tận dụng trực tiếp chân **GPIO 0 (Nút BOOT có sẵn trên kit ESP32)** với cấu hình `pinMode(BUTTON_PIN, INPUT_PULLUP)` (dòng 52 của `ESP32.ino`), không cần tốn thêm linh kiện nút bấm ngoài.

### Câu 10: Tín hiệu tương tự từ AD8232 đi vào ESP32 được cấu hình độ phân giải bao nhiêu bit?
* **Bản chất câu hỏi:** Cấu hình ADC trong firmware.
* **Trả lời:** Cấu hình **ADC 12-bit** (`analogReadResolution(12)` tại dòng 397 của `ESP32C3.ino`), giá trị số trả về từ $0$ đến $4095$ tương ứng với dải điện áp $0\text{V} - 3.3\text{V}$ nhờ thiết lập độ suy hao `analogSetAttenuation(ADC_11db)`.

---

## PHẦN 2: FIRMWARE ESP32-C3 & XỬ LÝ THỜI GIAN THỰC (CÂU 11 – 20)

### Câu 11: Tần số lấy mẫu tín hiệu ECG là bao nhiêu? Định thời trong code ở đâu?
* **Bản chất câu hỏi:** Tần số lấy mẫu chuẩn y sinh và kỹ thuật định thời.
* **Trả lời:** Tần số chuẩn y tế **$F_s = 250\text{ Hz}$** (chu kỳ đúng $4.0\text{ ms} = 4000\mu\text{s}$). Trong `ESP32C3.ino` (dòng 517), sử dụng hàm `micros()` vi sai:
  ```cpp
  if (currentMicros - lastSampleMicros >= 4000) { ... }
  ```
  Cách này đảm bảo chu kỳ lấy mẫu cực kỳ chuẩn xác, không bị hiện tượng trôi pha lũy kế (Accumulated Timing Jitter).

### Câu 12: Tại sao trong hàm lấy mẫu lại dùng `sampleIndex % 2 == 0` để đọc MPU-6050?
* **Bản chất câu hỏi:** Đa nhiệm chia tần số (Time-division multiplexing).
* **Trả lời:** Tại dòng 525 của `ESP32C3.ino`, ECG cần lấy mẫu $250\text{ Hz}$ ($4\text{ms}/lần$), còn cảm biến quán tính MPU-6050 chỉ cần lấy mẫu ở tần số **$125\text{ Hz}$ ($8\text{ms}/lần$)** là đủ bắt trọn động học rơi ngã. Việc chia đôi tần số giúp giảm tải bus I2C và tiết kiệm năng lượng CPU.

### Câu 13: Đọc cảm biến nhiệt độ DS18B20 mất tới 750ms để chuyển đổi 12-bit, làm sao không bị treo luồng lấy mẫu ECG?
* **Bản chất câu hỏi:** Kỹ thuật lập trình Non-blocking FSM.
* **Trả lời:** Sử dụng **Máy trạng thái hữu hạn (FSM) 2 bước** trong hàm `processTemperature()` (dòng 126 của `ESP32C3.ino`):
  * *Bước 1 (`TRIGGER_CONVERT`):* Phát lệnh bắt đầu đo `0x44` rồi thoát ra ngay.
  * *Bước 2 (`WAIT_READ`):* Đếm bằng `millis()`, trong suốt $800\text{ ms}$ chờ đợi CPU vẫn lấy mẫu ECG $250\text{ Hz}$ bình thường. Hết $800\text{ ms}$ mới quay lại phát lệnh `0xBE` để đọc 9 byte Scratchpad.

### Câu 14: Tại sao gói tin UDP từ C3 sang Gateway lại gom 25 mẫu ECG mà không gửi từng mẫu một?
* **Bản chất câu hỏi:** Tối ưu hóa ngăn xếp mạng (Network Overhead Optimization).
* **Trả lời:** Nếu mỗi mẫu $4\text{ms}$ gửi 1 gói tin thì tần số phát Wi-Fi là $250\text{ gói/giây}$, chi phí tiêu tốn cho tiêu đề gói UDP/IP (Header Overhead) sẽ làm nghẽn sóng và làm cạn kiệt pin trong vòng vài tiếng. Gom **25 mẫu = 100ms** (dòng 532) giảm tần số phát xuống chỉ còn **$10\text{ gói/giây}$**, độ trễ chỉ $0.1\text{ giây}$ nhưng tiết kiệm hơn $70\%$ năng lượng vô tuyến.

### Câu 15: Từ khóa `__attribute__((packed))` trong định nghĩa `struct_message` có tác dụng gì?
* **Bản chất câu hỏi:** Kiến trúc bộ nhớ vi điều khiển và căn chỉnh byte.
* **Trả lời:** Tại dòng 101 của `ESP32C3.ino`, trình biên dịch 32-bit thường tự động chèn thêm các byte rỗng (Padding bytes) để căn chỉnh bộ nhớ theo bội số 4 byte. Thuộc tính `packed` ép trình biên dịch triệt tiêu hoàn toàn padding bytes, đảm bảo cấu trúc gói tin cố định chính xác đúng **62 bytes** trên cả vi điều khiển RISC-V (C3) và Xtensa (Gateway), tránh lỗi lệch trường dữ liệu khi giải mã nhị phân.

### Câu 16: Mã kiểm tra toàn vẹn CRC-16-CCITT hoạt động thế nào trong code?
* **Bản chất câu hỏi:** An toàn truyền thông dữ liệu y tế.
* **Trả lời:** Hàm `calculateCRC16()` (dòng 75) sử dụng đa thức sinh chuẩn $X^{16} + X^{12} + X^5 + 1$ (`0x1021`). Trước khi gửi gói UDP, Node C3 tính CRC trên 60 byte dữ liệu đầu và gán vào 2 byte cuối. Gateway khi nhận gói tin tính lại CRC; nếu kết quả không khớp (do nhiễu đường truyền làm sai bit), gói tin sẽ bị hủy ngay lập tức, không đưa dữ liệu sai vào đồ thị.

### Câu 17: Hãy chỉ vị trí code hạ xung nhịp CPU xuống 80MHz và tắt Bluetooth?
* **Bản chất câu hỏi:** Khả năng định vị mã nguồn tối ưu năng lượng.
* **Trả lời:** Trong hàm `setup()` của `ESP32C3.ino`:
  * Dòng 382: `setCpuFrequencyMhz(80);` hạ xung từ 160MHz xuống 80MHz.
  * Dòng 385: `btStop();` đóng hoàn toàn phần cứng vô tuyến Bluetooth, triệt tiêu dòng rò tĩnh.

### Câu 18: Khi mất kết nối Wi-Fi, Node C3 có tiếp tục đo nhịp tim và phát hiện ngã không?
* **Bản chất câu hỏi:** Tiêu chí an toàn Fail-safe.
* **Trả lời:** **Có, hoạt động 100%!** Tại dòng 478 của `ESP32C3.ino`, trong khối `if (WiFi.status() != WL_CONNECTED)`, **tuyệt đối không dùng `delay()` hay lệnh `return`**. Vi điều khiển vẫn tiếp tục chạy tuần tự xuống dưới để đọc ECG $250\text{ Hz}$ và chạy thuật toán nhận diện ngã, chỉ tạm dừng lệnh `udp.endPacket()` cho đến khi có mạng trở lại.

### Câu 19: Dải đo của MPU-6050 được cấu hình là bao nhiêu? Tại sao không dùng mặc định $\pm 2g$?
* **Bản chất câu hỏi:** Cấu hình thanh ghi cảm biến MEMS.
* **Trả lời:** Tại dòng 210 của `ESP32C3.ino`, ghi giá trị `0x10` vào thanh ghi `ACCEL_CONFIG (0x1C)` để chọn dải đo **$\pm 8g$** (độ nhạy $4096\text{ LSB}/g$). Dải mặc định $\pm 2g$ quá hẹp, khi người già ngã va đập xuống sàn xung lực thường đạt $2.5g - 4.5g$ sẽ làm cảm biến bị bão hòa chạm trần (cắt đỉnh), mất thông tin gia tốc thực tế.

### Câu 20: Hệ số chuyển đổi từ vận tốc góc thô sang độ/giây ($^\circ/\text{s}$) trong code là bao nhiêu?
* **Bản chất câu hỏi:** Hiểu biết về datasheet MPU-6050.
* **Trả lời:** Tại dòng 282 của `ESP32C3.ino`, chia cho **`131.0f`** vì thanh ghi Gyroscope đang cấu hình dải $\pm 250^\circ/\text{s}$ (mặc định `FS_SEL = 0`), tương ứng với độ nhạy $131\text{ LSB}/(^\circ/\text{s})$.

---

## PHẦN 3: THUẬT TOÁN ĐIỆN TIM ECG & PAN-TOMPKINS (CÂU 21 – 30)

### Câu 21: Thuật toán Pan-Tompkins trong code của em chạy ở Node C3 hay chạy ở Gateway? Tại sao?
* **Bản chất câu hỏi:** Kiến trúc phân bổ tải tính toán (Edge Computing Architecture).
* **Trả lời:** Chạy tại **trạm Gateway ESP32** trong hàm `processEdgeBPM()` (dòng 377 của `ESP32.ino`). Lý do: Node C3 phải ưu tiên toàn bộ tài nguyên CPU cho việc lấy mẫu ngắt chuẩn xác $250\text{ Hz}$ và tiết kiệm pin. Gateway cắm nguồn $220\text{V}$ trực tiếp và có CPU 2 nhân mạnh mẽ ($240\text{ MHz}$), rất phù hợp để chạy các thuật toán số học nặng như đạo hàm, MWI và lọc trung vị.

### Câu 22: Hãy chỉ ra 5 bước của thuật toán Pan-Tompkins trong hàm `processEdgeBPM`?
* **Bản chất câu hỏi:** Nắm vững thuật toán kinh điển bóc tách phức bộ QRS.
* **Trả lời:**
  1. *Bước 1 (Đạo hàm 5 điểm):* Dòng 381: `deriv = (2.0f * hp + ptHp1 - ptHp3 - 2.0f * ptHp4) * 0.125f;` lấy độ dốc lớn nhất của sườn sóng R.
  2. *Bước 2 (Bình phương phi tuyến):* Dòng 383: `float dsq = deriv * deriv;` làm dương hóa dữ liệu và khuếch đại đỉnh R nổi bật khỏi nhiễu nền.
  3. *Bước 3 (Tích phân cửa sổ trượt MWI):* Dòng 386-389: Cửa sổ trượt dài $25\text{ mẫu} = 100\text{ ms}$ tại $250\text{ Hz}$ gom toàn bộ năng lượng khử cực thất thành 1 xung chuông.
  4. *Bước 4 (Cập nhật ngưỡng thích ứng):* Dòng 392-395: Đỉnh suy hao chậm `mwiPeak *= 0.999f` và ngưỡng đặt tại $35\%$ đỉnh (`thr = 0.35f * mwiPeak`).
  5. *Bước 5 (Bắt đỉnh & thời gian trơ):* Dòng 401: Điều kiện phát hiện đỉnh R kèm khóa trơ $380\text{ ms}$.

### Câu 23: "Thời gian trơ sinh lý" (Refractory Period) trong code được đặt là bao nhiêu? Có ý nghĩa y học gì?
* **Bản chất câu hỏi:** Kiến thức sinh lý học tim mạch gắn liền với thuật toán.
* **Trả lời:** Đặt là **$380\text{ ms}$** tương ứng với **$95\text{ mẫu}$** tại $250\text{ Hz}$ (điều kiện `edgeSampleCounter - lastPeakSample > 95` tại dòng 401 của `ESP32.ino`). Ý nghĩa y học: Sau khi tâm thất co bóp (phức bộ QRS), cơ tim bước vào giai đoạn trơ tuyệt đối không thể bị kích thích tiếp. Khóa trơ $380\text{ ms}$ ngăn thuật toán không đếm nhầm sóng T (tái cực thất) biên độ cao thành một nhịp tim thứ hai (chống đếm đúp nhịp).

### Câu 24: Tại sao lại dùng bộ lọc trung vị (Median Filter) 7 phần tử cho khoảng RR?
* **Bản chất câu hỏi:** Kỹ thuật loại bỏ ngoại lai (Outlier Rejection).
* **Trả lời:** Tại dòng 418-428 của `ESP32.ino`, nhịp tim người già hay gặp hiện tượng ngoại tâm thu hoặc bị giật cơ làm xuất hiện 1 khoảng RR quá ngắn hoặc quá dài bất thường. Bộ lọc trung vị lấy 7 khoảng RR liên tiếp và chọn giá trị nằm chính giữa sau khi sắp xếp, triệt tiêu hoàn toàn các giá trị đột biến ngoại lai mà không làm trễ pha nhịp tim.

### Câu 25: Sau bộ lọc trung vị, nhịp tim còn đi qua bộ lọc nào nữa không?
* **Bản chất câu hỏi:** Bộ lọc làm mịn nhịp tim (EMA).
* **Trả lời:** Có, đi qua bộ lọc **trung bình trượt có trọng số EMA** (Exponential Moving Average) tại dòng 435 của `ESP32.ino`:
  `edgeBpm = (edgeBpm * 13 + calculatedBpm * 3) / 16;`
  Trọng số chiếm $81.25\%$ lịch sử và $18.75\%$ giá trị mới, giúp chỉ số BPM trên màn hình OLED và Web nhảy số êm ái, không bị rung lắc giật cục từng giây.

### Câu 26: Nhiễu điện lưới 50Hz và nhiễu trôi đường cơ sở do thở được triệt tiêu ở đâu?
* **Bản chất câu hỏi:** Thiết kế bộ lọc số dải thông.
* **Trả lời:** Được triệt tiêu bởi **Bộ lọc số dải thông IIR SOS (Second-Order Sections) $5 - 15\text{ Hz}$** trong hàm `filterGatewayEcg()` (dòng 345 của `ESP32.ino`). Tần số cắt thấp $5\text{ Hz}$ chặn hoàn toàn sóng trôi do hô hấp ($0.15 - 0.5\text{ Hz}$), tần số cắt cao $15\text{ Hz}$ dập tắt hoàn toàn nhiễu từ trường điện lưới $50\text{ Hz}$.

### Câu 27: Nếu người bệnh bị nhịp tim chậm hoặc ngừng tim thì hệ thống xử lý thế nào?
* **Bản chất câu hỏi:** Xử lý điều kiện biên (Edge case timeouts).
* **Trả lời:** Tại dòng 445 của `ESP32.ino`, nếu sau **$875\text{ mẫu} = 3.5\text{ giây}$** mà không phát hiện được bất kỳ đỉnh R nào, biến `edgeBpm` lập tức được gán về `0` để kích hoạt trạng thái cảnh báo khẩn cấp trên OLED và Web.

### Câu 28: Dải nhịp tim sinh lý người bình thường được giới hạn trong khoảng nào trong code?
* **Bản chất câu hỏi:** Hạn định sinh học của thuật toán.
* **Trả lời:** Tại dòng 409 của `ESP32.ino`, khoảng RR hợp lệ được kiểm tra từ **$400\text{ ms}$ đến $1400\text{ ms}$**, tương ứng với dải tần số tim từ **$43\text{ BPM}$ đến $150\text{ BPM}$**. Mọi xung nhiễu ngoài dải này đều bị loại bỏ.

### Câu 29: Thuật toán nhận diện tuột điện cực (Leads-off) trong code phân biệt thế nào giữa hở mạch và tim ngừng đập?
* **Bản chất câu hỏi:** Phân biệt lỗi kỹ thuật phần cứng và bệnh lý.
* **Trả lời:**
  * *Hở điện cực (Leads-off):* Dòng 562 của `ESP32C3.ino` kiểm tra chân `LO-`/`LO+` và bão hòa điện áp ADC ($ADC \ge 4080$ hoặc $ADC \le 50$). Khi đó cờ `leadsOff = 1`, hệ thống báo *"HỞ ĐIỆN CỰC"*.
  * *Ngừng tim thật sự:* Điện cực vẫn tiếp xúc tốt ($100 < ADC < 4050$), đường tín hiệu là đường đẳng điện phẳng lì quanh mức $1.65\text{V}$, `leadsOff = 0` nhưng không có đỉnh R $\rightarrow$ kích hoạt cảnh báo tim ngừng đập nguy kịch.

### Câu 30: Đồ thị sóng ECG trên màn hình OLED mini được tự động co giãn biên độ thế nào?
* **Bản chất câu hỏi:** Thuật toán Auto-ranging đồ họa.
* **Trả lời:** Trong hàm `updateWaveformBuffer()` (dòng 453 của `ESP32.ino`), theo dõi đỉnh biên độ `oledPeak`. Nếu biên độ sóng tăng thì mở rộng khung, nếu giảm thì suy hao chậm `oledPeak *= 0.998f` (ngưỡng sàn tối thiểu $80$). Điều này giúp sóng ECG của người có điện thế tim yếu hay mạnh đều hiển thị vừa vặn trong khung 40 pixel của OLED.

---

## PHẦN 4: THUẬT TOÁN NHẬN DIỆN TÉ NGÃ & MPU-6050 (CÂU 31 – 40)

### Câu 31: Công thức tính véc-tơ gia tốc toàn phần (SMV) là gì?
* **Bản chất câu hỏi:** Hiểu biết công thức nền tảng nhận diện quán tính.
* **Trả lời:** Tại dòng 288 của `ESP32C3.ino`:
  $$SMV = \sqrt{a_x^2 + a_y^2 + a_z^2}$$
  Đơn vị tính là $g$ ($1g \approx 9.81\text{ m/s}^2$). Ở trạng thái đứng yên, $SMV \approx 1.0g$ do chịu lực hút của trọng trường Trái Đất.

### Câu 32: Thuật toán nhận diện té ngã 2 giai đoạn của em hoạt động như thế nào?
* **Bản chất câu hỏi:** Luồng máy trạng thái FSM nhận diện té ngã.
* **Trả lời:**
  * **Giai đoạn 1 (Xung va đập):** Dòng 328 của `ESP32C3.ino`, khi $SMV \ge 2.8g$, phát hiện có va chạm mạnh. **Tuyệt đối chưa hú còi ở đây!** Mở cửa sổ thẩm định $2.5\text{ giây}$.
  * **Giai đoạn 2 (Thẩm định sau 2.5s):** Dòng 350, kiểm tra ĐỒNG THỜI 2 tiêu chí:
    1. Góc nghiêng cơ thể $Tilt \ge 40^\circ$ (đang nằm ngang sàn so với tư thế ban đầu).
    2. Bất động sau ngã: Vận tốc góc tổng $Gyro < 45^\circ/\text{s}$ và gia tốc ổn định quanh trọng lực ($|SMV - 1.0g| < 0.35g$).
  * Nếu thỏa mãn cả 2 tiêu chí mới chính thức xác nhận ngã thật (`sensorData.fallDetected = 1`).

### Câu 33: Góc nghiêng cơ thể (Tilt Angle) được tính theo công thức nào trong code?
* **Bản chất câu hỏi:** Hình học không gian và tích vô hướng véc-tơ.
* **Trả lời:** Tại dòng 321-324 của `ESP32C3.ino`, tính qua **tích vô hướng (Dot Product)** giữa véc-tơ gia tốc tức thời $\vec{A}$ và véc-tơ trọng trường tham chiếu $\vec{G}_{base}$:
  $$\cos(\theta) = \frac{a_x \cdot baseGx + a_y \cdot baseGy + a_z \cdot baseGz}{|\vec{A}| \cdot |\vec{G}_{base}|}$$
  Sau đó lấy góc: $\theta = \arccos(\cos(\theta)) \times \frac{180}{\pi}$.

### Câu 34: Nếu người bệnh đeo thiết bị bị lệch hoặc nghiêng góc thì thuật toán tính góc nghiêng có bị sai không?
* **Bản chất câu hỏi:** Thuật toán bù góc tự thích ứng (Adaptive Gravity Calibration).
* **Trả lời:** **Không bị sai!** Tại dòng 298-317 của `ESP32C3.ino`, có thuật toán **Adaptive Gravity Reference**: Khi người dùng mới đeo thiết bị ở trạng thái tĩnh ban đầu ($0.85g \le SMV \le 1.15g$), hệ thống tự động ghi nhận hướng trọng lực hiện tại làm vector gốc `baseG`. Thậm chí trong lúc sinh hoạt tĩnh, vector gốc này liên tục được cập nhật làm mịn bằng bộ lọc IIR ($98\%$ cũ + $2\%$ mới) để chống trôi.

### Câu 35: Tại sao lại chọn ngưỡng va đập $FALL\_THRESHOLD = 2.8g$? Nếu đặt $2.0g$ thì sao?
* **Bản chất câu hỏi:** Chọn tham số thực nghiệm tối ưu.
* **Trả lời:** Các hoạt động sinh hoạt thường nhật (ADL) như ngồi nhanh xuống ghế đệm, bước mạnh xuống bậc cầu thang có thể tạo xung lực từ $1.8g - 2.4g$. Nếu đặt ngưỡng $2.0g$ sẽ gây ra rất nhiều báo động giả (False Positives). Đặt $2.8g$ vừa đủ cao để vượt qua các hoạt động thường ngày nhưng vẫn bắt trọn các cú ngã tự do tiếp đất.

### Câu 36: Tại sao sau khi ngã phải chờ tới 2.5 giây mới báo động? Chờ lâu thế có nguy hiểm không?
* **Bản chất câu hỏi:** Y nghĩa y học của khoảng thời gian chờ (Post-fall immobility window).
* **Trả lời:** $2.5\text{ giây}$ là khoảng thời gian tối ưu trong y học lão khoa. Nếu người già chỉ vấp chân nhẹ rồi tự đứng dậy ngay (không có chấn thương nằm liệt), hệ thống sẽ tự hủy báo động, không gây hoảng loạn cho gia đình. $2.5\text{ giây}$ là cực kỳ ngắn và hoàn toàn nằm trọn trong "thời gian vàng cấp cứu".

### Câu 37: Cờ báo ngã `fallDetected` sau khi kích hoạt được giữ trong bao lâu trên gói tin UDP?
* **Bản chất câu hỏi:** Cơ chế chốt giữ cảnh báo (Latching).
* **Trả lời:** Dòng 356 của `ESP32C3.ino`, đặt biến đếm `fallHoldCounter = 40`. Vì chu kỳ gửi gói UDP là $100\text{ ms}$, nên cờ báo ngã được **chốt giữ liên tục trong đúng $4.0\text{ giây}$ (40 gói tin)**. Điều này đảm bảo trạm Gateway và Web Server không bị mất gói tin cảnh báo do chập chờn Wi-Fi tức thời.

### Câu 38: Nhảy mạnh lên nệm rồi nằm im có làm hệ thống báo ngã không? Tại sao?
* **Bản chất câu hỏi:** Hiểu sâu về bản chất vật lý và giới hạn của cảm biến gia tốc.
* **Trả lời:** Có báo ngã, và đây là **hợp lý tuyệt đối về mặt vật lý**! Hành động nhảy mạnh tạo xung va chạm $> 2.8g$, sau đó nằm im úp mặt tạo góc nghiêng $> 40^\circ$ và vận tốc góc $< 45^\circ/\text{s}$, hoàn toàn thỏa mãn mọi tiêu chí động học của một cú ngã thật.

### Câu 39: Ngã trượt người từ từ theo vách tường xuống sàn hệ thống có bắt được không? Khắc phục thế nào?
* **Bản chất câu hỏi:** Nắm rõ điểm hạn chế và hướng phát triển của đề tài.
* **Trả lời:** Cú ngã trượt từ từ (Sliding Fall) gia tốc biến thiên chậm, không tạo ra pha rơi tự do và không đạt xung va đập $2.8g$ nên thuật toán gia tốc thuần túy sẽ bị bỏ sót. Hướng khắc phục: Tích hợp thêm **cảm biến áp suất khí quyển độ nhạy cao (như BMP280/SPL06)** để đo độ cao; khi nhận thấy độ cao trọng tâm tụt đột ngột từ $1.2\text{ m}$ xuống sát sàn thì kích hoạt báo ngã.

### Câu 40: Vận tốc góc tổng `lastTotalGyro` được tính như thế nào?
* **Bản chất câu hỏi:** Đo lường trạng thái chuyển động xoay.
* **Trả lời:** Tại dòng 285 của `ESP32C3.ino`:
  `lastTotalGyro = fabsf(gx_dps) + fabsf(gy_dps) + fabsf(gz_dps);`
  Lấy tổng giá trị tuyệt đối vận tốc góc của cả 3 trục tính theo độ/giây. Khi người bệnh nằm bất động, giá trị này luôn nhỏ hơn $45^\circ/\text{s}$.

---

## PHẦN 5: FIRMWARE GATEWAY ESP32 & TRUYỀN THÔNG (CÂU 41 – 50)

### Câu 41: Tại sao giữa Node C3 và Gateway lại dùng giao thức UDP Socket mà không dùng TCP?
* **Bản chất câu hỏi:** Phân tích giao thức mạng tầng giao vận (Transport Layer).
* **Trả lời:**
  * Tín hiệu y tế ECG phát thời gian thực liên tục $100\text{ ms}/gói$.
  * Giao thức **TCP** có cơ chế bắt tay 3 bước và truyền lại (retransmission) khi rớt gói. Nếu mạng chập chờn, TCP sẽ giữ lại và gửi dồn cục các gói cũ, gây trễ dữ liệu dạng sóng và tràn bộ nhớ RAM của vi điều khiển.
  * Giao thức **UDP** gửi thẳng không cần bắt tay, độ trễ cực thấp dưới $2\text{ ms}$, nếu mất 1 gói thì gói sau bù ngay lập tức, giải phóng RAM và tiết kiệm pin tối đa cho Node C3.

### Câu 42: Gateway nhận và bóc tách gói UDP như thế nào?
* **Bản chất câu hỏi:** Lập trình mạng nhúng trên ESP32.
* **Trả lời:** Trong `loop()` của `ESP32.ino` (dòng 1279):
  Gateway gọi `udp.parsePacket()`. Nếu kích thước đúng bằng 62 bytes, đọc toàn bộ vào cấu trúc `incomingData`. Sau đó kiểm tra lại hàm `calculateCRC16()`; nếu khớp mã CRC thì mới lấy 25 mẫu ECG đưa vào bộ lọc số và lấy các trường `bodyTemp`, `smv`, `fallDetected`.

### Câu 43: Cơ chế Multi-WiFi Roaming tự động trên Node C3 hoạt động thế nào?
* **Bản chất câu hỏi:** Khả năng dự phòng mạng vô tuyến (Network Redundancy).
* **Trả lời:** Tại dòng 436-471 của `ESP32C3.ino`:
  * Mặc định Node C3 kết nối trực tiếp vào trạm SoftAP của Gateway (`BIOMED_GW`, IP tĩnh `192.168.4.2`).
  * Nếu người bệnh đi xa khỏi phòng khiến mất kết nối SoftAP quá 3 lần thử (9 giây), C3 tự động chuyển cấu hình sang kết nối vào **Wi-Fi Router gia đình** (`HOME_SSID`) và gửi dữ liệu qua địa chỉ Broadcast.
  * Nếu Wi-Fi nhà cũng mất sóng sau 3 lần thử, C3 lại tự động quay về tìm SoftAP Gateway.

### Câu 44: Từ Gateway đẩy dữ liệu lên máy chủ Web bằng giao thức gì? Cổng bao nhiêu?
* **Bản chất câu hỏi:** Giao tiếp tầng ứng dụng và bảo mật IoT.
* **Trả lời:** Sử dụng giao thức **WebSocket Secure (WSS)** mã hóa **TLS 1.3** qua cổng tiêu chuẩn **443** (kết nối trực tiếp tới endpoint `wss://mqtt.tsbyin.dev/mqtt`).

### Câu 45: Hệ thống của em có cần mở cổng Port Forwarding trên modem nhà mạng không? Tại sao?
* **Bản chất câu hỏi:** Hiểu biết về tường lửa NAT gia đình và Cloudflare Zero Trust.
* **Trả lời:** **Hoàn toàn không cần mở port!** Hệ thống sử dụng công nghệ **Cloudflare Zero Trust Tunnel**. Trạm Gateway thiết lập một kết nối đi ra (Outbound Tunnel) từ trong nhà lên Cloudflare Edge Server qua cổng 443. Web Dashboard từ xa truy cập vào cũng qua Cloudflare, do đó vượt qua mọi lớp tường lửa NAT mà không tạo lỗ hổng bảo mật cho mạng gia đình.

### Câu 46: Khi có sự cố té ngã, còi Buzzer tại trạm Gateway hoạt động theo logic nào?
* **Bản chất câu hỏi:** Logic máy trạng thái điều khiển còi.
* **Trả lời:** Trong hàm `handleBuzzer()` (dòng 632 của `ESP32.ino`):
  Cảnh báo ngã có **mức ưu tiên cao nhất**. Còi sẽ được kích hoạt kêu liên tục không ngừng (`isFallAlarmLatched = true`) để đánh thức người nhà trong phòng ngủ. Còi chỉ tắt khi có người bấm nút vật lý `BOOT` trên Gateway hoặc bấm nút tắt còi từ xa trên Web Dashboard.

### Câu 47: Màn hình OLED hiển thị những trang nào? Chuyển trang thế nào?
* **Bản chất câu hỏi:** Thiết kế giao diện người dùng nhúng (Embedded GUI).
* **Trả lời:** OLED hiển thị 2 trang giao diện:
  * *Trang 0 (Đồ thị ECG chuyên sâu):* Vẽ dạng sóng điện tim theo thời gian thực kiểu tia quét, đi kèm chỉ số BPM, nhiệt độ và dòng trạng thái đáy.
  * *Trang 1 (Dashboard tổng quan):* Hiển thị bảng số liệu lớn gồm BPM, Thân nhiệt, Gia tốc va đập SMV và trạng thái kết nối Wi-Fi/Cloud.
  Chuyển trang tự động theo chu kỳ hoặc người dùng bấm nút vật lý trên mạch.

### Câu 48: Tin nhắn Telegram cảnh báo khẩn cấp được gửi từ đâu? Bằng hàm nào?
* **Bản chất câu hỏi:** Gọi API đám mây từ vi điều khiển.
* **Trả lời:** Được gửi trực tiếp từ Gateway qua hàm `sendTelegramFallAlert()` (dòng 863 của `ESP32.ino`) bằng giao thức HTTPS POST tới API `https://api.telegram.org/bot<TOKEN>/sendMessage`. Tin nhắn gửi kèm đầy đủ thông tin: Lực va đập cực đại, nhịp tim hiện tại, thân nhiệt và mốc thời gian xảy ra tai nạn.

### Câu 49: Nếu nhịp tim người già đập quá nhanh hoặc quá chậm thì Gateway có cảnh báo không?
* **Bản chất câu hỏi:** Tính năng cảnh báo loạn nhịp tim.
* **Trả lời:** Có! Tại dòng 1332 của `ESP32.ino`, hệ thống giám sát liên tục:
  * Nếu $BPM > 105$ (Nhịp tim nhanh - Tachycardia).
  * Nếu $BPM < 50$ (Nhịp tim chậm nguy hiểm - Bradycardia).
  Gateway phát còi bíp cảnh báo ngắt quãng và gửi tin nhắn cảnh báo loạn nhịp riêng về Telegram/Zalo người nhà.

### Câu 50: Làm sao để tin nhắn Telegram đổ chuông to trên điện thoại người nhà dù đang để chế độ im lặng?
* **Bản chất câu hỏi:** Tính năng khẩn cấp của Telegram Bot API.
* **Trả lời:** Trong payload JSON gửi lên Telegram API, cấu hình trường `"disable_notification": false`. Khi đó ứng dụng Telegram trên điện thoại người thân sẽ kích hoạt âm thanh thông báo khẩn cấp với mức ưu tiên cao nhất.

---

## PHẦN 6: WEB DASHBOARD, CLOUD & CÂU HỎI "BẪY" CỦA HỘI ĐỒNG (CÂU 51 – 60)

### Câu 51: Giao diện Web Dashboard hiển thị sóng điện tim bằng công nghệ gì? Có dùng thư viện Chart.js không?
* **Bản chất câu hỏi:** Hiệu năng đồ họa Web thời gian thực.
* **Trả lời:** **Không dùng Chart.js!** Chart.js vẽ lại toàn bộ DOM rất nặng, không thể tải nổi $250\text{ mẫu/giây}$. Tự xây dựng bộ vẽ trực tiếp trên **HTML5 Canvas 2D tia quét Phosphor (Phosphor Beam Engine)** với tần số quét **60 FPS** dùng hàm `requestAnimationFrame()`. Tia quét chạy mượt mà giống hệt màn hình máy theo dõi bệnh nhân trong phòng hồi sức ICU.

### Câu 52: Thuật toán Soft-sync trên Web Dashboard giải quyết bài toán lệch pha thế nào?
* **Bản chất câu hỏi:** Đồng bộ hóa luồng dữ liệu mạng và luồng render 60 FPS.
* **Trả lời:** Luồng mạng UDP/WebSocket đổ dữ liệu theo đợt (Burst) trong khi màn hình máy tính vẽ đều đặn ở $60\text{ Hz}$. Thuật toán Soft-sync theo dõi khoảng cách giữa con trỏ nạp dữ liệu (`writeIndex`) và con trỏ tia quét (`sweepIndex`). Nếu tia quét đi quá nhanh sắp đuổi kịp con trỏ ghi, tốc độ quét tự động hãm chậm lại một chút; nếu bị trễ thì tăng tốc nhẹ, đảm bảo sóng tim hiển thị liên tục không bao giờ bị giật khựng hay rách hình.

### Câu 53: Tính năng Kiosk Security Manager trên Web có nhiệm vụ gì?
* **Bản chất câu hỏi:** Tính năng đóng gói phần mềm chuyên dụng y tế.
* **Trả lời:** Biến trình duyệt thành một màn hình Kiosk giám sát y tế chuyên dụng:
  * Vô hiệu hóa phím **F12**, `Ctrl+Shift+I` (chống tò mò mở devtools).
  * Vô hiệu hóa chuột phải (`contextmenu`), phím tắt `Ctrl+U` (xem mã nguồn), `Ctrl+S`, `Ctrl+P`.
  * Có cơ chế ngoại lệ thông minh (Smart Exception): Khi mở khung Settings để nhập token Telegram thì vẫn cho phép `Ctrl+C`, `Ctrl+V` bên trong các ô input.

### Câu 54: Cơ sở dữ liệu InfluxDB lưu trữ dữ liệu theo mô hình nào?
* **Bản chất câu hỏi:** Cơ sở dữ liệu chuỗi thời gian (Time-Series Database).
* **Trả lời:** Lưu trữ trên **InfluxDB v2.7**:
  * *Bucket:* `telemetry_data`
  * *Measurement:* `patient_telemetry`
  * *Tag Set:* `patient_id="P001"`, `device_id="ESP32_GW_01"`
  * *Field Set:* Các giá trị số biến thiên theo thời gian gồm `bpm`, `temperature`, `smv`, `leads_off`, `fall_detected`.

### Câu 55 (BẪY): "Tại sao em lấy mẫu 250Hz dùng `micros()` trong `loop()` mà không dùng Hardware Timer Interrupt?"
* **Bản chất câu hỏi:** Kiểm tra sự hiểu biết sâu sắc về kiến trúc ngắt và xung đột ngoại vi I2C/1-Wire.
* **Trả lời tự tin:**
  * Thưa thầy, dùng ngắt cứng Timer (Hardware Timer ISR) là rất chuẩn về mặt lý thuyết. Tuy nhiên, trong hàm lấy mẫu của em có gọi đọc cảm biến quán tính MPU-6050 qua giao tiếp **I2C (`Wire`)** và đọc **1-Wire**.
  * Các hàm thư viện `Wire` và `OneWire` trên ESP32 sử dụng ngắt mềm nội và có cơ chế khóa (Mutex/Spinlock). Nếu gọi giao tiếp I2C bên trong một trình phục vụ ngắt cứng ISR Timer sẽ gây ra hiện tượng **xung đột ngắt lồng nhau (Nested Interrupt Panic)** hoặc lỗi Crash Core vi điều khiển.
  * Vì vậy, thiết kế hàm `loop()` chạy ở tốc độ cao kết hợp `micros()` kiểm tra vi sai là giải pháp tối ưu và an toàn nhất trên vi điều khiển ESP32, đảm bảo chu kỳ $4.0\text{ ms}$ cực kỳ chính xác mà không bao giờ bị treo chip.

### Câu 56 (BẪY): "Nếu bệnh nhân ngã ở góc khuất mất sóng Wi-Fi thì người nhà có biết không?"
* **Bản chất câu hỏi:** Giới hạn thực tế của đề tài và cách ứng biến kỹ thuật.
* **Trả lời thành thật & thông minh:**
  * Thưa thầy, đây chính là một trong những hạn chế thực tế mà em đã nêu rõ trong Chương 5 của báo cáo. Trong phạm vi nhà ở, bán kính phủ sóng Wi-Fi của Gateway đạt từ $15 - 20\text{ mét}$, đáp ứng tốt khuôn viên căn hộ hoặc nhà phố.
  * Khi mất sóng, Node C3 có cơ chế tự động Roaming sang Wi-Fi gia đình để tìm đường truyền phụ.
  * Trong tương lai, để giải quyết triệt để vấn đề này, em sẽ tích hợp thêm một còi buzzer mini hoặc motor rung cảnh báo ngay trên Node đeo ngực, đồng thời trang bị module Sim 4G NB-IoT để truyền dữ liệu độc lập không phụ thuộc sóng Wi-Fi.

### Câu 57 (BẪY): "Tại sao nhịp tim đo bằng máy đo huyết áp là 75 BPM mà mạch của em hiển thị 76 BPM?"
* **Bản chất câu hỏi:** So sánh nguyên lý đo điện học (ECG) và đo cơ học (Oscillometric).
* **Trả lời chuyên sâu y sinh:**
  * Thưa thầy, chênh lệch $1 - 2\text{ BPM}$ là hoàn toàn bình thường về mặt sinh lý học y tế:
  * Máy đo huyết áp bắp tay đo theo **nguyên lý dao động vi áp (Oscillometric)** dựa trên sự phập phồng cơ học của dòng máu dội vào bao túi khí ở động mạch cánh tay.
  * Mạch của đề tài đo theo **nguyên lý điện thế khử cực của nút xoang tim (ECG)** bằng cảm biến y sinh AD8232.
  * Sóng điện tim luôn xuất hiện trước co bóp cơ học của mạch máu một khoảng thời gian trễ sinh lý ($PEP - Pre-ejection Period \approx 30 - 50\text{ ms}$). Sai số $1\text{ BPM}$ nằm sâu trong ngưỡng cho phép ($\pm 5\%$) của tiêu chuẩn y tế quốc tế ANSI/AAMI EC13.

### Câu 58 (BẪY): "Người già bị ngã rồi lồm cồm bò dậy thì hệ thống có báo ngã không?"
* **Bản chất câu hỏi:** Tính hợp lý của việc đánh giá bất động sau ngã.
* **Trả lời:**
  * Nếu sau cú ngã, người già ngay lập tức lồm cồm bò dậy hoặc đứng lên trong vòng $2.5\text{ giây}$ đầu tiên: Khi đó cảm biến con quay hồi chuyển sẽ ghi nhận vận tốc góc xoay $Gyro > 45^\circ/\text{s}$ và gia tốc rung lắc mạnh $\rightarrow$ hệ thống nhận định người bệnh vẫn tỉnh táo và tự đứng dậy được, nên **hủy còi báo động để tránh làm phiền**.
  * Ngược lại, nếu người già nằm bất động trên sàn quá $2.5\text{ giây}$ (dấu hiệu của chấn thương gãy xương hoặc bất tỉnh), hệ thống mới chính thức chốt giữ còi báo động khẩn cấp.

### Câu 59: Tổng chi phí sản xuất phần cứng của đề tài là bao nhiêu?
* **Bản chất câu hỏi:** Tính khả thi thương mại và tính ứng dụng thực tế.
* **Trả lời:** Toàn bộ linh kiện phần cứng bao gồm: ESP32-C3 SuperMini (~55k), ESP32 WROOM (~75k), module AD8232 kèm dây điện cực (~110k), MPU-6050 (~35k), DS18B20 bọc inox (~30k), OLED SH1106 (~65k), Pin Li-Po 1000mAh và module sạc TP4056 (~70k), còi buzzer và linh kiện phụ. Tổng chi phí **chưa đến 600.000 VNĐ**, hoàn toàn rẻ hơn hàng chục lần so với các thiết bị ngoại nhập đắt tiền.

### Câu 60: Nếu có thêm thời gian 3 tháng nữa để phát triển đề tài, em sẽ nâng cấp những gì?
* **Bản chất câu hỏi:** Tầm nhìn kỹ thuật và tư duy nghiên cứu mở rộng.
* **Trả lời:** Em sẽ tập trung 3 hướng nâng cấp đột phá:
  1. *Cảm biến độ cao BMP280:* Bắt trọn các cú ngã trượt từ từ (Sliding Fall) nhờ việc phát hiện cao độ cơ thể tụt đột ngột $1.2\text{ m}$ xuống mặt sàn.
  2. *Điện cực khô bằng vải dệt dẫn điện (Textile Dry Electrodes):* May điện cực trực tiếp vào đai vải, người già mặc áo vào là đo được liên tục mà không cần dán gel dính ngứa da.
  3. *Trí tuệ nhân tạo biên TinyML (1D-CNN):* Nén mô hình học sâu chạy trực tiếp trên vi điều khiển ESP32 để phân loại tự động các dạng loạn nhịp tim nguy hiểm như Rung nhĩ (AFib) và Ngoại tâm thu thất (PVC).

---
*Chúc bạn bình tĩnh, tự tin và đạt điểm A+ tuyệt đối trong buổi vấn đáp bảo vệ đồ án!*

