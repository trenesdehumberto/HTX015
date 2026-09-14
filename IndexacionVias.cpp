#include "IndexacionVias.h"

#include <string.h>

// -----------------------------------------------------------------------------
// Constructor: inicializa la configuración llamando a restablecer(),
// que pone todas las vías como no asignadas y el modo en Normal.
// -----------------------------------------------------------------------------
IndexacionVias::IndexacionVias() {
    restablecer();
}

// -----------------------------------------------------------------------------
// restablecer: borra toda la configuración de vías y sensores, y establece
// la versión y el modo Normal. Equivale a un reset de fábrica de la indexación.
// -----------------------------------------------------------------------------
void IndexacionVias::restablecer() {
    _configuracion.version = VERSION_CONFIGURACION;
    _configuracion.modo = ModoRecorrido::Normal;

    memset(
        _configuracion.posicionPorVia,
        POSICION_SIN_ASIGNAR,
        sizeof(_configuracion.posicionPorVia)
    );

    memset(
        _configuracion.sensorPorVia,
        0,
        sizeof(_configuracion.sensorPorVia)
    );
}

// -----------------------------------------------------------------------------
// ponerModo: establece el modo de recorrido activo (Normal o Indexado).
// En modo Normal, PH/PA mueven una posición mecánica.
// En modo Indexado, PH/PA saltan a la siguiente vía configurada.
// -----------------------------------------------------------------------------
void IndexacionVias::ponerModo(ModoRecorrido modo) {
    if (modo == ModoRecorrido::Normal ||
        modo == ModoRecorrido::Indexado) {
        _configuracion.modo = modo;
    }
}

// -----------------------------------------------------------------------------
// obtenerModo: devuelve el modo de recorrido activo.
// -----------------------------------------------------------------------------
ModoRecorrido IndexacionVias::obtenerModo() const {
    return _configuracion.modo;
}

// -----------------------------------------------------------------------------
// esModoIndexado: devuelve true si el modo activo es Indexado.
// -----------------------------------------------------------------------------
bool IndexacionVias::esModoIndexado() const {
    return _configuracion.modo == ModoRecorrido::Indexado;
}

// -----------------------------------------------------------------------------
// configurarVia1: asigna la vía lógica 1 a la posición física 1.
// Se llama tras ejecutar el comando 'referenciaaqui'.
// -----------------------------------------------------------------------------
void IndexacionVias::configurarVia1() {
    _configuracion.posicionPorVia[0] = 1;
    _configuracion.sensorPorVia[0] = SENSOR_SIN_ASIGNAR;
}

// -----------------------------------------------------------------------------
// asignarVia: asigna una vía lógica a una posición física.
// Valida que:
//   - La vía 1 solo puede estar en posición 1.
//   - Ninguna vía puede ocupar la posición 1 salvo la vía 1.
//   - La posición no esté ya ocupada por otra vía.
//   - La posición sea mayor que la de la vía anterior (orden creciente).
// Devuelve false si alguna validación falla.
// -----------------------------------------------------------------------------
bool IndexacionVias::asignarVia(
    uint8_t via,
    uint8_t posicion
) {
    if (!viaValida(via) || !posicionValida(posicion)) {
        return false;
    }

    if (via == 1 && posicion != 1) {
        return false;
    }

    if (via != 1 && posicion == 1) {
        return false;
    }

    if (!posicionDisponibleParaVia(via, posicion)) {
        return false;
    }

    // Las vías >= 2 deben tener posición mayor que la vía anterior
    if (via >= 2) {
        const uint8_t posAnt = posicionDeVia(via - 1);
        if (posAnt != POSICION_SIN_ASIGNAR && posicion <= posAnt) {
            return false;
        }
    }

    _configuracion.posicionPorVia[via - 1] = posicion;
    return true;
}

// -----------------------------------------------------------------------------
// eliminarVia: elimina una vía lógica (no se puede eliminar la vía 1).
// Desplaza todas las vías posteriores una posición hacia abajo para
// mantener la numeración consecutiva.
// Devuelve false si la vía no existe o es la vía 1.
// -----------------------------------------------------------------------------
bool IndexacionVias::eliminarVia(uint8_t via) {
    if (!viaValida(via) || via == 1) {
        return false;
    }

    if (!viaConfigurada(via)) {
        return false;
    }

    for (uint8_t v = via; v < VIA_MAXIMA; ++v) {
        _configuracion.posicionPorVia[v - 1] =
            _configuracion.posicionPorVia[v];
        _configuracion.sensorPorVia[v - 1] =
            _configuracion.sensorPorVia[v];
    }

    _configuracion.posicionPorVia[VIA_MAXIMA - 1] =
        POSICION_SIN_ASIGNAR;
    _configuracion.sensorPorVia[VIA_MAXIMA - 1] =
        SENSOR_SIN_ASIGNAR;

    return true;
}

// -----------------------------------------------------------------------------
// insertarVia: inserta una nueva vía en la posición lógica indicada,
// desplazando hacia arriba todas las vías existentes desde ese índice.
// Valida que la posición respete el orden creciente respecto a las vías
// adyacentes: posición(via-1) < nueva_posicion < posición(via_actual).
// Devuelve false si alguna validación falla.
// -----------------------------------------------------------------------------
bool IndexacionVias::insertarVia(
    uint8_t via,
    uint8_t posicion,
    uint16_t sensor
) {
    if (!viaValida(via) || via == 1) {
        return false;
    }

    if (!posicionValida(posicion)) {
        return false;
    }

    if (via > 2 && !viaConfigurada(via - 1)) {
        return false;
    }

    if (via == 2 && !viaConfigurada(1)) {
        return false;
    }

    if (viaConfigurada(VIA_MAXIMA)) {
        return false;
    }

    if (!posicionDisponibleParaVia(via, posicion)) {
        return false;
    }

    // La nueva posición debe ser mayor que la de la vía anterior
    const uint8_t posAnterior = posicionDeVia(via - 1);
    if (posAnterior != POSICION_SIN_ASIGNAR && posicion <= posAnterior) {
        return false;
    }

    // La nueva posición debe ser menor que la de la vía que actualmente
    // ocupa ese slot (que pasará a ser via+1 tras el desplazamiento)
    if (viaConfigurada(via)) {
        const uint8_t posSig = posicionDeVia(via);
        if (posSig != POSICION_SIN_ASIGNAR && posicion >= posSig) {
            return false;
        }
    }

    // Desplazar vías existentes hacia arriba para hacer hueco
    for (uint8_t v = VIA_MAXIMA - 1; v >= via; --v) {
        _configuracion.posicionPorVia[v] =
            _configuracion.posicionPorVia[v - 1];
        _configuracion.sensorPorVia[v] =
            _configuracion.sensorPorVia[v - 1];

        if (v == via) break; // evitar desbordamiento en uint8_t
    }

    _configuracion.posicionPorVia[via - 1] = posicion;
    _configuracion.sensorPorVia[via - 1] = sensor;

    return true;
}

// -----------------------------------------------------------------------------
// siguienteViaLibre: devuelve el número de la siguiente vía lógica no
// configurada (la primera sin posición asignada).
// Devuelve 0 si todas las vías (1..24) están ocupadas.
// -----------------------------------------------------------------------------
uint8_t IndexacionVias::siguienteViaLibre() const {
    for (uint8_t via = VIA_MINIMA; via <= VIA_MAXIMA; ++via) {
        if (!viaConfigurada(via)) {
            return via;
        }
    }
    return 0;
}

// -----------------------------------------------------------------------------
// borrarViasExceptoVia1: elimina todas las vías de salida (2..24)
// conservando la vía 1. Se usa al ejecutar el comando 'via clear'.
// -----------------------------------------------------------------------------
void IndexacionVias::borrarViasExceptoVia1() {
    for (uint8_t indice = 1; indice < NUMERO_VIAS_LOGICAS; ++indice) {
        _configuracion.posicionPorVia[indice] = POSICION_SIN_ASIGNAR;
        _configuracion.sensorPorVia[indice] = SENSOR_SIN_ASIGNAR;
    }
}

// -----------------------------------------------------------------------------
// viaConfigurada: devuelve true si la vía lógica indicada tiene una
// posición física asignada (distinta de POSICION_SIN_ASIGNAR).
// -----------------------------------------------------------------------------
bool IndexacionVias::viaConfigurada(uint8_t via) const {
    return posicionDeVia(via) != POSICION_SIN_ASIGNAR;
}

// -----------------------------------------------------------------------------
// sensorDeVia: devuelve el número del sensor LocoNet asignado a la vía.
// Devuelve SENSOR_SIN_ASIGNAR (0) si la vía no es válida o no tiene sensor.
// -----------------------------------------------------------------------------
uint16_t IndexacionVias::sensorDeVia(uint8_t via) const {
    if (!viaValida(via)) {
        return SENSOR_SIN_ASIGNAR;
    }
    return _configuracion.sensorPorVia[via - 1];
}

// -----------------------------------------------------------------------------
// asignarSensorVia: asigna un número de sensor LocoNet a una vía lógica.
// Devuelve false si la vía no es válida.
// -----------------------------------------------------------------------------
bool IndexacionVias::asignarSensorVia(uint8_t via, uint16_t sensor) {
    if (!viaValida(via)) {
        return false;
    }
    _configuracion.sensorPorVia[via - 1] = sensor;
    return true;
}

// -----------------------------------------------------------------------------
// posicionDeVia: devuelve la posición física (1..48) de la vía lógica.
// Devuelve POSICION_SIN_ASIGNAR (0) si la vía no existe o no está configurada.
// -----------------------------------------------------------------------------
uint8_t IndexacionVias::posicionDeVia(uint8_t via) const {
    if (!viaValida(via)) {
        return POSICION_SIN_ASIGNAR;
    }
    return _configuracion.posicionPorVia[via - 1];
}

// -----------------------------------------------------------------------------
// viaEnPosicionFisica: busca qué vía lógica está configurada exactamente
// en la posición física indicada. No considera la posición opuesta.
// Devuelve 0 si ninguna vía ocupa esa posición.
// -----------------------------------------------------------------------------
uint8_t IndexacionVias::viaEnPosicionFisica(uint8_t posicion) const {
    if (!posicionValida(posicion)) {
        return 0;
    }

    for (uint8_t via = VIA_MINIMA; via <= VIA_MAXIMA; ++via) {
        if (posicionDeVia(via) == posicion) {
            return via;
        }
    }
    return 0;
}

// -----------------------------------------------------------------------------
// viaAccesibleEnPosicion: busca qué vía es accesible desde una orientación
// física del puente, considerando ambos extremos (directo y opuesto a 180°).
// Prioriza el extremo de referencia. Devuelve 0 si no hay vía accesible.
// -----------------------------------------------------------------------------
uint8_t IndexacionVias::viaAccesibleEnPosicion(uint8_t posicion) const {
    if (!posicionValida(posicion)) {
        return 0;
    }

    uint8_t via = viaEnPosicionFisica(posicion);
    if (via != 0) {
        return via;
    }

    return viaEnPosicionFisica(posicionOpuesta(posicion));
}

// -----------------------------------------------------------------------------
// contarVias: devuelve el número total de vías lógicas configuradas (1..24).
// -----------------------------------------------------------------------------
uint8_t IndexacionVias::contarVias() const {
    uint8_t total = 0;
    for (uint8_t via = VIA_MINIMA; via <= VIA_MAXIMA; ++via) {
        if (viaConfigurada(via)) {
            ++total;
        }
    }
    return total;
}

// -----------------------------------------------------------------------------
// hayAlgunaVia: devuelve true si hay al menos una vía configurada.
// -----------------------------------------------------------------------------
bool IndexacionVias::hayAlgunaVia() const {
    return contarVias() > 0;
}

// -----------------------------------------------------------------------------
// siguienteViaConfigurada: busca la siguiente vía configurada a partir de
// viaActual, recorriendo las posiciones físicas en el sentido indicado.
// Usada por PlataformaGiratoria::moverSiguiente() en modo Indexado.
// Devuelve 0 si viaActual no existe o no hay ninguna otra vía configurada.
// -----------------------------------------------------------------------------
uint8_t IndexacionVias::siguienteViaConfigurada(
    uint8_t viaActual,
    SentidoGiro sentido
) const {
    if (!viaValida(viaActual) || !viaConfigurada(viaActual)) {
        return 0;
    }

    uint8_t posicionCandidata = posicionDeVia(viaActual);

    for (uint8_t avance = 1; avance <= NUMERO_POSICIONES; ++avance) {
        posicionCandidata = siguientePosicion(posicionCandidata, sentido);

        const uint8_t viaCandidata = viaEnPosicionFisica(posicionCandidata);
        if (viaCandidata != 0) {
            return viaCandidata;
        }
    }
    return 0;
}

// -----------------------------------------------------------------------------
// posicionOpuesta: calcula la posición física opuesta a 180°.
// Posición 1↔25, 2↔26, ..., 24↔48.
// Devuelve 0 si la posición es inválida.
// -----------------------------------------------------------------------------
uint8_t IndexacionVias::posicionOpuesta(uint8_t posicion) {
    if (!posicionValida(posicion)) {
        return 0;
    }

    if (posicion <= POSICIONES_POR_LADO) {
        return posicion + POSICIONES_POR_LADO;
    }
    return posicion - POSICIONES_POR_LADO;
}

// -----------------------------------------------------------------------------
// esOrientacionIndexadaValida: devuelve true si en la orientación indicada
// por posicionReferencia hay alguna vía accesible (directa u opuesta).
// -----------------------------------------------------------------------------
bool IndexacionVias::esOrientacionIndexadaValida(
    uint8_t posicionReferencia
) const {
    return viaAccesibleEnPosicion(posicionReferencia) != 0;
}

// -----------------------------------------------------------------------------
// obtenerConfiguracion: devuelve la estructura de configuración completa
// para su almacenamiento en EEPROM o para transmisión al ESP32.
// -----------------------------------------------------------------------------
const ConfiguracionIndexacion& IndexacionVias::obtenerConfiguracion() const {
    return _configuracion;
}

// -----------------------------------------------------------------------------
// cargarConfiguracion: valida y carga una configuración externa (por ejemplo
// desde la EEPROM al arrancar). Si la validación falla, la configuración
// actual no se modifica. Devuelve true si la carga fue exitosa.
// -----------------------------------------------------------------------------
bool IndexacionVias::cargarConfiguracion(
    const ConfiguracionIndexacion& configuracion
) {
    if (!configuracionValida(configuracion)) {
        return false;
    }
    _configuracion = configuracion;
    return true;
}

// -----------------------------------------------------------------------------
// viaValida: devuelve true si el número de vía está en el rango válido (1..24).
// -----------------------------------------------------------------------------
bool IndexacionVias::viaValida(uint8_t via) {
    return via >= VIA_MINIMA && via <= VIA_MAXIMA;
}

// -----------------------------------------------------------------------------
// posicionValida: devuelve true si la posición física está en rango (1..48).
// -----------------------------------------------------------------------------
bool IndexacionVias::posicionValida(uint8_t posicion) {
    return posicion >= 1 && posicion <= NUMERO_POSICIONES;
}

// -----------------------------------------------------------------------------
// siguientePosicion: avanza o retrocede una posición en el sentido indicado,
// dando la vuelta completa al llegar a los extremos (1 o NUMERO_POSICIONES).
// -----------------------------------------------------------------------------
uint8_t IndexacionVias::siguientePosicion(uint8_t posicion, SentidoGiro sentido) {
    if (sentido == SentidoGiro::Horario) {
        return (posicion >= NUMERO_POSICIONES) ? 1 : posicion + 1;
    }
    return (posicion <= 1) ? NUMERO_POSICIONES : posicion - 1;
}

// -----------------------------------------------------------------------------
// posicionDisponibleParaVia: comprueba que la posición física no está ya
// ocupada exactamente por otra vía (ignora la posición opuesta a 180°).
// Ignora la asignación previa de la misma vía lógica (permite reasignación).
// -----------------------------------------------------------------------------
bool IndexacionVias::posicionDisponibleParaVia(
    uint8_t via,
    uint8_t posicion
) const {
    for (uint8_t otraVia = VIA_MINIMA; otraVia <= VIA_MAXIMA; ++otraVia) {
        if (otraVia == via) {
            continue;
        }

        const uint8_t posicionOcupada = posicionDeVia(otraVia);

        if (posicionOcupada == POSICION_SIN_ASIGNAR) {
            continue;
        }

        if (posicion == posicionOcupada) {
            return false;
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// configuracionValida: valida la integridad de una estructura de configuración
// antes de cargarla. Comprueba la versión, el modo, que las posiciones sean
// válidas y que no haya duplicados entre vías.
// Devuelve false si encuentra algún problema.
// -----------------------------------------------------------------------------
bool IndexacionVias::configuracionValida(
    const ConfiguracionIndexacion& configuracion
) const {
    if (configuracion.version != VERSION_CONFIGURACION) {
        return false;
    }

    if (configuracion.modo != ModoRecorrido::Normal &&
        configuracion.modo != ModoRecorrido::Indexado) {
        return false;
    }

    for (uint8_t via = VIA_MINIMA; via <= VIA_MAXIMA; ++via) {
        const uint8_t posicion = configuracion.posicionPorVia[via - 1];
        const uint16_t sensor  = configuracion.sensorPorVia[via - 1];

        if (posicion == POSICION_SIN_ASIGNAR) {
            if (sensor != SENSOR_SIN_ASIGNAR) {
                return false;
            }
            continue;
        }

        if (!posicionValida(posicion)) {
            return false;
        }

        if (via == 1 && posicion != 1) {
            return false;
        }

        if (via != 1 && posicion == 1) {
            return false;
        }

        // Verificar que no hay posiciones duplicadas con vías anteriores
        for (uint8_t otraVia = VIA_MINIMA; otraVia < via; ++otraVia) {
            const uint8_t otraPosicion =
                configuracion.posicionPorVia[otraVia - 1];

            if (otraPosicion == POSICION_SIN_ASIGNAR) {
                continue;
            }

            if (posicion == otraPosicion) {
                return false;
            }
        }
    }

    return true;
}
