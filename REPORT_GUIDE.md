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
4.4. Thuật toán phân biệt Té ngã thật sự với Ngồi nhanh / Nằm xuống.

---

### CHƯƠNG 5: KẾT QUẢ THỰC NGHIỆM & THẢO LUẬN (4–5 trang)
5.1. Kết quả thu nhận sóng ECG thực nghiệm tại bàn đo:
   - Ảnh chụp sóng ECG thô và sóng sau lọc số.
   - Đánh dấu chính xác từng đỉnh sóng R bằng chấm đỏ trên MATLAB.
5.2. Kết quả đo nhịp tim thực tế:
   - Nhịp tim lúc nghỉ: $73 – 77\text{ BPM}$ (đối chiếu chuẩn với Samsung Galaxy Watch 7: sai số $< 3\%$).
5.3. Kết quả thử nghiệm cảnh báo té ngã:
   - Đồ thị xung gia tốc va đập đạt đỉnh $3.5g > 2.5g$.
   - Cảnh báo tức thời trên OLED `! FALL DETECTED !` và còi hú buzzer.
5.4. Kết quả đo thân nhiệt bề mặt: $32.2^\circ\text{C} – 32.3^\circ\text{C}$.
5.5. Đánh giá hiệu năng mạng IoT:
   - Độ trễ truyền UDP giữa Node C3 và Gateway: $< 4\text{ms}$.
   - Tỷ lệ mất gói tin (Packet Loss Rate): $0\%$.

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
