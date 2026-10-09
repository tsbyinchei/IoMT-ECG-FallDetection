# 🌐 IoMT Realtime Telehealth Dashboard

Trang hiển thị và giám sát y tế thời gian thực (**Realtime Telehealth Monitor**) dành cho đề tài **IoMT-ECG-FallDetection**, thiết kế theo tiêu chuẩn màn hình monitor phòng hồi sức cấp cứu (**ICU Patient Monitor**).

---

## 🚀 Tính năng nổi bật

1. **Đồ thị điện tim ECG 60 FPS (Phosphor Sweep Oscilloscope):**
   - Bộ quét tia phosphor giống monitor y tế thực tế, khử giật lag, hiển thị dạng sóng 250Hz.
2. **Theo dõi sinh hiệu đa kênh:**
   - Nhịp tim tự động (BPM) kèm hoạt ảnh tim đập phập phồng (Visual Heart Pulse) và âm thanh tiếng bíp QRS (Web Audio API).
   - Thân nhiệt bề mặt da (DS18B20) phân cấp theo tiêu chuẩn lâm sàng.
   - Thanh đo lực va đập gia tốc $SMV$ ($0 - 4.0g$) kèm vạch cảnh báo té ngã $2.5g$.
   - Trạng thái hở điện cực (Smart Leads-Off Detection).
3. **Cảnh báo khẩn cấp tức thời (Emergency Alarm Siren):**
   - Banner cảnh báo nhấp nháy đỏ rực và âm thanh còi cứu thương khi phát hiện ngã.
4. **Bảng kiểm định lâm sàng (Clinical Audit Trail):**
   - Nhật ký ghi lại toàn bộ các biến cố với mốc thời gian chi tiết.
5. **Chế độ Demo (Simulator Mode):**
   - Cho phép mô phỏng phát sóng điện tim P-QRS-T và kích hoạt té ngã thử nghiệm mà không cần cắm kit phần cứng.
6. **Bảo mật tuyệt đối:**
   - Cấu hình MQTT Broker, tài khoản và mật khẩu được lưu vào `localStorage` của trình duyệt, không bao giờ hardcode lộ ra bên ngoài.

---

## 🌐 Đề xuất Subdomain & Triển khai

Do subdomain `iot.tsbyin.dev` đã được sử dụng cho mục đích khác, đề xuất các subdomain chuẩn cho trang Dashboard này:

| Subdomain đề xuất | Đánh giá | Lý do |
| :--- | :--- | :--- |
| ⭐ **`iomt.tsbyin.dev`** | **Tốt nhất (Khuyên dùng)** | Viết tắt chuẩn quốc tế của **Internet of Medical Things**, ngắn gọn, chuyên nghiệp và đúng tinh thần đề tài. |
| **`biomed.tsbyin.dev`** | Rất tốt | Mang tính học thuật y sinh cao, trùng với tên Wi-Fi AP `BIOMED_GW` của Gateway. |
| **`ecg.tsbyin.dev`** | Phù hợp | Trực diện vào tính năng theo dõi điện tim. |
| **`patient.tsbyin.dev`** | Phù hợp | Hướng bệnh viện / theo dõi bệnh nhân gia đình. |

---

## 🛠️ Hướng dẫn Triển khai nhanh

### Cách 1: Chạy trực tiếp trên máy cục bộ
Bạn có thể mở trực tiếp file [index.html](file:///c:/Users/TsByin/Documents/Arduino/IoT/web/index.html) bằng bất kỳ trình duyệt nào (Chrome, Edge, Safari, Firefox) hoặc chạy với VS Code Live Server.

### Cách 2: Triển khai lên Ubuntu Server (1Panel / Nginx) với subdomain `iomt.tsbyin.dev`
1. Đăng nhập vào 1Panel trên server `192.168.1.36`.
2. Vào mục **Website** $\to$ **Tạo trang web tĩnh (Static Site)**:
   - **Tên miền:** `iomt.tsbyin.dev`
   - **Thư mục gốc:** Tải toàn bộ thư mục `web/` lên.
3. Trong **Cloudflare Zero Trust Tunnel**:
   - Thêm Public Hostname mới:
     - Subdomain: `iomt`
     - Domain: `tsbyin.dev`
     - Service: `HTTP` $\to$ `localhost:<Cổng_Port_Nginx_1Panel>` (hoặc IP nội bộ `192.168.1.36:<Port>`).
4. Truy cập `https://iomt.tsbyin.dev` từ bất kỳ đâu trên thế giới!
