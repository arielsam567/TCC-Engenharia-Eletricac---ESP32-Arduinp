# FUNCIONAMENTO DOS MODOS - SISTEMA DE RELÉS ESP32

## Visão Geral
Este documento explica o funcionamento detalhado de cada modo operacional do sistema de relés controlado por ESP32. O sistema possui 5 modos principais que controlam o comportamento dos relés baseado em tempos configuráveis e estados de entrada.

## Configuração dos Modos
Cada modo é configurado através de dois parâmetros de tempo:
- **Tempo 1 (T1):** Primeiro intervalo de tempo em segundos
- **Tempo 2 (T2):** Segundo intervalo de tempo em segundos

**Formato de configuração:** `modo|tempo1|tempo2`

---

## MODO 1 - Retardo na Energização
**Descrição:** Sistema aguarda um tempo antes de energizar os relés após ativação da entrada.

### Funcionamento:
1. **Entrada acionada (GPIO34 = HIGH):**
   - Sistema aguarda **X segundos** (configurado em T1)
   - Após o tempo, liga ambos os relés simultaneamente

2. **Entrada desacionada (GPIO34 = LOW):**
   - **Desliga os relés IMEDIATAMENTE** (sem retardo)

 

---

## MODO 2 - Retardo na Desenergização
**Descrição:** Sistema energiza os relés imediatamente, mas aguarda um tempo para desenergizar após desativação da entrada.

### Funcionamento:
1. **Entrada acionada (GPIO34 = HIGH):**
   - **Liga os relés IMEDIATAMENTE** (sem retardo)

2. **Entrada desacionada (GPIO34 = LOW):**
   - Sistema aguarda **X segundos** (configurado em T2)
   - Após o tempo, desliga ambos os relés

 

---

## MODO 3 - Cíclico com Início Ligado
**Descrição:** Sistema opera em ciclo contínuo, alternando entre ligado e desligado, mas apenas quando a entrada estiver acionada.

### Funcionamento:
1. **Entrada acionada (GPIO34 = HIGH):**
   - Relés iniciam **LIGADOS**
   - Após **T1 segundos**, desliga os relés
   - Após **T1 segundos** (mesmo tempo), liga os relés novamente
   - Ciclo continua infinitamente

2. **Entrada desacionada (GPIO34 = LOW):**
   - **Desliga os relés IMEDIATAMENTE** (independente do estado do ciclo)
   - Ciclo é interrompido

 

---

## MODO 4 - Cíclico com Início Desligado
**Descrição:** Sistema opera em ciclo contínuo, alternando entre ligado e desligado, mas apenas quando a entrada estiver acionada.

### Funcionamento:
1. **Entrada acionada (GPIO34 = HIGH):**
   - Relés iniciam **DESLIGADOS**
   - Após **Y segundos** (T2), liga os relés
   - Após **X segundos** (T1), desliga os relés novamente
   - Ciclo continua infinitamente

2. **Entrada desacionada (GPIO34 = LOW):**
   - **Desliga os relés IMEDIATAMENTE** (independente do estado do ciclo)
   - Ciclo é interrompido

 

## MODO 5 - Partida Estrela-Triângulo
**Descrição:** Sistema implementa partida estrela-triângulo para motores trifásicos, com transição automática após tempo configurado.

### Funcionamento:
1. **Entrada desacionada (GPIO34 = LOW):**
   - **Ambos os relés permanecem DESLIGADOS**

2. **Entrada acionada (GPIO34 = HIGH):**
   - **Relé 1 ligado** (GPIO25 = HIGH) - Modo Estrela
   - **Relé 2 desligado** (GPIO32 = LOW)
   - Motor opera em configuração estrela (tensão reduzida)

3. **Transição para Triângulo:**
   - Após **X segundos** (T1), sistema executa transição:
     - Desliga Relé 1 por **150ms** (tempo de transição)
     - Liga Relé 2 (GPIO32 = HIGH) - Modo Triângulo
     - Relé 1 permanece desligado
   - Motor opera em configuração triângulo (tensão plena)

4. **Estado final:**
   - Sistema permanece em modo triângulo até entrada ser desacionada
   - Ao desacionar entrada, ambos os relés são desligados imediatamente

 
---

## Controle da Porta 2 (GPIO2)
**Importante:** A porta 2 (GPIO2) é controlada automaticamente pelo status da conexão Bluetooth:
- **Bluetooth conectado:** Porta 2 ligada (HIGH)
- **Bluetooth desconectado:** Porta 2 desligada (LOW)

Esta porta não é afetada pelos modos operacionais e serve como indicador de status de conexão.

---

## Comandos de Controle
O sistema aceita comandos via Bluetooth para:
- **Configuração:** `modo|tempo1|tempo2`
- **Controle manual:** `X` ou `6` para alternar estado dos relés
- **Status:** Sistema envia automaticamente o status atual

---

## Observações Importantes
1. **Tempos máximos:** Cada tempo pode ser configurado até 20 dias (1.728.000 segundos)
2. **Persistência:** Configurações são salvas na EEPROM e restauradas após reset
3. **Monitoramento:** Sistema detecta e notifica alterações manuais nos relés
4. **Segurança:** Relés iniciam sempre em estado seguro (desligados para modos 1 e 4, ligados para modos 2, 3 e 5)
