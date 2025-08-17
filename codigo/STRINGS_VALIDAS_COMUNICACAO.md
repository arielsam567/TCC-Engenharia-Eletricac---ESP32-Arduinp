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

**Regras:**
- Todos os comandos DEVEM terminar com `_END`
- Modos válidos: 1, 2, 3, 4, 5
- Tempos em segundos (máximo 20 dias = 1.728.000s)
- Separador: caractere pipe `|`
- Sem espaços extras

### 2. Comando de Ativação
**Formato:** `"START_END"`

**Descrição:** Inicia a execução do modo previamente configurado

**Exemplo:**
- `"START_END"`

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

### 1. Sequência de Configuração
```
App Flutter → ESP32: "2|300|0_END"
ESP32 → App Flutter: "OK"
```

### 2. Sequência de Ativação
```
App Flutter → ESP32: "START_END"
ESP32 → App Flutter: "OK"
```

### 3. Status Automático (após 3 segundos de conexão)
```
ESP32 → App Flutter: "STATUS|RETARDO_DESENERGIZACAO|300|0|LIGADO"
```

### 4. Notificações de Estado
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

### Comportamentos do Sistema
- **Status automático:** Enviado automaticamente após 3 segundos de conexão
- **Notificações:** Enviadas sempre que há mudança de estado
- **Persistência:** Configurações são salvas na EEPROM

### Tratamento de Erros
- Comandos inválidos retornam "ERR: Formato inválido"
- Tentativas de iniciar modo em execução retornam "ERR: Modo já em execução"
- Sistema continua funcionando mesmo com comandos inválidos

---

## 📋 EXEMPLOS COMPLETOS DE USO

### Exemplo 1: Configurar e Ativar Modo 1
```
1. App → ESP32: "1|300|0_END"
2. ESP32 → App: "OK"
3. App → ESP32: "START_END"
4. ESP32 → App: "OK"
5. ESP32 → App: "STATUS|RETARDO_ENERGIZACAO|300|0|DESLIGADO"
6. ESP32 → App: "ON" (após 5 minutos)
```

### Exemplo 2: Configurar e Ativar Modo 3
```
1. App → ESP32: "3|60|30_END"
2. ESP32 → App: "OK"
3. App → ESP32: "START_END"
4. ESP32 → App: "OK"
5. ESP32 → App: "STATUS|CICLICO_INICIO_LIGADO|60|30|LIGADO"
6. ESP32 → App: "OFF" (após 1 minuto)
7. ESP32 → App: "ON" (após 30 segundos)
8. ESP32 → App: "OFF" (após 1 minuto)
... (ciclo continua)
```

### Exemplo 3: Configurar e Ativar Modo 5
```
1. App → ESP32: "5|10|0_END"
2. ESP32 → App: "OK"
3. App → ESP32: "START_END"
4. ESP32 → App: "OK"
5. ESP32 → App: "STATUS|ESTRELA_TRIANGULO|10|0|ESTRELA"
6. ESP32 → App: "STATUS|ESTRELA_TRIANGULO|10|0|TRIANGULO" (após 10s)
```

---

## 🔧 IMPLEMENTAÇÃO NO APP FLUTTER

### Estrutura de Comandos
```dart
class ComandosESP32 {
  // Configurar modo
  static String configurarModo(int modo, int tempo1, int tempo2) {
    return "$modo|$tempo1|$tempo2_END";
  }
  
  // Iniciar execução
  static String iniciarExecucao() {
    return "START_END";
  }
}
```

### Estrutura de Respostas
```dart
class RespostasESP32 {
  // Tipos de resposta
  static const String OK = "OK";
  static const String ERRO_FORMATO = "ERR: Formato inválido";
  static const String ERRO_EXECUCAO = "ERR: Modo já em execução";
  
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
