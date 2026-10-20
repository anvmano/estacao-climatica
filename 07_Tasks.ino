// Centraliza tarefas periodicas que dependem de horario, sem usar delay().
void executarTarefasPeriodicas() {
  DateTime agora = rtc.now();

  bool deveGravarClima = false;
  if (MODO_DIAGNOSTICO_FIREBASE) {
    deveGravarClima = millis() - ultimaLeituraClimaMs >= UM_MINUTO_MS;
  } else {
    deveGravarClima = agora.minute() == 0 && millis() - ultimaLeituraClimaMs >= UM_MINUTO_MS;
  }

  // Normalmente grava na hora cheia; no diagnostico, grava uma vez por minuto.
  if (deveGravarClima) {
    LOG_PRINT("[TASK] Rotina de clima acionada em ");
    LOG_PRINT(formatarData(agora));
    LOG_PRINT(" ");
    LOG_PRINTLN(formatarHoraMinuto(agora));

    if (lerSensorDht()) {
      bool climaGravado = salvarClimaNoFirebase();
      LOG_PRINTLN(climaGravado
        ? "[TASK] Leitura enviada ou preservada na fila local."
        : "[TASK][CRITICO] Leitura nao foi enviada nem preservada.");
    } else {
      LOG_PRINTLN("[TASK] Sensor DHT falhou. Gravacao de clima ignorada.");
    }
    ultimaLeituraClimaMs = millis();
  }

  // Uma vez por dia, atualiza dados solares para a localizacao atual.
  if (agora.hour() == HORA_ATUALIZACAO_SOL_DIARIA && agora.minute() == MINUTO_ATUALIZACAO_SOL_DIARIA && millis() - ultimaAtualizacaoSolMs >= UM_MINUTO_MS) {
    bool horariosSolAtualizados = atualizarHorariosSolDaApi(localizacaoAtual);
    bool horariosCrepusculoAtualizados = false;
    if (horariosSolAtualizados) {
      horariosCrepusculoAtualizados = atualizarHorariosCrepusculoDaApi(localizacaoAtual);
    }
    if (horariosSolAtualizados && horariosCrepusculoAtualizados) {
      bool solarPreservado = salvarHorariosSolNoFirebase();
      LOG_PRINTLN(solarPreservado
        ? "[TASK] Dados solares enviados ou preservados na fila local."
        : "[TASK][CRITICO] Dados solares nao foram enviados nem preservados.");
    }
    ultimaAtualizacaoSolMs = millis();
  }

  static String ultimaDataAlerta = "";
  String dataAtual = formatarData(agora);
  int horaAlerta = 0;
  int minutoAlerta = 0;
  bool podeEnviarAlerta = horariosCrepusculoValidos();
  if (podeEnviarAlerta) {
    somarMinutos(horariosCrepusculoHoje.horaAmanhecer, horariosCrepusculoHoje.minutoAmanhecer, MINUTOS_APOS_AMANHECER_ALERTA, horaAlerta, minutoAlerta);
  }

  // Envia somente uma vez por data e apenas depois que o amanhecer foi carregado.
  if (podeEnviarAlerta &&
      dataAtual != ultimaDataAlerta &&
      agora.hour() == horaAlerta &&
      agora.minute() == minutoAlerta &&
      millis() - ultimoAlertaNascerSolMs >= UM_MINUTO_MS) {
    enviarAlertaDiarioAmanhecer();
    ultimaDataAlerta = dataAtual;
    ultimoAlertaNascerSolMs = millis();
  }
}
