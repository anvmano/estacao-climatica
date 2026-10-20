namespace {
constexpr size_t LIMITE_FILA_FIREBASE_BYTES = 256 * 1024;
constexpr size_t LIMITE_REGISTRO_FIREBASE_BYTES = 4096;
constexpr uint32_t BACKOFF_INICIAL_FIREBASE_MS = 5000;
constexpr uint32_t BACKOFF_MAXIMO_FIREBASE_MS = 300000;

bool filaFirebaseDisponivel = false;
bool arquivoTemporarioInvalido = false;
uint32_t ultimaTentativaFilaMs = 0;
uint32_t intervaloTentativaFilaMs = BACKOFF_INICIAL_FIREBASE_MS;

bool arquivoPendente(const String& nome) {
  return nome.startsWith("/q") && nome.endsWith(".json");
}

String primeiroArquivoPendente() {
  File raiz = LittleFS.open("/", "r");
  String maisAntigo;
  File arquivo = raiz.openNextFile();
  while (arquivo) {
    String nome = arquivo.name();
    if (arquivoPendente(nome) && (maisAntigo.length() == 0 || nome < maisAntigo)) {
      maisAntigo = nome;
    }
    arquivo = raiz.openNextFile();
  }
  return maisAntigo;
}

}

void inicializarFilaFirebase() {
  Preferences preferencias;
  if (!preferencias.begin("firebaseq", false)) {
    LOG_PRINTLN("[FILA] NVS indisponivel; fila persistente desativada.");
    return;
  }

  bool inicializadaAntes = preferencias.getBool("lfsinit", false);
  filaFirebaseDisponivel = LittleFS.begin(false);
  if (!filaFirebaseDisponivel && !inicializadaAntes) {
    LOG_PRINTLN("[FILA] Inicializando particao LittleFS nova.");
    filaFirebaseDisponivel = LittleFS.format() && LittleFS.begin(false);
  }

  if (filaFirebaseDisponivel) {
    if (!inicializadaAntes) preferencias.putBool("lfsinit", true);
    LOG_PRINT("[FILA] LittleFS pronta. Uso: ");
    LOG_PRINT(LittleFS.usedBytes());
    LOG_PRINT("/");
    LOG_PRINTLN(LittleFS.totalBytes());
    File temporario = LittleFS.open("/pending.tmp", "r");
    if (temporario) {
      JsonDocument registro;
      DeserializationError erro = deserializeJson(registro, temporario);
      temporario.close();
      if (!erro && registro["operacaoId"].is<const char*>() && registro["timestamp"].is<uint32_t>() &&
          registro["caminho"].is<const char*>() && registro["conteudo"].is<const char*>() &&
          registro["operacao"] == "setJSON") {
        char nomeArquivo[64];
        String operacaoId = registro["operacaoId"].as<String>();
        snprintf(nomeArquivo, sizeof(nomeArquivo), "/q%010lu_%s.json",
          static_cast<unsigned long>(registro["timestamp"].as<uint32_t>()), operacaoId.c_str());
        bool recuperado = LittleFS.exists(nomeArquivo)
          ? LittleFS.remove("/pending.tmp")
          : LittleFS.rename("/pending.tmp", nomeArquivo);
        LOG_PRINTLN(recuperado
          ? "[FILA] Operacao temporaria recuperada apos reinicio."
          : "[FILA][CRITICO] Nao foi possivel recuperar operacao temporaria.");
        arquivoTemporarioInvalido = !recuperado;
      } else {
        arquivoTemporarioInvalido = true;
        LOG_PRINTLN("[FILA][CRITICO] pending.tmp incompleto preservado; fila bloqueada para evitar sobrescrita.");
      }
    }
  } else {
    LOG_PRINTLN("[FILA][CRITICO] LittleFS nao montou. Sem formatar particao ja inicializada.");
  }
  preferencias.end();
}

bool filaFirebaseTemPendencias() {
  return filaFirebaseDisponivel && primeiroArquivoPendente().length() > 0;
}

bool filaFirebaseAdicionar(const String& tipo, const String& caminho, const String& operacaoId, uint32_t timestamp, const String& conteudoJson) {
  if (!filaFirebaseDisponivel || arquivoTemporarioInvalido || conteudoJson.length() > LIMITE_REGISTRO_FIREBASE_BYTES) {
    LOG_PRINTLN("[FILA][CRITICO] Fila indisponivel ou registro excede 4096 bytes.");
    return false;
  }

  char nomeArquivo[64];
  snprintf(nomeArquivo, sizeof(nomeArquivo), "/q%010lu_%s.json", static_cast<unsigned long>(timestamp), operacaoId.c_str());
  if (LittleFS.exists(nomeArquivo)) {
    LOG_PRINT("[FILA] Operacao ja enfileirada: ");
    LOG_PRINTLN(operacaoId);
    return true;
  }

  JsonDocument registro;
  registro["tipo"] = tipo;
  registro["caminho"] = caminho;
  registro["conteudo"] = conteudoJson;
  registro["timestamp"] = timestamp;
  registro["operacaoId"] = operacaoId;
  registro["operacao"] = "setJSON";

  String envelope;
  serializeJson(registro, envelope);
  size_t bytesNecessarios = envelope.length() + 512;
  if (bytesNecessarios > LIMITE_FILA_FIREBASE_BYTES ||
      LittleFS.usedBytes() + bytesNecessarios > LIMITE_FILA_FIREBASE_BYTES ||
      LittleFS.totalBytes() - LittleFS.usedBytes() < bytesNecessarios) {
    LOG_PRINTLN("[FILA][CRITICO] Fila cheia; nenhum registro antigo foi descartado.");
    return false;
  }

  File temporario = LittleFS.open("/pending.tmp", "w");
  if (!temporario || temporario.print(envelope) != envelope.length()) {
    if (temporario) temporario.close();
    LittleFS.remove("/pending.tmp");
    LOG_PRINTLN("[FILA][CRITICO] Falha ao gravar arquivo temporario.");
    return false;
  }
  temporario.flush();
  temporario.close();

  if (!LittleFS.rename("/pending.tmp", nomeArquivo)) {
    LittleFS.remove("/pending.tmp");
    LOG_PRINTLN("[FILA][CRITICO] Falha ao confirmar arquivo pendente.");
    return false;
  }

  LOG_PRINT("[FILA] Pendente salvo. bytes=");
  LOG_PRINT(envelope.length());
  LOG_PRINT(" total=");
  LOG_PRINTLN(LittleFS.usedBytes());
  return true;
}

void processarFilaFirebase() {
  if (!filaFirebaseDisponivel || !wifiConectado()) {
    return;
  }

  uint32_t agoraMs = millis();
  if (agoraMs - ultimaTentativaFilaMs < intervaloTentativaFilaMs) {
    return;
  }

  String nomeArquivo = primeiroArquivoPendente();
  if (nomeArquivo.length() == 0) {
    intervaloTentativaFilaMs = BACKOFF_INICIAL_FIREBASE_MS;
    return;
  }
  ultimaTentativaFilaMs = agoraMs;

  File arquivo = LittleFS.open(nomeArquivo, "r");
  if (!arquivo) {
    LOG_PRINTLN("[FILA][ERRO] Nao foi possivel abrir o registro mais antigo.");
    return;
  }
  JsonDocument registro;
  DeserializationError erro = deserializeJson(registro, arquivo);
  arquivo.close();
  if (erro || !registro["caminho"].is<const char*>() || !registro["conteudo"].is<const char*>() ||
      !registro["operacaoId"].is<const char*>() || registro["operacao"] != "setJSON") {
    LOG_PRINT("[FILA][CRITICO] Registro invalido preservado: ");
    LOG_PRINTLN(nomeArquivo);
    intervaloTentativaFilaMs = BACKOFF_MAXIMO_FIREBASE_MS;
    return;
  }

  String caminho = registro["caminho"].as<String>();
  String operacaoId = registro["operacaoId"].as<String>();
  String conteudoJson = registro["conteudo"].as<String>();
  LOG_PRINT("[FILA] Enviando registro antigo: ");
  LOG_PRINTLN(caminho + "/" + operacaoId);

  if (gravarJsonFirebase(caminho, operacaoId, conteudoJson)) {
    if (LittleFS.remove(nomeArquivo)) {
      LOG_PRINT("[FILA] Gravacao confirmada e pendencia removida: ");
      LOG_PRINTLN(operacaoId);
      intervaloTentativaFilaMs = BACKOFF_INICIAL_FIREBASE_MS;
    } else {
      LOG_PRINTLN("[FILA][ERRO] Firebase confirmou, mas nao removeu pendencia; repeticao idempotente.");
    }
    return;
  }

  LOG_PRINT("[FILA] Envio falhou; registro mantido. Motivo: ");
  LOG_PRINTLN(dadosFirebase.errorReason());
  intervaloTentativaFilaMs = min(intervaloTentativaFilaMs * 2, BACKOFF_MAXIMO_FIREBASE_MS);
  LOG_PRINT("[FILA] Proxima tentativa em ms: ");
  LOG_PRINTLN(intervaloTentativaFilaMs);
}
