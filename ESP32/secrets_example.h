#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

// Mẫu cấu hình Wi-Fi Router
#define SECRET_ROUTER_SSID "YOUR_WIFI_SSID"
#define SECRET_ROUTER_PASS "YOUR_WIFI_PASSWORD"

// Mẫu cấu hình MQTT Broker (1Panel Ubuntu Server: EMQX / Mosquitto)
#define SECRET_MQTT_SERVER "192.168.1.36"
#define SECRET_MQTT_PORT   1883
#define SECRET_MQTT_USER   ""
#define SECRET_MQTT_PASS   ""

// Mẫu cấu hình Telegram Bot (Điền Token và Chat ID của bạn vào file secrets.h)
#define TELEGRAM_BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
#define TELEGRAM_CHAT_ID   "YOUR_TELEGRAM_CHAT_ID"

#endif // SECRETS_EXAMPLE_H
