// PlataformaGiratoria.cpp
#include "PlataformaGiratoria.h"

PlataformaGiratoria::PlataformaGiratoria(Hardware& hw, const ConfigHardware& cfg, const Opciones& opt)
: _hw(hw), _cfg(cfg), _opt(opt) {}

void PlataformaGiratoria::iniciar() {
  _ocupado = false;
  _restantes = 0;

  _pasoActivo = _hw.leerPulsoPaso();
  _pasoActivoPrev = _pasoActivo;
  _tCambioPaso = millis();

  _via1Activa = _hw.leerAutoajusteVia1();
  _via1Prev = _via1Activa;
  _tCambioVia1 = millis();

  _motor(false);
  _destino(true);
  _entrar(Estado::Idle);
  //_actualizarPolaridadPorVia();  // v0.3: Se elimina para cambiar polaridad antes de llegar a posiciones de cambio
  _polaridadActiva = false;
  _hw.activarRelePolaridad(false);

}

bool PlataformaGiratoria::step(SentidoGiro sentido) {
  return moveSteps(1, sentido);
}

bool PlataformaGiratoria::moveSteps(uint16_t pasos, SentidoGiro s) {
  if (_ocupado) return false;
  if (pasos == 0) return false;

  _sentido = s;
  _restantes = pasos;
  _ocupado = true;

  _planificarTogglePolaridadParaEstePaso();
  _aplicarSentido();
  _destino(false);

  _motor(true);

  // Pulso de desenclavamiento (Hardware gestiona el apagado tras cfg.pulsoDesenclavarMs)
  delay(_cfg.tiempoAsentamientoReleMs);
  _hw.desenclavarPulso();

  _entrar(Estado::EsperandoInicioMovimiento);
  return true;
}

void PlataformaGiratoria::stop() {
  _restantes = 0;
  _ocupado = false;
  _motor(false);
  _destino(true);
  _entrar(Estado::Idle);
}

bool PlataformaGiratoria::gotoVia(uint8_t via) {
  // Aceptamos 1..48 por compatibilidad, pero trabajamos como salida lógica 1..24
  if (via < 1 || via > 48) return false;
  if (estaOcupado()) return false;

  if (!_calibrado) return false;

  uint8_t salida = _salida24DeVia48(via);     // 1..24

  // Dos destinos físicos posibles: vía física salida o salida+24
  uint8_t viaA48 = salida;                   // 1..24
  uint8_t viaB48 = (uint8_t)(salida + 24);   // 25..48

  // Convertimos esas vías físicas a posiciones internas 0..47
  uint8_t posA = _posDeVia48(viaA48);      // tu función actual (1..48 -> pos)
  uint8_t posB = _posDeVia48(viaB48);

  // Calculamos ruta más corta a A
  Ruta rA;
  rA.destinoPos = posA;
  _calcularPasosMasCorto(posA, rA.pasos, rA.sentido);
  rA.cambiosPol = _contarCambiosPolaridadEnRuta(_pos, rA.sentido, rA.pasos);

  // Calculamos ruta más corta a B
  Ruta rB;
  rB.destinoPos = posB;
  _calcularPasosMasCorto(posB, rB.pasos, rB.sentido);
  rB.cambiosPol = _contarCambiosPolaridadEnRuta(_pos, rB.sentido, rB.pasos);

  // Elegimos: menos pasos. Empate -> menos cambios de polaridad. Empate -> horario
  const Ruta* mejor = &rA;

  if (rB.pasos < rA.pasos) mejor = &rB;
  else if (rB.pasos == rA.pasos) {
    if (rB.cambiosPol < rA.cambiosPol) mejor = &rB;
    else if (rB.cambiosPol == rA.cambiosPol) {
      if (rB.sentido == SentidoGiro::Horario && rA.sentido != SentidoGiro::Horario) mejor = &rB;
    }
  }

  if (mejor->pasos == 0) return true;
  return moveSteps(mejor->pasos, mejor->sentido);
}


bool PlataformaGiratoria::turn180() {
  if (estaOcupado()) return false;
  return moveSteps(24, SentidoGiro::Horario); // 48 vías -> 180º = 24 pasos
}

void PlataformaGiratoria::fijarPosicion(uint8_t pos) {
  _pos = pos % NUM_POSICIONES;
}

void PlataformaGiratoria::calibrarVia1EnPosicionActual() {
  _posDeVia1 = _pos;
  _calibrado = true;
  _eventoCalVia1 = true;
  //_actualizarPolaridadPorVia(); //v0.3: Se elimina para cambiar polaridad antes de llegar a posiciones de cambio
  // Referencia: al definir Via 1, fijamos polaridad base = OFF
  _polaridadActiva = false;
  _hw.activarRelePolaridad(false);

}

uint8_t PlataformaGiratoria::viaActual() const {
  if (!_calibrado) return 0;

  // Vía 1 cuando _pos == _posDeVia1
  int16_t d = (int16_t)_pos - (int16_t)_posDeVia1;
  d %= NUM_POSICIONES;
  if (d < 0) d += NUM_POSICIONES;

  // 0..47 -> 1..48
  return (uint8_t)d + 1;
}

uint8_t PlataformaGiratoria::salidaActual() const {
  uint8_t via48 = viaActual();   // 1..48 (0 si no calibrado)
  if (via48 == 0) return 0;
  return (uint8_t)(((via48 - 1) % 24) + 1); // 1..24
}


bool PlataformaGiratoria::consumirEventoCalibracionVia1() {
  if (!_eventoCalVia1) return false;
  _eventoCalVia1 = false;
  return true;
}

void PlataformaGiratoria::actualizar() {
  _leerSensorPaso();
  _leerSensorVia1();

  // Auto-calibración solo si el puente está parado (PASO inactivo) y no estamos en movimiento
  _procesarAutoCalVia1();

  switch (_estado) {
    case Estado::Idle:
      break;

    case Estado::Preparando:
      break;

    case Estado::EsperandoInicioMovimiento:
      // Esperamos a que el sensor PASO pase a ACTIVO (moviendo)
      if (_pasoActivo) {
        _tInicioMovimientoReal = millis();
        _entrar(Estado::Moviendo);
      } else if (millis() - _tEstado >= _opt.timeoutInicioMovimientoMs) {
        _fallo(Estado::Error);
      }
      break;

    case Estado::Moviendo:
      _procesarTogglePolaridadEnMovimiento();
      // Esperamos a que el sensor PASO pase a INACTIVO (parado en destino)
      if (!_pasoActivo) {
        _entrar(Estado::FinalizandoPaso);
      } else if (millis() - _tEstado >= _opt.timeoutMovimientoMs) {
        _fallo(Estado::Error);
      }
      break;

    case Estado::FinalizandoPaso:
      _onPasoCompletado();
      _planificarTogglePolaridadParaEstePaso();

      if (_restantes > 0) {
        _aplicarSentido();
        _destino(false);

        _motor(true);

        delay(_cfg.tiempoAsentamientoReleMs);
        _hw.desenclavarPulso();

        _entrar(Estado::EsperandoInicioMovimiento);
      } else {
        _motor(false);
        _destino(true);
        _ocupado = false;
        _entrar(Estado::Idle);
      }
      break;

    case Estado::Error:
      _motor(false);
      _destino(true);
      _ocupado = false;
      break;
  }
}

void PlataformaGiratoria::_onPasoCompletado() {
  if (_sentido == SentidoGiro::Horario) {
    _pos = (_pos + 1) % NUM_POSICIONES;
  } else {
    _pos = (_pos == 0) ? (NUM_POSICIONES - 1) : (_pos - 1);
  }

  //_actualizarPolaridadPorVia();  //Se elimina para cambiar polaridad antes de llegar a posiciones de cambio
  
  if (_restantes > 0) _restantes--;
}

void PlataformaGiratoria::_leerSensorPaso() {
  bool raw = _hw.leerPulsoPaso();
  uint32_t now = millis();

  if (raw != _pasoActivoPrev) {
    _pasoActivoPrev = raw;
    _tCambioPaso = now;
  }

  if (now - _tCambioPaso >= _opt.antirreboteMs) {
    _pasoActivo = _pasoActivoPrev;
  }
}

void PlataformaGiratoria::_leerSensorVia1() {
  bool raw = _hw.leerAutoajusteVia1();
  uint32_t now = millis();

  if (raw != _via1Prev) {
    _via1Prev = raw;
    _tCambioVia1 = now;
  }

  if (now - _tCambioVia1 >= _opt.antirreboteVia1Ms) {
    _via1Activa = _via1Prev;
  }
}

void PlataformaGiratoria::_procesarAutoCalVia1() {
  if (!_autoCalVia1) return;

  // Solo calibramos si el puente está parado.
  // Además, evitamos calibrar en medio de una secuencia de movimiento.
  if (_ocupado) return;
  if (_pasoActivo) return;     // si está moviendo, no tocar
  if (!_via1Activa) return;    // sensor no activo

  // Si se excita externamente el sensor con el puente quieto, esta posición es vía 1
  if (!_calibrado || (_posDeVia1 != _pos)) {
    calibrarVia1EnPosicionActual();
  }
}

void PlataformaGiratoria::_entrar(Estado e) {
  _estado = e;
  _tEstado = millis();
}

void PlataformaGiratoria::_fallo(Estado siguiente) {
  _motor(false);
  _destino(true);
  _ocupado = false;
  _entrar(siguiente);
}

void PlataformaGiratoria::_aplicarSentido() {
  _hw.sentidoPoner(_sentido);
}

void PlataformaGiratoria::_motor(bool on) {
  _hw.motorActivar(on);
}

void PlataformaGiratoria::_destino(bool enDestino) {
  _hw.indicarEnDestino(enDestino);
}

//Se elimina para cambiar polaridad antes de llegar a posiciones de cambio
/*void PlataformaGiratoria::_actualizarPolaridadPorVia() {
  // Seguridad: sin calibración no podemos saber la vía real -> desactiva
  if (!_calibrado) {
    _hw.activarRelePolaridad(false);
    return;
  }

  uint8_t v = viaActual(); // 1..48
  bool activar = (v >= 12 && v <= 36);   // ACTIVO desde vía 12 hasta vía 36 (incluidas)
  _hw.activarRelePolaridad(activar);
}*/

uint8_t PlataformaGiratoria::_viaDePos(uint8_t pos) const {
  // Requiere calibración
  int16_t d = (int16_t)pos - (int16_t)_posDeVia1;
  d %= NUM_POSICIONES;
  if (d < 0) d += NUM_POSICIONES;
  return (uint8_t)d + 1; // 1..48
}

uint8_t PlataformaGiratoria::_posDeVia48(uint8_t via48) const {
  // Requiere calibración: _posDeVia1 es la pos (0..47) donde está la vía 1
  // via48: 1..48  -> offset 0..47
  uint8_t offset = (uint8_t)((via48 - 1) % NUM_POSICIONES);
  return (uint8_t)((_posDeVia1 + offset) % NUM_POSICIONES);
}

void PlataformaGiratoria::_calcularPasosMasCorto(uint8_t destinoPos, uint16_t& pasos, SentidoGiro& sentido) const {
  uint8_t cw  = (uint8_t)((destinoPos + NUM_POSICIONES - _pos) % NUM_POSICIONES);
  uint8_t ccw = (uint8_t)((_pos + NUM_POSICIONES - destinoPos) % NUM_POSICIONES);

  if (cw <= ccw) {
    pasos = cw;
    sentido = SentidoGiro::Horario;
  } else {
    pasos = ccw;
    sentido = SentidoGiro::Antihorario;
  }
}

uint8_t PlataformaGiratoria::_salida24DeVia48(uint8_t via48) {
  // 1..48 -> 1..24 (opuestas comparten número)
  return (uint8_t)(((via48 - 1) % 24) + 1);
}

bool PlataformaGiratoria::_esCruceCambioPolaridad(uint8_t curVia48, uint8_t nextVia48) const {
  bool cruza12 = (curVia48 == 11 && nextVia48 == 12) || (curVia48 == 12 && nextVia48 == 11);
  bool cruza36 = (curVia48 == 35 && nextVia48 == 36) || (curVia48 == 36 && nextVia48 == 35);
  return cruza12 || cruza36;
}

uint8_t PlataformaGiratoria::_contarCambiosPolaridadEnRuta(uint8_t startPos, SentidoGiro s, uint16_t pasos) const {
  if (!_calibrado) return 0;

  uint8_t count = 0;
  uint8_t p = startPos;

  for (uint16_t i = 0; i < pasos; i++) {
    uint8_t nextPos;
    if (s == SentidoGiro::Horario) nextPos = (uint8_t)((p + 1) % NUM_POSICIONES);
    else                            nextPos = (uint8_t)((p == 0) ? (NUM_POSICIONES - 1) : (p - 1));

    uint8_t curVia48  = _viaDePos(p);
    uint8_t nextVia48 = _viaDePos(nextPos);

    if (_esCruceCambioPolaridad(curVia48, nextVia48)) count++;

    p = nextPos;
  }

  return count;
}

/*void PlataformaGiratoria::_prepararPolaridadAntesDePaso() {
  if (!_calibrado) return;

  // Calcula la posición a la que vamos a llegar en ESTE paso
  uint8_t nextPos;
  if (_sentido == SentidoGiro::Horario) nextPos = (uint8_t)((_pos + 1) % NUM_POSICIONES);
  else                                  nextPos = (uint8_t)((_pos == 0) ? (NUM_POSICIONES - 1) : (_pos - 1));

  uint8_t curVia  = _viaDePos(_pos);
  uint8_t nextVia = _viaDePos(nextPos);

  // Conmutar al cruzar los puntos 11<->12 y 35<->36, en ambos sentidos
  if ( (curVia == 11 && nextVia == 12) || (curVia == 12 && nextVia == 11) ||
     (curVia == 35 && nextVia == 36) || (curVia == 36 && nextVia == 35) ) {
    _togglePolPendiente = true; // pero NO conmutamos aún (se hace en movimiento)
  }
}*/

/*void PlataformaGiratoria::_planificarTogglePolaridadParaEstePaso() {
  _togglePolPendiente = false;
  _togglePolHecho = false;
  bool cruza12 = (curVia == 11 && nextVia == 12) || (curVia == 12 && nextVia == 11);
  bool cruza36 = (curVia == 35 && nextVia == 36) || (curVia == 36 && nextVia == 35);

  if (!_calibrado) return;

  uint8_t nextPos;
  if (_sentido == SentidoGiro::Horario) nextPos = (uint8_t)((_pos + 1) % NUM_POSICIONES);
  else                                  nextPos = (uint8_t)((_pos == 0) ? (NUM_POSICIONES - 1) : (_pos - 1));

  uint8_t nextVia = _viaDePos(nextPos);

  if (cruza12 || cruza36) {
    _togglePolPendiente = true; // pero NO conmutamos aún
  }
}*/ //  Sustituida por la siguiente

void PlataformaGiratoria::_planificarTogglePolaridadParaEstePaso() {
  _togglePolPendiente = false;
  _togglePolHecho = false;

  if (!_calibrado) return;

  // Posición a la que vamos a llegar en ESTE paso (según el sentido actual)
  uint8_t nextPos;
  if (_sentido == SentidoGiro::Horario) nextPos = (uint8_t)((_pos + 1) % NUM_POSICIONES);
  else                                  nextPos = (uint8_t)((_pos == 0) ? (NUM_POSICIONES - 1) : (_pos - 1));

  // Vía actual y vía siguiente (1..48)
  uint8_t curVia  = _viaDePos(_pos);
  uint8_t nextVia = _viaDePos(nextPos);

  // Conmutar al cruzar los puntos 11<->12 y 35<->36, en ambos sentidos
  bool cruza12 = (curVia == 11 && nextVia == 12) || (curVia == 12 && nextVia == 11);
  bool cruza36 = (curVia == 35 && nextVia == 36) || (curVia == 36 && nextVia == 35);

  if (cruza12 || cruza36) {
    _togglePolPendiente = true; // pero NO conmutamos aquí, se hace en movimiento
  }
}


void PlataformaGiratoria::_procesarTogglePolaridadEnMovimiento() {
  if (!_togglePolPendiente) return;
  if (_togglePolHecho) return;

  // Estamos en movimiento (Estado::Moviendo) y ya pasó el retardo desde que empezó realmente
  if (millis() - _tInicioMovimientoReal < _opt.retardoCambioPolaridadMs) return;

  _polaridadActiva = !_polaridadActiva;
  _hw.activarRelePolaridad(_polaridadActiva);

  _togglePolHecho = true;
  _togglePolPendiente = false;
}

const char* PlataformaGiratoria::estadoStr() const {
  switch (_estado) {
    case Estado::Idle: return "Idle";
    case Estado::Preparando: return "Preparando";
    case Estado::EsperandoInicioMovimiento: return "EsperandoInicioMovimiento";
    case Estado::Moviendo: return "Moviendo";
    case Estado::FinalizandoPaso: return "FinalizandoPaso";
    case Estado::Error: return "Error";
    default: return "?";
  }
}
