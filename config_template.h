#ifndef CONFIG_SECRETS_H
#define CONFIG_SECRETS_H

// ═══════════════════════════════════════════════════════════════════════════════
//                              WIFI & MQTT CONFIGURATION SECRETS
// ═══════════════════════════════════════════════════════════════════════════════
//
// This file is created from config_template.h and contains your actual credentials
// This file is included in .gitignore and will not be committed to the repository
//

// WiFi Configuration - Replace with your actual WiFi credentials
#define CONFIG_WIFI_SSID "YOUR_WIFI_SSID_HERE"
#define CONFIG_WIFI_PASSWORD "YOUR_WIFI_PASSWORD_HERE"

// MQTT Configuration - Replace with your MQTT server details
#define CONFIG_MQTT_SERVER "YOUR_MQTT_SERVER_IP_HERE"  // e.g., "192.168.1.100"

// OTA Configuration - Replace with your preferred OTA password
#define CONFIG_OTA_PASSWORD "YOUR_OTA_PASSWORD_HERE"

#endif // CONFIG_SECRETS_H