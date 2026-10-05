import os
import re
import sys
import time
import json
import urllib.request
import urllib.parse

def load_bot_token():
    for arg in sys.argv[1:]:
        if ":" in arg and len(arg) > 20:
            return arg
    secrets_path = os.path.join(os.path.dirname(__file__), "..", "ESP32", "secrets.h")
    if os.path.exists(secrets_path):
        with open(secrets_path, "r", encoding="utf-8", errors="ignore") as f:
            m = re.search(r'#define\s+ZALO_BOT_TOKEN\s+"([^"]+)"', f.read())
            if m and "YOUR_" not in m.group(1):
                return m.group(1)
    return "YOUR_ZALO_BOT_TOKEN"

BOT_TOKEN = load_bot_token()
BASE_URL = f"https://bot-api.zaloplatforms.com/bot{BOT_TOKEN}"

def get_me():
    try:
        url = f"{BASE_URL}/getMe"
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=10) as response:
            res = json.loads(response.read().decode())
            print("[INFO] Bot Info:", json.dumps(res, indent=2, ensure_ascii=False))
            return res
    except Exception as e:
        print("[ERROR] Cannot connect to getMe:", e)
        return None

def poll_updates(timeout_sec=30):
    print(f"\n[*] Dang lang nghe tin nhan gui toi Bot trong {timeout_sec}s...")
    print("[*] Vui long mo Zalo, vao chat voi Bot (hoac nhom da them Bot) va gui 1 tin nhan bat ky (vi du: 'alo' hoac 'start')...")
    url = f"{BASE_URL}/getUpdates"
    req = urllib.request.Request(url)
    try:
        with urllib.request.urlopen(req, timeout=timeout_sec) as response:
            data = json.loads(response.read().decode())
            print("[INFO] Ket qua getUpdates:\n", json.dumps(data, indent=2, ensure_ascii=False))
            if data.get("ok") and data.get("result"):
                results = data["result"]
                if isinstance(results, dict):
                    results = [results]
                for update in results:
                    msg = update.get("message", {})
                    chat = msg.get("chat", {})
                    chat_id = chat.get("id") or msg.get("from", {}).get("id")
                    user_name = msg.get("from", {}).get("display_name", "") or chat.get("title", "")
                    text = msg.get("text", "")
                    print(f"\n>>> TIM THAY CHAT_ID: {chat_id} (Tu: {user_name}, Noi dung: '{text}') <<<")
                    return chat_id
            else:
                print("Chua co tin nhan nao duoc ghi nhan.")
    except Exception as e:
        print("[TIMEOUT/ERROR]", e)
    return None

def send_test_message(chat_id, text="Xin chao tu IoMT Biomedical Gateway!"):
    print(f"\n[*] Dang gui tin nhan thu nghiem toi chat_id: {chat_id}...")
    url = f"{BASE_URL}/sendMessage?chat_id={chat_id}&text=" + urllib.parse.quote(text)
    try:
        req = urllib.request.Request(url)
        with urllib.request.urlopen(req, timeout=10) as response:
            res = json.loads(response.read().decode())
            print("[SUCCESS] Ket qua gui tin nhan:", json.dumps(res, indent=2, ensure_ascii=False))
            return res
    except Exception as e:
        print("[ERROR] Gui tin nhan that bai:", e)
        return None

if __name__ == "__main__":
    get_me()
    if len(sys.argv) > 1:
        target_id = sys.argv[1]
        send_test_message(target_id)
    else:
        chat_id = poll_updates(timeout_sec=15)
        if chat_id:
            send_test_message(chat_id, "🚨 [IoMT Gateway] Ket noi Zalo Bot thanh cong! He thong san sang canh bao te nga.")
