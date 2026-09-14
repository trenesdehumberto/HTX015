#pragma once

#include <Arduino.h>

#include "Hardware.h"
#include "Constantes.h"
#include "IndexacionVias.h"

class IndexacionVias;

class PlataformaGiratoria {
public:
    explicit PlataformaGiratoria(Hardware& hardware);

    void iniciar();
    void actualizar();

    // Movimiento no bloqueante de 1 a NUM_POSICIONES pasos.
    bool moverPasos(uint8_t pasos, SentidoGiro sentido);
    bool moverUnPaso(SentidoGiro sentido);

    // Realiza un paso mecánico en modo normal o avanza hasta la
    // siguiente orientación con vía existente en modo indexado.
    bool moverSiguiente(
        SentidoGiro sentido,
        const IndexacionVias& indexacion
    );


    void parar();

    // Reanuda la última orden compatible con "Parar + Resume"
    // recibida por LocoNet/menú.
    //
    // Se requiere IndexacionVias para reanudar órdenes basadas en "siguiente vía indexada".
    bool resumeUltimaOrden(const IndexacionVias& indexacion);

    bool estaOcupada() const;

    // Configura la posición actual enclavada como vía física 1.
    bool configurarVia1Aqui();

    bool via1Configurada() const;
    uint8_t obtenerPosicionFisica() const;

    // Persistencia/restauración de estado
    void restaurarEstado(bool via1Configurada, uint8_t posicionFisica);

    // Restaura el modo de contador desde la EEPROM.
    void restaurarModoContador(ModoContador modo);

    // Devuelve el modo de contador actualmente configurado en Hardware.
    ModoContador obtenerModoContador() const;

    // Stubs internos para futuras notificaciones de sensores LocoNet.
    // La lógica real de sensores vive en LocoNetTT9152.
    void activarSensores();
    void desactivarSensores();

    // Devuelve y borra el resultado final de una orden de movimiento.
    ResultadoPasoHardware obtenerResultadoMovimiento();

    // Va a una vía de salida. Puede utilizar cualquiera de los dos extremos
    // accesibles del puente y escoge la ruta global más corta.
    bool irAViaSalida(uint8_t via);

    // Va exactamente a la posición indicada para el extremo de referencia.
    // Uso de prueba, calibración o mantenimiento.
    bool irAPosicionFisicaExacta(uint8_t destino);



private:
    enum class TipoOrdenResume : uint8_t {
        Ninguna = 0,
        Pasos,
        SiguienteViaIndexada
    };

    Hardware& _hardware;

    // Posición interna: 0..47.
    // Al mostrarla se convierte a 1..48.
    bool _via1Configurada;
    uint8_t _posicionActual;

    bool _movimientoActivo;
    uint8_t _pasosRestantes;
    SentidoGiro _sentidoMovimiento;

    ResultadoPasoHardware _resultadoPendiente;

    // ---- Resume (Parar + Resume) ----
    TipoOrdenResume _tipoOrdenResume;
    uint8_t _pasosResume;
    SentidoGiro _sentidoResume;

    // ---- Cambio automático de polaridad ----
    bool _cambioPolaridadPendiente;
    bool _nuevoEstadoPolaridad;
    bool _movimientoDetectadoParaPolaridad;
    uint32_t _instanteInicioMovimiento;

    bool iniciarSiguientePaso();
    void actualizarPosicionTrasPasoCompletado();

    // Prepara el cambio si el siguiente paso cruza 12-13 o 36-37.
    void prepararCambioPolaridadParaSiguientePaso();

    // Ejecuta el cambio 500 ms después de detectar movimiento real.
    void actualizarCambioPolaridad();

    bool calcularRutaExacta(
        uint8_t destino,
        uint8_t& pasos,
        SentidoGiro& sentido
    ) const;

    static uint8_t posicionOpuesta(uint8_t posicion);

};
