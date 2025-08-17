# Strings Válidas de Comunicação - Sistema ESP32

## 📱 COMANDOS ENVIADOS PELO APP FLUTTER (RECEBIDOS PELO ESP32)

### 1. Comando de Configuração
**Formato:** `"modo|tempo1|tempo2_END"`

**Exemplos válidos:**
- `"1|300|0_END"` - Modo 1, 5 minutos, tempo2 não usado
- `"2|0|120_END"` - Modo 2, tempo1 não usado, 2 minutos
- `"3|60|30_END"` - Modo 3, 1 minuto ligado, 30s desligado
- `"4|60|30_END"` - Modo 4, 1 minuto ligado, 30s desligado
- `"5|10|0_END"` - Modo 5, 10 segundos em estrela
- `"6|0|0_END"` - Modo 6, alteração via comando bluetooth

### 2. Comando de Controle Direto dos Relés
**Formato:** `"X_END"`

**Descrição:** Altera o estado atual dos relés (liga se estiver desligado, desliga se estiver ligado)

**Exemplo:**
- `"X_END"` - Inverte o estado atual dos relés

**Regras:**
- Todos os comandos DEVEM terminar com `_END`
- Modos válidos: 1, 2, 3, 4, 5
- Tempos em segundos (máximo 20 dias = 1.728.000s)
- Separador: caractere pipe `|`
- Sem espaços extras

### 3. Execução Automática
**Descrição:** Após receber o comando de configuração, o sistema **automaticamente** inicia a execução do modo configurado, mudando o estado dos relés conforme a lógica do modo de operação.

**Não é necessário enviar comando separado para iniciar.**

---

## 📤 RESPOSTAS ENVIADAS PELO ESP32 (RECEBIDAS PELO APP FLUTTER)

### 1. Respostas de Confirmação
**Sucesso:**
- `"OK"` - Comando executado com sucesso

**Erro:**
- `"ERR: Formato inválido"` - Formato do comando incorreto
- `"ERR: Modo já em execução"` - Tentativa de iniciar modo em execução

### 2. Notificações de Estado dos Relés
**Estados básicos:**
- `"ON"` - Relés ligados
- `"OFF"` - Relés desligados

**Estados especiais (Modo 5 - Estrela-Triângulo):**
- `"ON"` - Relés em modo estrela ou triângulo (ambos são considerados "ligados")

### 3. Status Automático
**Formato:** `"STATUS|modo|tempo1|tempo2|estado_reles"`

**Exemplos:**
- `"STATUS|IDLE|300|0|DESLIGADO"` - Sistema em espera, modo 1 configurado
- `"STATUS|CICLICO_INICIO_LIGADO|60|30|LIGADO"` - Modo 3 em execução
- `"STATUS|ESTRELA_TRIANGULO|10|0|ESTRELA"` - Modo 5 em execução, modo estrela
- `"STATUS|ESTRELA_TRIANGULO|10|0|TRIANGULO"` - Modo 5 em execução, modo triângulo

**Descrição dos campos:**
- **modo:** Nome descritivo do modo atual
- **tempo1:** Primeiro tempo configurado (em segundos)
- **tempo2:** Segundo tempo configurado (em segundos)
- **estado_reles:** Estado atual dos relés

**Nomes dos modos:**
- `IDLE` - Sistema em espera
- `RETARDO_ENERGIZACAO` - Modo 1
- `RETARDO_DESENERGIZACAO` - Modo 2
- `CICLICO_INICIO_LIGADO` - Modo 3
- `CICLICO_INICIO_DESLIGADO` - Modo 4
- `ESTRELA_TRIANGULO` - Modo 5

**Estados dos relés:**
- `DESLIGADO` - Relés desligados
- `LIGADO` - Relés ligados
- `ESTRELA` - Modo estrela ativo (Modo 5)
- `TRIANGULO` - Modo triângulo ativo (Modo 5)



---

## 🔄 FLUXO DE COMUNICAÇÃO COMPLETO

### 1. Sequência de Configuração e Execução
```
App Flutter → ESP32: "2|300|0_END"
ESP32 → App Flutter: "OK"
ESP32 → App Flutter: "STATUS|RETARDO_DESENERGIZACAO|300|0|LIGADO"
ESP32 → App Flutter: "OFF" (após 300 segundos)
```

### 2. Sequência de Controle Direto dos Relés
```
App Flutter → ESP32: "CHANGE_RELE_STATUS_END"
ESP32 → App Flutter: "OK"
ESP32 → App Flutter: "OFF" (se estava ligado) ou "ON" (se estava desligado)
```



### 4. Status Automático (após 3 segundos de conexão)
```
ESP32 → App Flutter: "STATUS|RETARDO_DESENERGIZACAO|300|0|LIGADO"
```

### 5. Notificações de Estado
```
ESP32 → App Flutter: "ON"    (quando relés ligam)
ESP32 → App Flutter: "OFF"   (quando relés desligam)
```



---

## ⚠️ VALIDAÇÕES E REGRAS

### Validações do ESP32
- **Modo:** Deve ser 1, 2, 3, 4 ou 5
- **Tempos:** Máximo 1.728.000 segundos (20 dias)
- **Formato:** Exatamente "modo|tempo1|tempo2_END"
- **Terminação:** Obrigatório terminar com "_END"
- **Comando especial:** "CHANGE_RELE_STATUS_END" para controle direto dos relés

### Comportamentos do Sistema
- **Status automático:** Enviado automaticamente após 3 segundos de conexão
- **Notificações:** Enviadas sempre que há mudança de estado
- **Persistência:** Configurações são salvas na EEPROM

### Tratamento de Erros
- Comandos inválidos retornam "ERR: Formato inválido"
- Sistema continua funcionando mesmo com comandos inválidos

---

## 📋 EXEMPLOS COMPLETOS DE USO

### Exemplo 1: Configurar e Executar Modo 1
```
1. App → ESP32: "1|300|0_END"
2. ESP32 → App: "OK"
3. ESP32 → App: "STATUS|RETARDO_ENERGIZACAO|300|0|DESLIGADO"
4. ESP32 → App: "ON" (após 5 minutos)
```

### Exemplo 2: Configurar e Executar Modo 3
```
1. App → ESP32: "3|60|30_END"
2. ESP32 → App: "OK"
3. ESP32 → App: "STATUS|CICLICO_INICIO_LIGADO|60|30|LIGADO"
4. ESP32 → App: "OFF" (após 1 minuto)
5. ESP32 → App: "ON" (após 30 segundos)
6. ESP32 → App: "OFF" (após 1 minuto)
... (ciclo continua)
```

### Exemplo 3: Configurar e Executar Modo 5
```
1. App → ESP32: "5|10|0_END"
2. ESP32 → App: "OK"
3. ESP32 → App: "STATUS|ESTRELA_TRIANGULO|10|0|ESTRELA"
4. ESP32 → App: "STATUS|ESTRELA_TRIANGULO|10|0|TRIANGULO" (após 10s)
```

### Exemplo 4: Controle Direto dos Relés
```
1. App → ESP32: "CHANGE_RELE_STATUS_END"
2. ESP32 → App: "OK"
3. ESP32 → App: "OFF" (se estava ligado) ou "ON" (se estava desligado)
```

---

## 🔧 IMPLEMENTAÇÃO NO APP FLUTTER

### Estrutura de Comandos
```dart
class ComandosESP32 {
  // Configurar modo e iniciar execução automaticamente
  static String configurarModo(int modo, int tempo1, int tempo2) {
    return "$modo|$tempo1|$tempo2_END";
  }
  
  // Alterar estado dos relés diretamente
  static String alterarEstadoRele() {
    return "CHANGE_RELE_STATUS_END";
  }
}
```

### Estrutura de Respostas
```dart
class RespostasESP32 {
     // Tipos de resposta
   static const String OK = "OK";
   static const String ERRO_FORMATO = "ERR: Formato inválido";
  
  // Tipos de notificação
  static const String ON = "ON";
  static const String OFF = "OFF";
  
     // Prefixos para parsing
   static const String STATUS_PREFIX = "STATUS|";
}
```

---

## 📝 NOTAS IMPORTANTES

1. **Todos os comandos DEVEM terminar com "_END"**
2. **O ESP32 acumula caracteres até detectar "_END"**
3. **Status é enviado automaticamente após 3 segundos de conexão**
4. **Notificações são enviadas em tempo real**

5. **Configurações são persistentes na EEPROM**
6. **Sistema é robusto a comandos inválidos**
7. **Conexão Bluetooth é restaurada automaticamente**
