# 🛡️ CHÍNH SÁCH BẢO MẬT & AN TOÀN Y SINH (SECURITY POLICY)
## Dual-MCU IoMT Wearable System Security & Clinical Safety

Tài liệu này xác định các chính sách bảo mật mạng và khuyến cáo an toàn điện học y sinh đối với thiết bị đeo IoMT đo điện tâm đồ và cảnh báo té ngã.

---

## ⚕️ 1. KHUYẾN CÁO AN TOÀN ĐIỆN HỌC Y SINH (ELECTRICAL SAFETY)

> ⚠️ **CẢNH BÁO QUAN TRỌNG:** Thiết bị AD8232 kết nối trực tiếp các điện cực Ag/AgCl vào da người bệnh nhân, do đó việc tuân thủ an toàn điện sinh học (chuẩn **IEC 60601-1**) là ưu tiên hàng đầu!

1. **Nguồn cấp an toàn (Power Isolation):**
   * **Bắt buộc:** Chỉ cấp nguồn cho Node cảm biến ESP32-C3 bằng **Pin sạc Li-Po/Li-Ion (3.7V - 4.2V)** hoặc cổng USB của máy tính xách tay **đang chạy bằng pin thuần túy** (đã rút cắm sạc 220V).
   * **Tuyệt đối nghiêm cấm:** Không được cắm nguồn ESP32 vào củ sạc điện thoại hoặc máy tính đang cắm nguồn lưới điện $220\text{V}$ không có bộ cách ly y tế (Medical Galvanic Isolation), nhằm tránh nguy cơ dòng rò điện áp cao gây nguy hiểm cho người đeo điện cực.
2. **Khuyến cáo ứng dụng:**
   * Dự án được thiết kế cho mục đích học tập, nghiên cứu khoa học và phát triển nguyên mẫu (Prototype). Không được sử dụng thay thế các thiết bị chẩn đoán lâm sàng chuyên dụng trong phòng hồi sức cấp cứu (ICU) mà chưa qua kiểm định của cơ quan y tế có thẩm quyền (Bộ Y tế / FDA / CE).

---

## 🔒 2. CHÍNH SÁCH BẢO MẬT MẠNG & DỮ LIỆU IOMT (CYBERSECURITY)

1. **Giao thức Wi-Fi UDP Nội bộ (Local Socket):**
   * Mạng SoftAP `BIOMED_GW` sử dụng dải IP nội bộ `192.168.4.x`, cô lập hoàn toàn lưu lượng gói tin UDP sinh hiệu giữa Node C3 và Gateway, tránh bị nghe lén từ mạng ngoài.
2. **Bảo mật luồng MQTT Cloud qua Cloudflare Tunnel (WSS):**
   * Dữ liệu truyền từ Gateway ESP32 lên máy chủ EMQX qua Internet được đóng gói trong kênh truyền **WebSocket Secure (WSS)** qua cổng **443** với chứng chỉ mã hóa TLS/SSL toàn cầu của Cloudflare.
   * Ngăn chặn hoàn toàn các cuộc tấn công trung gian (Man-in-the-Middle - MITM), nghe lén hoặc chèn sửa đổi gói tin điện tim ECG trên mạng công cộng.
   * Kích hoạt cơ chế xác thực dựa trên mật khẩu (Password-based Authentication) trên EMQX để chỉ các thiết bị được cấp phép mới được quyền Publish / Subscribe.
3. **Quản lý Thông tin Bí mật & API Token (Secrets Management):**
   * Toàn bộ Token Telegram Bot (`TELEGRAM_BOT_TOKEN`), Token Zalo Bot (`ZALO_BOT_TOKEN`), Chat ID và mật khẩu Wi-Fi/MQTT được lưu riêng biệt trong các file cục bộ:
     * `ESP32/secrets.h`
     * `MATLAB/config_private.m`
   * Các file này đã được đưa vào `.gitignore` để **tuyệt đối không bao giờ bị đẩy lên GitHub hoặc rò rỉ ra ngoài**.
   * Dự án cung cấp các file mẫu công khai `secrets_example.h` và `config_example.m` với các giá trị placeholder để người dùng tham khảo cấu hình.
4. **Bảo mật kênh Cảnh báo Khẩn cấp (Telegram & Zalo):**
   * Giao tiếp giữa Gateway ESP32/MATLAB với máy chủ Telegram API và Zalo Bot Platform API được thực hiện qua giao thức HTTPS (TLS Port 443) an toàn.

---

## 🚨 3. BÁO CÁO LỖ HỔNG BẢO MẬT (REPORTING VULNERABILITIES)

Nếu bạn phát hiện bất kỳ lỗ hổng bảo mật nào về phần mềm, giao thức truyền thông hoặc nguy cơ phần cứng:
* Vui lòng **không đăng công khai** lên GitHub Issues.
* Hãy liên hệ trực tiếp với tác giả:
  * **Tác giả:** Nguyễn Văn Tuấn Sỹ
  * **GitHub:** [@tsbyinchei](https://github.com/tsbyinchei)
  * **Email:** [contact@tsbyin.dev](mailto:contact@tsbyin.dev) • [tsbyinchei@gmail.com](mailto:tsbyinchei@gmail.com)
* Chúng tôi sẽ tiếp nhận, kiểm tra và phát hành bản vá lỗi trong thời gian sớm nhất.
