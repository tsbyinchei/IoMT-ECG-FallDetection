%% =========================================================================
% SCRIPT PHÂN TÍCH CHUYÊN SÂU DỮ LIỆU THỰC NGHIỆM TỪ FILE .MAT
% PHỤC VỤ VIẾT BÁO CÁO 25 - 30 TRANG MÔN XỬ LÝ TÍN HIỆU Y SINH (BDSP)
% Tác giả: Nguyễn Văn Tuấn Sỹ (@tsbyinchei) - contact@tsbyin.dev
% =========================================================================
% Tự động đọc file 'ECG_Patient_Record.mat' và tạo các đồ thị báo cáo chuẩn:
% 1. Đồ thị phức hợp QRS, sóng P-T và khoảng RR
% 2. Biểu đồ Poincaré biểu diễn độ biến thiên nhịp tim HRV (SD1, SD2)
% 3. Phổ mật độ công suất PSD (Welch Method) chứng minh hiệu quả lọc dải
% 4. Phân tích động lực học gia tốc té ngã 3 pha (Free-fall -> Impact -> Rest)
% 5. Tự động xuất ảnh chất lượng cao (300 DPI) vào thư mục 'figures/'
% =========================================================================

clear; clc; close all;

filename = 'ECG_Patient_Record.mat';
if ~isfile(filename)
    error('Chưa tìm thấy file %s! Hãy chạy script thu thập tín hiệu trước.', filename);
end

load(filename, 'PatientRecord');
disp('====================================================================');
disp('  ĐÃ TẢI THÀNH CÔNG DỮ LIỆU THỰC NGHIỆM: ' + string(filename));
disp('====================================================================');

% Trích xuất các tín hiệu sinh hiệu
t            = PatientRecord.Signals.Time_sec;
raw          = PatientRecord.Signals.Raw_ECG_ADC;
filt         = PatientRecord.Signals.Filtered_ECG;
mwi          = PatientRecord.Signals.PanTompkins_MWI;
smv          = PatientRecord.Signals.SMV_Acceleration_g;
bpm          = PatientRecord.Signals.HeartRate_BPM;
r_peaks_idx  = PatientRecord.Events.R_Peaks_Indices;
r_peaks_amp  = PatientRecord.Events.R_Peaks_Amplitudes;
r_peaks_t    = PatientRecord.Events.R_Peaks_Time_sec;
Fs           = PatientRecord.Metadata.SamplingRate_Hz;

% Tạo thư mục lưu ảnh báo cáo nếu chưa có
output_dir = fullfile(fileparts(mfilename('fullpath')), 'figures');
if ~exist(output_dir, 'dir')
    mkdir(output_dir);
end

%% =========================================================================
%% FIGURE 1: ĐẶC TÍNH PHỨC HỢP QRS & BÓC TÁCH ĐỈNH R (KẾT QUẢ BDSP)
%% =========================================================================
fig1 = figure('Name', 'HÌNH 1: BÓC TÁCH ĐỈNH R & DẢI ĐIỆN TIM ECG', ...
              'Color', 'w', 'Position', [80, 80, 1100, 650]);

subplot(2, 1, 1);
plot(t, raw, 'Color', [0.65 0.65 0.65], 'LineWidth', 0.8); hold on;
plot(t, filt, 'b', 'LineWidth', 1.2);
if ~isempty(r_peaks_t)
    plot(r_peaks_t, r_peaks_amp, 'ro', 'MarkerFaceColor', 'r', 'MarkerSize', 5.5);
end
grid on;
title('(a) Tín hiệu điện tim ECG gốc và tín hiệu sau lọc Bandpass (5-15 Hz) có gán nhãn đỉnh R', 'FontSize', 11, 'FontWeight', 'bold');
xlabel('Thời gian (giây)'); ylabel('Biên độ ADC (12-bit)');
legend('Tín hiệu thô (ADC)', 'Sau lọc Pan-Tompkins Bandpass', 'Đỉnh R (R-Peaks)', 'Location', 'northeast');
xlim([min(t) min(min(t) + 6, max(t))]); % Thu phóng hiển thị 6 giây đầu để thấy rõ hình dạng sóng

subplot(2, 1, 2);
plot(t, mwi, 'm', 'LineWidth', 1.2); grid on;
title('(b) Năng lượng tích phân cửa sổ trượt (Moving Window Integration - 150ms)', 'FontSize', 11, 'FontWeight', 'bold');
xlabel('Thời gian (giây)'); ylabel('Năng lượng');
xlim([min(t) min(min(t) + 6, max(t))]);

export_fig1 = fullfile(output_dir, 'Hinh1_BocTach_QRS_PanTompkins.png');
print(fig1, export_fig1, '-dpng', '-r300');
fprintf('[XUẤT ẢNH] Đã lưu Hình 1 độ phân giải 300 DPI: %s\n', export_fig1);

%% =========================================================================
%% FIGURE 2: PHÂN TÍCH ĐỘ BIẾN THIÊN NHỊP TIM HRV (POINCARÉ PLOT)
%% =========================================================================
sd1 = 0; sd2 = 0; rmssd = 0; sdnn = 0;
if length(r_peaks_t) >= 4
    rr_intervals_ms = diff(r_peaks_t) * 1000; % ms
    rr_n   = rr_intervals_ms(1:end-1);
    rr_n1  = rr_intervals_ms(2:end);

    % Tính các chỉ số HRV kinh điển
    diff_rr = rr_n - rr_n1;
    sum_rr  = rr_n + rr_n1;
    sd1     = sqrt(var(diff_rr) / 2);
    sd2     = sqrt(var(sum_rr) / 2);
    sdnn    = std(rr_intervals_ms);
    rmssd   = sqrt(mean(diff_rr .^ 2));

    fig2 = figure('Name', 'HÌNH 2: BIỂU ĐỒ POINCARÉ PHÂN TÍCH HRV', ...
                  'Color', 'w', 'Position', [120, 120, 650, 550]);
    scatter(rr_n, rr_n1, 45, 'filled', 'MarkerFaceColor', [0.15 0.5 0.85], 'MarkerEdgeColor', [0.05 0.25 0.5]); hold on;
    min_ax = min([rr_n(:); rr_n1(:)]) - 40;
    max_ax = max([rr_n(:); rr_n1(:)]) + 40;
    if min_ax < max_ax
        plot([min_ax max_ax], [min_ax max_ax], 'r--', 'LineWidth', 1.2);
        grid on; axis square;
        xlim([min_ax max_ax]); ylim([min_ax max_ax]);
    else
        grid on; axis square;
    end
    title(sprintf('Biểu đồ Poincaré phân tích HRV (SD1 = %.2f ms, SD2 = %.2f ms)', sd1, sd2), 'FontSize', 11, 'FontWeight', 'bold');
    xlabel('Khoảng RR_n (ms)'); ylabel('Khoảng RR_{n+1} (ms)');
    legend('Cặp khoảng RR kế tiếp', 'Đường đẳng giác quy chiếu (y = x)', 'Location', 'northwest');

    export_fig2 = fullfile(output_dir, 'Hinh2_BieuDo_Poincare_HRV.png');
    print(fig2, export_fig2, '-dpng', '-r300');
    fprintf('[XUẤT ẢNH] Đã lưu Hình 2 độ phân giải 300 DPI: %s\n', export_fig2);
end

%% =========================================================================
%% FIGURE 3: PHỔ MẬT ĐỘ CÔNG SUẤT PSD (WELCH METHOD - CHỨNG MINH LỌC NHIỄU)
%% =========================================================================
fig3 = figure('Name', 'HÌNH 3: PHỔ MẬT ĐỘ CÔNG SUẤT PSD', ...
              'Color', 'w', 'Position', [160, 160, 950, 500]);

nfft = 1024;
if exist('pwelch', 'file') == 2 || exist('pwelch', 'builtin')
    window = hamming(min(round(Fs * 2), length(raw)));
    noverlap = round(length(window) * 0.5);
    [pxx_raw, f_raw]   = pwelch(raw - mean(raw), window, noverlap, nfft, Fs);
    [pxx_filt, f_filt] = pwelch(filt - mean(filt), window, noverlap, nfft, Fs);
else
    % Fallback tính phổ mật độ công suất bằng FFT cơ bản
    L = length(raw);
    Y_raw = fft(raw - mean(raw));
    Y_filt = fft(filt - mean(filt));
    P2_raw = abs(Y_raw / L); P1_raw = P2_raw(1:floor(L/2)+1); P1_raw(2:end-1) = 2 * P1_raw(2:end-1);
    P2_filt = abs(Y_filt / L); P1_filt = P2_filt(1:floor(L/2)+1); P1_filt(2:end-1) = 2 * P1_filt(2:end-1);
    f_raw = Fs * (0:(floor(L/2))) / L;
    f_filt = f_raw;
    pxx_raw = (P1_raw .^ 2) / Fs + 1e-12;
    pxx_filt = (P1_filt .^ 2) / Fs + 1e-12;
end

plot(f_raw, 10*log10(max(pxx_raw, 1e-12)), 'Color', [0.7 0.7 0.7], 'LineWidth', 1.0); hold on;
plot(f_filt, 10*log10(max(pxx_filt, 1e-12)), 'b', 'LineWidth', 1.5);
xline(5, 'g--', 'Ngưỡng thông dải thấp (5 Hz)', 'LineWidth', 1.0);
xline(15, 'g--', 'Ngưỡng thông dải cao (15 Hz)', 'LineWidth', 1.0);
xline(50, 'r:', 'Nhiễu tần số lưới điện (50 Hz)', 'LineWidth', 1.2);
grid on;
xlim([0 65]);
title('Phổ mật độ công suất (Power Spectral Density - Welch Method)', 'FontSize', 11, 'FontWeight', 'bold');
xlabel('Tần số (Hz)'); ylabel('Mật độ công suất (dB/Hz)');
legend('Phổ tín hiệu thô (Có nhiễu 50Hz và nhiễu chậm)', ...
       'Phổ sau lọc Pan-Tompkins (Tập trung năng lượng dải QRS 5-15Hz)', ...
       'Dải thông 5-15Hz', '', 'Nhiễu lưới 50Hz đã bị triệt tiêu', ...
       'Location', 'northeast');

export_fig3 = fullfile(output_dir, 'Hinh3_PhoCongSuat_PSD_Welch.png');
print(fig3, export_fig3, '-dpng', '-r300');
fprintf('[XUẤT ẢNH] Đã lưu Hình 3 độ phân giải 300 DPI: %s\n', export_fig3);

%% =========================================================================
%% FIGURE 4: PHÂN TÍCH ĐỘNG LỰC HỌC GIA TỐC TÉ NGÃ 3 PHA (SMV)
%% =========================================================================
fig4 = figure('Name', 'HÌNH 4: PHÂN TÍCH GIA TỐC TÉ NGÃ 3 PHA (SMV)', ...
              'Color', 'w', 'Position', [200, 200, 1000, 500]);
plot(t, smv, 'Color', [0.85 0.2 0.35], 'LineWidth', 1.3); hold on;
yline(2.5, 'r--', 'Ngưỡng va đập ngã (Impact Threshold = 2.5g)', 'LineWidth', 1.2);
yline(0.6, 'b--', 'Ngưỡng rơi tự do (Free-fall Threshold = 0.6g)', 'LineWidth', 1.0);
grid on;
title('Phân tích động lực học véc-tơ gia tốc SMV trong sự kiện té ngã', 'FontSize', 11, 'FontWeight', 'bold');
xlabel('Thời gian (giây)'); ylabel('Gia tốc SMV (g)');
ylim([0 max(4.2, max(smv) + 0.5)]);
legend('Vector gia tốc SMV = \surd(a_x^2 + a_y^2 + a_z^2)', ...
       'Ngưỡng va chạm (2.5g)', 'Ngưỡng không trọng lực rơi tự do (0.6g)', 'Location', 'northeast');

export_fig4 = fullfile(output_dir, 'Hinh4_GiaToc_TeNga_SMV.png');
print(fig4, export_fig4, '-dpng', '-r300');
fprintf('[XUẤT ẢNH] Đã lưu Hình 4 độ phân giải 300 DPI: %s\n', export_fig4);

%% =========================================================================
%% IN BẢNG TỔNG HỢP SỐ LIỆU ĐỂ COPY THẲNG VÀO WORD / LATEX
%% =========================================================================
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
fprintf('| Độ biến thiên nhịp tim (SDNN)                | %.2f ms             |\n', sdnn);
fprintf('| Độ biến thiên nhịp tim (RMSSD)               | %.2f ms             |\n', rmssd);
fprintf('| Chỉ số Poincaré SD1 (Biến thiên ngắn hạn)    | %.2f ms             |\n', sd1);
fprintf('| Chỉ số Poincaré SD2 (Biến thiên dài hạn)     | %.2f ms             |\n', sd2);
fprintf('| Tỷ số SD1 / SD2                              | %.3f                |\n', sd1 / max(0.001, sd2));
fprintf('| Thân nhiệt trung bình                        | %.2f °C             |\n', PatientRecord.ClinicalSummary.Mean_BodyTemp);
fprintf('| Số lần kích hoạt cảnh báo té ngã             | %d lần              |\n', PatientRecord.Events.Total_Falls_Detected);
fprintf('====================================================================\n');
fprintf('>>> TẤT CẢ BIỂU ĐỒ 300 DPI ĐÃ ĐƯỢC XUẤT VÀO THƯ MỤC: MATLAB/figures/ <<<\n');
