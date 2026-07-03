// Tipos.h
#pragma once

#include <Arduino.h>
#include <stdint.h>

enum class NivelActivo : uint8_t {
  ActivoAlto = 0,
  ActivoBajo = 1
};

enum class SentidoGiro : int8_t {
  Antihorario = -1,
  Horario  = 1
};

enum class EstadoMovimiento : uint8_t {
  Parado = 0,
  Moviendo = 1,
  Error = 2
};

struct ConfigHardware {
  // Nivel activo de salidas que atacan relés (depende del módulo de relés)
  NivelActivo reles = NivelActivo::ActivoAlto;

  // Nivel activo de salida "en destino"
  NivelActivo atDest = NivelActivo::ActivoAlto;

  // Entradas (si usas PULLUP, normalmente activo es LOW)
  bool entradasActivasEnBajo = true;

  // Temporizaciones básicas (ms)
  uint32_t pulsoDesenclavarMs = 500;
  uint32_t tiempoAsentamientoReleMs = 20;
  uint32_t timeoutPasoMs = 2500;
};

