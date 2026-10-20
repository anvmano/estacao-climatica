# Estação climática ESP32

Firmware Arduino para medir temperatura, umidade e sensação térmica com DHT22, manter data/hora com RTC DS3231 e NTP, consultar clima e horários solares, responder consultas pelo Telegram e registrar históricos no Firebase Realtime Database. Uma fila persistente em LittleFS preserva gravações pendentes para reenvio.

## Hardware identificado no código

- ESP32; os builds locais existentes usam `esp32:esp32:esp32` (ESP32 Dev Module). O modelo físico exato da placa não está documentado.
- DHT22 com sinal no GPIO 4 (`DHT_PIN = 4`, `DHT_TYPE = DHT22`).
- RTC DS3231 via I²C. O firmware chama `Wire.begin()` sem definir SDA/SCL; confirme os pinos padrão da placa selecionada. Não há esquema elétrico no projeto.
- Monitor serial: 9600 baud. Fuso configurado no firmware: UTC−3.
- LittleFS e Preferences do ESP32 para a fila local. A configuração de partição precisa disponibilizar espaço para LittleFS; o firmware usa a partição padrão `spiffs`.

## Bibliotecas

As bibliotecas externas identificadas e suas versões locais de referência são:

| Biblioteca | Versão local | Uso |
| --- | --- | --- |
| ArduinoJson | 7.4.3 | JSON; o código usa `JsonDocument` |
| DHT sensor library (Adafruit) | 1.4.7 | DHT22 |
| Adafruit Unified Sensor | 1.1.15 | Dependência do DHT |
| RTClib (Adafruit) | 2.1.4 | DS3231 |
| Adafruit BusIO | 1.17.4 | Dependência da RTClib |
| Firebase ESP32 Client (Mobizt) | 4.4.17 | `FirebaseESP32.h`, autenticação por token legado |
| UniversalTelegramBot | 1.3.0 | Bot Telegram |
| NTPClient | 3.2.1 | Sincronização NTP |

O pacote **esp32 by Espressif Systems**, versão local 2.0.14, fornece `WiFi`, `WiFiClientSecure`, `WiFiUdp`, `HTTPClient`, `Wire`, `LittleFS` e `Preferences`.

Os serviços utilizados são OpenWeather, Sunrise-Sunset, Nominatim/OpenStreetMap, NTP, Telegram e Firebase Realtime Database. Não foi identificada integração MQTT.

## Configuração local

1. Clone o repositório para uma pasta chamada `estacao_climatica`:

   ```sh
   git clone https://github.com/anvmano/estacao-climatica.git estacao_climatica
   cd estacao_climatica
   ```

2. Copie `secrets.example.h` para `secrets.h`. No PowerShell:

   ```powershell
   Copy-Item secrets.example.h secrets.h
   ```

3. Edite somente o arquivo local `secrets.h`, preenchendo:

   - `WIFI_SSID` e `WIFI_PASSWORD`: sua rede Wi-Fi.
   - `OPENWEATHER_API_KEY`: chave do OpenWeather.
   - `BOT_TOKEN`: token do seu bot Telegram.
   - `VALID_SENDER_IDS`: IDs dos usuários Telegram autorizados, mantendo os IDs como strings. `VALID_SENDER_ID_COUNT` é calculado pelo próprio código.
   - `FIREBASE_HOST`: URL do seu Realtime Database; `FIREBASE_AUTH`: token legado usado por este firmware.
   - `DEFAULT_LATITUDE`, `DEFAULT_LONGITUDE` e `DEFAULT_ALTITUDE`: localização e altitude da sua estação. Os valores `0.0` do exemplo são placeholders numéricos e devem ser substituídos.

O arquivo real `secrets.h` está no `.gitignore`. O exemplo contém apenas placeholders, inclusive para os IDs Telegram e a localização. Não coloque credenciais em documentação ou exemplos e não use `git add -f` para incluir arquivos ignorados.

## Compilação

No Arduino IDE, instale o pacote ESP32 e as bibliotecas acima pelo gerenciador, abra `estacao_climatica.ino` e selecione a placa correspondente ao seu hardware. Todos os arquivos `.ino` precisam permanecer juntos na pasta `estacao_climatica`. Para reproduzir o alvo dos builds locais, selecione ESP32 Dev Module.

Com Arduino CLI, depois de instalar o core e as bibliotecas e criar `secrets.h`:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 .
```

A gravação na placa depende da porta e do modelo físico usados. Após gravar, acompanhe a inicialização pelo monitor serial a 9600 baud. Se o DS3231 não for detectado, o firmware permanece aguardando sua inicialização.

## Organização

- `estacao_climatica.ino`: configuração, objetos, `setup()` e `loop()`.
- `01_Utils.ino` a `08_FirebaseQueue.ino`: utilitários, conectividade, sensores, APIs, Firebase, Telegram, tarefas e fila persistente.
- `secrets.example.h`: modelo sem credenciais para a configuração local.
- `docs/`: documentação técnica existente, incluindo arquitetura, regras e dependências.

A publicação preserva os arquivos de firmware, a pinagem, os tipos, as calibrações e a lógica. Credenciais, IDs pessoais e localização real não são distribuídos. Pastas de compilação, backups e configurações pessoais ficam fora do repositório.
