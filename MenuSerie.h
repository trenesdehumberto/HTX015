#pragma once

#include <Arduino.h>

#include "Hardware.h"
#include "PlataformaGiratoria.h"
#include "IndexacionVias.h"
#include "Constantes.h"
#include "EepromTT9152.h"

class LocoNetTT9152;

class MenuSerie {
public:
    MenuSerie(
        Hardware& hardware,
        PlataformaGiratoria& plataforma,
        IndexacionVias& indexacion,
        LocoNetTT9152* loconet = nullptr
    );

    void iniciar();
    void actualizar();

private:
    Hardware& _hardware;
    PlataformaGiratoria& _plataforma;
    IndexacionVias& _indexacion;

    LocoNetTT9152* _loconet;

    char _buffer[TAMANO_BUFFER_SERIE];
    uint8_t _posicionBuffer;

    void procesarLinea(char* linea);

    void mostrarAyuda() const;
    void mostrarEstado() const;
    void mostrarViasConfiguradas() const;

    void imprimirResultadoMovimiento(ResultadoPasoHardware resultado) const;

    bool extraerPosicion(
        const char* texto,
        uint8_t& posicion
    ) const;

    bool extraerViaYPosicion(
        const char* texto,
        uint8_t& via,
        uint8_t& posicion
    ) const;

    bool extraerViaPosicionYSensor(
        const char* texto,
        uint8_t& via,
        uint8_t& posicion,
        uint16_t& sensor
    ) const;

    // Guarda la configuración actual en EEPROM.
    void guardarEEPROM() const;

    // Procesa comandos recibidos del ESP32 por UART (prefijo CMD: ya eliminado).
    void procesarComandoESP32(const char* cmd);

    // Emite una línea de estado hacia el ESP32 por UART.
    void emitirEstadoESP32() const;

    // Emite la configuración completa (vías, sensores, modos) hacia el ESP32.
    // Formato multilinea:
    //   CFG:META:V1:<0|1>:POS:<p>:MODO:<m>:MC:<mc>:SG:<sg>:LN:<ln>:N:<n>
    //   CFG:VIA:<id>:<pos>:<sensor>   (una por cada vía configurada)
    //   CFG:END
    void emitirConfigCompleta() const;

    // Exporta la configuración como una línea IMPORT: lista para copiar/pegar.
    void exportarConfiguracion() const;

    // Importa configuración desde una cadena en formato IMPORT: (sin el prefijo).
    // Aplica sensores, modos, vías y dirección LN base.
    // La referencia de vía 1 (referenciaaqui) siempre es manual.
    void importarConfiguracion(char* datos);
};
