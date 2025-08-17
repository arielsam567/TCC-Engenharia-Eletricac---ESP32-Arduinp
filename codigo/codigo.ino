#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

// Definição dos pinos
const int entrada = 34;     // GPIO34 (entrada)
const int saida1 = 25;      // GPIO25 (relé 1)
const int saida2 = 32;      // GPIO32 (relé 2) 
const int saida3 = 2;       // GPIO2 (relé 3) - Controlado automaticamente pelo status do Bluetooth

// Configurações BLE
#define DEVICE_NAME "RelayTimer"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Estados da máquina de estados
enum Estados {
  IDLE,           // Aguardando comando START
  MODO_1,         // Retardo na energização
  MODO_2,         // Retardo na desenergização
  MODO_3,         // Cíclico com início ligado
  MODO_4,         // Cíclico com início desligado
  MODO_5          // Partida estrela-triângulo
};

// Estrutura para configuração dos relés
struct ConfigReles {
  int modo;
  unsigned long tempo1;  // em segundos
  unsigned long tempo2;  // em segundos
};

// Variáveis globais
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

Estados estadoAtual = IDLE;
ConfigReles config;
unsigned long tempoInicio = 0;
unsigned long tempoAtual = 0;
bool relesLigados = false;
bool modoEstrela = true; // true = estrela, false = triangulo

Preferences preferences;

// Declarações de funções (forward declarations)
void iniciarModo();
void executarMaquinaEstados();
void ligarRele(bool ligar);
void ligarReleEstrela();
void ligarReleTriangulo();
void enviarResposta(String resposta);
void enviarNotificacao(String notificacao);
bool processarConfiguracao(String comando);
void salvarConfiguracao();
void carregarConfiguracao();
void inicializarBLE();

// Callback para conexão BLE
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      // Ligar a porta 2 (GPIO2) quando Bluetooth conectar
      digitalWrite(saida3, HIGH);
      Serial.println("Dispositivo conectado! Porta 2 ligada.");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      // Desligar a porta 2 (GPIO2) quando Bluetooth desconectar
      digitalWrite(saida3, LOW);
      Serial.println("Dispositivo desconectado! Porta 2 desligada.");
    }
};

// Callback para característica BLE
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = String(pCharacteristic->getValue().c_str());
      
      if (rxValue.length() > 0) {
        Serial.println("Comando recebido: " + rxValue);
        
        if (rxValue == "START") {
          // Comando para iniciar execução
          if (estadoAtual == IDLE) {
            iniciarModo();
            enviarResposta("OK");
          } else {
            enviarResposta("ERR: Modo já em execução");
          }
        } else {
          // Comando de configuração
          if (processarConfiguracao(rxValue)) {
            salvarConfiguracao();
            enviarResposta("OK");
          } else {
            enviarResposta("ERR: Formato inválido");
          }
        }
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando RelayTimer...");
  
  // Configuração dos pinos
  pinMode(entrada, INPUT);
  pinMode(saida1, OUTPUT);
  pinMode(saida2, OUTPUT);
  pinMode(saida3, OUTPUT);
  
  // Inicializar relés desligados
  digitalWrite(saida1, LOW);
  digitalWrite(saida2, LOW);
  digitalWrite(saida3, LOW); // Porta 2 (GPIO2) inicia desligada
  
  // Carregar configuração salva
  carregarConfiguracao();
  
  // Inicializar BLE
  inicializarBLE();
  
  Serial.println("RelayTimer iniciado e aguardando conexão...");
}

void loop() {
  // Verificar conexão BLE
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // dar tempo para finalizar
    pServer->startAdvertising(); // reiniciar anúncio
    Serial.println("Reiniciando anúncio BLE");
    oldDeviceConnected = deviceConnected;
  }
  
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  // Executar máquina de estados
  executarMaquinaEstados();
  
  delay(100); // pequena pausa para estabilidade
}

void inicializarBLE() {
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                    
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  pServer->getAdvertising()->start();
  
  Serial.println("BLE iniciado, aguardando conexão...");
}

bool processarConfiguracao(String comando) {
  // Verificar se é comando de configuração (formato: "modo|tempo1|tempo2")
  int pipe1 = comando.indexOf('|');
  int pipe2 = comando.indexOf('|', pipe1 + 1);
  
  if (pipe1 == -1 || pipe2 == -1) {
    Serial.println("Formato inválido: " + comando);
    return false;
  }
  
  int modo = comando.substring(0, pipe1).toInt();
  unsigned long t1 = comando.substring(pipe1 + 1, pipe2).toInt();
  unsigned long t2 = comando.substring(pipe2 + 1).toInt();
  
  // Validar modo
  if (modo < 1 || modo > 5) {
    Serial.println("Modo inválido: " + String(modo));
    return false;
  }
  
  // Validar tempos (máximo 20 dias = 1.728.000 segundos)
  if (t1 > 1728000 || t2 > 1728000) {
    Serial.println("Tempo muito longo");
    return false;
  }
  
  config.modo = modo;
  config.tempo1 = t1;
  config.tempo2 = t2;
  
  Serial.println("Configuração válida - Modo: " + String(modo) + 
                 ", T1: " + String(t1) + "s, T2: " + String(t2) + "s");
  
  return true;
}

void salvarConfiguracao() {
  preferences.begin("relaytimer", false);
  preferences.putInt("modo", config.modo);
  preferences.putULong("tempo1", config.tempo1);
  preferences.putULong("tempo2", config.tempo2);
  preferences.end();
  Serial.println("Configuração salva na EEPROM");
}

void carregarConfiguracao() {
  preferences.begin("relaytimer", true);
  config.modo = preferences.getInt("modo", 1);
  config.tempo1 = preferences.getULong("tempo1", 300);
  config.tempo2 = preferences.getULong("tempo2", 0);
  preferences.end();
  
  Serial.println("Configuração carregada - Modo: " + String(config.modo) + 
                 ", T1: " + String(config.tempo1) + "s, T2: " + String(config.tempo2) + "s");
}

void iniciarModo() {
  estadoAtual = (Estados)config.modo;
  tempoInicio = millis();
  tempoAtual = 0;
  relesLigados = false;
  modoEstrela = true;
  
  Serial.println("Iniciando modo " + String(config.modo));
  
  // Configurar estado inicial baseado no modo
  switch (estadoAtual) {
    case MODO_1: // Retardo na energização - inicia desligado
      ligarRele(false);
      break;
    case MODO_2: // Retardo na desenergização - inicia ligado
      ligarRele(true);
      break;
    case MODO_3: // Cíclico com início ligado
      ligarRele(true);
      break;
    case MODO_4: // Cíclico com início desligado
      ligarRele(false);
      break;
    case MODO_5: // Partida estrela-triângulo - inicia em estrela
      ligarReleEstrela();
      break;
  }
}

void executarMaquinaEstados() {
  if (estadoAtual == IDLE) return;
  
  tempoAtual = (millis() - tempoInicio) / 1000; // converter para segundos
  
  switch (estadoAtual) {
    case MODO_1: // Retardo na energização
      if (tempoAtual >= config.tempo1) {
        ligarRele(true);
        estadoAtual = IDLE;
        Serial.println("Modo 1 concluído - Relés ligados");
      }
      break;
      
    case MODO_2: // Retardo na desenergização
      if (tempoAtual >= config.tempo2) {
        ligarRele(false);
        estadoAtual = IDLE;
        Serial.println("Modo 2 concluído - Relés desligados");
      }
      break;
      
    case MODO_3: // Cíclico com início ligado
      if (relesLigados && tempoAtual >= config.tempo1) {
        ligarRele(false);
        tempoInicio = millis();
        tempoAtual = 0;
      } else if (!relesLigados && tempoAtual >= config.tempo2) {
        ligarRele(true);
        tempoInicio = millis();
        tempoAtual = 0;
      }
      break;
      
    case MODO_4: // Cíclico com início desligado
      if (!relesLigados && tempoAtual >= config.tempo2) {
        ligarRele(true);
        tempoInicio = millis();
        tempoAtual = 0;
      } else if (relesLigados && tempoAtual >= config.tempo1) {
        ligarRele(false);
        tempoInicio = millis();
        tempoAtual = 0;
      }
      break;
      
    case MODO_5: // Partida estrela-triângulo
      if (modoEstrela && tempoAtual >= config.tempo1) {
        ligarReleTriangulo();
        modoEstrela = false;
        tempoInicio = millis();
        tempoAtual = 0;
        Serial.println("Transição para triângulo");
      }
      break;
  }
}

void ligarRele(bool ligar) {
  relesLigados = ligar;
  
  if (ligar) {
    digitalWrite(saida1, HIGH);
    digitalWrite(saida2, HIGH);
    // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
    Serial.println("Relés LIGADOS");
    enviarNotificacao("ON");
  } else {
    digitalWrite(saida1, LOW);
    digitalWrite(saida2, LOW);
    // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
    Serial.println("Relés DESLIGADOS");
    enviarNotificacao("OFF");
  }
}

void ligarReleEstrela() {
  // Modo estrela: apenas relé 1 ligado
  digitalWrite(saida1, HIGH);
  digitalWrite(saida2, LOW);
  // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
  relesLigados = true;
  Serial.println("Modo estrela ativado");
  enviarNotificacao("ON");
}

void ligarReleTriangulo() {
  // Modo triângulo: relés 2 e 3 ligados
  digitalWrite(saida1, LOW);
  digitalWrite(saida2, HIGH);
  // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
  relesLigados = true;
  Serial.println("Modo triângulo ativado");
  enviarNotificacao("ON");
}

void enviarResposta(String resposta) {
  if (deviceConnected && pCharacteristic != NULL) {
    pCharacteristic->setValue(resposta.c_str());
    pCharacteristic->notify();
    Serial.println("Resposta enviada: " + resposta);
  }
}

void enviarNotificacao(String notificacao) {
  if (deviceConnected && pCharacteristic != NULL) {
    pCharacteristic->setValue(notificacao.c_str());
    pCharacteristic->notify();
    Serial.println("Notificação enviada: " + notificacao);
  }
}
