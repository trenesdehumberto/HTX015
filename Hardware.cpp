#include "Hardware.h"

#include "Pines.h"
#include "Constantes.h"

Hardware::Hardware(const Configuracion& configuracion)
    : _configuracion(configuracion),
      _estado(EstadoPasoHardware::Parado),
      _resultadoPendiente(ResultadoPasoHardware::Ninguno),
      _sentidoActual(SentidoGiro::Horario),
      _instanteEstado(0),
      _movimientoDetectado(false),
      _haDetectadoMovimiento(false),
      _modoContador(MODO_CONTADOR_DEFECTO),
      _polaridadInvertida(false),
      _ultimoSensorEnclavamiento(false),
      _ultimoSensorContadorExterno(false),
      _eventoEnclavamiento(false),
      _eventoContadorExterno(false) {
}

     


void Hardware::iniciar() {
    // Salidas.
    pinMode(Pines::MOTOR_PWM, OUTPUT);
    pinMode(Pines::RELE_SENTIDO, OUTPUT);
    pinMode(Pines::RELE_DESENCLAVAMIENTO, OUTPUT);
    pinMode(Pines::RELE_POLARIDAD, OUTPUT);
    pinMode(Pines::INDICADOR_DESTINO, OUTPUT);

    // Entradas. Se usa pull-up porque los sensores se han previsto
    // como contactos u optoacopladores que llevan la entrada a GND.
    pinMode(Pines::SENSOR_ENCLAVAMIENTO, INPUT_PULLUP);
    pinMode(Pines::SENSOR_CONTADOR_EXTERNO, INPUT_PULLUP);
    pinMode(Pines::SENSOR_VIA_1, INPUT_PULLUP);

    // Los pines de LocoNet (D7=TX, D8=RX/ICP1) los configura exclusivamente
    // la librería mrrwa/LocoNet en LocoNet.init().
    // NO se tocan aquí para no interferir con el bus.

    // Estado seguro inicial.
    activarMotor(false);
    activarDesenclavamiento(false);
    establecerSentido(SentidoGiro::Horario);
    establecerPolaridadInvertida(false);
    establecerIndicadorDestino(true);

    _ultimoSensorEnclavamiento = puenteEnclavado();
    _ultimoSensorContadorExterno =
        leerEntradaActiva(Pines::SENSOR_CONTADOR_EXTERNO,
                          _configuracion.sensorContadorExternoActivoBajo);

    _estado = EstadoPasoHardware::Parado;
}

void Hardware::actualizar() {
    const unsigned long ahora = millis();

    // Detección por sondeo de eventos de sensores.
    const bool enclavadoAhora = puenteEnclavado();
    const bool contadorExternoAhora =
        leerEntradaActiva(Pines::SENSOR_CONTADOR_EXTERNO,
                          _configuracion.sensorContadorExternoActivoBajo);

    // Evento al entrar en estado de enclavamiento.
    if (enclavadoAhora && !_ultimoSensorEnclavamiento) {
        _eventoEnclavamiento = true;
    }

    // Evento al activarse el contador externo.
    if (contadorExternoAhora && !_ultimoSensorContadorExterno) {
        _eventoContadorExterno = true;
    }

    _ultimoSensorEnclavamiento = enclavadoAhora;
    _ultimoSensorContadorExterno = contadorExternoAhora;

    switch (_estado) {
        case EstadoPasoHardware::Parado:
        case EstadoPasoHardware::ErrorInicioMovimiento:
        case EstadoPasoHardware::ErrorFinMovimiento:
            break;

        case EstadoPasoHardware::AsentandoRele:
            if (ahora - _instanteEstado >= TIEMPO_ASENTAMIENTO_RELE_MS) {
                activarMotor(true);
                activarDesenclavamiento(true);
                _instanteEstado = ahora;
                _estado = EstadoPasoHardware::PulsandoDesenclavamiento;
            }
            break;

        case EstadoPasoHardware::PulsandoDesenclavamiento:
            if (ahora - _instanteEstado >= TIEMPO_PULSO_DESENCLAVAMIENTO_MS) {
                activarDesenclavamiento(false);

                // Desde aquí el motor queda controlado por el interruptor mecánico
                // de la plataforma y por la leva.
                _instanteEstado = ahora;
                _movimientoDetectado = false;
                _estado = EstadoPasoHardware::EnMovimiento;
            }
            break;

        case EstadoPasoHardware::EnMovimiento:
            // El contacto de enclavamiento deja de estar activo mientras el puente gira.
            if (puenteEnMovimiento()) {
                _movimientoDetectado = true;
            }

            // Solo se acepta la llegada si antes se ha detectado movimiento.
            // Así evitamos aceptar como destino la posición de salida.
            if (_movimientoDetectado && finDePasoDetectado()) {
                finalizarPasoCorrectamente();
            }
            else if (ahora - _instanteEstado >= TIMEOUT_MOVIMIENTO_PASO_MS) {
                // Protección: si la leva no ha detenido el puente en un tiempo
                // razonable, se corta el PWM del motor.
                finalizarPasoError(
                    EstadoPasoHardware::ErrorFinMovimiento,
                    ResultadoPasoHardware::ErrorFinMovimiento
                );
            }
            break;



    }
}

bool Hardware::iniciarPaso(SentidoGiro sentido) {
    if (estaOcupado()) {
        return false;
    }

    _resultadoPendiente = ResultadoPasoHardware::Ninguno;
    _movimientoDetectado = false;

    establecerIndicadorDestino(false);
    establecerSentido(sentido);

    _instanteEstado = millis();
    _estado = EstadoPasoHardware::AsentandoRele;

    return true;
}

void Hardware::parar() {
    activarMotor(false);
    activarDesenclavamiento(false);
    establecerIndicadorDestino(true);

    if (estaOcupado()) {
        _resultadoPendiente = ResultadoPasoHardware::Cancelado;
    }

    _estado = EstadoPasoHardware::Parado;
}

bool Hardware::estaOcupado() const {
    return _estado == EstadoPasoHardware::AsentandoRele ||
           _estado == EstadoPasoHardware::PulsandoDesenclavamiento ||
           _estado == EstadoPasoHardware::EsperandoInicioMovimiento ||
           _estado == EstadoPasoHardware::EnMovimiento;
}

EstadoPasoHardware Hardware::obtenerEstadoPaso() const {
    return _estado;
}

ResultadoPasoHardware Hardware::obtenerResultadoPaso() {
    const ResultadoPasoHardware resultado = _resultadoPendiente;
    _resultadoPendiente = ResultadoPasoHardware::Ninguno;
    return resultado;
}

void Hardware::establecerSentido(SentidoGiro sentido) {
    _sentidoActual = sentido;

    bool nivelAlto = (sentido == SentidoGiro::Horario);

    if (!_configuracion.sentidoAltoEsHorario) {
        nivelAlto = !nivelAlto;
    }

    escribirRele(Pines::RELE_SENTIDO, nivelAlto);
}

void Hardware::establecerVelocidadPWM(uint8_t velocidad) {
    _configuracion.velocidadPWM = velocidad;

    if (estaOcupado()) {
        analogWrite(Pines::MOTOR_PWM, velocidad);
    }
}

uint8_t Hardware::obtenerVelocidadPWM() const {
    return _configuracion.velocidadPWM;
}

void Hardware::establecerPolaridadInvertida(bool invertida) {
    _polaridadInvertida = invertida;
    escribirRele(Pines::RELE_POLARIDAD, invertida);
}

bool Hardware::obtenerPolaridadInvertida() const {
    return _polaridadInvertida;
}

void Hardware::establecerIndicadorDestino(bool activo) {
    digitalWrite(Pines::INDICADOR_DESTINO, activo ? HIGH : LOW);
}

bool Hardware::puenteEnclavado() const {
    return leerEntradaActiva(Pines::SENSOR_ENCLAVAMIENTO,
                             _configuracion.sensorEnclavamientoActivoBajo);
}

bool Hardware::puenteEnMovimiento() const {
    return !puenteEnclavado();
}

bool Hardware::sensorVia1Activo() const {
    return leerEntradaActiva(Pines::SENSOR_VIA_1,
                             _configuracion.sensorVia1ActivoBajo);
}

void Hardware::establecerModoContador(ModoContador modo) {
    _modoContador = modo;
}

ModoContador Hardware::obtenerModoContador() const {
    return _modoContador;
}

bool Hardware::finDePasoDetectado() {
    if (_modoContador == ModoContador::Externo) {
        return consumirEventoContadorExterno();
    }

    // Modo interno: usa la leva de enclavamiento
    return puenteEnclavado();
}

bool Hardware::consumirEventoContadorExterno() {
    const bool evento = _eventoContadorExterno;
    _eventoContadorExterno = false;
    return evento;
}

bool Hardware::consumirEventoEnclavamiento() {
    const bool evento = _eventoEnclavamiento;
    _eventoEnclavamiento = false;
    return evento;
}

void Hardware::escribirRele(uint8_t pin, bool activo) {
    const bool nivelAlto = _configuracion.relesActivosAlto ? activo : !activo;
    digitalWrite(pin, nivelAlto ? HIGH : LOW);
}

bool Hardware::leerEntradaActiva(uint8_t pin, bool activoBajo) const {
    const bool nivelAlto = digitalRead(pin) == HIGH;
    return activoBajo ? !nivelAlto : nivelAlto;
}

void Hardware::activarMotor(bool activar) {
    analogWrite(Pines::MOTOR_PWM,
                activar ? _configuracion.velocidadPWM : 0);
}

void Hardware::activarDesenclavamiento(bool activar) {
    escribirRele(Pines::RELE_DESENCLAVAMIENTO, activar);
}

void Hardware::finalizarPasoCorrectamente() {
    // El motor ya ha sido detenido mecánicamente por la leva.
    activarDesenclavamiento(false);
    establecerIndicadorDestino(true);

    _resultadoPendiente = ResultadoPasoHardware::Completado;
    _estado = EstadoPasoHardware::Parado;
}


void Hardware::finalizarPasoError(EstadoPasoHardware estadoError,
                                  ResultadoPasoHardware resultadoError) {
    activarMotor(false);
    activarDesenclavamiento(false);
    establecerIndicadorDestino(true);

    _resultadoPendiente = resultadoError;
    _estado = estadoError;
}
