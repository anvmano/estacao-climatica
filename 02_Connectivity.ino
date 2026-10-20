// Garante conexao Wi-Fi antes de chamadas HTTP, Firebase e Telegram.
void conectarWifi() {
  clienteSeguro.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  clienteHttps.setInsecure();

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  uint32_t agoraMs = millis();
  if (ultimaTentativaWifiMs != 0 && agoraMs - ultimaTentativaWifiMs < INTERVALO_TENTATIVA_WIFI_MS) {
    return;
  }
  ultimaTentativaWifiMs = agoraMs;

  LOG_PRINT("Conectando no Wi-Fi: ");
  LOG_PRINTLN(WIFI_SSID);

  WiFi.setHostname(NOME_HOST_WIFI);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t inicioConexaoMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicioConexaoMs < TEMPO_LIMITE_CONEXAO_WIFI_MS) {
    delay(1000);
    LOG_PRINT(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOG_PRINTLN("\nWi-Fi nao conectado. Nova tentativa sera feita depois.");
    WiFi.disconnect(false);
    return;
  }

  LOG_PRINT("\nWi-Fi conectado. IP: ");
  LOG_PRINTLN(WiFi.localIP());
}

bool wifiConectado() {
  return WiFi.status() == WL_CONNECTED;
}

// Inicia o RTC DS3231 e aplica a data da compilacao se ele perdeu energia.
void inicializarRtc() {
  if (!rtc.begin()) {
    while (true) {
      LOG_PRINTLN("Nao foi possivel encontrar o RTC.");
      delay(1000);
    }
  }

  if (rtc.lostPower()) {
    LOG_PRINTLN("RTC perdeu energia. Ajustando para data/hora da compilacao.");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

// Atualiza o RTC via NTP. O parametro forcar permite sincronizar logo no boot.
void sincronizarRtcComNtp(bool forcar) {
  if (!wifiConectado()) {
    return;
  }

  uint32_t agoraMs = millis();
  if (!forcar && agoraMs - ultimaSincroniaNtpMs < INTERVALO_SINCRONIA_NTP_MS) {
    return;
  }

  if (clienteTempo.update()) {
    rtc.adjust(DateTime(clienteTempo.getEpochTime()));
    ultimaSincroniaNtpMs = agoraMs;
  }
}
