#pragma once

#include <Arduino.h>
#include <LocoNet.h>

#include "Tipos.h"

class PlataformaGiratoria;
class IndexacionVias;

// Gestiona las órdenes LocoNet de desvíos destinadas a la plataforma.
//
// Tabla inicial:
//   229 rojo  -> vía 1
//   229 verde -> vía 2
//   230 rojo  -> vía 3
//   230 verde -> vía 4
//   ...
//   240 rojo  -> vía 23
//   240 verde -> vía 24
class LocoNetTT9152 {
public:
    LocoNetTT9152(
        PlataformaGiratoria& plataforma,
        const IndexacionVias& indexacion
    );

    // Inicializa la librería LocoNet.
    // En Arduino Nano:
    //   D8: RX LocoNet / ICP1
    //   D7: TX LocoNet
    void iniciar();

    // Debe llamarse continuamente desde loop().
    // Procesa como máximo un paquete por llamada para no bloquear
    // la gestión del movimiento de la plataforma.
    void actualizar();

private:
    PlataformaGiratoria& _plataforma;
    const IndexacionVias& _indexacion;

    bool _sensorGlobalActivo;
    uint16_t _sensorGlobal;
    uint16_t _lnBase;   // Dirección base de desvíos LocoNet (configurable)

    uint8_t _ultimaViaNotificada;
    bool _ocupacionNotificada;

    // offset base+3: establece el sentido "base" para operaciones dependientes.
    SentidoGiro _sentidoBase;

    bool procesarPaquete(lnMsg* paquete);

    bool procesarSolicitudDesvio(
        uint16_t direccion,
        bool verde,
        bool salidaActiva
    );

    bool ordenarMovimientoAVia(uint8_t via);

    void notificarSensor(uint16_t direccionSensor, bool ocupado);
    void actualizarSensoresOcupacion();
    void liberarSensoresOcupacion();

public:
    // Configura el sensor global de ocupación de la plataforma.
    // sensor = 0 (SENSOR_SIN_ASIGNAR) deshabilita el sensor global.
    void configurarSensorGlobal(uint16_t sensor);

    // Devuelve el sensor global configurado actualmente.
    uint16_t obtenerSensorGlobal() const;

    // Configura la dirección base de desvíos LocoNet (1..2047).
    // Por defecto LOCONET_DIRECCION_INICIAL (225).
    void configurarDireccionBase(uint16_t base);

    // Devuelve la dirección base actualmente configurada.
    uint16_t obtenerDireccionBase() const;
};
