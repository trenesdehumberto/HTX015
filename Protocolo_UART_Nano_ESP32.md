# Protocolo de Comunicación UART — Nano (TT9152) ↔ ESP32

## Parámetros físicos

| Parámetro | Valor |
|---|---|
| Velocidad | **115200 baudios** |
| Bits de datos | 8 |
| Paridad | Ninguna |
| Bits de parada | 1 |
| Fin de línea | `\n` (LF) o `\r\n` (CRLF) |
| Codificación | ASCII |

El Nano comparte el mismo puerto serie (UART0, pines D0/D1) para el monitor serie del usuario y para la comunicación con el ESP32. El ESP32 **no debe conectar sus pines TX/RX al Nano mientras el cable USB de programación esté enchufado**.

---

## Dirección de los mensajes

```
ESP32  →  Nano :  prefijo  CMD:
Nano   →  ESP32:  prefijos ST:  /  CFG:
```

---

## 1. Mensajes ESP32 → Nano (comandos)

Todos los comandos que el ESP32 envía al Nano llevan el prefijo `CMD:` seguido del nombre del comando en mayúsculas. El Nano los detecta en `actualizar()` y los procesa en `procesarComandoESP32()`.

### Formato general

```
CMD:<COMANDO>\n
CMD:<COMANDO>:<PARÁMETRO>\n
CMD:<COMANDO>:<P1>:<P2>:<P3>\n
```

### Tabla de comandos disponibles

| Comando | Parámetros | Descripción |
|---|---|---|
| `CMD:MOVERVIA:<n>` | `n` = número de vía (1..24) | Mueve el puente a la vía lógica indicada por la ruta más corta |
| `CMD:PARAR` | — | Para el movimiento en curso |
| `CMD:REANUDAR` | — | Reanuda la última orden tras una parada |
| `CMD:GIRO180` | — | Gira 180° en sentido horario |
| `CMD:REFERENCIAAQUI` | — | Establece la posición actual como posición 1 (referencia) |
| `CMD:VIACLEAR` | — | Elimina todas las vías 2..24, activa modo Normal |
| `CMD:MODNORMAL` | — | Activa el modo de recorrido Normal |
| `CMD:MODINDEXADO` | — | Activa el modo de recorrido Indexado |
| `CMD:GUARDAR` | — | Guarda la configuración en EEPROM |
| `CMD:MH:<n>` | `n` = pasos (1..48) | Mueve `n` pasos en sentido horario |
| `CMD:MA:<n>` | `n` = pasos (1..48) | Mueve `n` pasos en sentido antihorario |
| `CMD:VIADEL:<n>` | `n` = número de vía (2..24) | Elimina la vía indicada con reordenación |
| `CMD:VIAADDNUM:<v>:<p>:<s>` | `v`=vía, `p`=posición, `s`=sensor | Añade o inserta una vía con posición y sensor |
| `CMD:SENSORGLOBAL:<s>` | `s` = dirección sensor (0=desactivar) | Configura el sensor LocoNet global de ocupación |
| `CMD:GETCFG` | — | Solicita al Nano que emita el estado + configuración completa |
| `CMD:LNBASE:<n>` | `n` = dirección base LocoNet (1..2047) | Cambia la dirección base del bloque de desvíos LocoNet |
| `CMD:MODCONTADOR:<m>` | `m` = 0 (interno) / 1 (externo) | Cambia el modo de contador de posición |

### Ejemplos

```
CMD:MOVERVIA:3\n          → mueve el puente a la vía lógica 3
CMD:GIRO180\n             → giro de 180 grados
CMD:MH:2\n                → avanza 2 pasos horario
CMD:VIAADDNUM:3:15:5\n    → crea/inserta vía 3 en posición 15 con sensor LocoNet 5
CMD:SENSORGLOBAL:10\n     → activa sensor global en dirección 10
CMD:SENSORGLOBAL:0\n      → desactiva sensor global
CMD:LNBASE:229\n          → establece dirección base LocoNet en 229
CMD:MODCONTADOR:1\n       → activa contador externo (encoder Hall)
```

---

## 2. Mensajes Nano → ESP32 (respuestas y notificaciones)

El Nano emite mensajes al ESP32 de forma **espontánea** (al completar un movimiento, al cambiar la configuración) o **bajo demanda** (en respuesta a un `CMD:GETCFG`). Nunca lleva prefijo `CMD:`.

### 2.1 Mensaje de estado — `ST:`

Se emite automáticamente al:
- Completar un movimiento.
- Ejecutar cualquier cambio de configuración.
- Recibir `CMD:GETCFG`.

**Formato cuando el puente está enclavado:**
```
ST:ENCLAVADO:VIA:<via>:POS:<pos>:MODO:<modo>:V1:<v1>\n
```

**Formato cuando el puente está en movimiento:**
```
ST:MOVIMIENTO\n
```

**Campos:**

| Campo | Tipo | Descripción |
|---|---|---|
| `VIA:<via>` | entero 0..24 | Vía lógica actual (0 si no hay vía en esa posición) |
| `POS:<pos>` | entero 0..48 | Posición física actual (0 si la referencia no está fijada) |
| `MODO:<modo>` | 0 / 1 | 0 = Normal, 1 = Indexado |
| `V1:<v1>` | 0 / 1 | 0 = referencia no fijada, 1 = referencia fijada |

**Ejemplos:**
```
ST:ENCLAVADO:VIA:2:POS:7:MODO:0:V1:1\n
ST:ENCLAVADO:VIA:0:POS:12:MODO:1:V1:1\n
ST:MOVIMIENTO\n
```

---

### 2.2 Configuración completa — `CFG:`

Se emite en respuesta a `CMD:GETCFG`, tras `CMD:SENSORGLOBAL`, `CMD:LNBASE`, `CMD:MODCONTADOR`, y tras `reset eeprom`.

Consiste en **una línea META** seguida de **una línea por vía configurada** y una línea de cierre `CFG:END`.

#### Línea META

```
CFG:META:V1:<v1>:POS:<pos>:MODO:<modo>:MC:<mc>:SG:<sg>:LN:<ln>:N:<n>\n
```

**Campos:**

| Campo | Tipo | Descripción |
|---|---|---|
| `V1:<v1>` | 0 / 1 | 1 = referencia fijada |
| `POS:<pos>` | entero 0..48 | Posición física actual |
| `MODO:<modo>` | 0 / 1 | 0 = Normal, 1 = Indexado |
| `MC:<mc>` | 0 / 1 | 0 = contador interno, 1 = contador externo |
| `SG:<sg>` | entero 0..255 | Dirección del sensor global (0 = desactivado) |
| `LN:<ln>` | entero 1..2047 | Dirección base LocoNet |
| `N:<n>` | entero 0..24 | Número total de vías configuradas |

#### Líneas de vía

Una por cada vía configurada (solo las que tienen posición asignada):

```
CFG:VIA:<via>:<pos>:<sensor>\n
```

| Campo | Tipo | Descripción |
|---|---|---|
| `<via>` | entero 1..24 | Número de vía lógica |
| `<pos>` | entero 1..48 | Posición física asignada |
| `<sensor>` | entero 0..255 | Sensor LocoNet (0 = sin sensor) |

#### Línea de cierre

```
CFG:END\n
```

#### Ejemplo completo de bloque CFG

```
CFG:META:V1:1:POS:1:MODO:0:MC:0:SG:10:LN:225:N:5\n
CFG:VIA:1:1:1\n
CFG:VIA:2:7:2\n
CFG:VIA:3:14:3\n
CFG:VIA:4:21:4\n
CFG:VIA:5:28:5\n
CFG:END\n
```

---

## 3. Otros mensajes del Nano visibles en el puerto serie

El Nano también emite mensajes de texto libre (para el monitor serie humano) con los prefijos `[OK]`, `[ERROR]`, `[AVISO]` y encabezados como `--- Estado TT9152 ---`. El ESP32 **debe ignorar todas las líneas que no empiecen por `ST:` o `CFG:`**.

---

## 4. Flujo de arranque recomendado para el ESP32

Al encender o conectar el ESP32 al Nano:

1. Esperar 1-2 segundos a que el Nano termine de arrancar.
2. Enviar `CMD:GETCFG\n` para obtener el estado inicial y la configuración completa.
3. Procesar la respuesta (`ST:...` + bloque `CFG:META...CFG:END`).
4. A partir de ese momento, escuchar los mensajes `ST:` espontáneos que el Nano envía tras cada movimiento o cambio.

---

## 5. Diagrama de secuencia típico

```
ESP32                              Nano
  |                                  |
  |--- CMD:GETCFG\n ---------------->|
  |                                  |
  |<-- ST:ENCLAVADO:VIA:1:POS:1:... -|
  |<-- CFG:META:V1:1:POS:1:...      -|
  |<-- CFG:VIA:1:1:1                -|
  |<-- CFG:VIA:2:7:2                -|
  |<-- CFG:END                      -|
  |                                  |
  |--- CMD:MOVERVIA:3\n ------------>|
  |                                  |
  |<-- ST:MOVIMIENTO                -|  (inmediato, al iniciar)
  |                                  |
  |          [puente girando...]     |
  |                                  |
  |<-- ST:ENCLAVADO:VIA:3:POS:14:...-|  (al enclavarse en destino)
  |                                  |
  |--- CMD:SENSORGLOBAL:10\n ------->|
  |                                  |
  |<-- CFG:META:V1:1:....:SG:10:... -|
  |<-- CFG:VIA:...                  -|
  |<-- CFG:END                      -|
```

---

## 6. Notas de implementación para el ESP32

- **Lectura:** Leer línea a línea hasta encontrar `\n`. Ignorar `\r`.
- **Filtrado:** Procesar solo líneas que empiecen por `ST:` o `CFG:`.
- **Bloque CFG:** Acumular líneas desde `CFG:META` hasta `CFG:END` antes de actualizar la pantalla/web.
- **Timeout:** Si tras enviar `CMD:GETCFG` no se recibe respuesta en 3 segundos, reenviar.
- **Concurrencia:** El Nano puede enviar `ST:MOVIMIENTO` o `ST:ENCLAVADO` en cualquier momento, no solo como respuesta a un comando. El ESP32 debe procesarlos siempre.
- **Velocidad:** 115200 baudios, sin control de flujo hardware.
