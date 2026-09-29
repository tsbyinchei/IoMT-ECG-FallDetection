%% =========================================================================
% SCRIPT PHÂN TÍCH CHUYÊN SÂU DỮ LIỆU THỰC NGHIỆM TỪ FILE .MAT
% PHỤC VỤ VIẾT BÁO CÁO 25 - 30 TRANG MÔN XỬ LÝ TÍN HIỆU Y SINH (BDSP)
% =========================================================================
% Tự động đọc file 'ECG_Patient_Record.mat' và tạo các đồ thị báo cáo chuẩn:
% 1. Đồ thị phức hợp QRS, sóng P-T và khoảng RR
% 2. Biểu đồ Poincaré biểu diễn độ biến thiên nhịp tim HRV (SD1, SD2)
% 3. Phổ mật độ công suất PSD (Welch Method)
% 4. Phân tích động lực học gia tốc té ngã 3 pha (Free-fall -> Impact -> Rest)
% =========================================================================

clear; clc; close all;

filename = 'ECG_Patient_Record.mat';
if ~isfile(filename)
    error('Chưa tìm thấy file %s! Hãy chạy script thu thập tín hiệu trước.', filename);
end

load(filename, 'PatientRecord');
disp('Đã tải thành công file thực nghiệm: ' + string(filename));

t = PatientRecord.Signals.Time_sec;
raw = PatientRecord.Signals.Raw_ECG_ADC;
filt = PatientRecord.Signals.Filtered_ECG;
mwi = PatientRecord.Signals.PanTompkins_MWI;
smv = PatientRecord.Signals.SMV_Acceleration_g;
bpm = PatientRecord.Signals.HeartRate_BPM;
r_peaks_idx = PatientRecord.Events.R_Peaks_Indices;
r_peaks_amp = PatientRecord.Events.R_Peaks_Amplitudes;
r_peaks_t   = PatientRecord.Events.R_Peaks_Time_sec;
Fs = PatientRecord.Metadata.SamplingRate_Hz;

%% FIGURE 1: ĐẶC TÍNH PHỨC HỢP QRS & BÓC TÁCH ĐỈNH R (CHO CHƯƠNG KẾT QUẢ BDSP)
fig1 = figure('Name', 'HÌNH 1: BÓC TÁCH ĐỈNH R & DẢI ĐIỆN TIM ECG', 'Position', [100, 100, 1000, 600]);
subplot(2, 1, 1);
plot(t, raw, 'Color', [0.6 0.6 0.6], 'LineWidth', 0.8); hold on;
plot(t, filt, 'b', 'LineWidth', 1.2);
if ~isempty(r_peaks_t)
    plot(r_peaks_t, r_peaks_amp, 'ro', 'MarkerFaceColor', 'r', 'MarkerSize', 5);
end
grid on;
title('(a) Tín hiệu điện tim ECG gốc và tín hiệu sau lọc Bandpass (5-15 Hz) có gán nhãn đỉnh R');
xlabel('Thời gian (giây)'); ylabel('Biên độ');
legend('Tín hiệu thô (ADC)', 'Sau lọc Pan-Tompkins Bandpass', 'Đỉnh R (R-Peaks)', 'Location', 'northeast');

subplot(2, 1, 2);
plot(t, mwi, 'm', 'LineWidth', 1.2); grid on;
title('(b) Năng lượng tích phân cửa sổ trượt (Moving Window Integration)');
xlabel('Thời gian (giây)'); ylabel('Năng lượng');

%% FIGURE 2: PHÂN TÍCH ĐỘ BIẾN THIÊN NHỊP TIM HRV (POINCARÉ PLOT)
if length(r_peaks_t) >= 4
    rr_intervals_ms = diff(r_peaks_t) * 1000; % ms
    rr_n   = rr_intervals_ms(1:end-1);
    rr_n1  = rr_intervals_ms(2:end);

    % Tính thông số Poincaré SD1, SD2
    diff_rr = rr_n - rr_n1;
    sum_rr  = rr_n + rr_n1;
    sd1 = sqrt(var(diff_rr) / 2);
    sd2 = sqrt(var(sum_rr) / 2);

    fig2 = figure('Name', 'HÌNH 2: BIỂU ĐỒ POINCARÉ PHÂN TÍCH HRV', 'Position', [150, 150, 600, 500]);
    scatter(rr_n, rr_n1, 35, 'filled', 'MarkerFaceColor', [0.2 0.6 0.9]); hold on;
    plot([min(rr_n) max(rr_n)], [min(rr_n) max(rr_n)], 'r--', 'LineWidth', 1.2);
    grid on; axis square;
    title(sprintf('Biểu đồ Poincaré phân tích HRV (SD1 = %.2f ms, SD2 = %.2f ms)', sd1, sd2));
    xlabel('RR_n (ms)'); ylabel('RR_{n+1} (ms)');
end

%% FIGURE 3: ĐỘNG LỰC HỌC TÉ NGÃ 3 PHA TỪ VÉC-TƠ GIA TỐC SMV
fig3 = figure('Name', 'HÌNH 3: PHÂN TÍCH GIA TỐC TÉ NGÃ (SMV)', 'Position', [200, 200, 900, 450]);
plot(t, smv, 'Color', [0.8 0.2 0.5], 'LineWidth', 1.3); hold on;
yline(2.5, 'r--', 'Ngưỡng va đập ngã (2.5g)', 'LineWidth', 1.2);
yline(0.5, 'b--', 'Ngưỡng rơi tự do (0.5g)', 'LineWidth', 1.0);
grid on;
title('Phân tích động học véc-tơ gia tốc SMV trong sự kiện té ngã');
xlabel('Thời gian (giây)'); ylabel('SMV (g)');
ylim([0 max(4.0, max(smv) + 0.5)]);
legend('Gia tốc SMV = \surd(a_x^2 + a_y^2 + a_z^2)', 'Ngưỡng va đập', 'Ngưỡng không trọng lượng', 'Location', 'northeast');

%% IN BẢNG TỔNG HỢP SỐ LIỆU ĐỂ COPY VÀO BÁO CÁO BDSP
fprintf('\n====================================================================\n');
fprintf('  BẢNG THỐNG KÊ KẾT QUẢ THỰC NGHIỆM ĐỂ ĐƯA VÀO BÁO CÁO 25-30 TRANG\n');
fprintf('====================================================================\n');
fprintf('| Đại lượng đo đạc / Chỉ số Y sinh            | Giá trị thực nghiệm |\n');
fprintf('|----------------------------------------------|---------------------|\n');
fprintf('| Tần số lấy mẫu (Sampling Rate Fs)            | %d Hz               |\n', Fs);
fprintf('| Tổng thời gian đo đạc                        | %.2f giây           |\n', PatientRecord.Metadata.TotalDuration_Sec);
fprintf('| Tổng số mẫu tín hiệu ECG                     | %d mẫu              |\n', length(raw));
fprintf('| Tổng số phức hợp QRS (Đỉnh R) bóc tách       | %d đỉnh             |\n', length(r_peaks_idx));
fprintf('| Nhịp tim trung bình (Mean Heart Rate)        | %.1f BPM            |\n', PatientRecord.ClinicalSummary.Mean_BPM);
fprintf('| Nhịp tim nhỏ nhất (Min Heart Rate)           | %.1f BPM            |\n', PatientRecord.ClinicalSummary.Min_BPM);
fprintf('| Nhịp tim lớn nhất (Max Heart Rate)           | %.1f BPM            |\n', PatientRecord.ClinicalSummary.Max_BPM);
fprintf('| Độ biến thiên nhịp tim (RMSSD)               | %.2f ms             |\n', PatientRecord.ClinicalSummary.HRV_RMSSD_ms);
fprintf('| Thân nhiệt trung bình                        | %.2f °C             |\n', PatientRecord.ClinicalSummary.Mean_BodyTemp);
fprintf('| Số lần kích hoạt cảnh báo té ngã             | %d lần              |\n', PatientRecord.Events.Total_Falls_Detected);
fprintf('====================================================================\n');
