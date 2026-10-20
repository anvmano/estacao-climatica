void inicializarFirebase() {
  LOG_PRINTLN("[FIREBASE] Inicializando cliente...");
  configFirebase.database_url = FIREBASE_HOST;
  configFirebase.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.reconnectNetwork(true);
  dadosFirebase.setBSSLBufferSize(4096, 1024);
  Firebase.begin(&configFirebase, &authFirebase);
  LOG_PRINTLN("[FIREBASE] Firebase.begin() chamado.");
}

// setJSON no mesmo destino torna as tentativas repetidas idempotentes.
bool gravarJsonFirebase(const String& caminho, const String& operacaoId, const String& conteudoJson) {
  if (!wifiConectado() || !Firebase.ready()) {
    return false;
  }

  FirebaseJson json;
  if (!json.setJsonData(conteudoJson)) {
    LOG_PRINTLN("[FIREBASE] JSON pendente invalido; registro preservado na fila.");
    return false;
  }

  String caminhoComId = caminho + "/" + operacaoId;
  bool gravou = Firebase.setJSON(dadosFirebase, caminhoComId, json);
  if (!gravou) {
    LOG_PRINT("[FIREBASE] Falha em setJSON: ");
    LOG_PRINTLN(dadosFirebase.errorReason());
  }
  return gravou;
}

bool gravarOuEnfileirarFirebase(const String& tipo, const String& caminho, const String& operacaoId, uint32_t timestamp, FirebaseJson& json) {
  String conteudoJson;
  json.toString(conteudoJson, false);

  if (!filaFirebaseTemPendencias() && gravarJsonFirebase(caminho, operacaoId, conteudoJson)) {
    LOG_PRINT("[FIREBASE] Gravacao confirmada: ");
    LOG_PRINTLN(caminho + "/" + operacaoId);
    return true;
  }

  bool enfileirou = filaFirebaseAdicionar(tipo, caminho, operacaoId, timestamp, conteudoJson);
  LOG_PRINT(enfileirou ? "[FIREBASE] Registro persistido na fila: " : "[FIREBASE][CRITICO] Nao foi possivel preservar registro: ");
  LOG_PRINTLN(caminho + "/" + operacaoId);
  return enfileirou;
}

// Um registro por dia; repeticoes sobrescrevem o mesmo destino.
bool salvarHorariosSolNoFirebase() {
  LOG_PRINTLN("[FIREBASE] Tentando gravar horarios solares...");

  String path = "/historico/NascePorDoSol/" + formatarData(rtc.now());
  LOG_PRINT("[FIREBASE] Path solar: ");
  LOG_PRINTLN(path);

  FirebaseJson json;
  json.set("HoraPorDoSol", horariosSolHoje.horaPorSol);
  json.set("MinutePorDoSol", horariosSolHoje.minutoPorSol);
  json.set("HourNascerDoSol", horariosSolHoje.horaNascerSol);
  json.set("MinuteNascerDoSol", horariosSolHoje.minutoNascerSol);
  json.set("HoraAmanhecer", horariosCrepusculoHoje.horaAmanhecer);
  json.set("MinuteAmanhecer", horariosCrepusculoHoje.minutoAmanhecer);
  json.set("HoraZenite", horariosCrepusculoHoje.horaZenite);
  json.set("MinuteZenite", horariosCrepusculoHoje.minutoZenite);
  json.set("HourAnoitecer", horariosCrepusculoHoje.horaAnoitecer);
  json.set("MinuteAnoitecer", horariosCrepusculoHoje.minutoAnoitecer);

  DateTime agora = rtc.now();
  return gravarOuEnfileirarFirebase("solar", path, "solar", agora.unixtime(), json);
}

// Um registro por minuto; repeticoes dentro do mesmo intervalo sobrescrevem o destino.
bool salvarClimaNoFirebase() {
  LOG_PRINTLN("[FIREBASE] Tentando gravar clima...");

  if (!leituraClimaValida(climaAtual)) {
    LOG_PRINTLN("[FIREBASE] Falha: leitura de clima invalida. Nada gravado.");
    return false;
  }

  DateTime agora = rtc.now();
  String path = "/historico/Temperatura/" + formatarData(agora) + "/" + formatarHoraMinuto(agora);
  LOG_PRINT("[FIREBASE] Path clima: ");
  LOG_PRINTLN(path);
  LOG_PRINT("[FIREBASE] Temperatura=");
  LOG_PRINT(climaAtual.temperatura);
  LOG_PRINT(" Umidade=");
  LOG_PRINT(climaAtual.umidade);
  LOG_PRINT(" Sensacao=");
  LOG_PRINTLN(climaAtual.sensacaoTermica);

  FirebaseJson json;
  json.set("Temperatura", climaAtual.temperatura);
  json.set("Umidade", climaAtual.umidade);
  json.set("Sensacao termica", climaAtual.sensacaoTermica);

  return gravarOuEnfileirarFirebase("clima", path, "clima", agora.unixtime(), json);
}
