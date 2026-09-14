#include "LocoNetTT9152.h"

#include "Constantes.h"
#include "IndexacionVias.h"
#include "PlataformaGiratoria.h"

// -----------------------------------------------------------------------------
// Constructor: inicializa el módulo LocoNet con la referencia a la plataforma
// y la indexación de vías. La dirección base y el sensor global se configuran
// con sus valores por defecto definidos en Constantes.h.
// -----------------------------------------------------------------------------
LocoNetTT9152::LocoNetTT9152(
    PlataformaGiratoria& plataforma,
    const IndexacionVias& indexacion
)
    : _plataforma(plataforma),
      _indexacion(indexacion),
      _sensorGlobalActivo(false),
      _sensorGlobal(0),
      _lnBase(LOCONET_DIRECCION_INICIAL),
      _ultimaViaNotificada(0),
      _ocupacionNotificada(false),
      _sentidoBase(SentidoGiro::Horario) {
}

// -----------------------------------------------------------------------------
// iniciar: inicializa la librería mrrwa/LocoNet.
// En Arduino Nano los pines usados son:
//   - D8 (ICP1): entrada RX del bus LocoNet.
//   - D7: salida TX del bus LocoNet.
// Debe llamarse una vez desde setup().
// -----------------------------------------------------------------------------
void LocoNetTT9152::iniciar() {
    LocoNet.init(7);
}

// -----------------------------------------------------------------------------
// actualizar: procesa los mensajes LocoNet pendientes y actualiza el estado
// de los sensores de ocupación. Debe llamarse desde loop() en cada iteración.
// Se procesa como máximo un paquete por llamada para no bloquear la
// supervisión del motor ni los sensores.
// -----------------------------------------------------------------------------
void LocoNetTT9152::actualizar() {
    if (!_plataforma.estaOcupada()) {
        actualizarSensoresOcupacion();
    }

    lnMsg* paquete = LocoNet.receive();

    if (paquete == nullptr) {
        return;
    }

    procesarPaquete(paquete);
}

// -----------------------------------------------------------------------------
// procesarPaquete: examina un paquete LocoNet recibido. Solo procesa
// paquetes OPC_SW_REQ (solicitud de cambio de desvío). Extrae la dirección,
// el estado (rojo/verde) y si la salida está activa, y delega en
// procesarSolicitudDesvio().
// Devuelve false si el paquete no es relevante.
// -----------------------------------------------------------------------------
bool LocoNetTT9152::procesarPaquete(lnMsg* paquete) {
    if (paquete == nullptr) {
        return false;
    }

    if (paquete->data[0] != OPC_SW_REQ) {
        return false;
    }

    // Dirección LocoNet: sw1 (bits 0..6) + sw2 (bits 0..3 como parte alta).
    // Se suma 1 porque LocoNet codifica desde 0.
    const uint16_t direccion =
        static_cast<uint16_t>(
            paquete->srq.sw1 |
            ((paquete->srq.sw2 & 0x0F) << 7)
        ) + 1;

    // Bit 5 de sw2: 0=rojo/recto, 1=verde/desviado
    const bool verde = (paquete->srq.sw2 & 0x20) != 0;

    // Bit 4 de sw2: 1=activación, 0=desactivación del pulso.
    // Solo se procesan las activaciones para evitar órdenes duplicadas.
    const bool salidaActiva = (paquete->srq.sw2 & 0x10) != 0;

    return procesarSolicitudDesvio(direccion, verde, salidaActiva);
}

// -----------------------------------------------------------------------------
// procesarSolicitudDesvio: interpreta una solicitud de desvío dentro del
// rango de direcciones del decodificador (base..base+15).
// Tabla de funciones por offset:
//   +0 Rojo:  Parar
//   +1 Rojo:  Reanudar última orden (Resume)
//   +1 Verde: Giro 180° en el sentido base
//   +2 Rojo:  Siguiente vía horario
//   +2 Verde: Siguiente vía antihorario
//   +3 Rojo:  Sentido base = Horario
//   +3 Verde: Sentido base = Antihorario
//   +4..+15:  Ir a vía 1..24 (rojo=impar, verde=par)
// Devuelve false si la dirección está fuera del rango o la acción no aplica.
// -----------------------------------------------------------------------------
bool LocoNetTT9152::procesarSolicitudDesvio(
    uint16_t direccion,
    bool verde,
    bool salidaActiva
) {
    if (!salidaActiva) {
        return false;
    }

    const uint16_t base = _lnBase;
    const uint16_t direccionFinal = static_cast<uint16_t>(base + 16 - 1);

    if (direccion < base || direccion > direccionFinal) {
        return false;
    }

    const uint8_t offset = static_cast<uint8_t>(direccion - base);

    if (offset == 0) {
        if (verde) return false;
        _plataforma.parar();
        return true;
    }

    if (offset == 1) {
        if (!verde) {
            return _plataforma.resumeUltimaOrden(_indexacion);
        }
        return _plataforma.moverPasos(PASOS_GIRO_180, _sentidoBase);
    }

    if (offset == 2) {
        const SentidoGiro sentido =
            verde ? SentidoGiro::Antihorario : SentidoGiro::Horario;
        return _plataforma.moverSiguiente(sentido, _indexacion);
    }

    if (offset == 3) {
        _sentidoBase = verde ? SentidoGiro::Antihorario : SentidoGiro::Horario;
        return true;
    }

    // Offsets 4..15 → vías 1..24 (n=0..11, rojo=impar, verde=par)
    const uint8_t n = offset - 4;
    const uint8_t via = static_cast<uint8_t>(n * 2 + (verde ? 2 : 1));

    return ordenarMovimientoAVia(via);
}

// -----------------------------------------------------------------------------
// ordenarMovimientoAVia: ejecuta el movimiento a la vía lógica indicada,
// equivalente al comando serie 'ir <via>'. Antes de mover libera los
// sensores de ocupación actuales. Devuelve false si la vía no está
// configurada, el puente está en movimiento o la referencia no está fijada.
// -----------------------------------------------------------------------------
bool LocoNetTT9152::ordenarMovimientoAVia(uint8_t via) {
    if (!_plataforma.via1Configurada()) {
        return false;
    }

    if (_plataforma.estaOcupada()) {
        return false;
    }

    liberarSensoresOcupacion();

    if (!_indexacion.viaConfigurada(via)) {
        return false;
    }

    const uint8_t posicionDestino = _indexacion.posicionDeVia(via);

    if (posicionDestino == 0) {
        return false;
    }

    // irAViaSalida() elige automáticamente el extremo más cercano.
    return _plataforma.irAViaSalida(posicionDestino);
}

// -----------------------------------------------------------------------------
// notificarSensor: envía una notificación de ocupado/libre al bus LocoNet
// usando LocoNet.reportSensor(). Si la dirección es 0 o SENSOR_SIN_ASIGNAR,
// no envía nada.
// -----------------------------------------------------------------------------
void LocoNetTT9152::notificarSensor(uint16_t direccionSensor, bool ocupado) {
    if (direccionSensor == 0 ||
        direccionSensor == IndexacionVias::SENSOR_SIN_ASIGNAR) {
        return;
    }

    LocoNet.reportSensor(
        direccionSensor,
        static_cast<uint8_t>(ocupado ? 1 : 0)
    );
}

// -----------------------------------------------------------------------------
// actualizarSensoresOcupacion: monitoriza la posición del puente y emite
// notificaciones de ocupación/liberación cuando cambia la vía en la que
// se encuentra. Activa el sensor global y el sensor de la vía actual al
// enclavarse, y los libera cuando el puente comienza a moverse.
// Solo actúa cuando el puente está enclavado y la referencia está fijada.
// -----------------------------------------------------------------------------
void LocoNetTT9152::actualizarSensoresOcupacion() {
    if (!_plataforma.via1Configurada()) {
        return;
    }

    if (_plataforma.estaOcupada()) {
        return;
    }

    const uint8_t posicionActual = _plataforma.obtenerPosicionFisica();
    const uint8_t viaActual =
        _indexacion.viaAccesibleEnPosicion(posicionActual);

    if (!_ocupacionNotificada) {
        // Primera notificación tras enclavarse: activar sensores
        if (_sensorGlobalActivo) {
            notificarSensor(_sensorGlobal, true);
        }

        if (viaActual != 0) {
            notificarSensor(_indexacion.sensorDeVia(viaActual), true);
            _ultimaViaNotificada = viaActual;
        }

        _ocupacionNotificada = true;
        return;
    }

    // Si el puente ha cambiado de vía (por ejemplo tras un Resume),
    // liberar la vía anterior y activar la nueva
    if (viaActual != _ultimaViaNotificada) {
        if (_ultimaViaNotificada != 0) {
            notificarSensor(_indexacion.sensorDeVia(_ultimaViaNotificada), false);
        }

        if (viaActual != 0) {
            notificarSensor(_indexacion.sensorDeVia(viaActual), true);
        }

        _ultimaViaNotificada = viaActual;
    }
}

// -----------------------------------------------------------------------------
// liberarSensoresOcupacion: desactiva inmediatamente todos los sensores de
// ocupación activos (global y de la última vía notificada). Se llama antes
// de iniciar un movimiento nuevo para que el software de gestión sepa que
// el puente ha salido de esa vía.
// -----------------------------------------------------------------------------
void LocoNetTT9152::liberarSensoresOcupacion() {
    if (_ocupacionNotificada) {
        if (_sensorGlobalActivo) {
            notificarSensor(_sensorGlobal, false);
        }

        if (_ultimaViaNotificada != 0) {
            notificarSensor(_indexacion.sensorDeVia(_ultimaViaNotificada), false);
        }

        _ocupacionNotificada = false;
        _ultimaViaNotificada = 0;
    }
}

// -----------------------------------------------------------------------------
// configurarDireccionBase: establece la primera dirección LocoNet del bloque
// de 16 desvíos asignados al decodificador. Rango válido: 1..2047.
// -----------------------------------------------------------------------------
void LocoNetTT9152::configurarDireccionBase(uint16_t base) {
    if (base > 0 && base <= 2047) {
        _lnBase = base;
    }
}

// -----------------------------------------------------------------------------
// obtenerDireccionBase: devuelve la dirección base LocoNet configurada.
// -----------------------------------------------------------------------------
uint16_t LocoNetTT9152::obtenerDireccionBase() const {
    return _lnBase;
}

// -----------------------------------------------------------------------------
// configurarSensorGlobal: establece la dirección del sensor LocoNet global
// de ocupación de la plataforma. Si la dirección es 0 (SENSOR_SIN_ASIGNAR),
// el sensor global queda desactivado.
// -----------------------------------------------------------------------------
void LocoNetTT9152::configurarSensorGlobal(uint16_t sensor) {
    _sensorGlobal = sensor;
    _sensorGlobalActivo = (sensor != IndexacionVias::SENSOR_SIN_ASIGNAR);
}

// -----------------------------------------------------------------------------
// obtenerSensorGlobal: devuelve la dirección del sensor global configurado.
// Devuelve 0 si el sensor global está desactivado.
// -----------------------------------------------------------------------------
uint16_t LocoNetTT9152::obtenerSensorGlobal() const {
    return _sensorGlobal;
}
