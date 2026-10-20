#include <ArduinoJson.h>
#include <DHT.h>
#include <FirebaseESP32.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <RTClib.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <Wire.h>

#include "secrets.h"

#define DEBUG_SERIAL 1

#if DEBUG_SERIAL
#define LOG_BEGIN(baud) Serial.begin(baud)
#define LOG_PRINT(valor) Serial.print(valor)
#define LOG_PRINTLN(valor) Serial.println(valor)
#else
#define LOG_BEGIN(baud) do {} while (0)
#define LOG_PRINT(valor) do {} while (0)
#define LOG_PRINTLN(valor) do {} while (0)
#endif

// Configuracoes gerais do hardware e dos intervalos de execucao.
constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t DHT_TYPE = DHT22;
constexpr char NOME_HOST_WIFI[] = "estacao-climatica-esp32";
constexpr int DESLOCAMENTO_FUSO_SEGUNDOS = -3 * 3600;
constexpr uint32_t INTERVALO_CONSULTA_TELEGRAM_MS = 1000;
constexpr uint32_t UM_MINUTO_MS = 60000;
constexpr bool MODO_DIAGNOSTICO_FIREBASE = false;
constexpr uint32_t INTERVALO_SINCRONIA_NTP_MS = 10 * UM_MINUTO_MS;
constexpr uint32_t TEMPO_LIMITE_CONEXAO_WIFI_MS = 15000;
constexpr uint32_t INTERVALO_TENTATIVA_WIFI_MS = 30000;
constexpr uint16_t TEMPO_LIMITE_HTTP_MS = 5000;
constexpr uint8_t HORA_ATUALIZACAO_SOL_DIARIA = 4;
constexpr uint8_t MINUTO_ATUALIZACAO_SOL_DIARIA = 30;
constexpr uint8_t MINUTOS_APOS_AMANHECER_ALERTA = 15;
constexpr size_t TAMANHO_BUFFER_URL = 220;
constexpr size_t MAX_CHATS_AGUARDANDO_LOCALIZACAO = 5;

// Endpoints externos usados pelo sketch.
const char URL_BUSCA_OPENSTREETMAP[] = "https://nominatim.openstreetmap.org/search?format=json&countrycodes=br&limit=1&postalcode=";
const char USER_AGENT_OPENSTREETMAP[] = "EstacaoClimaticaESP32/1.0";
const char URL_OPENWEATHER[] = "https://api.openweathermap.org/data/2.5/weather?lat=";
const char URL_SUNRISE_SUNSET[] = "https://api.sunrise-sunset.org/json";
const char SERVIDOR_NTP[] = "pool.ntp.org";

// Textos aceitos pelo bot do Telegram.
const String COMANDO_NASCER_SOL = "nascer do sol";
const String COMANDO_POR_SOL = "por do sol";
const String COMANDO_STATUS = "status";
const String COMANDO_CLIMA = "clima";
const String COMANDO_LOCALIZACAO = "localizacao";
const String COMANDO_START = "/start";
const String COMANDO_CEP = "CEP";

// Estruturas pequenas deixam claro quais valores caminham juntos.
struct Localizacao {
  double latitude;
  double longitude;
  double altitude;
};

struct HorariosSol {
  int horaNascerSol = 0;
  int minutoNascerSol = 0;
  int segundoNascerSol = 0;
  int horaPorSol = 0;
  int minutoPorSol = 0;
  int segundoPorSol = 0;
  int deslocamentoFusoSegundos = DESLOCAMENTO_FUSO_SEGUNDOS;
};

struct HorariosCrepusculo {
  int horaAmanhecer = 0;
  int minutoAmanhecer = 0;
  int horaZenite = 0;
  int minutoZenite = 0;
  int horaAnoitecer = 0;
  int minutoAnoitecer = 0;
};

struct LeituraClima {
  float temperatura = NAN;
  float umidade = NAN;
  float sensacaoTermica = NAN;
};

// Instancias das bibliotecas que conversam com sensores e servicos externos.
DHT dht(DHT_PIN, DHT_TYPE);
FirebaseData dadosFirebase;
FirebaseAuth authFirebase;
FirebaseConfig configFirebase;
RTC_DS3231 rtc;
WiFiClientSecure clienteSeguro;
WiFiClientSecure clienteHttps;
WiFiUDP udpNtp;
NTPClient clienteTempo(udpNtp, SERVIDOR_NTP, DESLOCAMENTO_FUSO_SEGUNDOS, UM_MINUTO_MS);
UniversalTelegramBot bot(BOT_TOKEN, clienteSeguro);

// Estado atual da estacao, atualizado por sensores, APIs e comandos do bot.
Localizacao localizacaoAtual = { DEFAULT_LATITUDE, DEFAULT_LONGITUDE, DEFAULT_ALTITUDE };
HorariosSol horariosSolHoje;
HorariosCrepusculo horariosCrepusculoHoje;
LeituraClima climaAtual;
String chatsAguardandoLocalizacao[MAX_CHATS_AGUARDANDO_LOCALIZACAO];

// Marcadores usados com millis() para evitar delay() no loop principal.
uint32_t ultimoConsultaTelegramMs = 0;
uint32_t ultimaLeituraClimaMs = 0;
uint32_t ultimaAtualizacaoSolMs = 0;
uint32_t ultimoAlertaNascerSolMs = 0;
uint32_t ultimaSincroniaNtpMs = 0;
uint32_t ultimaTentativaWifiMs = 0;

const char* DIAS_DA_SEMANA[] = {
  "Domingo",
  "Segunda",
  "Terca",
  "Quarta",
  "Quinta",
  "Sexta",
  "Sabado"
};

void conectarWifi();
void inicializarRtc();
void sincronizarRtcComNtp(bool forcar = false);
bool lerSensorDht();
bool atualizarHorariosSolDaApi(const Localizacao& localizacao);
bool atualizarHorariosSolDaApi(const Localizacao& localizacao, HorariosSol& horariosSol);
bool atualizarHorariosCrepusculoDaApi(const Localizacao& localizacao);
void inicializarFirebase();
void inicializarFilaFirebase();
void processarFilaFirebase();
bool salvarHorariosSolNoFirebase();
bool salvarClimaNoFirebase();
bool filaFirebaseTemPendencias();
bool filaFirebaseAdicionar(const String& tipo, const String& caminho, const String& operacaoId, uint32_t timestamp, const String& conteudoJson);
void executarTarefasPeriodicas();
void consultarMensagensTelegram();
bool wifiConectado();
bool horariosCrepusculoValidos();
void enviarAlertaDiarioAmanhecer();

#if DEBUG_SERIAL
void imprimirDiagnosticoSerial() {
  static uint32_t ultimoDiagnosticoMs = 0;
  uint32_t agoraMs = millis();
  if (agoraMs - ultimoDiagnosticoMs < 5000) {
    return;
  }
  ultimoDiagnosticoMs = agoraMs;

  DateTime agora = rtc.now();
  Serial.print("[DIAG] uptime_ms=");
  Serial.print(agoraMs);
  Serial.print(" wifi=");
  Serial.print(wifiConectado() ? "ok" : "off");
  Serial.print(" firebase=");
  Serial.print(Firebase.ready() ? "ok" : "off");
  Serial.print(" rtc=");
  Serial.print(formatarData(agora));
  Serial.print(" ");
  Serial.print(formatarHoraMinuto(agora));
  Serial.print(" clima_valido=");
  Serial.println(leituraClimaValida(climaAtual) ? "sim" : "nao");
}
#endif

// setup() roda uma vez: inicializa hardware, rede, horario e primeiras leituras.
void setup() {
  Wire.begin();
  LOG_BEGIN(9600);
  delay(1000);
  LOG_PRINTLN("Inicializando estacao climatica...");

  dht.begin();
  inicializarRtc();
  conectarWifi();

  clienteTempo.begin();
  sincronizarRtcComNtp(true);

  inicializarFirebase();
  inicializarFilaFirebase();

  bool horariosSolAtualizados = atualizarHorariosSolDaApi(localizacaoAtual);
  bool horariosCrepusculoAtualizados = false;
  if (horariosSolAtualizados) {
    horariosCrepusculoAtualizados = atualizarHorariosCrepusculoDaApi(localizacaoAtual);
  }
  if (!horariosSolAtualizados || !horariosCrepusculoAtualizados) {
    LOG_PRINTLN("Horarios solares iniciais indisponiveis. Nova tentativa sera feita na rotina diaria.");
  }
  lerSensorDht();
}

// loop() fica curto de proposito: cada responsabilidade mora em uma aba propria.
void loop() {
  conectarWifi();
  sincronizarRtcComNtp();
  processarFilaFirebase();
  executarTarefasPeriodicas();
  consultarMensagensTelegram();
#if DEBUG_SERIAL
  imprimirDiagnosticoSerial();
#endif
}
