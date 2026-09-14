#pragma once

#include <Arduino.h>

#include "Constantes.h"
#include "Tipos.h"

enum class ModoRecorrido : uint8_t {
    Normal = 0,
    Indexado = 1
};

// Estructura preparada para su futuro almacenamiento en EEPROM.
//
// posicionPorVia[0]  -> vía lógica 1
// posicionPorVia[1]  -> vía lógica 2
// ...
// posicionPorVia[23] -> vía lógica 24
//
// Valor 0: vía sin configurar.
// Valor 1..48: posición física asignada.
struct ConfiguracionIndexacion {
    uint8_t version;
    ModoRecorrido modo;
    uint8_t posicionPorVia[NUMERO_POSICIONES / 2];
    uint16_t sensorPorVia[NUMERO_POSICIONES / 2];
};

class IndexacionVias {
public:
    static constexpr uint8_t VERSION_CONFIGURACION = 2;

    static constexpr uint8_t NUMERO_VIAS_LOGICAS =
        NUMERO_POSICIONES / 2;

    static constexpr uint8_t POSICIONES_POR_LADO =
        NUMERO_POSICIONES / 2;

    static constexpr uint8_t VIA_MINIMA = 1;
    static constexpr uint8_t VIA_MAXIMA = NUMERO_VIAS_LOGICAS;

    static constexpr uint8_t POSICION_SIN_ASIGNAR = 0;
    static constexpr uint16_t SENSOR_SIN_ASIGNAR = 0;

    IndexacionVias();

    // Configuración general.
    void restablecer();

    void ponerModo(ModoRecorrido modo);
    ModoRecorrido obtenerModo() const;
    bool esModoIndexado() const;

    // Vías lógicas.
    //
    // La vía lógica 1 siempre corresponde a la posición física 1.
    // Debe llamarse tras calibrar la referencia con "referenciaaqui".
    void configurarVia1();

    // Asigna una vía lógica a una posición física.
    //
    // No se permite:
    // - asignar vía 1 a otra posición diferente de 1;
    // - repetir una posición ya usada;
    // - usar una posición opuesta a otra ya asignada.
    bool asignarVia(uint8_t via, uint8_t posicion);

    // Elimina una vía lógica. La vía 1 no puede eliminarse.
    // Las vías posteriores se reordenan para mantener la correlatividad.
    bool eliminarVia(uint8_t via);

    // Inserta una vía en la posición lógica indicada desplazando las
    // vías posteriores (+1). Devuelve false si el número no es válido
    // (anterior no existe o se supera el máximo).
    bool insertarVia(uint8_t via, uint8_t posicion, uint16_t sensor);

    // Elimina las vías 2..24 y conserva la vía 1.
    void borrarViasExceptoVia1();

    // Devuelve el siguiente número de vía libre (consecutivo al último
    // configurado). Devuelve 0 si ya están todas configuradas.
    uint8_t siguienteViaLibre() const;

    bool viaConfigurada(uint8_t via) const;

    // Devuelve la dirección de sensor LocoNet asociada a una vía
    uint16_t sensorDeVia(uint8_t via) const;

    // Asigna al internal la dirección de sensor de la vía (sensor = 0: sin sensor)
    bool asignarSensorVia(uint8_t via, uint16_t sensor);

    // Devuelve la posición física de una vía lógica.
    // Devuelve 0 si la vía no existe o no está configurada.
    uint8_t posicionDeVia(uint8_t via) const;

    // Devuelve la vía asignada exactamente a una posición física.
    // No considera la posición opuesta.
    uint8_t viaEnPosicionFisica(uint8_t posicion) const;

    // Devuelve la vía accesible desde una orientación física del puente.
    // Considera ambos extremos del puente.
    // Devuelve 0 si no hay una vía configurada.
    uint8_t viaAccesibleEnPosicion(uint8_t posicion) const;

    uint8_t contarVias() const;
    bool hayAlgunaVia() const;

    // Busca la siguiente vía configurada en el orden de las posiciones
    // físicas, según el sentido indicado.
    //
    // Devuelve 0 si la vía actual no existe o no hay ninguna configurada.
    uint8_t siguienteViaConfigurada(
        uint8_t viaActual,
        SentidoGiro sentido
    ) const;

    // Posiciones físicas.
    //
    // 1 <-> 25, 2 <-> 26, ..., 24 <-> 48.
    static uint8_t posicionOpuesta(uint8_t posicion);

    // Indica si en esa orientación hay alguna vía accesible por uno
    // de los dos extremos del puente.
    bool esOrientacionIndexadaValida(
        uint8_t posicionReferencia
    ) const;

    // Preparado para EEPROM y futura interfaz web.
    const ConfiguracionIndexacion& obtenerConfiguracion() const;

    // Valida y carga una configuración externa.
    // Si no es válida, no altera la configuración actual.
    bool cargarConfiguracion(
        const ConfiguracionIndexacion& configuracion
    );

private:
    ConfiguracionIndexacion _configuracion;

    static bool viaValida(uint8_t via);
    static bool posicionValida(uint8_t posicion);

    static uint8_t siguientePosicion(
        uint8_t posicion,
        SentidoGiro sentido
    );

    bool posicionDisponibleParaVia(
        uint8_t via,
        uint8_t posicion
    ) const;

    bool configuracionValida(
        const ConfiguracionIndexacion& configuracion
    ) const;
};
