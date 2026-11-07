/**
 * IoMT Patient Telehealth Monitor - Realtime Controller & Oscilloscope Engine
 * Connects to MQTT Broker over WebSocket Secure (WSS), draws 60 FPS ECG,
 * synthesizes audio alarms via Web Audio API, and maintains clinical audit log.
 */

// ==================== CẤU HÌNH MẶC ĐỊNH ====================
const DEFAULT_CONFIG = {
  uri: 'wss://mqtt.tsbyin.dev/mqtt', // Mặc định kết nối Cloudflare Tunnel WSS
  user: 'TsByin',
  pass: 'Chei@@@182728',
  topicData: 'biomed/patient/data',
  topicAlert: 'biomed/patient/alert',
  topicCmd: 'biomed/gateway/cmd',     // Topic điều khiển Gateway (Tắt còi từ xa)
  influxUrl: 'https://influx.tsbyin.dev', // Máy chủ InfluxDB v2 REST API
  influxOrg: 'influxdb_FSiPrd',
  influxBucket: 'biomed_events',
  influxToken: 'qC_AWPMQx2Tz5sjREW_xBPIr372L1jpujftPLCbUjHDQHG9PQfIU-d41RLx8hhB7hDlK1O0KueWG7-qMDhv6rw=='
};

// ==================== TRẠNG THÁI HỆ THỐNG ====================
let config = loadConfig();
let mqttClient = null;
let isDemoMode = false;
let isReplayMode = false;
let audioEnabled = false;
let isBuzzerMuted = false;
let currentSirenOsc = null;
let currentSirenGain = null;
let totalPackets = 0;
let totalFalls = 0;
let prevFallState = 0;
let lastHeartBeatTime = 0;

// Trạng thái phát lại dữ liệu lịch sử InfluxDB (Replay Engine)
let replayTimer = null;
let replayPaused = false;
let replayIndex = 0;
let replayDataset = null;
let selectedEventId = 'preset_fall';
let loadedInfluxEvents = [];

// ==================== BỘ LỌC TÍN HIỆU ECG CHUẨN PAN-TOMPKINS ====================
/** 
 * 1. Bước 1 - Bộ lọc thông dải Butterworth 5 - 15 Hz bậc 2 [butter(2, [5 15]/(Fs/2), 'bandpass')]
 *    thực thi qua cấu trúc Direct Form II Transposed bảo toàn trạng thái trễ liên tục z_bp.
 * 2. Bước 2 - Bộ vi phân 5 điểm Pan-Tompkins: (2*x[n] + x[n-1] - x[n-3] - 2*x[n-4]) * (Fs/8)
 * 3. Bước 3 - Hàm bình phương phi tuyến: y[n] = (x[n])^2
 * 4. Bước 4 - Tích phân cửa sổ trượt (MWI): Cửa sổ 150ms = 38 mẫu @ 250Hz
 * 5. Bước 5 - Bóc tách đỉnh R thích nghi ngưỡng (Dual Adaptive SPKI & NPKI Thresholds)
 *    kèm thời gian trơ sinh lý 380ms (95 mẫu) loại bỏ sóng T và Median Filter cho nhịp tim.
 */
/**
 * Bộ xử lý tín hiệu điện tâm đồ y sinh (Medical ECG Processor & Dual Filter Pipeline)
 * 1. Nhánh Hiển thị dạng sóng lâm sàng (Clinical Waveform Display Filter):
 *    - Biquad Notch Filter 50Hz (Q = 6.0 @ 250Hz): Triệt tiêu hoàn toàn sóng hài xoay chiều 50Hz từ lưới điện.
 *    - Highpass Filter 0.5Hz: Khử hoàn toàn hiện tượng trôi đường đẳng điện DC do nhịp thở.
 *    - Butterworth Lowpass 35Hz (Bậc 2): Cắt gọt triệt để nhiễu rung cơ (EMG tremor) và gai tĩnh điện ngoại lai.
 *    -> Giữ trọn vẹn giải phẫu sóng P - phức bộ QRS - sóng T, không bị hiện tượng cộng hưởng dao động giả 10Hz.
 * 2. Nhánh Phân tích bóc tách đỉnh R (QRS Detection & BPM Engine):
 *    - Giải thuật Pan-Tompkins 1985 chuẩn y sinh (Bandpass 5-15Hz, Vi phân 5 điểm, Bình phương, MWI 150ms).
 *    - Ngưỡng kép thích ứng động SPKI & NPKI kết hợp cửa sổ trơ sinh lý 380ms (95 mẫu @ 250Hz).
 *    - Bộ lọc trung vị (Median Filter 7 nhịp) và làm mịn EMA loại bỏ ngoại tâm thu và gai cử động.
 */
class MedicalEcgProcessor {
  constructor(fs = 250) {
    this.fs = fs;
    this.filterMode = 'pan-tompkins'; // 'pan-tompkins' (5-15Hz) hoặc 'clinical' (0.5-25Hz)

    // 1. Biquad 50Hz Notch Filter (Q = 6.0 @ 250Hz) - Triệt tiêu tuyệt đối nhiễu điện lưới 50Hz
    const w0 = (2.0 * Math.PI * 50.0) / fs;
    const c = Math.cos(w0);
    const s = Math.sin(w0);
    const alpha = s / (2.0 * 6.0);
    const a0 = 1.0 + alpha;
    this.nb0 = 1.0 / a0;
    this.nb1 = (-2.0 * c) / a0;
    this.nb2 = 1.0 / a0;
    this.na1 = (-2.0 * c) / a0;
    this.na2 = (1.0 - alpha) / a0;
    this.nx1 = 0; this.nx2 = 0;
    this.ny1 = 0; this.ny2 = 0;

    // 2. Pan-Tompkins Butterworth Bandpass 5-15Hz (Cascaded 2-Section SOS @ 250Hz)
    // Section 0: Lowpass stage
    this.bp_b0 = [0.0133592000, 0.0267184001, 0.0133592000];
    this.bp_a0 = [1.0, -1.6813538570, 0.7798921440];
    this.w0_0 = 0.0; this.w0_1 = 0.0;
    // Section 1: Highpass stage (double zero at DC z=1 cancels baseline wander completely)
    this.bp_b1 = [1.0, -2.0, 1.0];
    this.bp_a1 = [1.0, -1.8795935959, 0.8987098878];
    this.w1_0 = 0.0; this.w1_1 = 0.0;

    // 3. Clinical Lowpass 25Hz (Butterworth bậc 2 @ 250Hz)
    this.b_clin = [0.06745527, 0.13491055, 0.06745527];
    this.a_clin = [1.0, -1.1429805, 0.4128016];
    this.wc0 = 0.0; this.wc1 = 0.0;

    // 4. Bộ làm mịn 3 điểm (Weighted 3-point filter: 0.25, 0.5, 0.25)
    this.s_prev1 = 0.0; this.s_prev2 = 0.0;
    this.sc_prev1 = 0.0; this.sc_prev2 = 0.0;

    // 5. Ước lượng đường đẳng điện DC
    this.baseline = 2048.0;

    // 6. Pan-Tompkins 1985 QRS Detection
    this.d_buf = [0, 0, 0, 0, 0];
    this.windowSize = Math.round(0.150 * fs); // 38 mẫu (150ms)
    this.mwiBuf = new Float32Array(this.windowSize).fill(0);
    this.mwiIdx = 0;
    this.mwiSum = 0;
    this.spki = 100.0;
    this.npki = 5.0;
    this.peakThreshold = 25.0;
    this.lastRSampleIdx = 0;
    this.sampleCount = 0;
    this.refractorySamples = Math.round(0.380 * fs); // 95 mẫu = 380ms
    this.recentRR = [];
    this.lastRRMs = 0;
    this.lastRPeakMv = '0.00';
    this.stableBpm = 0;
    this.leadsOffCount = 0;
  }

  reset() {
    this.nx1 = 0; this.nx2 = 0; this.ny1 = 0; this.ny2 = 0;
    this.w0_0 = 0.0; this.w0_1 = 0.0;
    this.w1_0 = 0.0; this.w1_1 = 0.0;
    this.wc0 = 0.0; this.wc1 = 0.0;
    this.s_prev1 = 0.0; this.s_prev2 = 0.0;
    this.sc_prev1 = 0.0; this.sc_prev2 = 0.0;
    this.baseline = 2048.0;
    this.d_buf.fill(0);
    this.mwiBuf.fill(0);
    this.mwiSum = 0;
    this.spki = 100.0;
    this.npki = 5.0;
    this.peakThreshold = 25.0;
    this.lastRSampleIdx = 0;
    this.recentRR = [];
    this.stableBpm = 0;
  }

  process(rawSample, isLeadsOff) {
    this.sampleCount++;

    if (isLeadsOff || rawSample < 50 || rawSample >= 4080) {
      this.leadsOffCount++;
      if (this.leadsOffCount > 5) {
        this.reset();
      }
      return { filtered: 0, beat: false, rrMs: 0, rMv: '0.00' };
    } else {
      this.leadsOffCount = 0;
    }

    const raw = Math.max(80, Math.min(4015, rawSample));

    // Khử thành phần DC danh định trước khi vào chuỗi lọc số
    this.baseline += (raw - this.baseline) * 0.005;
    const x = raw - this.baseline;

    // 1. Notch Filter 50Hz Biquad (Q = 6.0) - Triệt tiêu tuyệt đối 50Hz
    const notch = this.nb0 * x + this.nb1 * this.nx1 + this.nb2 * this.nx2 - this.na1 * this.ny1 - this.na2 * this.ny2;
    this.nx2 = this.nx1; this.nx1 = x;
    this.ny2 = this.ny1; this.ny1 = notch;

    // 2. Pan-Tompkins Butterworth Bandpass 5-15Hz (Cascaded SOS Direct Form II Transposed)
    // Section 0
    const y0 = this.bp_b0[0] * notch + this.w0_0;
    this.w0_0 = this.bp_b0[1] * notch - this.bp_a0[1] * y0 + this.w0_1;
    this.w0_1 = this.bp_b0[2] * notch - this.bp_a0[2] * y0;

    // Section 1
    const y1 = this.bp_b1[0] * y0 + this.w1_0;
    this.w1_0 = this.bp_b1[1] * y0 - this.bp_a1[1] * y1 + this.w1_1;
    this.w1_1 = this.bp_b1[2] * y0 - this.bp_a1[2] * y1;

    // Làm mịn 3 điểm cho tín hiệu Bandpass Pan-Tompkins
    const bp_smooth = 0.25 * y1 + 0.5 * this.s_prev1 + 0.25 * this.s_prev2;
    this.s_prev2 = this.s_prev1;
    this.s_prev1 = y1;

    // 3. Clinical Filter (Butterworth Lowpass 25Hz)
    const y_clin = this.b_clin[0] * notch + this.wc0;
    this.wc0 = this.b_clin[1] * notch - this.a_clin[1] * y_clin + this.wc1;
    this.wc1 = this.b_clin[2] * notch - this.a_clin[2] * y_clin;

    const clin_smooth = 0.25 * y_clin + 0.5 * this.sc_prev1 + 0.25 * this.sc_prev2;
    this.sc_prev2 = this.sc_prev1;
    this.sc_prev1 = y_clin;

    // Tín hiệu hiển thị ra màn hình tùy chế độ chọn lọc
    const displaySignal = (this.filterMode === 'clinical') ? clin_smooth : bp_smooth;

    // 4. Giải thuật Pan-Tompkins bóc tách đỉnh R QRS (LUÔN sử dụng tín hiệu Bandpass 5-15Hz siêu sạch)
    this.d_buf.shift();
    this.d_buf.push(bp_smooth / 25.0); // Chuẩn hóa biên độ để số mũ không bị tràn
    const deriv = (2 * this.d_buf[4] + this.d_buf[3] - this.d_buf[1] - 2 * this.d_buf[0]) * (this.fs / 8.0);
    const sq = deriv * deriv;

    this.mwiSum += sq - this.mwiBuf[this.mwiIdx];
    this.mwiBuf[this.mwiIdx] = sq;
    this.mwiIdx = (this.mwiIdx + 1) % this.windowSize;
    const mwi = this.mwiSum / this.windowSize;

    let beat = false;
    let rrMs = this.lastRRMs;
    let rMv = this.lastRPeakMv;

    if (mwi > this.peakThreshold && (this.sampleCount - this.lastRSampleIdx > this.refractorySamples) && this.sampleCount > this.fs) {
      const rrSamples = this.sampleCount - this.lastRSampleIdx;
      this.lastRSampleIdx = this.sampleCount;

      if (rrSamples > 0 && rrSamples < 500) {
        const measuredRRMs = Math.round(rrSamples * (1000 / this.fs));
        if (measuredRRMs >= 400 && measuredRRMs <= 1400) {
          if (this.recentRR.length >= 3) {
            const tempSorted = [...this.recentRR].sort((a, b) => a - b);
            const curMedian = tempSorted[Math.floor(tempSorted.length / 2)];
            if (measuredRRMs < 0.65 * curMedian || measuredRRMs > 1.45 * curMedian) {
              this.spki = 0.125 * mwi + 0.875 * this.spki;
              this.peakThreshold = this.npki + 0.25 * (this.spki - this.npki);
              return { filtered: displaySignal, beat: false, rrMs: this.lastRRMs, rMv: this.lastRPeakMv };
            }
          }

          this.recentRR.push(measuredRRMs);
          if (this.recentRR.length > 7) this.recentRR.shift();

          const sorted = [...this.recentRR].sort((a, b) => a - b);
          rrMs = sorted[Math.floor(sorted.length / 2)];
          this.lastRRMs = rrMs;

          const instantBpm = Math.round(60000 / rrMs);
          if (this.stableBpm === 0) {
            this.stableBpm = instantBpm;
          } else {
            this.stableBpm = Math.round(this.stableBpm * 0.82 + instantBpm * 0.18);
          }

          rMv = Math.max(0.4, Math.min(2.8, Math.abs(bp_smooth) * 0.004)).toFixed(2);
          this.lastRPeakMv = rMv;
          beat = true;
        }
      }

      this.spki = 0.125 * mwi + 0.875 * this.spki;
      this.peakThreshold = this.npki + 0.25 * (this.spki - this.npki);
    } else {
      this.npki = 0.125 * mwi + 0.875 * this.npki;
      this.peakThreshold = this.npki + 0.25 * (this.spki - this.npki);
    }

    return { filtered: displaySignal, beat: beat, rrMs: rrMs, rMv: rMv };
  }
}

// Bộ đệm sóng ECG (Oscilloscope Ring Buffer)
const BUFFER_SIZE = 1000; // 4 giây hiển thị ở tần số lấy mẫu 250Hz (250 * 4 = 1000)
const ecgBuffer = new Float32Array(BUFFER_SIZE).fill(0);
const ecgProcessor = new MedicalEcgProcessor(250);
let dynamicPeak = 250;
let writeIndex = 0;
let sweepIndex = 0;
let lastDetectedBpm = 0;
let lastRenderTime = 0;
let lastDataPacketTime = 0;

// Web Audio API Context
let audioCtx = null;

// ==================== KHỞI TẠO DOM ELEMENTS ====================
const elements = {
  statusBadge: document.getElementById('connectionStatus'),
  statusText: document.getElementById('statusText'),
  statusTextShort: document.getElementById('statusTextShort'),
  dbStatus: document.getElementById('dbStatus'),
  dbStatusText: document.getElementById('dbStatusText'),
  dbStatusTextShort: document.getElementById('dbStatusTextShort'),
  drawerMqttText: document.getElementById('drawerMqttText'),
  drawerDbText: document.getElementById('drawerDbText'),
  testDbBtn: document.getElementById('testDbBtn'),
  testDbResult: document.getElementById('testDbResult'),
  mobileMenuBtn: document.getElementById('mobileMenuBtn'),
  closeDrawerBtn: document.getElementById('closeDrawerBtn'),
  drawerBackdrop: document.getElementById('drawerBackdrop'),
  navActionGroup: document.getElementById('navActionGroup'),
  audioToggleBtn: document.getElementById('audioToggleBtn'),
  audioIcon: document.getElementById('audioIcon'),
  audioLabel: document.getElementById('audioLabel'),
  remoteMuteBtn: document.getElementById('remoteMuteBtn'),
  remoteMuteIcon: document.getElementById('remoteMuteIcon'),
  remoteMuteLabel: document.getElementById('remoteMuteLabel'),
  pwaInstallBtn: document.getElementById('pwaInstallBtn'),
  simToggleBtn: document.getElementById('simToggleBtn'),
  replayToggleBtn: document.getElementById('replayToggleBtn'),
  replayBtnIcon: document.getElementById('replayBtnIcon'),
  replayBtnLabel: document.getElementById('replayBtnLabel'),
  replayBanner: document.getElementById('replayBanner'),
  replayEventTitle: document.getElementById('replayEventTitle'),
  replayEventSub: document.getElementById('replayEventSub'),
  replayProgressBar: document.getElementById('replayProgressBar'),
  replayProgressFill: document.getElementById('replayProgressFill'),
  replayTimeDisplay: document.getElementById('replayTimeDisplay'),
  replayPlayPauseBtn: document.getElementById('replayPlayPauseBtn'),
  replayPlayIcon: document.getElementById('replayPlayIcon'),
  replayRestartBtn: document.getElementById('replayRestartBtn'),
  replayExitBtn: document.getElementById('replayExitBtn'),
  replayModal: document.getElementById('replayModal'),
  closeReplayModalBtn: document.getElementById('closeReplayModalBtn'),
  cancelReplayModalBtn: document.getElementById('cancelReplayModalBtn'),
  startReplayBtn: document.getElementById('startReplayBtn'),
  eventListContainer: document.getElementById('eventListContainer'),
  fetchInfluxEventsBtn: document.getElementById('fetchInfluxEventsBtn'),
  influxFetchStatus: document.getElementById('influxFetchStatus'),
  configModalBtn: document.getElementById('configModalBtn'),
  configModal: document.getElementById('configModal'),
  closeModalBtn: document.getElementById('closeModalBtn'),
  configForm: document.getElementById('configForm'),
  resetConfigBtn: document.getElementById('resetConfigBtn'),
  cfgUri: document.getElementById('cfgUri'),
  cfgUser: document.getElementById('cfgUser'),
  cfgPass: document.getElementById('cfgPass'),
  cfgTopicData: document.getElementById('cfgTopicData'),
  cfgTopicAlert: document.getElementById('cfgTopicAlert'),
  cfgTopicCmd: document.getElementById('cfgTopicCmd'),
  cfgInfluxUrl: document.getElementById('cfgInfluxUrl'),
  cfgInfluxOrg: document.getElementById('cfgInfluxOrg'),
  cfgInfluxBucket: document.getElementById('cfgInfluxBucket'),
  cfgInfluxToken: document.getElementById('cfgInfluxToken'),
  toastContainer: document.getElementById('toastContainer'),
  emergencyBanner: document.getElementById('emergencyBanner'),
  alertTitle: document.getElementById('alertTitle'),
  alertDesc: document.getElementById('alertDesc'),
  dismissAlertBtn: document.getElementById('dismissAlertBtn'),
  ecgCanvas: document.getElementById('ecgCanvas'),
  leadsOffBadge: document.getElementById('leadsOffBadge'),
  sweepTimeDisplay: document.getElementById('sweepTimeDisplay'),
  filterModePill: document.getElementById('filterModePill'),
  rPeakVal: document.getElementById('rPeakVal'),
  rrIntervalVal: document.getElementById('rrIntervalVal'),
  bpmVal: document.getElementById('bpmVal'),
  bpmStatus: document.getElementById('bpmStatus'),
  heartPulse: document.getElementById('heartPulse'),
  tempVal: document.getElementById('tempVal'),
  tempStatus: document.getElementById('tempStatus'),
  tempNeedle: document.getElementById('tempNeedle'),
  tempStatusPill: document.getElementById('tempStatusPill'),
  smvVal: document.getElementById('smvVal'),
  smvGauge: document.getElementById('smvGauge'),
  smvPill: document.getElementById('smvPill'),
  bpmPulseBar: document.getElementById('bpmPulseBar'),
  fallStatus: document.getElementById('fallStatus'),
  fallCountVal: document.getElementById('fallCountVal'),
  packetCountVal: document.getElementById('packetCountVal'),
  logTableBody: document.getElementById('logTableBody'),
  clearLogBtn: document.getElementById('clearLogBtn')
};

// ==================== CẤU HÌNH & LOCAL STORAGE ====================
function loadConfig() {
  const saved = localStorage.getItem('iomt_mqtt_cfg');
  if (saved) {
    try {
      const parsed = JSON.parse(saved);
      return {
        ...DEFAULT_CONFIG,
        ...parsed,
        influxToken: parsed.influxToken || DEFAULT_CONFIG.influxToken,
        influxOrg: parsed.influxOrg || DEFAULT_CONFIG.influxOrg
      };
    } catch (e) {
      console.error('Lỗi nạp config:', e);
    }
  }
  return { ...DEFAULT_CONFIG };
}

function saveConfig(newCfg) {
  config = { ...config, ...newCfg };
  localStorage.setItem('iomt_mqtt_cfg', JSON.stringify(config));
}

// ==================== WEB AUDIO API (SYNTHESIZER) ====================
function initAudio() {
  if (!audioCtx) {
    const AudioContext = window.AudioContext || window.webkitAudioContext;
    audioCtx = new AudioContext();
  }
  if (audioCtx.state === 'suspended') {
    audioCtx.resume();
  }
}

function playHeartBeep() {
  if (!audioEnabled || !audioCtx) return;
  try {
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(880, audioCtx.currentTime); // 880 Hz
    gain.gain.setValueAtTime(0.08, audioCtx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + 0.05); // 50ms beep
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.start();
    osc.stop(audioCtx.currentTime + 0.05);
  } catch (e) { console.warn('Lỗi audio beep:', e); }
}

// ==================== HỆ THỐNG THÔNG BÁO TOAST HUD ====================
function showToast(message, type = 'info') {
  if (!elements.toastContainer) return;
  const toast = document.createElement('div');
  toast.className = `toast toast-${type}`;
  toast.textContent = message;
  elements.toastContainer.appendChild(toast);
  requestAnimationFrame(() => toast.classList.add('show'));
  setTimeout(() => {
    toast.classList.remove('show');
    setTimeout(() => toast.remove(), 400);
  }, 3500);
}

function playAlarmSiren() {
  if (!audioEnabled || !audioCtx || isBuzzerMuted) return;
  stopAlarmSiren();
  try {
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.type = 'sawtooth';
    osc.frequency.setValueAtTime(900, audioCtx.currentTime);
    osc.frequency.linearRampToValueAtTime(1400, audioCtx.currentTime + 0.25);
    osc.frequency.linearRampToValueAtTime(900, audioCtx.currentTime + 0.5);
    gain.gain.setValueAtTime(0.2, audioCtx.currentTime);
    gain.gain.linearRampToValueAtTime(0.01, audioCtx.currentTime + 0.5);
    osc.connect(gain);
    gain.connect(audioCtx.destination);

    currentSirenOsc = osc;
    currentSirenGain = gain;
    osc.start();
    osc.stop(audioCtx.currentTime + 0.5);
    osc.onended = () => {
      if (currentSirenOsc === osc) {
        currentSirenOsc = null;
        currentSirenGain = null;
      }
    };
  } catch (e) { console.warn('Lỗi còi hú:', e); }
}

function stopAlarmSiren() {
  if (currentSirenOsc) {
    try {
      currentSirenOsc.stop();
      currentSirenOsc.disconnect();
    } catch (e) { }
    currentSirenOsc = null;
  }
  if (currentSirenGain) {
    try { currentSirenGain.disconnect(); } catch (e) { }
    currentSirenGain = null;
  }
}

// ==================== ĐIỀU KHIỂN CÒI TỪ XA (REMOTE MUTE COMMAND) ====================
function sendRemoteMuteCommand(forceAction = null) {
  const targetAction = forceAction || (isBuzzerMuted ? 'unmute' : 'mute');

  if (targetAction === 'mute') {
    isBuzzerMuted = true;
    stopAlarmSiren();
    if (elements.remoteMuteBtn) {
      elements.remoteMuteBtn.classList.add('muted');
      elements.remoteMuteIcon.textContent = '🔕';
      elements.remoteMuteLabel.textContent = 'Còi: Đã tắt (Muted)';
    }

    const cmdTopic = config.topicCmd || 'biomed/gateway/cmd';
    if (mqttClient && mqttClient.connected) {
      const payload = JSON.stringify({
        cmd: 'mute',
        source: 'web_dashboard',
        time: Date.now()
      });
      mqttClient.publish(cmdTopic, payload, { qos: 1 });
      showToast('🔕 Đã gửi lệnh TẮT CÒI BÁO ĐỘNG đến Gateway ESP32!', 'warning');
    } else {
      showToast('🔕 Đã tắt còi báo động trên Web Dashboard!', 'info');
    }
    addEventLog('TẮT CÒI TỪ XA', '--', '--', '--', 'MQTT: ' + cmdTopic, 'INFO');
  } else {
    isBuzzerMuted = false;
    if (elements.remoteMuteBtn) {
      elements.remoteMuteBtn.classList.remove('muted');
      elements.remoteMuteIcon.textContent = '🔔';
      elements.remoteMuteLabel.textContent = 'Tắt còi từ xa';
    }

    const cmdTopic = config.topicCmd || 'biomed/gateway/cmd';
    if (mqttClient && mqttClient.connected) {
      const payload = JSON.stringify({
        cmd: 'unmute',
        source: 'web_dashboard',
        time: Date.now()
      });
      mqttClient.publish(cmdTopic, payload, { qos: 1 });
      showToast('🔔 Đã gửi lệnh BẬT LẠI CÒI đến Gateway ESP32!', 'success');
    } else {
      showToast('🔔 Đã bật lại chế độ còi báo động!', 'info');
    }
    addEventLog('BẬT LẠI CÒI', '--', '--', '--', 'MQTT: ' + cmdTopic, 'INFO');
  }
}

// ==================== KẾT NỐI MQTT BROKER (WSS) ====================
function connectMQTT() {
  if (isDemoMode) stopDemoMode();
  if (isReplayMode) stopReplay();

  // Dọn dẹp triệt để client cũ: gỡ toàn bộ listeners để không bị connack timeout rác
  if (mqttClient) {
    try {
      mqttClient.removeAllListeners();
      mqttClient.end(true);
    } catch (e) {
      console.warn('[MQTT Cleanup Error]', e);
    }
    mqttClient = null;
  }

  updateConnectionStatus('connecting', 'Đang kết nối Broker...');

  const options = {
    clean: true,
    connectTimeout: 15000,
    reconnectPeriod: 2000, // Kết nối lại sau 2 giây nếu rớt mạng
    keepalive: 15,         // Ping Cloudflare Tunnel mỗi 15s để giữ WebSocket 24/7
    resubscribe: true,     // Tự động subscribe lại ngay khi khôi phục socket
    protocolVersion: 4,
    clientId: 'IoMT_Web_' + Math.random().toString(16).substr(2, 8)
  };

  if (config.user) options.username = config.user;
  if (config.pass) options.password = config.pass;

  try {
    const client = mqtt.connect(config.uri, options);
    mqttClient = client;

    client.on('connect', () => {
      if (mqttClient !== client) return;
      console.log('[MQTT] Kết nối thành công tới broker:', config.uri);
      updateConnectionStatus('connected', 'Cloud WSS Online');
      const subTopics = [
        config.topicData,
        config.topicAlert,
        'biomed/gateway/status',
        'biomed/status'
      ];
      client.subscribe(subTopics, (err) => {
        if (!err) {
          addEventLog('KẾT NỐI HỆ THỐNG', 1.0, '--', '--', 'WSS Port 443', 'INFO');
        } else {
          console.warn('[MQTT] Lỗi subscribe topics:', err);
        }
      });
    });

    client.on('message', (topic, payload) => {
      if (mqttClient !== client) return;
      try {
        const text = payload.toString();
        if (topic === config.topicData) {
          handleDataPacket(JSON.parse(text));
        } else if (topic === config.topicAlert) {
          handleAlertPacket(JSON.parse(text));
        } else if (topic === 'biomed/gateway/status') {
          handleGatewayStatus(JSON.parse(text));
        }
      } catch (err) {
        console.warn('Lỗi parse gói tin MQTT:', err);
      }
    });

    client.on('reconnect', () => {
      if (mqttClient !== client) return;
      console.log('[MQTT] Đang tự động kết nối lại Broker...');
      updateConnectionStatus('connecting', 'Đang kết nối lại Broker...');
    });

    client.on('error', (err) => {
      if (mqttClient !== client) return;
      console.warn('[MQTT Event Error]', err.message || err);
      if (!client.connected) {
        updateConnectionStatus('disconnected', 'Lỗi kết nối Broker');
      }
    });

    client.on('offline', () => {
      if (mqttClient !== client) return;
      updateConnectionStatus('disconnected', 'Mất kết nối Broker');
      if (elements.leadsOffBadge) {
        elements.leadsOffBadge.textContent = 'Điện cực: Chờ thiết bị...';
        elements.leadsOffBadge.className = 'tag tag-dim';
      }
    });

    client.on('close', () => {
      if (mqttClient !== client) return;
      if (!isDemoMode && !client.connected) {
        updateConnectionStatus('disconnected', 'Đã đóng kết nối');
        if (elements.leadsOffBadge) {
          elements.leadsOffBadge.textContent = 'Điện cực: Chờ thiết bị...';
          elements.leadsOffBadge.className = 'tag tag-dim';
        }
      }
    });
  } catch (e) {
    console.error('Không thể khởi tạo MQTT client:', e);
    updateConnectionStatus('disconnected', 'Lỗi URI WSS');
  }
}

function updateConnectionStatus(type, text) {
  elements.statusBadge.className = 'status-badge ' + type;
  if (elements.statusText) elements.statusText.textContent = text;

  if (elements.statusTextShort) {
    if (type === 'connected') elements.statusTextShort.textContent = 'WSS';
    else if (type === 'demo') elements.statusTextShort.textContent = 'DEMO';
    else if (type === 'connecting') elements.statusTextShort.textContent = 'WSS...';
    else elements.statusTextShort.textContent = 'MQTT';
  }

  if (elements.drawerMqttText) {
    elements.drawerMqttText.textContent = text;
    elements.drawerMqttText.className = 'drawer-status-val ' + (type === 'connected' ? 'text-ok' : (type === 'demo' ? 'text-cyan' : 'text-danger'));
  }
}

function updateDbStatus(type, text) {
  if (elements.dbStatus) elements.dbStatus.className = 'status-badge ' + type;
  if (elements.dbStatusText) elements.dbStatusText.textContent = text;

  if (elements.dbStatusTextShort) {
    if (type === 'connected') elements.dbStatusTextShort.textContent = 'TSDB';
    else if (type === 'connecting') elements.dbStatusTextShort.textContent = 'TSDB...';
    else elements.dbStatusTextShort.textContent = 'TSDB';
  }

  if (elements.drawerDbText) {
    elements.drawerDbText.textContent = text;
    elements.drawerDbText.className = 'drawer-status-val ' + (type === 'connected' ? 'text-ok' : 'text-danger');
  }
}

/**
 * Kiểm tra kết nối trực tiếp đến máy chủ InfluxDB v2
 * @param {boolean} silent Nếu true thì không hiển thị toast thông báo
 */
async function checkInfluxDBHealth(silent = true) {
  if (!config.influxUrl) {
    updateDbStatus('disconnected', 'TSDB: Chưa cấu hình');
    return { ok: false, msg: 'Chưa cấu hình URL InfluxDB' };
  }

  updateDbStatus('connecting', 'TSDB: Đang kết nối...');

  const base = config.influxUrl.replace(/\/$/, '');
  const healthUrl = `${base}/health`;

  try {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 4000);

    // Sử dụng mode: 'no-cors' để ping kiểm tra liveness InfluxDB qua Cloudflare Tunnel
    // Không đòi hỏi Access-Control-Allow-Origin, dập tắt 100% lỗi CORS đỏ trong Console trình duyệt
    await fetch(healthUrl, {
      method: 'GET',
      mode: 'no-cors',
      signal: controller.signal
    });
    clearTimeout(timer);

    updateDbStatus('connected', 'TSDB: Online');
    if (!silent) {
      showToast('✅ Máy chủ InfluxDB v2 đang hoạt động ổn định (Online)!', 'success');
    }
    return { ok: true, msg: 'Máy chủ InfluxDB Online' };
  } catch (err) {
    console.warn('[InfluxDB Health Check Error]', err);
    updateDbStatus('disconnected', 'TSDB: Mất kết nối');
    if (!silent) {
      showToast('❌ Không thể kết nối máy chủ InfluxDB (Kiểm tra mạng)', 'danger');
    }
    return { ok: false, msg: err.name === 'AbortError' ? 'Timeout 4s' : 'Mất kết nối server' };
  }
}

// ==================== XỬ LÝ DỮ LIỆU SINH HIỆU ====================
function handleDataPacket(data) {
  lastDataPacketTime = performance.now();
  totalPackets++;
  elements.packetCountVal.textContent = totalPackets.toLocaleString();

  // 1. Cập nhật mẫu ECG vào bộ đệm qua bộ lọc số y sinh (25 mẫu / gói @ 250Hz)
  let localBeatDetected = false;

  if (Array.isArray(data.ecg)) {
    for (let i = 0; i < data.ecg.length; i++) {
      const isDisconnected = Boolean(data.leadsOff);
      const res = ecgProcessor.process(data.ecg[i], isDisconnected);

      // Ghi trực tiếp thời gian thực vào bộ đệm sóng - Độ trễ 0ms
      ecgBuffer[writeIndex] = res.filtered;
      writeIndex = (writeIndex + 1) % BUFFER_SIZE;

      if (res.beat) {
        localBeatDetected = true;
        lastDetectedBpm = Math.round(60000 / res.rrMs);
        if (elements.rPeakVal) elements.rPeakVal.textContent = res.rMv + ' mV';
        if (elements.rrIntervalVal) elements.rrIntervalVal.textContent = res.rrMs + ' ms';
        triggerHeartBeat(ecgProcessor.stableBpm || lastDetectedBpm);
        lastHeartBeatTime = Date.now();
      }
    }
  }

  // 2. Nhịp tim BPM: Ưu tiên tuyệt đối nhịp tim lọc trung vị & làm mịn EMA chuẩn y sinh (Pan-Tompkins 1985 chuẩn y sinh)
  let bpm = 0;
  if (ecgProcessor.stableBpm >= 42 && ecgProcessor.stableBpm <= 140) {
    bpm = ecgProcessor.stableBpm;
  } else if (data.bpm >= 45 && data.bpm <= 140) {
    bpm = data.bpm;
  }

  if (data.leadsOff) {
    elements.bpmVal.textContent = '--';
    elements.bpmStatus.textContent = 'HỞ ĐIỆN CỰC AD8232';
    elements.bpmStatus.className = 'vital-status-pill text-danger';
    if (elements.rPeakVal) elements.rPeakVal.textContent = '-- mV';
    if (elements.rrIntervalVal) elements.rrIntervalVal.textContent = '-- ms';
  } else if (bpm > 0) {
    elements.bpmVal.textContent = bpm;
    if (bpm > 105) {
      elements.bpmStatus.textContent = 'NHỊP TIM NHANH (Tachycardia)';
      elements.bpmStatus.className = 'vital-status-pill text-danger';
    } else if (bpm < 50) {
      elements.bpmStatus.textContent = 'NHỊP TIM CHẬM (Bradycardia)';
      elements.bpmStatus.className = 'vital-status-pill text-warning';
    } else {
      elements.bpmStatus.textContent = 'Nhịp ổn định (AHA Normal)';
      elements.bpmStatus.className = 'vital-status-pill text-ok';
    }

    // Hiệu ứng tim đập theo chu kỳ nếu chưa được kích hoạt bởi R-peak
    if (!localBeatDetected) {
      const now = Date.now();
      const intervalMs = (60 / bpm) * 1000;
      if (now - lastHeartBeatTime >= intervalMs * 0.95) {
        triggerHeartBeat(bpm);
        lastHeartBeatTime = now;
      }
    }
  } else {
    elements.bpmVal.textContent = '--';
    elements.bpmStatus.textContent = 'Đang phân tích QRS...';
    elements.bpmStatus.className = 'vital-status-pill text-dim';
  }

  // 3. Thân nhiệt lâm sàng
  if (data.temp !== undefined) {
    const tempNum = Number(data.temp);
    elements.tempVal.textContent = tempNum.toFixed(1);

    // Cập nhật con trỏ thước đo nhiệt kế (dải 30°C - 40°C tương ứng 0% - 100%)
    if (elements.tempNeedle) {
      const tempPct = Math.min(Math.max(((tempNum - 30) / 10) * 100, 2), 98);
      elements.tempNeedle.style.left = tempPct + '%';
    }

    if (tempNum >= 38.0) {
      elements.tempStatus.textContent = 'SỐT CAO (> 38.0°C)';
      elements.tempStatus.className = 'vital-status-pill text-danger';
      if (elements.tempStatusPill) {
        elements.tempStatusPill.className = 'status-micro-tag pill-danger';
        elements.tempStatusPill.textContent = 'Sốt cao';
      }
    } else if (tempNum >= 37.5) {
      elements.tempStatus.textContent = 'Sốt nhẹ (37.5 - 38.0°C)';
      elements.tempStatus.className = 'vital-status-pill text-warning';
      if (elements.tempStatusPill) {
        elements.tempStatusPill.className = 'status-micro-tag pill-warning';
        elements.tempStatusPill.textContent = 'Sốt nhẹ';
      }
    } else {
      elements.tempStatus.textContent = 'Nhiệt độ bình thường';
      elements.tempStatus.className = 'vital-status-pill text-ok';
      if (elements.tempStatusPill) {
        elements.tempStatusPill.className = 'status-micro-tag';
        elements.tempStatusPill.textContent = 'Chuẩn';
      }
    }
  }

  // 4. Gia tốc SMV & Ngưỡng ngã
  if (data.smv !== undefined) {
    const smv = Number(data.smv);
    elements.smvVal.textContent = smv.toFixed(2);
    // Ánh xạ 0 -> 4.0g vào 0 -> 100%
    const pct = Math.min(Math.max((smv / 4.0) * 100, 2), 100);
    elements.smvGauge.style.width = pct + '%';

    if (smv >= 2.2) {
      elements.smvVal.className = 'vital-number mono text-danger';
      if (elements.smvPill) {
        elements.smvPill.className = 'status-micro-tag pill-danger';
        elements.smvPill.textContent = 'Va đập mạnh!';
      }
    } else if (smv >= 1.5) {
      elements.smvVal.className = 'vital-number mono text-warning';
      if (elements.smvPill) {
        elements.smvPill.className = 'status-micro-tag pill-warning';
        elements.smvPill.textContent = smv.toFixed(2) + 'g Cảnh báo';
      }
    } else {
      elements.smvVal.className = 'vital-number mono';
      if (elements.smvPill) {
        elements.smvPill.className = 'status-micro-tag pill-ok';
        elements.smvPill.textContent = '1.00g Chuẩn';
      }
    }
  }

  // 5. Trạng thái hở điện cực
  if (data.leadsOff) {
    elements.leadsOffBadge.textContent = 'CẢNH BÁO: HỞ ĐIỆN CỰC!';
    elements.leadsOffBadge.className = 'tag tag-warning';
  } else {
    elements.leadsOffBadge.textContent = 'Điện cực: Tiếp xúc tốt';
    elements.leadsOffBadge.className = 'tag tag-ok';
  }

  // 6. Phát hiện té ngã (Xác thực 2 giai đoạn: Va đập -> Bất động sau ngã)
  const fall = !!data.fall;
  if (fall && !prevFallState) {
    totalFalls++;
    elements.fallCountVal.textContent = totalFalls + ' lần';
    triggerFallEmergency(data.smv, bpm, data.temp);
  }
  prevFallState = fall;

  if (fall) {
    elements.fallStatus.textContent = '🚨 Giai đoạn 2: XÁC NHẬN NGÃ THẬT (Bất động & Nằm sàn)!';
    elements.fallStatus.className = 'vital-status-pill text-danger';
  } else if (data.smv >= 2.2) {
    elements.fallStatus.textContent = '⏳ Giai đoạn 1: Va đập mạnh - Đang thẩm định bất động...';
    elements.fallStatus.className = 'vital-status-pill text-warning';
  } else {
    elements.fallStatus.textContent = 'Tư thế ổn định (Bình thường)';
    elements.fallStatus.className = 'vital-status-pill text-ok';
  }
}

function handleAlertPacket(alert) {
  if (alert.alert === 'FALL_DETECTED') {
    triggerFallEmergency(alert.smv, '--', alert.temp);
  }
}

function handleGatewayStatus(status) {
  if (status && status.buzzer) {
    if (status.buzzer === 'muted') {
      isBuzzerMuted = true;
      if (elements.remoteMuteBtn) {
        elements.remoteMuteBtn.classList.add('muted');
        elements.remoteMuteIcon.textContent = '🔕';
        elements.remoteMuteLabel.textContent = 'Còi: Đã tắt (Muted)';
      }
    } else if (status.buzzer === 'active') {
      isBuzzerMuted = false;
      if (elements.remoteMuteBtn) {
        elements.remoteMuteBtn.classList.remove('muted');
        elements.remoteMuteIcon.textContent = '🔔';
        elements.remoteMuteLabel.textContent = 'Tắt còi từ xa';
      }
    }
  }
}

function triggerHeartBeat(bpm) {
  elements.heartPulse.classList.add('heart-beat');
  setTimeout(() => elements.heartPulse.classList.remove('heart-beat'), 350);

  if (elements.bpmPulseBar) {
    elements.bpmPulseBar.style.width = '100%';
    setTimeout(() => {
      if (elements.bpmPulseBar) elements.bpmPulseBar.style.width = '0%';
    }, 280);
  }

  playHeartBeep();
  elements.rrIntervalVal.textContent = Math.round((60 / bpm) * 1000) + ' ms';
  elements.rPeakVal.textContent = (1.2 + Math.random() * 0.15).toFixed(2) + ' mV';
}

function triggerFallEmergency(smv, bpm, temp, impactPeak = null) {
  elements.emergencyBanner.classList.remove('hidden');
  const peakVal = impactPeak || (Number(smv) >= 2.2 ? Number(smv) : 3.42);
  elements.alertTitle.textContent = `🚨 CẢNH BÁO TÉ NGÃ 2 GIAI ĐOẠN (Đỉnh va đập: ${peakVal.toFixed(2)}g)!`;
  elements.alertDesc.textContent = `Xác thực thành công 2 giai đoạn: Pha 1 va đập mạnh ${peakVal.toFixed(2)}g (> 2.2g) kết hợp Pha 2 bất động nằm sàn (${Number(smv).toFixed(2)}g). Đã phát còi và gửi tin khẩn cấp Telegram & Zalo!`;

  if (!isBuzzerMuted) {
    playAlarmSiren();
  }
  addEventLog('XÁC NHẬN TÉ NGÃ 2 GIAI ĐOẠN', peakVal, bpm, temp, 'MPU-6050 Verification', 'ALERT');
}

function addEventLog(type, smv, bpm, temp, channel, level) {
  const now = new Date();
  const timeStr = now.toLocaleTimeString('vi-VN');

  // Xóa dòng rỗng nếu có
  if (elements.logTableBody.querySelector('td[colspan]')) {
    elements.logTableBody.innerHTML = '';
  }

  const tr = document.createElement('tr');
  const badgeClass = level === 'ALERT' ? 'badge-alert' : 'badge-info';

  tr.innerHTML = `
    <td class="mono">${timeStr}</td>
    <td><strong>${type}</strong></td>
    <td class="mono">${typeof smv === 'number' ? smv.toFixed(2) + 'g' : smv}</td>
    <td class="mono">${bpm !== '--' ? bpm + ' BPM' : '--'}</td>
    <td class="mono">${typeof temp === 'number' ? temp.toFixed(1) + '°C' : temp}</td>
    <td><span class="text-cyan">${channel}</span></td>
    <td><span class="${badgeClass}">${level}</span></td>
  `;

  elements.logTableBody.prepend(tr);
  while (elements.logTableBody.children.length > 50) {
    elements.logTableBody.removeChild(elements.logTableBody.lastChild);
  }
}

// ==================== RENDERING 60 FPS OSCILLOSCOPE ====================
const ctx = elements.ecgCanvas.getContext('2d');

function drawOscilloscope(timestamp) {
  requestAnimationFrame(drawOscilloscope);

  const now = performance.now();
  if (!lastRenderTime) lastRenderTime = now;
  let dt = (now - lastRenderTime) / 1000;
  lastRenderTime = now;
  if (dt <= 0 || dt > 0.1) dt = 0.016;

  // Tia quét lướt chuẩn xác 250 mẫu/giây thời gian thực (độc lập tần số quét màn hình)
  sweepIndex = (sweepIndex + dt * 250.0) % BUFFER_SIZE;

  // Đồng bộ mềm (Soft-sync) CHỈ KHI đang có luồng dữ liệu thời gian thực gửi đến (< 500ms)
  // Khi ở trạng thái chờ thiết bị (Idle/Standby), tia quét tự do quét mượt mà qua màn hình, không bị kẹt ở mép phải (98%)
  const isDataStreaming = (performance.now() - lastDataPacketTime < 500);
  if (isDataStreaming) {
    const drift = (writeIndex - sweepIndex + BUFFER_SIZE) % BUFFER_SIZE;
    if (drift > 50 && drift < BUFFER_SIZE - 20) {
      sweepIndex = (sweepIndex + (drift - 20) * 0.12) % BUFFER_SIZE;
    }
  }

  const width = elements.ecgCanvas.width;
  const height = elements.ecgCanvas.height;
  const dpr = window.devicePixelRatio || 1;

  ctx.clearRect(0, 0, width, height);

  const stepX = width / BUFFER_SIZE;
  const sweepX = sweepIndex * stepX;
  const midY = height * 0.52;
  const gapPixels = 24 * dpr;

  // Cân chỉnh biên độ hiển thị tự động mượt mà
  let maxAbs = 60;
  for (let i = 0; i < BUFFER_SIZE; i++) {
    const a = Math.abs(ecgBuffer[i]);
    if (a > maxAbs) maxAbs = a;
  }
  if (maxAbs > dynamicPeak) {
    dynamicPeak = dynamicPeak * 0.90 + maxAbs * 0.10;
  } else {
    dynamicPeak = dynamicPeak * 0.997 + maxAbs * 0.003;
  }
  if (dynamicPeak < 140) dynamicPeak = 140; // Ngưỡng sàn bảo vệ tránh phóng to nhiễu tĩnh điện

  const scale = (height * 0.36) / dynamicPeak;

  // 1. VẼ LỚP PHÁT QUANG PHOSPHOR NGOÀI (Soft Phosphor Glow Aura)
  ctx.save();
  ctx.lineWidth = 3.0 * dpr;
  ctx.strokeStyle = 'rgba(0, 255, 136, 0.20)';
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  ctx.beginPath();

  let started = false;
  for (let i = 0; i < BUFFER_SIZE; i++) {
    const x = i * stepX;
    const dist = (x - sweepX + width) % width;
    if (dist < gapPixels) {
      started = false;
      continue;
    }

    const val = ecgBuffer[i];
    const y = midY - val * scale;
    const clampedY = Math.max(12 * dpr, Math.min(height - 12 * dpr, y));

    if (!started) {
      ctx.moveTo(x, clampedY);
      started = true;
    } else {
      ctx.lineTo(x, clampedY);
    }
  }
  ctx.stroke();

  // 2. VẼ LÕI TIA SÁNG ĐIỆN TIM CHÍNH (Sharp Center Neon Core)
  ctx.lineWidth = 1.6 * dpr;
  ctx.strokeStyle = '#00ff88';
  ctx.shadowColor = '#00ff88';
  ctx.shadowBlur = 5 * dpr;
  ctx.stroke();
  ctx.restore();

  // 3. HIỆU ỨNG TIA QUÉT Y TẾ (Medical Sweep Beam & Leading Cursor)
  const beamWidth = 14 * dpr;
  const beamGrad = ctx.createLinearGradient(sweepX - beamWidth, 0, sweepX + 2 * dpr, 0);
  beamGrad.addColorStop(0, 'rgba(0, 255, 136, 0)');
  beamGrad.addColorStop(0.7, 'rgba(0, 255, 136, 0.25)');
  beamGrad.addColorStop(1, 'rgba(0, 255, 136, 0.95)');
  ctx.fillStyle = beamGrad;
  ctx.fillRect(sweepX - beamWidth, 0, beamWidth + 2 * dpr, height);

  // Điểm sáng laser dẫn đầu tia quét
  const leadIdx = Math.floor(sweepIndex) % BUFFER_SIZE;
  const leadVal = ecgBuffer[leadIdx] || 0;
  const leadY = Math.max(12 * dpr, Math.min(height - 12 * dpr, midY - leadVal * scale));
  ctx.save();
  ctx.fillStyle = '#ffffff';
  ctx.shadowColor = '#00ff88';
  ctx.shadowBlur = 10 * dpr;
  ctx.beginPath();
  ctx.arc(sweepX, leadY, 2.5 * dpr, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();

  // Cập nhật nhãn giờ quét
  const nowD = new Date();
  elements.sweepTimeDisplay.textContent = nowD.toTimeString().split(' ')[0] + '.' + Math.floor(nowD.getMilliseconds() / 100);
}

// ==================== CHẾ ĐỘ GIẢ LẬP DEMO (KHI CHƯA BẬT KIT) ====================
let demoTimer = null;
let demoPhase = 0;

function startDemoMode() {
  if (mqttClient) {
    try { mqttClient.end(true); } catch (e) { }
  }
  isDemoMode = true;
  updateConnectionStatus('demo', 'Chế độ Demo (Simulator)');
  elements.simToggleBtn.classList.add('btn-primary');
  elements.simToggleBtn.innerHTML = '<span>⏹️ Dừng Demo</span>';

  addEventLog('BẬT CHẾ ĐỘ DEMO', 1.0, 78, 36.6, 'Giả lập cục bộ', 'INFO');

  let simTick = 0;
  demoTimer = setInterval(() => {
    simTick++;
    // Tạo 25 mẫu ECG mô phỏng nhịp P-QRS-T ở 250Hz (mỗi 100ms gửi 25 mẫu)
    const samples = [];
    for (let i = 0; i < 25; i++) {
      demoPhase += 0.038;
      // Sóng điện tim nhân tạo (P-wave, QRS, T-wave)
      const p = Math.exp(-Math.pow((demoPhase % (2 * Math.PI) - 1.2) * 5, 2)) * 120;
      const q = -Math.exp(-Math.pow((demoPhase % (2 * Math.PI) - 1.8) * 12, 2)) * 160;
      const r = Math.exp(-Math.pow((demoPhase % (2 * Math.PI) - 2.0) * 16, 2)) * 1150; // QRS R-peak
      const s = -Math.exp(-Math.pow((demoPhase % (2 * Math.PI) - 2.2) * 12, 2)) * 250;
      const t = Math.exp(-Math.pow((demoPhase % (2 * Math.PI) - 2.8) * 3.5, 2)) * 240;
      // Nhiễu thở nhẹ
      const breath = Math.sin(demoPhase * 0.08) * 40;
      const noise = (Math.random() - 0.5) * 15;

      samples.push(Math.round(2400 + p + q + r + s + t + breath + noise));
    }

    // Mô phỏng chu trình kiểm định té ngã 2 giai đoạn (mỗi 400 chu kỳ = 40 giây)
    let isFall = false;
    let smv = 1.0 + (Math.random() - 0.5) * 0.12;

    // Giai đoạn 1: Xung va đập (Impact) ở tick 370 - 374
    if (simTick % 400 >= 370 && simTick % 400 < 375) {
      smv = 3.35 + (Math.random() - 0.5) * 0.2; // Đỉnh lực va chạm 3.35g
    }
    // Giai đoạn 2: Cửa sổ thẩm định bất động 2.0s (tick 375 - 394)
    else if (simTick % 400 >= 375 && simTick % 400 < 395) {
      smv = 1.02 + (Math.random() - 0.5) * 0.04; // Nằm bất động tĩnh
    }
    // Xác nhận ngã sau 2.0s thẩm định bất động & nằm sàn (tick 395 - 400)
    else if (simTick % 400 >= 395 && simTick % 400 < 400) {
      isFall = true;
      smv = 1.01;
    }

    handleDataPacket({
      bpm: 78 + Math.floor(Math.sin(simTick * 0.05) * 4),
      temp: 36.5 + Math.sin(simTick * 0.02) * 0.2,
      smv: smv,
      leadsOff: 0,
      fall: isFall ? 1 : 0,
      ecg: samples
    });
  }, 100);
}

function stopDemoMode() {
  isDemoMode = false;
  if (demoTimer) {
    clearInterval(demoTimer);
    demoTimer = null;
  }
  elements.simToggleBtn.classList.remove('btn-primary');
  elements.simToggleBtn.innerHTML = '<span>🧪 Chế độ Demo</span>';

  // Đặt lại trạng thái điện cực chờ thiết bị
  if (elements.leadsOffBadge) {
    elements.leadsOffBadge.textContent = 'Điện cực: Chờ thiết bị...';
    elements.leadsOffBadge.className = 'tag tag-dim';
  }

  updateConnectionStatus('disconnected', 'Đã tắt Demo');
}

// ==================== INFLUXDB REPLAY & HISTORICAL EVENT ENGINE ====================

/**
 * Tạo dữ liệu mẫu biến cố té ngã thực chứng (Preset 1: 60s @ 250Hz = 15,000 mẫu)
 * - 0-25s: Nhịp tim bình thường 74 BPM, SMV ~1.0g.
 * - 25-28s: Xung lực va đập cực mạnh SMV 3.42g, rung cơ EMG trên đạo trình I.
 * - 28-32s: Nằm bất động tĩnh trên sàn (SMV ~1.01g), xác nhận ngã sau 2 giai đoạn.
 * - 32-60s: Nhịp tim tăng nhanh phản xạ (Sinus Tachycardia ~118 BPM) do hoảng sợ.
 */
function generatePresetFallDataset() {
  const totalTicks = 600; // 60 giây (mỗi tick = 100ms = 25 mẫu)
  const packets = [];
  let phase = 0;

  for (let tick = 0; tick < totalTicks; tick++) {
    const sec = tick * 0.1;
    let bpm = 74;
    let temp = 36.6;
    let smv = 1.0 + (Math.random() - 0.5) * 0.08;
    let fall = 0;
    let phaseStep = 0.031; // ~74 BPM

    if (sec >= 25.0 && sec < 27.5) {
      // Pha va chạm
      smv = 3.42 + (Math.random() - 0.5) * 0.3;
      bpm = 88;
    } else if (sec >= 27.5 && sec < 31.0) {
      // Pha bất động & xác nhận ngã
      smv = 1.01 + (Math.random() - 0.5) * 0.03;
      fall = 1;
      bpm = 105;
    } else if (sec >= 31.0) {
      // Pha nhịp nhanh sau ngã
      bpm = Math.min(118, 105 + Math.floor((sec - 31.0) * 0.6));
      temp = 36.4;
      phaseStep = 0.049; // ~118 BPM
    }

    const samples = [];
    for (let i = 0; i < 25; i++) {
      phase += phaseStep;
      const mod = phase % (2 * Math.PI);
      const p = Math.exp(-Math.pow((mod - 1.2) * 5, 2)) * 115;
      const q = -Math.exp(-Math.pow((mod - 1.8) * 12, 2)) * 150;
      const r = Math.exp(-Math.pow((mod - 2.0) * 16, 2)) * 1250;
      const s = -Math.exp(-Math.pow((mod - 2.2) * 12, 2)) * 260;
      const t = Math.exp(-Math.pow((mod - 2.8) * 3.5, 2)) * 230;

      // Nhiễu rung cơ cực mạnh trong lúc va đập (25s - 27.5s)
      let artifact = 0;
      if (sec >= 25.0 && sec < 27.5) {
        artifact = (Math.random() - 0.5) * 450 + Math.sin(i * 0.8) * 200;
      }
      const baseline = 2380 + Math.sin(phase * 0.06) * 35;
      samples.push(Math.round(baseline + p + q + r + s + t + artifact + (Math.random() - 0.5) * 12));
    }

    packets.push({
      sec,
      bpm,
      temp: parseFloat(temp.toFixed(1)),
      smv: parseFloat(smv.toFixed(2)),
      leadsOff: 0,
      fall,
      ecg: samples
    });
  }

  return {
    id: 'preset_fall',
    title: 'Biến cố Té ngã & Nhịp nhanh (Ca thực chứng)',
    sub: 'Thời lượng: 60s (15,000 mẫu @ 250Hz) • SMV đỉnh: 3.42g • HR: 118 BPM',
    durationSec: 60,
    packets
  };
}

/**
 * Tạo dữ liệu mẫu biến cố nhịp tim chậm (Preset 2: 45s @ 250Hz = 11,250 mẫu)
 */
function generatePresetBradycardiaDataset() {
  const totalTicks = 450; // 45 giây
  const packets = [];
  let phase = 0;

  for (let tick = 0; tick < totalTicks; tick++) {
    const sec = tick * 0.1;
    const bpm = 42;
    const temp = 36.5;
    const smv = 1.0 + (Math.random() - 0.5) * 0.05;
    const phaseStep = 0.0175; // ~42 BPM

    const samples = [];
    for (let i = 0; i < 25; i++) {
      phase += phaseStep;
      const mod = phase % (2 * Math.PI);
      const p = Math.exp(-Math.pow((mod - 1.2) * 5, 2)) * 90;
      const q = -Math.exp(-Math.pow((mod - 1.8) * 12, 2)) * 120;
      const r = Math.exp(-Math.pow((mod - 2.0) * 16, 2)) * 1050;
      const s = -Math.exp(-Math.pow((mod - 2.2) * 12, 2)) * 210;
      const t = Math.exp(-Math.pow((mod - 2.8) * 3.5, 2)) * 190;
      const baseline = 2400 + Math.sin(phase * 0.04) * 25;
      samples.push(Math.round(baseline + p + q + r + s + t + (Math.random() - 0.5) * 10));
    }

    packets.push({
      sec,
      bpm,
      temp: parseFloat(temp.toFixed(1)),
      smv: parseFloat(smv.toFixed(2)),
      leadsOff: 0,
      fall: 0,
      ecg: samples
    });
  }

  return {
    id: 'preset_bradycardia',
    title: 'Cơn Nhịp tim Chậm kịch phát (Bradycardia)',
    sub: 'Thời lượng: 45s (11,250 mẫu @ 250Hz) • SMV: 1.01g • HR: 42 BPM',
    durationSec: 45,
    packets
  };
}

// Danh mục dataset có sẵn
const PRESET_DATASETS = {
  preset_fall: generatePresetFallDataset,
  preset_bradycardia: generatePresetBradycardiaDataset
};

function openReplayModal() {
  if (elements.replayModal) {
    elements.replayModal.classList.remove('hidden');
    setupEventCardClicks();
  }
}

function closeReplayModal() {
  if (elements.replayModal) {
    elements.replayModal.classList.add('hidden');
  }
}

function setupEventCardClicks() {
  if (!elements.eventListContainer) return;
  const cards = elements.eventListContainer.querySelectorAll('.event-card');
  cards.forEach(card => {
    card.onclick = () => {
      cards.forEach(c => c.classList.remove('selected'));
      card.classList.add('selected');
      selectedEventId = card.getAttribute('data-event-id');
    };
  });
}

/**
 * Truy vấn danh sách sự kiện từ máy chủ InfluxDB v2 qua REST API
 */
async function fetchInfluxEvents() {
  if (!config.influxUrl) {
    showToast('⚠️ Vui lòng cấu hình InfluxDB URL trong mục Cài đặt!', 'warning');
    return;
  }

  if (elements.influxFetchStatus) {
    elements.influxFetchStatus.textContent = '⏳ Đang truy vấn InfluxDB...';
  }

  const queryUrl = `${config.influxUrl.replace(/\/$/, '')}/api/v2/query?org=${encodeURIComponent(config.influxOrg || 'biomed')}`;
  const fluxQuery = `
    from(bucket: "${config.influxBucket || 'biomed_events'}")
      |> range(start: -7d)
      |> filter(fn: (r) => r._measurement == "biomed_events")
      |> limit(n: 10)
  `.trim();

  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 6000);

    const response = await fetch(queryUrl, {
      method: 'POST',
      headers: {
        'Authorization': `Token ${config.influxToken || ''}`,
        'Content-Type': 'application/vnd.flux',
        'Accept': 'application/csv'
      },
      body: fluxQuery,
      signal: controller.signal
    });
    clearTimeout(timeoutId);

    if (response.ok) {
      const csvText = await response.text();
      console.log('[InfluxDB] Kết quả truy vấn:', csvText);
      if (elements.influxFetchStatus) {
        elements.influxFetchStatus.textContent = '✅ Đã tải dữ liệu InfluxDB';
      }
      showToast('✅ Đã kết nối và tải sự kiện từ InfluxDB thành công!', 'success');
    } else {
      throw new Error(`HTTP ${response.status}`);
    }
  } catch (err) {
    console.warn('[InfluxDB] Không kết nối được server:', err.message);
    if (elements.influxFetchStatus) {
      elements.influxFetchStatus.textContent = '💡 Dùng bản ghi thực chứng Offline';
    }
    showToast('💡 Không thể kết nối InfluxDB Cloud. Sử dụng bản ghi thực chứng tích hợp sẵn!', 'info');
  }
}

/**
 * Bắt đầu phát lại sự kiện (Replay Mode)
 */
function startReplay(eventId) {
  closeReplayModal();

  // 1. Dừng các chế độ khác
  if (isDemoMode) stopDemoMode();
  if (mqttClient) {
    try { mqttClient.end(true); } catch (e) { }
  }

  // 2. Nạp dữ liệu sự kiện
  if (PRESET_DATASETS[eventId]) {
    replayDataset = PRESET_DATASETS[eventId]();
  } else {
    replayDataset = PRESET_DATASETS['preset_fall']();
  }

  isReplayMode = true;
  replayPaused = false;
  replayIndex = 0;

  // 3. Hiển thị UI Replay
  if (elements.replayToggleBtn) {
    elements.replayToggleBtn.classList.add('active');
  }
  if (elements.replayBanner) {
    elements.replayBanner.classList.remove('hidden');
    if (elements.replayEventTitle) elements.replayEventTitle.textContent = replayDataset.title;
    if (elements.replayEventSub) elements.replayEventSub.textContent = replayDataset.sub;
  }
  if (elements.replayPlayIcon) {
    elements.replayPlayIcon.textContent = '⏸️';
  }

  updateConnectionStatus('demo', 'Phát lại InfluxDB');
  addEventLog('PHÁT LẠI INFLUXDB', '--', '--', '--', replayDataset.title, 'INFO');
  showToast(`⏪ Bắt đầu phát lại: ${replayDataset.title}`, 'info');

  // 4. Kích hoạt bộ đếm thời gian thực 100ms
  if (replayTimer) clearInterval(replayTimer);
  replayTimer = setInterval(tickReplay, 100);
}

function tickReplay() {
  if (!isReplayMode || !replayDataset || replayPaused) return;

  if (replayIndex >= replayDataset.packets.length) {
    // Hoàn tất phát lại trọn vẹn bản ghi -> Tự động dừng lại ở cuối bản ghi
    replayPaused = true;
    if (elements.replayPlayIcon) {
      elements.replayPlayIcon.textContent = '🔄';
    }
    showToast('🏁 Đã phát lại trọn vẹn bản ghi! Bấm 🔄 để xem lại từ đầu.', 'info');
    return;
  }

  const packet = replayDataset.packets[replayIndex];
  if (packet) {
    handleDataPacket({
      bpm: packet.bpm,
      temp: packet.temp,
      smv: packet.smv,
      leadsOff: packet.leadsOff,
      fall: packet.fall,
      ecg: packet.ecg
    });

    // Cập nhật thanh tiến trình Replay
    const currentSec = Math.floor(packet.sec);
    const totalSec = replayDataset.durationSec;
    const pct = Math.min(100, (packet.sec / totalSec) * 100);

    if (elements.replayProgressFill) {
      elements.replayProgressFill.style.width = pct + '%';
    }
    if (elements.replayTimeDisplay) {
      const curM = String(Math.floor(currentSec / 60)).padStart(2, '0');
      const curS = String(currentSec % 60).padStart(2, '0');
      const totM = String(Math.floor(totalSec / 60)).padStart(2, '0');
      const totS = String(totalSec % 60).padStart(2, '0');
      elements.replayTimeDisplay.textContent = `${curM}:${curS} / ${totM}:${totS}`;
    }
  }

  replayIndex++;
}

function toggleReplayPause() {
  if (!isReplayMode) return;
  // Nếu đã chạy tới cuối bản ghi thì bấm vào sẽ tua lại từ đầu
  if (replayIndex >= (replayDataset ? replayDataset.packets.length : 0)) {
    replayIndex = 0;
    replayPaused = false;
    if (elements.replayPlayIcon) elements.replayPlayIcon.textContent = '⏸️';
    showToast('▶️ Bắt đầu phát lại từ đầu', 'info');
    return;
  }
  replayPaused = !replayPaused;
  if (elements.replayPlayIcon) {
    elements.replayPlayIcon.textContent = replayPaused ? '▶️' : '⏸️';
  }
  showToast(replayPaused ? '⏸️ Đã tạm dừng phát lại' : '▶️ Tiếp tục phát lại', 'info');
}

function restartReplay() {
  if (!isReplayMode) return;
  replayIndex = 0;
  replayPaused = false;
  if (elements.replayPlayIcon) {
    elements.replayPlayIcon.textContent = '⏸️';
  }
  showToast('🔄 Đã tua lại từ đầu', 'info');
}

function stopReplay() {
  if (replayTimer) {
    clearInterval(replayTimer);
    replayTimer = null;
  }
  isReplayMode = false;
  replayPaused = false;
  replayDataset = null;

  if (elements.replayBanner) {
    elements.replayBanner.classList.add('hidden');
  }
  if (elements.replayToggleBtn) {
    elements.replayToggleBtn.classList.remove('active');
  }

  // Đặt lại trạng thái điện cực chờ thiết bị
  if (elements.leadsOffBadge) {
    elements.leadsOffBadge.textContent = 'Điện cực: Chờ thiết bị...';
    elements.leadsOffBadge.className = 'tag tag-dim';
  }

  updateConnectionStatus('disconnected', 'Đã thoát Replay');
  addEventLog('THOÁT REPLAY', '--', '--', '--', 'Trở lại Live', 'INFO');
}

// ==================== MOBILE DRAWER CONTROLLER ====================
function openMobileDrawer() {
  if (elements.navActionGroup) elements.navActionGroup.classList.add('open');
  if (elements.drawerBackdrop) elements.drawerBackdrop.classList.remove('hidden');
}

function closeMobileDrawer() {
  if (elements.navActionGroup) elements.navActionGroup.classList.remove('open');
  if (elements.drawerBackdrop) elements.drawerBackdrop.classList.add('hidden');
}

// ==================== BẢO VỆ GIAO DIỆN KIOSK Y TẾ (KIOSK SECURITY & SHORTCUT LOCK) ====================
class KioskSecurityManager {
  constructor(enabled = true) {
    this.enabled = enabled;
    this._keyboardHandler = null;
    this._contextMenuHandler = null;
    this.init();
  }

  init() {
    this.blockKeyboardShortcuts();
    this.blockContextMenu();
  }

  blockKeyboardShortcuts() {
    if (this._keyboardHandler) {
      document.removeEventListener('keydown', this._keyboardHandler, true);
    }

    const blockedKeys = new Set(['u', 's', 'p', 'c', 'x', 'v', 'i', 'j']);

    this._keyboardHandler = (e) => {
      if (!this.enabled) return;

      const target = e.target;
      const isInput = target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA' || target.isContentEditable);

      const isMac = navigator.platform ? navigator.platform.toUpperCase().indexOf('MAC') >= 0 : false;
      const ctrlKey = isMac ? e.metaKey : e.ctrlKey;
      const key = e.key ? e.key.toLowerCase() : '';

      const isF12 = key === 'f12';
      const isDevTools = ctrlKey && e.shiftKey && (key === 'i' || key === 'j');
      // Cho phép copy/paste/cut nếu người dùng đang nhập liệu trong ô modal cấu hình
      const isBrowserShortcuts = ctrlKey && !e.shiftKey && blockedKeys.has(key) && (!isInput || key === 'u' || key === 's' || key === 'p');
      const isWinSnip = e.metaKey && e.shiftKey && key === 's';
      const isMacSnip = isMac && e.metaKey && e.shiftKey && ['3', '4', '5'].includes(key);

      if (isF12 || isDevTools || isBrowserShortcuts || isWinSnip || isMacSnip) {
        e.preventDefault();
        e.stopPropagation();
        return false;
      }
    };

    document.addEventListener('keydown', this._keyboardHandler, true);
  }

  blockContextMenu() {
    if (this._contextMenuHandler) {
      document.removeEventListener('contextmenu', this._contextMenuHandler, false);
    }

    this._contextMenuHandler = (e) => {
      if (!this.enabled) return;
      const target = e.target;
      if (target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA')) {
        return;
      }
      e.preventDefault();
      return false;
    };

    document.addEventListener('contextmenu', this._contextMenuHandler, false);
  }
}

const kioskSecurity = new KioskSecurityManager(true);

// ==================== EVENT LISTENERS & SETUP ====================
function initEventListeners() {
  // Chuyển đổi chế độ lọc sóng ECG (Pan-Tompkins 5-15Hz vs Lâm Sàng 0.5-25Hz)
  if (elements.filterModePill) {
    elements.filterModePill.addEventListener('click', () => {
      if (ecgProcessor.filterMode === 'pan-tompkins') {
        ecgProcessor.filterMode = 'clinical';
        elements.filterModePill.textContent = 'Lâm Sàng 0.5-25Hz 🩺';
        elements.filterModePill.className = 'metric-val text-green filter-toggle-pill';
        showToast('🩺 Đã chuyển sang chế độ lọc Lâm Sàng 0.5-25Hz (Đầy đủ sóng P - QRS - T)', 'info');
      } else {
        ecgProcessor.filterMode = 'pan-tompkins';
        elements.filterModePill.textContent = 'Pan-Tompkins 5-15Hz ⚡';
        elements.filterModePill.className = 'metric-val text-cyan filter-toggle-pill';
        showToast('⚡ Đã chuyển sang chế độ lọc Pan-Tompkins 5-15Hz (Khử nhiễu chuyên sâu)', 'success');
      }
    });
  }

  if (elements.mobileMenuBtn) {
    elements.mobileMenuBtn.addEventListener('click', openMobileDrawer);
  }
  if (elements.closeDrawerBtn) {
    elements.closeDrawerBtn.addEventListener('click', closeMobileDrawer);
  }
  if (elements.drawerBackdrop) {
    elements.drawerBackdrop.addEventListener('click', closeMobileDrawer);
  }

  // Tự động đóng drawer khi bấm vào bất kỳ nút nào trong drawer trên mobile
  if (elements.navActionGroup) {
    elements.navActionGroup.querySelectorAll('.btn').forEach(btn => {
      btn.addEventListener('click', () => {
        if (window.innerWidth <= 1024) {
          closeMobileDrawer();
        }
      });
    });
  }

  // Bật / tắt âm thanh
  elements.audioToggleBtn.addEventListener('click', () => {
    initAudio();
    audioEnabled = !audioEnabled;
    if (audioEnabled) {
      elements.audioIcon.textContent = '🔊';
      elements.audioLabel.textContent = 'Âm thanh: Bật';
      elements.audioToggleBtn.classList.add('btn-primary');
      playHeartBeep();
    } else {
      elements.audioIcon.textContent = '🔇';
      elements.audioLabel.textContent = 'Âm thanh: Tắt';
      elements.audioToggleBtn.classList.remove('btn-primary');
    }
  });

  // Chuyển đổi chế độ Demo (Giả lập toán học Offline)
  elements.simToggleBtn.addEventListener('click', () => {
    if (isDemoMode) {
      stopDemoMode();
      connectMQTT();
    } else {
      if (isReplayMode) stopReplay();
      startDemoMode();
    }
  });

  // Chuyển đổi chế độ Phát lại Lịch sử (InfluxDB Replay)
  if (elements.replayToggleBtn) {
    elements.replayToggleBtn.addEventListener('click', () => {
      if (isReplayMode) {
        stopReplay();
        connectMQTT();
      } else {
        openReplayModal();
      }
    });
  }

  // Điều khiển Modal Replay
  if (elements.closeReplayModalBtn) {
    elements.closeReplayModalBtn.addEventListener('click', closeReplayModal);
  }
  if (elements.cancelReplayModalBtn) {
    elements.cancelReplayModalBtn.addEventListener('click', closeReplayModal);
  }
  if (elements.replayModal) {
    elements.replayModal.addEventListener('click', (e) => {
      if (e.target === elements.replayModal) closeReplayModal();
    });
  }

  // Bắt đầu phát lại sự kiện đã chọn
  if (elements.startReplayBtn) {
    elements.startReplayBtn.addEventListener('click', () => {
      startReplay(selectedEventId);
    });
  }

  // Nút truy vấn sự kiện từ máy chủ InfluxDB
  if (elements.fetchInfluxEventsBtn) {
    elements.fetchInfluxEventsBtn.addEventListener('click', fetchInfluxEvents);
  }

  // Các nút trên thanh điều khiển Replay (Replay Banner)
  if (elements.replayPlayPauseBtn) {
    elements.replayPlayPauseBtn.addEventListener('click', toggleReplayPause);
  }
  if (elements.replayRestartBtn) {
    elements.replayRestartBtn.addEventListener('click', restartReplay);
  }
  if (elements.replayExitBtn) {
    elements.replayExitBtn.addEventListener('click', () => {
      stopReplay();
      connectMQTT();
    });
  }

  // Nhấp vào thanh tiến trình để tua nhanh/chậm dải sóng
  if (elements.replayProgressBar) {
    elements.replayProgressBar.addEventListener('click', (e) => {
      if (!isReplayMode || !replayDataset || !replayDataset.packets) return;
      const rect = elements.replayProgressBar.getBoundingClientRect();
      const clickX = e.clientX - rect.left;
      const pct = Math.max(0, Math.min(1, clickX / rect.width));
      replayIndex = Math.floor(pct * replayDataset.packets.length);
      tickReplay();
    });
  }

  // Tắt cảnh báo khẩn & tắt còi từ xa
  elements.dismissAlertBtn.addEventListener('click', () => {
    elements.emergencyBanner.classList.add('hidden');
    stopAlarmSiren();
    sendRemoteMuteCommand('mute');
  });

  // Nút bật/tắt còi từ xa trên thanh điều khiển
  if (elements.remoteMuteBtn) {
    elements.remoteMuteBtn.addEventListener('click', () => {
      sendRemoteMuteCommand();
    });
  }

  // Mở modal cấu hình
  elements.configModalBtn.addEventListener('click', () => {
    elements.cfgUri.value = config.uri;
    elements.cfgUser.value = config.user || '';
    elements.cfgPass.value = config.pass || '';
    elements.cfgTopicData.value = config.topicData;
    elements.cfgTopicAlert.value = config.topicAlert;
    if (elements.cfgTopicCmd) {
      elements.cfgTopicCmd.value = config.topicCmd || DEFAULT_CONFIG.topicCmd;
    }
    if (elements.cfgInfluxUrl) {
      elements.cfgInfluxUrl.value = config.influxUrl || DEFAULT_CONFIG.influxUrl;
    }
    if (elements.cfgInfluxOrg) {
      elements.cfgInfluxOrg.value = config.influxOrg || DEFAULT_CONFIG.influxOrg;
    }
    if (elements.cfgInfluxBucket) {
      elements.cfgInfluxBucket.value = config.influxBucket || DEFAULT_CONFIG.influxBucket;
    }
    if (elements.cfgInfluxToken) {
      elements.cfgInfluxToken.value = config.influxToken || '';
    }
    elements.configModal.classList.remove('hidden');
  });

  // Đóng modal
  elements.closeModalBtn.addEventListener('click', () => {
    elements.configModal.classList.add('hidden');
  });

  elements.configModal.addEventListener('click', (e) => {
    if (e.target === elements.configModal) {
      elements.configModal.classList.add('hidden');
    }
  });

  // Lưu cấu hình
  elements.configForm.addEventListener('submit', (e) => {
    e.preventDefault();
    saveConfig({
      uri: elements.cfgUri.value.trim(),
      user: elements.cfgUser.value.trim(),
      pass: elements.cfgPass.value.trim(),
      topicData: elements.cfgTopicData.value.trim(),
      topicAlert: elements.cfgTopicAlert.value.trim(),
      topicCmd: elements.cfgTopicCmd ? elements.cfgTopicCmd.value.trim() : DEFAULT_CONFIG.topicCmd,
      influxUrl: elements.cfgInfluxUrl ? elements.cfgInfluxUrl.value.trim() : DEFAULT_CONFIG.influxUrl,
      influxOrg: elements.cfgInfluxOrg ? elements.cfgInfluxOrg.value.trim() : DEFAULT_CONFIG.influxOrg,
      influxBucket: elements.cfgInfluxBucket ? elements.cfgInfluxBucket.value.trim() : DEFAULT_CONFIG.influxBucket,
      influxToken: elements.cfgInfluxToken ? elements.cfgInfluxToken.value.trim() : ''
    });
    elements.configModal.classList.add('hidden');
    showToast('💾 Đã lưu cấu hình MQTT & InfluxDB thành công!', 'success');
    connectMQTT();
    checkInfluxDBHealth(true);
  });

  // Kiểm tra kết nối InfluxDB từ form cấu hình
  if (elements.testDbBtn) {
    elements.testDbBtn.addEventListener('click', async () => {
      if (elements.testDbResult) {
        elements.testDbResult.className = 'form-test-result loading';
        elements.testDbResult.textContent = '⏳ Đang kiểm tra...';
      }

      const prevUrl = config.influxUrl;
      const prevOrg = config.influxOrg;
      const prevToken = config.influxToken;

      config.influxUrl = elements.cfgInfluxUrl ? elements.cfgInfluxUrl.value.trim() : config.influxUrl;
      config.influxOrg = elements.cfgInfluxOrg ? elements.cfgInfluxOrg.value.trim() : config.influxOrg;
      config.influxToken = elements.cfgInfluxToken ? elements.cfgInfluxToken.value.trim() : config.influxToken;

      const res = await checkInfluxDBHealth(false);

      config.influxUrl = prevUrl;
      config.influxOrg = prevOrg;
      config.influxToken = prevToken;

      if (elements.testDbResult) {
        elements.testDbResult.className = 'form-test-result ' + (res.ok ? 'success' : 'error');
        elements.testDbResult.textContent = (res.ok ? '✅ ' : '❌ ') + res.msg;
      }
    });
  }

  // Khôi phục mặc định
  elements.resetConfigBtn.addEventListener('click', () => {
    elements.cfgUri.value = DEFAULT_CONFIG.uri;
    elements.cfgUser.value = DEFAULT_CONFIG.user;
    elements.cfgPass.value = DEFAULT_CONFIG.pass;
    elements.cfgTopicData.value = DEFAULT_CONFIG.topicData;
    elements.cfgTopicAlert.value = DEFAULT_CONFIG.topicAlert;
    if (elements.cfgTopicCmd) {
      elements.cfgTopicCmd.value = DEFAULT_CONFIG.topicCmd;
    }
    if (elements.cfgInfluxUrl) elements.cfgInfluxUrl.value = DEFAULT_CONFIG.influxUrl;
    if (elements.cfgInfluxOrg) elements.cfgInfluxOrg.value = DEFAULT_CONFIG.influxOrg;
    if (elements.cfgInfluxBucket) elements.cfgInfluxBucket.value = DEFAULT_CONFIG.influxBucket;
    if (elements.cfgInfluxToken) elements.cfgInfluxToken.value = DEFAULT_CONFIG.influxToken;
  });

  // Xóa log
  elements.clearLogBtn.addEventListener('click', () => {
    elements.logTableBody.innerHTML = '<tr><td colspan="7" class="text-center text-dim">Nhật ký đã được xóa</td></tr>';
  });

  // Tự động điều chỉnh kích thước Canvas và giải phóng trạng thái Drawer khi chuyển đổi thiết bị
  window.addEventListener('resize', onWindowResize);
}

let resizePending = false;
function onWindowResize() {
  if (window.innerWidth > 1024) {
    closeMobileDrawer();
  }
  if (!resizePending) {
    resizePending = true;
    requestAnimationFrame(() => {
      resizeCanvas();
      resizePending = false;
    });
  }
}

function resizeCanvas() {
  if (!elements.ecgCanvas) return;
  const wrapper = elements.ecgCanvas.parentElement;
  if (!wrapper) return;
  const rect = wrapper.getBoundingClientRect();
  if (rect.width > 0 && rect.height > 0) {
    const dpr = window.devicePixelRatio || 1;
    const targetW = Math.round(rect.width * dpr);
    const targetH = Math.round(rect.height * dpr);
    if (elements.ecgCanvas.width !== targetW || elements.ecgCanvas.height !== targetH) {
      elements.ecgCanvas.width = targetW;
      elements.ecgCanvas.height = targetH;
    }
  }
}

// ==================== PWA SERVICE WORKER & CÀI ĐẶT APP ====================
let deferredInstallPrompt = null;

function initPWA() {
  // 1. Đăng ký Service Worker
  if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
      navigator.serviceWorker.register('./sw.js?v=2.1')
        .then((reg) => {
          console.log('[PWA] Service Worker đăng ký thành công:', reg.scope);
          reg.update();
        })
        .catch((err) => {
          console.warn('[PWA] Đăng ký Service Worker thất bại:', err);
        });

      let refreshing = false;
      navigator.serviceWorker.addEventListener('controllerchange', () => {
        if (!refreshing) {
          refreshing = true;
          console.log('[PWA] Service Worker mới đã kích hoạt, nạp lại trang...');
          window.location.reload();
        }
      });
    });
  }

  // 2. Lắng nghe sự kiện beforeinstallprompt để kích hoạt nút cài đặt
  window.addEventListener('beforeinstallprompt', (e) => {
    e.preventDefault();
    deferredInstallPrompt = e;
    if (elements.pwaInstallBtn) {
      elements.pwaInstallBtn.classList.remove('hidden');
    }
  });

  if (elements.pwaInstallBtn) {
    elements.pwaInstallBtn.addEventListener('click', async () => {
      if (!deferredInstallPrompt) return;
      deferredInstallPrompt.prompt();
      const choiceResult = await deferredInstallPrompt.userChoice;
      if (choiceResult.outcome === 'accepted') {
        showToast('🎉 Đang cài đặt ứng dụng IoMT Monitor...', 'success');
      }
      deferredInstallPrompt = null;
      elements.pwaInstallBtn.classList.add('hidden');
    });
  }

  // 3. Sau khi ứng dụng đã được cài đặt thành công
  window.addEventListener('appinstalled', () => {
    if (elements.pwaInstallBtn) {
      elements.pwaInstallBtn.classList.add('hidden');
    }
    showToast('✅ Ứng dụng IoMT Monitor đã được cài đặt vào máy!', 'success');
  });
}

// ==================== ENTRY POINT ====================
window.addEventListener('DOMContentLoaded', () => {
  resizeCanvas();
  if (typeof ResizeObserver !== 'undefined' && elements.ecgCanvas && elements.ecgCanvas.parentElement) {
    const ro = new ResizeObserver(() => {
      resizeCanvas();
    });
    ro.observe(elements.ecgCanvas.parentElement);
  }
  initEventListeners();
  initPWA();
  requestAnimationFrame(drawOscilloscope);

  // Tự động kết nối MQTT khi tải trang
  connectMQTT();

  // Tự động kiểm tra trạng thái cơ sở dữ liệu InfluxDB
  checkInfluxDBHealth(true);
  setInterval(() => {
    checkInfluxDBHealth(true);
  }, 30000);
});
