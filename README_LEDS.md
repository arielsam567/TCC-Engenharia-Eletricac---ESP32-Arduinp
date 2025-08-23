# 📋 Documentação dos LEDs - Sistema de Controle de Relés

## 🎯 Visão Geral
Este documento descreve o comportamento dos LEDs indicadores do sistema de controle de relés multifuncional, especificando as cores, portas e estados para cada modo de operação.

## 🔌 Configuração das Portas dos LEDs

| Porta | Cor do LED | Função |
|-------|------------|---------|
| **GPIO 23** | 🔵 **Azul** | Indicador do Modo 1 e 2 |
| **GPIO 22** | 🟡 **Amarelo** | Indicador do Modo 3 e 4 |
| **GPIO 21** | 🔴 **Vermelho** | Indicador de leitura da porta 34 (sensor ZMPT101B) |


## 📊 Tabela de Estados dos LEDs por Modo

### **Modo 1 - Retardo na Energização**
- **LED Azul (GPIO 23)**: 🔵 **ACESO** - Indica modo ativo
- **LED Amarelo (GPIO 22)**: ⚫ **DESLIGADO** - Modo não utilizado
- **Descrição**: Sistema inicia desligado e aguarda tempo configurado para energizar

### **Modo 2 - Retardo na Desenergização**
- **LED Azul (GPIO 23)**: 🔵 **PISCANDO** - Indica modo ativo com contagem regressiva
- **LED Amarelo (GPIO 22)**: ⚫ **DESLIGADO** - Modo não utilizado
- **Descrição**: Sistema inicia ligado e aguarda tempo configurado para desenergizar

### **Modo 3 - Cíclico com Início Ligado**
- **LED Azul (GPIO 23)**: ⚫ **DESLIGADO** - Modo não utilizado
- **LED Amarelo (GPIO 22)**: 🟡 **ACESO** - Indica modo ativo
- **Descrição**: Sistema opera em ciclo contínuo, iniciando na posição ligada

### **Modo 4 - Cíclico com Início Desligado**
- **LED Azul (GPIO 23)**: ⚫ **DESLIGADO** - Modo não utilizado
- **LED Amarelo (GPIO 22)**: 🟡 **PISCANDO** - Indica modo ativo com operação cíclica
- **Descrição**: Sistema opera em ciclo contínuo, iniciando na posição desligada

### **Modo 5 - Partida Estrela-Triângulo**
- **LED Azul (GPIO 23)**: 🔵 **ACESO** - Indica modo ativo
- **LED Amarelo (GPIO 22)**: 🟡 **ACESO** - Indica modo ativo
- **Descrição**: Ambos os LEDs ficam acesos durante a operação estrela-triângulo

## 🔴 LED Vermelho - Indicador de Tensão AC

### **Funcionamento do LED Vermelho (GPIO 21)**
- **Cor**: 🔴 **Vermelho**
- **Função**: Indicador de presença de tensão AC na porta 34 (sensor ZMPT101B)
- **Comportamento**:
  - **ACESO**: 🔴 **LIGADO** - Tensão AC detectada (porta 34 = true)
  - **DESLIGADO**: ⚫ **DESLIGADO** - Sem tensão AC (porta 34 = false)
- **Descrição**: Este LED fornece feedback visual em tempo real sobre a presença de tensão elétrica no sistema

## 🔄 Estados de Transição

### **Modo 5 - Transição Estrela-Triângulo**
Durante a transição entre estrela e triângulo:
- **LED Azul (GPIO 23)**: 🔵 **ACESO** - Mantém-se aceso
- **LED Amarelo (GPIO 22)**: 🟡 **ACESO** - Mantém-se aceso
- **Tempo de Transição**: 150ms (configurável)

## ⚠️ Estados de Erro e Inicialização

### **Sistema Inicializando**
- **Todos os LEDs**: ⚫ **DESLIGADOS** - Durante boot e configuração inicial

### **Erro de Comunicação**
- **LED Azul (GPIO 23)**: 🔵 **PISCANDO RÁPIDO** - Indica erro de comunicação
- **LED Amarelo (GPIO 22)**: 🟡 **PISCANDO RÁPIDO** - Indica erro de comunicação

### **Modo Inválido**
- **Todos os LEDs**: ⚫ **DESLIGADOS** - Sistema em estado de erro

## 🎨 Especificações Técnicas dos LEDs

### **LED Azul (GPIO 23)**
- **Cor**: Azul
- **Tensão de Operação**: 3.3V
- **Corrente**: 20mA (com resistor limitador)
- **Função**: Indicador dos Modos 1, 2 e 5

### **LED Amarelo (GPIO 22)**
- **Cor**: Amarelo
- **Tensão de Operação**: 3.3V
- **Corrente**: 20mA (com resistor limitador)
- **Função**: Indicador dos Modos 3, 4 e 5

### **LED Vermelho (GPIO 21)**
- **Cor**: Vermelho
- **Tensão de Operação**: 3.3V
- **Corrente**: 20mA (com resistor limitador)
- **Função**: Indicador de tensão AC (porta 34 - sensor ZMPT101B)
- **Comportamento**: Acende quando há tensão AC, apaga quando não há



## 🔧 Configuração e Manutenção

### **Verificação dos LEDs**
1. **Teste de Inicialização**: Todos os LEDs devem piscar uma vez durante o boot
2. **Verificação de Modo**: Confirmar que o LED correto está aceso para o modo ativo
3. **Verificação de Transições**: Observar mudanças de estado durante troca de modos
4. **Verificação do LED Vermelho**: Testar com e sem tensão AC para confirmar funcionamento do sensor

### **Troubleshooting**
- **LED não acende**: Verificar conexão elétrica e resistor limitador
- **LED pisca incorretamente**: Verificar código e configuração do modo
- **LED sempre aceso**: Verificar se há curto-circuito ou problema no código
- **LED Vermelho não responde à tensão**: Verificar conexão do sensor ZMPT101B na porta 34
- **LED Vermelho sempre aceso**: Verificar se há tensão AC constante ou problema no sensor
- **LED Vermelho sempre apagado**: Verificar se há tensão AC ou problema na leitura da porta 34

## 📝 Notas de Implementação

- Os LEDs são controlados via GPIO do ESP32
- Cada modo tem um comportamento específico para os LEDs
- O sistema permite transição suave entre modos
- Os LEDs fornecem feedback visual do estado atual do sistema
- Configuração via Bluetooth Serial disponível para alteração de modos

## 🔗 Arquivos Relacionados

- `codigo.ino` - Código principal do sistema
- `FUNCIONAMENTO_MODOS.md` - Documentação detalhada dos modos de operação
- `COMUNICACAO_APP_ESP32.md` - Protocolo de comunicação Bluetooth

---

**Última Atualização**: Dezembro 2024  
**Versão**: 1.0  
**Autor**: Sistema de Controle de Relés - TCC Engenharia Elétrica
