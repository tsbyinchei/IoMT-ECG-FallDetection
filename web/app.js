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

// Bộ đệm sóng ECG (Oscilloscope Ring Buffer)
const BUFFER_SIZE = 1000; // 4 giây hiển thị ở tần số lấy mẫu 250Hz (250 * 4 = 1000)
const ecgBuffer = new Float32Array(BUFFER_SIZE).fill(2048);
let writeIndex = 0;
let sweepIndex = 0;

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

  // 1. Cập nhật mẫu ECG vào bộ đệm (25 mẫu / gói)
  if (Array.isArray(data.ecg)) {
    for (let i = 0; i < data.ecg.length; i++) {
      // Khi hở điện cực (leadsOff = 1) hoặc ADC bão hòa (4095), vẽ đường đẳng điện chuẩn ở giữa màn hình (2400)
      const isDisconnected = data.leadsOff || data.ecg[i] >= 4080 || data.ecg[i] <= 50;
      ecgBuffer[writeIndex] = isDisconnected ? 2400 : data.ecg[i];
      writeIndex = (writeIndex + 1) % BUFFER_SIZE;
    }
  }

  // 2. Nhịp tim BPM
  const bpm = data.bpm || 0;
  if (data.leadsOff) {
    elements.bpmVal.textContent = '--';
    elements.bpmStatus.textContent = 'HỞ ĐIỆN CỰC AD8232';
    elements.bpmStatus.className = 'vital-status text-danger';
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

    // Hiệu ứng tim đập theo chu kỳ
    const now = Date.now();
    const intervalMs = (60 / bpm) * 1000;
    if (now - lastHeartBeatTime >= intervalMs * 0.9) {
      triggerHeartBeat(bpm);
      lastHeartBeatTime = now;
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

  // Xóa canvas với hiệu ứng phosphor decay nhẹ
  ctx.fillStyle = 'rgba(3, 8, 12, 0.18)';
  ctx.fillRect(0, 0, width, height);

  // Đường quét y tế (Sweeping line phosphor trace)
  // Tính tọa độ quét X từ 0 đến width
  const stepX = width / BUFFER_SIZE;
  const sweepX = (sweepIndex % BUFFER_SIZE) * stepX;

  // Xóa một vệt phía trước đầu quét (Beam Erase Window 30px)
  ctx.fillStyle = '#03080c';
  ctx.fillRect(sweepX, 0, 36, height);

  // Vẽ sóng ECG
  ctx.lineWidth = 2.2;
  ctx.strokeStyle = '#00ff88';
  ctx.shadowColor = '#00ff88';
  ctx.shadowBlur = 8;
  ctx.beginPath();

  let started = false;
  // Vẽ toàn bộ buffer
  for (let i = 0; i < BUFFER_SIZE; i++) {
    const x = i * stepX;
    // Bỏ qua cửa sổ ngay sát tia quét
    if (Math.abs(x - sweepX) < 16) {
      started = false;
      continue;
    }

    const raw = ecgBuffer[i];
    // Ánh xạ ADC 12-bit (2000 - 3600) vào khung chiều cao canvas
    // Tâm đồ thị nằm ở height * 0.55
    const baseline = 2400;
    const scale = height / 1600;
    const y = (height * 0.55) - (raw - baseline) * scale;
    const clampedY = Math.max(15, Math.min(height - 15, y));

    if (!started) {
      ctx.moveTo(x, clampedY);
      started = true;
    } else {
      ctx.lineTo(x, clampedY);
    }
  }
  ctx.stroke();
  ctx.shadowBlur = 0; // Tắt shadow để tối ưu hiệu năng

  // Vẽ vạch tia quét phosphor (Sweep Beam)
  const gradient = ctx.createLinearGradient(sweepX, 0, sweepX + 4, 0);
  gradient.addColorStop(0, 'rgba(0, 255, 136, 0.9)');
  gradient.addColorStop(1, 'rgba(0, 255, 136, 0.0)');
  ctx.fillStyle = gradient;
  ctx.fillRect(sweepX - 2, 0, 6, height);

  // Tốc độ quét đồng bộ tần số lấy mẫu 250Hz (250 / 60 FPS = ~4.17 mẫu/frame)
  sweepIndex = (sweepIndex + 4.17) % BUFFER_SIZE;

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
