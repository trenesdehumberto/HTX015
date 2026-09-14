#pragma once

#include <Arduino.h>
#include "Tipos.h"
#include "Constantes.h"


// Esta clase es la única autorizada para acceder a los GPIO del Arduino.
// Las futuras capas PlataformaGiratoria, LocoNet, EEPROM y web deberán
// solicitar acciones a Hardware, sin tocar los pines directamente.
class Hardware {
public:
    struct Configuracion {
      bool relesActivosAlto;
      bool sentidoAltoEsHorario;
      bool sensorEnclavamientoActivoBajo;
      bool sensorVia1ActivoBajo;
      bool sensorContadorExternoActivoBajo;
      uint8_t velocidadPWM;
      

      Configuracion(
          bool relesActivosAlto_ = true,
          bool sentidoAltoEsHorario_ = false,               //Si el sentido de giro está del revés
          bool sensorEnclavamientoActivoBajo_ = false,
          bool sensorVia1ActivoBajo_ = true,
          bool sensorContadorExternoActivoBajo_ = true,
          uint8_t velocidadPWM_ = VELOCIDAD_PWM_INICIAL
      )
        : relesActivosAlto(relesActivosAlto_),
          sentidoAltoEsHorario(sentidoAltoEsHorario_),
          sensorEnclavamientoActivoBajo(sensorEnclavamientoActivoBajo_),
          sensorVia1ActivoBajo(sensorVia1ActivoBajo_),
          sensorContadorExternoActivoBajo(sensorContadorExternoActivoBajo_),
          velocidadPWM(velocidadPWM_) {
    }
};


    explicit Hardware(const Configuracion& configuracion);

    void iniciar();
    void actualizar();

    // Movimiento físico elemental: exactamente un paso de leva.
    // La futura clase PlataformaGiratoria encadenará estos movimientos.
    bool iniciarPaso(SentidoGiro sentido);
    void parar();

    bool estaOcupado() const;
    EstadoPasoHardware obtenerEstadoPaso() const;

    // Devuelve y borra el resultado pendiente del último movimiento.
    ResultadoPasoHardware obtenerResultadoPaso();

    // Control directo para pruebas y para futuras capas.
    void establecerSentido(SentidoGiro sentido);
    void establecerVelocidadPWM(uint8_t velocidad);
    uint8_t obtenerVelocidadPWM() const;

    void establecerPolaridadInvertida(bool invertida);
    bool obtenerPolaridadInvertida() const;

    void establecerIndicadorDestino(bool activo);

    // Lecturas de sensores sin acceso directo de otros módulos a los GPIO.
    bool puenteEnclavado() const;
    bool puenteEnMovimiento() const;
    bool sensorVia1Activo() const;

    // Modo de contador de posiciones (interno=leva / externo=D4).
    void establecerModoContador(ModoContador modo);
    ModoContador obtenerModoContador() const;

    // Devuelve true cuando se detecta el final de un paso,
    // usando el contador configurado (leva o pulso externo).
    bool finDePasoDetectado();

    // Eventos de flanco para uso futuro.
    bool consumirEventoContadorExterno();
    bool consumirEventoEnclavamiento();

private:
    Configuracion _configuracion;

    EstadoPasoHardware _estado;
    ResultadoPasoHardware _resultadoPendiente;

    SentidoGiro _sentidoActual;
    unsigned long _instanteEstado;
    bool _movimientoDetectado;

    bool _haDetectadoMovimiento; //Controlando el puente


    ModoContador _modoContador;
    bool _polaridadInvertida;

    bool _ultimoSensorEnclavamiento;
    bool _ultimoSensorContadorExterno;
    bool _eventoEnclavamiento;
    bool _eventoContadorExterno;

    void escribirRele(uint8_t pin, bool activo);
    bool leerEntradaActiva(uint8_t pin, bool activoBajo) const;

    void activarMotor(bool activar);
    void activarDesenclavamiento(bool activar);

    void finalizarPasoCorrectamente();
    void finalizarPasoError(EstadoPasoHardware estadoError,
                            ResultadoPasoHardware resultadoError);
};
