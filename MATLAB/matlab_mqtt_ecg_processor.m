%% =========================================================================
% CHƯƠNG TRÌNH XỬ LÝ TÍN HIỆU ECG THỜI GIAN THỰC QUA MQTT (PAN-TOMPKINS)
% DỰ ÁN: HỆ THỐNG IOMT THEO DÕI BỆNH NHÂN & PHÁT HIỆN TÉ NGÃ
% MÔN HỌC: XỬ LÝ TÍN HIỆU Y SINH (BDSP) & THIẾT KẾ HỆ THỐNG IOT
% =========================================================================
% Tần số lấy mẫu: Fs = 250 Hz (Khoảng lấy mẫu Ts = 4 ms)
% Giao thức: MQTT Broker (Ubuntu Server / 1Panel) -> JSON Payload
% Đầu ra: Đồ thị thời gian thực & File dữ liệu 'ECG_Patient_Record.mat'
% =========================================================================

clear; clc; close all;

%% 1. CẤU HÌNH THÔNG SỐ MQTT & MẠNG
% Tự động nạp cấu hình bảo mật (Token Telegram & IP Broker)
if exist('config_private.m', 'file') == 2
    run('config_private.m');
elseif exist('config_example.m', 'file') == 2
    run('config_example.m');
end

% THAY ĐỔI ĐỊA CHỈ IP HOẶC TÊN MIỀN CỦA UBUNTU SERVER 1PANEL TẠI ĐÂY:
if exist('mqtt_broker_ip', 'var') && ~isempty(mqtt_broker_ip)
    brokerAddress = "tcp://" + string(mqtt_broker_ip);
else
    brokerAddress = "tcp://192.168.1.36"; % IP Ubuntu Server 1Panel
end

if exist('mqtt_broker_port', 'var') && ~isempty(mqtt_broker_port)
    brokerPort = mqtt_broker_port;
else
    brokerPort = 1883;
end

clientTopic   = "biomed/patient/data";
clientID      = "MATLAB_Biomed_Client_" + string(randi(10000));
last_telegram_fall_time = -100;

% Thông số y sinh
Fs = 250;               % Tần số lấy mẫu (Hz)
dt = 1 / Fs;            % Chu kỳ lấy mẫu (0.004s)
BUFFER_LEN = 1000;      % Hiển thị 4 giây gần nhất trên đồ thị (1000 mẫu @ 250Hz)
FALL_THRESHOLD = 2.5;   % Ngưỡng phát hiện gia tốc té ngã (g)

%% 2. KHỞI TẠO BỘ ĐỆM DỮ LIỆU THỜI GIAN THỰC
time_buf       = zeros(1, BUFFER_LEN);
raw_ecg_buf    = zeros(1, BUFFER_LEN);
filt_ecg_buf   = zeros(1, BUFFER_LEN);
mwi_buf        = zeros(1, BUFFER_LEN);
smv_buf        = ones(1, BUFFER_LEN);
bpm_buf        = zeros(1, BUFFER_LEN);
temp_buf       = zeros(1, BUFFER_LEN);

% Bộ nhớ lưu trữ toàn bộ phiên ghi để xuất file .MAT
all_time       = [];
all_raw_ecg    = [];
all_filt_ecg   = [];
all_mwi        = [];
all_smv        = [];
all_bpm        = [];
all_temp       = [];
all_r_peaks    = []; % [Sample_Index, Amplitude, Time]
fall_timestamps = [];

%% 3. THIẾT KẾ CÁC BỘ LỌC PAN-TOMPKINS THEO CHUẨN Y SINH
% 3.1. Bộ lọc thông dải Bandpass (5 - 15 Hz) loại bỏ trôi đẳng điện & nhiễu cơ
[b_bp, a_bp] = butter(2, [5 15] / (Fs / 2), 'bandpass');
z_bp = zeros(max(length(a_bp), length(b_bp)) - 1, 1); % Lưu trạng thái bộ lọc liên tục giữa các gói tin

% 3.2. Cửa sổ tích phân trượt Moving Window Integration (150 ms = 38 mẫu @ 250Hz)
window_size = round(0.150 * Fs); 
kernel_mwi = ones(1, window_size) / window_size;

%% 4. THIẾT LẬP GIAO DIỆN ĐỒ THỊ REAL-TIME (4 CỬA SỔ Y SINH)
fig = figure('Name', 'HỆ THỐNG GIÁM SÁT ĐIỆN TIM ECG & TÉ NGÃ - ALGORITHM PAN-TOMPKINS', ...
             'Color', [0.12 0.14 0.18], 'Position', [100, 80, 1100, 750], ...
             'CloseRequestFcn', @onFigureClose);

% Nút Dừng & Lưu dữ liệu
btnStop = uicontrol('Style', 'pushbutton', 'String', 'DỪNG & XUẤT FILE .MAT', ...
                    'Units', 'normalized', 'Position', [0.4, 0.015, 0.2, 0.04], ...
                    'BackgroundColor', [0.85 0.25 0.2], 'ForegroundColor', 'w', ...
                    'FontWeight', 'bold', 'FontSize', 10, ...
                    'Callback', @(src, event) setappdata(fig, 'is_running', false));

setappdata(fig, 'is_running', true);

% Đồ thị 1: Tín hiệu ECG thô và ECG lọc dải thông
ax1 = subplot(4, 1, 1);
h_raw = plot(ax1, 1:BUFFER_LEN, raw_ecg_buf, 'Color', [0.5 0.5 0.5], 'LineWidth', 0.8);
hold(ax1, 'on');
h_filt = plot(ax1, 1:BUFFER_LEN, filt_ecg_buf, 'Color', [0.1 0.85 0.3], 'LineWidth', 1.3);
h_rpeak_dots = plot(ax1, NaN, NaN, 'ro', 'MarkerFaceColor', [1 0.2 0.2], 'MarkerEdgeColor', 'w', 'LineWidth', 1.2, 'MarkerSize', 7);
grid(ax1, 'on');
set(ax1, 'Color', [0.08 0.09 0.12], 'XColor', [0.8 0.8 0.8], 'YColor', [0.8 0.8 0.8]);
title(ax1, '1. Tín hiệu điện tim ECG (Xám: Raw ADC | Xanh lá: Bandpass | Chấm đỏ: Đỉnh R)', 'Color', 'w', 'FontWeight', 'bold');
ylabel(ax1, 'Biên độ ADC');
legend(ax1, {'Raw ECG', 'Filtered ECG', 'Đỉnh R'}, 'TextColor', 'w', 'Location', 'northeast');

% Đồ thị 2: Năng lượng QRS (Pan-Tompkins MWI) & Đỉnh R-Peak
ax2 = subplot(4, 1, 2);
h_mwi = plot(ax2, 1:BUFFER_LEN, mwi_buf, 'Color', [0.2 0.7 1.0], 'LineWidth', 1.2);
hold(ax2, 'on');
h_thresh = plot(ax2, [1 BUFFER_LEN], [0 0], 'r--', 'LineWidth', 1.0);
h_rpeaks = plot(ax2, nan, nan, 'ro', 'MarkerFaceColor', 'r', 'MarkerSize', 6);
grid(ax2, 'on');
set(ax2, 'Color', [0.08 0.09 0.12], 'XColor', [0.8 0.8 0.8], 'YColor', [0.8 0.8 0.8]);
title(ax2, '2. Tích phân năng lượng Pan-Tompkins & Bóc tách đỉnh R (R-Peaks)', 'Color', 'w', 'FontWeight', 'bold');
ylabel(ax2, 'Năng lượng');

% Đồ thị 3: Nhịp tim BPM & Thân nhiệt
ax3 = subplot(4, 1, 3);
yyaxis(ax3, 'left');
h_bpm = plot(ax3, 1:BUFFER_LEN, bpm_buf, 'Color', [1.0 0.4 0.2], 'LineWidth', 1.5);
ylabel(ax3, 'Nhịp tim (BPM)', 'Color', [1.0 0.4 0.2]);
ylim(ax3, [40 160]);
set(ax3, 'YColor', [1.0 0.4 0.2]);

yyaxis(ax3, 'right');
h_temp = plot(ax3, 1:BUFFER_LEN, temp_buf, 'Color', [0.9 0.8 0.2], 'LineWidth', 1.3);
ylabel(ax3, 'Nhiệt độ (°C)', 'Color', [0.9 0.8 0.2]);
ylim(ax3, [34 42]);
set(ax3, 'YColor', [0.9 0.8 0.2]);

grid(ax3, 'on');
set(ax3, 'Color', [0.08 0.09 0.12], 'XColor', [0.8 0.8 0.8]);
title(ax3, '3. Xu hướng Nhịp tim (BPM - Cam) & Thân nhiệt (°C - Vàng)', 'Color', 'w', 'FontWeight', 'bold');

% Đồ thị 4: Gia tốc SMV & Ngưỡng ngã
ax4 = subplot(4, 1, 4);
h_smv = plot(ax4, 1:BUFFER_LEN, smv_buf, 'Color', [0.9 0.3 0.9], 'LineWidth', 1.3);
hold(ax4, 'on');
yline(ax4, FALL_THRESHOLD, 'r--', 'NGƯỠNG TÉ NGÃ (2.5g)', 'Color', [1 0.2 0.2], 'LineWidth', 1.2);
grid(ax4, 'on');
set(ax4, 'Color', [0.08 0.09 0.12], 'XColor', [0.8 0.8 0.8], 'YColor', [0.8 0.8 0.8]);
title(ax4, '4. Vector tổng gia tốc SMV (Signal Magnitude Vector từ MPU-6050)', 'Color', 'w', 'FontWeight', 'bold');
ylabel(ax4, 'Gia tốc (g)');
ylim(ax4, [0 4.0]);

%% 5. KẾT NỐI MQTT BROKER
disp('================================================================');
disp('   HỆ THỐNG XỬ LÝ TÍN HIỆU Y SINH MATLAB ĐANG KHỞI CHẠY');
disp('   Kết nối MQTT Broker: ' + brokerAddress + ':' + string(brokerPort));
disp('   Đang đăng ký lắng nghe Topic: ' + clientTopic);
disp('================================================================');

try
    mqClient = mqttclient(brokerAddress, 'Port', brokerPort, 'ClientID', clientID);
    subscribe(mqClient, clientTopic);
    disp('[OK] Đã kết nối MQTT Broker thành công! Đang đợi dữ liệu từ Gateway...');
catch ME
    warning('Không thể kết nối trực tiếp MQTT: %s', ME.message);
    disp('-> HƯỚNG DẪN: Nếu chạy offline tại bàn hoặc demo kiểm thử,');
    disp('   bạn có thể sử dụng script "matlab_serial_ecg_processor.m" qua cổng COM USB!');
    return;
end

%% 6. VÒNG LẶP TIẾP NHẬN DỮ LIỆU & BÓC TÁCH ĐỈNH R THỜI GIAN THỰC
total_samples_received = 0;
peak_threshold = 50000; % Ngưỡng động khởi tạo
spki = 100000;
npki = 10000;
last_r_sample_idx = 0;
current_time_sec = 0;

while isvalid(fig) && getappdata(fig, 'is_running')
    % Kiểm tra tin nhắn mới trong hàng đợi MQTT
    msgTable = read(mqClient);
    if isempty(msgTable)
        pause(0.02);
        continue;
    end

    % Xử lý từng gói tin nhận được
    for m = 1:height(msgTable)
        payloadStr = char(msgTable.Data(m));
        
        % Giải mã chuỗi JSON
        try
            pkg = jsondecode(payloadStr);
        catch
            continue;
        end
        
        if ~isfield(pkg, 'ecg') || isempty(pkg.ecg)
            continue;
        end

        new_ecg   = double(pkg.ecg(:))';
        new_temp  = double(pkg.temp);
        new_smv   = double(pkg.smv);
        new_leads = double(pkg.leadsOff);
        new_fall  = double(pkg.fall);
        N_new     = length(new_ecg);

        % Tạo trục thời gian cho gói mẫu mới
        time_new = current_time_sec + (1:N_new) * dt;
        current_time_sec = time_new(end);

        % Ghi nhận cảnh báo ngã & gửi tin nhắn Telegram khẩn cấp
        if (new_fall == 1 || new_smv >= FALL_THRESHOLD) && (current_time_sec - last_telegram_fall_time > 8)
            last_telegram_fall_time = current_time_sec;
            fall_timestamps(end+1) = current_time_sec; %#ok<AGROW>
            fprintf('[CẢNH BÁO TÉ NGÃ] Thời điểm: %.2fs | SMV = %.2fg\n', current_time_sec, new_smv);
            
            % Tự động gửi tin nhắn Telegram khẩn cấp tới điện thoại qua bot
            if exist('telegram_bot_token', 'var') && ~contains(telegram_bot_token, 'YOUR_')
                try
                    msg_text = sprintf(['🚨 [CẢNH BÁO TÉ NGÃ - IoMT MQTT CLOUD]\n' ...
                                        'Thời điểm: %.1fs\n' ...
                                        'Lực va đập SMV: %.2fg\n' ...
                                        'Nhịp tim: %.0f BPM\n' ...
                                        'Thân nhiệt: %.1f°C\n' ...
                                        'Kênh truyền: MQTT (192.168.1.36)\n' ...
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

        % ==================== THUẬT TOÁN PAN-TOMPKINS ====================
        % Bước 1: Lọc Bandpass (Loại bỏ Baseline Wander 0.5-5Hz & Nhiễu cao tần)
        [new_filt, z_bp] = filter(b_bp, a_bp, new_ecg, z_bp);

        % Bước 2: Bộ vi phân 5 điểm (Derivative filter)
        new_deriv = zeros(1, N_new);
        for n = 5:N_new
            new_deriv(n) = (2*new_filt(n) + new_filt(n-1) - new_filt(n-3) - 2*new_filt(n-4)) * (Fs / 8);
        end

        % Bước 3: Hàm bình phương phi tuyến (Squaring Function)
        new_sq = new_deriv .^ 2;

        % Bước 4: Tích phân cửa sổ trượt (Moving Window Integration)
        new_mwi = conv(new_sq, kernel_mwi, 'same');

        % Bước 5: Bóc tách đỉnh R thích nghi ngưỡng (Thời gian trơ 380ms loại bỏ sóng T)
        current_r_peaks = [];
        for i = 1:N_new
            global_sample_idx = total_samples_received + i;
            
            % Thời gian trơ sinh lý: 380ms (round(0.38 * Fs) = 95 mẫu)
            if (new_mwi(i) > peak_threshold) && ...
               (global_sample_idx - last_r_sample_idx > round(0.38 * Fs))
                
                search_start = max(1, i - 12);
                search_end   = min(N_new, i + 12);
                [r_amp, local_peak_offset] = max(new_filt(search_start:search_end));
                r_idx = search_start + local_peak_offset - 1;

                last_r_sample_idx = global_sample_idx;
                all_r_peaks = [all_r_peaks; [global_sample_idx, r_amp, time_new(r_idx)]]; %#ok<AGROW>
                current_r_peaks = [current_r_peaks, r_idx]; %#ok<AGROW>

                spki = 0.125 * new_mwi(i) + 0.875 * spki;
                peak_threshold = npki + 0.25 * (spki - npki);
            else
                npki = 0.125 * new_mwi(i) + 0.875 * npki;
                peak_threshold = npki + 0.25 * (spki - npki);
            end
        end

        % Tính toán nhịp tim BPM từ các đỉnh R gần nhất với bộ lọc trung vị (Median Filter) chống nhiễu
        current_bpm = 0;
        if size(all_r_peaks, 1) >= 3
            recent_rr = diff(all_r_peaks(max(1, end-6):end, 3));
            valid_rr = recent_rr(recent_rr >= 0.40 & recent_rr <= 1.40);
            if ~isempty(valid_rr)
                current_bpm = 60 / median(valid_rr);
            end
        end

        % Lưu trữ dữ liệu lịch sử phục vụ xuất file .MAT
        all_time     = [all_time, time_new]; %#ok<AGROW>
        all_raw_ecg  = [all_raw_ecg, new_ecg]; %#ok<AGROW>
        all_filt_ecg = [all_filt_ecg, new_filt]; %#ok<AGROW>
        all_mwi      = [all_mwi, new_mwi]; %#ok<AGROW>
        all_smv      = [all_smv, repmat(new_smv, 1, N_new)]; %#ok<AGROW>
        all_bpm      = [all_bpm, repmat(current_bpm, 1, N_new)]; %#ok<AGROW>
        all_temp     = [all_temp, repmat(new_temp, 1, N_new)]; %#ok<AGROW>
        total_samples_received = total_samples_received + N_new;

        % Cập nhật bộ đệm hiển thị đồ thị cuộn
        raw_ecg_buf  = [raw_ecg_buf(N_new+1:end), new_ecg];
        filt_ecg_buf = [filt_ecg_buf(N_new+1:end), new_filt];
        mwi_buf      = [mwi_buf(N_new+1:end), new_mwi];
        smv_buf      = [smv_buf(N_new+1:end), repmat(new_smv, 1, N_new)];
        bpm_buf      = [bpm_buf(N_new+1:end), repmat(current_bpm, 1, N_new)];
        temp_buf     = [temp_buf(N_new+1:end), repmat(new_temp, 1, N_new)];
    end

    % ==================== VẼ ĐỒ THỊ THỜI GIAN THỰC ====================
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

    % Tự động điều chỉnh thang đo trục tung (Y-axis Auto-scaling)
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

%% 7. LƯU TRỮ DỮ LIỆU THỰC NGHIỆM VÀO FILE .MAT
savePatientRecord(all_time, all_raw_ecg, all_filt_ecg, all_mwi, all_r_peaks, ...
                 all_smv, fall_timestamps, all_temp, all_bpm, Fs);

%% ==================== CÁC HÀM BỔ TRỢ ====================
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
    disp(' ');
    disp('================================================================');
    disp('        ĐANG XUẤT DỮ LIỆU THỰC NGHIỆM RA FILE .MAT');
    disp('================================================================');

    % Tính toán các chỉ số thống kê Y sinh học cho báo cáo BDSP
    valid_bpm = bpm(bpm > 40 & bpm < 180);
    mean_bpm = mean(valid_bpm);
    min_bpm = min(valid_bpm);
    max_bpm = max(valid_bpm);

    % Tính độ biến thiên nhịp tim HRV (RMSSD - Root Mean Square of Successive Differences)
    rmssd_ms = 0;
    if size(r_peaks, 1) > 2
        rr_diff = diff(diff(r_peaks(:, 3)) * 1000); % Đơn vị: miligiây
        rmssd_ms = sqrt(mean(rr_diff .^ 2));
    end

    % Đóng gói cấu trúc bản ghi bệnh nhân
    PatientRecord.Metadata.PatientID = 'BN-01-EXP';
    PatientRecord.Metadata.RecordingDate = datestr(now, 'yyyy-mm-dd HH:MM:SS');
    PatientRecord.Metadata.SamplingRate_Hz = Fs;
    PatientRecord.Metadata.TotalDuration_Sec = t(end) - t(1);
    PatientRecord.Metadata.TotalSamples = length(raw);
    
    % Chuỗi thời gian
    PatientRecord.Signals.Time_sec = t;
    PatientRecord.Signals.Raw_ECG_ADC = raw;
    PatientRecord.Signals.Filtered_ECG = filt;
    PatientRecord.Signals.PanTompkins_MWI = mwi;
    PatientRecord.Signals.SMV_Acceleration_g = smv;
    PatientRecord.Signals.HeartRate_BPM = bpm;
    PatientRecord.Signals.BodyTemp_C = temp;

    % Biến cố bóc tách được
    PatientRecord.Events.R_Peaks_Indices = r_peaks(:, 1);
    PatientRecord.Events.R_Peaks_Amplitudes = r_peaks(:, 2);
    PatientRecord.Events.R_Peaks_Time_sec = r_peaks(:, 3);
    PatientRecord.Events.Fall_Timestamps_sec = falls;
    PatientRecord.Events.Total_Falls_Detected = length(falls);

    % Tóm tắt chỉ số sinh lý
    PatientRecord.ClinicalSummary.Mean_BPM = mean_bpm;
    PatientRecord.ClinicalSummary.Min_BPM = min_bpm;
    PatientRecord.ClinicalSummary.Max_BPM = max_bpm;
    PatientRecord.ClinicalSummary.HRV_RMSSD_ms = rmssd_ms;
    PatientRecord.ClinicalSummary.Mean_BodyTemp = mean(temp(temp > 20));

    % Lưu ra file .mat tiêu chuẩn
    save(output_filename, 'PatientRecord');

    fprintf('[THÀNH CÔNG] Dữ liệu đã được lưu vào file: %s\n', fullfile(pwd, output_filename));
    fprintf('----------------- TÓM TẮT DỮ LIỆU BÁO CÁO BDSP -----------------\n');
    fprintf('  + Thời lượng ghi thực nghiệm  : %.2f giây (%d mẫu)\n', PatientRecord.Metadata.TotalDuration_Sec, length(raw));
    fprintf('  + Tần số lấy mẫu Fs           : %d Hz\n', Fs);
    fprintf('  + Tổng số đỉnh R bóc tách     : %d đỉnh\n', size(r_peaks, 1));
    fprintf('  + Nhịp tim trung bình         : %.1f BPM (Min: %.1f | Max: %.1f)\n', mean_bpm, min_bpm, max_bpm);
    fprintf('  + Độ biến thiên nhịp tim HRV  : %.2f ms (RMSSD)\n', rmssd_ms);
    fprintf('  + Số lần phát hiện té ngã     : %d lần\n', length(falls));
    fprintf('  + Thân nhiệt trung bình       : %.2f *C\n', PatientRecord.ClinicalSummary.Mean_BodyTemp);
    disp('================================================================');
    disp('File này sẵn sàng làm minh chứng đưa vào Báo cáo 25-30 trang!');
end
