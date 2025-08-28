// Inclui a biblioteca para comunicação Bluetooth Serial
#include "BluetoothSerial.h"
#include <Preferences.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
#include <math.h>

/*
 * ========================================
 * SISTEMA DE DEBUG CONFIGURÁVEL
 * ========================================
 * 
 * Para DESATIVAR todos os Serial.println de debug:
 * - Altere a constante DEBUG_ENABLED para false
 * - Não é necessário remover nenhum código
 * 
 * Para ATIVAR novamente:
 * - Altere DEBUG_ENABLED para true
 * 
 * Exemplo:
 * const bool DEBUG_ENABLED = false;  // Debug desativado
 * const bool DEBUG_ENABLED = true;   // Debug ativado
 * 
 * ========================================
 */

// Verifica se o Bluetooth está realmente disponível no microcontrolador
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

// Cria um objeto para controlar o Bluetooth Serial
BluetoothSerial SerialBT;

// Definição dos pinos
const int entrada = 34;     // GPIO34 (entrada analógica do ZMPT101B)
const int rele1 = 25;      // GPIO25 (relé 1)
const int rele2 = 32;      // GPIO32 (relé 2) 
const int ledBluetooh = 2;       // GPIO2 (relé 3) - Controlado automaticamente pelo status do Bluetooth

// Definição dos pinos dos LEDs indicadores
const int ledAzul = 23;     // GPIO23 - LED Azul (Modos 1, 2 e 5)
const int ledAmarelo = 22;  // GPIO22 - LED Verde (Modos 3, 4 e 5)
const int ledVermelho = 19; // GPIO21 - LED Vermelho (Indicador de tensão AC)
const int ledBranco = 18;   // GPIO18 - LED Branco (Indicador de contagem de tempo e operação cíclica)

const char* btName = "RELÉ MULTIFUNCIONAL - TCC ";

// Configurações do sensor ZMPT101B - ADC ESP32
const adc1_channel_t ZMPT101B_CHANNEL = ADC1_CHANNEL_6; // GPIO34
const adc_atten_t ZMPT101B_ATTEN = ADC_ATTEN_DB_11;
const adc_bits_width_t ZMPT101B_WIDTH = ADC_WIDTH_BIT_12;
const int ZMPT101B_SAMPLES = 2000;        // Número de amostras para cálculo RMS
const float ZMPT101B_THRESHOLD_VRMS = 15.0f; // Threshold em Volts RMS para detectar tensão AC
const float ZMPT101B_MODULE_MEASURED_VRMS = 1.663f; // Tensão medida no pino OUT (Vrms) - ajuste conforme sua instalação
const float ZMPT101B_NETWORK_VOLTAGE = 220.0f; // Tensão da rede (V)

// Variável para caracterização do ADC
esp_adc_cal_characteristics_t adc_chars;

// Estados da máquina de estados
enum Estados {
  MODO_1 = 1,     // Retardo na energização
  MODO_2 = 2,     // Retardo na desenergização
  MODO_3 = 3,     // Cíclico com início ligado
  MODO_4 = 4,     // Cíclico com início desligado
  MODO_5 = 5,     // Partida estrela-triângulo
  MODO_6 = 6      // Alteração via comando bluetooh
};

// Estrutura para configuração dos relés
struct ConfigReles {
  int modo;
  unsigned long tempo1;  // em segundos (usando sempre tempo1)
};

// ========================================
// CONSTANTES DO SISTEMA
// ========================================

// Valores default para configuração
const int DEFAULT_MODO = 1;
const unsigned long DEFAULT_TEMPO1 = 300; // 5 minutos em segundos

// Valores de validação para modo
const int MIN_MODO = 1;
const int MAX_MODO = 5;

// Valores de validação para tempo
const unsigned long MAX_TEMPO = 1728000; // 20 dias em segundos
const unsigned long SEGUNDOS_POR_DIA = 86400; // Segundos em um dia

// Tempos de controle
const unsigned long TEMPO_STATUS_AUTOMATICO = 3000; // 3 segundos para envio automático de status
const unsigned long TEMPO_LOG_MODO = 5000; // 5 segundos para log do modo
const unsigned long DELAY_LOOP = 50; // Delay do loop principal em milissegundos
const unsigned long CONVERSOR_SEGUNDOS = 1000; // Conversor de millis() para segundos

// Configurações de comunicação
const unsigned long VELOCIDADE_SERIAL = 115200; // Velocidade do Serial em bauds

// Valores de controle
const int VALOR_INVALIDO = -1; // Valor para indicar erro ou inválido
const int VALOR_NAO_ENCONTRADO = -1; // Valor para indicar que não foi encontrado
const unsigned long VALOR_INICIAL = 0; // Valor inicial para variáveis de tempo

// Tempo de transição estrela-triângulo (em milissegundos)
const unsigned long TEMPO_TRANSICAO_ESTRELA_TRIANGULO = 150;

// Configurações dos LEDs
const unsigned long TEMPO_PISCA_LED = 500; // Tempo de piscada dos LEDs em milissegundos
const unsigned long TEMPO_PISCA_LED_BRANCO_RAPIDO = 200; // Tempo de piscada rápida do LED branco (200ms)
const unsigned long TEMPO_PISCA_LED_BRANCO_LENTO = 500;  // Tempo de piscada lenta do LED branco (500ms)

// Controle de debug - altere para false para desativar todos os Serial.println
const bool DEBUG_ENABLED = true;

// ========================================
// VARIÁVEIS GLOBAIS
// ========================================

bool deviceConnected = false;
bool oldDeviceConnected = false;
String comandoRecebido = "";

Estados estadoAtual = (Estados)DEFAULT_MODO; // Inicializa com modo padrão
ConfigReles config;
unsigned long tempoInicio = 0;
unsigned long tempoAtual = 0;
bool relesLigados = false;
bool modoEstrela = true; // true = estrela, false = triangulo

// Variáveis para controle de alteração manual
bool relesLigadosAnterior = false;
bool modoEstrelaAnterior = true;
unsigned long ultimaAlteracaoManual = 0;

// Variáveis para controle da transição estrela-triângulo (MODO 5)
bool transicaoEstrelaTrianguloEmAndamento = false;
unsigned long tempoInicioTransicao = 0;

// Variáveis para validação da entrada (anti-ruído)
const int VALIDACAO_ENTRADA_COUNT = 3; // Número de leituras consecutivas necessárias
int contadorEntradaAtiva = 0;
int contadorEntradaInativa = 0;
bool entradaValidada = false; // Estado validado da entrada

// Variável para controle de mudança de status da entrada
bool entradaAtivaAnterior = false;

// Variáveis para controle de status automático
unsigned long tempoConexao = 0;
bool statusEnviado = false;

// Variáveis para controle de tempo no modo 1
bool temporizadorModo1Iniciado = false;

// Variáveis para controle dos LEDs
unsigned long ultimoTempoPiscaLed = 0;
bool estadoPiscaLed = false;

// Variáveis para controle do LED branco
unsigned long ultimoTempoPiscaLedBranco = 0;
bool estadoPiscaLedBranco = false;

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
void restaurarEstadoSalvo();
void verificarConexaoBluetooth();
void processarComandosRecebidos();
void verificarAlteracaoManual();
void enviarNotificacaoAlteracaoManual(bool novoEstado, String tipoAlteracao);
void enviarStatusAutomatico();
void verificarStatusEntrada();
void calibrarZMPT101B();
bool ajustarThresholdZMPT101B(String comando);
void debugPrint(String mensagem);
void controlarLEDs();
void configurarLEDs();
void controlarLEDBranco();

void setup() {
  Serial.begin(VELOCIDADE_SERIAL);
  debugPrint("=== INICIANDO RELAY TIMER ===");
  
  // Configuração do ADC para ZMPT101B
  debugPrint("🔧 Configurando ADC para ZMPT101B...");
  adc1_config_width(ZMPT101B_WIDTH);
  adc1_config_channel_atten(ZMPT101B_CHANNEL, ZMPT101B_ATTEN);
  esp_adc_cal_characterize(ADC_UNIT_1, ZMPT101B_ATTEN, ZMPT101B_WIDTH, 1100, &adc_chars);
  delay(100);
  debugPrint("✅ ADC configurado com sucesso");
  
  // Configuração dos pinos
  // GPIO34 é entrada analógica por padrão, não precisa de pinMode
  pinMode(rele1, OUTPUT);
  pinMode(rele2, OUTPUT);
  pinMode(ledBluetooh, OUTPUT);
  
  // Configuração dos LEDs indicadores
  configurarLEDs();
  
  // Inicializar relés desligados
  digitalWrite(rele1, HIGH);
  digitalWrite(rele2, HIGH);
  digitalWrite(ledBluetooh, LOW); // LED Bluetooth inicia desligado (sem conexão)
  
  // Inicializar variável de controle da entrada
  entradaAtivaAnterior = validarEntrada();
  debugPrint("🔍 Status inicial da entrada: " + String(entradaAtivaAnterior ? "ATIVA" : "INATIVA"));
  
  // Carregar configuração salva
  debugPrint("🔄 Iniciando carregamento da configuração");
  carregarConfiguracao();
  
  // Inicializar Bluetooth
  SerialBT.begin(btName); 
  
  // Verificar status inicial do Bluetooth e configurar LED
  deviceConnected = SerialBT.hasClient();
  digitalWrite(ledBluetooh, deviceConnected ? HIGH : LOW);
  debugPrint("🔵 Status inicial Bluetooth: " + String(deviceConnected ? "CONECTADO" : "DESCONECTADO"));
  
  debugPrint("🚀 Relay Timer iniciado - Modo " + String(config.modo) + " - T1: " + String(config.tempo1) + "s");
  debugPrint("📊 Estado atual após inicialização: " + String(estadoAtual));
}

void loop() {
  // Verificar conexão Bluetooth
  verificarConexaoBluetooth();
  
  // Processar comandos recebidos
  processarComandosRecebidos();
  
  // Verificar alterações manuais nos relés
  verificarAlteracaoManual();
  
  // Executar máquina de estados
  executarMaquinaEstados();
  
  // Controlar LEDs indicadores
  controlarLEDs();
  
  // delay(DELAY_LOOP); // pausa para estabilidade
}

void verificarConexaoBluetooth() {
  // Verificar se há dispositivos conectados
  bool conectado = SerialBT.hasClient();
  
  if (conectado != deviceConnected) {
    deviceConnected = conectado;
    
    if (deviceConnected) {
      // Ligar LED Bluetooth quando conectar
      digitalWrite(ledBluetooh, HIGH);
      debugPrint("🔵 Bluetooth conectado - LED Bluetooth ligado");
      
      // Inicializar controle de status automático
      tempoConexao = millis();
      statusEnviado = false;
      
      enviarNotificacao("CONECTADO");
    } else {
      // Desligar LED Bluetooth quando desconectar
      digitalWrite(ledBluetooh, LOW);
      debugPrint("🔴 Bluetooth desconectado - LED Bluetooth desligado");
      
      // Resetar controle de status automático
      statusEnviado = false;
    }
    
    oldDeviceConnected = deviceConnected;
  }
  
  // Verificar se deve enviar status automático
  if (deviceConnected && !statusEnviado && (millis() - tempoConexao) >= TEMPO_STATUS_AUTOMATICO) {
    debugPrint("📊 Enviando status automático após conexão Bluetooth");
    enviarStatusAutomatico();
    statusEnviado = true;
  }
}

void processarComandosRecebidos() {
  // Se há dados disponíveis no Bluetooth
  if (SerialBT.available()) {
    char c = SerialBT.read();
    
    // Adicionar caractere ao comando
    comandoRecebido += c;
    
    // Verificar se o comando termina com _END
    if (comandoRecebido.endsWith("_END")) {
      // Comando completo recebido - remover _END
      String comandoProcessado = comandoRecebido.substring(0, comandoRecebido.length() - 4);
      debugPrint("📥 Comando recebido: " + comandoProcessado);
      
      if (comandoProcessado == "X" || comandoProcessado == "6") {
        // Comando para alterar estado dos relés
        debugPrint("🔧 Comando de controle de relés recebido: " + comandoProcessado);
        if (relesLigados) {
          ligarRele(false); // Desliga os relés
        } else {
          ligarRele(true);  // Liga os relés
        }
        enviarResposta("OK");
      } else if (comandoProcessado == "CALIBRAR") {
        // Comando para calibrar o sensor ZMPT101B
        debugPrint("🔧 Comando de calibração recebido");
        calibrarZMPT101B();
        enviarResposta("CALIBRACAO_INICIADA");
      } else if (ajustarThresholdZMPT101B(comandoProcessado)) {
        // Comando para ajustar threshold do ZMPT101B
        debugPrint("🔧 Comando de ajuste de threshold processado");
        // A resposta já é enviada na função ajustarThresholdZMPT101B
      } else if (processarConfiguracao(comandoProcessado)) {
        // Comando de configuração válido
        debugPrint("✅ Comando de configuração válido - aplicando alterações");
        salvarConfiguracao();
        debugPrint("🔄 Restaurando estado baseado na nova configuração");
        restaurarEstadoSalvo(); // Restaurar estado baseado na nova configuração
        debugPrint("🚀 Iniciando modo com nova configuração");
        iniciarModo(); // Aplicar a nova configuração imediatamente
        enviarResposta("OK");
        debugPrint("✅ Configuração aplicada com sucesso");
      } else {
        // Comando inválido ou bloqueado por segurança
        debugPrint("❌ Configuração não pôde ser aplicada");
      }
      
      comandoRecebido = ""; // Limpar comando
    }
  }
}

bool processarConfiguracao(String comando) {
  debugPrint("🔍 Processando configuração: " + comando);
  
  // Verificar se é comando de configuração (formato: "modo|tempo1|tempo1")
  int pipe1 = comando.indexOf('|');
  int pipe2 = comando.indexOf('|', pipe1 + 1);
  
  if (pipe1 == VALOR_INVALIDO || pipe2 == VALOR_INVALIDO) {
    debugPrint("❌ Formato inválido: " + comando);
    return false;
  }
  
  int modo = comando.substring(0, pipe1).toInt();
  unsigned long t1 = comando.substring(pipe1 + 1, pipe2).toInt();
  unsigned long t1_aux = comando.substring(pipe2 + 1).toInt();
  
  debugPrint("📊 Valores extraídos - Modo: " + String(modo) + ", T1: " + String(t1) + ", T1_aux: " + String(t1_aux));
  
  // Validar modo
  if (modo < MIN_MODO || modo > MAX_MODO) {
    debugPrint("❌ Modo inválido: " + String(modo) + " (deve ser entre " + String(MIN_MODO) + " e " + String(MAX_MODO) + ")");
    return false;
  }
  
  // Validar tempos (máximo 20 dias)
  if (t1 > MAX_TEMPO || t1_aux > MAX_TEMPO) {
    debugPrint("❌ Tempo muito longo: máximo " + String(MAX_TEMPO / SEGUNDOS_POR_DIA) + " dias");
    return false;
  }
  
  // VERIFICAÇÃO DE SEGURANÇA: Não permitir alterar modo se entrada estiver ativa
  bool entradaAtiva = validarEntrada();
  debugPrint("🔍 Status VALIDADO da entrada durante validação: " + String(entradaAtiva ? "ATIVA" : "INATIVA"));
  
  if (entradaAtiva) {
    debugPrint("🚨 Não é possível alterar modo com entrada ativa!");
    
    // Enviar mensagem de erro via Bluetooth
    if (deviceConnected) {
      enviarResposta("ERR: Entrada ativa. Desligue a entrada para alterar o modo.");
      enviarNotificacao("ALERTA: Modo não pode ser alterado com entrada ativa!");
    }
    
    return false;
  }
  
  // Verificar se está tentando alterar para um modo diferente
  if (modo != config.modo) {
    debugPrint("🔄 Alterando modo: " + String(config.modo) + " → " + String(modo));
  }
  
  debugPrint("💾 Configuração atual antes da alteração - Modo: " + String(config.modo) + ", T1: " + String(config.tempo1));
  
  config.modo = modo;
  config.tempo1 = t1;
  
  debugPrint("✅ Configuração válida - Modo: " + String(modo) + ", T1: " + String(t1) + "s");
  debugPrint("💾 Nova configuração definida - Modo: " + String(config.modo) + ", T1: " + String(config.tempo1));
  
  return true;
}

void salvarConfiguracao() {
  debugPrint("💾 Salvando configuração na EEPROM - Modo: " + String(config.modo) + ", T1: " + String(config.tempo1));
  
  preferences.begin("relaytimer", false);
  preferences.putInt("modo", config.modo);
  preferences.putULong("tempo1", config.tempo1);
  preferences.end();
  
  debugPrint("✅ Configuração salva na EEPROM com sucesso");
  
  // Verificar se foi salva corretamente
  preferences.begin("relaytimer", true);
  int modoSalvo = preferences.getInt("modo", VALOR_NAO_ENCONTRADO);
  unsigned long tempoSalvo = preferences.getULong("tempo1", VALOR_INICIAL);
  preferences.end();
  
  debugPrint("🔍 Verificação - Modo salvo: " + String(modoSalvo) + ", Tempo salvo: " + String(tempoSalvo));
}

void carregarConfiguracao() {
  debugPrint("📖 Iniciando carregamento da configuração da EEPROM");
  
  preferences.begin("relaytimer", true);
  config.modo = preferences.getInt("modo", DEFAULT_MODO);
  config.tempo1 = preferences.getULong("tempo1", DEFAULT_TEMPO1);
  preferences.end();
  
  debugPrint("📖 Configuração carregada da EEPROM - Modo: " + String(config.modo) + ", T1: " + String(config.tempo1) + "s");
  
  // Restaurar o estado atual baseado na configuração carregada
  debugPrint("🔄 Chamando restaurarEstadoSalvo() para aplicar configuração carregada");
  restaurarEstadoSalvo();
}

void restaurarEstadoSalvo() {
  debugPrint("🔄 Restaurando estado salvo - Modo configurado: " + String(config.modo));
  
  // Se há uma configuração válida salva, restaurar o estado
  if (config.modo >= MIN_MODO && config.modo <= MAX_MODO) {
    estadoAtual = (Estados)config.modo;
    debugPrint("✅ Estado restaurado para MODO " + String(estadoAtual));
    
    // Configurar estado inicial dos relés baseado no modo restaurado
    switch (estadoAtual) {
      case MODO_1: // Retardo na energização - inicia desligado
        relesLigados = false;
        temporizadorModo1Iniciado = false; // Inicializar temporizador como false
        debugPrint("📋 MODO 1 configurado - relés iniciam DESLIGADOS");
        break;
      case MODO_2: // Retardo na desenergização - inicia ligado
        relesLigados = true;
        modoEstrela = false; // Inicializar como false para MODO 2
        debugPrint("📋 MODO 2 configurado - relés iniciam LIGADOS");
        break;
      case MODO_3: // Cíclico com início ligado
        relesLigados = true;
        debugPrint("📋 MODO 3 configurado - relés iniciam LIGADOS");
        break;
      case MODO_4: // Cíclico com início desligado
        relesLigados = false;
        debugPrint("📋 MODO 4 configurado - relés iniciam DESLIGADOS");
        break;
      case MODO_5: // Partida estrela-triângulo - inicia desligado
        relesLigados = false;
        modoEstrela = true;
        debugPrint("📋 MODO 5 configurado - relés iniciam DESLIGADOS (modo estrela)");
        break;
    }
    
    // Atualizar estados anteriores para detecção de alteração manual
    relesLigadosAnterior = relesLigados;
    modoEstrelaAnterior = modoEstrela;
    
    debugPrint("💾 Estados anteriores atualizados - relesLigados: " + String(relesLigados) + ", modoEstrela: " + String(modoEstrela));
  } else {
    // Se não há configuração, usar modo padrão
    estadoAtual = (Estados)DEFAULT_MODO;
    relesLigados = false;
    modoEstrela = true;
    debugPrint("🔄 Usando MODO " + String(DEFAULT_MODO) + " como padrão - configuração inválida");
  }
}

void iniciarModo() {
  debugPrint("🚀 Iniciando modo - Configuração atual: modo=" + String(config.modo) + ", tempo1=" + String(config.tempo1));
  
  estadoAtual = (Estados)config.modo;
  tempoInicio = millis();
  tempoAtual = 0;
  
  debugPrint("📊 Estado atual definido para: " + String(estadoAtual));
  
  // Configurar estado inicial baseado no modo (consistente com restaurarEstadoSalvo)
  switch (estadoAtual) {
    case MODO_1: // Retardo na energização - inicia desligado
      relesLigados = false;
      modoEstrela = true;
      temporizadorModo1Iniciado = false; // Inicializar temporizador como false
      debugPrint("📋 MODO 1 selecionado - relés iniciam DESLIGADOS");
      break;
    case MODO_2: // Retardo na desenergização - inicia ligado
      relesLigados = true;
      modoEstrela = false; // Inicializar como false para MODO 2
      debugPrint("📋 MODO 2 selecionado - relés iniciam LIGADOS");
      break;
    case MODO_3: // Cíclico com início ligado
      relesLigados = true;
      modoEstrela = true;
      debugPrint("📋 MODO 3 selecionado - relés iniciam LIGADOS");
      break;
    case MODO_4: // Cíclico com início desligado
      relesLigados = false;
      modoEstrela = true;
      debugPrint("📋 MODO 4 selecionado - relés iniciam DESLIGADOS");
      break;
    case MODO_5: // Partida estrela-triângulo - inicia desligado
      relesLigados = false;
      modoEstrela = true;
      debugPrint("📋 MODO 5 selecionado - relés iniciam DESLIGADOS (modo estrela)");
      break;
  }
  
  // Inicializar estados anteriores para detecção de alteração manual
  relesLigadosAnterior = relesLigados;
  modoEstrelaAnterior = modoEstrela;
  
  debugPrint("🚀 Iniciando modo " + String(config.modo) + " - Estado inicial: " + String(relesLigados ? "LIGADO" : "DESLIGADO"));
  
  // Verificar se a entrada está ativa antes de configurar estado inicial
  bool entradaAtiva = validarEntrada();
  debugPrint("🔍 Status VALIDADO da entrada: " + String(entradaAtiva ? "ATIVA" : "INATIVA"));
  
  if (!entradaAtiva) {
    // Entrada desacionada - todos os modos iniciam com relés desligados
    ligarRele(false);
    debugPrint("🔴 Entrada inativa - relés desligados");
    return;
  }
  
  // Aplicar estado inicial baseado no modo (apenas quando entrada ativa)
  ligarRele(relesLigados);
  debugPrint("🟢 Entrada ativa - aplicando estado inicial do modo " + String(config.modo));
}

void executarMaquinaEstados() {
  // Validar entrada com anti-ruído
  bool entradaAtiva = validarEntrada();
  
  // Log apenas quando o status validado da entrada mudar
  if (entradaAtiva != entradaAtivaAnterior) {
    unsigned long timestamp = millis() / CONVERSOR_SEGUNDOS;
    debugPrint("🔄 Mudança de status VALIDADO da entrada: " + String(entradaAtiva ? "ATIVA" : "INATIVA") + " (segundo " + String(timestamp) + ")");
    entradaAtivaAnterior = entradaAtiva;
    
    // Resetar temporizador quando entrada mudar de status
    if (estadoAtual == MODO_1) {
      temporizadorModo1Iniciado = false;
      tempoInicio = millis();
      tempoAtual = 0;
    }
  }
  
  tempoAtual = (millis() - tempoInicio) / CONVERSOR_SEGUNDOS; // converter para segundos
  
  // Log do modo atual sendo executado (a cada 5 segundos para não poluir o log)
  static unsigned long ultimoLogModo = 0;
  if (millis() - ultimoLogModo >= TEMPO_LOG_MODO) {
    debugPrint("🔄 Executando MODO " + String(estadoAtual) + " - Tempo atual: " + String(tempoAtual) + "s");
    ultimoLogModo = millis();
  }
  
  switch (estadoAtual) {
    case MODO_1: // Retardo na energização
      if (!entradaAtiva) {
        // Entrada desacionada - desligar relés imediatamente
        if (relesLigados) {
          unsigned long timestamp = millis() / 1000;
          debugPrint("🔴 MODO 1: Entrada desacionada - desligando relés imediatamente (segundo " + String(timestamp) + ")");
          ligarRele(false);
          // Resetar temporizador para próxima operação
          temporizadorModo1Iniciado = false;
          tempoInicio = millis();
          tempoAtual = 0;
        }
      } else if (entradaAtiva && !relesLigados) {
        // Entrada acionada e relés desligados - controlar temporizador
        if (!temporizadorModo1Iniciado) {
          // Primeira vez que entrada foi acionada - iniciar temporizador
          tempoInicio = millis();
          tempoAtual = 0;
          temporizadorModo1Iniciado = true;
                unsigned long timestamp = millis() / CONVERSOR_SEGUNDOS;
      debugPrint("⏰ MODO 1: Entrada acionada - iniciando temporizador de " + String(config.tempo1) + "s (segundo " + String(timestamp) + ")");
        } else if (tempoAtual >= config.tempo1) {
          // Tempo atingido - ligar relés
          unsigned long timestamp = millis() / CONVERSOR_SEGUNDOS;
          debugPrint("✅ MODO 1 CONCLUÍDO - Relés ligados após " + String(config.tempo1) + "s (segundo " + String(timestamp) + ")");
          ligarRele(true);
          // Resetar temporizador para parar o LED branco de piscar
          temporizadorModo1Iniciado = false;
        }
      }
      break;
      
    case MODO_2: // Retardo na desenergização
      if (!entradaAtiva) {
        // Entrada desacionada - iniciar contagem para desligamento
        if (relesLigados && !modoEstrela) { // Usar modoEstrela como flag de controle
          // Primeira vez que entrada foi desacionada - iniciar temporizador
          modoEstrela = true; // Marcar que temporizador foi iniciado
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("🔴 MODO 2: Entrada desacionada - iniciando contagem para desligamento em " + String(config.tempo1) + "s");
        } else if (relesLigados && modoEstrela && tempoAtual >= config.tempo1) {
          // Tempo de retardo atingido - desligar relés
          debugPrint("✅ MODO 2 CONCLUÍDO - Relés desligados após " + String(config.tempo1) + "s");
          ligarRele(false);
          // Resetar flag para parar o LED branco de piscar
          modoEstrela = false;
          estadoAtual = MODO_2; // Permanecer no MODO_2
        }
      } else if (entradaAtiva) {
        // Entrada acionada - ligar relés imediatamente e resetar temporizador
        if (!relesLigados) {
          debugPrint("🟢 MODO 2: Entrada acionada - ligando relés imediatamente");
          ligarRele(true);
          modoEstrela = false; // Resetar flag de controle
          tempoInicio = millis();
          tempoAtual = 0;
        }
      }
      break;
      
    case MODO_3: // Cíclico com início ligado
      if (!entradaAtiva) {
        // Entrada desacionada - desligar relés imediatamente e interromper ciclo
        if (relesLigados) {
          debugPrint("🔴 MODO 3: Entrada desacionada - desligando relés imediatamente");
          ligarRele(false);
          // Resetar ciclo para próxima ativação
          tempoInicio = millis();
          tempoAtual = 0;
        }
      } else if (entradaAtiva) {
        // Entrada acionada - controlar ciclo
        if (relesLigados && tempoAtual >= config.tempo1) {
          // Tempo T1 atingido - desligar relés e iniciar contagem para ligar
          debugPrint("🔄 MODO 3: Desligando relés após " + String(config.tempo1) + "s");
          ligarRele(false);
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("⏰ MODO 3: Relés desligados, aguardando " + String(config.tempo1) + "s para ligar");
        } else if (!relesLigados && tempoAtual >= config.tempo1) {
          // Tempo T1 atingido - ligar relés e reiniciar ciclo
          debugPrint("🔄 MODO 3: Ligando relés após " + String(config.tempo1) + "s");
          ligarRele(true);
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("⏰ MODO 3: Relés ligados, aguardando " + String(config.tempo1) + "s para desligar");
        }
      }
      break;
      
    case MODO_4: // Cíclico com início desligado
      if (!entradaAtiva) {
        // Entrada desacionada - desligar relés imediatamente e interromper ciclo
        if (relesLigados) {
          debugPrint("🔴 MODO 4: Entrada desacionada - desligando relés imediatamente");
          ligarRele(false);
          // Resetar ciclo para próxima ativação
          tempoInicio = millis();
          tempoAtual = 0;
        }
      } else if (entradaAtiva) {
        // Entrada acionada - controlar ciclo
        if (!relesLigados && tempoAtual >= config.tempo1) {
          // Tempo T1 atingido - ligar relés e iniciar contagem para desligar
          debugPrint("🔄 MODO 4: Ligando relés após " + String(config.tempo1) + "s");
          ligarRele(true);
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("⏰ MODO 4: Relés ligados, aguardando " + String(config.tempo1) + "s para desligar");
        } else if (relesLigados && tempoAtual >= config.tempo1) {
          // Tempo T1 atingido - desligar relés e reiniciar ciclo
          debugPrint("🔄 MODO 4: Desligando relés após " + String(config.tempo1) + "s");
          ligarRele(false);
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("⏰ MODO 4: Relés desligados, aguardando " + String(config.tempo1) + "s para ligar");
        }
      }
      break;
      
    case MODO_5: // Partida estrela-triângulo
      if (!entradaAtiva) {
        // Entrada desacionada - desligar relés imediatamente e resetar transição
        if (relesLigados) {
          debugPrint("🔴 MODO 5: Entrada desacionada - desligando relés imediatamente");
          ligarRele(false);
          modoEstrela = true; // Reset para modo estrela
          transicaoEstrelaTrianguloEmAndamento = false; // Cancelar transição em andamento
          tempoInicio = millis();
          tempoAtual = 0;
        }
      } else if (entradaAtiva && !relesLigados && !transicaoEstrelaTrianguloEmAndamento) {
        // Entrada acionada e relés desligados - iniciar modo estrela
        debugPrint("🟢 MODO 5: Entrada acionada - iniciando modo estrela");
        ligarReleEstrela();
        tempoInicio = millis();
        tempoAtual = 0;
      } else if (entradaAtiva && modoEstrela && !transicaoEstrelaTrianguloEmAndamento && tempoAtual >= config.tempo1) {
        // Iniciar transição para triângulo após tempo configurado
        debugPrint("⭐→🔺 MODO 5: Iniciando transição para triângulo após " + String(config.tempo1) + "s");
        debugPrint("⏱️  Desligando Relé 1 para transição");
        
        // Iniciar transição - desligar Relé 1
        digitalWrite(rele1, HIGH);
        transicaoEstrelaTrianguloEmAndamento = true;
        tempoInicioTransicao = millis();
        debugPrint("⏰ Transição iniciada - aguardando " + String(TEMPO_TRANSICAO_ESTRELA_TRIANGULO) + "ms");
      } else if (entradaAtiva && transicaoEstrelaTrianguloEmAndamento) {
        // Verificar se tempo de transição foi atingido
        unsigned long tempoTransicao = millis() - tempoInicioTransicao;
        if (tempoTransicao >= TEMPO_TRANSICAO_ESTRELA_TRIANGULO) {
          // Tempo de transição atingido - completar transição
          debugPrint("✅ MODO 5: Transição concluída - ligando modo triângulo");
          
          // Ligar Relé 2 (modo triângulo)
          ligarReleTriangulo();
          modoEstrela = false;
          transicaoEstrelaTrianguloEmAndamento = false;
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("🔺 Modo triângulo ativado com sucesso");
        }
      }
      break;
  }
}

void verificarAlteracaoManual() {
  // Verificar se houve alteração manual nos relés
  if (relesLigados != relesLigadosAnterior) {
    unsigned long timestamp = millis() / CONVERSOR_SEGUNDOS;
    debugPrint("🔧 Alteração manual detectada (segundo " + String(timestamp) + "):");
    debugPrint("   Estado anterior: " + String(relesLigadosAnterior ? "LIGADO" : "DESLIGADO"));
    debugPrint("   Estado atual: " + String(relesLigados ? "LIGADO" : "DESLIGADO"));
    
    if (deviceConnected) {
      enviarNotificacaoAlteracaoManual(relesLigados, "RELE");
    }
    
    relesLigadosAnterior = relesLigados;
    ultimaAlteracaoManual = millis();
  }
  
  // Verificar alteração no modo estrela-triângulo (apenas para modo 5)
  if (estadoAtual == MODO_5 && modoEstrela != modoEstrelaAnterior) {
    debugPrint("🔧 Alteração manual detectada (Modo 5):");
    debugPrint("   Modo anterior: " + String(modoEstrelaAnterior ? "ESTRELA" : "TRIÂNGULO"));
    debugPrint("   Modo atual: " + String(modoEstrela ? "ESTRELA" : "TRIÂNGULO"));
    
    if (deviceConnected) {
      enviarNotificacaoAlteracaoManual(modoEstrela, "ESTRELA_TRIANGULO");
    }
    
    modoEstrelaAnterior = modoEstrela;
    ultimaAlteracaoManual = millis();
  }
}

void enviarNotificacaoAlteracaoManual(bool novoEstado, String tipoAlteracao) {
  unsigned long timestamp = millis() / CONVERSOR_SEGUNDOS; // timestamp em segundos
  String notificacao = "MANUAL|" + String(timestamp) + "|" + tipoAlteracao + "|" + String(novoEstado ? "ON" : "OFF");
  
  debugPrint("📤 Notificação de alteração manual: " + notificacao);
  debugPrint("📤 Detalhes - Tipo: " + tipoAlteracao + ", Estado: " + String(novoEstado ? "ON" : "OFF") + ", Timestamp: " + String(timestamp));
  
  enviarNotificacao(notificacao);
}

void ligarRele(bool ligar) {
  debugPrint("🔌 Função ligarRele chamada - Parâmetro: " + String(ligar ? "LIGAR" : "DESLIGAR"));
  debugPrint("🔌 Estado anterior dos relés: " + String(relesLigados ? "LIGADO" : "DESLIGADO"));
  
  relesLigados = ligar;
  
  if (ligar) {
    digitalWrite(rele1, LOW);
    digitalWrite(rele2, LOW);
    // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
    unsigned long timestamp = millis() / CONVERSOR_SEGUNDOS;
    debugPrint("🔌 Relés ligados (GPIO25 e GPIO32) - segundo " + String(timestamp));
    enviarNotificacao("ON");
  } else {
    digitalWrite(rele1, HIGH);
    digitalWrite(rele2, HIGH);
    // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
    unsigned long timestamp = millis() / CONVERSOR_SEGUNDOS;
    debugPrint("🔌 Relés desligados (GPIO25 e GPIO32) - segundo " + String(timestamp));
    enviarNotificacao("OFF");
  }
  
  debugPrint("🔌 Estado atual dos relés: " + String(relesLigados ? "LIGADO" : "DESLIGADO"));
}

void ligarReleEstrela() {
  // Modo estrela: apenas relé 1 ligado
  digitalWrite(rele2, HIGH);
  digitalWrite(rele1, LOW);
  // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
  relesLigados = true;
  debugPrint("⭐ Modo estrela ativado - Relé 1 ligado, Relé 2 desligado");
  enviarNotificacao("ON");
}

void ligarReleTriangulo() {
  // Modo triângulo: apenas relé 2 ligado
  digitalWrite(rele2, LOW);
  digitalWrite(rele1, HIGH);
  // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
  relesLigados = true;
  debugPrint("🔺 Modo triângulo ativado - Relé 1 desligado, Relé 2 ligado");
  enviarNotificacao("ON");
}

void enviarResposta(String resposta) {
  if (deviceConnected) {
    SerialBT.println(resposta);
    debugPrint("📤 Resposta enviada via Bluetooth: " + resposta);
  } else {
    debugPrint("📤 Resposta não enviada - Bluetooth desconectado: " + resposta);
  }
}

void enviarNotificacao(String notificacao) {
  if (deviceConnected) {
    SerialBT.println(notificacao);
    debugPrint("📤 Notificação enviada via Bluetooth: " + notificacao);
  } else {
    debugPrint("📤 Notificação não enviada - Bluetooth desconectado: " + notificacao);
  }
}

void enviarStatusAutomatico() {
  debugPrint("📊 Preparando status automático - Modo atual: " + String(estadoAtual) + ", Config modo: " + String(config.modo));
  
  // Verificar e informar status da entrada
  verificarStatusEntrada();
  
  // Determinar estado atual dos relés
  String estadoReles = "DESLIGADO";
  if (relesLigados) {
    if (estadoAtual == MODO_5 && !modoEstrela) {
      estadoReles = "TRIANGULO";
    } else if (estadoAtual == MODO_5 && modoEstrela) {
      estadoReles = "ESTRELA";
    } else {
      estadoReles = "LIGADO";
    }
  }
  
  // Determinar nome do modo - sempre mostrar o modo atual
  String nomeModo = "DESCONHECIDO";
  switch (estadoAtual) {
    case MODO_1: nomeModo = "RETARDO_ENERGIZACAO"; break;
    case MODO_2: nomeModo = "RETARDO_DESENERGIZACAO"; break;
    case MODO_3: nomeModo = "CICLICO_INICIO_LIGADO"; break;
    case MODO_4: nomeModo = "CICLICO_INICIO_DESLIGADO"; break;
    case MODO_5: nomeModo = "ESTRELA_TRIANGULO"; break;
    case MODO_6: nomeModo = "CONTROLE_MANUAL"; break;
    default: nomeModo = "MODO_" + String(estadoAtual); break;
  }
  
  // Criar string de status
  String status = "STATUS|" + nomeModo + "|" + String(config.tempo1) + "|" + String(config.tempo1) + "|" + estadoReles;
  
  debugPrint("📊 Enviando status automático: " + status);
  debugPrint("📊 Detalhes - Estado atual: " + String(estadoAtual) + ", Config modo: " + String(config.modo) + ", Relés: " + estadoReles);
  
  enviarNotificacao(status);
}

void verificarStatusEntrada() {
  bool entradaAtiva = validarEntrada();
  
  debugPrint("🔍 Verificando status VALIDADO da entrada: " + String(entradaAtiva ? "ATIVA" : "INATIVA"));
  
  if (deviceConnected) {
    if (entradaValidada) {
      enviarNotificacao("INFO: Entrada ATIVA - Modo não pode ser alterado");
    } else {
      enviarNotificacao("INFO: Entrada INATIVA - Modo pode ser alterado");
    }
  }
  
  debugPrint("📊 Status da entrada: " + String(entradaAtiva ? "ATIVA" : "INATIVA") + " | Validada: " + String(entradaValidada ? "SIM" : "NÃO"));
}

// Função para validar entrada com anti-ruído usando ZMPT101B com cálculo RMS
bool validarEntrada() {
  // Calcular tensão RMS usando método correto para tensão AC
  long sum_mV = 0;
  
  // Primeira passada: calcular média
  for (int i = 0; i < ZMPT101B_SAMPLES; i++) {
    int raw = adc1_get_raw(ZMPT101B_CHANNEL);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    sum_mV += (long)mv;
  }
  
  double mean_mV = (double)sum_mV / (double)ZMPT101B_SAMPLES;
  double meanV = mean_mV / 1000.0;

  // Segunda passada: calcular RMS
  double sq = 0.0;
  int lastRaw = 0;
  for (int i = 0; i < ZMPT101B_SAMPLES; i++) {
    int raw = adc1_get_raw(ZMPT101B_CHANNEL);
    lastRaw = raw;
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    double v = (double)mv / 1000.0;
    double d = v - meanV;
    sq += d * d;
  }

  double Vrms_raw = sqrt(sq / (double)ZMPT101B_SAMPLES); 
  double calib = ZMPT101B_NETWORK_VOLTAGE / ZMPT101B_MODULE_MEASURED_VRMS; 
  double Vrms_network = Vrms_raw * calib;
  
  // Determinar se há tensão AC baseado no threshold RMS
  bool leituraAtual = Vrms_network > ZMPT101B_THRESHOLD_VRMS;
  
  // Log da leitura RMS (a cada 10 verificações para não poluir o log)
  static int contadorLog = 0;
  contadorLog++;
  if (contadorLog >= 10) {
    debugPrint("📊 ZMPT101B - Vrms_rede: " + String(Vrms_network, 2) + "V | Vrms_modulo: " + String(Vrms_raw, 4) + "V | Threshold: " + String(ZMPT101B_THRESHOLD_VRMS) + "V | Status: " + String(leituraAtual ? "ATIVA" : "INATIVA"));
    contadorLog = 0;
  }
  
  if (leituraAtual) {
    // Entrada lida como ativa
    contadorEntradaAtiva++;
    contadorEntradaInativa = 0; // Reset contador inativo
    
    if (contadorEntradaAtiva >= VALIDACAO_ENTRADA_COUNT && !entradaValidada) {
      // Entrada validada como ativa
      entradaValidada = true;
      debugPrint("✅ Entrada VALIDADA como ATIVA após " + String(VALIDACAO_ENTRADA_COUNT) + " leituras consecutivas (Vrms: " + String(Vrms_network, 2) + "V)");
      return true;
    }
  } else {
    // Entrada lida como inativa
    contadorEntradaInativa++;
    contadorEntradaAtiva = 0; // Reset contador ativo
    
    if (contadorEntradaInativa >= VALIDACAO_ENTRADA_COUNT && entradaValidada) {
      // Entrada validada como inativa
      entradaValidada = false;
      debugPrint("✅ Entrada VALIDADA como INATIVA após " + String(VALIDACAO_ENTRADA_COUNT) + " leituras consecutivas (Vrms: " + String(Vrms_network, 2) + "V)");
      return false;
    }
  }
  
  // Retorna o estado validado anterior (não mudou ainda)
  return entradaValidada;
}

// Função para calibrar o sensor ZMPT101B
void calibrarZMPT101B() {
  debugPrint("🔧 Iniciando calibração do ZMPT101B...");
  debugPrint("⚠️  Certifique-se de que NÃO há tensão AC na entrada!");
  
  // Usar o mesmo método da função validarEntrada para consistência
  long sum_mV = 0;
  const int amostrasCalibracao = 2000;
  
  for (int i = 0; i < amostrasCalibracao; i++) {
    int raw = adc1_get_raw(ZMPT101B_CHANNEL);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    sum_mV += (long)mv;
    
    if (i % 500 == 0) {
      debugPrint("📊 Calibração: " + String(i) + "/" + String(amostrasCalibracao));
    }
  }
  
  double mean_mV = (double)sum_mV / (double)amostrasCalibracao;
  double meanV = mean_mV / 1000.0;
  
  // Calcular RMS para calibração
  double sq = 0.0;
  for (int i = 0; i < amostrasCalibracao; i++) {
    int raw = adc1_get_raw(ZMPT101B_CHANNEL);
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    double v = (double)mv / 1000.0;
    double d = v - meanV;
    sq += d * d;
  }
  
  double Vrms_raw = sqrt(sq / (double)amostrasCalibracao);
  double Vrms_network = Vrms_raw * (ZMPT101B_NETWORK_VOLTAGE / ZMPT101B_MODULE_MEASURED_VRMS);
  
  debugPrint("📊 Calibração concluída:");
  debugPrint("   Média sem tensão: " + String(meanV, 4) + "V");
  debugPrint("   Vrms módulo: " + String(Vrms_raw, 4) + "V");
  debugPrint("   Vrms rede estimada: " + String(Vrms_network, 2) + "V");
  debugPrint("   Threshold atual: " + String(ZMPT101B_THRESHOLD_VRMS) + "V");
  
  // Sugerir novo threshold baseado na calibração
  float novoThreshold = Vrms_network + 5.0; // Threshold = tensão calibrada + margem de segurança
  
  debugPrint("   Threshold sugerido: " + String(novoThreshold, 1) + "V");
  
  // Enviar resultado da calibração via Bluetooth se conectado
  if (deviceConnected) {
    String resultadoCalibracao = "CALIBRACAO|" + String(Vrms_network, 2) + "|" + String(novoThreshold, 1);
    enviarNotificacao(resultadoCalibracao);
  }
}

// Função para ajustar threshold do ZMPT101B via comando
bool ajustarThresholdZMPT101B(String comando) {
  // Formato: "THRESHOLD|valor" (valor em Volts RMS)
  if (comando.startsWith("THRESHOLD|")) {
    float novoThreshold = comando.substring(10).toFloat();
    
    if (novoThreshold > 0.0 && novoThreshold < 300.0) { // Threshold entre 0V e 300V RMS
      // Aqui você pode implementar a lógica para salvar o novo threshold
      // Por enquanto, apenas loga a alteração
      debugPrint("🔧 Threshold ZMPT101B alterado para: " + String(novoThreshold, 1) + "V RMS");
      
      if (deviceConnected) {
        enviarResposta("THRESHOLD_ALTERADO|" + String(novoThreshold, 1) + "V");
      }
      return true;
    } else {
      debugPrint("❌ Threshold inválido: " + String(novoThreshold, 1) + "V");
      if (deviceConnected) {
        enviarResposta("ERR: Threshold deve estar entre 0.1V e 300V RMS");
      }
      return false;
    }
  }
  return false;
}

void debugPrint(String mensagem) {
  if (DEBUG_ENABLED) {
    Serial.println(mensagem);
  }
}

// ========================================
// FUNÇÕES DE CONTROLE DOS LEDs
// ========================================

// Função para configurar os pinos dos LEDs
void configurarLEDs() {
  pinMode(ledAzul, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(ledVermelho, OUTPUT);
  pinMode(ledBranco, OUTPUT);
  
  // Inicializar todos os LEDs desligados
  digitalWrite(ledAzul, LOW);
  digitalWrite(ledAmarelo, LOW);
  digitalWrite(ledVermelho, LOW);
  digitalWrite(ledBranco, LOW);
  
  debugPrint("🔧 LEDs configurados: Azul(GPIO23), Amarelo(GPIO22), Vermelho(GPIO21), Branco(GPIO18)");
}

// Função principal para controlar os LEDs baseado no modo atual
void controlarLEDs() {
  // Controlar LED Vermelho (indicador de tensão AC)
  bool tensaoAC = validarEntrada();
  digitalWrite(ledVermelho, tensaoAC ? HIGH : LOW);
  
  // Controlar LEDs Azul e Amarelo baseado no modo atual
  switch (estadoAtual) {
    case MODO_1: // Retardo na energização
      digitalWrite(ledAzul, HIGH);      // LED Azul sempre aceso
      digitalWrite(ledAmarelo, LOW);    // LED Amarelo sempre desligado
      break;
      
    case MODO_2: // Retardo na desenergização
      // LED Azul piscando
      if (millis() - ultimoTempoPiscaLed >= TEMPO_PISCA_LED) {
        estadoPiscaLed = !estadoPiscaLed;
        digitalWrite(ledAzul, estadoPiscaLed ? HIGH : LOW);
        ultimoTempoPiscaLed = millis();
      }
      digitalWrite(ledAmarelo, LOW);    // LED Amarelo sempre desligado
      break;
      
    case MODO_3: // Cíclico com início ligado
      digitalWrite(ledAzul, LOW);       // LED Azul sempre desligado
      digitalWrite(ledAmarelo, HIGH);   // LED Amarelo sempre aceso
      break;
      
    case MODO_4: // Cíclico com início desligado
      digitalWrite(ledAzul, LOW);       // LED Azul sempre desligado
      // LED Amarelo piscando
      if (millis() - ultimoTempoPiscaLed >= TEMPO_PISCA_LED) {
        estadoPiscaLed = !estadoPiscaLed;
        digitalWrite(ledAmarelo, estadoPiscaLed ? HIGH : LOW);
        ultimoTempoPiscaLed = millis();
      }
      break;
      
    case MODO_5: // Partida estrela-triângulo
      digitalWrite(ledAzul, HIGH);      // LED Azul sempre aceso
      digitalWrite(ledAmarelo, HIGH);   // LED Amarelo sempre aceso
      break;
      
    default:
      // Modo inválido - todos os LEDs desligados
      digitalWrite(ledAzul, LOW);
      digitalWrite(ledAmarelo, LOW);
      digitalWrite(ledVermelho, LOW);
      digitalWrite(ledBranco, LOW);
      break;
  }
  
  // Controlar LED Branco baseado no modo atual
  controlarLEDBranco();
}

// Função para controlar especificamente o LED branco
void controlarLEDBranco() {
  switch (estadoAtual) {
    case MODO_1: // Retardo na energização
      // LED Branco pisca apenas durante contagem de tempo para energizar (200ms)
      if (temporizadorModo1Iniciado) {
        if (millis() - ultimoTempoPiscaLedBranco >= TEMPO_PISCA_LED_BRANCO_RAPIDO) {
          estadoPiscaLedBranco = !estadoPiscaLedBranco;
          digitalWrite(ledBranco, estadoPiscaLedBranco ? HIGH : LOW);
          ultimoTempoPiscaLedBranco = millis();
        }
      } else {
        digitalWrite(ledBranco, LOW); // Desligado quando não está contando tempo
      }
      break;
      
    case MODO_2: // Retardo na desenergização
      // LED Branco pisca apenas durante contagem de tempo para desenergizar (200ms)
      if (relesLigados && modoEstrela && tempoAtual < config.tempo1) {
        // Piscar apenas quando estiver contando tempo para desligar (modoEstrela = true indica temporizador ativo)
        if (millis() - ultimoTempoPiscaLedBranco >= TEMPO_PISCA_LED_BRANCO_RAPIDO) {
          estadoPiscaLedBranco = !estadoPiscaLedBranco;
          digitalWrite(ledBranco, estadoPiscaLedBranco ? HIGH : LOW);
          ultimoTempoPiscaLedBranco = millis();
        }
      } else {
        digitalWrite(ledBranco, LOW); // Desligado quando não está contando tempo
      }
      break;
      
    case MODO_3: // Cíclico com início ligado
      // LED Branco pisca continuamente durante operação cíclica (500ms), apenas quando sensor estiver ativo
      if (validarEntrada()) { // Apenas quando entrada estiver ativa
        if (millis() - ultimoTempoPiscaLedBranco >= TEMPO_PISCA_LED_BRANCO_LENTO) {
          estadoPiscaLedBranco = !estadoPiscaLedBranco;
          digitalWrite(ledBranco, estadoPiscaLedBranco ? HIGH : LOW);
          ultimoTempoPiscaLedBranco = millis();
        }
      } else {
        digitalWrite(ledBranco, LOW); // Desligado quando sensor inativo
      }
      break;
      
    case MODO_4: // Cíclico com início desligado
      // LED Branco pisca continuamente durante operação cíclica (500ms), apenas quando sensor estiver ativo
      if (validarEntrada()) { // Apenas quando entrada estiver ativa
        if (millis() - ultimoTempoPiscaLedBranco >= TEMPO_PISCA_LED_BRANCO_LENTO) {
          estadoPiscaLedBranco = !estadoPiscaLedBranco;
          digitalWrite(ledBranco, estadoPiscaLedBranco ? HIGH : LOW);
          ultimoTempoPiscaLedBranco = millis();
        }
      } else {
        digitalWrite(ledBranco, LOW); // Desligado quando sensor inativo
      }
      break;
      
    case MODO_5: // Partida estrela-triângulo
      // LED Branco pisca apenas durante contagem de tempo estrela (200ms), no modo triângulo não deve piscar
      if (modoEstrela && tempoAtual < config.tempo1) {
        // Piscar apenas quando estiver no modo estrela e contando tempo para transição
        if (millis() - ultimoTempoPiscaLedBranco >= TEMPO_PISCA_LED_BRANCO_RAPIDO) {
          estadoPiscaLedBranco = !estadoPiscaLedBranco;
          digitalWrite(ledBranco, estadoPiscaLedBranco ? HIGH : LOW);
          ultimoTempoPiscaLedBranco = millis();
        }
      } else {
        digitalWrite(ledBranco, LOW); // Desligado quando não está contando tempo estrela
      }
      break;
      
    default:
      digitalWrite(ledBranco, LOW); // Desligado para modo inválido
      break;
  }
}
