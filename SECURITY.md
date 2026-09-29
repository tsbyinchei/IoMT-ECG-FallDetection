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
2. **Bảo mật luồng MQTT Cloud:**
   * Khi triển khai trên môi trường Internet (Bản 2 qua 1Panel), khuyến nghị bật xác thực tên người dùng/mật khẩu trên MQTT Broker và sử dụng cổng mã hóa **MQTTS (TLS Port 8883)**.
   * Dữ liệu sinh hiệu cá nhân (ECG, nhịp tim, thân nhiệt) cần được ẩn danh hóa (Anonymization) trước khi lưu trữ vào cơ sở dữ liệu chuỗi thời gian InfluxDB.

---

## 🚨 3. BÁO CÁO LỖ HỔNG BẢO MẬT (REPORTING VULNERABILITIES)

Nếu bạn phát hiện bất kỳ lỗ hổng bảo mật nào về phần mềm, giao thức truyền thông hoặc nguy cơ phần cứng:
* Vui lòng **không đăng công khai** lên GitHub Issues.
* Hãy liên hệ trực tiếp với tác giả:
  * **Tác giả:** Nguyễn Văn Tuấn Sỹ
  * **GitHub:** [@tsbyinchei](https://github.com/tsbyinchei)
  * **Email:** [contact@tsbyin.dev](mailto:contact@tsbyin.dev) • [tsbyinchei@gmail.com](mailto:tsbyinchei@gmail.com)
* Chúng tôi sẽ tiếp nhận, kiểm tra và phát hành bản vá lỗi trong thời gian sớm nhất.
