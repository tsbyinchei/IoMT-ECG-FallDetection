# 🌐 IoMT Realtime Telehealth Dashboard
## ĐỀ TÀI: "THIẾT KẾ HỆ THỐNG IoT THEO DÕI ĐIỆN TIM, THÂN NHIỆT VÀ CẢNH BÁO TÉ NGÃ TỪ XA CHO NGƯỜI CAO TUỔI TẠI NHÀ"

Giao diện Web PWA (Internet of Medical Things) hiển thị và giám sát dữ liệu thời gian thực dành cho đề tài **"Thiết kế hệ thống IoT theo dõi điện tim, thân nhiệt và cảnh báo té ngã từ xa cho người cao tuổi tại nhà"**, kết nối qua giao thức WebSocket Secure (WSS).

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
4. **Tắt còi báo động từ xa (Remote Mute):**
   - Nút **🔕 Tắt còi từ xa** trên thanh điều khiển hoặc ngay trong Banner cảnh báo.
   - Gửi lệnh MQTT tới topic `biomed/gateway/cmd` để dập tắt tiếng còi chip trên kit Gateway ESP32 ngay tức thì.
   - Hỗ trợ tắt còi tại chỗ bằng nút cứng **BOOT** trên Gateway ESP32 khi người thân đã tiếp cận bệnh nhân.
   - Tự động tái kích hoạt còi khi người bệnh hồi phục và kết thúc sự cố (tuân thủ tiêu chuẩn an toàn cảnh báo y tế).
5. **Bảng kiểm định lâm sàng (Clinical Audit Trail):**
   - Nhật ký ghi lại toàn bộ các biến cố với mốc thời gian chi tiết.
6. **Vận hành song song 3 Chế độ (Tri-Mode Operation):**
   - **Trực tiếp (Live MQTT Stream):** Giám sát thời gian thực từ trạm Gateway ESP32 qua WSS TLS 1.3.
   - **Giả lập Toán học (Math Simulator Demo):** Mô phỏng dải sóng P-QRS-T và kích hoạt kiểm định té ngã thử nghiệm offline khi không có phần cứng và mạng.
   - **Phát lại Lịch sử (InfluxDB Replay):** Kết nối InfluxDB v2 REST API (`/api/v2/query`), tải dải sóng 60 giây của các ca biến cố thực tế (30s trước + 30s sau ngã) và phát lại với tần số thực $250\text{ Hz}$ trên Oscilloscope kèm thanh điều khiển Play/Pause/Tua lại.
7. **Hỗ trợ PWA (Progressive Web App):**
   - Tích hợp trọn bộ icon chuẩn (`48px` - `512px`), splash screen và logo thương hiệu sắc nét `assets/logo.webp`.
   - Cài đặt Service Worker `sw.js` và `manifest.json` cho phép **Cài đặt lên màn hình chính điện thoại/máy tính (Add to Home Screen)** như một ứng dụng native.
   - Hỗ trợ hoạt động ngoại tuyến (Offline Mode) nhờ cơ chế cache tài nguyên thông minh.
8. **Bảo mật tuyệt đối:**
   - Cấu hình MQTT Broker, tài khoản và mật khẩu được lưu vào `localStorage` của trình duyệt, không bao giờ hardcode lộ ra bên ngoài.

---

## 🛠️ Hướng dẫn Triển khai nhanh

### Cách 1: Chạy trực tiếp trên máy cục bộ
Bạn có thể mở trực tiếp file [index.html](index.html) bằng bất kỳ trình duyệt nào (Chrome, Edge, Safari, Firefox) hoặc chạy với VS Code Live Server.

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
