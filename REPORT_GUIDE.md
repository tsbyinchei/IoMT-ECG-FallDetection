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
5.1. Danh mục 4 đồ thị thực nghiệm cần chèn vào báo cáo (Xuất từ thư mục `MATLAB/figures/`):
   - **Hình 5.1:** Tín hiệu ECG thô, tín hiệu sau lọc số Pan-Tompkins Bandpass và năng lượng tích phân MWI gán nhãn đỉnh R.
   - **Hình 5.2:** Biểu đồ Poincaré Plot biểu diễn phân bố elip các cặp khoảng $RR_n - RR_{n+1}$.
   - **Hình 5.3:** Phổ mật độ công suất PSD Welch chứng minh năng lượng tập trung ở dải $5 - 15\text{Hz}$ và triệt tiêu hoàn toàn nhiễu lưới $50\text{Hz}$.
   - **Hình 5.4:** Động lực học véc-tơ gia tốc $SMV$ phân tích 3 pha té ngã.
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
   - Tầng mạng nội bộ (Wi-Fi UDP Socket): Đánh giá độ trễ truyền gói tin điểm-điểm và tỷ lệ mất gói tin.
   - Tầng đám mây (Cloudflare Tunnel WSS MQTT): Đánh giá thời gian phân phối thông điệp từ Gateway lên Broker và Web Dashboard.

---

### CHƯƠNG 6: KẾT LUẬN & HƯỚNG PHÁT TRIỂN (1–2 trang)
6.1. Những kết quả đã đạt được của đề tài.  
6.2. Hạn chế còn tồn tại (vấn đề nhiễu khi vận động mạnh, kích thước breadboard).  
6.3. Hướng phát triển trong tương lai:
   - Đóng gói mạch in PCB 2 lớp nhỏ gọn dạng hộp đồng hồ đeo ngực.
   - Ứng dụng mô hình học sâu (TinyML / CNN 1D) nhận diện loạn nhịp tim (Arrhythmia classification) trực tiếp trên ESP32.
   - Kết nối cơ sở dữ liệu bệnh viện đám mây (HL7/FHIR Standard).

---

### TÀI LIỆU THAM KHẢO (IEEE Standard)
1. J. Pan and W. J. Tompkins, "A Real-Time QRS Detection Algorithm," *IEEE Transactions on Biomedical Engineering*, vol. BME-32, no. 3, pp. 230-236, 1985.
2. ITU-T Recommendation Y.2060, "Overview of the Internet of things," International Telecommunication Union, 2012.
3. ANSI/AAMI EC13:2002, "Cardiac monitors, heart rate meters, and alarms," Association for the Advancement of Medical Instrumentation, 2002.
4. Circuit Digest, "IoT Elderly Fall Detection System with WhatsApp Alert," GitHub Repository: https://github.com/Circuit-Digest/Elderly-Fall-Detection-System, 2024.
