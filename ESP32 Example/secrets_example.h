#ifndef SECRETS_EXAMPLE_H
#define SECRETS_EXAMPLE_H

// Mẫu cấu hình Wi-Fi Router
#define SECRET_ROUTER_SSID "YOUR_WIFI_SSID"
#define SECRET_ROUTER_PASS "YOUR_WIFI_PASSWORD"

// Mẫu cấu hình MQTT Broker qua Cloudflare Tunnel (Toàn cầu)
#define SECRET_MQTT_URI "wss://mqtt.yourdomain.com/mqtt"
#define SECRET_MQTT_SERVER "mqtt.yourdomain.com"
#define SECRET_MQTT_PORT 443
#define SECRET_MQTT_USER "YOUR_MQTT_USERNAME"
#define SECRET_MQTT_PASS "YOUR_MQTT_PASSWORD"

// Mẫu cấu hình Telegram Bot (Điền Token và Chat ID của bạn vào file secrets.h)
#define TELEGRAM_BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
#define TELEGRAM_CHAT_ID "YOUR_TELEGRAM_CHAT_ID"

#endif // SECRETS_EXAMPLE_H
