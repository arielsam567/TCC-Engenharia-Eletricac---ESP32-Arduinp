# Documentação de Comunicação App-ESP32

## Projeto de Firmware

O firmware foi implementado na **IDE Arduino** (núcleo ESP32 v3.1.2) em C, utilizando apenas as funções padrão do ambiente. A adoção de uma arquitetura em camadas facilita a manutenção e os testes.

### Arquitetura de estados

1. **HAL** – encapsula GPIO (`pinMode`, `digitalWrite`), contagem de tempo com `millis()` e a API BLE do próprio núcleo Arduino-ESP32 (`#include <BLEDevice.h>`).

2. **Core** – máquina de estados finita que executa os cinco modos de temporização descritos na Fundamentação Teórica.

3. **Service** – interpreta comandos BLE, grava parâmetros persistentes na `EEPROM` (`Preferences`) e publica eventos para o `loop()` principal.

Os intervalos T, T_on, T_off e T_Y são contados com a função `millis()` (resolução de 1 milissegundo). Quando o tempo programado expira, o estado dos relés é atualizado e a contagem reinicia. Para intervalos longos (até 20 dias) o valor recebido via BLE é armazenado em segundos, evitando estouro de variáveis de 32 bits.

A sequência completa de execução parte do Power-on/Reset, passa pela inicialização de periféricos e pelo anúncio BLE, ramificando-se em dois caminhos:
- O ciclo de configuração, onde a string "modo|t1|t2" é validada, armazenada na EEPROM e confirmada ao usuário
- O ciclo de contagem, responsável por atualizar os relés no instante programado e notificar o estado "ON/OFF"

Caso a conexão BLE se perca, o firmware retorna automaticamente ao estado de anúncio, garantindo robustez sem intervenção do operador.

### Protocolo BLE

#### Biblioteca e pareamento

Utiliza-se a biblioteca BLE nativa do núcleo Arduino-ESP32, dispensando dependências externas e LE Secure Connections. O pareamento opera no modo "Just Works", adequado a ambientes onde o acesso físico ao equipamento é restrito.

#### Formato de comando

O sistema recebe dois tipos de comandos em formato de string:

**1. Comando de configuração:**
```
"modo|tempo_1|tempo_2"
```
(todos os campos em segundos, separados pelo caractere pipe "|").

**2. Comando de entrada de sinal:**
```
"START"
```
Este comando ativa a execução do modo de operação previamente configurado.

**Exemplos**: 
- "2|300|0" ⇒ modo = 2 (Cíclico ON), T_on=300 s, T_off=0 s
- "START" ⇒ inicia a execução do modo configurado

#### Serviço e característica

Define-se um único serviço BLE contendo uma característica de texto UTF-8 (propriedades `WRITE` e `NOTIFY`). O aplicativo envia a string de configuração; o firmware responde "OK" ou "ERR" e, sempre que o contato muda, notifica "ON" ou "OFF".

#### Fluxo de comunicação

1. **Configuração inicial**: O aplicativo conecta-se ao dispositivo e descobre a característica de comando.
2. **Definição do modo**: O usuário envia a string de configuração; o firmware faz `split('|')`, converte para inteiros e atualiza a máquina de estados.
3. **Persistência**: Os parâmetros são salvos na `EEPROM`.
4. **Ativação**: O usuário envia o comando "START" para iniciar a execução do modo configurado.
5. **Execução**: O firmware executa a sequência de temporização conforme o modo configurado.
6. **Monitoramento**: Mudanças de estado geram notificações "ON"/"OFF" para atualização em tempo real na interface.
7. **Notificação de alteração manual**: Quando conectado via BLE, ao alterar manualmente o estado de algum relé, o firmware envia automaticamente uma string informando o timestamp da alteração e o novo estado, permitindo sincronização em tempo real entre o app e o hardware.

### Implementação do firmware

O firmware utiliza as bibliotecas nativas do ESP32 para BLE (`BLEDevice.h`, `BLEServer.h`) e armazenamento persistente (`Preferences.h`). A implementação segue uma estrutura modular onde:

- **Inicialização BLE**: Configura o dispositivo com nome "RelayTimer", cria um servidor BLE e um serviço com característica de comando
- **Característica de comando**: Possui propriedades WRITE e NOTIFY para receber comandos e enviar respostas
- **Callback de escrita**: Processa a string recebida, identifica se é comando de configuração ou START, valida o formato e atualiza a máquina de estados
- **Persistência**: Salva os parâmetros na EEPROM usando a biblioteca Preferences
- **Loop principal**: Verifica temporizadores, atualiza relés e envia notificações de estado

A adoção de uma única string simplifica o protocolo BLE e mantém o projeto totalmente compatível com a IDE Arduino.

## Implementação no App Flutter

### Estrutura de Comunicação

O app Flutter utiliza a biblioteca `flutter_bluetooth_serial` para estabelecer comunicação com o ESP32. A arquitetura segue o padrão Provider para gerenciamento de estado da conexão Bluetooth.

### Fluxo de Conexão

1. **Descoberta de dispositivos**: O app escaneia dispositivos BLE disponíveis
2. **Conexão**: Estabelece conexão com o dispositivo "RelayTimer"
3. **Descoberta de serviços**: Identifica o serviço BLE e a característica de comando
4. **Configuração**: Envia string de configuração no formato "modo|t1|t2"
5. **Ativação**: Envia comando "START" para iniciar a execução
6. **Monitoramento**: Recebe notificações de estado em tempo real

### Estrutura de Dados

O app Flutter utiliza duas classes principais para gerenciar a comunicação:

- **BluetoothModel**: Contém informações do dispositivo Bluetooth como nome, endereço MAC, status de conexão e estado atual da comunicação

- **ReleModel**: Armazena a configuração dos relés incluindo o modo de operação, tempos de configuração e estado atual (ligado/desligado)

### Gerenciamento de Estado

O `BTProvider` gerencia:
- Estado da conexão Bluetooth
- Lista de dispositivos descobertos
- Configuração atual dos relés
- Notificações de estado em tempo real

### Tratamento de Erros

- Timeout de conexão
- Reconexão automática em caso de perda de sinal
- Validação de comandos antes do envio
- Feedback visual para o usuário sobre o status da comunicação

## Modos de Operação

### 1. Retardo na energização
- **Formato**: "1|tempo|0"
- **Comportamento**: Aguarda o tempo especificado e liga o relé
- **Ativação**: Comando "START" inicia a contagem regressiva

### 2. Retardo na desenergização  
- **Formato**: "2|0|tempo"
- **Comportamento**: Liga o relé imediatamente e desliga após o tempo especificado
- **Ativação**: Comando "START" liga o relé e inicia a contagem

### 3. Cíclico com início ligado
- **Formato**: "3|tempo_on|tempo_off"
- **Comportamento**: Inicia ligado, alterna entre ON/OFF nos intervalos especificados
- **Ativação**: Comando "START" inicia o ciclo começando com relé ligado

### 4. Cíclico com início desligado
- **Formato**: "4|tempo_on|tempo_off"  
- **Comportamento**: Inicia desligado, alterna entre OFF/ON nos intervalos especificados
- **Ativação**: Comando "START" inicia o ciclo começando com relé desligado

### 5. Partida estrela-triângulo
- **Formato**: "5|tempo_estrela|0"
- **Comportamento**: Controla sequência de partida com tempo de transição
- **Ativação**: Comando "START" inicia a sequência de partida

## Considerações de Segurança

- Pareamento BLE no modo "Just Works" para simplicidade
- Validação de comandos no firmware
- Timeout de conexão para evitar travamentos
- Reconexão automática para robustez operacional

## Testes e Validação

- Teste de conectividade em diferentes distâncias
- Validação de todos os modos de operação
- Teste de estabilidade da conexão
- Verificação de comportamento em caso de perda de sinal
