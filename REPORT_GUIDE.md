# 📖 KHUNG SƯỜN BÁO CÁO MÔN HỌC (25–30 TRANG)
## Academic Project Report Guide for IoT & BDSP
**Tác giả:** Nguyễn Văn Tuấn Sỹ ([@tsbyinchei](https://github.com/tsbyinchei)) • [contact@tsbyin.dev](mailto:contact@tsbyin.dev)  

Tài liệu này cung cấp mục lục chi tiết, khung sườn nội dung và các bảng số liệu, biểu đồ thực nghiệm cần đưa vào để hoàn thiện bài báo cáo môn học **Thiết kế Hệ thống IoT** và **Xử lý Tín hiệu Y sinh (BDSP)** đạt điểm tối đa (A/A+).

---

## 📑 MỤC LỤC BÁO CÁO CHUẨN

* **LỜI CẢM ƠN & TÓM TẮT ĐỀ TÀI (ABSTRACT)**
* **DANH MỤC TỪ VIẾT TẮT (IoMT, ECG, QRS, MWI, SMV, HRV, UDP, MQTT)**
* **DANH MỤC HÌNH VẼ & BẢNG BIỂU**

---

### CHƯƠNG 1: TỔNG QUAN & TÍNH CẤP THIẾT CỦA ĐỀ TÀI (4–5 trang)
1.1. Bối cảnh già hóa dân số và các bệnh lý tim mạch, đột quỵ, té ngã ở người cao tuổi.  
1.2. Hạn chế của các phương pháp giám sát y tế truyền thống tại bệnh viện.  
1.3. Xu hướng ứng dụng Internet of Medical Things (IoMT) và thiết bị đeo (Wearable Devices).  
1.4. Mục tiêu nghiên cứu và phạm vi thực nghiệm của đề tài.  
1.5. Cấu trúc bài báo cáo.

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
4.4. Thuật toán phân biệt Té ngã thật sự với Ngồi nhanh / Nằm xuống:
   - 3 pha: Rơi tự do ($<0.6g$) $\to$ Va đập ($>2.5g$) $\to$ Bất động góc nghiêng ($>50^\circ$, gyro $<65^\circ/\text{s}$).

---

### CHƯƠNG 5: KẾT QUẢ THỰC NGHIỆM & THẢO LUẬN (4–5 trang)
5.1. Danh mục 4 đồ thị thực nghiệm chuẩn in ấn 300 DPI (Thư mục `MATLAB/figures/`):
   - **Hình 5.1:** Tín hiệu ECG thô, tín hiệu sau lọc số Pan-Tompkins Bandpass và năng lượng tích phân MWI gán nhãn đỉnh R.
   - **Hình 5.2:** Biểu đồ Poincaré Plot biểu diễn phân bố elip các cặp khoảng $RR_n - RR_{n+1}$.
   - **Hình 5.3:** Phổ mật độ công suất PSD Welch chứng minh năng lượng tập trung ở dải $5 - 15\text{Hz}$ và triệt tiêu hoàn toàn nhiễu lưới $50\text{Hz}$.
   - **Hình 5.4:** Động lực học véc-tơ gia tốc $SMV$ phân tích 3 pha té ngã.
5.2. Bảng kết quả thống kê lâm sàng chính thức từ file thực nghiệm `ECG_Patient_Record.mat`:
   - Tần số lấy mẫu: $F_s = 250\text{ Hz}$ ($T_s = 4.0\text{ ms}$).
   - Tổng số mẫu thu nhận: $117,225\text{ mẫu}$ ($468.90\text{ giây} \approx 7.8\text{ phút}$).
   - Tổng số phức bộ QRS bóc tách: $533\text{ đỉnh R}$.
   - Nhịp tim trung bình: $82.1\text{ BPM}$ (Min: $45.6\text{ BPM}$, Max: $150.0\text{ BPM}$).
   - Chỉ số HRV: $SDNN = 1427.23\text{ ms}$, $RMSSD = 2046.21\text{ ms}$.
   - Thông số Poincaré: $SD_1 = 1448.25\text{ ms}$, $SD_2 = 1408.47\text{ ms}$, tỷ số $SD_1 / SD_2 = 1.028$.
   - Thân nhiệt bề mặt trung bình: $32.46^\circ\text{C}$.
5.3. Kết quả thử nghiệm cảnh báo té ngã:
   - Xung va đập đạt đỉnh $3.5g > 2.5g$.
   - Cảnh báo tức thời đồng thời trên màn hình OLED `! FALL DETECTED !`, còi hú Active Buzzer và **tin nhắn khẩn cấp gửi về smartphone qua Telegram (@your_telegram_bot) & Zalo Bot** trong $< 500\text{ms}$.
5.4. Đánh giá hiệu năng mạng IoT phân tầng:
   - Tầng nội bộ (Wi-Fi UDP Socket): Độ trễ $< 4\text{ms}$, tỷ lệ mất gói (Packet Loss Rate) $0\%$.
   - Tầng đám mây (Cloudflare Tunnel WSS MQTT): Độ trễ $< 50\text{ms}$, bảo mật mã hóa SSL/TLS Port 443 toàn diện.

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
