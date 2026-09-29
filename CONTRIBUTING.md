# 🤝 HƯỚNG DẪN ĐÓNG GÓP PHÁT TRIỂN (CONTRIBUTING GUIDE)
## Dual-MCU IoMT Wearable ECG Monitoring & Fall Detection System
**Quản lý dự án (Maintainer):** Nguyễn Văn Tuấn Sỹ ([@tsbyinchei](https://github.com/tsbyinchei))  

Cảm ơn bạn đã quan tâm và muốn đóng góp cho dự án thiết bị y tế đeo người IoMT! Dưới đây là các hướng dẫn và quy chuẩn kỹ thuật nhằm đảm bảo chất lượng mã nguồn và tính chính xác sinh học của hệ thống.

---

## 🛠️ 1. THIẾT LẬP MÔI TRƯỜNG PHÁT TRIỂN

### 1.1. Phần cứng & Vi điều khiển (Arduino IDE)
1. **Board Packages:**
   * Cài đặt `esp32` by Espressif Systems (phiên bản $\ge 2.0.14$).
2. **Cấu hình biên dịch cho ESP32-C3 SuperMini:**
   * Board: `ESP32C3 Dev Module`
   * USB CDC On Boot: `Enabled`
   * Flash Size: `4MB`
3. **Thư viện yêu cầu trên Gateway ESP32:**
   * `U8g2` (OLED SH1106 display driver)
   * `PubSubClient` (MQTT Client)
   * `Wire`, `WiFi`, `WiFiUdp` (Thư viện tích hợp sẵn)

### 1.2. Môi trường Xử lý Tín hiệu (MATLAB)
* Yêu cầu phiên bản **MATLAB R2020a** trở lên.
* Toolbox cần thiết:
  * *Signal Processing Toolbox* (phục vụ bộ lọc IIR Butterworth và tích phân số MWI).
  * *Instrument Control Toolbox* (phục vụ giao tiếp USB Serial).

---

## 📝 2. QUY CHUẨN MÃ NGUỒN (CODING CONVENTIONS)

1. **Chu kỳ lấy mẫu & Thời gian thực (C++ / Arduino):**
   * Tần số lấy mẫu ECG phải giữ cố định **$250\text{ Hz}$ ($4.0\text{ ms}$)**. Tuyệt đối không sử dụng hàm `delay()` trong các vòng lặp xử lý tín hiệu.
   * Giao tiếp I2C MPU-6050 phải luôn thiết lập `Wire.setTimeOut(20)` để chống treo CPU khi lỏng dây tiếp xúc.
2. **Quy chuẩn Xử lý Tín hiệu trên MATLAB:**
   * Mọi bộ lọc số IIR xử lý theo từng khối gói tin (Block processing) bắt buộc phải duy trì biến trạng thái trễ $\mathbf{z}_{bp}$ để tránh hiện tượng rung cạnh quá độ (Transient ringing).
   * Luôn sử dụng bộ lọc trung vị `median()` khi tính toán nhịp tim từ các khoảng RR để triệt tiêu các đỉnh nhiễu co cơ EMG hoặc va đập bất ngờ.

---

## 🔀 3. QUY TRÌNH TẠO PULL REQUEST (PR WORKFLOW)

1. **Fork** repository này về tài khoản GitHub của bạn.
2. Tạo nhánh làm việc mới cho tính năng hoặc bản sửa lỗi:
   ```bash
   git checkout -b feature/ten-tinh-nang-moi
   ```
3. Commit các thay đổi với thông điệp rõ ràng theo chuẩn Conventional Commits:
   * `feat: them tinh nang canh bao rung lắc`
   * `fix: sua loi mat ket noi i2c mpu6050`
   * `docs: cap nhat so do dau noi`
4. Push nhánh lên GitHub và tạo **Pull Request (PR)** về nhánh `main` của repo gốc.
5. Mô tả rõ mục tiêu thay đổi, kết quả đo kiểm thực nghiệm và ảnh chụp biểu đồ đối chiếu nếu có.

---

## ⚕️ 4. CAM KẾT ĐẠO ĐỨC Y SINH (BIOMEDICAL ETHICS)
* Dự án này được phát triển cho mục đích học thuật, nghiên cứu và giáo dục. Mọi dữ liệu ECG thực nghiệm được thu thập phải có sự đồng thuận của người thử nghiệm và đảm bảo an toàn điện học (chỉ dùng nguồn pin hoặc cổng USB laptop chạy bằng pin).
