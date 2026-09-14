#include "PlataformaGiratoria.h"
#include "IndexacionVias.h"
#include <LocoNet.h>

namespace {
    enum class TipoOrdenResume : uint8_t {
        Ninguna = 0,
        Pasos,
        SiguienteViaIndexada
    };
}

void PlataformaGiratoria::restaurarEstado(bool via1Configurada, uint8_t posicionFisica) {
    _via1Configurada = via1Configurada;
    _posicionActual = posicionFisica - 1;
}

void PlataformaGiratoria::restaurarModoContador(ModoContador modo) {
    _hardware.establecerModoContador(modo);
}

ModoContador PlataformaGiratoria::obtenerModoContador() const {
    return _hardware.obtenerModoContador();
}

PlataformaGiratoria::PlataformaGiratoria(Hardware& hardware)
    : _hardware(hardware),
      _via1Configurada(false),
      _posicionActual(0),
      _movimientoActivo(false),
      _pasosRestantes(0),
      _sentidoMovimiento(SentidoGiro::Horario),
      _resultadoPendiente(ResultadoPasoHardware::Ninguno),
      _cambioPolaridadPendiente(false),
      _nuevoEstadoPolaridad(false),
      _movimientoDetectadoParaPolaridad(false),
      _instanteInicioMovimiento(0),
      _tipoOrdenResume(TipoOrdenResume::Ninguna),
      _pasosResume(0),
      _sentidoResume(SentidoGiro::Horario) {
}

void PlataformaGiratoria::iniciar() {
    _via1Configurada = false;
    _posicionActual = 0;
    _movimientoActivo = false;
    _pasosRestantes = 0;
    _resultadoPendiente = ResultadoPasoHardware::Ninguno;
    _cambioPolaridadPendiente = false;
    _nuevoEstadoPolaridad = false;
    _movimientoDetectadoParaPolaridad = false;
    _instanteInicioMovimiento = 0;
    _tipoOrdenResume = TipoOrdenResume::Ninguna;
    _pasosResume = 0;
    _sentidoResume = SentidoGiro::Horario;
}

void PlataformaGiratoria::actualizar() {
    // Este bloque debe ejecutarse continuamente, incluso mientras
    // Hardware está realizando un paso físico.
    actualizarCambioPolaridad();

    const ResultadoPasoHardware resultado = _hardware.obtenerResultadoPaso();

    if (resultado == ResultadoPasoHardware::Ninguno) {
        return;
    }

    // Un paso físico ha terminado correctamente.
    if (resultado == ResultadoPasoHardware::Completado) {
        actualizarPosicionTrasPasoCompletado();

        if (_pasosRestantes > 0) {
            _pasosRestantes--;
        }

        // Quedan pasos por realizar: se inicia el siguiente.
        if (_pasosRestantes > 0) {
            if (!iniciarSiguientePaso()) {
                _movimientoActivo = false;
                _pasosRestantes = 0;
                _cambioPolaridadPendiente = false;
                _resultadoPendiente = ResultadoPasoHardware::ErrorInicioMovimiento;
            }
            return;
        }

        // La orden completa ha finalizado.
        _movimientoActivo = false;
        _cambioPolaridadPendiente = false;
        _resultadoPendiente = ResultadoPasoHardware::Completado;

        // Activar sensor global y de la vía actual
        activarSensores();
        
        return;
    }

    // Cancelación o error: se cancela también un posible cambio pendiente.
    _movimientoActivo = false;
    _pasosRestantes = 0;
    _cambioPolaridadPendiente = false;
    _resultadoPendiente = resultado;
}

bool PlataformaGiratoria::moverPasos(uint8_t pasos, SentidoGiro sentido) {
    if (pasos == 0 || pasos > NUMERO_POSICIONES) {
        return false;
    }

    if (estaOcupada()) {
        return false;
    }

    _tipoOrdenResume = TipoOrdenResume::Pasos;
    _pasosResume = pasos;
    _sentidoResume = sentido;

    // Desactivar sensores al iniciar movimiento
    desactivarSensores();

    _resultadoPendiente = ResultadoPasoHardware::Ninguno;
    _movimientoActivo = true;
    _pasosRestantes = pasos;
    _sentidoMovimiento = sentido;

    if (!iniciarSiguientePaso()) {
        _movimientoActivo = false;
        _pasosRestantes = 0;
        _resultadoPendiente = ResultadoPasoHardware::ErrorInicioMovimiento;
        return false;
    }

    return true;
}

bool PlataformaGiratoria::moverUnPaso(SentidoGiro sentido) {
    return moverPasos(1, sentido);
}

uint8_t PlataformaGiratoria::posicionOpuesta(uint8_t posicion) {
    // 1 <-> 25, 2 <-> 26, ..., 24 <-> 48.
    return (posicion <= (NUMERO_POSICIONES / 2))
        ? posicion + (NUMERO_POSICIONES / 2)
        : posicion - (NUMERO_POSICIONES / 2);
}

bool PlataformaGiratoria::calcularRutaExacta(
    uint8_t destino,
    uint8_t& pasos,
    SentidoGiro& sentido
) const {
    if (!_via1Configurada) {
        return false;
    }

    if (destino < 1 || destino > NUMERO_POSICIONES) {
        return false;
    }

    const uint8_t actual = obtenerPosicionFisica();

    const uint8_t pasosHorario =
        (destino >= actual)
            ? destino - actual
            : NUMERO_POSICIONES - actual + destino;

    const uint8_t pasosAntihorario =
        (actual >= destino)
            ? actual - destino
            : NUMERO_POSICIONES - destino + actual;

    // En empate de 24 pasos se mantiene el criterio determinista:
    // prioridad para el sentido horario.
    if (pasosHorario <= pasosAntihorario) {
        pasos = pasosHorario;
        sentido = SentidoGiro::Horario;
    } else {
        pasos = pasosAntihorario;
        sentido = SentidoGiro::Antihorario;
    }

    return true;
}

bool PlataformaGiratoria::irAPosicionFisicaExacta(uint8_t destino) {
    if (estaOcupada()) {
        return false;
    }

    uint8_t pasos = 0;
    SentidoGiro sentido = SentidoGiro::Horario;

    if (!calcularRutaExacta(destino, pasos, sentido)) {
        return false;
    }

    // Ya está en la posición exacta solicitada.
    if (pasos == 0) {
        _resultadoPendiente = ResultadoPasoHardware::Completado;
        return true;
    }

    return moverPasos(pasos, sentido);
}

bool PlataformaGiratoria::irAViaSalida(uint8_t via) {
    if (estaOcupada()) {
        return false;
    }

    if (!_via1Configurada) {
        return false;
    }

    if (via < 1 || via > NUMERO_POSICIONES) {
        return false;
    }

    // Una misma vía puede alcanzarse con el extremo de referencia
    // encarado a "via", o con él encarado a la posición opuesta.
    const uint8_t destinoDirecto = via;
    const uint8_t destinoOpuesto = posicionOpuesta(via);

    uint8_t pasosDirecto = 0;
    uint8_t pasosOpuesto = 0;

    SentidoGiro sentidoDirecto = SentidoGiro::Horario;
    SentidoGiro sentidoOpuesto = SentidoGiro::Horario;

    if (!calcularRutaExacta(
            destinoDirecto,
            pasosDirecto,
            sentidoDirecto)) {
        return false;
    }

    if (!calcularRutaExacta(
            destinoOpuesto,
            pasosOpuesto,
            sentidoOpuesto)) {
        return false;
    }

    // En igualdad se prioriza el destino indicado directamente.
    // Así el resultado es reproducible y no gira 180° sin necesidad.
    if (pasosDirecto <= pasosOpuesto) {
        if (pasosDirecto == 0) {
            _resultadoPendiente = ResultadoPasoHardware::Completado;
            return true;
        }

        return moverPasos(pasosDirecto, sentidoDirecto);
    }

    if (pasosOpuesto == 0) {
        _resultadoPendiente = ResultadoPasoHardware::Completado;
        return true;
    }

    return moverPasos(pasosOpuesto, sentidoOpuesto);
}



void PlataformaGiratoria::parar() {
    // Cancelamos la orden en curso, pero mantenemos el estado de resume
    // para que la central pueda reenviar base+1 (Resume) y continuar.
    _cambioPolaridadPendiente = false;
    _hardware.parar();

    _movimientoActivo = false;
    _pasosRestantes = 0;
    // No tocamos _tipoOrdenResume/_pasosResume/_sentidoResume
}

bool PlataformaGiratoria::estaOcupada() const {
    return _movimientoActivo || _hardware.estaOcupado();
}

bool PlataformaGiratoria::configurarVia1Aqui() {
    if (estaOcupada() || !_hardware.puenteEnclavado()) {
        return false;
    }

    // La vía 1 equivale a la posición interna 0.
    _via1Configurada = true;
    _posicionActual = 0;

    // La zona 1..12 usa polaridad normal.
    // Se presupone que al arrancar el relé ya está en ese estado,
    // por lo que no se conmuta aquí: los cambios se hacen solo
    // durante el movimiento entre dos posiciones.
    return true;
}

bool PlataformaGiratoria::via1Configurada() const {
    return _via1Configurada;
}

uint8_t PlataformaGiratoria::obtenerPosicionFisica() const {
    return _posicionActual + 1;
}

ResultadoPasoHardware PlataformaGiratoria::obtenerResultadoMovimiento() {
    const ResultadoPasoHardware resultado = _resultadoPendiente;
    _resultadoPendiente = ResultadoPasoHardware::Ninguno;

    return resultado;
}

bool PlataformaGiratoria::iniciarSiguientePaso() {
    prepararCambioPolaridadParaSiguientePaso();

    if (!_hardware.iniciarPaso(_sentidoMovimiento)) {
        _cambioPolaridadPendiente = false;
        return false;
    }

    return true;
}

void PlataformaGiratoria::actualizarPosicionTrasPasoCompletado() {
    if (!_via1Configurada) {
        return;
    }

    if (_sentidoMovimiento == SentidoGiro::Horario) {
        _posicionActual = (_posicionActual + 1) % NUMERO_POSICIONES;
        return;
    }

    if (_posicionActual == 0) {
        _posicionActual = NUMERO_POSICIONES - 1;
    } else {
        _posicionActual--;
    }
}

void PlataformaGiratoria::prepararCambioPolaridadParaSiguientePaso() {
    _cambioPolaridadPendiente = false;
    _movimientoDetectadoParaPolaridad = false;
    _instanteInicioMovimiento = 0;

    // Sin vía 1 no existe una posición física conocida; por seguridad
    // no se puede determinar si el siguiente paso cruza una frontera.
    if (!_via1Configurada) {
        return;
    }

    const uint8_t posicionOrigen = _posicionActual + 1;

    uint8_t posicionDestino;

    if (_sentidoMovimiento == SentidoGiro::Horario) {
        posicionDestino = (posicionOrigen == NUMERO_POSICIONES)
            ? 1
            : posicionOrigen + 1;
    } else {
        posicionDestino = (posicionOrigen == 1)
            ? NUMERO_POSICIONES
            : posicionOrigen - 1;
    }

    // Entrada a la zona 13..36: polaridad invertida / relé activado.
    if ((posicionOrigen == 12 && posicionDestino == 13) ||
        (posicionOrigen == 37 && posicionDestino == 36)) {

        _nuevoEstadoPolaridad = true;
        _cambioPolaridadPendiente = true;
        return;
    }

    // Salida de la zona 13..36: polaridad normal / relé desactivado.
    if ((posicionOrigen == 13 && posicionDestino == 12) ||
        (posicionOrigen == 36 && posicionDestino == 37)) {

        _nuevoEstadoPolaridad = false;
        _cambioPolaridadPendiente = true;
        return;
    }
}

bool PlataformaGiratoria::moverSiguiente(
    SentidoGiro sentido,
    const IndexacionVias& indexacion
) {
    if (estaOcupada()) {
        return false;
    }

    // En modo normal, PH y PA conservan su significado mecánico:
    // un único paso de leva.
    if (!indexacion.esModoIndexado()) {
        return moverPasos(1, sentido);
    }

    // Guardar orden resume (para Parar + Resume en protocolo LocoNet).
    _tipoOrdenResume = TipoOrdenResume::SiguienteViaIndexada;
    _sentidoResume = sentido;

    // Para indexar se necesita una referencia física válida.
    if (!_via1Configurada) {
        return false;
    }

    // Con una única vía no existe una siguiente parada diferente.
    if (indexacion.contarVias() < 2) {
        return false;
    }

    const uint8_t posicionActual = obtenerPosicionFisica();

    // Determina qué vía lógica queda accesible en la orientación actual,
    // tanto por el extremo de referencia como por el opuesto.
    const uint8_t viaActual =
        indexacion.viaAccesibleEnPosicion(posicionActual);

    if (viaActual == 0) {
        return false;
    }

    // Busca la siguiente salida físicamente configurada en el sentido
    // solicitado. El resultado es un número de vía lógica: 1..24.
    const uint8_t viaDestino =
        indexacion.siguienteViaConfigurada(viaActual, sentido);

    if (viaDestino == 0 || viaDestino == viaActual) {
        return false;
    }

    // Convierte la vía lógica elegida a su posición física configurada.
    const uint8_t posicionDestino =
        indexacion.posicionDeVia(viaDestino);

    if (posicionDestino == 0) {
        return false;
    }

    return irAViaSalida(posicionDestino);
}




bool PlataformaGiratoria::resumeUltimaOrden(const IndexacionVias& indexacion) {
    if (_tipoOrdenResume == TipoOrdenResume::Ninguna) {
        return false;
    }

    if (_tipoOrdenResume == TipoOrdenResume::Pasos) {
        return moverPasos(_pasosResume, _sentidoResume);
    }

    if (_tipoOrdenResume == TipoOrdenResume::SiguienteViaIndexada) {
        return moverSiguiente(_sentidoResume, indexacion);
    }

    return false;
}

void PlataformaGiratoria::actualizarCambioPolaridad() {
    if (!_cambioPolaridadPendiente) {
        return;
    }

    if (!_movimientoDetectadoParaPolaridad) {
        if (_hardware.puenteEnMovimiento()) {
            _movimientoDetectadoParaPolaridad = true;
            _instanteInicioMovimiento = millis();
        }

        return;
    }

    if (!_hardware.puenteEnMovimiento()) {
        return;
    }

    if (millis() - _instanteInicioMovimiento <
        RETARDO_CAMBIO_POLARIDAD_MS) {
        return;
    }

    _hardware.establecerPolaridadInvertida(_nuevoEstadoPolaridad);

    _cambioPolaridadPendiente = false;

    Serial.print(F("[POL] Polaridad "));
    Serial.println(_nuevoEstadoPolaridad
        ? F("INVERTIDA activada durante el giro.")
        : F("NORMAL activada durante el giro."));
}

void PlataformaGiratoria::activarSensores() {
    // TODO: Integrar con configuración real:
    // - sensor global (configurable)
    // - sensor por vía (IndexacionVias::sensorDeVia)
    //
    // Para ello PlataformaGiratoria debe tener acceso a IndexacionVias y
    // almacenar el sensor global. En esta iteración dejamos un stub
    // compilable para continuar con el resto del proyecto.
}

void PlataformaGiratoria::desactivarSensores() {
    // Stub compilable. Ver TODO en activarSensores().
}
