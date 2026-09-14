#pragma once

#include <Arduino.h>

// Modo de conteo de posiciones del puente.
// Interno: usa la leva de enclavamiento mecánica (por defecto).
// Externo: usa pulsos a GND en el pin SENSOR_CONTADOR_EXTERNO (D4).
// Solo configurable desde la web o la pantalla OLED.
enum class ModoContador : uint8_t {
    Interno = 0,
    Externo = 1
};

enum class SentidoGiro : uint8_t {
    Horario,
    Antihorario
};

enum class EstadoPasoHardware : uint8_t {
    Parado,
    AsentandoRele,
    PulsandoDesenclavamiento,
    EsperandoInicioMovimiento,
    EnMovimiento,
    ErrorInicioMovimiento,
    ErrorFinMovimiento
};

enum class ResultadoPasoHardware : uint8_t {
    Ninguno,
    Completado,
    Cancelado,
    ErrorInicioMovimiento,
    ErrorFinMovimiento
};
