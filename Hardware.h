// Hardware.h
#pragma once

#include <Arduino.h>
#include "Pines.h"
#include "Tipos.h"
#include "Constantes.h"

class Hardware {
public:
  explicit Hardware(const ConfigHardware& cfg);

  void iniciar();
  void actualizar();

  // -------------------------
  // Salidas (relés / motor)
  // -------------------------
  void motorActivar(bool activar);
  void sentidoPoner(SentidoGiro sentido);
  void desenclavarPulso();                 // pulso no-bloqueante (se gestiona en actualizar)
  void activarRelePolaridad(bool activar);  // true/false según tu cableado
  void indicarEnDestino(bool enDestino);

  // -------------------------
  // Entradas
  // -------------------------
  bool leerAutoajusteVia1() const;
  bool leerPulsoPaso() const;
  bool leerVia1() const;

  // -------------------------
  // Utilidades
  // -------------------------
  void probarSalidas(uint32_t retardoMs = 200);

private:
  ConfigHardware _cfg;

  // Estado del pulso de desenclavamiento
  bool _pulsoDesenclavarActivo = false;
  uint32_t _tPulsoInicio = 0;

  // Helpers
  void escribirSalida(int pin, bool activo);
  bool leerEntrada(int pin) const;
};

