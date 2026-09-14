#pragma once

// Comunicación serie.
constexpr unsigned long VELOCIDAD_SERIE = 115200UL;

// Plataforma.
constexpr uint8_t NUMERO_POSICIONES = 48;
constexpr uint8_t NUMERO_VIAS_MAXIMAS = 24;
constexpr uint8_t PASOS_GIRO_180 = NUMERO_POSICIONES / 2;

// Movimiento.
constexpr unsigned long TIEMPO_ASENTAMIENTO_RELE_MS = 20UL;
constexpr unsigned long TIEMPO_PULSO_DESENCLAVAMIENTO_MS = 500UL;
//constexpr unsigned long TIMEOUT_INICIO_MOVIMIENTO_MS = 1500UL;
constexpr unsigned long TIMEOUT_MOVIMIENTO_PASO_MS = 6000UL;

// Motor PWM.
// En Arduino Nano, analogWrite() trabaja normalmente a 490 Hz en D3.
constexpr uint8_t VELOCIDAD_PWM_INICIAL = 255;
constexpr uint8_t VELOCIDAD_PWM_MINIMA = 0;
constexpr uint8_t VELOCIDAD_PWM_MAXIMA = 255;

// Menú serie.
constexpr uint8_t TAMANO_BUFFER_SERIE = 48;

// Posiciones físicas en las que se cambia la polaridad del puente.
constexpr uint8_t POSICION_CAMBIO_POLARIDAD_A = 12;
constexpr uint8_t POSICION_CAMBIO_POLARIDAD_B = 36;

// El relé se conmuta durante el giro, nunca al enclavar o desenclavar.
constexpr uint32_t RETARDO_CAMBIO_POLARIDAD_MS = 500;

// Modo de contador de posiciones por defecto.
// Interno: usa la leva de enclavamiento.
// Externo: usa pulsos a GND en el pin SENSOR_CONTADOR_EXTERNO (D4).
// Solo configurable por web o pantalla OLED, no desde el menú serie.
// Cambiar aquí el valor para establecer el default de fábrica.
#define MODO_CONTADOR_DEFECTO ModoContador::Interno

// LocoNet (decodificación de desvíos).
// Mapeo por defecto:
//   229 rojo  -> vía 1
//   229 verde -> vía 2
//   230 rojo  -> vía 3
//   ...
//   240 rojo  -> vía 23
//   240 verde -> vía 24
constexpr uint16_t LOCONET_DIRECCION_INICIAL = 225;
