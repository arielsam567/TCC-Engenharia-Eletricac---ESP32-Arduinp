// Inclui a biblioteca para comunicação Bluetooth Serial
#include "BluetoothSerial.h"

// Verifica se o Bluetooth está realmente disponível no microcontrolador
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// Cria um objeto para controlar o Bluetooth Serial
BluetoothSerial SerialBT;

void setup() {
  // Inicia a comunicação Serial com o computador (para debug)
  // A velocidade 115200 é padrão para o ESP32
  Serial.begin(115200);

  // Inicia o dispositivo Bluetooth com um nome visível para outros dispositivos
  // Você pode alterar o nome "ESP32_DevKit" para o que preferir
  SerialBT.begin("ESP32_DevKit"); 
  
  Serial.println("O dispositivo foi iniciado, agora você pode pareá-lo por Bluetooth!");
  Serial.println("Abra o Monitor Serial para ver os dados recebidos e enviar dados.");
}

void loop() {
  // --- Recebimento de Dados (Celular -> ESP32) ---
  // Se houver algum dado disponível para ser lido na conexão Bluetooth...
  if (SerialBT.available()) {
    // ...lê o dado recebido e o escreve no Monitor Serial.
    // Isso permite que você veja no computador o que o celular enviou.
    char dadoRecebido = SerialBT.read();
    Serial.print("Recebido: ");
    Serial.write(dadoRecebido);
    Serial.println(); // Pula uma linha para melhor formatação
  }

  // --- Envio de Dados (ESP32 -> Celular) ---
  // Se houver algum dado disponível para ser lido no Monitor Serial...
  // (ou seja, se você digitou algo no Monitor Serial e apertou Enter)
  if (Serial.available()) {
    // ...lê o dado do Monitor Serial e o envia via Bluetooth.
    // Isso permite que você envie comandos do computador para o celular.
    char dadoEnviado = Serial.read();
    SerialBT.write(dadoEnviado);
  }
}