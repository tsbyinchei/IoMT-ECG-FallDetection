#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IoMT Biomedical System - Dual-Channel Notification Test (Telegram & Zalo)
Kiểm tra tín hiệu gửi cảnh báo đồng thời đến Telegram Bot và Zalo Bot Platform.
"""

import os
import re
import sys
import time
import json
import urllib.request
import urllib.parse
from datetime import datetime

# Đảm bảo in tiếng Việt & emoji trên mọi phiên bản Windows Console
if sys.platform.startswith("win"):
    try:
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
        sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

def load_credentials():
    """
    Đọc thông tin bí mật từ ESP32/secrets.h nếu có.
    """
    creds = {
        "TELEGRAM_BOT_TOKEN": "",
        "TELEGRAM_CHAT_ID": "",
        "ZALO_BOT_TOKEN": "",
        "ZALO_CHAT_ID": ""
    }
    secrets_path = os.path.join(os.path.dirname(__file__), "..", "ESP32", "secrets.h")
    if os.path.exists(secrets_path):
        with open(secrets_path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
            for key in creds.keys():
                m = re.search(rf'#define\s+{key}\s+"([^"]*)"', content)
                if m and "YOUR_" not in m.group(1):
                    creds[key] = m.group(1)
    
    # Cho phép override qua biến môi trường
    for key in creds.keys():
        if os.getenv(key):
            creds[key] = os.getenv(key)
            
    return creds

def send_telegram(token, chat_id, text):
    """
    Gửi tin nhắn qua Telegram Bot API (HTTPS Port 443).
    """
    if not token or not chat_id:
        return False, 0, "Chưa cấu hình TELEGRAM_BOT_TOKEN hoặc TELEGRAM_CHAT_ID"
    
    url = f"https://api.telegram.org/bot{token}/sendMessage?chat_id={chat_id}&text=" + urllib.parse.quote(text)
    start_t = time.time()
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "ESP32-IoMT-Gateway"})
        with urllib.request.urlopen(req, timeout=8) as resp:
            latency = (time.time() - start_t) * 1000
            data = json.loads(resp.read().decode())
            if data.get("ok"):
                msg_id = data.get("result", {}).get("message_id")
                return True, latency, f"Message ID: {msg_id}"
            else:
                return False, latency, data.get("description", "Unknown error")
    except Exception as e:
        latency = (time.time() - start_t) * 1000
        return False, latency, str(e)

def send_zalo(token, chat_id, text):
    """
    Gửi tin nhắn qua Zalo Bot Platform HTTP API (HTTPS Port 443).
    """
    if not token or not chat_id:
        return False, 0, "Chưa cấu hình ZALO_BOT_TOKEN hoặc ZALO_CHAT_ID"
    
    url = f"https://bot-api.zaloplatforms.com/bot{token}/sendMessage?chat_id={chat_id}&text=" + urllib.parse.quote(text)
    start_t = time.time()
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "ESP32-IoMT-Gateway"})
        with urllib.request.urlopen(req, timeout=8) as resp:
            latency = (time.time() - start_t) * 1000
            data = json.loads(resp.read().decode())
            if data.get("ok"):
                msg_id = data.get("result", {}).get("message_id")
                return True, latency, f"Message ID: {msg_id}"
            else:
                return False, latency, data.get("description", "Unknown error")
    except Exception as e:
        latency = (time.time() - start_t) * 1000
        return False, latency, str(e)

def main():
    print("=" * 65)
    print("  🩺 IoMT WEARABLE SYSTEM - KIỂM TRA TÍN HIỆU ĐA KÊNH")
    print("  Đồng bộ cảnh báo tức thời: Telegram Bot & Zalo Bot Platform")
    print("=" * 65)

    creds = load_credentials()
    
    print("\n[*] Thông tin cấu hình nhận diện:")
    print(f"  • Telegram Chat ID : {creds['TELEGRAM_CHAT_ID'] or '[CHƯA CÓ]'}")
    print(f"  • Zalo Chat ID     : {creds['ZALO_CHAT_ID'] or '[CHƯA CÓ]'}")
    
    now_str = datetime.now().strftime("%H:%M:%S - %d/%m/%Y")
    
    # Kiểm tra xem người dùng có truyền tham số tin nhắn tùy chỉnh không
    if len(sys.argv) > 1 and sys.argv[1] != "--check":
        custom_content = " ".join(sys.argv[1:])
        check_msg = (
            f"🔔 [IoMT GATEWAY - THỬ NGHIỆM ĐA KÊNH]\n"
            f"Thời điểm: {now_str}\n"
            f"Nội dung: {custom_content}\n"
            f"Hệ thống: Kênh truyền Telegram & Zalo hoạt động tốt!"
        )
    else:
        check_msg = (
            f"🩺 [IoMT SYSTEM - TÍN HIỆU KIỂM TRA ĐA KÊNH]\n"
            f"⏱️ Thời gian: {now_str}\n"
            f"📡 Trạng thái: Gateway ESP32 Online & Kết nối ổn định\n"
            f"━━━━━━━━━━━━━━━━━━\n"
            f"📊 Chỉ số sinh hiệu mô phỏng:\n"
            f"  • Nhịp tim: 78 BPM (Bình thường)\n"
            f"  • Thân nhiệt: 36.6°C (Bình thường)\n"
            f"  • Gia tốc SMV: 1.02g (Tư thế đứng / đi lại)\n"
            f"  • Điện cực ECG: Đang gắn tiếp xúc tốt\n"
            f"━━━━━━━━━━━━━━━━━━\n"
            f"✅ Kênh nhận cảnh báo: Telegram Bot + Zalo Bot\n"
            f"🛡️ Sẵn sàng phát hiện và báo động té ngã khẩn cấp!"
        )

    print("\n[*] Đang gửi tín hiệu kiểm tra song song tới cả 2 nền tảng...")
    
    # 1. Gửi Telegram
    print("  --> [1/2] Đang gửi Telegram Bot...", end="", flush=True)
    tele_ok, tele_lat, tele_res = send_telegram(
        creds["TELEGRAM_BOT_TOKEN"], creds["TELEGRAM_CHAT_ID"], check_msg
    )
    if tele_ok:
        print(f" [THÀNH CÔNG] ({tele_lat:.0f}ms) | {tele_res}")
    else:
        print(f" [THẤT BẠI] ({tele_lat:.0f}ms) | Lỗi: {tele_res}")

    # 2. Gửi Zalo
    print("  --> [2/2] Đang gửi Zalo Bot...", end="", flush=True)
    zalo_ok, zalo_lat, zalo_res = send_zalo(
        creds["ZALO_BOT_TOKEN"], creds["ZALO_CHAT_ID"], check_msg
    )
    if zalo_ok:
        print(f" [THÀNH CÔNG] ({zalo_lat:.0f}ms) | {zalo_res}")
    else:
        print(f" [THẤT BẠI] ({zalo_lat:.0f}ms) | Lỗi: {zalo_res}")

    print("\n" + "=" * 65)
    if tele_ok and zalo_ok:
        print("🎉 HOÀN TẤT: Cả 2 kênh Telegram và Zalo đều đã nhận được tín hiệu!")
    elif tele_ok or zalo_ok:
        print("⚠️ CẢNH BÁO: Chỉ có 1 trong 2 kênh nhận được tín hiệu. Vui lòng kiểm tra lại token/chat_id.")
    else:
        print("❌ LỖI: Cả 2 kênh đều không gửi được tín hiệu. Vui lòng kiểm tra kết nối mạng và Token.")
    print("=" * 65)

if __name__ == "__main__":
    main()
