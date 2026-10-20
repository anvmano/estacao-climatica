// Busca temperatura, umidade e sensacao termica na API OpenWeather.
bool atualizarClimaDaApi(const Localizacao& localizacao, LeituraClima& leitura) {
  if (!wifiConectado()) {
    return false;
  }

  char url[TAMANHO_BUFFER_URL];
  montarUrlOpenWeather(localizacao, url, sizeof(url));

  HTTPClient http;
  http.setTimeout(TEMPO_LIMITE_HTTP_MS);
  http.begin(clienteHttps, url);

  int codigoHttp = http.GET();
  if (codigoHttp != HTTP_CODE_OK) {
    LOG_PRINT("Erro HTTP ao consultar OpenWeather: ");
    LOG_PRINTLN(codigoHttp);
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, http.getString());
  http.end();

  if (erro) {
    LOG_PRINTLN("Erro ao interpretar JSON do OpenWeather.");
    return false;
  }

  if (!doc["main"]["temp"].is<float>() || !doc["main"]["humidity"].is<float>() || !doc["main"]["feels_like"].is<float>()) {
    LOG_PRINTLN("JSON do OpenWeather sem campos de clima esperados.");
    return false;
  }

  leitura.temperatura = doc["main"]["temp"].as<float>() - 273.15;
  leitura.umidade = doc["main"]["humidity"].as<float>();
  leitura.sensacaoTermica = doc["main"]["feels_like"].as<float>() - 273.15;
  return true;
}

// A mesma resposta do OpenWeather tambem traz nascer e por do sol em UTC.
bool atualizarHorariosSolDaApi(const Localizacao& localizacao) {
  return atualizarHorariosSolDaApi(localizacao, horariosSolHoje);
}

bool atualizarHorariosSolDaApi(const Localizacao& localizacao, HorariosSol& horariosSol) {
  if (!wifiConectado()) {
    return false;
  }

  char url[TAMANHO_BUFFER_URL];
  montarUrlOpenWeather(localizacao, url, sizeof(url));

  HTTPClient http;
  http.setTimeout(TEMPO_LIMITE_HTTP_MS);
  http.begin(clienteHttps, url);

  int codigoHttp = http.GET();
  if (codigoHttp != HTTP_CODE_OK) {
    LOG_PRINT("Erro HTTP ao consultar nascer/por do sol: ");
    LOG_PRINTLN(codigoHttp);
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, http.getString());
  http.end();

  if (erro) {
    LOG_PRINTLN("Erro ao interpretar JSON de nascer/por do sol.");
    return false;
  }

  if (!doc["sys"]["sunrise"].is<long>() || !doc["sys"]["sunset"].is<long>() || !doc["timezone"].is<int>()) {
    LOG_PRINTLN("JSON do OpenWeather sem campos de nascer/por do sol esperados.");
    return false;
  }

  // A API retorna Unix timestamp em UTC e tambem o fuso da coordenada consultada.
  horariosSol.deslocamentoFusoSegundos = doc["timezone"].as<int>();
  atribuirHorarioLocalDeTimestampUtc(
    doc["sys"]["sunrise"].as<long>(),
    horariosSol.deslocamentoFusoSegundos,
    horariosSol.horaNascerSol,
    horariosSol.minutoNascerSol,
    horariosSol.segundoNascerSol);
  atribuirHorarioLocalDeTimestampUtc(
    doc["sys"]["sunset"].as<long>(),
    horariosSol.deslocamentoFusoSegundos,
    horariosSol.horaPorSol,
    horariosSol.minutoPorSol,
    horariosSol.segundoPorSol);

  return true;
}

// Consulta amanhecer/anoitecer astronomico usando a data atual do RTC.
bool atualizarHorariosCrepusculoDaApi(const Localizacao& localizacao) {
  if (!wifiConectado()) {
    return false;
  }

  DateTime agora = rtc.now();
  String data = formatarDataParaApi(agora);
  char url[TAMANHO_BUFFER_URL];
  montarUrlSunriseSunset(localizacao, data.c_str(), url, sizeof(url));

  HTTPClient http;
  http.setTimeout(TEMPO_LIMITE_HTTP_MS);
  http.begin(clienteHttps, url);

  int codigoHttp = http.GET();
  if (codigoHttp != HTTP_CODE_OK) {
    LOG_PRINT("Erro HTTP ao consultar amanhecer/anoitecer: ");
    LOG_PRINTLN(codigoHttp);
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, http.getString());
  http.end();

  if (erro) {
    LOG_PRINTLN("Erro ao interpretar JSON de amanhecer/anoitecer.");
    return false;
  }

  if (doc["status"].as<String>() != "OK") {
    LOG_PRINTLN("API sunrise-sunset retornou status diferente de OK.");
    return false;
  }

  String amanhecer = doc["results"]["astronomical_twilight_begin"].as<String>();
  String zenite = doc["results"]["solar_noon"].as<String>();
  String anoitecer = doc["results"]["astronomical_twilight_end"].as<String>();

  if (amanhecer.length() == 0 || zenite.length() == 0 || anoitecer.length() == 0 ||
      amanhecer.indexOf(':') < 0 || zenite.indexOf(':') < 0 || anoitecer.indexOf(':') < 0) {
    LOG_PRINTLN("JSON de amanhecer/anoitecer sem horarios esperados.");
    return false;
  }

  // Com formatted=0, a API responde em ISO/UTC; aplicamos o mesmo fuso obtido do OpenWeather.
  if (!interpretarHorarioIsoUtcComFuso(amanhecer, horariosSolHoje.deslocamentoFusoSegundos, horariosCrepusculoHoje.horaAmanhecer, horariosCrepusculoHoje.minutoAmanhecer) ||
      !interpretarHorarioIsoUtcComFuso(zenite, horariosSolHoje.deslocamentoFusoSegundos, horariosCrepusculoHoje.horaZenite, horariosCrepusculoHoje.minutoZenite) ||
      !interpretarHorarioIsoUtcComFuso(anoitecer, horariosSolHoje.deslocamentoFusoSegundos, horariosCrepusculoHoje.horaAnoitecer, horariosCrepusculoHoje.minutoAnoitecer)) {
    LOG_PRINTLN("JSON de amanhecer/anoitecer com horarios em formato inesperado.");
    return false;
  }

  return true;
}
