# 🌐 IoMT Realtime Telehealth Dashboard
## ĐỀ TÀI: "THIẾT KẾ HỆ THỐNG IoT THEO DÕI ĐIỆN TIM, THÂN NHIỆT VÀ CẢNH BÁO TÉ NGÃ TỪ XA CHO NGƯỜI CAO TUỔI TẠI NHÀ"

Giao diện Web PWA (Internet of Medical Things) hiển thị và giám sát dữ liệu thời gian thực dành cho đề tài **"Thiết kế hệ thống IoT theo dõi điện tim, thân nhiệt và cảnh báo té ngã từ xa cho người cao tuổi tại nhà"**, kết nối qua giao thức WebSocket Secure (WSS TLS 1.3) tại địa chỉ chính thức: [https://iomt.tsbyin.dev](https://iomt.tsbyin.dev).

---

## 🚀 Tính năng Kỹ thuật Nổi bật (Phiên bản v1.9)

### 1. Đồ thị Điện tim ECG 60 FPS (Phosphor Sweep Oscilloscope)
* **Bộ quét tia Phosphor y tế:** Mô phỏng cơ chế quét phosphor của máy đo điện tim chuyên dụng trong bệnh viện, khử giật lag, kết xuất mượt mà 60 khung hình/giây.
* **Tần số lấy mẫu y sinh $250\text{ Hz}$:** Tiếp nhận 25 mẫu/100ms từ trạm Gateway ESP32, hiển thị dạng sóng P-QRS-T sắc nét với khe quét xóa dữ liệu cũ (Sweep Erase Gap 26px).
* **Tự động cân chỉnh biên độ hiển thị (Dynamic Auto-Gain Envelope):** Thu phóng linh hoạt theo biên độ sóng người bệnh, có ngưỡng sàn chống khuếch đại nhiễu tĩnh điện khi chưa dán điện cực.
* **Tia quét laser & Điểm sáng dẫn đầu (Medical Sweep Cursor):** Đèn quét phát quang gradient neon xanh lục (`#00ff88`) kèm chấm laser trắng chỉ dẫn thời điểm hiện tại.

### 2. Dải HUD Sinh hiệu Lâm sàng Tích hợp (Clinical Telemetry HUD Strip)
Thay thế hoàn toàn bố cục thẻ card rời rạc trước đây bằng dải giám sát sinh hiệu liên hoàn chuẩn monitor y tế:
* **Node đèn xung nhịp tim (Visual Pulse LED `#heartPulse`):** Nhấp nháy màu đỏ đồng bộ từng nhát đập của tim khi nhận diện phức bộ QRS, kết hợp âm thanh bíp tim qua Web Audio API.
* **Ruy-băng nhiệt độ lâm sàng (`#tempNeedle`):** Trực quan hóa thân nhiệt biểu mô từ cảm biến DS18B20 với thang đo từ $30^\circ\text{C}$ đến $42^\circ\text{C}$, vạch cảnh báo sốt $\ge 37.5^\circ\text{C}$ và hạ thân nhiệt $< 35.0^\circ\text{C}$.
* **Đồng hồ gia tốc 2 phân tầng (`#smvGauge`):** Hiển thị vector gia tốc toàn phần $SMV = \sqrt{a_x^2 + a_y^2 + a_z^2}$ ($0\text{--}4.0g$) kèm vạch chỉ điểm ngưỡng cảnh báo va đập $2.5g$.
* **Dải phụ đo lường hệ thống (Telemetry Mini Sub-Strip):** Tích hợp bộ đếm mẫu ECG nhận được, số lần phát hiện té ngã, số lần kích hoạt báo động khẩn cấp và độ trễ kết nối.

### 3. Hệ thống Thanh bên Di động & Cách ly Stacking Context (Mobile Drawer System)
* **Tối ưu hóa đa thiết bị (Responsive Breakpoints):**
  - **Desktop (> 1024px):** Thanh điều khiển dàn phẳng ngang (horizontal action bar) trong Header.
  - **Mobile & Tablet ($\le 1024\text{px}$):** Tự động thu gọn vào Drawer trượt từ cạnh phải với hiệu ứng chuyển động mượt mà bằng CSS Hardware Acceleration.
* **Cách ly Stacking Context (Khắc phục triệt để lỗi mờ & kẹt backdrop):**
  - Thiết lập phân tầng z-index phân cấp nghiêm ngặt: `.top-nav` đạt `z-index: 100000 !important`, `.nav-action-group` đạt `z-index: 100001 !important`, và `.drawer-backdrop` nằm ở `z-index: 99990 !important`.
  - Loại bỏ hoàn toàn lớp mờ GPU `backdrop-filter: blur(4px)` trên Mobile Backdrop, chuyển sang màu phủ phẳng `rgba(0, 0, 0, 0.72)`. Nhờ đó, menu Drawer trên điện thoại hiển thị sắc nét 100%, không bị lớp mờ đè lên mặt.
* **Đồng bộ hóa chuyển đổi màn hình (Adaptive Resize Sync):**
  - Trình lắng nghe sự kiện `resize` tự động đóng Drawer và giải phóng lớp nền khi màn hình mở rộng trở lại kích thước Desktop (>1024px).
  - Tự động đồng bộ co giãn kích thước Canvas qua `requestAnimationFrame(resizeCanvas)`, ngăn chặn hiện tượng méo tỷ lệ hoặc kẹt khung khi chuyển đổi qua lại giữa PC và Mobile.

### 4. Vận hành Song song 3 Chế độ (Tri-Mode Operation)
* **Chế độ Trực tiếp (Live MQTT Stream):** Kết nối liên tục với trạm Gateway ESP32 qua giao thức an toàn `wss://mqtt.tsbyin.dev/mqtt` (TLS 1.3), tự động khôi phục kết nối khi mạng chập chờn.
* **Chế độ Giả lập Toán học (Math Simulator Demo):** Mô phỏng chuỗi sóng P-QRS-T theo công thức hàm số sinh học và kích hoạt kiểm định té ngã thử nghiệm offline khi không có kit phần cứng.
* **Chế độ Phát lại Biến cố InfluxDB (60s Fall Event Replay):** Tích hợp trực tiếp với InfluxDB v2 REST API (`/api/v2/query`), truy vấn dải sóng 60 giây của các ca té ngã thực tế (30s trước và 30s sau cú va đập) và phát lại với tần số thực $250\text{ Hz}$ trên Oscilloscope kèm thanh tua nhanh/chậm/tạm dừng.

### 5. Cảnh báo Khẩn cấp & Điều khiển Còi hú Từ xa (Remote Mute)
* **Emergency Alarm Siren:** Khi phát hiện va chạm té ngã ($SMV \ge 2.8g$), giao diện kích hoạt banner cảnh báo nhấp nháy đỏ rực kèm âm thanh còi cứu thương decibel cao (Web Audio API).
* **Nút Tắt còi từ xa (Remote Mute):** Gửi lệnh điều khiển MQTT tới topic `biomed/gateway/cmd` để dập tắt tiếng còi hú Active Buzzer trên kit Gateway ESP32 ngay tức thì.
* **Cơ chế an toàn:** Tự động hoàn nguyên trạng thái còi khi người cao tuổi phục hồi hoặc sau khi sự cố được xác nhận xử lý xong.

### 6. Ứng dụng Web Cấp tiến (PWA - Progressive Web App v1.9)
* Hỗ trợ cài đặt trực tiếp lên màn hình chính điện thoại hoặc máy tính (Add to Home Screen) mà không cần thông qua kho ứng dụng.
* Bộ nhớ đệm Service Worker thông minh (`sw.js?v=1.9`) cho phép giao diện hoạt động mượt mà ngay cả khi mất kết nối mạng Internet.

---

## 🛠️ Hướng dẫn Triển khai & Vận hành

### Cách 1: Chạy trực tiếp trên máy phát triển cục bộ
1. Mở thư mục `web/` bằng VS Code.
2. Khởi chạy bằng tiện ích mở rộng **Live Server** (Port `5500`) hoặc mở trực tiếp file `index.html` trên trình duyệt Chrome / Edge.

### Cách 2: Triển khai Đám mây qua Ubuntu Server & Cloudflare Tunnel
1. Đặt mã nguồn thư mục `web/` vào thư mục phục vụ web của Nginx hoặc quản lý qua 1Panel trên máy chủ Ubuntu Server.
2. Cấu hình dịch vụ Cloudflare Zero Trust Tunnel định tuyến tên miền `iomt.tsbyin.dev` tới cổng HTTP nội bộ của Nginx.
3. Truy cập an toàn qua giao thức HTTPS: [https://iomt.tsbyin.dev](https://iomt.tsbyin.dev).
