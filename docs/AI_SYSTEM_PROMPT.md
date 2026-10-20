# AI System Prompt

Voce esta trabalhando no projeto `estacao_climatica`, um firmware Arduino/ESP32.

## Objetivo Do Sistema

O sistema roda em um ESP32 e executa uma estacao climatica. Ele le um DHT22, usa RTC DS3231, sincroniza horario via NTP, consulta APIs externas para clima e dados solares, grava historico no Firebase Realtime Database e responde comandos via Telegram.

## Tecnologias

- Arduino C++.
- ESP32 core `esp32:esp32` 2.0.14 no ambiente analisado.
- FQBN usado: `esp32:esp32:esp32`.
- DHT sensor library 1.4.7.
- RTClib 2.1.4.
- NTPClient 3.2.1.
- Firebase ESP32 Client 4.4.17.
- UniversalTelegramBot 1.3.0.
- ArduinoJson 7.4.3.
- HTTPClient, WiFi, WiFiClientSecure, WiFiUdp, Wire.

## Arquivos Importantes

- `estacao_climatica.ino`: principal, constantes, structs, estado global, setup/loop.
- `01_Utils.ino`: tempo, formatacao, URLs, validacoes, estado de localizacao Telegram.
- `02_Connectivity.ino`: Wi-Fi, TLS, RTC, NTP.
- `03_Sensors.ino`: DHT22.
- `04_WeatherAndSun.ino`: OpenWeather e Sunrise-Sunset.
- `05_Firebase.ino`: contrato de escrita Firebase.
- `06_Telegram.ino`: bot, comandos, visitantes/autorizados, localizacao e CEP.
- `07_Tasks.ino`: tarefas periodicas.
- `08_FirebaseQueue.ino`: fila persistente Firebase FIFO em LittleFS.
- `secrets.h`: credenciais e configuracoes sensiveis. Nunca exponha valores.

## Fluxos Principais

### Boot

`setup()` inicializa I2C, DHT22, RTC, Wi-Fi, NTP, Firebase, horarios solares iniciais e uma leitura DHT.

### Loop

`loop()` chama:

- `conectarWifi()`
- `sincronizarRtcComNtp()`
- `executarTarefasPeriodicas()`
- `consultarMensagensTelegram()`

### Gravacao De Clima

`executarTarefasPeriodicas()` grava clima na hora cheia quando `MODO_DIAGNOSTICO_FIREBASE` e `false`. Fluxo:

```text
lerSensorDht() -> salvarClimaNoFirebase() -> /historico/Temperatura/DD-MM-AAAA/HH-MM/clima
```

### Gravacao Solar

Uma vez por dia as 04:30:

```text
atualizarHorariosSolDaApi()
atualizarHorariosCrepusculoDaApi()
salvarHorariosSolNoFirebase()
```

O path e:

```text
/historico/NascePorDoSol/DD-MM-AAAA/solar
```

### Telegram

- Autorizados recebem comandos completos.
- Visitantes recebem `/start`, `localizacao`/`5` e CEP.
- CEP usa Nominatim, pega o primeiro `lat/lon` retornado e depois consulta OpenWeather. Essa coordenada pode ser aproximada.
- Localizacao usa coordenadas enviadas pelo Telegram e depois OpenWeather.

## Regras Criticas

- Nao altere paths Firebase sem revisar consumidores externos.
- Preserve `Firebase.setJSON()` sincrono com ID deterministico e a fila LittleFS; push em retries pode duplicar gravacoes apos timeout ambiguo.
- A fila usa no maximo 256 KiB, reenvia FIFO com backoff e remove so apos confirmacao. Fila cheia ou FS indisponivel e erro critico serial, sem apagar pendencias antigas.
- Nao grave clima se `lerSensorDht()` falhar.
- Nao grave dados solares se OpenWeather ou Sunrise-Sunset falharem.
- Preserve isolamento de credenciais em `secrets.h`.
- Preserve aceite de localizacao Telegram com coordenadas validas, mesmo sem estado previo.
- Preserve `aguardandoLocalizacao` por `chatId` para o fluxo com botao.
- Preserve `User-Agent` em Nominatim.
- Nao descreva CEP como coordenada exata; no codigo atual ele e geocodificacao aproximada pelo primeiro resultado do Nominatim.
- Preserve timezone retornado pela OpenWeather para coordenadas consultadas.
- `DEBUG_SERIAL` esta `1` para diagnosticar conexao e sincronizacao Firebase; baud 9600.
- `MODO_DIAGNOSTICO_FIREBASE` deve ficar `false` em modo normal.
- Alerta diario deve usar amanhecer valido e disparar alguns minutos depois dele.

## Nunca Altere

- Valores secretos em `secrets.h` sem pedido explicito.
- Nomes dos campos Firebase ja existentes:
  - `Temperatura`
  - `Umidade`
  - `Sensacao termica`
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
- Paths historicos `/historico/Temperatura` e `/historico/NascePorDoSol`, salvo pedido explicito.

## Pontos De Atencao

- Sketch usa 97% da flash no build com fila; restam cerca de 27 KiB na particao de app.
- `clienteHttps.setInsecure()` usa TLS sem validacao de certificado.
- `inicializarRtc()` trava se DS3231 nao responder.
- O projeto usa `String` em varios pontos.
- Pastas `build-*` sao temporarias.
- A pasta `.theia` e metadado de editor.

## Estrategia Recomendada Para Alteracoes

1. Leia primeiro o arquivo responsavel direto.
2. Leia `estacao_climatica.ino` para entender estado global e constantes.
3. Para Firebase, leia tambem consumidores externos se disponiveis.
4. Mantenha compatibilidade de paths/campos.
5. Compile com:

```text
arduino-cli compile --fqbn esp32:esp32:esp32 .
```

6. Se upload for necessario, use a porta serial detectada por `arduino-cli board list`.

## Checklist Antes De Modificar Codigo

- Identifique se a mudanca afeta Firebase, Telegram, APIs, sensor ou RTC.
- Verifique se mexe em nomes de campos.
- Verifique se mexe em frequencia de gravacao.
- Verifique se mexe em permissao de Telegram.
- Verifique se mexe em `secrets.h`; se sim, nao exponha valores.
- Verifique se ha risco de aumentar flash.

## Checklist Antes De Finalizar Alteracoes

- Compilacao Arduino passou.
- Build temporario foi removido se criado.
- `DEBUG_SERIAL` esta no valor desejado.
- `MODO_DIAGNOSTICO_FIREBASE` esta no valor desejado.
- Paths Firebase continuam compativeis.
- Telegram autorizado/visitante continua coerente.
- Nao ha credenciais expostas na resposta.
