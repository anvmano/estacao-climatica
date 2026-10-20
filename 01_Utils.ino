// Converte horario para segundos desde 00:00, facilitando comparacoes de hora.
int converterParaSegundosDoDia(int hora, int minuto, int segundo = 0) {
  return hora * 3600 + minuto * 60 + segundo;
}

int segundosAtuaisDoDia() {
  DateTime agora = rtc.now();
  return converterParaSegundosDoDia(agora.hour(), agora.minute(), agora.second());
}

int normalizarSegundosDoDia(long segundos) {
  while (segundos < 0) {
    segundos += 24L * 3600L;
  }

  return segundos % (24L * 3600L);
}

// O RTC fica no fuso da estacao. Para respostas por CEP/localizacao, convertemos
// o "agora" para o fuso retornado pela API daquela coordenada.
int segundosAtuaisParaFuso(int deslocamentoFusoSegundos) {
  long segundosUtc = segundosAtuaisDoDia() - DESLOCAMENTO_FUSO_SEGUNDOS;
  return normalizarSegundosDoDia(segundosUtc + deslocamentoFusoSegundos);
}

String formatarDoisDigitos(int valor) {
  char buffer[3];
  snprintf(buffer, sizeof(buffer), "%02d", valor);
  return String(buffer);
}

// Formatos de texto usados em mensagens, paths do Firebase e chamadas de API.
String formatarHorario(int hora, int minuto) {
  return formatarDoisDigitos(hora) + ":" + formatarDoisDigitos(minuto);
}

String formatarData(DateTime dataHora) {
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%02d-%02d-%04d", dataHora.day(), dataHora.month(), dataHora.year());
  return String(buffer);
}

String formatarDataParaApi(DateTime dataHora) {
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", dataHora.year(), dataHora.month(), dataHora.day());
  return String(buffer);
}

String formatarHoraMinuto(DateTime dataHora) {
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d-%02d", dataHora.hour(), dataHora.minute());
  return String(buffer);
}

// Monta URLs em buffers fixos para reduzir alocacoes de String durante HTTP.
void montarUrlOpenWeather(const Localizacao& localizacao, char* buffer, size_t tamanhoBuffer) {
  snprintf(
    buffer,
    tamanhoBuffer,
    "%s%.6f&lon=%.6f&appid=%s",
    URL_OPENWEATHER,
    localizacao.latitude,
    localizacao.longitude,
    OPENWEATHER_API_KEY);
}

void montarUrlOpenStreetMap(const char* consulta, char* buffer, size_t tamanhoBuffer) {
  snprintf(buffer, tamanhoBuffer, "%s%s", URL_BUSCA_OPENSTREETMAP, consulta);
}

void montarUrlSunriseSunset(const Localizacao& localizacao, const char* data, char* buffer, size_t tamanhoBuffer) {
  snprintf(
    buffer,
    tamanhoBuffer,
    "%s?lat=%.6f&lng=%.6f&date=%s&formatted=0",
    URL_SUNRISE_SUNSET,
    localizacao.latitude,
    localizacao.longitude,
    data);
}

void atribuirHorarioLocalDeTimestampUtc(long timestampUtc, int deslocamentoFusoSegundos, int& valorHora, int& valorMinuto, int& valorSegundo) {
  int segundosLocais = normalizarSegundosDoDia(timestampUtc + deslocamentoFusoSegundos);
  valorHora = segundosLocais / 3600;
  valorMinuto = (segundosLocais % 3600) / 60;
  valorSegundo = segundosLocais % 60;
}

bool interpretarHorarioIsoUtcComFuso(const String& horarioIso, int deslocamentoFusoSegundos, int& valorHora, int& valorMinuto) {
  int separadorHorario = horarioIso.indexOf('T');
  if (separadorHorario < 0 || horarioIso.length() < separadorHorario + 9) {
    return false;
  }

  int horaUtc = horarioIso.substring(separadorHorario + 1, separadorHorario + 3).toInt();
  int minutoUtc = horarioIso.substring(separadorHorario + 4, separadorHorario + 6).toInt();
  int segundoUtc = horarioIso.substring(separadorHorario + 7, separadorHorario + 9).toInt();
  int segundosLocais = normalizarSegundosDoDia(converterParaSegundosDoDia(horaUtc, minutoUtc, segundoUtc) + deslocamentoFusoSegundos);

  valorHora = segundosLocais / 3600;
  valorMinuto = (segundosLocais % 3600) / 60;
  return true;
}

// Mensagem padrao de clima reutilizada pelo comando "clima", status e alerta.
String montarMensagemClima(const LeituraClima& leitura) {
  if (!leituraClimaValida(leitura)) {
    return "Leitura de clima indisponivel no momento.";
  }

  String message;
  message.reserve(160);
  message += "A temperatura atual e de <b>";
  message += String(leitura.temperatura);
  message += " graus Celsius</b> com <b>";
  message += String(leitura.umidade);
  message += "%</b> de umidade.";
  message += "\nA sensacao termica e de <b>";
  message += String(leitura.sensacaoTermica);
  message += " graus Celsius</b>.";
  return message;
}

bool remetenteAutorizado(const String& idRemetente) {
  for (size_t i = 0; i < VALID_SENDER_ID_COUNT; i++) {
    if (idRemetente == VALID_SENDER_IDS[i]) {
      return true;
    }
  }
  return false;
}

bool leituraClimaValida(const LeituraClima& leitura) {
  return !isnan(leitura.temperatura) && !isnan(leitura.umidade) && !isnan(leitura.sensacaoTermica);
}

bool coordenadasValidas(double latitude, double longitude) {
  if (isnan(latitude) || isnan(longitude)) {
    return false;
  }

  if (latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180) {
    return false;
  }

  // A biblioteca deixa latitude/longitude como 0 quando a mensagem nao traz localizacao.
  return latitude != 0 || longitude != 0;
}

// Aceita CEP com ou sem hifen; chamadas ruins ficam fora antes de bater na API.
bool ehComandoCep(String texto) {
  texto.trim();
  texto.replace("-", "");

  if (texto.length() != 8) {
    return false;
  }

  for (size_t i = 0; i < texto.length(); i++) {
    if (!isDigit(texto[i])) {
      return false;
    }
  }

  return true;
}

bool ehMensagemLocalizacaoBruta(int indiceMensagem) {
  return bot.messages[indiceMensagem].text.length() == 0 &&
         !bot.messages[indiceMensagem].hasDocument &&
         coordenadasValidas(bot.messages[indiceMensagem].latitude, bot.messages[indiceMensagem].longitude);
}

bool aguardandoLocalizacao(const String& idChat) {
  for (size_t i = 0; i < MAX_CHATS_AGUARDANDO_LOCALIZACAO; i++) {
    if (chatsAguardandoLocalizacao[i] == idChat) {
      return true;
    }
  }
  return false;
}

void marcarAguardandoLocalizacao(const String& idChat) {
  if (aguardandoLocalizacao(idChat)) {
    return;
  }

  for (size_t i = 0; i < MAX_CHATS_AGUARDANDO_LOCALIZACAO; i++) {
    if (chatsAguardandoLocalizacao[i].length() == 0) {
      chatsAguardandoLocalizacao[i] = idChat;
      return;
    }
  }

  chatsAguardandoLocalizacao[0] = idChat;
}

void limparAguardandoLocalizacao(const String& idChat) {
  for (size_t i = 0; i < MAX_CHATS_AGUARDANDO_LOCALIZACAO; i++) {
    if (chatsAguardandoLocalizacao[i] == idChat) {
      chatsAguardandoLocalizacao[i] = "";
      return;
    }
  }
}

bool horarioValido(int hora, int minuto) {
  return hora >= 0 && hora < 24 && minuto >= 0 && minuto < 60;
}

bool horariosCrepusculoValidos() {
  return horarioValido(horariosCrepusculoHoje.horaAmanhecer, horariosCrepusculoHoje.minutoAmanhecer) &&
         horarioValido(horariosCrepusculoHoje.horaZenite, horariosCrepusculoHoje.minutoZenite) &&
         horarioValido(horariosCrepusculoHoje.horaAnoitecer, horariosCrepusculoHoje.minutoAnoitecer) &&
         (horariosCrepusculoHoje.horaAmanhecer != 0 || horariosCrepusculoHoje.minutoAmanhecer != 0);
}

void somarMinutos(int hora, int minuto, int minutosParaSomar, int& horaResultado, int& minutoResultado) {
  int totalMinutos = hora * 60 + minuto + minutosParaSomar;

  while (totalMinutos < 0) {
    totalMinutos += 24 * 60;
  }

  horaResultado = (totalMinutos / 60) % 24;
  minutoResultado = totalMinutos % 60;
}
