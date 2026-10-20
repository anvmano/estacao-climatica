// Menu completo para usuarios autorizados.
String obterComandosAutorizados() {
  String message;
  message.reserve(360);
  message += "Os comandos disponiveis sao:\n\n";
  message += "<b>" + COMANDO_NASCER_SOL + " || '1' </b>: Para horario do nascer do sol.\n";
  message += "<b>" + COMANDO_POR_SOL + " || '2' </b>: Para horario do por do sol.\n";
  message += "<b>" + COMANDO_CLIMA + " || '3' </b>: Para verificar o clima.\n";
  message += "<b>" + COMANDO_STATUS + " || '4' </b>: Todas informacoes.\n";
  message += "<b>" + COMANDO_LOCALIZACAO + " || '5' </b>: Usar localizacao para nascer e por do sol.\n";
  message += "<b>" + COMANDO_CEP + " </b>: Informar CEP para dados da localizacao.\n\n";
  return message;
}

// Menu limitado para usuarios que nao estao na lista de IDs permitidos.
String obterComandosVisitante() {
  String message;
  message.reserve(220);
  message += "Os comandos disponiveis sao:\n\n";
  message += "<b>" + COMANDO_LOCALIZACAO + " </b>: Usar localizacao para nascer e por do sol.\n";
  message += "<b>" + COMANDO_CEP + " </b>: Informar o CEP para dados da sua localizacao.\n\n";
  return message;
}

void enviarMensagemInicial(const String& idChat, const String& nomeOrigem, bool autorizado) {
  String nome = nomeOrigem.length() == 0 ? "Guest" : nomeOrigem;
  String message;
  message.reserve(420);
  message += "<b>Ola ";
  message += nome;
  message += ".</b>\n";
  message += autorizado ? obterComandosAutorizados() : obterComandosVisitante();
  bot.sendMessage(idChat, message, "HTML");
}

// Informa nascer do sol considerando se o horario ainda vai acontecer hoje.
void enviarMensagemNascerSol(const String& idChat) {
  bool ehFuturo = segundosAtuaisDoDia() < converterParaSegundosDoDia(horariosSolHoje.horaNascerSol, horariosSolHoje.minutoNascerSol, horariosSolHoje.segundoNascerSol);
  String message = "Hoje o nascer do sol ";
  message += ehFuturo ? "sera as <b>" : "foi as <b>";
  message += formatarHorario(horariosSolHoje.horaNascerSol, horariosSolHoje.minutoNascerSol);
  message += "</b> da manha.";
  bot.sendMessage(idChat, message, "HTML");
}

// Informa por do sol considerando se o horario ainda vai acontecer hoje.
void enviarMensagemPorSol(const String& idChat) {
  bool ehFuturo = segundosAtuaisDoDia() < converterParaSegundosDoDia(horariosSolHoje.horaPorSol, horariosSolHoje.minutoPorSol, horariosSolHoje.segundoPorSol);
  String message = "Hoje o por do sol ";
  message += ehFuturo ? "sera as <b>" : "foi as <b>";
  message += formatarHorario(horariosSolHoje.horaPorSol, horariosSolHoje.minutoPorSol);
  message += "</b> da noite.";
  bot.sendMessage(idChat, message, "HTML");
}

// Monta uma resposta contextual conforme o periodo do dia.
void enviarMensagemStatus(const String& idChat) {
  enviarMensagemStatus(idChat, horariosSolHoje, climaAtual);
}

void enviarMensagemStatus(const String& idChat, const HorariosSol& horariosSol, const LeituraClima& leituraClima) {
  int segundosAgora = segundosAtuaisParaFuso(horariosSol.deslocamentoFusoSegundos);
  int segundosNascerSol = converterParaSegundosDoDia(horariosSol.horaNascerSol, horariosSol.minutoNascerSol, horariosSol.segundoNascerSol);
  int segundosPorSol = converterParaSegundosDoDia(horariosSol.horaPorSol, horariosSol.minutoPorSol, horariosSol.segundoPorSol);

  String message;
  message.reserve(260);

  if (segundosAgora < segundosNascerSol) {
    message += "Bom dia!\nAproveite o dia!\n";
    message += "Hoje o nascer do sol sera as <b>" + formatarHorario(horariosSol.horaNascerSol, horariosSol.minutoNascerSol) + "</b> da manha.\n";
  } else if (segundosAgora < 43200) {
    message += "Bom dia!\nAproveite o dia!\n";
    message += "Hoje o nascer do sol foi as <b>" + formatarHorario(horariosSol.horaNascerSol, horariosSol.minutoNascerSol) + "</b> da manha.";
    message += "\nE o por do sol sera as <b>" + formatarHorario(horariosSol.horaPorSol, horariosSol.minutoPorSol) + "</b> da noite.\n";
  } else if (segundosAgora < segundosPorSol) {
    message += "Boa tarde!\nAproveite o restante do dia!\n";
    message += "Hoje o nascer do sol foi as <b>" + formatarHorario(horariosSol.horaNascerSol, horariosSol.minutoNascerSol) + "</b> da manha.";
    message += "\nE o por do sol sera as <b>" + formatarHorario(horariosSol.horaPorSol, horariosSol.minutoPorSol) + "</b> da noite.\n";
  } else {
    message += "Boa noite!\nEspero que seu dia tenha sido otimo!\n";
    message += "Hoje o por do sol foi as <b>" + formatarHorario(horariosSol.horaPorSol, horariosSol.minutoPorSol) + "</b> da noite.\n";
  }

  bot.sendMessage(idChat, message, "HTML");
  bot.sendMessage(idChat, montarMensagemClima(leituraClima), "HTML");
}

void enviarAvisoClimaIndisponivel(const String& idChat) {
  bot.sendMessage(idChat, "Consegui calcular os horarios solares, mas nao consegui consultar o clima agora.", "HTML");
}

void enviarAvisoAguardandoLocalizacao(const String& idChat) {
  bot.sendMessage(idChat, "Ainda preciso da localizacao. Toque no botao <b>Compartilhar localizacao</b>, envie um CEP com 8 digitos ou mande outro comando para sair desse modo.", "HTML");
}

bool ehComandoLocalizacao(const String& texto) {
  return texto.equalsIgnoreCase(COMANDO_LOCALIZACAO) || texto == "5";
}

// Pede ao Telegram para mostrar um botao de compartilhamento de localizacao.
void solicitarLocalizacaoDoUsuario(const String& idChat) {
  marcarAguardandoLocalizacao(idChat);
  String tecladoLocalizacao = "[[{\"text\":\"Compartilhar localizacao\",\"request_location\":true}]]";
  bot.sendMessageWithReplyKeyboard(
    idChat,
    "Toque em <b>Compartilhar localizacao</b> ou envie um CEP com 8 digitos.",
    "HTML",
    tecladoLocalizacao,
    true,
    true);
}

// Se a mensagem ja contem coordenadas, calcula clima/sol sem mudar a localizacao fixa da estacao.
void tratarComandoLocalizacao(const String& idChat, int indiceMensagem) {
  if (!ehMensagemLocalizacaoBruta(indiceMensagem)) {
    solicitarLocalizacaoDoUsuario(idChat);
    return;
  }

  Localizacao localizacaoSolicitada = {
    bot.messages[indiceMensagem].latitude,
    bot.messages[indiceMensagem].longitude,
    0
  };
  HorariosSol horariosSolSolicitados;
  LeituraClima climaSolicitado;
  limparAguardandoLocalizacao(idChat);

  bot.sendMessage(idChat, "Segue as informacoes para sua localizacao.", "HTML");
  bool horariosSolAtualizados = atualizarHorariosSolDaApi(localizacaoSolicitada, horariosSolSolicitados);
  bool climaAtualizado = atualizarClimaDaApi(localizacaoSolicitada, climaSolicitado);
  if (horariosSolAtualizados) {
    enviarMensagemStatus(idChat, horariosSolSolicitados, climaSolicitado);
    if (!climaAtualizado) {
      enviarAvisoClimaIndisponivel(idChat);
    }
  } else {
    bot.sendMessage(idChat, "Nao foi possivel atualizar os horarios solares para essa localizacao.", "HTML");
  }
}

// Resolve um CEP via OpenStreetMap/Nominatim e usa as coordenadas encontradas.
void tratarComandoCep(String cep, const String& idChat) {
  if (!ehComandoCep(cep)) {
    bot.sendMessage(idChat, "CEP invalido. Informe exatamente 8 digitos.", "HTML");
    return;
  }

  cep.trim();
  cep.replace("-", "");

  if (!wifiConectado()) {
    bot.sendMessage(idChat, "Sem conexao Wi-Fi no momento. Tente novamente em instantes.", "HTML");
    return;
  }

  char url[TAMANHO_BUFFER_URL];
  montarUrlOpenStreetMap(cep.c_str(), url, sizeof(url));

  HTTPClient http;
  http.setTimeout(TEMPO_LIMITE_HTTP_MS);
  http.begin(clienteHttps, url);
  http.addHeader("User-Agent", USER_AGENT_OPENSTREETMAP);

  int codigoHttp = http.GET();
  if (codigoHttp != HTTP_CODE_OK) {
    String message = "Nao consegui consultar o CEP agora. Tente novamente em instantes ou envie sua localizacao pelo Telegram.";
    bot.sendMessage(idChat, message, "HTML");
    http.end();
    return;
  }

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, http.getString());
  http.end();

  if (erro || doc.size() == 0) {
    String message = "Infelizmente nao conseguimos encontrar o CEP.\n";
    message += "Favor usar o metodo de <b>localizacao</b> para poder melhor atende-lo.";
    bot.sendMessage(idChat, message, "HTML");
    return;
  }

  if (!doc[0]["lat"].is<const char*>() || !doc[0]["lon"].is<const char*>()) {
    bot.sendMessage(idChat, "A resposta do CEP veio sem coordenadas validas.", "HTML");
    return;
  }

  Localizacao localizacaoSolicitada = {
    doc[0]["lat"].as<double>(),
    doc[0]["lon"].as<double>(),
    0
  };
  HorariosSol horariosSolSolicitados;
  LeituraClima climaSolicitado;

  bool horariosSolAtualizados = atualizarHorariosSolDaApi(localizacaoSolicitada, horariosSolSolicitados);
  bool climaAtualizado = atualizarClimaDaApi(localizacaoSolicitada, climaSolicitado);
  if (horariosSolAtualizados) {
    enviarMensagemStatus(idChat, horariosSolSolicitados, climaSolicitado);
    if (!climaAtualizado) {
      enviarAvisoClimaIndisponivel(idChat);
    }
  } else {
    bot.sendMessage(idChat, "Encontrei o CEP, mas nao consegui calcular os horarios solares agora. Tente novamente em instantes.", "HTML");
  }
}

// Comandos de usuarios autorizados podem acessar status local e atalhos numericos.
void processarComandoAutorizado(const String& idChat, const String& texto, int indiceMensagem) {
  if (ehMensagemLocalizacaoBruta(indiceMensagem)) {
    bot.sendMessage(idChat, "Um momento por favor...", "HTML");
    tratarComandoLocalizacao(idChat, indiceMensagem);
    return;
  }

  if (aguardandoLocalizacao(idChat)) {
    if (ehComandoLocalizacao(texto)) {
      solicitarLocalizacaoDoUsuario(idChat);
      return;
    }

    if (ehComandoCep(texto)) {
      limparAguardandoLocalizacao(idChat);
      bot.sendMessage(idChat, "Um momento por favor...", "HTML");
      tratarComandoCep(texto, idChat);
      return;
    }

    limparAguardandoLocalizacao(idChat);
  }

  if (texto == COMANDO_START) {
    enviarMensagemInicial(idChat, bot.messages[indiceMensagem].from_name, true);
  } else if (texto.equalsIgnoreCase(COMANDO_NASCER_SOL) || texto == "1") {
    enviarMensagemNascerSol(idChat);
  } else if (texto.equalsIgnoreCase(COMANDO_POR_SOL) || texto == "2") {
    enviarMensagemPorSol(idChat);
  } else if (texto.equalsIgnoreCase(COMANDO_CLIMA) || texto == "3") {
    lerSensorDht();
    bot.sendMessage(idChat, montarMensagemClima(climaAtual), "HTML");
  } else if (texto.equalsIgnoreCase(COMANDO_STATUS) || texto == "4") {
    lerSensorDht();
    enviarMensagemStatus(idChat);
  } else if (ehComandoLocalizacao(texto)) {
    bot.sendMessage(idChat, "Um momento por favor...", "HTML");
    tratarComandoLocalizacao(idChat, indiceMensagem);
  } else if (ehComandoCep(texto)) {
    bot.sendMessage(idChat, "Um momento por favor...", "HTML");
    tratarComandoCep(texto, idChat);
  } else {
    String message = "Comando nao encontrado.\n";
    message += obterComandosAutorizados();
    bot.sendMessage(idChat, message, "HTML");
  }
}

// Visitantes podem consultar por localizacao ou CEP, mas nao recebem todos os comandos.
void processarComandoVisitante(const String& idChat, const String& texto, int indiceMensagem) {
  if (ehMensagemLocalizacaoBruta(indiceMensagem)) {
    bot.sendMessage(idChat, "Um momento por favor...", "HTML");
    tratarComandoLocalizacao(idChat, indiceMensagem);
    return;
  }

  if (aguardandoLocalizacao(idChat)) {
    if (ehComandoLocalizacao(texto)) {
      solicitarLocalizacaoDoUsuario(idChat);
      return;
    }

    if (ehComandoCep(texto)) {
      limparAguardandoLocalizacao(idChat);
      bot.sendMessage(idChat, "Um momento por favor...", "HTML");
      tratarComandoCep(texto, idChat);
      return;
    }

    limparAguardandoLocalizacao(idChat);
  }

  if (texto == COMANDO_START) {
    enviarMensagemInicial(idChat, bot.messages[indiceMensagem].from_name, false);
  } else if (ehComandoLocalizacao(texto)) {
    bot.sendMessage(idChat, "Um momento por favor...", "HTML");
    tratarComandoLocalizacao(idChat, indiceMensagem);
  } else if (ehComandoCep(texto)) {
    bot.sendMessage(idChat, "Um momento por favor...", "HTML");
    tratarComandoCep(texto, idChat);
  } else {
    String message = "Comando nao encontrado.\n";
    message += obterComandosVisitante();
    bot.sendMessage(idChat, message, "HTML");
  }
}

// Distribui cada mensagem recebida para o fluxo autorizado ou visitante.
void tratarNovasMensagens(int quantidadeMensagens) {
  for (int i = 0; i < quantidadeMensagens; i++) {
    String idChat = bot.messages[i].chat_id;
    String texto = bot.messages[i].text;
    String idRemetente = String(bot.messages[i].from_id);

    if (remetenteAutorizado(idRemetente)) {
      processarComandoAutorizado(idChat, texto, i);
    } else {
      processarComandoVisitante(idChat, texto, i);
    }
  }
}

// Faz polling do Telegram sem bloquear o loop principal.
void consultarMensagensTelegram() {
  if (!wifiConectado()) {
    return;
  }

  uint32_t agoraMs = millis();
  if (agoraMs - ultimoConsultaTelegramMs <= INTERVALO_CONSULTA_TELEGRAM_MS) {
    return;
  }

  int quantidadeMensagens = bot.getUpdates(bot.last_message_received + 1);
  while (quantidadeMensagens) {
    tratarNovasMensagens(quantidadeMensagens);
    quantidadeMensagens = bot.getUpdates(bot.last_message_received + 1);
  }

  ultimoConsultaTelegramMs = agoraMs;
}

// Alerta automatico enviado para os IDs autorizados alguns minutos apos o amanhecer.
void enviarAlertaDiarioAmanhecer() {
  String message;
  message.reserve(320);
  message += "Bom dia!\n";
  message += "Hoje e <b>";
  message += formatarData(rtc.now());
  message += "</b> (";
  message += DIAS_DA_SEMANA[rtc.now().dayOfTheWeek()];
  message += ").\n";
  message += "O amanhecer astronomico foi as <b>";
  message += formatarHorario(horariosCrepusculoHoje.horaAmanhecer, horariosCrepusculoHoje.minutoAmanhecer);
  message += "</b>.\n";
  message += "Hoje o nascer do sol sera as <b>";
  message += formatarHorario(horariosSolHoje.horaNascerSol, horariosSolHoje.minutoNascerSol);
  message += "</b> da manha.\n";
  message += montarMensagemClima(climaAtual);

  for (size_t i = 0; i < VALID_SENDER_ID_COUNT; i++) {
    bot.sendMessage(VALID_SENDER_IDS[i], message, "HTML");
  }
}
