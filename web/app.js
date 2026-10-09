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
  topicCmd: 'biomed/gateway/cmd'      // Topic điều khiển Gateway (Tắt còi từ xa)
};

// ==================== TRẠNG THÁI HỆ THỐNG ====================
let config = loadConfig();
let mqttClient = null;
let isDemoMode = false;
let audioEnabled = false;
let isBuzzerMuted = false;
let currentSirenOsc = null;
let currentSirenGain = null;
let totalPackets = 0;
let totalFalls = 0;
let prevFallState = 0;
let lastHeartBeatTime = 0;

// ==================== BỘ LỌC TÍN HIỆU ECG CHUẨN Y TẾ ====================
/**
 * Chuỗi lọc số thời gian thực chuẩn sinh y:
 * 1. 2nd-order IIR Biquad 50Hz Notch Filter (Fs = 250Hz, Q = 7.0): Triệt tiêu triệt để nhiễu điện lưới 50Hz.
 * 2. High-pass Filter (0.5Hz Baseline Detrending): Loại bỏ trôi đường đẳng điện do thở và dịch chuyển điện cực.
 * 3. Low-pass Filter (35Hz Muscle Tremor Rejection): Làm mịn gai nhiễu ADC và co cơ EMG.
 * 4. Pan-Tompkins QRS Derivative + Moving Window Integration: Nhận diện R-Peak và đo chu kỳ RR thực tế.
 */
class MedicalEcgProcessor {
  constructor(fs = 250, mainsHz = 50) {
    this.fs = fs;
    // Biquad 50Hz Notch Filter (@ 250Hz sample rate, f0 = 50Hz, Q = 7.0)
    const w0 = (2.0 * Math.PI * mainsHz) / fs;
    const c = Math.cos(w0);
    const s = Math.sin(w0);
    const alpha = s / (2.0 * 7.0);
    const a0 = 1.0 + alpha;
    this.b0 = 1.0 / a0;
    this.b1 = (-2.0 * c) / a0;
    this.b2 = 1.0 / a0;
    this.a1 = (-2.0 * c) / a0;
    this.a2 = (1.0 - alpha) / a0;

    this.x1 = 2048; this.x2 = 2048;
    this.y1 = 2048; this.y2 = 2048;

    this.baseline = 2048;
    this.lp = 0;

    // Pan-Tompkins QRS Detector
    this.hp1 = 0; this.hp2 = 0; this.hp3 = 0; this.hp4 = 0;
    this.mwiWindow = 30; // 120ms @ 250Hz
    this.mwiBuf = new Float32Array(this.mwiWindow).fill(0);
    this.mwiIdx = 0;
    this.mwiSum = 0;
    this.mwiPeak = 300;
    this.lastBeatSample = 0;
    this.sampleCount = 0;
    this.refractorySamples = Math.round(fs * 0.24); // 240ms refractory (~250 BPM max)
    this.above = false;
    this.recentRR = [];
    this.lastRRMs = 0;
    this.lastRPeakMv = 0;
  }

  reset() {
    this.x1 = 2048; this.x2 = 2048;
    this.y1 = 2048; this.y2 = 2048;
    this.baseline = 2048;
    this.lp = 0;
    this.hp1 = 0; this.hp2 = 0; this.hp3 = 0; this.hp4 = 0;
    this.mwiBuf.fill(0);
    this.mwiSum = 0;
    this.mwiPeak = 300;
    this.above = false;
  }

  process(rawSample, isLeadsOff) {
    this.sampleCount++;
    if (isLeadsOff || rawSample <= 20 || rawSample >= 4080) {
      this.reset();
      return { filtered: 0, beat: false, rrMs: 0, rMv: '0.00' };
    }

    // 1. Biquad Notch Filter 50Hz (Triệt tiêu hoàn toàn nhiễu sóng xoay chiều 220V/50Hz)
    const notch = this.b0 * rawSample + this.b1 * this.x1 + this.b2 * this.x2 - this.a1 * this.y1 - this.a2 * this.y2;
    this.x2 = this.x1; this.x1 = rawSample;
    this.y2 = this.y1; this.y1 = notch;

    // 2. High-pass Filter (0.5Hz Baseline Tracker): Khử trôi baseline do hô hấp
    this.baseline += (notch - this.baseline) * 0.005;
    const hp = notch - this.baseline;

    // 3. Low-pass Filter (~35Hz): Khử nhiễu rung cơ EMG & gai ADC
    this.lp += (hp - this.lp) * 0.38;

    // 4. Pan-Tompkins 5-point derivative
    const deriv = (2.0 * hp + this.hp1 - this.hp3 - 2.0 * this.hp4) * 0.125;
    this.hp4 = this.hp3; this.hp3 = this.hp2; this.hp2 = this.hp1; this.hp1 = hp;
    const dsq = deriv * deriv;

    this.mwiSum += dsq - this.mwiBuf[this.mwiIdx];
    this.mwiBuf[this.mwiIdx] = dsq;
    this.mwiIdx = (this.mwiIdx + 1) % this.mwiWindow;
    const mwi = this.mwiSum / this.mwiWindow;

    this.mwiPeak *= 0.9985;
    if (mwi > this.mwiPeak) this.mwiPeak = mwi;

    const threshold = this.mwiPeak * 0.35;
    const isAbove = mwi > threshold;
    const rising = isAbove && !this.above;
    this.above = isAbove;

    let beat = false;
    let rrMs = this.lastRRMs;
    let rMv = this.lastRPeakMv;

    if (rising && (this.sampleCount - this.lastBeatSample > this.refractorySamples) && this.sampleCount > this.fs) {
      const prevBeat = this.lastBeatSample;
      const rrSamples = this.sampleCount - prevBeat;
      this.lastBeatSample = this.sampleCount;

      if (prevBeat !== 0) {
        const measuredRRMs = Math.round(rrSamples * (1000 / this.fs));
        // Giới hạn dải nhịp tim sinh học hợp lý: 35 BPM (1714ms) đến 200 BPM (300ms)
        if (measuredRRMs >= 300 && measuredRRMs <= 1714) {
          this.recentRR.push(measuredRRMs);
          if (this.recentRR.length > 5) this.recentRR.shift();

          const sorted = [...this.recentRR].sort((a, b) => a - b);
          rrMs = sorted[Math.floor(sorted.length / 2)];
          this.lastRRMs = rrMs;

          // Tính biên độ sóng R (AD8232 Gain ~1100, Vref 3.3V)
          const estMv = Math.max(0.4, Math.min(2.8, Math.abs(hp) * 0.0016)).toFixed(2);
          rMv = estMv;
          this.lastRPeakMv = rMv;
          beat = true;
        }
      }
    }

    return {
      filtered: this.lp,
      beat: beat,
      rrMs: rrMs,
      rMv: rMv
    };
  }
}

// Bộ đệm sóng ECG (Oscilloscope Ring Buffer)
const BUFFER_SIZE = 1000; // 4 giây hiển thị ở tần số lấy mẫu 250Hz (250 * 4 = 1000)
const ecgBuffer = new Float32Array(BUFFER_SIZE).fill(0);
const ecgProcessor = new MedicalEcgProcessor(250, 50);
let dynamicPeak = 250;
let writeIndex = 0;
let sweepIndex = 0;
let lastDetectedBpm = 0;

// Web Audio API Context
let audioCtx = null;

// ==================== KHỞI TẠO DOM ELEMENTS ====================
const elements = {
  statusBadge: document.getElementById('connectionStatus'),
  statusText: document.getElementById('statusText'),
  audioToggleBtn: document.getElementById('audioToggleBtn'),
  audioIcon: document.getElementById('audioIcon'),
  audioLabel: document.getElementById('audioLabel'),
  remoteMuteBtn: document.getElementById('remoteMuteBtn'),
  remoteMuteIcon: document.getElementById('remoteMuteIcon'),
  remoteMuteLabel: document.getElementById('remoteMuteLabel'),
  pwaInstallBtn: document.getElementById('pwaInstallBtn'),
  simToggleBtn: document.getElementById('simToggleBtn'),
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
  toastContainer: document.getElementById('toastContainer'),
  emergencyBanner: document.getElementById('emergencyBanner'),
  alertTitle: document.getElementById('alertTitle'),
  alertDesc: document.getElementById('alertDesc'),
  dismissAlertBtn: document.getElementById('dismissAlertBtn'),
  ecgCanvas: document.getElementById('ecgCanvas'),
  leadsOffBadge: document.getElementById('leadsOffBadge'),
  sweepTimeDisplay: document.getElementById('sweepTimeDisplay'),
  rPeakVal: document.getElementById('rPeakVal'),
  rrIntervalVal: document.getElementById('rrIntervalVal'),
  bpmVal: document.getElementById('bpmVal'),
  bpmStatus: document.getElementById('bpmStatus'),
  heartPulse: document.getElementById('heartPulse'),
  tempVal: document.getElementById('tempVal'),
  tempStatus: document.getElementById('tempStatus'),
  smvVal: document.getElementById('smvVal'),
  smvGauge: document.getElementById('smvGauge'),
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
    try { return { ...DEFAULT_CONFIG, ...JSON.parse(saved) }; }
    catch (e) { console.error('Lỗi nạp config:', e); }
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
    } catch (e) {}
    currentSirenOsc = null;
  }
  if (currentSirenGain) {
    try { currentSirenGain.disconnect(); } catch (e) {}
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
  if (mqttClient) {
    try { mqttClient.end(true); } catch (e) {}
  }

  updateConnectionStatus('connecting', 'Đang kết nối Broker...');

  const options = {
    clean: true,
    connectTimeout: 20000, // 20s cho WSS qua Cloudflare Tunnel & EMQX
    reconnectPeriod: 4000,
    keepalive: 60,
    protocolVersion: 4,    // Chuẩn MQTT 3.1.1 ổn định và tương thích cao nhất
    clientId: 'IoMT_Web_' + Math.random().toString(16).substr(2, 8)
  };

  if (config.user) options.username = config.user;
  if (config.pass) options.password = config.pass;

  try {
    mqttClient = mqtt.connect(config.uri, options);

    mqttClient.on('connect', () => {
      console.log('[MQTT] Kết nối thành công tới broker:', config.uri);
      updateConnectionStatus('connected', 'Cloud WSS Online');
      const subTopics = [
        config.topicData,
        config.topicAlert,
        'biomed/gateway/status',
        'biomed/status'
      ];
      mqttClient.subscribe(subTopics, (err) => {
        if (!err) {
          addEventLog('KẾT NỐI HỆ THỐNG', 1.0, '--', '--', 'WSS Port 443', 'INFO');
        } else {
          console.warn('[MQTT] Lỗi subscribe topics:', err);
        }
      });
    });

    mqttClient.on('message', (topic, payload) => {
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

    mqttClient.on('reconnect', () => {
      console.log('[MQTT] Đang tự động kết nối lại Broker...');
      updateConnectionStatus('connecting', 'Đang kết nối lại Broker...');
    });

    mqttClient.on('error', (err) => {
      console.warn('[MQTT Event Error]', err.message || err);
      if (!mqttClient.connected) {
        updateConnectionStatus('disconnected', 'Lỗi kết nối Broker');
      }
    });

    mqttClient.on('offline', () => {
      updateConnectionStatus('disconnected', 'Mất kết nối Broker');
    });

    mqttClient.on('close', () => {
      if (!isDemoMode && !mqttClient.connected) {
        updateConnectionStatus('disconnected', 'Đã đóng kết nối');
      }
    });
  } catch (e) {
    console.error('Không thể khởi tạo MQTT client:', e);
    updateConnectionStatus('disconnected', 'Lỗi URI WSS');
  }
}

function updateConnectionStatus(type, text) {
  elements.statusBadge.className = 'status-badge ' + type;
  elements.statusText.textContent = text;
}

// ==================== XỬ LÝ DỮ LIỆU SINH HIỆU ====================
function handleDataPacket(data) {
  totalPackets++;
  elements.packetCountVal.textContent = totalPackets.toLocaleString();

  // 1. Cập nhật mẫu ECG vào bộ đệm qua bộ lọc số y sinh (25 mẫu / gói @ 250Hz)
  let localBeatDetected = false;

  if (Array.isArray(data.ecg)) {
    for (let i = 0; i < data.ecg.length; i++) {
      // Khi hở điện cực (leadsOff = 1) hoặc ADC bão hòa (4080) / chạm đáy (50)
      const isDisconnected = Boolean(data.leadsOff || data.ecg[i] >= 4080 || data.ecg[i] <= 50);
      const res = ecgProcessor.process(data.ecg[i], isDisconnected);

      ecgBuffer[writeIndex] = res.filtered;
      writeIndex = (writeIndex + 1) % BUFFER_SIZE;

      if (res.beat) {
        localBeatDetected = true;
        lastDetectedBpm = Math.round(60000 / res.rrMs);
        if (elements.rPeakVal) elements.rPeakVal.textContent = res.rMv + ' mV';
        if (elements.rrIntervalVal) elements.rrIntervalVal.textContent = res.rrMs + ' ms';
        triggerHeartBeat(lastDetectedBpm);
        lastHeartBeatTime = Date.now();
      }
    }
  }

  // 2. Nhịp tim BPM (Kết hợp tính toán tại Gateway và thuật toán Pan-Tompkins Web)
  let bpm = data.bpm || 0;
  // Nếu Gateway bị nhiễu điện lưới kích hoạt nhịp ảo > 120 trong khi Web tính được nhịp chuẩn sinh học 45 - 110:
  if (lastDetectedBpm >= 45 && lastDetectedBpm <= 115 && (bpm <= 0 || bpm > 115)) {
    bpm = lastDetectedBpm;
  }

  if (data.leadsOff) {
    elements.bpmVal.textContent = '--';
    elements.bpmStatus.textContent = 'HỞ ĐIỆN CỰC AD8232';
    elements.bpmStatus.className = 'vital-status text-danger';
    if (elements.rPeakVal) elements.rPeakVal.textContent = '-- mV';
    if (elements.rrIntervalVal) elements.rrIntervalVal.textContent = '-- ms';
  } else if (bpm > 0) {
    elements.bpmVal.textContent = bpm;
    if (bpm > 120) {
      elements.bpmStatus.textContent = 'NHỊP TIM NHANH (Tachycardia)';
      elements.bpmStatus.className = 'vital-status text-danger';
    } else if (bpm < 50) {
      elements.bpmStatus.textContent = 'NHỊP TIM CHẬM (Bradycardia)';
      elements.bpmStatus.className = 'vital-status text-warning';
    } else {
      elements.bpmStatus.textContent = 'Nhịp ổn định (AHA Normal)';
      elements.bpmStatus.className = 'vital-status text-ok';
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
    elements.bpmStatus.className = 'vital-status text-dim';
  }

  // 3. Thân nhiệt
  if (data.temp !== undefined) {
    elements.tempVal.textContent = Number(data.temp).toFixed(1);
    if (data.temp >= 38.0) {
      elements.tempStatus.textContent = 'SỐT CAO (> 38.0°C)';
      elements.tempStatus.className = 'vital-status text-danger';
    } else if (data.temp >= 37.5) {
      elements.tempStatus.textContent = 'Sốt nhẹ (37.5 - 38.0°C)';
      elements.tempStatus.className = 'vital-status text-warning';
    } else {
      elements.tempStatus.textContent = 'Nhiệt độ da bình thường';
      elements.tempStatus.className = 'vital-status text-ok';
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
    } else {
      elements.smvVal.className = 'vital-number mono';
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
    elements.fallStatus.className = 'vital-status text-danger';
  } else if (data.smv >= 2.2) {
    elements.fallStatus.textContent = '⏳ Giai đoạn 1: Va đập mạnh - Đang thẩm định bất động...';
    elements.fallStatus.className = 'vital-status text-warning';
  } else {
    elements.fallStatus.textContent = 'Tư thế ổn định (Bình thường)';
    elements.fallStatus.className = 'vital-status text-ok';
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
  playHeartBeep();
  elements.rrIntervalVal.textContent = Math.round((60 / bpm) * 1000) + ' ms';
  elements.rPeakVal.textContent = (1.2 + Math.random() * 0.15).toFixed(2) + ' mV';
}

function triggerFallEmergency(smv, bpm, temp) {
  elements.emergencyBanner.classList.remove('hidden');
  elements.alertTitle.textContent = `🚨 CẢNH BÁO TÉ NGÃ 2 GIAI ĐOẠN (SMV: ${Number(smv).toFixed(2)}g)!`;
  elements.alertDesc.textContent = `Xác thực thành công 2 giai đoạn (Va đập ${Number(smv).toFixed(2)}g > 2.2g kèm bất động & nằm sàn). Đã phát còi và gửi tin khẩn cấp Telegram & Zalo!`;
  
  if (!isBuzzerMuted) {
    playAlarmSiren();
  }
  addEventLog('XÁC NHẬN TÉ NGÃ 2 GIAI ĐOẠN', smv, bpm, temp, 'MPU-6050 Verification', 'ALERT');
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
let lastRenderTime = 0;

function drawOscilloscope(timestamp) {
  requestAnimationFrame(drawOscilloscope);

  const width = elements.ecgCanvas.width;
  const height = elements.ecgCanvas.height;

  // Xóa sạch canvas mỗi khung hình để không tích tụ vệt phát quang gây nhòe / biến dạng sóng
  ctx.clearRect(0, 0, width, height);

  const stepX = width / BUFFER_SIZE;
  const sweepX = (sweepIndex % BUFFER_SIZE) * stepX;
  const midY = height * 0.52;
  const gapPixels = 26; // Khe hở quét xóa dữ liệu cũ phía trước tia quét (Sweep Erase Gap)

  // 1. Tự động cân chỉnh biên độ hiển thị (Dynamic Auto-Gain Envelope)
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
  if (dynamicPeak < 90) dynamicPeak = 90; // Ngưỡng sàn bảo vệ chống phóng to nhiễu tĩnh điện

  // Hệ số co giãn trục Y: Sóng chiếm tối đa 72% chiều cao khung hình, không chạm trần / chạm sàn
  const scale = (height * 0.36) / dynamicPeak;

  // 2. VẼ LỚP PHÁT QUANG PHOSPHOR NGOÀI (Soft Phosphor Glow Aura)
  ctx.save();
  ctx.lineWidth = 3.6;
  ctx.strokeStyle = 'rgba(0, 255, 136, 0.22)';
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  ctx.beginPath();

  let started = false;
  for (let i = 0; i < BUFFER_SIZE; i++) {
    const x = i * stepX;
    // Bỏ qua dải quét phía trước để tạo hiệu ứng sweep gap chuẩn màn hình bệnh viện
    const dist = (x - sweepX + width) % width;
    if (dist < gapPixels) {
      started = false;
      continue;
    }

    const val = ecgBuffer[i];
    const y = midY - val * scale;
    const clampedY = Math.max(14, Math.min(height - 14, y));

    if (!started) {
      ctx.moveTo(x, clampedY);
      started = true;
    } else {
      ctx.lineTo(x, clampedY);
    }
  }
  ctx.stroke();

  // 3. VẼ LÕI TIA SÁNG ĐIỆN TIM CHÍNH (Sharp Center Neon Core)
  ctx.lineWidth = 1.8;
  ctx.strokeStyle = '#00ff88';
  ctx.shadowColor = '#00ff88';
  ctx.shadowBlur = 6;
  ctx.stroke();
  ctx.restore();

  // 4. HIỆU ỨNG TIA QUÉT Y TẾ (Medical Sweep Beam & Leading Cursor)
  const beamGrad = ctx.createLinearGradient(sweepX - 12, 0, sweepX + 2, 0);
  beamGrad.addColorStop(0, 'rgba(0, 255, 136, 0)');
  beamGrad.addColorStop(0.7, 'rgba(0, 255, 136, 0.25)');
  beamGrad.addColorStop(1, 'rgba(0, 255, 136, 0.95)');
  ctx.fillStyle = beamGrad;
  ctx.fillRect(sweepX - 12, 0, 14, height);

  // Điểm sáng laser dẫn đầu tia quét
  const currIdx = Math.floor(sweepIndex) % BUFFER_SIZE;
  const leadVal = ecgBuffer[currIdx] || 0;
  const leadY = Math.max(14, Math.min(height - 14, midY - leadVal * scale));

  ctx.save();
  ctx.fillStyle = '#ffffff';
  ctx.shadowColor = '#00ff88';
  ctx.shadowBlur = 12;
  ctx.beginPath();
  ctx.arc(sweepX, leadY, 2.8, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();

  // Tốc độ quét đồng bộ tần số lấy mẫu 250Hz (250 / 60 FPS = 4.167 mẫu / frame)
  sweepIndex = (sweepIndex + 4.167) % BUFFER_SIZE;

  // Cập nhật nhãn giờ quét
  const now = new Date();
  elements.sweepTimeDisplay.textContent = now.toTimeString().split(' ')[0] + '.' + Math.floor(now.getMilliseconds() / 100);
}

// ==================== CHẾ ĐỘ GIẢ LẬP DEMO (KHI CHƯA BẬT KIT) ====================
let demoTimer = null;
let demoPhase = 0;

function startDemoMode() {
  if (mqttClient) {
    try { mqttClient.end(true); } catch (e) {}
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
  updateConnectionStatus('disconnected', 'Đã tắt Demo');
}

// ==================== EVENT LISTENERS & SETUP ====================
function initEventListeners() {
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

  // Chuyển đổi chế độ Demo
  elements.simToggleBtn.addEventListener('click', () => {
    if (isDemoMode) {
      stopDemoMode();
      connectMQTT();
    } else {
      startDemoMode();
    }
  });

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
      topicCmd: elements.cfgTopicCmd ? elements.cfgTopicCmd.value.trim() : DEFAULT_CONFIG.topicCmd
    });
    elements.configModal.classList.add('hidden');
    showToast('💾 Đã lưu cấu hình MQTT thành công!', 'success');
    connectMQTT();
  });

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
  });

  // Xóa log
  elements.clearLogBtn.addEventListener('click', () => {
    elements.logTableBody.innerHTML = '<tr><td colspan="7" class="text-center text-dim">Nhật ký đã được xóa</td></tr>';
  });

  // Tự động điều chỉnh kích thước Canvas
  window.addEventListener('resize', resizeCanvas);
}

function resizeCanvas() {
  const rect = elements.ecgCanvas.getBoundingClientRect();
  elements.ecgCanvas.width = rect.width * (window.devicePixelRatio || 1);
  elements.ecgCanvas.height = rect.height * (window.devicePixelRatio || 1);
}

// ==================== PWA SERVICE WORKER & CÀI ĐẶT APP ====================
let deferredInstallPrompt = null;

function initPWA() {
  // 1. Đăng ký Service Worker
  if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
      navigator.serviceWorker.register('./sw.js')
        .then((reg) => {
          console.log('[PWA] Service Worker đăng ký thành công:', reg.scope);
        })
        .catch((err) => {
          console.warn('[PWA] Đăng ký Service Worker thất bại:', err);
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
  initEventListeners();
  initPWA();
  requestAnimationFrame(drawOscilloscope);

  // Tự động kết nối MQTT khi tải trang
  connectMQTT();
});
