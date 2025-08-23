# Implementação do Módulo ZMPT101B - Versão Corrigida

## Visão Geral
O código foi corrigido para usar a implementação profissional do módulo ZMPT101B com cálculo RMS correto e configuração adequada do ADC do ESP32, baseado no código de teste fornecido pelo usuário.

## Características do ZMPT101B
- **Tipo**: Sensor de tensão AC
- **Saída**: Analógica (0-3.3V)
- **Porta**: GPIO34 (ADC1_CH6)
- **Resolução**: 12 bits (0-4095)
- **Tensão de entrada**: Até 250V AC

## Configurações Implementadas

### Constantes Configuráveis
```cpp
// Configurações do sensor ZMPT101B - ADC ESP32
const adc1_channel_t ZMPT101B_CHANNEL = ADC1_CHANNEL_6; // GPIO34
const adc_atten_t ZMPT101B_ATTEN = ADC_ATTEN_DB_11;
const adc_bits_width_t ZMPT101B_WIDTH = ADC_WIDTH_BIT_12;
const int ZMPT101B_SAMPLES = 2000;        // Número de amostras para cálculo RMS
const float ZMPT101B_THRESHOLD_VRMS = 15.0f; // Threshold em Volts RMS para detectar tensão AC
const float ZMPT101B_MODULE_MEASURED_VRMS = 1.663f; // Tensão medida no pino OUT (Vrms)
const float ZMPT101B_NETWORK_VOLTAGE = 220.0f; // Tensão da rede (V)
```

### Ajuste do Threshold
- **Threshold padrão**: 15.0V RMS (ajuste conforme sua instalação)
- **Valor baixo**: Mais sensível, pode detectar ruído
- **Valor alto**: Menos sensível, mais estável
- **Unidade**: Volts RMS (não mais valores ADC brutos)

## Funcionalidades Implementadas

### 1. Leitura RMS Profissional
- Usa `adc1_get_raw()` e `esp_adc_cal_raw_to_voltage()` para leitura precisa
- Calcula tensão RMS usando método matemático correto para tensão AC
- 2000 amostras para estabilizar a leitura
- Calibração automática baseada na tensão da rede

### 2. Calibração Automática
- **Comando**: `CALIBRAR`
- **Função**: Mede o ruído de fundo sem tensão AC
- **Resultado**: Sugere novo threshold em Volts RMS automaticamente

### 3. Ajuste Manual de Threshold
- **Comando**: `THRESHOLD|valor`
- **Exemplo**: `THRESHOLD|20.5` (20.5V RMS)
- **Valores válidos**: 0.1V a 300V RMS

## Comandos Bluetooth Disponíveis

### Calibração
```
CALIBRAR
```
**Resposta**: `CALIBRACAO_INICIADA`

### Ajuste de Threshold
```
THRESHOLD|20.0
```
**Resposta**: `THRESHOLD_ALTERADO|20.0V`

## Como Calibrar

### Passo 1: Preparação
1. Certifique-se de que NÃO há tensão AC na entrada
2. Conecte via Bluetooth
3. Envie o comando: `CALIBRAR`

### Passo 2: Aguardar
- O sistema fará 2000 leituras
- Aguarde a mensagem de conclusão
- Anote o novo threshold sugerido em Volts

### Passo 3: Aplicar Threshold
1. Use o comando: `THRESHOLD|novo_valor`
2. Teste com tensão AC
3. Ajuste se necessário

## Monitoramento

### Logs de Debug
O sistema envia logs a cada 10 verificações:
```
📊 ZMPT101B - Vrms_rede: 220.45V | Vrms_modulo: 1.6654V | Threshold: 15.0V | Status: ATIVA
```

### Status via Bluetooth
- **Vrms_rede**: Tensão RMS da rede em Volts
- **Vrms_modulo**: Tensão RMS medida no módulo
- **Threshold**: Valor configurado em Volts RMS
- **Status**: ATIVA ou INATIVA

## Troubleshooting

### Problema: Detecção instável
**Solução**: Aumentar o threshold
```cpp
const float ZMPT101B_THRESHOLD_VRMS = 25.0f; // Aumentar de 15.0V para 25.0V
```

### Problema: Não detecta tensão
**Solução**: Diminuir o threshold
```cpp
const float ZMPT101B_THRESHOLD_VRMS = 10.0f; // Diminuir de 15.0V para 10.0V
```

### Problema: Muitas leituras falsas
**Solução**: Ajustar calibração
```cpp
const float ZMPT101B_MODULE_MEASURED_VRMS = 1.500f; // Ajustar valor calibrado
```

## Conexões Físicas

### ZMPT101B → ESP32
- **VCC**: 3.3V
- **GND**: GND
- **OUT**: GPIO34 (ADC1_CH6)

### Observações
- GPIO34 é entrada analógica por padrão
- Não precisa de `pinMode()`
- Tensão máxima de entrada: 3.3V
- ADC configurado com atenuação 11dB para melhor precisão

## Exemplo de Uso

### 1. Primeira Configuração
```
1. Conectar ZMPT101B
2. Enviar: CALIBRAR
3. Aguardar resultado
4. Aplicar threshold sugerido: THRESHOLD|20.5
```

### 2. Ajuste Fino
```
1. Testar com tensão AC
2. Se necessário, ajustar threshold
3. Repetir até funcionar perfeitamente
```

### 3. Monitoramento Contínuo
```
- Sistema envia status automático
- Logs de debug mostram tensões RMS
- Bluetooth permite ajustes remotos
```

## Segurança

### ⚠️ Avisos Importantes
- **NUNCA** toque nos terminais de alta tensão
- **SEMPRE** desligue a alimentação antes de conectar
- **VERIFIQUE** todas as conexões antes de energizar
- **USE** equipamentos de proteção adequados

### Recomendações
- Teste primeiro com tensão baixa
- Monitore a temperatura do módulo
- Verifique isolamento elétrico
- Mantenha distância segura de partes energizadas

## Diferenças da Versão Anterior

### ✅ Correções Implementadas
- **ADC configurado corretamente** com `adc1_config_width()` e `adc1_config_channel_atten()`
- **Cálculo RMS matemático correto** para tensão AC
- **Calibração automática** baseada na tensão da rede
- **Threshold em Volts RMS** em vez de valores ADC brutos
- **2000 amostras** para estabilizar leitura (em vez de 100)
- **Função `esp_adc_cal_raw_to_voltage()`** para conversão precisa

### 🔧 Melhorias Técnicas
- Uso das bibliotecas nativas do ESP32 (`driver/adc.h`, `esp_adc_cal.h`)
- Cálculo de tensão RMS usando desvio padrão
- Calibração automática baseada na tensão real da rede
- Logs mais informativos com valores em Volts
- Sistema de threshold mais intuitivo (Volts em vez de valores ADC)
