# 📐 CƠ SỞ TOÁN HỌC & NGUYÊN LÝ THUẬT TOÁN (ALGORITHMS)
## Mathematical Foundations & Signal Processing Architecture

Tài liệu này trình bày chi tiết cơ sở toán học của 2 thuật toán cốt lõi được triển khai trong hệ thống:
1. **Thuật toán Pan-Tompkins hoàn chỉnh (Bóc tách đỉnh R thời gian thực & HRV)**
2. **Thuật toán Phát hiện Té ngã Động lực học 3 Pha (3-Phase Fall Dynamic Verification)**

---

## 🫀 1. THUẬT TOÁN PAN-TOMPKINS BÓC TÁCH ĐỈNH R

Thuật toán Pan-Tompkins (1985) là tiêu chuẩn vàng trong xử lý tín hiệu điện tim để bóc tách phức bộ QRS. Tín hiệu đầu vào là chuỗi rời rạc $x[n]$ lấy mẫu ở tần số $F_s = 250\text{ Hz}$ ($T = 4\text{ ms}$).

```
  x[n] ──> [ 1. Lọc thông dải ] ──> [ 2. Đạo hàm ] ──> [ 3. Bình phương ] ──> [ 4. Tích phân MWI ] ──> [ 5. Ngưỡng thích ứng ] ──> Đỉnh R
            (5 - 15 Hz)              (Làm nổi dốc)      (Phi tuyến)            (Cửa sổ 38 mẫu)        (Trơ sinh lý 380ms)
```

### Bước 1: Bộ lọc thông dải số (Bandpass Filter 5–15 Hz)
Mục tiêu: Loại bỏ trôi đường đẳng điện do nhịp thở ($< 0.5\text{Hz}$) và nhiễu xoay chiều 50Hz, nhiễu co cơ EMG ($> 30\text{Hz}$).
Hàm truyền đạt trong miền Z:
$$H_{BP}(z) = H_{LP}(z) \cdot H_{HP}(z)$$

Trong MATLAB, bộ lọc được thiết kế dưới dạng IIR Butterworth bậc 2 với dải thông $[5\text{Hz}, 15\text{Hz}]$.  
Để đảm bảo tín hiệu liên tục không bị gián đoạn giữa các khối 25 mẫu ($100\text{ms}$), bộ lọc duy trì vector trạng thái trễ $\mathbf{z}_{bp}$:
$$[y[n], \mathbf{z}_{bp}] = \text{filter}(b, a, x[n], \mathbf{z}_{bp})$$

### Bước 2: Đạo hàm bậc một 5-điểm (Five-Point Derivative)
Làm nổi bật sườn dốc đứng của phức bộ QRS và triệt tiêu các sóng P, T có tần số thấp:
$$y[n] = \frac{1}{8T} \left( 2x[n] + x[n-1] - x[n-3] - 2x[n-4] \right)$$
Hàm truyền đạt trong miền Z:
$$H(z) = \frac{1}{8T} \left( 2 + z^{-1} - z^{-3} - 2z^{-4} \right)$$

### Bước 3: Phép toán bình phương phi tuyến (Squaring Function)
Biến đổi toàn bộ giá trị âm thành dương và khuếch đại phi tuyến các đỉnh nhọn QRS so với sóng nền:
$$y[n] = \left( x[n] \right)^2$$

### Bước 4: Tích phân cửa sổ trượt (Moving Window Integrator - MWI)
Trích xuất độ rộng của phức bộ QRS và tích lũy năng lượng sóng. Độ rộng cửa sổ $N$ được chọn bằng $38\text{ mẫu} \approx 150\text{ ms}$ (tương đương bề rộng sinh lý tối đa của phức bộ QRS người):
$$y[n] = \frac{1}{N} \sum_{k=0}^{N-1} x[n - k]$$

### Bước 5: Ngưỡng thích ứng kép & Thời gian trơ sinh lý
* **Cập nhật ngưỡng tín hiệu ($SPKI$) và ngưỡng nhiễu ($NPKI$):**
  $$SPKI = 0.125 \cdot PEEK + 0.875 \cdot SPKI$$
  $$NPKI = 0.125 \cdot PEEK + 0.875 \cdot NPKI$$
* **Ngưỡng quyết định thích ứng ($Threshold$):**
  $$Threshold = NPKI + 0.25 \cdot (SPKI - NPKI)$$
* **Thời gian trơ sinh lý (Refractory Period):**  
  Sau khi phát hiện 1 đỉnh R, tim người không thể co bóp lại ngay lập tức (thời kỳ trơ tuyệt đối của cơ tim). Thuật toán khóa nhận diện trong **$380\text{ ms}$ ($95\text{ mẫu} @ 250\text{Hz}$)**, loại bỏ triệt để hiện tượng đếm đúp đỉnh sóng T.
* **Bộ lọc trung vị RR (Median Filter):**
  $$BPM = \frac{60}{\text{median}(RR_{valid})}$$

### Bước 6: Phân tích Biến thiên Nhịp tim HRV & Biểu đồ Poincaré
* **Độ lệch chuẩn khoảng cách RR (SDNN):**
  $$SDNN = \sqrt{\frac{1}{K-1} \sum_{i=1}^K (RR_i - \overline{RR})^2}$$
* **Căn bậc hai trung bình bình phương các hiệu kế tiếp (RMSSD):**
  $$RMSSD = \sqrt{\frac{1}{K-1} \sum_{i=1}^{K-1} (RR_{i+1} - RR_i)^2}$$
* **Chỉ số hình học Poincaré Plot ($SD_1, SD_2$):**
  $$SD_1 = \sqrt{\frac{1}{2} \text{Var}(RR_n - RR_{n+1})}, \quad SD_2 = \sqrt{\frac{1}{2} \text{Var}(RR_n + RR_{n+1})}$$
  Trong đó $SD_1$ đo lường biến thiên ngắn hạn (phó giao cảm), $SD_2$ đo lường biến thiên dài hạn (giao cảm và phó giao cảm).

### Bước 7: Phổ Mật độ Công suất PSD (Welch's Method)
Tín hiệu được chia thành $K$ đoạn có độ dài $L$ chồng lấn $50\%$, áp dụng cửa sổ Hamming $w[n]$:
$$\hat{P}_{Welch}(f) = \frac{1}{K} \sum_{k=1}^K \frac{1}{F_s \sum_{n=0}^{L-1} w^2[n]} \left| \sum_{n=0}^{L-1} x_k[n] w[n] e^{-j 2\pi f n / F_s} \right|^2$$
Phổ công suất chứng minh năng lượng tập trung tại dải tần số $5 - 15\text{Hz}$ của phức bộ QRS và triệt tiêu hoàn toàn nhiễu nguồn điện $50\text{Hz}$ và trôi dạt đường đẳng điện $< 0.5\text{Hz}$.

---

## 🤸 2. THUẬT TOÁN PHÁT HIỆN TÉ NGÃ 3 PHA

### 2.1. Vector độ lớn gia tốc (Signal Magnitude Vector - SMV)
Gia tốc được chuẩn hóa độc lập với hướng đặt cảm biến:
$$SMV[n] = \sqrt{a_x[n]^2 + a_y[n]^2 + a_z[n]^2}$$
Ở trạng thái tĩnh bình thường chịu trọng lực Trái Đất: $SMV \approx 1.0g$.

### 2.2. Kiểm định động học 3 pha (3-Phase Fall Verification)

```
                 Va chạm chạm sàn
                     (Peak >= 2.5g)
                         ▲
                        / \
                       /   \
  SMV = 1.0g ─────────      \
              \      /       \────────── Bất động (σ < 0.15g trong >= 2s)
               \    /                    Góc nghiêng thân lệch > 60°
                ▼  /
             Rơi tự do
            (SMV < 0.6g)
```

1. **Pha 1 (Free-Fall):** Cơ thể bắt đầu mất thăng bằng rơi xuống $\to$ Triệt tiêu trọng trường $\to$ $SMV < 0.6g$ kéo dài từ $100\text{ms} - 250\text{ms}$.
2. **Pha 2 (Impact):** Cơ thể đập mạnh vào mặt sàn $\to$ Xung lực phản chấn cực đại $SMV \ge 2.5g - 3.5g$.
3. **Pha 3 (Post-fall Inactivity & Tilt):**
   * *Bất động:* Sau va đập, người bệnh nằm im $\ge 2\text{ giây}$:
     $$\sigma_{SMV} = \sqrt{\frac{1}{M}\sum_{k=1}^M (SMV_k - \mu)^2} < 0.15g$$
   * *Đổi tư thế:* Góc nghiêng thân mình so với phương thẳng đứng trước khi ngã lệch $> 60^\circ$:
     $$\theta_{tilt} = \arccos\left( \frac{\mathbf{g}_{pre} \cdot \mathbf{g}_{post}}{\|\mathbf{g}_{pre}\| \|\mathbf{g}_{post}\|} \right) > 60^\circ$$
   * Nhờ đó loại bỏ hoàn toàn các trường hợp ngồi nhanh hoặc nhảy lên đệm.

---

## 📚 TÀI LIỆU THAM KHẢO (REFERENCES)
1. **Pan, J., & Tompkins, W. J. (1985).** A real-time QRS detection algorithm. *IEEE Transactions on Biomedical Engineering*, (3), 230-236.
2. **Circuit Digest (2024).** IoT Elderly Fall Detection System with WhatsApp Alert.  
   Mã nguồn mở: [https://github.com/Circuit-Digest/Elderly-Fall-Detection-System](https://github.com/Circuit-Digest/Elderly-Fall-Detection-System) (Cơ chế tính góc nghiêng `Tilt Angle` và kiểm tra bất động qua `Gyroscope`).
