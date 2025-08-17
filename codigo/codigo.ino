// Inclui a biblioteca para comunicação Bluetooth Serial
#include "BluetoothSerial.h"
#include <Preferences.h>

// Verifica se o Bluetooth está realmente disponível no microcontrolador
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// Cria um objeto para controlar o Bluetooth Serial
BluetoothSerial SerialBT;

// Definição dos pinos
const int entrada = 34;     // GPIO34 (entrada)
const int saida1 = 25;      // GPIO25 (relé 1)
const int saida2 = 32;      // GPIO32 (relé 2) 
const int saida3 = 2;       // GPIO2 (relé 3) - Controlado automaticamente pelo status do Bluetooth
const char* btName = "RELÉ MULTIFUNCIONAL - TCC ";
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
bool deviceConnected = false;
bool oldDeviceConnected = false;
String comandoRecebido = "";

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
void verificarConexaoBluetooth();
void processarComandosRecebidos();

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
  
  // Inicializar Bluetooth
  SerialBT.begin(btName); 
  
  Serial.println("RelayTimer iniciado e aguardando conexão Bluetooth...");
  Serial.println("Nome do dispositivo: RelayTimer");
}

void loop() {
  // Verificar conexão Bluetooth
  verificarConexaoBluetooth();
  
  // Processar comandos recebidos
  processarComandosRecebidos();
  
  // Executar máquina de estados
  executarMaquinaEstados();
  
  delay(100); // pequena pausa para estabilidade
}

void verificarConexaoBluetooth() {
  // Verificar se há dispositivos conectados
  bool conectado = SerialBT.hasClient();
  
  if (conectado != deviceConnected) {
    deviceConnected = conectado;
    
    if (deviceConnected) {
      // Ligar a porta 2 (GPIO2) quando Bluetooth conectar
      digitalWrite(saida3, HIGH);
      Serial.println("Dispositivo conectado! Porta 2 ligada.");
      enviarNotificacao("CONECTADO");
    } else {
      // Desligar a porta 2 (GPIO2) quando Bluetooth desconectar
      digitalWrite(saida3, LOW);
      Serial.println("Dispositivo desconectado! Porta 2 desligada.");
    }
    
    oldDeviceConnected = deviceConnected;
  }
}

void processarComandosRecebidos() {
  // Se há dados disponíveis no Bluetooth
  if (SerialBT.available()) {
    char c = SerialBT.read();
    
    if (c == '\n' || c == '\r') {
      // Comando completo recebido
      if (comandoRecebido.length() > 0) {
        Serial.println("Comando recebido: " + comandoRecebido);
        
        if (comandoRecebido == "START") {
          // Comando para iniciar execução
          if (estadoAtual == IDLE) {
            iniciarModo();
            enviarResposta("OK");
          } else {
            enviarResposta("ERR: Modo já em execução");
          }
        } else {
          // Comando de configuração
          if (processarConfiguracao(comandoRecebido)) {
            salvarConfiguracao();
            enviarResposta("OK");
          } else {
            enviarResposta("ERR: Formato inválido");
          }
        }
        
        comandoRecebido = ""; // Limpar comando
      }
    } else {
      // Adicionar caractere ao comando
      comandoRecebido += c;
    }
  }
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
  if (deviceConnected) {
    SerialBT.println(resposta);
    Serial.println("Resposta enviada: " + resposta);
  }
}

void enviarNotificacao(String notificacao) {
  if (deviceConnected) {
    SerialBT.println(notificacao);
    Serial.println("Notificação enviada: " + notificacao);
  }
}
