// =============================================================================
// TT9152.ino — Programa principal del decodificador de plataforma giratoria
//
// Inicializa todos los módulos del sistema y los actualiza en cada ciclo.
// La configuración se carga desde la EEPROM al arrancar para restaurar
// el estado guardado anteriormente (posición, vías, modo, sensores).
// =============================================================================

#include "Constantes.h"
#include "Hardware.h"
#include "PlataformaGiratoria.h"
#include "IndexacionVias.h"
#include "MenuSerie.h"
#include "LocoNetTT9152.h"
#include "EepromTT9152.h"

// Estructura de configuración de pines y parámetros hardware
Hardware::Configuracion configuracionHardware;

// Instancias globales de los módulos del sistema
Hardware hardware(configuracionHardware);
PlataformaGiratoria plataforma(hardware);
IndexacionVias indexacion;
LocoNetTT9152 loconet(plataforma, indexacion);
MenuSerie menuSerie(hardware, plataforma, indexacion, &loconet);

// -----------------------------------------------------------------------------
// setup: se ejecuta una sola vez al arrancar el decodificador.
// Inicializa el puerto serie, el hardware, LocoNet y el menú.
// Después carga la configuración guardada en la EEPROM; si no hay datos
// válidos, reinicia la EEPROM con los valores de fábrica.
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(VELOCIDAD_SERIE);

    hardware.iniciar();
    plataforma.iniciar();
    loconet.iniciar();
    menuSerie.iniciar();

    EepromDatosTT9152 datos;

    if (EepromTT9152::cargar(datos)) {
        // Restaurar la posición física del puente y si la referencia está fijada
        plataforma.restaurarEstado(datos.via1configurada, datos.posicionActualPuente);

        // Construir la estructura de configuración de vías e indexación
        ConfiguracionIndexacion cfg;
        cfg.version = IndexacionVias::VERSION_CONFIGURACION;
        cfg.modo = static_cast<ModoRecorrido>(datos.modoRecorrido);

        for (uint8_t i = 0; i < 24; ++i) {
            cfg.posicionPorVia[i] = datos.posicionPorVia[i];
            cfg.sensorPorVia[i] = datos.sensorPorVia[i];
        }

        // Cargar la configuración de vías validándola antes de aplicarla
        indexacion.cargarConfiguracion(cfg);

        // Restaurar el sensor global de ocupación de LocoNet
        loconet.configurarSensorGlobal(datos.sensorGlobal);

        // Restaurar la dirección base LocoNet (guardada en los dos primeros
        // bytes del campo reservedWeb de la EEPROM)
        {
            uint16_t lnBase =
                static_cast<uint16_t>(datos.reservedWeb[0]) |
                (static_cast<uint16_t>(datos.reservedWeb[1]) << 8);
            if (lnBase == 0 || lnBase == 0xFFFFU) {
                lnBase = LOCONET_DIRECCION_INICIAL;
            }
            loconet.configurarDireccionBase(lnBase);
        }

        // Restaurar el modo de contador de posición (leva interna o pulso externo)
        plataforma.restaurarModoContador(
            static_cast<ModoContador>(datos.modoContador)
        );

        Serial.println(F("[EEPROM] Configuracion restaurada."));
    } else {
        // No hay datos válidos: reiniciar a valores de fábrica
        EepromTT9152::reiniciar();
        Serial.println(F("[EEPROM] Sin datos validos. Configuracion de fabrica."));
    }
}

// -----------------------------------------------------------------------------
// loop: se ejecuta continuamente durante el funcionamiento normal.
// Cada módulo procesa sus tareas pendientes en orden:
//   1. Hardware:  lee sensores y gestiona el relé de polaridad.
//   2. Plataforma: avanza pasos físicos y detecta enclavamientos.
//   3. LocoNet:   procesa mensajes del bus y actualiza sensores de ocupación.
//   4. MenuSerie: atiende comandos del usuario por el puerto serie.
// -----------------------------------------------------------------------------
void loop() {
    hardware.actualizar();
    plataforma.actualizar();
    loconet.actualizar();
    menuSerie.actualizar();
}
