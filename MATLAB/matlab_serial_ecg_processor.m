%% =========================================================================
% CHƯƠNG TRÌNH XỬ LÝ TÍN HIỆU ECG THỜI GIAN THỰC QUA CỔNG COM SERIAL (USB)
% DỰ ÁN: HỆ THỐNG IOMT NỘI BỘ OFFLINE (BẢN 1 - DEMO TRỰC TIẾP TẠI LỚP)
% MÔN HỌC: XỬ LÝ TÍN HIỆU Y SINH (BDSP) & THIẾT KẾ HỆ THỐNG IOT
% =========================================================================
% Giao thức: Đọc trực tiếp bản tin $DATA từ cổng COM của Gateway ESP32
% Tần số lấy mẫu: Fs = 250 Hz (25 mẫu/gói = 100ms)
% =========================================================================

clear; clc; close all;

%% 1. CẤU HÌNH CỔNG NỐI TIẾP SERIAL
% LƯU Ý QUAN TRỌNG: Hãy ĐÓNG cửa sổ Serial Monitor trong Arduino IDE trước khi chạy
% vì Windows không cho phép 2 phần mềm cùng mở một cổng COM cùng lúc!

% Cổng COM của ESP32 Gateway (Theo ảnh Arduino IDE của bạn là COM10)
comPort = "COM10";  
baudRate = 115200;

% Tự động nạp cấu hình Telegram Bot bảo mật
if exist('config_private.m', 'file') == 2
    run('config_private.m');
elseif exist('config_example.m', 'file') == 2
    run('config_example.m');
end
last_telegram_fall_time = -100;

availablePorts = serialportlist("available");
disp('Danh sách cổng COM đang rảnh trên máy:');
disp(availablePorts);

% Kiểm tra nếu cổng COM10 đang bị chiếm dụng
if ~ismember(comPort, availablePorts)
    warning('Không thấy cổng %s khả dụng! Rất có thể bạn đang mở Serial Monitor trong Arduino IDE.', comPort);
    disp('-> HÃY BẤM NÚT ĐÓNG (TẮT) CỬA SỔ SERIAL MONITOR TRÊN ARDUINO IDE RỒI CHẠY LẠI!');
end

fprintf('Đang mở cổng %s với Baudrate %d...\n', comPort, baudRate);
try
    s = serialport(comPort, baudRate, "Timeout", 5);
    configureTerminator(s, "LF");
    flush(s);
    disp('[OK] Đã kết nối cổng Serial thành công tới Gateway ESP32!');
catch ME
    error(['Không thể mở cổng ' char(comPort) ': ' ME.message ...
           newline '-> NGUYÊN NHÂN: Cổng COM đang bị Arduino IDE chiếm giữ. Hãy tắt Serial Monitor trong Arduino IDE!']);
end

%% 2. THÔNG SỐ VÀ KHỞI TẠO BỘ ĐỆM
Fs = 250;               % Tần số lấy mẫu (Hz)
dt = 1 / Fs;
BUFFER_LEN = 1000;      % Hiển thị 4 giây gần nhất
FALL_THRESHOLD = 2.5;   % Ngưỡng gia tốc ngã (g)

raw_ecg_buf  = zeros(1, BUFFER_LEN);
filt_ecg_buf = zeros(1, BUFFER_LEN);
mwi_buf      = zeros(1, BUFFER_LEN);
smv_buf      = ones(1, BUFFER_LEN);
bpm_buf      = zeros(1, BUFFER_LEN);
temp_buf     = zeros(1, BUFFER_LEN);

all_time     = [];
all_raw_ecg  = [];
all_filt_ecg = [];
all_mwi      = [];
all_smv      = [];
all_bpm      = [];
all_temp     = [];
all_r_peaks  = [];
fall_timestamps = [];

% Bộ lọc Pan-Tompkins
[b_bp, a_bp] = butter(2, [5 15] / (Fs / 2), 'bandpass');
z_bp = zeros(max(length(a_bp), length(b_bp)) - 1, 1); % Lưu trạng thái bộ lọc liên tục giữa các gói tin
window_size = round(0.150 * Fs); 
kernel_mwi = ones(1, window_size) / window_size;

%% 3. GIAO DIỆN ĐỒ THỊ REAL-TIME
fig = figure('Name', 'XỬ LÝ ĐIỆN TIM ECG & TÉ NGÃ OFFLINE QUA SERIAL (PAN-TOMPKINS)', ...
             'Color', [0.12 0.14 0.18], 'Position', [100, 80, 1100, 750], ...
             'CloseRequestFcn', @onFigureClose);

btnStop = uicontrol('Style', 'pushbutton', 'String', 'DỪNG & XUẤT FILE .MAT', ...
                    'Units', 'normalized', 'Position', [0.4, 0.015, 0.2, 0.04], ...
                    'BackgroundColor', [0.85 0.25 0.2], 'ForegroundColor', 'w', ...
                    'FontWeight', 'bold', 'FontSize', 10, ...
                    'Callback', @(src, event) setappdata(fig, 'is_running', false));

setappdata(fig, 'is_running', true);

ax1 = subplot(4, 1, 1);
h_raw = plot(ax1, 1:BUFFER_LEN, raw_ecg_buf, 'Color', [0.5 0.5 0.5], 'LineWidth', 0.8);
hold(ax1, 'on');
h_filt = plot(ax1, 1:BUFFER_LEN, filt_ecg_buf, 'Color', [0.1 0.85 0.3], 'LineWidth', 1.3);
h_rpeak_dots = plot(ax1, NaN, NaN, 'ro', 'MarkerFaceColor', [1 0.2 0.2], 'MarkerEdgeColor', 'w', 'LineWidth', 1.2, 'MarkerSize', 7);
grid(ax1, 'on');
set(ax1, 'Color', [0.08 0.09 0.12], 'XColor', [0.8 0.8 0.8], 'YColor', [0.8 0.8 0.8]);
title(ax1, '1. Tín hiệu điện tim ECG (Xám: Raw ADC | Xanh lá: Bandpass | Chấm đỏ: Đỉnh R)', 'Color', 'w', 'FontWeight', 'bold');
ylabel(ax1, 'ADC');

ax2 = subplot(4, 1, 2);
h_mwi = plot(ax2, 1:BUFFER_LEN, mwi_buf, 'Color', [0.2 0.7 1.0], 'LineWidth', 1.2);
hold(ax2, 'on');
h_thresh = plot(ax2, [1 BUFFER_LEN], [0 0], 'r--', 'LineWidth', 1.0);
grid(ax2, 'on');
set(ax2, 'Color', [0.08 0.09 0.12], 'XColor', [0.8 0.8 0.8], 'YColor', [0.8 0.8 0.8]);
title(ax2, '2. Tích phân năng lượng Pan-Tompkins & Bóc tách đỉnh R', 'Color', 'w', 'FontWeight', 'bold');
ylabel(ax2, 'Năng lượng');

ax3 = subplot(4, 1, 3);
yyaxis(ax3, 'left');
h_bpm = plot(ax3, 1:BUFFER_LEN, bpm_buf, 'Color', [1.0 0.4 0.2], 'LineWidth', 1.5);
ylabel(ax3, 'Nhịp tim (BPM)');
ylim(ax3, [40 160]);
yyaxis(ax3, 'right');
h_temp = plot(ax3, 1:BUFFER_LEN, temp_buf, 'Color', [0.9 0.8 0.2], 'LineWidth', 1.3);
ylabel(ax3, 'Nhiệt độ (°C)');
ylim(ax3, [34 42]);
grid(ax3, 'on');
set(ax3, 'Color', [0.08 0.09 0.12], 'XColor', [0.8 0.8 0.8]);
title(ax3, '3. Xu hướng Nhịp tim (BPM) & Thân nhiệt (°C)', 'Color', 'w', 'FontWeight', 'bold');

ax4 = subplot(4, 1, 4);
h_smv = plot(ax4, 1:BUFFER_LEN, smv_buf, 'Color', [0.9 0.3 0.9], 'LineWidth', 1.3);
hold(ax4, 'on');
yline(ax4, FALL_THRESHOLD, 'r--', 'NGƯỠNG TÉ NGÃ (2.5g)', 'Color', [1 0.2 0.2], 'LineWidth', 1.2);
grid(ax4, 'on');
set(ax4, 'Color', [0.08 0.09 0.12], 'XColor', [0.8 0.8 0.8], 'YColor', [0.8 0.8 0.8]);
title(ax4, '4. Vector gia tốc SMV (Signal Magnitude Vector MPU-6050)', 'Color', 'w', 'FontWeight', 'bold');
ylabel(ax4, 'Gia tốc (g)');
ylim(ax4, [0 4.0]);

%% 4. VÒNG LẶP ĐỌC SERIAL VÀ THỰC THI THUẬT TOÁN
total_samples_received = 0;
peak_threshold = 50000;
spki = 100000;
npki = 10000;
last_r_sample_idx = 0;
current_time_sec = 0;

while isvalid(fig) && getappdata(fig, 'is_running')
    if s.NumBytesAvailable == 0
        pause(0.01);
        continue;
    end

    lineData = readline(s);
    lineStr = strtrim(char(lineData));
    
    % Kiểm tra định dạng gói tin: $DATA,temp,smv,leadsOff,fall,bpm,ecg0,...,ecg24
    if ~startsWith(lineStr, '$DATA,')
        continue;
    end

    parts = strsplit(lineStr, ',');
    if length(parts) < 31 % $DATA + 5 params + 25 samples = 31 parts
        continue;
    end

    new_temp  = str2double(parts{2});
    new_smv   = str2double(parts{3});
    new_leads = str2double(parts{4});
    new_fall  = str2double(parts{5});
    edge_bpm  = str2double(parts{6});
    
    new_ecg = zeros(1, 25);
    for k = 1:25
        new_ecg(k) = str2double(parts{6 + k});
    end
    N_new = 25;

    time_new = current_time_sec + (1:N_new) * dt;
    current_time_sec = time_new(end);

    if (new_fall == 1 || new_smv >= FALL_THRESHOLD) && (current_time_sec - last_telegram_fall_time > 8)
        last_telegram_fall_time = current_time_sec;
        fall_timestamps(end+1) = current_time_sec; %#ok<AGROW>
        fprintf('[CẢNH BÁO TÉ NGÃ] Thời điểm: %.2fs | SMV = %.2fg\n', current_time_sec, new_smv);
        
        % Tự động gửi tin nhắn Telegram khẩn cấp tới điện thoại qua bot
        if exist('telegram_bot_token', 'var') && ~contains(telegram_bot_token, 'YOUR_')
            try
                msg_text = sprintf(['🚨 [CẢNH BÁO TÉ NGÃ - IoMT SYSTEM]\n' ...
                                    'Thời điểm: %.1fs\n' ...
                                    'Lực va đập SMV: %.2fg\n' ...
                                    'Nhịp tim: %.0f BPM\n' ...
                                    'Thân nhiệt: %.1f°C\n' ...
                                    'Tình trạng: Bất động sau ngã\n' ...
                                    '👉 Cần kiểm tra người bệnh ngay!'], ...
                                   current_time_sec, new_smv, current_bpm, new_temp);
                api_url = sprintf('https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s', ...
                                  telegram_bot_token, telegram_chat_id, urlencode(msg_text));
                webread(api_url);
                fprintf('[TELEGRAM] >>> Đã gửi tin nhắn cảnh báo khẩn cấp tới điện thoại thành công! <<<\n');
            catch ME
                fprintf('[TELEGRAM] Lỗi gửi tin nhắn: %s\n', ME.message);
            end
        end
    end

    % Pan-Tompkins với bộ nhớ trạng thái z_bp liên tục không rung nhiễu
    [new_filt, z_bp] = filter(b_bp, a_bp, new_ecg, z_bp);
    new_deriv = zeros(1, N_new);
    for n = 5:N_new
        new_deriv(n) = (2*new_filt(n) + new_filt(n-1) - new_filt(n-3) - 2*new_filt(n-4)) * (Fs / 8);
    end
    new_sq = new_deriv .^ 2;
    new_mwi = conv(new_sq, kernel_mwi, 'same');

    for i = 1:N_new
        global_sample_idx = total_samples_received + i;
        % Thời gian trơ sinh lý: 380ms (round(0.38 * Fs) = 95 mẫu) loại bỏ hoàn toàn sóng T
        if (new_mwi(i) > peak_threshold) && (global_sample_idx - last_r_sample_idx > round(0.38 * Fs))
            search_start = max(1, i - 12);
            search_end   = min(N_new, i + 12);
            [r_amp, local_peak_offset] = max(new_filt(search_start:search_end));
            r_idx = search_start + local_peak_offset - 1;

            last_r_sample_idx = global_sample_idx;
            all_r_peaks = [all_r_peaks; [global_sample_idx, r_amp, time_new(r_idx)]]; %#ok<AGROW>

            spki = 0.125 * new_mwi(i) + 0.875 * spki;
            peak_threshold = npki + 0.25 * (spki - npki);
        else
            npki = 0.125 * new_mwi(i) + 0.875 * npki;
            peak_threshold = npki + 0.25 * (spki - npki);
        end
    end

    % Ưu tiên lấy BPM tính toán từ các đỉnh R Pan-Tompkins với bộ lọc trung vị (Median Filter) chống nhiễu
    current_bpm = edge_bpm;
    if size(all_r_peaks, 1) >= 3
        recent_rr = diff(all_r_peaks(max(1, end-6):end, 3));
        valid_rr = recent_rr(recent_rr >= 0.40 & recent_rr <= 1.40);
        if ~isempty(valid_rr)
            current_bpm = 60 / median(valid_rr);
        end
    end

    % Lưu dữ liệu
    all_time     = [all_time, time_new]; %#ok<AGROW>
    all_raw_ecg  = [all_raw_ecg, new_ecg]; %#ok<AGROW>
    all_filt_ecg = [all_filt_ecg, new_filt]; %#ok<AGROW>
    all_mwi      = [all_mwi, new_mwi]; %#ok<AGROW>
    all_smv      = [all_smv, repmat(new_smv, 1, N_new)]; %#ok<AGROW>
    all_bpm      = [all_bpm, repmat(current_bpm, 1, N_new)]; %#ok<AGROW>
    all_temp     = [all_temp, repmat(new_temp, 1, N_new)]; %#ok<AGROW>
    total_samples_received = total_samples_received + N_new;

    % Cập nhật buffer
    raw_ecg_buf  = [raw_ecg_buf(N_new+1:end), new_ecg];
    filt_ecg_buf = [filt_ecg_buf(N_new+1:end), new_filt];
    mwi_buf      = [mwi_buf(N_new+1:end), new_mwi];
    smv_buf      = [smv_buf(N_new+1:end), repmat(new_smv, 1, N_new)];
    bpm_buf      = [bpm_buf(N_new+1:end), repmat(current_bpm, 1, N_new)];
    temp_buf     = [temp_buf(N_new+1:end), repmat(new_temp, 1, N_new)];

    % Cập nhật đồ thị với trừ DC offset để sóng hiển thị rõ ràng tại tâm 0
    raw_disp = raw_ecg_buf - mean(raw_ecg_buf);
    set(h_raw, 'YData', raw_disp);
    set(h_filt, 'YData', filt_ecg_buf);

    % Hiển thị chấm đỏ đánh dấu đỉnh R thời gian thực trong cửa sổ hiển thị
    buf_start_idx = total_samples_received - BUFFER_LEN + 1;
    if ~isempty(all_r_peaks)
        in_window_mask = (all_r_peaks(:, 1) >= buf_start_idx) & (all_r_peaks(:, 1) <= total_samples_received);
        if any(in_window_mask)
            r_x = all_r_peaks(in_window_mask, 1) - buf_start_idx + 1;
            r_y = filt_ecg_buf(r_x);
            set(h_rpeak_dots, 'XData', r_x, 'YData', r_y);
        else
            set(h_rpeak_dots, 'XData', NaN, 'YData', NaN);
        end
    else
        set(h_rpeak_dots, 'XData', NaN, 'YData', NaN);
    end

    set(h_mwi, 'YData', mwi_buf);
    set(h_thresh, 'YData', [peak_threshold peak_threshold]);
    set(h_smv, 'YData', smv_buf);
    set(h_bpm, 'YData', bpm_buf);
    set(h_temp, 'YData', temp_buf);

    % Tự động thích nghi thang đo trục tung (Y-axis Auto-scaling)
    max_amp = max(abs([filt_ecg_buf, raw_disp]));
    if max_amp > 10
        ylim(ax1, [-max_amp * 1.25, max_amp * 1.25]);
    end
    
    max_mwi_val = max(mwi_buf);
    if max_mwi_val > 500
        ylim(ax2, [0, max_mwi_val * 1.3]);
    end

    drawnow limitrate;
end

%% 5. ĐÓNG CỔNG NỐI TIẾP VÀ LƯU FILE .MAT
clear s;
savePatientRecord(all_time, all_raw_ecg, all_filt_ecg, all_mwi, all_r_peaks, ...
                 all_smv, fall_timestamps, all_temp, all_bpm, Fs);

function onFigureClose(src, ~)
    setappdata(src, 'is_running', false);
    delete(src);
end

function savePatientRecord(t, raw, filt, mwi, r_peaks, smv, falls, temp, bpm, Fs)
    if isempty(raw)
        disp('[INFO] Không có dữ liệu để lưu trữ.');
        return;
    end

    output_filename = 'ECG_Patient_Record.mat';
    valid_bpm = bpm(bpm > 40 & bpm < 180);
    mean_bpm = mean(valid_bpm);
    min_bpm = min(valid_bpm);
    max_bpm = max(valid_bpm);

    rmssd_ms = 0;
    if size(r_peaks, 1) > 2
        rr_diff = diff(diff(r_peaks(:, 3)) * 1000);
        rmssd_ms = sqrt(mean(rr_diff .^ 2));
    end

    PatientRecord.Metadata.PatientID = 'BN-01-EXP';
    PatientRecord.Metadata.RecordingDate = datestr(now, 'yyyy-mm-dd HH:MM:SS');
    PatientRecord.Metadata.SamplingRate_Hz = Fs;
    PatientRecord.Metadata.TotalDuration_Sec = t(end) - t(1);
    PatientRecord.Metadata.TotalSamples = length(raw);
    
    PatientRecord.Signals.Time_sec = t;
    PatientRecord.Signals.Raw_ECG_ADC = raw;
    PatientRecord.Signals.Filtered_ECG = filt;
    PatientRecord.Signals.PanTompkins_MWI = mwi;
    PatientRecord.Signals.SMV_Acceleration_g = smv;
    PatientRecord.Signals.HeartRate_BPM = bpm;
    PatientRecord.Signals.BodyTemp_C = temp;

    PatientRecord.Events.R_Peaks_Indices = r_peaks(:, 1);
    PatientRecord.Events.R_Peaks_Amplitudes = r_peaks(:, 2);
    PatientRecord.Events.R_Peaks_Time_sec = r_peaks(:, 3);
    PatientRecord.Events.Fall_Timestamps_sec = falls;
    PatientRecord.Events.Total_Falls_Detected = length(falls);

    PatientRecord.ClinicalSummary.Mean_BPM = mean_bpm;
    PatientRecord.ClinicalSummary.Min_BPM = min_bpm;
    PatientRecord.ClinicalSummary.Max_BPM = max_bpm;
    PatientRecord.ClinicalSummary.HRV_RMSSD_ms = rmssd_ms;
    PatientRecord.ClinicalSummary.Mean_BodyTemp = mean(temp(temp > 20));

    save(output_filename, 'PatientRecord');

    fprintf('\n[THÀNH CÔNG] Dữ liệu thực nghiệm lưu tại: %s\n', fullfile(pwd, output_filename));
    fprintf('  + Thời lượng ghi: %.2f s | Tổng mẫu: %d | Số đỉnh R: %d\n', ...
            PatientRecord.Metadata.TotalDuration_Sec, length(raw), size(r_peaks, 1));
    fprintf('  + Nhịp tim trung bình: %.1f BPM | HRV RMSSD: %.2f ms | Số lần ngã: %d\n', ...
            mean_bpm, rmssd_ms, length(falls));
end
