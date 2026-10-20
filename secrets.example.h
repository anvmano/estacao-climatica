#ifndef SECRETS_H
#define SECRETS_H

// Wi-Fi
const char WIFI_SSID[] = "SEU_WIFI_SSID";
const char WIFI_PASSWORD[] = "SUA_SENHA_WIFI";

// OpenWeather
const char OPENWEATHER_API_KEY[] = "SUA_CHAVE_OPENWEATHER";

// Telegram BOT
const char BOT_TOKEN[] = "SEU_TOKEN_TELEGRAM";
const char* const VALID_SENDER_IDS[] = { "SEU_ID_TELEGRAM_1", "SEU_ID_TELEGRAM_2", "SEU_ID_TELEGRAM_3" };
const size_t VALID_SENDER_ID_COUNT = sizeof(VALID_SENDER_IDS) / sizeof(VALID_SENDER_IDS[0]);

// Firebase
const char FIREBASE_HOST[] = "https://SEU_PROJETO.firebaseio.com";
const char FIREBASE_AUTH[] = "SEU_TOKEN_LEGADO_FIREBASE";

// Localizacao padrao
const double DEFAULT_LATITUDE = 0.0;
const double DEFAULT_LONGITUDE = 0.0;
const double DEFAULT_ALTITUDE = 0.0;

#endif
