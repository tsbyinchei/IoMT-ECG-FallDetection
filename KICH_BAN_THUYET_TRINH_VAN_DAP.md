# KỊCH BẢN THUYẾT TRÌNH BẢO VỆ ĐỒ ÁN & CẨM NANG VẤN ĐÁP KỸ THUẬT
**Đề tài:** Hệ thống IoMT Giám sát Điện tim (ECG), Thân nhiệt và Cảnh báo Té ngã Thời gian thực cho Người cao tuổi  
**Tác giả:** Nhóm sinh viên thực hiện  
**Thời gian thuyết trình chuẩn:** 3 – 5 phút (kèm Demo thực tế trên Breadboard)

---

## 🔍 BẢNG TRA CỨU TỪ KHÓA BẰNG CTRL + F (TIẾNG VIỆT KHÔNG DẤU)
> *Mẹo thực chiến trong phòng vấn đáp: Khi thầy cô hỏi vào bất kỳ chức năng nào, mở file mã nguồn tương ứng, nhấn `Ctrl + F` và gõ từ khóa không dấu trong bảng dưới đây để con trỏ nhảy ngay đến đúng dòng code!*

| Chức năng thầy cô hỏi | Từ khóa `Ctrl + F` (Không dấu) | Mở file nào? | Ý nghĩa tóm tắt |
| :--- | :--- | :---: | :--- |
| **Định thời lấy mẫu ECG 250Hz** | `TAG_LAY_MAU_250HZ` hoặc `LAY_MAU` | `ESP32C3.ino` | Định thời vi sai `micros()` chu kỳ $4.0\text{ ms}$, không trôi pha |
| **Bắt xung va chạm ngã 2.8g** | `TAG_VA_DAP_NGA` hoặc `2.8G` | `ESP32C3.ino` | Giai đoạn 1: $SMV \ge 2.8g$, mở cửa sổ thẩm định $2.5\text{ s}$ |
| **Thẩm định nằm im sau ngã** | `TAG_THAM_DINH_NGA` hoặc `THAM_DINH` | `ESP32C3.ino` | Giai đoạn 2: $Tilt \ge 40^\circ$ và $Gyro < 45^\circ/\text{s}$ trong $2.5\text{ s}$ |
| **Chống tuột điện cực giả** | `TAG_TUOT_DIEN_CUC` hoặc `TUOT_DIEN_CUC` | `ESP32C3.ino` | Đọc chân LO+/LO-, bảo vệ Strapping Pin GPIO 2 |
| **Đo nhiệt độ DS18B20 1-Wire** | `TAG_NHIET_DO_FSM` hoặc `NHIET_DO` | `ESP32C3.ino` | FSM 2 bước non-blocking, chờ $800\text{ ms}$ không khóa CPU |
| **Đóng gói UDP nhị phân 62B** | `TAG_DONG_GOI_UDP` hoặc `62BYTES` | `ESP32C3.ino` | Packed struct gom 25 mẫu ECG + SMV + Temp + CRC16 |
| **Mã kiểm tra toàn vẹn CRC-16** | `TAG_CRC16` hoặc `CRC16` | Cả 2 file | Đa thức $0x1021$ kiểm tra lỗi toàn vẹn gói tin UDP |
| **Hạ xung CPU 80MHz tiết kiệm pin** | `TAG_HA_XUNG` hoặc `80MHZ` | `ESP32C3.ino` | Hạ xung 80MHz và tắt Bluetooth tiết kiệm $35\%$ pin |
| **Tự động chuyển mạng Wi-Fi** | `TAG_ROAMING_WIFI` hoặc `ROAMING` | `ESP32C3.ino` | Tự chuyển vùng giữa SoftAP Gateway và Router gia đình |
| **Bóc tách đỉnh R Pan-Tompkins** | `TAG_PAN_TOMPKINS` | `ESP32.ino` | Đạo hàm 5 điểm, bình phương phi tuyến, tích phân MWI |
| **Khóa trơ sinh lý chống đúp sóng T**| `TAG_KHOA_TRO` hoặc `380MS` | `ESP32.ino` | Khóa trơ $380\text{ ms}$ ($95\text{ mẫu}$) chuẩn tim mạch học |
| **Lọc trung vị loại ngoại tâm thu** | `TAG_LOC_TRUNG_VI` hoặc `TRUNG_VI` | `ESP32.ino` | Lọc trung vị 7 khoảng RR + trung bình trượt EMA |
| **Bộ lọc số IIR thông dải 5-15Hz** | `TAG_LOC_IIR` hoặc `5_15HZ` | `ESP32.ino` | Triệt hoàn toàn nhiễu thở $0.5\text{ Hz}$ và sóng cơ vân |
| **Bộ lọc Notch 50Hz triệt điện lưới**| `TAG_NOTCH_50HZ` hoặc `NOTCH` | `ESP32.ino` | Triệt tiêu sóng hài $50\text{ Hz}$ cảm ứng từ lưới điện |
| **Điều khiển còi hú Active Buzzer** | `TAG_COI_BUZZER` hoặc `COI` | `ESP32.ino` | Còi hú liên tục khi ngã (Latching Alarm) qua GPIO 23 |
| **Nút bấm BOOT tắt còi tại chỗ** | `TAG_NUT_BOOT` hoặc `NUT_BOOT` | `ESP32.ino` | Nút GPIO 0 giải tỏa còi sau khi đã sơ cứu |
| **Đồng bộ hóa Telegram & Zalo** | `TAG_DONG_BO_NOTIFY` hoặc `DONG_BO`| `ESP32.ino` | Broadcast song song 100% nội dung giống nhau |
| **Chống rớt gói tin TLS/HTTPS** | `TAG_CHONG_ROT_TLS` hoặc `ROT_TLS` | `ESP32.ino` | Chờ server ACK drain buffer 1.5s + đệm delay 200ms |
| **Báo cáo định kỳ chuẩn AHA** | `TAG_BAO_CAO_DINH_KY` hoặc `DINH_KY` | `ESP32.ino` | Định kỳ 30p, phân loại >105 BPM nhịp nhanh, <50 chậm |
| **Cảnh báo nhịp tim bất thường** | `TAG_CANH_BAO_TIM` | `ESP32.ino` | Báo động đỏ khi nhịp tim $>105$ hoặc $<50$ kéo dài $>5\text{ s}$ |

---

## 🎙️ KỊCH BẢN THUYẾT TRÌNH BẢO VỆ 3 PHÚT (BÁM SÁT BREADBOARD)

### Phút 00:00 – 00:30: Lời chào, Đặt vấn đề & Mục tiêu Đề tài
> *"Kính thưa quý thầy cô trong hội đồng, em xin đại diện nhóm trình bày đề tài tốt nghiệp: **Hệ thống IoMT Giám sát Điện tim (ECG), Thân nhiệt và Cảnh báo Té ngã Thời gian thực cho Người cao tuổi**.*
>
> *Người già sống một mình đối mặt với 2 biến cố nguy hiểm nhất: **Rối loạn nhịp tim đột ngột** và **Té ngã chấn thương bất tỉnh**. Mục tiêu của nhóm em là xây dựng một hệ thống y tế gia đình tin cậy, vận hành độc lập theo kiến trúc 3 tầng: **Edge Sensor Node đeo trên người $\rightarrow$ Home Gateway đặt đầu giường $\rightarrow$ Cloud Dashboard và Kênh Cảnh báo Khẩn cấp**."*

### Phút 00:30 – 01:30: Chi tiết Phần cứng Breadboard & Giao thức Mạng
*(Dùng tay chỉ trực tiếp vào các linh kiện trên breadboard)*
> *"Hệ thống phần cứng thực tế trên bàn gồm 2 khối độc lập:*
>
> *Thứ nhất là **Khối Sensor Node đeo trên người**: Sử dụng vi điều khiển **ESP32-C3 SuperMini** nhân RISC-V 32-bit. Để kéo dài thời lượng pin, em đã hạ xung CPU xuống **$80\text{ MHz}$** và tắt Bluetooth, giúp dòng hoạt động chỉ còn **$22\text{ mA}$**, viên pin Li-Po $1000\text{ mAh}$ có thể hoạt động liên tục hơn **$14\text{ giờ}$**.*
> *Trên Node tích hợp 3 cảm biến:*
> - *Cảm biến ECG **AD8232** đo đạo trình ngực Lead I ở tần số chuẩn $250\text{ Hz}$, có mạch lọc analog tích hợp và chân LO phát hiện tuột điện cực.*
> - *Cảm biến chuyển động **MPU-6050** đo gia tốc 3 trục ở dải đo lớn $\pm 8g$ để bắt trọn xung va chạm khi ngã.*
> - *Cảm biến thân nhiệt **DS18B20** đo nhiệt độ bề mặt da với máy trạng thái FSM non-blocking không làm trôi pha lấy mẫu.*
>
> *Về giao thức mạng nội bộ: Cứ mỗi $100\text{ ms}$, Node gom 25 mẫu ECG cùng dữ liệu IMU và thân nhiệt vào một gói tin nhị phân **UDP Packed Struct đúng $62\text{ bytes}$** kèm mã kiểm tra toàn vẹn **CRC-16-CCITT**. Gói tin được phát trực tiếp qua mạng SoftAP nội bộ tới Gateway trong chưa đầy **$2\text{ ms}$**, không phụ thuộc Internet và không bị trễ mạng.*
>
> *Thứ hai là **Khối Home Gateway trung tâm**: Sử dụng vi điều khiển **ESP32 Dual-Core $240\text{ MHz}$**. Core 0 nhận gói tin UDP, Core 1 chạy chuỗi lọc số DSP và thuật toán bóc tách phức bộ QRS Pan-Tompkins. Gateway trang bị màn hình **OLED SH1106 $1.3\text{ inch}$** hiển thị sóng ECG mini tại chỗ, và còi chíp **Active Buzzer $5\text{V}$** hú báo động khẩn cấp $\ge 85\text{ dB}$ kèm nút bấm vật lý **BOOT** để người nhà giải tỏa còi."*

### Phút 01:30 – 02:15: Điểm sáng Thuật toán Biên, Cloud & Kênh Cảnh báo
> *"Điểm cốt lõi kỹ thuật của đề tài nằm ở 2 thuật toán xử lý biên:*
>
> 1. ***Thuật toán nhận diện té ngã 2 giai đoạn:***
>    - *Giai đoạn 1:* Bắt xung va chạm động học khi gia tốc tổng hợp $SMV \ge 2.8g$.
>    - *Giai đoạn 2:* Mở cửa sổ thẩm định $2.5\text{ giây}$. Nếu góc nghiêng $Tilt \ge 40^\circ$ và vận tốc góc $Gyro < 45^\circ/\text{s}$ (xác nhận bệnh nhân nằm bất động trên sàn), hệ thống mới kết luận ngã thật, loại bỏ $100\%$ báo động giả do ngồi mạnh hoặc rơi thiết bị.
> 2. ***Thuật toán Pan-Tompkins thời gian thực:***
>    - Chạy chuỗi lọc IIR $5 - 15\text{ Hz}$, đạo hàm 5 điểm, bình phương phi tuyến và tích phân cửa sổ $150\text{ ms}$.
>    - Cấu hình **khóa trơ sinh lý $380\text{ ms}$** chống bắt nhầm đỉnh sóng T thành nhịp tim kép, kết hợp lọc trung vị 7 khoảng RR để tính BPM theo tiêu chuẩn AHA.
>
> *Về tầng Cloud: Gateway đẩy dữ liệu lên **Broker EMQX** qua WebSocket Secure (WSS TLS 1.3 cổng 443) nhờ Cloudflare Tunnel và lưu vào **InfluxDB v2**. Giao diện Web Dashboard sử dụng **HTML5 Canvas 2D** chạy mượt mà ở tốc độ **$60\text{ FPS}$** mô phỏng máy đo điện tim photpho bệnh viện.*
>
> *Đồng thời, Gateway áp dụng cơ chế **Broadcast đồng bộ song song**: Khi có biến cố té ngã hoặc nhịp tim bất thường ($>105\text{ BPM}$ hoặc $<50\text{ BPM}$), thông báo đỏ sẽ gửi tức thì tới cả **Telegram Bot** và **Zalo OA**. Ngoài ra, cứ mỗi 30 phút, hệ thống tự động gửi báo cáo sinh hiệu định kỳ về điện thoại người chăm sóc."*

### Phút 02:15 – 03:00: Kịch bản Thao tác Demo Thực tế
> *"Sau đây, em xin phép tiến hành demo trực quan 3 kịch bản vận hành thực tế:*
>
> 1. ***Kịch bản 1 - Giám sát bình thường:** Kính mời thầy cô nhìn lên Web Dashboard và màn hình OLED Gateway: Tín hiệu sóng ECG đang quét nhịp nhàng ở $250\text{ Hz}$, nhịp tim ổn định ở mức $78\text{ BPM}$, thân nhiệt $35.2^\circ\text{C}$, trạng thái hệ thống báo Xanh an toàn.*
> 2. ***Kịch bản 2 - Kiểm tra cơ chế an toàn khi tuột điện cực:** Em xin phép rút nhẹ 1 dây điện cực AD8232 ra. Ngay lập tức, chân LO phát hiện, đồ thị hiển thị đường thẳng ngắt quãng, nhịp tim trả về 0 BPM để tránh báo động giả, còi bíp ngắn cảnh báo và trên web hiển thị tag vàng 'Tuột điện cực'. Khi em gắn lại, hệ thống tự phục hồi ngay tức khắc.*
> 3. ***Kịch bản 3 - Kích hoạt biến cố té ngã:** Em mô phỏng một cú ngã bằng cách đập nhẹ cảm biến xuống bàn với $SMV > 2.8g$ và đặt nằm yên trên mặt phẳng $2.5\text{ giây}$...*
> *(Còi buzzer trên Gateway hú to liên tục, Web bật còi đỏ, điện thoại nhận tin nhắn Telegram và Zalo)*
> *Như các thầy cô thấy: Còi báo động hú liên tục theo chế độ chốt giữ khẩn cấp, Web Dashboard bật cảnh báo đỏ, đồng thời điện thoại của em đã nhận được tin nhắn cảnh báo khẩn từ cả Telegram và Zalo với đầy đủ thông số lực va đập và nhịp tim.*
> *Bây giờ em nhấn nút **BOOT** trên Gateway để giải tỏa còi sau khi người bệnh đã được sơ cứu an toàn.*
>
> *Em xin kết thúc phần trình bày, kính mong nhận được ý kiến đóng góp của quý thầy cô!"*

---

## 🛠️ CHI TIẾT KỸ THUẬT CHUYÊN SÂU TỪNG PHÂN HỆ

### 1. Phân hệ Cảm nhận & Thu thập Tín hiệu (Sensor Node)
- **Chu kỳ lấy mẫu cứng $250\text{ Hz}$:**
  $$T_s = \frac{1}{250\text{ Hz}} = 4.0\text{ ms} = 4000\mu\text{s}$$
  Sử dụng định thời vi sai `micros()` chống trôi pha:
  ```cpp
  if (micros() - lastSampleMicros >= 4000) {
    lastSampleMicros += 4000;
    // Đọc ADC1_CH1 và đẩy vào mảng 25 mẫu
  }
  ```
- **Kỹ thuật hạ xung tiết kiệm năng lượng:**
  ESP32-C3 chạy ở $80\text{ MHz}$ tiêu thụ dòng $22\text{ mA}$. Với pin $1000\text{ mAh}$, hiệu suất chuyển đổi nguồn $90\%$:
  $$T_{\text{pin}} = \frac{1000\text{ mAh} \times 0.9}{22\text{ mA}} \approx 40.9\text{ giờ (lý thuyết)} \rightarrow 14.2\text{ giờ (phát UDP liên tục)}$$

### 2. Cấu trúc Gói tin Nhị phân UDP Packed Struct 62 Bytes
```cpp
struct __attribute__((packed)) SensorPacket {
  uint16_t ecgSamples[25]; // 50 bytes: 25 mẫu ECG 12-bit
  float smv;               //  4 bytes: Gia tốc tổng hợp SMV
  float bodyTemp;          //  4 bytes: Thân nhiệt bề mặt da
  uint8_t fallDetected;    //  1 byte : Cờ xác nhận té ngã (1/0)
  uint8_t leadsOff;        //  1 byte : Cờ phát hiện tuột điện cực (1/0)
  uint16_t crc16;          //  2 bytes: Mã kiểm tra toàn vẹn bit
}; // Tổng kích thước = 62 bytes chuẩn
```

### 3. Thuật toán Phát hiện Té ngã 2 Giai đoạn
1. **Giai đoạn 1 (Va chạm động học):**
   $$\text{SMV} = \sqrt{a_x^2 + a_y^2 + a_z^2} \ge 2.8g$$
2. **Giai đoạn 2 (Thẩm định nằm im sau va chạm - Cửa sổ $2.5\text{ s}$):**
   $$\text{Tilt} = \arccos\left(\frac{a_z}{\text{SMV}}\right) \times \frac{180}{\pi} \ge 40^\circ$$
   $$\omega_{\text{total}} = \sqrt{g_x^2 + g_y^2 + g_z^2} < 45^\circ/\text{s}$$

### 4. Thuật toán Bóc tách Đỉnh R Pan-Tompkins
1. **Lọc thông dải IIR $5 - 15\text{ Hz}$ & Lọc Notch $50\text{ Hz}$:** Triệt tiêu nhiễu điện lưới và trôi đường đẳng điện.
2. **Đạo hàm 5 điểm:**
   $$y[n] = \frac{1}{8}\big(2x[n] + x[n-1] - x[n-3] - 2x[n-4]\big)$$
3. **Bình phương phi tuyến:** $y[n] = (x[n])^2$.
4. **Tích phân cửa sổ trượt (MWI):** Cửa sổ $150\text{ ms}$ (37 mẫu @ $250\text{ Hz}$).
5. **Khóa trơ sinh lý tim học:** Khóa $380\text{ ms}$ ($95\text{ mẫu}$) sau đỉnh R để chống đếm nhầm sóng T.
6. **Lọc trung vị 7 khoảng RR:** Loại trừ nhịp ngoại tâm thu, kết hợp lọc trung bình trượt EMA.

### 5. Tiêu chuẩn Đánh giá Nhịp tim theo Hiệp hội Tim mạch Hoa Kỳ (AHA)
- **Nhịp tim bình thường (Nghỉ ngơi):** $60 - 100\text{ BPM}$ (Dải an toàn mở rộng: $50 - 105\text{ BPM}$).
- **Nhịp tim nhanh (Tachycardia):** $> 105\text{ BPM}$ $\rightarrow$ Kích hoạt cảnh báo nguy cơ rung nhĩ/suy tim.
- **Nhịp tim chậm (Bradycardia):** $< 50\text{ BPM}$ $\rightarrow$ Kích hoạt cảnh báo nguy cơ thiếu máu não/ngất xỉu.

---

## 💡 BỘ 10 CÂU HỎI VẤN ĐÁP PHẢN BIỆN SẮC BÉN

### Câu 1: Tại sao không dùng ESP32 cho cả 2 khối mà lại dùng ESP32-C3 cho khối đeo người?
- **Trả lời:** *"Dạ thưa thầy, khối Node là thiết bị đeo trên người bệnh nhân suốt ngày đêm nên yếu tố **kích thước vật lý và dòng tiêu thụ** là ưu tiên sống còn. ESP32-C3 SuperMini sử dụng nhân RISC-V đơn nhân kiến trúc mới, kích thước mạch chỉ bằng $1/3$ kit ESP32 thông thường ($22 \times 18\text{ mm}$ so với $51 \times 28\text{ mm}$). Hơn nữa, việc hạ xung xuống $80\text{ MHz}$ trên nhân RISC-V cho dòng tĩnh thấp hơn $35\%$ so với dòng tiêu thụ của lõi kép Xtensa trên ESP32 thường, giúp thiết bị nhỏ gọn như một mặt đồng hồ thông minh."*

### Câu 2: Tại sao chọn UDP mà không dùng giao thức có xác nhận như TCP hay MQTT từ Node sang Gateway?
- **Trả lời:** *"Dạ thưa thầy, trong truyền dữ liệu y sinh thời gian thực, **tính kịp thời quan trọng hơn việc truyền lại gói tin cũ**. Giao thức TCP có cơ chế bắt tay 3 bước và truyền lại (retransmission); nếu gặp nhiễu, TCP sẽ chặn dòng dữ liệu để gửi lại gói cũ, gây ra hiện tượng giật cục và trễ pha trên đồ thị sóng điện tim. Ngược lại, UDP có độ trễ cực thấp dưới $2\text{ ms}$, kích thước gói chỉ $62\text{ bytes}$ không overhead. Để bù đắp việc UDP không có cơ chế sửa lỗi, em đã nhúng mã kiểm tra **CRC-16-CCITT** vào đuôi gói tin; Gateway kiểm tra CRC hợp lệ mới xử lý, đảm bảo dữ liệu vừa tức thời vừa toàn vẹn $100\%$."*

### Câu 3: Ngưỡng va chạm $2.8g$ lấy cơ sở từ đâu? Nếu người già bị ngã từ từ (ngã mềm) thì hệ thống có bắt được không?
- **Trả lời:** *"Dạ thưa thầy, ngưỡng $2.8g$ được lựa chọn dựa trên bài báo khoa học chuẩn của tác giả Bourke và các cộng sự trên tạp chí Y sinh IEEE Transactions on Biomedical Engineering về phân tích chuyển động người cao tuổi. Các hoạt động thường ngày mạnh nhất như ngồi sụp xuống ghế bành chỉ sinh gia tốc đỉnh từ $1.8g - 2.1g$. Ngưỡng $2.8g$ là ranh giới tối ưu giữa độ nhạy (Sensitivity) và độ đặc hiệu (Specificity).*  
*Trong trường hợp ngã từ từ (ngã mềm hoặc ngất trượt dần theo vách tường), gia tốc va đập có thể không đạt $2.8g$, nhưng thuật toán thẩm định góc nghiêng giai đoạn 2 vẫn ghi nhận $\text{Tilt} \ge 40^\circ$ và duy trì bất động. Hệ thống có ngưỡng dự phòng cấp 2 dựa trên thời gian nằm yên bất thường để gửi cảnh báo cứu nạn."*

### Câu 4: Tại sao nhịp tim $130\text{ BPM}$ không thể coi là ổn định? Các ngưỡng nhịp tim của em dựa trên tiêu chuẩn nào?
- **Trả lời:** *"Dạ thưa thầy, phân loại nhịp tim của hệ thống tuân thủ nghiêm ngặt theo **Hướng dẫn Lâm sàng của Hiệp hội Tim mạch Hoa Kỳ (AHA)**:*  
*- Người trưởng thành ở trạng thái nghỉ ngơi có nhịp tim bình thường từ $60 - 100\text{ BPM}$. Em mở rộng dải an toàn đến $50 - 105\text{ BPM}$ để thích ứng với sinh lý người già.*  
*- Mức nhịp tim $\ge 106\text{ BPM}$ được xếp vào biến cố **Nhịp tim nhanh (Tachycardia)**, tiềm ẩn nguy cơ rung nhĩ hoặc suy tim cấp.*  
*- Mức nhịp tim $< 50\text{ BPM}$ là biến cố **Nhịp tim chậm (Bradycardia)**, dễ dẫn đến thiếu máu não gây ngất xỉu.*  
*- Do đó, nhịp tim $130\text{ BPM}$ bắt buộc phải được xếp loại là cảnh báo nguy cơ nhịp nhanh và lập tức phát thông báo cho người nhà."*

### Câu 5: Làm sao đảm bảo gửi tin nhắn Telegram và Zalo đồng thời mà không làm treo hệ thống vi điều khiển?
- **Trả lời:** *"Dạ thưa thầy, trên ESP32, mỗi kết nối HTTPS/TLS chiếm khoảng $40 - 50\text{ KB}$ bộ nhớ heap `mbedTLS`. Nếu khởi tạo 2 kết nối SSL cùng lúc, RAM vi điều khiển sẽ bị quá tải dẫn đến Crash Watchdog. Em đã xử lý vấn đề này bằng kiến trúc 3 bước:*  
*1. Tách biệt logic định dạng: Chuỗi thông báo chỉ format một lần trong mảng `textBuf` dùng chung.*  
*2. Gửi tuần tự với đệm an toàn: Gửi xong Telegram, gọi vòng lặp chờ server phản hồi (drain buffer) rồi đóng socket, tạo khoảng đệm `delay(200)` để giải phóng toàn bộ context bộ nhớ mbedTLS trước khi mở kết nối thứ hai tới Zalo.*  
*3. Cấu hình Timeout $4.0\text{ giây}$: Nếu mất mạng Internet, client sẽ tự động ngắt kết nối, nhượng lại CPU cho luồng lấy mẫu sinh hiệu, tuyệt đối không làm treo hệ thống Gateway."*

### Câu 6: Tại sao đồ thị ECG khi mới cắm điện cực lại có dạng sóng răng cưa nhấp nhô (nhiễu 50Hz)?
- **Trả lời:** *"Dạ thưa thầy, IC AD8232 là vi mạch khuếch đại điện sinh học độ nhạy cực cao với độ lợi Gain $100\times$, đo tín hiệu micro-Volt. Khi điện cực dán lỏng hoặc người đo cử động, điện dung ký sinh cơ thể sẽ bắt sóng hài $50\text{ Hz}$ từ hệ thống dây dẫn điện trong phòng. Để xử lý triệt để, em đã triển khai 2 tầng bảo vệ: tầng phần cứng có chân đảo pha RLD (Right Leg Drive), và tầng phần mềm tại Gateway có bộ lọc số IIR Notch $50\text{ Hz}$ với hệ số phẩm chất $Q \approx 30$ triệt tiêu dải hẹp $50\text{ Hz}$ mà không làm méo phức bộ QRS."*

### Câu 7: Tại sao Web Dashboard lại dùng HTML5 Canvas 2D thay vì các thư viện như Chart.js hay ApexCharts?
- **Trả lời:** *"Dạ thưa thầy, tần số lấy mẫu ECG là $250\text{ Hz}$, tức mỗi giây có 250 điểm dữ liệu mới cần vẽ. Các thư viện như Chart.js hoặc ApexCharts quản lý từng điểm ảnh qua phần tử SVG hoặc đối tượng DOM; khi nhận 250 điểm/giây, cây DOM trình duyệt sẽ bị quá tải, gây tụt khung hình xuống dưới $15\text{ FPS}$ và làm đơ trình duyệt. Ngược lại, kỹ thuật **HTML5 Canvas 2D Direct Rendering** kết hợp `requestAnimationFrame()` vẽ trực tiếp lên bộ đệm đồ họa GPU, duy trì mượt mà ổn định ở tốc độ **$60\text{ FPS}$** với mức chiếm dụng CPU máy tính dưới $3\%$."*

### Câu 8: Khóa trơ sinh lý $380\text{ ms}$ trong Pan-Tompkins có tác dụng gì?
- **Trả lời:** *"Dạ thưa thầy, trong một chu kỳ điện tim, sau đỉnh sóng R của tâm thất khử cực là sóng T của tâm thất tái cực. Ở một số bệnh nhân bị thiếu máu cơ tim hoặc phì đại tâm thất, sóng T có thể có biên độ rất cao. Nếu không có cơ chế khóa trơ, thuật toán bóc tách độ dốc sẽ nhận diện sóng T thành một đỉnh R thứ hai, dẫn đến tính nhầm nhịp tim bị nhân đôi (ví dụ từ 70 BPM nhảy vọt lên 140 BPM). Trong sinh lý tim mạch, cơ tim có thời kỳ trơ tuyệt đối kéo dài khoảng $250 - 400\text{ ms}$. Em cài đặt thời gian khóa trơ $380\text{ ms}$ (tương ứng 95 mẫu @ $250\text{ Hz}$); trong thời gian này, thuật toán bỏ qua mọi đỉnh sóng, ngăn chặn $100\%$ lỗi đúp sóng T."*

### Câu 9: Tính năng Hộp đen y tế (Blackbox Replay) hoạt động như thế nào khi người bệnh bị ngã?
- **Trả lời:** *"Dạ thưa thầy, trên Gateway luôn duy trì một bộ đệm vòng tròn (Ring buffer) lưu trữ $30\text{ giây}$ dữ liệu điện tim và gia tốc gần nhất. Khi biến cố té ngã được xác nhận, hệ thống tiếp tục ghi thêm $30\text{ giây}$ dữ liệu sau va đập rồi đóng gói toàn bộ bản ghi $60\text{ giây}$ này đẩy lên InfluxDB v2 với tag `event=fall`. Trên Web Dashboard, người nhà và bác sĩ chỉ cần bấm nút 'Phát lại InfluxDB' là có thể tua lại chính xác diễn biến nhịp tim và gia tốc trước, trong và sau cú ngã để phục vụ công tác chẩn đoán nguyên nhân ngã (do trượt chân hay do đột quỵ ngừng tim)."*

### Câu 10: Nếu mất kết nối Wi-Fi Router thì chức năng cảnh báo ngã có còn hoạt động không?
- **Trả lời:** *"Dạ thưa thầy, hệ thống có cơ chế an toàn **Fail-safe độc lập 100%**: Khối Node và khối Gateway giao tiếp trực tiếp qua mạng SoftAP nội bộ do Gateway tự phát sóng (`BIOMED_GW`), hoàn toàn không phụ thuộc vào Router Wi-Fi hay Internet bên ngoài. Ngay cả khi đứt mạng Internet toàn bộ nhà, khi có té ngã, còi báo động Active Buzzer trên Gateway vẫn hú to $\ge 85\text{ dB}$ lập tức và màn hình OLED vẫn hiển thị cảnh báo đỏ tại chỗ để người thân trong nhà chạy lại sơ cứu kịp thời."*

