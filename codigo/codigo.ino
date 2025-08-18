// Inclui a biblioteca para comunicação Bluetooth Serial
#include "BluetoothSerial.h"
#include <Preferences.h>

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
const int entrada = 34;     // GPIO34 (entrada)
const int saida1 = 25;      // GPIO25 (relé 1)
const int saida2 = 32;      // GPIO32 (relé 2) 
const int saida3 = 2;       // GPIO2 (relé 3) - Controlado automaticamente pelo status do Bluetooth
const char* btName = "RELÉ MULTIFUNCIONAL - TCC ";

// Estados da máquina de estados
enum Estados {
  MODO_1,         // Retardo na energização
  MODO_2,         // Retardo na desenergização
  MODO_3,         // Cíclico com início ligado
  MODO_4,         // Cíclico com início desligado
  MODO_5,         // Partida estrela-triângulo
  MODO_6          // Alteração via comando bluetooh
};

// Estrutura para configuração dos relés
struct ConfigReles {
  int modo;
  unsigned long tempo1;  // em segundos
  unsigned long tempo1;  // em segundos (usando sempre tempo1)
};

// Variáveis globais
bool deviceConnected = false;
bool oldDeviceConnected = false;
String comandoRecebido = "";

Estados estadoAtual = MODO_1; // Inicializa com MODO_1
ConfigReles config;
unsigned long tempoInicio = 0;
unsigned long tempoAtual = 0;
bool relesLigados = false;
bool modoEstrela = true; // true = estrela, false = triangulo

// Tempo de transição estrela-triângulo (em milissegundos)
const unsigned long TEMPO_TRANSICAO_ESTRELA_TRIANGULO = 150;

// Variáveis para controle de alteração manual
bool relesLigadosAnterior = false;
bool modoEstrelaAnterior = true;
unsigned long ultimaAlteracaoManual = 0;

// Variáveis para controle de status automático
unsigned long tempoConexao = 0;
bool statusEnviado = false;

// Controle de debug - altere para false para desativar todos os Serial.println
const bool DEBUG_ENABLED = true;

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
void debugPrint(String mensagem);

void setup() {
  Serial.begin(115200);
  debugPrint("=== INICIANDO RELAY TIMER ===");
  debugPrint("Iniciando RelayTimer...");
  
  // Configuração dos pinos
  pinMode(entrada, INPUT);
  pinMode(saida1, OUTPUT);
  pinMode(saida2, OUTPUT);
  pinMode(saida3, OUTPUT);
  
  // Inicializar relés desligados
  digitalWrite(saida1, LOW);
  digitalWrite(saida2, LOW);
  digitalWrite(saida3, LOW); // Porta 2 (GPIO2) inicia desligada
  
  debugPrint("Pinos configurados:");
  debugPrint("- Entrada: GPIO" + String(entrada));
  debugPrint("- Saída 1 (Relé 1): GPIO" + String(saida1));
  debugPrint("- Saída 2 (Relé 2): GPIO" + String(saida2));
  debugPrint("- Saída 3 (Relé 3): GPIO" + String(saida3));
  
  // Carregar configuração salva
  carregarConfiguracao();
  
  // Inicializar Bluetooth
  SerialBT.begin(btName); 
  
  debugPrint("=== RELAY TIMER INICIADO ===");
  debugPrint("Nome do dispositivo Bluetooth: " + String(btName));
  debugPrint("Aguardando conexão Bluetooth...");
  debugPrint("================================");
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
      debugPrint("🔵 BLUETOOTH CONECTADO! Porta 2 (GPIO2) ligada.");
      debugPrint("📱 Dispositivo conectado via Bluetooth Serial");
      
      // Inicializar controle de status automático
      tempoConexao = millis();
      statusEnviado = false;
      
      enviarNotificacao("CONECTADO");
    } else {
      // Desligar a porta 2 (GPIO2) quando Bluetooth desconectar
      digitalWrite(saida3, LOW);
      debugPrint("🔴 BLUETOOTH DESCONECTADO! Porta 2 (GPIO2) desligada.");
      debugPrint("❌ Dispositivo desconectado do Bluetooth");
      
      // Resetar controle de status automático
      statusEnviado = false;
    }
    
    oldDeviceConnected = deviceConnected;
  }
  
  // Verificar se deve enviar status automático (após 3 segundos)
  if (deviceConnected && !statusEnviado && (millis() - tempoConexao) >= 3000) {
    enviarStatusAutomatico();
    statusEnviado = true;
  }
}

void processarComandosRecebidos() {
  // Se há dados disponíveis no Bluetooth
  if (SerialBT.available()) {
    char c = SerialBT.read();
    debugPrint("📱 CHAR RECEBIDO: '" + String(c) + "' (ASCII: " + String((int)c) + ")");
    
    // Adicionar caractere ao comando
    comandoRecebido += c;
    debugPrint("📝 COMANDO ACUMULADO: '" + comandoRecebido + "'");
    
    // Verificar se o comando termina com _END
    if (comandoRecebido.endsWith("_END")) {
      // Comando completo recebido - remover _END
      String comandoProcessado = comandoRecebido.substring(0, comandoRecebido.length() - 4);
      debugPrint("📥 COMANDO RECEBIDO: '" + comandoProcessado + "'");
      
      if (comandoProcessado == "X" && comandoProcessado == "6") {
        // Comando para alterar estado dos relés
        debugPrint("🔄 Comando CHANGE_RELE_STATUS recebido");
        if (relesLigados) {
          ligarRele(false); // Desliga os relés
          debugPrint("✅ Relés desligados");
        } else {
          ligarRele(true);  // Liga os relés
          debugPrint("✅ Relés ligados");
        }
        enviarResposta("OK");
      } else if (processarConfiguracao(comandoProcessado)) {
        // Comando de configuração válido
        debugPrint("⚙️  Processando comando de configuração...");
        salvarConfiguracao();
        enviarResposta("OK");
        debugPrint("✅ Configuração aplicada e salva");
      } else {
        // Comando inválido ou bloqueado por segurança
        // A mensagem de erro já foi enviada na função processarConfiguracao
        debugPrint("❌ ERRO: Configuração não pôde ser aplicada");
      }
      
      comandoRecebido = ""; // Limpar comando
    }
  }
}

bool processarConfiguracao(String comando) {
  // Verificar se é comando de configuração (formato: "modo|tempo1|tempo1")
  int pipe1 = comando.indexOf('|');
  int pipe2 = comando.indexOf('|', pipe1 + 1);
  
  if (pipe1 == -1 || pipe2 == -1) {
    debugPrint("❌ FORMATO INVÁLIDO: " + comando);
    debugPrint("   Formato esperado: 'modo|tempo1|tempo1'");
    return false;
  }
  
  int modo = comando.substring(0, pipe1).toInt();
  unsigned long t1 = comando.substring(pipe1 + 1, pipe2).toInt();
  unsigned long t1 = comando.substring(pipe2 + 1).toInt();
  
  debugPrint("🔍 ANÁLISE DO COMANDO:");
  debugPrint("   Modo: " + String(modo));
  debugPrint("   Tempo 1: " + String(t1) + "s");
  debugPrint("   Tempo 1: " + String(t1) + "s");
  
  // Validar modo
  if (modo < 1 || modo > 5) {
    debugPrint("❌ MODO INVÁLIDO: " + String(modo) + " (deve ser 1-5)");
    return false;
  }
  
  // Validar tempos (máximo 20 dias = 1.728.000 segundos)
  if (t1 > 1728000 || t1 > 1728000) {
    debugPrint("❌ TEMPO MUITO LONGO: máximo 20 dias (1.728.000s)");
    return false;
  }
  
  // VERIFICAÇÃO DE SEGURANÇA: Não permitir alterar modo se entrada estiver ativa
  bool entradaAtiva = digitalRead(entrada) == HIGH;
  if (entradaAtiva) {
    debugPrint("🚨 SEGURANÇA: Tentativa de alterar modo com entrada ativa!");
    debugPrint("   Para alterar o modo, a entrada deve estar DESLIGADA");
    
    // Enviar mensagem de erro via Bluetooth
    if (deviceConnected) {
      enviarResposta("ERR: Entrada ativa. Desligue a entrada para alterar o modo.");
      enviarNotificacao("ALERTA: Modo não pode ser alterado com entrada ativa!");
    }
    
    return false;
  }
  
  // Verificar se está tentando alterar para um modo diferente
  if (modo != config.modo) {
    debugPrint("🔄 ALTERANDO MODO: " + String(config.modo) + " → " + String(modo));
  }
  
  config.modo = modo;
  config.tempo1 = t1;
  config.tempo1 = t1;
  
  debugPrint("✅ CONFIGURAÇÃO VÁLIDA:");
  debugPrint("   Modo: " + String(modo));
  debugPrint("   T1: " + String(t1) + "s");
  debugPrint("   T1: " + String(t1) + "s");
  
  return true;
}

void salvarConfiguracao() {
  preferences.begin("relaytimer", false);
  preferences.putInt("modo", config.modo);
  preferences.putULong("tempo1", config.tempo1);
  preferences.putULong("tempo1", config.tempo1);
  preferences.end();
  debugPrint("💾 Configuração salva na EEPROM");
}

void carregarConfiguracao() {
  preferences.begin("relaytimer", true);
  config.modo = preferences.getInt("modo", 1);
  config.tempo1 = preferences.getULong("tempo1", 300);
  config.tempo1 = preferences.getULong("tempo1", 0);
  preferences.end();
  
  debugPrint("📖 CONFIGURAÇÃO CARREGADA DA EEPROM:");
  debugPrint("   Modo: " + String(config.modo));
  debugPrint("   T1: " + String(config.tempo1) + "s");
  debugPrint("   T1: " + String(config.tempo1) + "s");
  
  // Restaurar o estado atual baseado na configuração carregada
  restaurarEstadoSalvo();
}

void restaurarEstadoSalvo() {
  // Se há uma configuração válida salva, restaurar o estado
  if (config.modo >= 1 && config.modo <= 5) {
    estadoAtual = (Estados)config.modo;
    debugPrint("🔄 ESTADO RESTAURADO: Modo " + String(config.modo));
    
    // Configurar estado inicial dos relés baseado no modo restaurado
    switch (estadoAtual) {
      case MODO_1: // Retardo na energização - inicia desligado
        relesLigados = false;
        debugPrint("⏰ Estado restaurado: Modo 1 - Relés desligados");
        break;
      case MODO_2: // Retardo na desenergização - inicia ligado
        relesLigados = true;
        modoEstrela = false; // Inicializar como false para MODO 2
        debugPrint("⏰ Estado restaurado: Modo 2 - Relés ligados");
        break;
      case MODO_3: // Cíclico com início ligado
        relesLigados = true;
        debugPrint("🔄 Estado restaurado: Modo 3 - Relés ligados");
        break;
      case MODO_4: // Cíclico com início desligado
        relesLigados = false;
        debugPrint("🔄 Estado restaurado: Modo 4 - Relés desligados");
        break;
      case MODO_5: // Partida estrela-triângulo - inicia desligado
        relesLigados = false;
        modoEstrela = true;
        debugPrint("⭐ Estado restaurado: Modo 5 - Relés desligados");
        break;
    }
    
    // Atualizar estados anteriores para detecção de alteração manual
    relesLigadosAnterior = relesLigados;
    modoEstrelaAnterior = modoEstrela;
  } else {
    debugPrint("⚠️  Nenhuma configuração válida encontrada para restaurar");
    // Se não há configuração, usar MODO_1 como padrão
    estadoAtual = MODO_1;
    relesLigados = false;
    modoEstrela = true;
    debugPrint("🔄 Usando MODO_1 como padrão");
  }
}

void iniciarModo() {
  estadoAtual = (Estados)config.modo;
  tempoInicio = millis();
  tempoAtual = 0;
  relesLigados = false;
  modoEstrela = true;
  
  // Inicializar estados anteriores para detecção de alteração manual
  relesLigadosAnterior = false;
  modoEstrelaAnterior = true;
  
  debugPrint("🚀 INICIANDO MODO " + String(config.modo));
  
  // Verificar se a entrada está ativa antes de configurar estado inicial
  bool entradaAtiva = digitalRead(entrada) == HIGH;
  
  if (!entradaAtiva) {
    // Entrada desacionada - todos os modos iniciam com relés desligados
    debugPrint("🔴 Entrada desacionada - iniciando com relés desligados");
    ligarRele(false);
    return;
  }
  
  // Configurar estado inicial baseado no modo (apenas quando entrada ativa)
  switch (estadoAtual) {
    case MODO_1: // Retardo na energização - inicia desligado
      debugPrint("⏰ Modo 1: Retardo na energização - iniciando desligado");
      ligarRele(false);
      break;
    case MODO_2: // Retardo na desenergização - inicia ligado
      debugPrint("⏰ Modo 2: Retardo na desenergização - iniciando ligado");
      ligarRele(true);
      modoEstrela = false; // Inicializar como false para MODO 2
      break;
    case MODO_3: // Cíclico com início ligado
      debugPrint("🔄 Modo 3: Cíclico com início ligado");
      ligarRele(true);
      break;
    case MODO_4: // Cíclico com início desligado
      debugPrint("🔄 Modo 4: Cíclico com início desligado");
      ligarRele(false);
      break;
    case MODO_5: // Partida estrela-triângulo - inicia desligado
      debugPrint("⭐ Modo 5: Partida estrela-triângulo - iniciando desligado");
      ligarRele(false); // Inicia com relés desligados
      break;
  }
}

void executarMaquinaEstados() {
  // Verificar estado da entrada para modos que dependem dela
  bool entradaAtiva = digitalRead(entrada) == HIGH;
  // debugPrint("STATUS DA ENTRADA: " + String(entradaAtiva ? "ATIVA" : "INATIVA"));
  tempoAtual = (millis() - tempoInicio) / 1000; // converter para segundos
  
  switch (estadoAtual) {
    case MODO_1: // Retardo na energização
      if (!entradaAtiva) {
        // Entrada desacionada - desligar relés imediatamente
        if (relesLigados) {
          debugPrint("🔴 MODO 1: Entrada desacionada - desligando relés imediatamente");
          ligarRele(false);
          // Resetar temporizador para próxima operação
          tempoInicio = millis();
          tempoAtual = 0;
        }
      } else if (entradaAtiva && !relesLigados) {
        // Entrada acionada e relés desligados - controlar temporizador
        if (tempoAtual == 0) {
          // Primeira vez que entrada foi acionada - iniciar temporizador
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("⏰ MODO 1: Entrada acionada - iniciando temporizador de " + String(config.tempo1) + "s");
        } else if (tempoAtual >= config.tempo1) {
          // Tempo atingido - ligar relés
          debugPrint("✅ MODO 1 CONCLUÍDO - Relés ligados após " + String(config.tempo1) + "s");
          ligarRele(true);
          debugPrint("👁️  MODO 1: Permanecendo ativo para monitorar entrada");
        }
      }
      // Se entrada estiver ativa e relés já estiverem ligados, não faz nada
      // apenas continua monitorando para detectar quando entrada for desacionada
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
          tempoInicio = millis();
          tempoAtual = 0;
        }
      } else if (entradaAtiva) {
        // Entrada acionada - controlar ciclo
        if (relesLigados && tempoAtual == 0) {
          // Primeira vez que entrada foi acionada com relés ligados - iniciar temporizador T1
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("⏰ MODO 3: Iniciando ciclo - relés ligados, aguardando " + String(config.tempo1) + "s para desligar");
        } else if (relesLigados && tempoAtual >= config.tempo1) {
          // Tempo T1 atingido - desligar relés e iniciar temporizador T1
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
          tempoInicio = millis();
          tempoAtual = 0;
        }
      } else if (entradaAtiva) {
        // Entrada acionada - controlar ciclo
        if (!relesLigados && tempoAtual == 0) {
          // Primeira vez que entrada foi acionada com relés desligados - iniciar temporizador T1
          tempoInicio = millis();
          tempoAtual = 0;
          debugPrint("⏰ MODO 4: Iniciando ciclo - relés desligados, aguardando " + String(config.tempo1) + "s para ligar");
        } else if (!relesLigados && tempoAtual >= config.tempo1) {
          // Tempo T1 atingido - ligar relés e iniciar temporizador T1
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
        // Entrada desacionada - desligar relés imediatamente
        if (relesLigados) {
          debugPrint("🔴 MODO 5: Entrada desacionada - desligando relés imediatamente");
          ligarRele(false);
          modoEstrela = true; // Reset para modo estrela
          tempoInicio = millis();
          tempoAtual = 0;
        }
      } else if (entradaAtiva && !relesLigados) {
        // Entrada acionada e relés desligados - iniciar modo estrela
        debugPrint("🟢 MODO 5: Entrada acionada - iniciando modo estrela");
        ligarReleEstrela();
        tempoInicio = millis();
        tempoAtual = 0;
      } else if (entradaAtiva && modoEstrela && tempoAtual >= config.tempo1) {
        // Transição para triângulo após tempo configurado
        debugPrint("⭐→🔺 MODO 5: Transição para triângulo após " + String(config.tempo1) + "s");
        debugPrint("⏱️  Desligando Relé 1 por " + String(TEMPO_TRANSICAO_ESTRELA_TRIANGULO) + "ms");
        
        // Desligar Relé 1 por tempo de transição
        digitalWrite(saida1, LOW);
        delay(TEMPO_TRANSICAO_ESTRELA_TRIANGULO);
        
        // Ligar Relé 2 (modo triângulo)
        ligarReleTriangulo();
        modoEstrela = false;
        tempoInicio = millis();
        tempoAtual = 0;
      }
      break;
  }
}

void verificarAlteracaoManual() {
  // Verificar se houve alteração manual nos relés
  if (relesLigados != relesLigadosAnterior) {
    debugPrint("🔧 ALTERAÇÃO MANUAL DETECTADA:");
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
    debugPrint("🔧 ALTERAÇÃO MANUAL DETECTADA (Modo 5):");
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
  unsigned long timestamp = millis() / 1000; // timestamp em segundos
  String notificacao = "MANUAL|" + String(timestamp) + "|" + tipoAlteracao + "|" + String(novoEstado ? "ON" : "OFF");
  
  debugPrint("📤 NOTIFICAÇÃO DE ALTERAÇÃO MANUAL:");
  debugPrint("   " + notificacao);
  
  enviarNotificacao(notificacao);
}

void ligarRele(bool ligar) {
  relesLigados = ligar;
  
  if (ligar) {
    digitalWrite(saida1, HIGH);
    digitalWrite(saida2, HIGH);
    // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
    debugPrint("🔌 RELÉS LIGADOS (GPIO25 e GPIO32)");
    enviarNotificacao("ON");
  } else {
    digitalWrite(saida1, LOW);
    digitalWrite(saida2, LOW);
    // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
    debugPrint("🔌 RELÉS DESLIGADOS (GPIO25 e GPIO32)");
    enviarNotificacao("OFF");
  }
}

void ligarReleEstrela() {
  // Modo estrela: apenas relé 1 ligado
  digitalWrite(saida1, HIGH);
  digitalWrite(saida2, LOW);
  // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
  relesLigados = true;
  debugPrint("⭐ MODO ESTRELA ATIVADO - Relé 1 ligado, Relé 2 desligado");
  enviarNotificacao("ON");
}

void ligarReleTriangulo() {
  // Modo triângulo: relés 2 e 3 ligados
  digitalWrite(saida1, LOW);
  digitalWrite(saida2, HIGH);
  // Não alterar saida3 (porta 2) - ela é controlada pelo status do Bluetooth
  relesLigados = true;
  debugPrint("🔺 MODO TRIÂNGULO ATIVADO - Relé 1 desligado, Relé 2 ligado");
  enviarNotificacao("ON");
}

void enviarResposta(String resposta) {
  if (deviceConnected) {
    SerialBT.println(resposta);
    debugPrint("📤 RESPOSTA ENVIADA: '" + resposta + "'");
  } else {
    debugPrint("⚠️  TENTATIVA DE ENVIAR RESPOSTA SEM CONEXÃO: '" + resposta + "'");
  }
}

void enviarNotificacao(String notificacao) {
  if (deviceConnected) {
    SerialBT.println(notificacao);
    debugPrint("📤 NOTIFICAÇÃO ENVIADA: '" + notificacao + "'");
  } else {
    debugPrint("⚠️  TENTATIVA DE ENVIAR NOTIFICAÇÃO SEM CONEXÃO: '" + notificacao + "'");
  }
}

void enviarStatusAutomatico() {
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
  String status = "STATUS|" + nomeModo + "|" + String(config.tempo1) + "|" + String(config.tempo1) + "|" + estadoReles);
  
  debugPrint("📊 ENVIANDO STATUS AUTOMÁTICO:");
  debugPrint("   " + status);
  debugPrint("   Modo: " + nomeModo);
  debugPrint("   T1: " + String(config.tempo1) + "s");
  debugPrint("   T1: " + String(config.tempo1) + "s");
  debugPrint("   Estado dos relés: " + estadoReles);
  
  enviarNotificacao(status);
}

void verificarStatusEntrada() {
  bool entradaAtiva = digitalRead(entrada) == HIGH;
  
  if (deviceConnected) {
    if (entradaAtiva) {
      enviarNotificacao("INFO: Entrada ATIVA - Modo não pode ser alterado");
    } else {
      enviarNotificacao("INFO: Entrada INATIVA - Modo pode ser alterado");
    }
  }
  
  debugPrint("📊 STATUS DA ENTRADA: " + String(entradaAtiva ? "ATIVA" : "INATIVA"));
}

void debugPrint(String mensagem) {
  if (DEBUG_ENABLED) {
    // implementar com F()
    Serial.println(F(mensagem));
    // Serial.println(mensagem);
  }
}
