// Le o DHT22 e mantem a ultima leitura valida em climaAtual.
bool lerSensorDht() {
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  // Quando o DHT falha, ele retorna NaN. Nao atualizamos o estado nesse caso.
  if (isnan(umidade) || isnan(temperatura)) {
    LOG_PRINTLN("Falha ao ler o sensor DHT.");
    return false;
  }

  climaAtual.umidade = umidade;
  climaAtual.temperatura = temperatura;
  climaAtual.sensacaoTermica = round(dht.computeHeatIndex(temperatura, umidade, false) * 100) / 100.0;

  LOG_PRINT("Temperatura: ");
  LOG_PRINTLN(climaAtual.temperatura);
  LOG_PRINT("Umidade: ");
  LOG_PRINTLN(climaAtual.umidade);
  LOG_PRINT("Sensacao termica: ");
  LOG_PRINTLN(climaAtual.sensacaoTermica);

  return true;
}
