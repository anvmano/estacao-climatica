# AI Quick Context

## O Que E O Sistema

Projeto Arduino/ESP32 de uma estacao climatica. O firmware roda em um ESP32, le um sensor DHT22, usa um RTC DS3231 para data/hora local, sincroniza o RTC via NTP, consulta APIs externas para clima e eventos solares, responde comandos via Telegram e grava historico no Firebase Realtime Database.

O sketch principal e `estacao_climatica.ino`. As demais abas `.ino` dividem utilitarios, conectividade, sensores, APIs, Firebase, Telegram e tarefas periodicas.

## Tecnologias

- Arduino C++ para ESP32.
- Core `esp32:esp32` instalado: 2.0.14.
- FQBN usado em compilacao: `esp32:esp32:esp32`.
- Firebase Realtime Database via `Firebase ESP32 Client`.
- Telegram Bot via `UniversalTelegramBot`.
- HTTP/HTTPS via `HTTPClient` e `WiFiClientSecure`.
- JSON via `ArduinoJson`.
- NTP via `NTPClient`.
- RTC DS3231 via `RTClib` e I2C/Wire.
- Sensor DHT22 via `DHT sensor library`.

## Arquitetura Resumida

`setup()` inicializa I2C, serial condicional, DHT22, RTC, Wi-Fi, NTP, Firebase, horarios solares iniciais e uma primeira leitura DHT.

`loop()` chama quatro rotinas:

- `conectarWifi()`
- `sincronizarRtcComNtp()`
- `executarTarefasPeriodicas()`
- `consultarMensagensTelegram()`

O firmware usa estado global para leitura climatica atual, horarios solares do dia, horarios de crepusculo/zenite, localizacao padrao da estacao e chats que aguardam localizacao no Telegram.

## Fluxo Principal

1. ESP32 inicia.
2. `Wire.begin()` prepara I2C.
3. DHT22 e RTC DS3231 sao inicializados.
4. Wi-Fi conecta usando credenciais de `secrets.h` e hostname `estacao-climatica-esp32`.
5. `clienteTempo.begin()` inicia NTPClient.
6. RTC e ajustado por NTP quando possivel.
7. Firebase e inicializado com URL e token legado de `secrets.h`.
8. OpenWeather busca nascer/por do sol e timezone da localizacao padrao.
9. Sunrise-Sunset busca amanhecer, zenite solar e anoitecer.
10. DHT22 faz leitura inicial.
11. LittleFS e inicializado; uma particao nova pode ser formatada na primeira inicializacao, mas uma fila previamente inicializada nunca e formatada automaticamente.
12. `loop()` mantem Wi-Fi, NTP, fila Firebase, tarefas periodicas e polling Telegram.

## Componentes Criticos

- `estacao_climatica.ino`: includes, constantes, structs, objetos globais, `setup()` e `loop()`.
- `01_Utils.ino`: formatacao, validacoes, montagem de URLs, conversao de horario/fuso, controle do estado de localizacao no Telegram.
- `02_Connectivity.ino`: Wi-Fi, TLS, RTC e NTP.
- `03_Sensors.ino`: leitura DHT22.
- `04_WeatherAndSun.ino`: OpenWeather e Sunrise-Sunset.
- `05_Firebase.ino`: inicializacao e escrita em Firebase.
- `06_Telegram.ino`: comandos, permissao, fluxo de localizacao e CEP.
- `07_Tasks.ino`: gravacao periodica, atualizacao solar diaria e alerta Telegram.
- `08_FirebaseQueue.ino`: outbox persistente FIFO em LittleFS e reenvio com backoff.
- `secrets.h`: credenciais e configuracoes sensiveis.

## Onde Estao As Coisas

- Configuracoes de pinos e intervalos: `estacao_climatica.ino`.
- Credenciais: `secrets.h`.
- Paths Firebase: `05_Firebase.ino`.
- Endpoints externos: `estacao_climatica.ino`.
- Montagem de URLs: `01_Utils.ino`.
- Logica de Telegram: `06_Telegram.ino`.
- Frequencia de gravacao: `07_Tasks.ino`.
- Fila e sincronizacao Firebase: `08_FirebaseQueue.ino`.
- Validacao de CEP/localizacao: `01_Utils.ino`.

## Fluxos Criticos

- Gravacao de clima: `executarTarefasPeriodicas()` -> `lerSensorDht()` -> `salvarClimaNoFirebase()` -> `/historico/Temperatura/DD-MM-AAAA/HH-MM/id`.
- Gravacao solar diaria: `executarTarefasPeriodicas()` -> `atualizarHorariosSolDaApi()` -> `atualizarHorariosCrepusculoDaApi()` -> `salvarHorariosSolNoFirebase()` -> `/historico/NascePorDoSol/DD-MM-AAAA/id`.
- Telegram autorizado: `consultarMensagensTelegram()` -> `tratarNovasMensagens()` -> `processarComandoAutorizado()`.
- Telegram visitante: `consultarMensagensTelegram()` -> `tratarNovasMensagens()` -> `processarComandoVisitante()`.
- Consulta por CEP: `tratarComandoCep()` -> Nominatim `doc[0].lat/lon` -> OpenWeather -> resposta Telegram aproximada.
- Consulta por localizacao: mensagem Telegram com `latitude/longitude` validas -> `tratarComandoLocalizacao()` -> OpenWeather -> resposta Telegram. O envio direto de localizacao e aceito mesmo sem estado previo.
- CEP no Telegram e tratado como geocodificacao por Nominatim: o firmware envia o CEP como `postalcode`, usa o primeiro resultado que tenha `lat`/`lon` e consulta OpenWeather por essas coordenadas. O codigo atual nao valida `postcode` retornado, nao confirma cidade/UF e nao mede precisao; portanto CEP pode gerar coordenada aproximada. A localizacao compartilhada pelo Telegram e o fluxo mais preciso.

## Arquivos Mais Importantes

1. `estacao_climatica.ino`
2. `06_Telegram.ino`
3. `05_Firebase.ino`
4. `04_WeatherAndSun.ino`
5. `07_Tasks.ino`
6. `01_Utils.ino`
7. `02_Connectivity.ino`
8. `03_Sensors.ino`
9. `secrets.h`

## Regras Criticas

- `DEBUG_SERIAL` esta em `1`; diagnosticos de conexao e fila aparecem em Serial a 9600 baud.
- Escritas historicas usam `setJSON`: clima sobrescreve o filho `clima` na pasta de data/hora, e dados solares sobrescrevem o filho `solar` na pasta do dia. Falhas sao persistidas em LittleFS.
- A fila ocupa no maximo 256 KiB e processa um registro por tentativa. Fila cheia ou indisponivel gera erro critico serial e nao descarta pendencias antigas.
- Um sucesso confirmado remove apenas o arquivo daquela operacao. Timeout ambiguo repete o mesmo destino/ID, tornando a escrita idempotente.
- Leitura climatica so e gravada se `lerSensorDht()` retornar sucesso.
- Dados solares so sao gravados se OpenWeather e Sunrise-Sunset retornarem sucesso.
- Alerta diario e enviado somente se amanhecer/zenite/anoitecer estiverem carregados, alguns minutos depois do amanhecer.
- Visitantes do Telegram so recebem comandos de localizacao/CEP/start.
- Usuarios autorizados sao comparados contra `VALID_SENDER_IDS`.
- `MODO_DIAGNOSTICO_FIREBASE = false`; em modo normal grava clima apenas na hora cheia.
- RTC DS3231 ausente trava o firmware em loop com delay.
- O comando CEP depende do primeiro resultado do Nominatim com `lat`/`lon`; se nao houver resultado ou coordenadas, o bot pede uso de localizacao.

## Resumo Para IA

Este e um sketch Arduino ESP32 modularizado em abas `.ino`. O sistema depende de Wi-Fi, RTC DS3231, DHT22, Firebase, Telegram, OpenWeather, Sunrise-Sunset e Nominatim. O contrato de dados com Firebase e central: `Temperatura` e `NascePorDoSol` sao os caminhos historicos. Evite alterar nomes de campos Firebase sem revisar consumidores externos. Para mudancas de Telegram, leia `06_Telegram.ino` e `01_Utils.ino`. Para mudancas Firebase, leia `05_Firebase.ino` e `08_FirebaseQueue.ino`; as gravacoes usam LittleFS na particao `spiffs` selecionada pela placa.
