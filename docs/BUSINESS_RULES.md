# Business Rules

## Regra: Inicializacao Obrigatoria Do RTC

Arquivo: `02_Connectivity.ino`

Metodo: `inicializarRtc()`

Objetivo:

- Garantir que o DS3231 exista antes do restante do firmware operar.

Entradas:

- `rtc.begin()`
- `rtc.lostPower()`

Saidas:

- RTC pronto.
- RTC ajustado para data/hora da compilacao se perdeu energia.

Impacto:

- Critica.

Dependencias:

- RTClib.
- I2C.
- DS3231.

O que acontece se for alterada:

- Permitir boot sem RTC exige revisar todas as rotinas que usam `rtc.now()`.
- Manter como esta faz o firmware travar se o RTC estiver ausente.

## Regra: Conexao Wi-Fi Com Retentativa Controlada

Arquivo: `02_Connectivity.ino`

Metodo: `conectarWifi()`

Objetivo:

- Conectar Wi-Fi sem tentar continuamente a cada loop.

Entradas:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `TEMPO_LIMITE_CONEXAO_WIFI_MS`
- `INTERVALO_TENTATIVA_WIFI_MS`

Saidas:

- `WiFi.status() == WL_CONNECTED` quando conectado.

Impacto:

- Alta criticidade.

Dependencias:

- WiFi.
- Clientes TLS.

O que acontece se for alterada:

- Pode bloquear loop, impedir Telegram/Firebase/APIs ou reconectar agressivamente.

## Regra: TLS Do Telegram E APIs

Arquivo: `02_Connectivity.ino`

Metodo: `conectarWifi()`

Objetivo:

- Configurar `clienteSeguro` com certificado do Telegram e `clienteHttps` em modo inseguro para chamadas HTTPS.

Entradas:

- `TELEGRAM_CERTIFICATE_ROOT`

Saidas:

- Cliente Telegram com CA configurado.
- Cliente HTTPS externo sem validacao de certificado.

Impacto:

- Alta criticidade.

Dependencias:

- UniversalTelegramBot.
- HTTPClient.
- WiFiClientSecure.

O que acontece se for alterada:

- Telegram ou APIs HTTPS podem falhar.
- Trocar `setInsecure()` por CA valida melhora seguranca, mas exige certificados por host.

## Regra: Leitura DHT So Atualiza Estado Se Valida

Arquivo: `03_Sensors.ino`

Metodo: `lerSensorDht()`

Objetivo:

- Evitar sobrescrever `climaAtual` com NaN.

Entradas:

- `dht.readHumidity()`
- `dht.readTemperature()`

Saidas:

- `climaAtual` atualizado somente quando ambos os valores sao numericos.

Impacto:

- Alta criticidade.

Dependencias:

- DHT22.

O que acontece se for alterada:

- Firebase e Telegram podem receber valores invalidos.

## Regra: Gravacao De Clima So Na Hora Cheia

Arquivo: `07_Tasks.ino`

Metodo: `executarTarefasPeriodicas()`

Objetivo:

- Gravar historico de clima uma vez por hora em modo normal.

Entradas:

- `rtc.now().minute()`
- `millis()`
- `MODO_DIAGNOSTICO_FIREBASE`

Saidas:

- Chamada a `salvarClimaNoFirebase()` na hora cheia.

Impacto:

- Alta criticidade.

Dependencias:

- RTC.
- DHT22.
- Firebase.

O que acontece se for alterada:

- Pode aumentar volume no Firebase, quebrar granularidade esperada ou perder registros horarios.

## Regra: Modo Diagnostico Grava Clima A Cada Minuto

Arquivo: `07_Tasks.ino`

Metodo: `executarTarefasPeriodicas()`

Objetivo:

- Quando `MODO_DIAGNOSTICO_FIREBASE` for `true`, gravar a cada minuto.

Entradas:

- `MODO_DIAGNOSTICO_FIREBASE`
- `millis()`

Saidas:

- Mais tentativas de gravacao no Firebase.

Impacto:

- Media criticidade.

Dependencias:

- Firebase.
- DHT22.

O que acontece se for alterada:

- Diagnostico pode ficar lento ou gerar excesso de dados.

## Regra: Firebase Usa Outbox Persistente E Escrita Idempotente

Arquivo: `05_Firebase.ino`

Metodos:

- `salvarClimaNoFirebase()`
- `salvarHorariosSolNoFirebase()`

Objetivo:

- Enviar com `Firebase.setJSON()` para destino deterministico e confirmar cada operacao.
- Se falhar, persistir tipo, caminho, conteudo JSON, timestamp original, ID e operacao em arquivo LittleFS atomico.
- Reenviar FIFO e remover somente apos sucesso confirmado; timeout ambiguo repete o mesmo destino e ID.

Entradas:

- Objetos JSON montados no firmware.

Saidas:

- Dados confirmados no Realtime Database ou preservados em LittleFS. Falha de persistencia local e erro critico serial.

Impacto:

- Critica.

Dependencias:

- Firebase ESP32 Client.
- Wi-Fi.

O que acontece se for alterada:

- `pushJSON` em cada tentativa pode criar duplicatas quando a resposta se perde depois do commit.

## Regra: Path De Temperatura

Arquivo: `05_Firebase.ino`

Metodo: `salvarClimaNoFirebase()`

Objetivo:

- Gravar clima em path historico por data e hora.

Entradas:

- `formatarData(rtc.now())`
- `formatarHoraMinuto(rtc.now())`

Saidas:

```text
/historico/Temperatura/DD-MM-AAAA/HH-MM/clima
```

Campos:

- `Temperatura`
- `Umidade`
- `Sensacao termica`

Impacto:

- Critica.

Dependencias:

- Consumidores do Firebase.

O que acontece se for alterada:

- Dashboards ou integracoes que leem esse caminho podem parar de encontrar dados.

## Regra: Path De Dados Solares

Arquivo: `05_Firebase.ino`

Metodo: `salvarHorariosSolNoFirebase()`

Objetivo:

- Gravar nascer, por do sol, amanhecer, zenite e anoitecer.

Entradas:

- `horariosSolHoje`
- `horariosCrepusculoHoje`
- `rtc.now()`

Saidas:

```text
/historico/NascePorDoSol/DD-MM-AAAA/solar
```

Campos:

- `HoraPorDoSol`
- `MinutePorDoSol`
- `HourNascerDoSol`
- `MinuteNascerDoSol`
- `HoraAmanhecer`
- `MinuteAmanhecer`
- `HoraZenite`
- `MinuteZenite`
- `HourAnoitecer`
- `MinuteAnoitecer`

Impacto:

- Critica.

Dependencias:

- Consumidores do Firebase.
- OpenWeather.
- Sunrise-Sunset.

O que acontece se for alterada:

- Leitores externos podem perder compatibilidade.

## Regra: Atualizacao Solar Diaria Depende De Duas APIs

Arquivo: `07_Tasks.ino`

Metodo: `executarTarefasPeriodicas()`

Objetivo:

- Salvar dados solares somente se OpenWeather e Sunrise-Sunset atualizarem com sucesso.

Entradas:

- `HORA_ATUALIZACAO_SOL_DIARIA = 4`
- `MINUTO_ATUALIZACAO_SOL_DIARIA = 30`

Saidas:

- Escrita solar no Firebase apenas se ambas as etapas retornarem `true`.

Impacto:

- Alta criticidade.

Dependencias:

- Wi-Fi.
- OpenWeather.
- Sunrise-Sunset.
- Firebase.

O que acontece se for alterada:

- Pode gravar zeros/defaults ou dados solares incompletos.

## Regra: OpenWeather Fornece Timezone Da Localizacao

Arquivo: `04_WeatherAndSun.ino`

Metodo: `atualizarHorariosSolDaApi()`

Objetivo:

- Usar `timezone` da coordenada consultada para converter sunrise/sunset.

Entradas:

- `sys.sunrise`
- `sys.sunset`
- `timezone`

Saidas:

- Horarios locais de nascer e por do sol.

Impacto:

- Alta criticidade.

Dependencias:

- OpenWeather.

O que acontece se for alterada:

- Consultas por CEP/localizacao fora do fuso padrao podem retornar horario errado.

## Regra: Sunrise-Sunset Usa ISO UTC Com Fuso Do OpenWeather

Arquivo: `04_WeatherAndSun.ino`

Metodo: `atualizarHorariosCrepusculoDaApi()`

Objetivo:

- Converter amanhecer, zenite e anoitecer de ISO UTC para fuso local.

Entradas:

- `astronomical_twilight_begin`
- `solar_noon`
- `astronomical_twilight_end`
- `horariosSolHoje.deslocamentoFusoSegundos`

Saidas:

- `horariosCrepusculoHoje`.

Impacto:

- Alta criticidade.

Dependencias:

- Sunrise-Sunset.
- OpenWeather executado antes.

O que acontece se for alterada:

- Horarios solares complementares podem ficar em UTC ou fuso incorreto.

## Regra: Visitantes Do Telegram Têm Comandos Limitados

Arquivo: `06_Telegram.ino`

Metodo: `processarComandoVisitante()`

Objetivo:

- Permitir visitantes apenas em `/start`, `localizacao`/`5` e CEP.

Entradas:

- Texto Telegram.
- Estado `aguardandoLocalizacao`.

Saidas:

- Respostas limitadas.

Impacto:

- Alta criticidade.

Dependencias:

- `remetenteAutorizado()`.

O que acontece se for alterada:

- Visitantes podem receber dados locais da estacao ou comandos nao previstos.

## Regra: Autorizados Têm Comandos Completos

Arquivo: `06_Telegram.ino`

Metodo: `processarComandoAutorizado()`

Objetivo:

- Permitir comandos locais e externos para remetentes autorizados.

Entradas:

- Texto Telegram.

Saidas:

- Nascer, por do sol, clima local, status, localizacao, CEP.

Impacto:

- Alta criticidade.

Dependencias:

- `VALID_SENDER_IDS`.

O que acontece se for alterada:

- Pode bloquear usuarios validos ou ampliar acesso.

## Regra: Localizacao Telegram Pode Ser Direta Ou Solicitada

Arquivo: `01_Utils.ino` e `06_Telegram.ino`

Metodos:

- `marcarAguardandoLocalizacao()`
- `limparAguardandoLocalizacao()`
- `aguardandoLocalizacao()`
- `ehMensagemLocalizacaoBruta()`
- `tratarComandoLocalizacao()`

Objetivo:

- Aceitar localizacao Telegram quando a mensagem traz latitude/longitude validas, mesmo que o usuario nao tenha pedido o botao antes.
- Manter o estado por `chatId` apenas para o fluxo em que o bot solicita localizacao pelo teclado.

Entradas:

- `idChat`
- mensagem Telegram com latitude/longitude.

Saidas:

- Chat marcado/limpo.
- Localizacao processada ou botao reenviado.

Impacto:

- Alta criticidade.

Dependencias:

- UniversalTelegramBot.

O que acontece se for alterada:

- Foto/sticker/audio podem ser interpretados como localizacao se a validacao de coordenadas for enfraquecida.
- Localizacao real pode cair no menu de comandos se voltar a depender obrigatoriamente do estado `aguardandoLocalizacao`.

## Regra: CEP Deve Ter 8 Digitos

Arquivo: `01_Utils.ino`

Metodo: `ehComandoCep()`

Objetivo:

- Validar CEP com ou sem hifen antes de chamar Nominatim.

Entradas:

- Texto Telegram.

Saidas:

- Booleano.

Impacto:

- Media criticidade.

Dependencias:

- Nominatim.

O que acontece se for alterada:

- Chamadas invalidas podem aumentar falhas de API.

## Regra: CEP Vira Coordenada Pelo Primeiro Resultado Do Nominatim

Arquivo: `06_Telegram.ino`

Metodo: `tratarComandoCep()`

Objetivo:

- Converter CEP recebido pelo Telegram em latitude/longitude para consultar clima e horarios solares.

Entradas:

- CEP validado por `ehComandoCep()`.
- Resposta JSON do Nominatim/OpenStreetMap.

Saidas:

- `Localizacao` montada com `doc[0]["lat"]` e `doc[0]["lon"]`.
- Resposta Telegram com sol e clima da coordenada encontrada.

Impacto:

- Alta criticidade.

Dependencias:

- Nominatim/OpenStreetMap.
- OpenWeather.
- `clienteHttps`.
- `USER_AGENT_OPENSTREETMAP`.

O que acontece se for alterada:

- Validar cidade/UF, trocar o provedor ou exigir maior precisao pode mudar respostas de CEP.
- O firmware atual nao usa ViaCEP e nao converte CEP para cidade/UF antes da coordenada.
- O firmware atual nao valida `postcode`, cidade, UF, `display_name`, `class`, `type` ou precisao do resultado.
- Como usa o primeiro resultado do Nominatim, a coordenada pode ser aproximada.

## Regra: Nominatim Exige User-Agent

Arquivo: `06_Telegram.ino`

Metodo: `tratarComandoCep()`

Objetivo:

- Enviar header `User-Agent` em consultas de CEP.

Entradas:

- `USER_AGENT_OPENSTREETMAP`

Saidas:

- Requisicao HTTP com header.

Impacto:

- Media criticidade.

Dependencias:

- Nominatim.

O que acontece se for alterada:

- Nominatim pode recusar ou limitar chamadas.

## Regra: Localizacao Telegram E Mais Precisa Que CEP

Arquivo: `06_Telegram.ino`

Metodo: `tratarComandoLocalizacao()`

Objetivo:

- Usar a latitude/longitude compartilhada pelo usuario no Telegram quando o chat esta aguardando localizacao.

Entradas:

- `bot.messages[indiceMensagem].latitude`
- `bot.messages[indiceMensagem].longitude`
- Estado `aguardandoLocalizacao(idChat)`.

Saidas:

- Consulta direta a OpenWeather para clima e horarios solares.
- Resposta Telegram para a coordenada recebida.

Impacto:

- Alta criticidade.

Dependencias:

- UniversalTelegramBot.
- OpenWeather.
- Estado por `chatId` em `01_Utils.ino`.

O que acontece se for alterada:

- Aceitar localizacao fora do fluxo esperado pode voltar a interpretar midias como coordenadas invalidas.
- Para consultas fora da localizacao padrao, compartilhar localizacao deve ser tratado como caminho mais preciso que CEP.

## Regra: Alerta Diario Depois Do Amanhecer

Arquivo: `07_Tasks.ino` e `06_Telegram.ino`

Metodos:

- `executarTarefasPeriodicas()`
- `enviarAlertaDiarioAmanhecer()`

Objetivo:

- Enviar alerta aos IDs autorizados uma vez por data, `MINUTOS_APOS_AMANHECER_ALERTA` depois do amanhecer astronomico.

Entradas:

- `horariosSolHoje`
- `climaAtual`
- `horariosCrepusculoHoje`
- `VALID_SENDER_IDS`

Saidas:

- Mensagem Telegram para cada ID autorizado.

Impacto:

- Media criticidade.

Dependencias:

- Telegram.
- RTC.

O que acontece se for alterada:

- Alertas podem duplicar, atrasar, antecipar ou deixar de ser enviados.
- Remover a validacao de crepusculo pode fazer o firmware disparar alerta com horarios zerados.

# REGRAS QUE NAO DEVEM SER ALTERADAS

- Paths Firebase de `Temperatura` e `NascePorDoSol`.
- Nomes dos campos Firebase ja consumidos externamente.
- Paths e nomes dos campos Firebase; IDs filhos deterministas permitem repetir sem duplicidade.
- Uso sincrono de `Firebase.setJSON()` com fila persistente nas gravacoes historicas.
- Validacao de leitura DHT contra NaN.
- Condicao de salvar solar apenas apos sucesso nas duas APIs.
- Uso do timezone da OpenWeather para horarios por coordenada.
- Aceite de localizacao Telegram valida, mesmo sem estado previo, mantendo `aguardandoLocalizacao` para o fluxo com botao.
- CEP como consulta aproximada via primeiro resultado do Nominatim; nao assumir coordenada exata sem implementar validacao adicional.
- Separacao entre usuario autorizado e visitante.
- Credenciais isoladas em `secrets.h`.

# MAPA DE IMPACTO

## Arquivo: estacao_climatica.ino

Responsabilidade:

- Configuracao global, estado e ciclo principal.

O que pode quebrar:

- Boot, pinos, endpoints, comandos, structs, objetos globais.

Dependencias afetadas:

- Todos os modulos.

Nivel de risco:

- Critico.

## Arquivo: 01_Utils.ino

Responsabilidade:

- Formatacao, tempo, URL, validacao, estado de localizacao.

O que pode quebrar:

- Paths Firebase, horarios, APIs, Telegram.

Dependencias afetadas:

- Firebase, Telegram, Weather/Sun.

Nivel de risco:

- Alto.

## Arquivo: 02_Connectivity.ino

Responsabilidade:

- Wi-Fi, TLS, RTC, NTP.

O que pode quebrar:

- Toda conectividade e horario.

Dependencias afetadas:

- Firebase, Telegram, APIs, tarefas.

Nivel de risco:

- Critico.

## Arquivo: 03_Sensors.ino

Responsabilidade:

- DHT22.

O que pode quebrar:

- Valores de clima local.

Dependencias afetadas:

- Firebase Temperatura, Telegram clima/status.

Nivel de risco:

- Medio.

## Arquivo: 04_WeatherAndSun.ino

Responsabilidade:

- OpenWeather e Sunrise-Sunset.

O que pode quebrar:

- Horarios solares, clima por CEP/localizacao, fuso horario.

Dependencias afetadas:

- Firebase solar, Telegram.

Nivel de risco:

- Alto.

## Arquivo: 05_Firebase.ino

Responsabilidade:

- Escrita no Firebase.

O que pode quebrar:

- Historico, contrato com web/integracoes externas.

Dependencias afetadas:

- Firebase Realtime Database.

Nivel de risco:

- Critico.

## Arquivo: 06_Telegram.ino

Responsabilidade:

- Bot Telegram.

O que pode quebrar:

- Comandos, permissoes, localizacao, CEP, alertas.

Dependencias afetadas:

- Telegram, APIs externas.

Nivel de risco:

- Alto.

## Arquivo: 07_Tasks.ino

Responsabilidade:

- Periodicidade e automacao.

O que pode quebrar:

- Frequencia de gravacao, atualizacao solar, alerta.

Dependencias afetadas:

- Sensor, Firebase, APIs, Telegram.

Nivel de risco:

- Alto.

# CALL GRAPH

```text
Arduino Runtime
  setup()
    Wire.begin()
    dht.begin()
    inicializarRtc()
    conectarWifi()
    clienteTempo.begin()
    sincronizarRtcComNtp(true)
    inicializarFirebase()
    atualizarHorariosSolDaApi(localizacaoAtual)
    atualizarHorariosCrepusculoDaApi(localizacaoAtual)
    lerSensorDht()

  loop()
    conectarWifi()
    sincronizarRtcComNtp()
    executarTarefasPeriodicas()
      lerSensorDht()
      salvarClimaNoFirebase()
      atualizarHorariosSolDaApi(localizacaoAtual)
      atualizarHorariosCrepusculoDaApi(localizacaoAtual)
      salvarHorariosSolNoFirebase()
      enviarAlertaDiarioAmanhecer()
    consultarMensagensTelegram()
      bot.getUpdates()
      tratarNovasMensagens()
        processarComandoAutorizado()
        processarComandoVisitante()
          tratarComandoLocalizacao()
          tratarComandoCep()
```

# GLOSSARIO

- DHT22: sensor de temperatura e umidade no GPIO 4.
- DS3231: RTC usado por I2C para data/hora.
- RTC: relogio de tempo real.
- NTP: protocolo de sincronizacao de horario.
- RTDB: Firebase Realtime Database.
- ID de operacao: chave fixa (`clima` por pasta de minuto e `solar` por pasta de dia) usada por `setJSON`; repeticoes no mesmo intervalo sobrescrevem o registro.
- OpenWeather: API usada para clima, sunrise, sunset e timezone.
- Sunrise-Sunset: API usada para amanhecer astronomico, zenite solar e anoitecer astronomico.
- Nominatim: servico OpenStreetMap usado para converter CEP em latitude/longitude.
- Zenite: campo `solar_noon` da Sunrise-Sunset, gravado como `HoraZenite` e `MinuteZenite`.
- Sensacao termica: calculada pelo DHT ou retornada pela OpenWeather dependendo do fluxo.
- Visitante: remetente Telegram fora de `VALID_SENDER_IDS`.
- Autorizado: remetente Telegram cujo `from_id` esta em `VALID_SENDER_IDS`.

# CODIGO MORTO

- `enviarAvisoAguardandoLocalizacao()` esta definido em `06_Telegram.ino`, mas nao foi encontrado uso direto no estado atual. POSSIVELMENTE NAO UTILIZADO.
- `MODO_DIAGNOSTICO_FIREBASE` permanece no codigo para alternar diagnostico; no estado atual esta `false`.
- `.theia/` nao participa do firmware. POSSIVELMENTE NAO UTILIZADO pelo build.
- Pastas `build-*`, quando existirem, sao saida de compilacao. POSSIVELMENTE NAO UTILIZADO como fonte.

# DIVIDA TECNICA

- Uso extensivo de `String`.
- TLS externo com `setInsecure()`.
- Boot bloqueante se RTC falha.
- Firebase e Telegram compartilham recursos de rede no mesmo loop.
- `consultarMensagensTelegram()` processa batches em `while (quantidadeMensagens)`, podendo ocupar o loop se houver muitas mensagens.
- Nomes de campos Firebase misturam portugues e ingles (`HourNascerDoSol`, `MinutePorDoSol`).

# TAREFAS COMUNS

## Alterar Graficos

Nao ha graficos neste firmware. Leia os consumidores externos do Firebase.

## Alterar Banco/Firebase

Leia:

- `05_Firebase.ino`
- `07_Tasks.ino`
- structs em `estacao_climatica.ino`
- `01_Utils.ino` para formatos de data/hora

## Alterar Autenticacao Telegram

Leia:

- `secrets.h`
- `01_Utils.ino` (`remetenteAutorizado`)
- `06_Telegram.ino`

## Alterar APIs

Leia:

- `estacao_climatica.ino` para endpoints
- `01_Utils.ino` para montagem de URLs
- `04_WeatherAndSun.ino`
- `06_Telegram.ino` para Nominatim

## Alterar Calculos De Horario

Leia:

- `01_Utils.ino`
- `04_WeatherAndSun.ino`
- `07_Tasks.ino`

## Alterar Sensores

Leia:

- `estacao_climatica.ino` para pino/tipo
- `03_Sensors.ino`
- `07_Tasks.ino`

## Alterar Frequencia De Gravacao

Leia:

- `07_Tasks.ino`
- constantes em `estacao_climatica.ino`
