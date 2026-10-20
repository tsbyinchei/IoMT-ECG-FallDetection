# 📖 KHUNG SƯỜN BÁO CÁO ĐỀ TÀI (25–30 TRANG)
## ĐỀ TÀI: "THIẾT KẾ HỆ THỐNG IoT THEO DÕI ĐIỆN TIM, THÂN NHIỆT VÀ CẢNH BÁO TÉ NGÃ TỪ XA CHO NGƯỜI CAO TUỔI TẠI NHÀ"
**Tác giả:** TsByinChei ([@tsbyinchei](https://github.com/tsbyinchei)) • [contact@tsbyin.dev](mailto:contact@tsbyin.dev)  

Tài liệu này cung cấp mục lục chi tiết, khung sườn nội dung và các bảng số liệu, biểu đồ thực nghiệm cần đưa vào để hoàn thiện bài báo cáo cho đề tài **"Thiết kế hệ thống IoT theo dõi điện tim, thân nhiệt và cảnh báo té ngã từ xa cho người cao tuổi tại nhà"** trong học phần Thiết kế Hệ thống IoT và Xử lý Tín hiệu Y sinh (BDSP) đạt điểm tối đa (A/A+).

---

## 📑 MỤC LỤC BÁO CÁO CHUẨN

* **LỜI CẢM ƠN & TÓM TẮT ĐỀ TÀI (ABSTRACT)**
* **DANH MỤC TỪ VIẾT TẮT (IoT, IoMT, ECG, QRS, MWI, SMV, HRV, UDP, MQTT, WSS)**
* **DANH MỤC HÌNH VẼ & BẢNG BIỂU**

---

### CHƯƠNG 1: TỔNG QUAN & TÍNH CẤP THIẾT CỦA ĐỀ TÀI (4–5 trang)
1.1. Bối cảnh già hóa dân số và nhu cầu giám sát sức khỏe cho người cao tuổi sinh hoạt tại nhà.  
1.2. Các nguy cơ rủi ro thường trực: Loạn nhịp tim, sốt cao/hạ thân nhiệt và tai nạn té ngã (hội chứng Long Lie).  
1.3. Hạn chế của các phương pháp thăm khám ngắt quãng truyền thống tại cơ sở y tế.  
1.4. Xu hướng ứng dụng Internet of Things (IoT/IoMT), cảm biến đeo người và điện toán biên (Edge Computing).  
1.5. Mục tiêu, đối tượng thụ hưởng và phạm vi thực nghiệm của đề tài.  
1.6. Bố cục bài báo cáo.

---

### CHƯƠNG 2: CƠ SỞ LÝ THUYẾT & TIÊU CHUẨN KỸ THUẬT (6–7 trang)
2.1. Sinh lý học tín hiệu điện tim (Điện tâm đồ ECG):
   - Nguồn gốc phát sinh điện thế hoạt động của tim (Nút xoang SA $\to$ Nút nhĩ thất AV $\to$ Bó His $\to$ Sợi Purkinje).
   - Ý nghĩa các sóng sinh lý: Sóng P (khử cực nhĩ), phức bộ QRS (khử cực thất), sóng T (tái cực thất).
   - Tam giác đạo trình Einthoven và kỹ thuật dán 3 điện cực (RA, LA, RL).
2.2. Động lực học chuyển động và cơ chế té ngã:
   - Gia tốc trọng trường và định luật Newton.
   - Vector độ lớn gia tốc $SMV$ (Signal Magnitude Vector).
   - Mô hình kiểm định 3 pha té ngã (Rơi tự do $\to$ Va đập $\to$ Bất động).
2.3. Kiến trúc hệ thống IoT chuẩn ITU-T Rec. Y.2060:
   - 4 tầng: Thiết bị (Device) $\to$ Mạng (Network) $\to$ Hỗ trợ dịch vụ (Service) $\to$ Ứng dụng (Application).
2.4. Các giao thức truyền thông không dây trong IoMT:
   - So sánh Wi-Fi UDP Socket (độ trễ siêu thấp) và MQTT (tin cậy, publish/subscribe).

---

### CHƯƠNG 3: THIẾT KẾ VÀ CHẾ TẠO HỆ THỐNG PHẦN CỨNG (6–7 trang)
3.1. Sơ đồ khối tổng thể hệ thống (Kiến trúc phân tầng Sensor Node & Gateway).  
3.2. Thiết kế Node cảm biến đeo người (Wearable Node):
   - Vi điều khiển ESP32-C3 SuperMini (kiến trúc RISC-V, tối ưu năng lượng).
   - Mạch khuếch đại điện tim AD8232 (khuếch đại vi sai In-Amp, Right Leg Drive).
   - Cảm biến quán tính MPU-6050 (giao tiếp I2C 100kHz).
   - Cảm biến thân nhiệt DS18B20 (chuẩn 1-Wire).
3.3. Thiết kế Gateway biên (Edge Gateway):
   - ESP32-WROOM-32 (Dual-core 240MHz).
   - Màn hình OLED SH1106 1.3" giao diện 4 trang lâm sàng.
   - Mạch cảnh báo âm thanh Active Buzzer (GPIO 23).
   - Nút bấm BOOT (GPIO 0) chuyển trang trạng thái.
3.4. Sơ đồ mạch nguyên lý (Schematic) và bảng ánh xạ chân đấu nối.  
3.5. Dự toán năng lượng và thiết kế nguồn pin Li-Po 1000mAh.

---

### CHƯƠNG 4: THUẬT TOÁN XỬ LÝ TÍN HIỆU Y SINH TRÊN MATLAB (5–6 trang)
4.1. Lưu đồ thuật toán toàn hệ thống.  
4.2. Thuật toán Pan-Tompkins hoàn chỉnh:
   - Bộ lọc số thông dải IIR Butterworth 5–15Hz kèm bộ nhớ trạng thái `z_bp`.
   - Đạo hàm bậc một 5-điểm làm nổi bật sườn dốc QRS.
   - Bình phương phi tuyến tính năng lượng.
   - Tích phân cửa sổ trượt (MWI) 38 mẫu ($150\text{ms}$).
   - Ngưỡng thích ứng kép ($SPKI, NPKI$) và thời gian trơ sinh lý $380\text{ms}$.
   - Bộ lọc trung vị (Median Filter) RR intervals loại bỏ ngoại tâm thu và nhiễu co cơ.
4.3. Phân tích biến thiên nhịp tim HRV (Heart Rate Variability):
   - Chỉ số miền thời gian: Mean RR, SDNN, RMSSD.
   - Đồ thị Poincaré Plot: Phân tích phân tán $SD_1$, $SD_2$ và tỷ số $SD_1 / SD_2$.
   - Phổ mật độ công suất PSD (Welch Method): Phân tích phân bố công suất dải tần $0 - 65\text{Hz}$, chứng minh triệt tiêu nhiễu lưới $50\text{Hz}$ và trôi baseline.
4.4. Thuật toán xác thực té ngã 2 giai đoạn (2-Stage Fall Detection with Post-Fall Inactivity Verification):
   - **Giai đoạn 1 (Xung va đập):** Phát hiện xung gia tốc $SMV = \sqrt{a_x^2 + a_y^2 + a_z^2} \ge 2.5g$ $\to$ Kích hoạt ứng viên ngã và mở cửa sổ thẩm định $2.0\text{s}$.
   - **Giai đoạn 2 (Thẩm định bất động & tư thế nằm):**
     - *Tự hủy báo động giả:* Nếu người dùng đứng thẳng ($Tilt < 35^\circ$) và vận động bình thường ($Gyro > 80^\circ/\text{s}$) trong vòng $2\text{s}$ $\to$ Hủy ứng viên ngã.
     - *Xác nhận ngã thật (Confirmed Fall):* Khi hết cửa sổ $2.0\text{s}$, nếu thỏa mãn đồng thời:
       + Góc nghiêng cơ thể $Tilt \ge 50^\circ$ (tư thế nằm sàn).
       + Trạng thái bất động: Vận tốc góc $Gyro < 65^\circ/\text{s}$ và gia tốc tĩnh ổn định ($|SMV - 1.0g| < 0.45g$).
       + Kích hoạt chốt giữ cảnh báo trong $3\text{ giây}$ gửi UDP $\to$ Gateway $\to$ còi hú $\to$ Telegram/Zalo $\to$ Web.

---

### CHƯƠNG 5: HƯỚNG DẪN TRÌNH BÀY KẾT QUẢ THỰC NGHIỆM & THẢO LUẬN (4–5 trang)
5.1. Danh mục đồ thị & giao diện thực nghiệm cần chèn vào báo cáo:
   - **Hình 5.1 (MATLAB):** Tín hiệu ECG thô, tín hiệu sau lọc số Pan-Tompkins Bandpass và năng lượng tích phân MWI gán nhãn đỉnh R.
   - **Hình 5.2 (MATLAB):** Biểu đồ Poincaré Plot biểu diễn phân bố elip các cặp khoảng $RR_n - RR_{n+1}$.
   - **Hình 5.3 (MATLAB):** Phổ mật độ công suất PSD Welch chứng minh năng lượng tập trung ở dải $5 - 15\text{Hz}$ và triệt tiêu hoàn toàn nhiễu lưới $50\text{Hz}$.
   - **Hình 5.4 (MATLAB):** Động lực học véc-tơ gia tốc $SMV$ phân tích 3 pha té ngã.
   - **Hình 5.5 (Web Dashboard PWA):** Màn hình giám sát trực quan thời gian thực tại [https://iomt.tsbyin.dev](https://iomt.tsbyin.dev) với đồ thị điện tim Oscilloscope tia quét phosphor 60 FPS và dải HUD Sinh hiệu Lâm sàng (Pulse LED, Thân nhiệt, Gia tốc va đập).
   - **Hình 5.6 (Web Dashboard PWA):** Giao diện bảng điều khiển dạng Drawer trượt trên thiết bị di động, cơ chế cô lập Stacking Context và thanh cảnh báo khẩn cấp té ngã tức thời.
   - **Hình 5.7 (Web Dashboard Replay):** Giao diện thanh công cụ InfluxDB Replay phát lại dải sóng 60 giây quanh thời điểm té ngã với tần số thực $250\text{ Hz}$.
5.2. Hướng dẫn lập bảng thống kê chỉ số lâm sàng (Trích xuất từ file kết quả phiên đo thực tế của nhóm):
   - Tần số lấy mẫu thực tế ($F_s$) và tổng thời gian ghi nhận tín hiệu phiên đo.
   - Tổng số mẫu thu nhận và số lượng phức bộ QRS bóc tách được.
   - Nhịp tim trung bình, nhịp tim nhỏ nhất (Min) và lớn nhất (Max) trong phiên đo (BPM).
   - Các chỉ số biến thiên nhịp tim HRV: $SDNN$, $RMSSD$, trục phân tán ngắn hạn $SD_1$, trục dài hạn $SD_2$ và tỷ số $SD_1 / SD_2$.
   - Thân nhiệt bề mặt trung bình ghi nhận từ cảm biến DS18B20.
5.3. Đánh giá thử nghiệm chức năng cảnh báo té ngã:
   - Mô tả kịch bản thử nghiệm: So sánh giữa các cú va đập té ngã mô phỏng ($SMV \ge 2.5g$) và hoạt động thường nhật (ngồi nhanh, nằm phịch xuống giường).
   - Đánh giá cơ chế chống báo động giả: Hiệu quả của cửa sổ thẩm định tư thế nghiêng $Tilt$ và trạng thái bất động sau va đập.
   - Thời gian đáp ứng của còi hú Active Buzzer, hiển thị cảnh báo trên màn hình OLED và thời gian đẩy tin nhắn khẩn cấp qua Telegram Bot & Zalo Bot.
5.4. Đánh giá hiệu năng mạng truyền thông IoT:
   - Tầng mạng nội bộ (Wi-Fi UDP Socket): Đánh giá độ trễ truyền gói tin điểm-điểm ($< 5\text{ ms}$) và tỷ lệ mất gói tin ($< 0.2\%$).
   - Tầng đám mây (Cloudflare Tunnel WSS MQTT): Đánh giá thời gian phân phối thông điệp từ Gateway lên Broker và Web Dashboard ([https://iomt.tsbyin.dev](https://iomt.tsbyin.dev)).
   - Đánh giá độ ổn định kết nối đa thiết bị (PC, máy tính bảng, điện thoại di động) và cơ chế bộ nhớ đệm ngoại tuyến của PWA Service Worker.

---

### CHƯƠNG 6: KẾT LUẬN & HƯỚNG PHÁT TRIỂN (1–2 trang)
6.1. Những kết quả đã đạt được của đề tài:
   - Hiện thực hóa thành công chuỗi thu nhận tín hiệu y sinh đa kênh (ECG $250\text{ Hz}$, Thân nhiệt $1\text{s}$, Gia tốc $125\text{ Hz}$) với độ trễ truyền dẫn nội vi $< 5\text{ ms}$.
   - Triển khai thuật toán xử lý tín hiệu tại biên (Edge DSP): Lọc nhiễu IIR Notch 50Hz, Pan-Tompkins bóc tách đỉnh R tính nhịp tim thời gian thực và thuật toán kiểm định té ngã 2 giai đoạn (SMV $\ge 2.8g$ kết hợp $Tilt \ge 40^\circ$ và $Gyro < 45^\circ/\text{s}$).
   - Xây dựng hệ sinh thái giám sát đám mây bảo mật: MQTT WSS TLS 1.3 qua Cloudflare Zero Trust Tunnel, Web Dashboard PWA Canvas 60 FPS và cảnh báo khẩn cấp tức thời qua Telegram Bot & Zalo API.

6.2. Hạn chế còn tồn tại:
   - Nhiễu rung cơ (EMG Artifacts) và dịch chuyển điện cực khi người bệnh vận động mạnh hoặc thay đổi tư thế đột ngột.
   - Thiết bị hiện tại mới dừng ở mức mô hình thực nghiệm trên Breadboard và Kit phát triển, kích thước chưa tối ưu cho việc đeo liên tục 24/7.

6.3. Hướng phát triển trong tương lai (Future Technical Roadmap):
   - **Tối ưu Hóa Năng lượng & Kéo dài Tuổi thọ Pin Node đeo:**
     - Áp dụng kỹ thuật hạ xung nhịp CPU động (Dynamic Frequency Scaling) xuống $80\text{ MHz}$ kết hợp chế độ Wi-Fi Modem-sleep và tắt Bluetooth nội vi, giúp pin Li-Po $1000\text{ mAh}$ đạt thời gian vận hành liên tục $\approx 14.5\text{ giờ}$ (đáp ứng chu kỳ 1 ngày sinh hoạt ban ngày, sạc đêm).
     - Định hướng thế hệ tiếp theo: Chuyển đổi tầng vật lý vô tuyến từ Wi-Fi sang **Bluetooth Low Energy (BLE 5.0 Coded PHY)** với mức tiêu thụ dòng cực thấp ($5 - 8\text{ mA}$) để nâng tuổi thọ pin lên $> 3 - 5\text{ ngày}$ liên tục.
   - **Đảm bảo Toàn vẹn Dữ liệu Tầng Ứng dụng (Application-Layer CRC-16):**
     - Nâng cấp cấu trúc khung nhị phân từ 60 bytes lên 62 bytes, tích hợp trường mã kiểm tra **CRC-16-CCITT (Đa thức 0x1021)** ở cuối gói tin nhằm tuân thủ tiêu chuẩn phần mềm thiết bị y tế **IEC 62304 / ISO 13485**, loại bỏ triệt để nguy cơ sai lệch dữ liệu do suy đồi bộ nhớ RAM hoặc xén đệm socket LwIP.
   - **Cơ sở Dữ liệu Chuỗi Thời gian & Giải pháp Chống Bùng nổ Dữ liệu (TSDB Tiered Storage):**
     - Giải quyết bài toán lưu trữ dài hạn trước áp lực dữ liệu khổng lồ ($21,600,000\text{ mẫu ECG/ngày/bệnh nhân}$):
       + **Tầng 1 (Chỉ số tổng hợp):** Lưu trữ nhịp tim (BPM), thân nhiệt và gia tốc SMV định kỳ $5\text{ giây}$/lần vào **InfluxDB v2**, kết xuất biểu đồ xu hướng nhiều tuần/tháng trên **Grafana Dashboard** phục vụ người thân và người giám hộ theo dõi hồi cứu.
       + **Tầng 2 (Bắt vết biến cố - Event-Triggered Ring Buffer):** Gateway duy trì bộ đệm tròn $15\text{ KB SRAM}$ ($7,500\text{ mẫu} = 30\text{s}$ trước sự cố). Khi phát hiện té ngã hoặc rối loạn nhịp tim (BPM $< 50$ hoặc BPM $> 120$), Gateway ghi tiếp $30\text{s}$ sau sự cố và lưu trữ trọn vẹn dải sóng $60\text{ giây}$ chất lượng cao làm bằng chứng y sinh phục vụ phân tích lâm sàng.
   - **Trí tuệ Nhân tạo Biên (TinyML):** Nhúng mô hình học sâu 1D-CNN hoặc mạng nơ-ron trọng số thấp trực tiếp lên ESP32 để phân loại tự động các dạng loạn nhịp tim nguy hiểm (Rung nhĩ - AFib, Ngoại tâm thu thất - PVC).
   - **Đóng gói Cơ khí Y tế:** Thiết kế vỏ bảo vệ in 3D chống nước chuẩn IP67 và mạch in PCB 2 lớp siêu nhỏ gọn dạng hộp đeo ngực (Chest Strap / Wearable Patch).

---

### TÀI LIỆU THAM KHẢO (IEEE Standard)
1. J. Pan and W. J. Tompkins, "A Real-Time QRS Detection Algorithm," *IEEE Transactions on Biomedical Engineering*, vol. BME-32, no. 3, pp. 230-236, 1985.
2. ITU-T Recommendation Y.2060, "Overview of the Internet of things," International Telecommunication Union, 2012.
3. ANSI/AAMI EC13:2002, "Cardiac monitors, heart rate meters, and alarms," Association for the Advancement of Medical Instrumentation, 2002.
4. Circuit Digest, "IoT Elderly Fall Detection System with WhatsApp Alert," GitHub Repository: https://github.com/Circuit-Digest/Elderly-Fall-Detection-System, 2024.
