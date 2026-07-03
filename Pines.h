// Pines.h
#pragma once

// Ajusta estos GPIO a tu cableado real en el ESP32-C3.
// Evito GPIO0 (BOOT) y dejo pines típicos que suelen estar disponibles.
// Si alguno te da problemas por tu placa concreta, cámbialo aquí.

namespace Pins {
  static constexpr int UNUSED = -1;

  // -------------------------
  // Salidas (relés / control)
  // -------------------------
  static constexpr int MOTOR_ENABLE   = 4;   // Habilita alimentación motor (relé o transistor)
  static constexpr int RELAY_DIR      = 5;   // Relé de sentido (CW/CCW)
  static constexpr int RELAY_UNLOCK   = 6;   // Relé/bobina de desenclavamiento (pulso)
  static constexpr int RELAY_POLARITY = 7;   // Relé cambio de polaridad del puente
  static constexpr int OUT_AT_DEST    = 8;   // Salida "en destino" / LED

  // -------------------------
  // Entradas
  // -------------------------
  static constexpr int IN_AUTOADJUST_T1 = 9;   // Sensor "estoy en vía 1" (autoajuste)
  static constexpr int IN_STEP_PULSE    = 10;  // Pulso "paso" / fin de movimiento (si lo tienes)
}
