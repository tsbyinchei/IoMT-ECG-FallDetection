#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
==============================================================================
CẦU NỐI DỮ LIỆU Y TẾ: MQTT BROKER -> INFLUXDB V2 TIME-SERIES DATABASE
==============================================================================
Đề tài: Hệ thống IoT theo dõi điện tim, thân nhiệt và cảnh báo té ngã từ xa
Tác giả: TsByinChei (contact@tsbyin.dev) • GitHub: @tsbyinchei

Nguyên lý hoạt động:
1. Đăng ký nhận bản tin sinh hiệu từ EMQX Broker:
   - 'biomed/patient/data': Bản tin vi chu kỳ (25 mẫu ECG / 100ms kèm HR, Temp, SMV)
   - 'biomed/patient/alert': Bản tin cảnh báo khẩn cấp té ngã hoặc tuột điện cực
   - 'biomed/gateway/event_snapshot': Dải sóng 60 giây khi có biến cố ngã
2. Phân tầng lưu trữ theo mô hình Tiered Storage:
   - Tầng 1 (biomed_metrics): Gom mẫu mỗi 5 giây ghi các chỉ số (HR, Temp, SMV, LeadsOff)
     vào InfluxDB (Retention: 365 ngày, dung lượng chỉ ~1.5 MB/ngày/bệnh nhân).
   - Tầng 2 (biomed_events): Khi phát hiện té ngã hoặc nhận event_snapshot, ghi bản ghi
     chất lượng cao chứa dải sóng thô vào InfluxDB phục vụ Web Dashboard tua lại (Replay).

Yêu cầu thư viện:
    pip install paho-mqtt influxdb-client
==============================================================================
"""

import sys
import json
import time
import argparse
import logging
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("[ERROR] Thư viện 'paho-mqtt' chưa được cài đặt. Vui lòng chạy: pip install paho-mqtt")
    sys.exit(1)

try:
    from influxdb_client import InfluxDBClient, Point, WritePrecision
    from influxdb_client.client.write_api import SYNCHRONOUS
except ImportError:
    print("[ERROR] Thư viện 'influxdb-client' chưa được cài đặt. Vui lòng chạy: pip install influxdb-client")
    sys.exit(1)

# Cấu hình logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger("MQTT-InfluxDB-Bridge")

# ==================== CẤU HÌNH MẶC ĐỊNH ====================
DEFAULT_CONFIG = {
    # Cấu hình MQTT Broker
    "mqtt_host": "mqtt.tsbyin.dev",
    "mqtt_port": 1883,           # Hoặc 8883 (TLS), hoặc dùng 1883 nội bộ LAN/Docker
    "mqtt_user": "TsByin",
    "mqtt_pass": "Chei@@@182728",
    "topic_data": "biomed/patient/data",
    "topic_alert": "biomed/patient/alert",
    "topic_snapshot": "biomed/gateway/event_snapshot",
    
    # Cấu hình InfluxDB v2
    "influx_url": "http://localhost:8086",
    "influx_token": "biomed_super_secret_admin_token_2026",
    "influx_org": "biomed",
    "bucket_metrics": "biomed_metrics",
    "bucket_events": "biomed_events",
    
    # Chu kỳ gom mẫu tầng 1 (giây)
    "aggregation_interval": 5.0,
    "patient_id": "BN01",
    "device_id": "ESP32_GW"
}


class BiomedicalDataBridge:
    def __init__(self, cfg):
        self.cfg = cfg
        self.influx_client = None
        self.write_api = None
        self.mqtt_client = None
        
        # Bộ đệm tích lũy số liệu 5 giây
        self.buffer_hr = []
        self.buffer_temp = []
        self.buffer_smv = []
        self.last_lead_state = 0
        self.last_flush_time = time.time()
        
        self.total_packets_received = 0
        self.total_metrics_flushed = 0
        self.total_events_saved = 0

    def init_influxdb(self):
        """Khởi tạo kết nối tới máy chủ InfluxDB v2"""
        logger.info(f"Đang kết nối InfluxDB tại: {self.cfg['influx_url']} (Org: {self.cfg['influx_org']})")
        try:
            self.influx_client = InfluxDBClient(
                url=self.cfg['influx_url'],
                token=self.cfg['influx_token'],
                org=self.cfg['influx_org'],
                timeout=10000
            )
            self.write_api = self.influx_client.write_api(write_options=SYNCHRONOUS)
            
            # Kiểm tra trạng thái máy chủ InfluxDB
            health = self.influx_client.health()
            if health.status == "pass":
                logger.info("✅ Kết nối InfluxDB v2 thành công! (Trạng thái: pass)")
            else:
                logger.warning(f"⚠️ InfluxDB phản hồi trạng thái: {health.status} - {health.message}")
        except Exception as e:
            logger.error(f"❌ Lỗi kết nối InfluxDB: {e}")
            logger.info("👉 Gợi ý: Hãy đảm bảo Docker InfluxDB đang chạy (docker compose up -d)")

    def init_mqtt(self):
        """Khởi tạo kết nối MQTT Client"""
        client_id = f"biomed_bridge_{int(time.time())}"
        self.mqtt_client = mqtt.Client(client_id=client_id, protocol=mqtt.MQTTv311)
        
        if self.cfg.get('mqtt_user'):
            self.mqtt_client.username_pw_set(self.cfg['mqtt_user'], self.cfg.get('mqtt_pass', ''))
            
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        self.mqtt_client.on_disconnect = self.on_mqtt_disconnect
        
        logger.info(f"Đang kết nối MQTT Broker tại {self.cfg['mqtt_host']}:{self.cfg['mqtt_port']}...")
        try:
            self.mqtt_client.connect(self.cfg['mqtt_host'], self.cfg['mqtt_port'], keepalive=60)
            self.mqtt_client.loop_start()
        except Exception as e:
            logger.error(f"❌ Không thể kết nối MQTT Broker: {e}")

    def on_mqtt_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logger.info("✅ Kết nối MQTT Broker thành công!")
            topics = [
                (self.cfg['topic_data'], 0),
                (self.cfg['topic_alert'], 1),
                (self.cfg['topic_snapshot'], 1)
            ]
            client.subscribe(topics)
            logger.info(f"Đã subscribe các topic: {[t[0] for t in topics]}")
        else:
            logger.error(f"❌ Kết nối MQTT thất bại với mã lỗi rc = {rc}")

    def on_mqtt_disconnect(self, client, userdata, rc):
        if rc != 0:
            logger.warning(f"⚠️ Mất kết nối MQTT Broker (rc = {rc}). Đang tự động kết nối lại...")

    def on_mqtt_message(self, client, userdata, msg):
        self.total_packets_received += 1
        topic = msg.topic
        payload_str = msg.payload.decode('utf-8', errors='ignore')
        
        try:
            payload = json.loads(payload_str)
        except Exception as e:
            logger.warning(f"Bỏ qua gói tin không đúng định dạng JSON trên topic {topic}: {e}")
            return

        # 1. Xử lý bản tin sinh hiệu thời gian thực (biomed/patient/data)
        if topic == self.cfg['topic_data']:
            self.handle_telemetry_packet(payload)

        # 2. Xử lý bản tin cảnh báo khẩn cấp (biomed/patient/alert)
        elif topic == self.cfg['topic_alert']:
            self.handle_alert_packet(payload)

        # 3. Xử lý bản tin bắt vết biến cố dải sóng 60s (biomed/gateway/event_snapshot)
        elif topic == self.cfg['topic_snapshot']:
            self.handle_snapshot_packet(payload)

    def handle_telemetry_packet(self, data):
        """Xử lý và tích lũy số liệu vào bộ đệm gom mẫu 5s"""
        hr = data.get('bpm', data.get('hr'))
        temp = data.get('temp')
        smv = data.get('smv')
        lead = data.get('leadsOff', data.get('lead', 0))
        fall = data.get('fall', 0)

        if hr and hr > 30 and hr < 240:
            self.buffer_hr.append(hr)
        if temp and temp > 20 and temp < 45:
            self.buffer_temp.append(temp)
        if smv is not None:
            self.buffer_smv.append(smv)
            
        self.last_lead_state = lead

        # Nếu phát hiện cờ té ngã trong gói dữ liệu, ghi ngay 1 bản ghi cảnh báo
        if fall == 1:
            logger.warning(f"🚨 Phát hiện cờ ngã trong gói sinh hiệu (SMV: {smv}g, HR: {hr}) -> Ghi InfluxDB!")
            self.write_event_point(
                event_type="FALL",
                severity="CRITICAL",
                peak_smv=float(smv) if smv else 3.0,
                avg_hr=int(hr) if hr else 100,
                ecg_data=data.get('ecg', [])
            )

        # Kiểm tra chu kỳ xả đệm 5 giây
        now = time.time()
        if now - self.last_flush_time >= self.cfg['aggregation_interval']:
            self.flush_metrics_to_influx()
            self.last_flush_time = now

    def flush_metrics_to_influx(self):
        """Tính giá trị trung bình trong 5s và ghi vào bucket 'biomed_metrics'"""
        if not self.buffer_hr and not self.buffer_temp and not self.buffer_smv:
            return

        avg_hr = round(sum(self.buffer_hr) / len(self.buffer_hr)) if self.buffer_hr else 75
        avg_temp = round(sum(self.buffer_temp) / len(self.buffer_temp), 1) if self.buffer_temp else 36.5
        peak_smv = round(max(self.buffer_smv), 2) if self.buffer_smv else 1.0

        # Làm rỗng bộ đệm
        self.buffer_hr.clear()
        self.buffer_temp.clear()
        self.buffer_smv.clear()

        if not self.write_api:
            return

        try:
            point = Point("biomed_metrics") \
                .tag("patient_id", self.cfg['patient_id']) \
                .tag("device", self.cfg['device_id']) \
                .field("hr", int(avg_hr)) \
                .field("temp", float(avg_temp)) \
                .field("smv", float(peak_smv)) \
                .field("leads_off", int(self.last_lead_state)) \
                .time(datetime.utcnow(), WritePrecision.NS)

            self.write_api.write(bucket=self.cfg['bucket_metrics'], org=self.cfg['influx_org'], record=point)
            self.total_metrics_flushed += 1
            logger.info(f"[TSDB Flush 5s] HR: {avg_hr} BPM | Temp: {avg_temp}°C | Peak SMV: {peak_smv}g -> biomed_metrics")
        except Exception as e:
            logger.error(f"Lỗi ghi metrics vào InfluxDB: {e}")

    def handle_alert_packet(self, data):
        """Xử lý bản tin cảnh báo khẩn cấp"""
        alert_type = data.get('alert', 'unknown')
        logger.warning(f"🚨 Nhận bản tin cảnh báo khẩn cấp: {data}")
        self.write_event_point(
            event_type=alert_type.upper(),
            severity="CRITICAL",
            peak_smv=data.get('smv', 3.0),
            avg_hr=data.get('hr', 100),
            ecg_data=[]
        )

    def handle_snapshot_packet(self, data):
        """Xử lý dải sóng 60s snapshot lưu vết biến cố"""
        logger.info(f"💾 Nhận gói Snapshot dải sóng biến cố ({len(data.get('samples', []))} mẫu)")
        self.write_event_point(
            event_type=data.get('type', 'FALL_SNAPSHOT'),
            severity="CRITICAL",
            peak_smv=data.get('peak_smv', 3.2),
            avg_hr=data.get('avg_hr', 110),
            ecg_data=data.get('samples', [])
        )

    def write_event_point(self, event_type, severity, peak_smv, avg_hr, ecg_data):
        """Ghi sự kiện lâm sàng vào bucket 'biomed_events'"""
        if not self.write_api:
            return

        try:
            # Chuyển đổi mảng ECG thành chuỗi JSON rút gọn để lưu trữ trong trường InfluxDB
            ecg_json = json.dumps(ecg_data) if ecg_data else "[]"
            
            point = Point("biomed_events") \
                .tag("patient_id", self.cfg['patient_id']) \
                .tag("event_type", event_type) \
                .tag("severity", severity) \
                .field("peak_smv", float(peak_smv)) \
                .field("avg_hr", int(avg_hr)) \
                .field("sample_count", int(len(ecg_data))) \
                .field("ecg_samples", ecg_json) \
                .time(datetime.utcnow(), WritePrecision.NS)

            self.write_api.write(bucket=self.cfg['bucket_events'], org=self.cfg['influx_org'], record=point)
            self.total_events_saved += 1
            logger.info(f"✅ Đã ghi thành công biến cố [{event_type}] vào bucket '{self.cfg['bucket_events']}'!")
        except Exception as e:
            logger.error(f"Lỗi ghi biến cố vào InfluxDB: {e}")

    def run(self):
        """Vòng lặp chạy chính của dịch vụ Bridge"""
        self.init_influxdb()
        self.init_mqtt()
        logger.info("🚀 Dịch vụ Biomedical MQTT-InfluxDB Bridge đang hoạt động. Nhấn Ctrl+C để dừng.")
        
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info("Đang dừng dịch vụ theo yêu cầu người dùng...")
            if self.mqtt_client:
                self.mqtt_client.loop_stop()
                self.mqtt_client.disconnect()
            if self.influx_client:
                self.influx_client.close()
            logger.info("Dịch vụ đã tắt an toàn.")


def main():
    parser = argparse.ArgumentParser(description="Biomedical MQTT to InfluxDB Bridge")
    parser.add_argument("--mqtt-host", default=DEFAULT_CONFIG["mqtt_host"], help="Địa chỉ MQTT Broker")
    parser.add_argument("--mqtt-port", type=int, default=DEFAULT_CONFIG["mqtt_port"], help="Cổng MQTT Broker")
    parser.add_argument("--influx-url", default=DEFAULT_CONFIG["influx_url"], help="URL máy chủ InfluxDB v2")
    parser.add_argument("--influx-token", default=DEFAULT_CONFIG["influx_token"], help="API Token InfluxDB")
    args = parser.parse_args()

    cfg = DEFAULT_CONFIG.copy()
    cfg["mqtt_host"] = args.mqtt_host
    cfg["mqtt_port"] = args.mqtt_port
    cfg["influx_url"] = args.influx_url
    cfg["influx_token"] = args.influx_token

    bridge = BiomedicalDataBridge(cfg)
    bridge.run()


if __name__ == "__main__":
    main()
