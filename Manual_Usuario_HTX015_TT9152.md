# Manual de Usuario — HTX015 / TT9152
## Decodificador de Plataforma Giratoria para maquetas de tren

**Versión del firmware:** Entrega 3  
**Bus de control:** LocoNet  
**Hardware:** Arduino Nano ATmega328p

---

## ¿Qué es el HTX015 / TT9152?

El HTX015 / TT9152 es un decodificador electrónico diseñado para controlar una **plataforma giratoria** Fleischman con referencia **9152c** en una maqueta de tren. Se conecta al bus LocoNet de tu central digital y permite girar el puente de la plataforma de forma precisa para alinearlo con cualquiera de las vías de salida del depósito.

El decodificador recuerda toda la configuración aunque se apague la corriente. Una vez configurado, funciona de forma autónoma recibiendo órdenes desde la central digital, desde el software de gestión (Rocrail, JMRI, etc.) o desde el ordenador a través del Monitor Serie de Arduino IDE.

---

## ¿Qué puede hacer?

- **Girar el puente** hasta cualquier vía de salida del depósito usando siempre la ruta más corta.
- **Configurar hasta 24 vías de salida**, asignando a cada una su posición en el círculo de la plataforma.
- **Modo Indexado:** los botones de avance/retroceso saltan automáticamente de vía en vía configurada.
- **Notificaciones LocoNet:** envía señales de ocupación al software de gestión cuando el puente llega a una vía.
- **Exportar/Importar** la configuración completa para hacer copias de seguridad o clonar la configuración en otro decodificador.

---

## Cómo cargar el firmware en el Arduino Nano

### Lo que necesitas

- Un ordenador
- Aplicación de Monitor Serial para la plataforma que estés usando (Disponible en Descargas)
- El cable USB para conectar el Arduino Nano al ordenador.

### Paso 1 — Conectar el cable USB

1. Conecta el cable USB al PC
2. El otro extremo a tu placa de Arduino NANO

### Paso 2 — Buscar el firmware

1. Ve a la página de https://trenes.dehumberto.es y busca la entrada de HTX015 - Plataforma Giratoria Loconet
2. Localiza al final de la página el cargador de firmware

### Paso 3 — Sube el firmware

1. Pulsa en el botón **Instalar**
2. Se abrirá una ventana del navegador para seleccionar el puerto serie donde está el cable conectado.
3. el proceso de carga comenzará y el porcentaje irá aumentando hasta que esté cargado todo.

### Paso 4 — Abrir el Monitor Serie

1. Descargar la aplicación **MonitorSerialArduino** y ejecutarla
2. Seleccionar el puerto serie donde está conectado nuestro Arduino
3. Seleccionar 115200 baudios
4. Pulsar en **Conectar**

---

## Primera puesta en marcha

Cuando el decodificador arranca por primera vez (o tras resetear a valores de fábrica), el puente no tiene posición de referencia. Sigue estos pasos en orden:

### Paso 1 — Posicionar el puente en la vía principal

Gira el puente (con los comandos `mh`/`ma` o manualmente) hasta que quede perfectamente alineado con la **vía de acceso principal** del depósito. Es la vía por la que entran y salen las locomotoras desde la línea principal.

El sensor de enclavamiento debe estar activo (puente enclavado).

### Paso 2 — Establecer la referencia

Escribe en el Monitor Serie:

```
referenciaaqui
```
o
```
v1a
```
El decodificador marcará esa posición como la **posición número 1** y la asignará automáticamente a la **vía lógica 1**. A partir de ese momento el decodificador sabe dónde está en cada momento.

### Paso 3 — Configurar las vías de salida del depósito

Para cada vía de salida que quieras usar:

1. Usa el comando `via add` para añadir todas las vías necesarias. El formato es `via add <vía> <posición> [sensor]`

Ejemplo:
```
via add 2 7
```
---

## Referencia completa de comandos

Todos los comandos se escriben en **minúsculas** en el Monitor Serie y se envían pulsando **Intro**.

---

### `ayuda` o `?`

Muestra la lista de todos los comandos disponibles directamente en pantalla.

---

### `estado`

Muestra el estado completo del decodificador en ese momento:
- Si el puente está en movimiento o parado y enclavado.
- Estado de los sensores físicos (enclavamiento y vía 1).
- La posición física actual (número del 1 al 48).
- En qué vía lógica está el puente.
- El modo de recorrido activo (Normal o Indexado).
- La velocidad del motor.
- El sensor LocoNet global configurado.

**Ejemplo de respuesta:**
```
--- Estado TT9152 ---
Puente: ENCLAVADO
Sensor enclavamiento: ACTIVO
Sensor via 1: INACTIVO
Referencia via 1: CONFIGURADA
Posicion fisica actual: 7
Via logica actual: Via 2 (configurada en posicion 7)
Modo: NORMAL
Velocidad PWM: 200
Sensor global LocoNet: 10
```

---

### `referenciaaqui` / `via1aqui` / `v1a`

Establece la posición actual del puente como la **posición de referencia** (posición 1) y la asigna a la vía lógica 1.

**Requisitos:**
- El puente debe estar completamente parado.
- El sensor de enclavamiento debe estar activo.

**Mensajes de respuesta:**
- `[OK] Referencia fisica configurada en posicion 1.` — Todo correcto.
- `[ERROR] El puente debe estar parado y enclavado.` — El puente se está moviendo o no está enclavado.

> ⚠️ Si mueves el puente manualmente desconectando el decodificador, debes volver a ejecutar `referenciaaqui` con el puente en la vía 1 para restablecer la referencia.

---

### `ph` / `paso horario`

Mueve el puente **un paso en sentido horario** (mirando la plataforma desde arriba).

- En **modo Normal:** avanza una posición (1/48 del círculo completo).
- En **modo Indexado:** salta directamente a la siguiente vía configurada en sentido horario.

**Mensajes de error:**
- `[ERROR] Primero ejecuta 'referenciaaqui'.` — Cambiar el modo de recorrido
- `[ERROR] En modo indexado deben existir al menos dos vias.`
- `[ERROR] No se puede iniciar el movimiento.` — El puente ya está en movimiento.

---

### `pa` / `paso antihorario`

Igual que `ph` pero en **sentido antihorario**.

---

### `ir <número de vía>`

Mueve el puente directamente a la vía de salida indicada, eligiendo automáticamente la **ruta más corta** (puede girar en cualquier sentido).

El decodificador considera también si la vía es accesible por el **extremo opuesto** del puente (el otro lado, a 180° de distancia). Si por el extremo opuesto está más cerca, el puente se moverá menos.

**Ejemplo:** `ir 3` — mueve el puente a la vía lógica 3 por el camino más corto.

**Mensajes de error:**
- `[ERROR] Indica una via entre 1 y 24.` — Número de vía no válido.
- `[ERROR] Primero ejecuta 'referenciaaqui'.`
- `[ERROR] La via logica X no esta configurada.`
- `[ERROR] No se puede iniciar el movimiento.`

---

### `irex <número de posición>`

Mueve el puente a una **posición física exacta** (del 1 al 48) usando siempre el extremo principal del puente (el lado del sensor de vía 1). Es útil para calibración o mantenimiento.

**Ejemplo:** `irex 25` — mueve el extremo principal del puente a la posición 25.

**Mensajes de error:**
- `[ERROR] Indica una posicion entre 1 y 48.`
- `[ERROR] Primero configura la referencia con 'via1aqui'.`
- `[ERROR] No se puede iniciar el movimiento.`

---

### `mh <número de pasos>`

Mueve el puente el número de pasos indicado en **sentido horario**. Un paso equivale a 1/48 del círculo completo (7,5 grados).

**Ejemplo:** `mh 3` — avanza 3 posiciones en sentido horario.

**Rango válido:** entre 1 y 48 pasos.

**Mensajes de error:**
- `[ERROR] Indica entre 1 y 48 pasos.`
- `[ERROR] No se puede iniciar el movimiento.`

---

### `ma <número de pasos>`

Igual que `mh` pero en **sentido antihorario**.

---

### `180` / `giro 180`

Gira el puente exactamente **180 grados** (media vuelta completa) en sentido horario. Ideal para invertir una locomotora en el depósito.

**Mensajes de error:**
- `[ERROR] No se puede iniciar el giro 180.` — El puente está en movimiento.

---

### `parar`

Detiene inmediatamente el movimiento en curso. El puente puede quedar en una posición intermedia. La última orden queda guardada para poder reanudarla más tarde desde LocoNet.

---

### `velocidad <valor>`

Ajusta la potencia del motor del puente. El valor va de **0 a 255**:
- **255** = velocidad máxima.
- **0** = motor detenido (no recomendado como valor de trabajo).
- Valores entre **100 y 220** son habituales en la práctica.

**Ejemplo:** `velocidad 180`

> El valor se guarda en memoria y se mantiene tras apagar y encender el decodificador.

**Mensajes de error:**
- `[ERROR] La velocidad debe estar entre 0 y 255.`

---

### `modo normal`

Activa el **modo Normal de recorrido**. En este modo los comandos `ph` y `pa` mueven el puente una posición mecánica por pulsación.

---

### `modo indexado`

Activa el **modo Indexado de recorrido**. En este modo los comandos `ph` y `pa` (y los botones equivalentes en LocoNet) saltan directamente de vía en vía configurada, ignorando las posiciones intermedias.

**Requisitos:**
- La referencia debe estar establecida.
- Deben existir al menos 2 vías configuradas.

**Mensajes de error:**
- `[ERROR] Primero ejecuta 'referenciaaqui'.`
- `[ERROR] Configura al menos dos vias con 'via add'.`

---

### `via add <vía> <posición> [sensor]`

Es el **comando principal de configuración** de vías. Permite añadir vías nuevas, insertar vías entre vías existentes y editar el sensor LocoNet de una vía ya configurada.

**Parámetros:**
- `<vía>`: número de vía lógica del **1 al 24**.
- `<posición>`: número de posición física del **1 al 48**.
- `[sensor]`: (opcional) número del sensor LocoNet para notificar ocupación (1–255). Si no se indica, la vía queda sin sensor.

**Las posiciones deben estar en orden creciente** según el número de vía. La vía 3 debe tener una posición mayor que la vía 2 y menor que la vía 4.

**Comportamiento según la situación:**

| Situación | Resultado |
|---|---|
| La vía **no existe** y la posición es correcta | Se crea la vía nueva |
| La vía **existe** con **misma posición**, sin sensor nuevo | `[AVISO] Via X ya existe. Sin cambios.` |
| La vía **existe** con **misma posición**, con sensor nuevo/distinto | Actualiza solo el sensor: `[OK] Via X modificada: sensor = Y` |
| La vía **existe** con **misma posición**, mismo sensor | `[AVISO] Via X ya tiene ese sensor.` |
| La vía **existe** con **posición distinta** y en orden | Inserta desplazando las vías siguientes un número arriba |
| Posición fuera del orden numérico | `[ERROR] Posicion fuera de orden o slot ocupado.` |

**Ejemplos:**
```
via add 2 10          → Crea la vía 2 en la posición 10
via add 3 20 5        → Crea la vía 3 en posición 20 con sensor LocoNet 5
via add 3 20 8        → Actualiza el sensor de la vía 3 a 8 (posición no cambia)
via add 2 15          → Inserta una nueva vía 2 en posición 15; la antigua vía 2 pasa a ser vía 3
```

> **Sobre posiciones opuestas:** El puente tiene dos extremos. Puedes configurar la vía 1 en la posición 1 y otra vía en la posición 25 (el extremo opuesto exacto). Esto es válido y el decodificador lo gestiona correctamente.

**Mensajes de error:**
- `[ERROR] Formato: via add <via 1..24> <posicion 1..48> [sensor]` — Faltan parámetros.
- `[ERROR] La via logica 1 solo puede estar en posicion 1.`
- `[ERROR] Usa primero 'referenciaaqui'.` (al modificar la vía 1 sin referencia)
- `[ERROR] Posicion no valida (ocupada o fuera de orden).`
- `[ERROR] Posicion fuera de orden o slot ocupado.`

---

### `via del <número de vía>`

Elimina una vía de salida. Las vías con número mayor se **reordenan automáticamente** para mantener la numeración consecutiva.

**Ejemplo:** Si tienes las vías 1, 2, 3 y 4, y ejecutas `via del 2`, el resultado es: vías 1, 2 y 3 (la antigua vía 3 pasa a ser la 2, la antigua vía 4 pasa a ser la 3).

**No se puede eliminar la vía 1.** La vía 1 es la referencia física y solo se puede restablecer con `referenciaaqui`.

**Mensajes de respuesta:**
- `[OK] Via logica X eliminada.`
- `[AVISO] La via X no estaba configurada.`
- `[ERROR] Indica una via entre 2 y 24.`
- `[ERROR] La via 1 se define con 'referenciaaqui' y no se elimina.`

---

### `via list` / `vias`

Muestra la lista completa de todas las vías configuradas con su posición física, la posición opuesta y el sensor LocoNet asignado (si tiene). También indica en qué vía está el puente en ese momento.

**Ejemplo de respuesta:**
```
--- Vias logicas configuradas ---
Modo de recorrido: NORMAL
Numero de vias: 4

Via 01: posicion 1  (opuesta 25)  [sensor 1]  <-- PUENTE AQUI
Via 02: posicion 7  (opuesta 31)  [sensor 2]
Via 03: posicion 14  (opuesta 38)
Via 04: posicion 21  (opuesta 45)  [sensor 4]
Via 05: sin configurar
...
```

---

### `via clear`

Elimina **todas las vías del 2 al 24** y activa el modo Normal. La vía 1 y su referencia física se conservan.

Es útil para empezar la configuración desde cero sin perder la referencia.

**Mensajes de respuesta:**
- `[OK] Vias 2..24 eliminadas.`
- `[OK] Via 1 conservada como referencia.`
- `[OK] Modo NORMAL activado.`

---

### `sensor global <número>`

Configura el número de sensor LocoNet que el decodificador activará cuando el puente esté parado en **cualquier vía**. Es un sensor de ocupación general de la plataforma.

- Usa el valor **0** para desactivar el sensor global.
- Rango válido: **1 a 255**.

**Ejemplos:**
```
sensor global 10    → Activa el sensor 10 cuando el puente esté enclavado
sensor global 0     → Desactiva el sensor global
```

**Mensajes de respuesta:**
- `[OK] Sensor global LocoNet configurado: 10`
- `[OK] Sensor global LocoNet desactivado.`
- `[ERROR] Sensor debe estar entre 0 y 65535 (0=desactivar).`

> Los sensores de vía individuales (configurados con `via add`) se activan además del sensor global cuando el puente está en esa vía concreta.

---

### `guardar`

Guarda manualmente la configuración actual en la memoria permanente (EEPROM) del decodificador. En circunstancias normales no es necesario usarlo porque el decodificador guarda automáticamente tras cada cambio. Puede usarse como confirmación de seguridad.

---

### `eeprom dump`

Muestra el contenido técnico completo de la memoria del decodificador. Es útil para verificar que la configuración se ha guardado correctamente. Los valores se muestran en formato numérico.

---

### `reset eeprom` / `fabrica`

Borra toda la configuración y **restaura los valores de fábrica**:
- Se eliminan todas las vías.
- La referencia de vía 1 queda sin configurar.
- El modo vuelve a Normal.
- El sensor global se desactiva.
- La dirección LocoNet vuelve al valor inicial (225).

> ⚠️ Tras ejecutar este comando debes volver a hacer la configuración completa desde cero, empezando por `referenciaaqui`.

**Mensajes de respuesta:**
- `[OK] EEPROM y estado en memoria restaurados a fabrica.`
- `[OK] Vias eliminadas, via 1 sin configurar, modo NORMAL.`

---

### `exportar` / `config export`

Genera una línea de texto con **toda la configuración** actual en un formato especial que puedes copiar. Esta línea se puede guardar como copia de seguridad o pegar en otro decodificador para clonarlo.

**Ejemplo de salida:**
```
import:sg:10;ln:225;modo:0;via:2:7:2;via:3:14:3;via:4:21:0
```

> La posición de la vía 1 (referencia física) **no se exporta** porque depende de la posición mecánica real de cada plataforma. Tras importar en otro decodificador debes ejecutar `referenciaaqui`.

---

### `import:<datos>`

Importa una configuración completa copiada previamente con el comando `exportar`. Pega la línea directamente en el Monitor Serie tal como fue generada.

**Ejemplo:**
```
import:sg:10;ln:225;modo:0;via:2:7:2;via:3:14:3;via:4:21:0
```

Tras importar:
- Se borran las vías 2 a 24 existentes.
- Se cargan las nuevas vías, el modo y el sensor global.
- La vía 1 y su referencia se conservan si ya estaban configuradas.

**Mensajes de respuesta:**
- `[OK] Configuracion importada.`
- `[OK] Vias importadas: X`
- `[AVISO] Ejecuta 'referenciaaqui' para establecer la referencia de via 1.`
- `[ERROR] Datos de importacion vacios.`

---

## Control por LocoNet

El decodificador también acepta órdenes directamente desde la central LocoNet o el software de gestión, sin necesidad del Monitor Serie.

### Tabla de direcciones LocoNet

El decodificador usa un bloque de **16 direcciones de desvío** a partir de la dirección base (por defecto: **225**).

| Dirección | Color | Acción |
|---|---|---|
| Base + 0 | Rojo | **Parar** el puente |
| Base + 1 | Rojo | **Reanudar** la última orden (tras parar) |
| Base + 1 | Verde | **Giro 180°** |
| Base + 2 | Rojo | **Paso horario** (siguiente vía en modo indexado) |
| Base + 2 | Verde | **Paso antihorario** |
| Base + 3 | Rojo | Fijar sentido base = horario |
| Base + 3 | Verde | Fijar sentido base = antihorario |
| Base + 4 | Rojo | Ir a **vía 1** |
| Base + 4 | Verde | Ir a **vía 2** |
| Base + 5 | Rojo | Ir a **vía 3** |
| Base + 5 | Verde | Ir a **vía 4** |
| … | … | … (patrón rojo=impar, verde=par) |
| Base + 15 | Rojo | Ir a **vía 23** |
| Base + 15 | Verde | Ir a **vía 24** |

### Cambiar la dirección base LocoNet

La dirección base se puede cambiar desde el Monitor Serie (requiere acceso al ordenador). No hay un comando de usuario para esto desde LocoNet. Contacta con el técnico de la instalación si necesitas cambiarla.

---

## Notificaciones de ocupación (sensores LocoNet)

Cuando el puente llega a su destino y se enclavado, el decodificador envía automáticamente al bus LocoNet:

1. **Sensor global** (si está configurado): se activa cuando el puente está enclavado en cualquier posición.
2. **Sensor de vía** (si la vía tiene uno asignado): se activa adicionalmente cuando el puente está en esa vía concreta.

Cuando el puente empieza a moverse, ambos sensores se desactivan automáticamente.

En tu software de gestión (Rocrail, JMRI, etc.) debes crear los sensores con los mismos números configurados en el decodificador para que el sistema los reconozca.

---

## Resolución de problemas frecuentes

### El puente no se mueve tras dar la orden

- Comprueba que el cable LocoNet está bien conectado.
- Verifica con `estado` que el puente no está ya en movimiento.
- Si la referencia dice "NO CONFIGURADA", ejecuta `referenciaaqui`.

### El puente se mueve pero no se enclavad

- El sensor de enclavamiento puede estar mal ajustado mecánicamente.
- Prueba a reducir la velocidad del motor con `velocidad 150`.

### El comando `ir X` da error "via no configurada"

- Usa `vias` para ver qué vías están configuradas.
- Añade la vía con `via add`.

### Tras apagar y encender, las vías no se cargan

- El decodificador guarda automáticamente. Si no carga, prueba `reset eeprom` y vuelve a configurar desde cero.
- Verifica que la versión del firmware instalada es la correcta.

### No se reciben notificaciones de sensor en Rocrail/JMRI

- Comprueba que el número de sensor en el software coincide exactamente con el configurado en el decodificador (`estado` o `vias`).
- Verifica que el cable LocoNet está bien conectado y que la central recibe datos del decodificador.

---

## Referencia rápida de comandos

| Comando | Descripción breve |
|---|---|
| `ayuda` | Lista de comandos |
| `estado` | Estado actual del decodificador |
| `referenciaaqui` | Establece posición de referencia (vía 1) |
| `ph` | Un paso horario |
| `pa` | Un paso antihorario |
| `ir <vía>` | Va a la vía indicada (ruta más corta) |
| `irex <pos>` | Va a la posición física exacta |
| `mh <n>` | Mueve n pasos horario |
| `ma <n>` | Mueve n pasos antihorario |
| `180` | Giro 180° |
| `parar` | Para el movimiento |
| `velocidad <v>` | Ajusta velocidad del motor (0–255) |
| `modo normal` | Activa modo Normal |
| `modo indexado` | Activa modo Indexado |
| `via add <v> <p> [s]` | Añade/edita vía |
| `via del <v>` | Elimina vía |
| `via list` | Lista vías configuradas |
| `via clear` | Borra vías 2–24 |
| `sensor global <n>` | Configura sensor LocoNet global |
| `guardar` | Guarda en memoria |
| `exportar` | Exporta configuración |
| `import:<datos>` | Importa configuración |
| `reset eeprom` | Restaura valores de fábrica |
| `eeprom dump` | Volcado técnico de memoria |
