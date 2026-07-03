// Hardware.cpp
#include "Hardware.h"

Hardware::Hardware(const ConfigHardware& cfg) : _cfg(cfg) {}

void Hardware::iniciar() {
  // Salidas
  pinMode(Pins::MOTOR_ENABLE, OUTPUT);
  pinMode(Pins::RELAY_DIR, OUTPUT);
  pinMode(Pins::RELAY_UNLOCK, OUTPUT);
  pinMode(Pins::RELAY_POLARITY, OUTPUT);
  pinMode(Pins::OUT_AT_DEST, OUTPUT);

  // Entradas (por defecto con PULLUP para sensores a masa)
  pinMode(Pins::IN_AUTOADJUST_T1, INPUT_PULLUP);
  pinMode(Pins::IN_STEP_PULSE, INPUT_PULLUP);

  // Estado seguro inicial
  motorActivar(false);
  escribirSalida(Pins::RELAY_UNLOCK, false);
  indicarEnDestino(true);
}

void Hardware::actualizar() {
  // Gestiona fin de pulso de desenclavamiento (no-bloqueante)
  if (_pulsoDesenclavarActivo) {
    if (millis() - _tPulsoInicio >= _cfg.pulsoDesenclavarMs) {
      escribirSalida(Pins::RELAY_UNLOCK, false);
      _pulsoDesenclavarActivo = false;
    }
  }
}

// -------------------------
// Salidas
// -------------------------
void Hardware::motorActivar(bool activar) {
  escribirSalida(Pins::MOTOR_ENABLE, activar);
}

void Hardware::sentidoPoner(SentidoGiro s) {
  if (s == SentidoGiro::Horario) {
    escribirSalida(Pins::RELAY_DIR, false);   // <-- antes era true. Intercambiar si gira del revés
  } else {
    escribirSalida(Pins::RELAY_DIR, true);    // <-- antes era false. Intercambiar si gira del revés
  }
}

void Hardware::desenclavarPulso() {
  // Re-disparo seguro
  _pulsoDesenclavarActivo = true;
  _tPulsoInicio = millis();
  escribirSalida(Pins::RELAY_UNLOCK, true);
}

void Hardware::activarRelePolaridad(bool activar) {
  // Convención: normal=true -> relé OFF (ajústalo si lo necesitas)
  // Lo dejo explícito para que lo cambies en un solo sitio.
  escribirSalida(Pins::RELAY_POLARITY, activar);
}

void Hardware::indicarEnDestino(bool enDestino) {
  escribirSalida(Pins::OUT_AT_DEST, enDestino);
}

// -------------------------
// Entradas
// -------------------------
bool Hardware::leerAutoajusteVia1() const {
  return leerEntrada(Pins::IN_AUTOADJUST_T1);
}

bool Hardware::leerPulsoPaso() const {

  //Pulso ACTIVO cuando el puente está en movimiento
  return leerEntrada(Pins::IN_STEP_PULSE);

  // Pulso ACTIVO cuando el puente esté parado (en destino)
  //return !leerEntrada(Pins::IN_STEP_PULSE);
}

bool Hardware::leerVia1() const {
  return leerEntrada(Pins::IN_AUTOADJUST_T1); // o el nombre de tu pin real de vía 1
}


// -------------------------
// Utilidades
// -------------------------
void Hardware::probarSalidas(uint32_t retardoMs) {
  // Prueba simple: activa cada salida y la desactiva
  indicarEnDestino(false);
  delay(retardoMs);

  motorActivar(true);
  delay(retardoMs);
  motorActivar(false);
  delay(retardoMs);

  escribirSalida(Pins::RELAY_DIR, true);
  delay(retardoMs);
  escribirSalida(Pins::RELAY_DIR, false);
  delay(retardoMs);

  escribirSalida(Pins::RELAY_UNLOCK, true);
  delay(retardoMs);
  escribirSalida(Pins::RELAY_UNLOCK, false);
  delay(retardoMs);

  escribirSalida(Pins::RELAY_POLARITY, true);
  delay(retardoMs);
  escribirSalida(Pins::RELAY_POLARITY, false);
  delay(retardoMs);

  indicarEnDestino(true);
}

// -------------------------
// Helpers privados
// -------------------------
void Hardware::escribirSalida(int pin, bool activo) {
  if (pin == Pins::UNUSED) return;

  // Si el relé es activo en bajo, invertimos
  bool activoAlto = (_cfg.reles == NivelActivo::ActivoAlto);
  bool nivel = activoAlto ? activo : !activo;

  digitalWrite(pin, nivel ? HIGH : LOW);
}

bool Hardware::leerEntrada(int pin) const {
  if (pin == Pins::UNUSED) return false;

  bool nivel = (digitalRead(pin) == HIGH);

  // Si entradas activas en bajo (INPUT_PULLUP), devolvemos "activo" cuando LOW
  if (_cfg.entradasActivasEnBajo) {
    return !nivel;
  } else {
    return nivel;
  }
}

