#include "MenuSerie.h"
#include "LocoNetTT9152.h"

#include <stdlib.h>
#include <string.h>

MenuSerie::MenuSerie(
    Hardware& hardware,
    PlataformaGiratoria& plataforma,
    IndexacionVias& indexacion,
    LocoNetTT9152* loconet
)
    : _hardware(hardware),
      _plataforma(plataforma),
      _indexacion(indexacion),
      _loconet(loconet),
      _posicionBuffer(0) {
    _buffer[0] = '\0';
}

void MenuSerie::iniciar() {
    Serial.println(F("\n=== TT9152 LocoNet - Entrega 3 ==="));
    Serial.println(F("Cálculo de ruta más corta."));
    Serial.println(F("Escribe 'ayuda' para ver los comandos."));
}

void MenuSerie::actualizar() {
    while (Serial.available() > 0) {
        const char caracter = static_cast<char>(Serial.read());

        if (caracter == '\r' || caracter == '\n') {
            if (_posicionBuffer > 0) {
                _buffer[_posicionBuffer] = '\0';

                // Comandos del ESP32 (protocolo CMD:...)
                if (strncmp(_buffer, "CMD:", 4) == 0) {
                    procesarComandoESP32(_buffer + 4);
                } else {
                    procesarLinea(_buffer);
                }

                _posicionBuffer = 0;
            }
            continue;
        }

        if (_posicionBuffer < TAMANO_BUFFER_SERIE - 1) {
            if (caracter >= 'A' && caracter <= 'Z') {
                _buffer[_posicionBuffer++] = caracter + ('a' - 'A');
            } else {
                _buffer[_posicionBuffer++] = caracter;
            }
        }
    }

    const ResultadoPasoHardware resultado =
        _plataforma.obtenerResultadoMovimiento();

    if (resultado != ResultadoPasoHardware::Ninguno) {
        imprimirResultadoMovimiento(resultado);

        // Al completar un movimiento, actualizar posicion en EEPROM y notificar ESP32
        if (resultado == ResultadoPasoHardware::Completado) {
            guardarEEPROM();
            emitirEstadoESP32();
        }
    }
}

void MenuSerie::procesarLinea(char* linea) {
    if (strcmp(linea, "ayuda") == 0 || strcmp(linea, "?") == 0) {
        mostrarAyuda();
        return;
    }

    if (strcmp(linea, "estado") == 0) {
        mostrarEstado();
        return;
    }

    if (strcmp(linea, "referenciaaqui") == 0 ||
        strcmp(linea, "via1aqui") == 0 ||
        strcmp(linea, "v1a") == 0) {

        if (_plataforma.configurarVia1Aqui()) {
            _indexacion.configurarVia1();
            guardarEEPROM();

            Serial.println(
                F("[OK] Referencia fisica configurada en posicion 1.")
            );
            Serial.println(
                F("[OK] Via logica 1 asignada a la posicion fisica 1.")
            );
            emitirEstadoESP32();
        } else {
            Serial.println(F("[ERROR] El puente debe estar parado y enclavado."));
        }
        return;
    }

    if (strcmp(linea, "ph") == 0 || strcmp(linea, "paso horario") == 0) {
        if (_plataforma.moverSiguiente(
                SentidoGiro::Horario,
                _indexacion)) {

            Serial.println(_indexacion.esModoIndexado()
                ? F("[OK] Movimiento horario indexado iniciado.")
                : F("[OK] Paso horario iniciado."));
        } else if (_indexacion.esModoIndexado() &&
                   !_plataforma.via1Configurada()) {
            Serial.println(
                F("[ERROR] Primero ejecuta 'referenciaaqui'.")
            );
        } else if (_indexacion.esModoIndexado() &&
                   _indexacion.contarVias() < 2) {
            Serial.println(
                F("[ERROR] En modo indexado deben existir al menos dos vias.")
            );
        } else {
            Serial.println(F("[ERROR] No se puede iniciar el movimiento."));
        }
        return;
    }

    if (strcmp(linea, "pa") == 0 || strcmp(linea, "paso antihorario") == 0) {
        if (_plataforma.moverSiguiente(
                SentidoGiro::Antihorario,
                _indexacion)) {

            Serial.println(_indexacion.esModoIndexado()
                ? F("[OK] Movimiento antihorario indexado iniciado.")
                : F("[OK] Paso antihorario iniciado."));
        } else if (_indexacion.esModoIndexado() &&
                   !_plataforma.via1Configurada()) {
            Serial.println(
                F("[ERROR] Primero ejecuta 'referenciaaqui'.")
            );
        } else if (_indexacion.esModoIndexado() &&
                   _indexacion.contarVias() < 2) {
            Serial.println(
                F("[ERROR] En modo indexado deben existir al menos dos vias.")
            );
        } else {
            Serial.println(F("[ERROR] No se puede iniciar el movimiento."));
        }
        return;
    }

    if (strcmp(linea, "modo normal") == 0) {
        _indexacion.ponerModo(ModoRecorrido::Normal);
        guardarEEPROM();
        Serial.println(F("[OK] Modo NORMAL activado."));
        emitirEstadoESP32();
        return;
    }

    if (strcmp(linea, "modo indexado") == 0) {
        if (!_plataforma.via1Configurada()) {
            Serial.println(
                F("[ERROR] Primero ejecuta 'referenciaaqui'.")
            );
            return;
        }

        if (_indexacion.contarVias() < 2) {
            Serial.println(
                F("[ERROR] Configura al menos dos vias con 'via set'.")
            );
            return;
        }

        _indexacion.ponerModo(ModoRecorrido::Indexado);
        guardarEEPROM();
        Serial.println(F("[OK] Modo INDEXADO activado."));
        emitirEstadoESP32();
        return;
    }

    // via add <via 1..24> <posicion 1..48> [sensor]
    if (strncmp(linea, "via add ", 8) == 0) {
        uint8_t via = 0;
        uint8_t posicion = 0;
        uint16_t sensor = IndexacionVias::SENSOR_SIN_ASIGNAR;

        if (!extraerViaPosicionYSensor(linea + 8, via, posicion, sensor)) {
            Serial.println(
                F("[ERROR] Formato: via add <via 1..24> <posicion 1..48> [sensor]")
            );
            return;
        }

        if (via == 1) {
            if (posicion != 1) {
                Serial.println(
                    F("[ERROR] La via logica 1 solo puede estar en posicion 1.")
                );
                return;
            }

            if (!_plataforma.via1Configurada()) {
                Serial.println(
                    F("[ERROR] Usa primero 'referenciaaqui'.")
                );
                return;
            }
        }

        if (via >= 2 && _indexacion.viaConfigurada(via)) {
            const uint8_t posExistente = _indexacion.posicionDeVia(via);
            if (posicion == posExistente) {
                // Misma posicion: editar sensor
                const uint16_t sensorExistente = _indexacion.sensorDeVia(via);
                if (sensor == IndexacionVias::SENSOR_SIN_ASIGNAR) {
                    Serial.print(F("[AVISO] Via "));
                    Serial.print(via);
                    Serial.println(F(" ya existe. Sin cambios."));
                    return;
                }
                if (sensor == sensorExistente) {
                    Serial.print(F("[AVISO] Via "));
                    Serial.print(via);
                    Serial.println(F(" ya tiene ese sensor."));
                    return;
                }
                _indexacion.asignarSensorVia(via, sensor);
                guardarEEPROM();
                Serial.print(F("[OK] Via "));
                Serial.print(via);
                Serial.print(F(" modificada: sensor = "));
                Serial.println(sensor);
                emitirEstadoESP32();
                return;
            }
            // Posicion distinta: insertar desplazando
            if (!_indexacion.insertarVia(via, posicion, sensor)) {
                Serial.println(F("[ERROR] Posicion fuera de orden o slot ocupado."));
                return;
            }
        } else {
            // Via nueva
            if (!_indexacion.asignarVia(via, posicion)) {
                Serial.println(F("[ERROR] Posicion no valida (ocupada o fuera de orden)."));
                return;
            }
            _indexacion.asignarSensorVia(via, sensor);
        }

        guardarEEPROM();

        Serial.print(F("[OK] Via logica "));
        Serial.print(via);
        Serial.print(F(" asignada a posicion fisica "));
        Serial.print(posicion);

        if (sensor != IndexacionVias::SENSOR_SIN_ASIGNAR) {
            Serial.print(F(", sensor LocoNet "));
            Serial.print(sensor);
        }

        Serial.println();
        emitirEstadoESP32();
        return;
    }

    if (strncmp(linea, "via del ", 8) == 0) {
        uint8_t via = 0;

        if (!extraerPosicion(linea + 8, via) ||
            via > IndexacionVias::VIA_MAXIMA) {
            Serial.println(F("[ERROR] Indica una via entre 2 y 24."));
            return;
        }

        if (via == 1) {
            Serial.println(
                F("[ERROR] La via 1 se define con 'referenciaaqui' y no se elimina.")
            );
            return;
        }

        if (!_indexacion.viaConfigurada(via)) {
            Serial.print(F("[AVISO] La via "));
            Serial.print(via);
            Serial.println(F(" no estaba configurada."));
            return;
        }

        _indexacion.eliminarVia(via);
        guardarEEPROM();

        Serial.print(F("[OK] Via logica "));
        Serial.print(via);
        Serial.println(F(" eliminada."));
        emitirEstadoESP32();
        return;
    }

    if (strcmp(linea, "via clear") == 0) {
        _indexacion.borrarViasExceptoVia1();
        _indexacion.ponerModo(ModoRecorrido::Normal);
        guardarEEPROM();

        Serial.println(F("[OK] Vias 2..24 eliminadas."));
        Serial.println(F("[OK] Via 1 conservada como referencia."));
        Serial.println(F("[OK] Modo NORMAL activado."));
        emitirEstadoESP32();
        return;
    }

    if (strcmp(linea, "via list") == 0 || strcmp(linea, "vias") == 0) {
        mostrarViasConfiguradas();
        return;
    }

    if (strncmp(linea, "ir ", 3) == 0) {
        uint8_t via = 0;

        if (!extraerPosicion(linea + 3, via) ||
            via > IndexacionVias::VIA_MAXIMA) {
            Serial.println(F("[ERROR] Indica una via entre 1 y 24."));
            return;
        }

        if (!_plataforma.via1Configurada()) {
            Serial.println(
                F("[ERROR] Primero ejecuta 'referenciaaqui'.")
            );
            return;
        }

        if (!_indexacion.viaConfigurada(via)) {
            Serial.print(F("[ERROR] La via logica "));
            Serial.print(via);
            Serial.println(F(" no esta configurada."));
            return;
        }

        const uint8_t posicionDestino =
            _indexacion.posicionDeVia(via);

        if (!_plataforma.irAViaSalida(posicionDestino)) {
            Serial.println(F("[ERROR] No se puede iniciar el movimiento."));
            return;
        }

        Serial.print(F("[OK] Movimiento iniciado hacia via logica "));
        Serial.print(via);
        Serial.print(F(" (posicion fisica "));
        Serial.print(posicionDestino);
        Serial.println(F(")."));
        return;
    }

    if (strncmp(linea, "irex ", 5) == 0) {
        const int destino = atoi(linea + 5);

        if (destino < 1 || destino > NUMERO_POSICIONES) {
            Serial.print(F("[ERROR] Indica una posicion entre 1 y "));
            Serial.print(NUMERO_POSICIONES);
            Serial.println(F("."));
            return;
        }

        if (!_plataforma.via1Configurada()) {
            Serial.println(
                F("[ERROR] Primero configura la referencia con 'via1aqui'.")
            );
            return;
        }

        if (!_plataforma.irAPosicionFisicaExacta(
                static_cast<uint8_t>(destino))) {
            Serial.println(F("[ERROR] No se puede iniciar el movimiento."));
            return;
        }

        Serial.print(F("[OK] Movimiento exacto iniciado hacia la posicion "));
        Serial.println(destino);
        return;
    }

    if (strncmp(linea, "mh ", 3) == 0) {
        const int pasos = atoi(linea + 3);

        if (pasos < 1 || pasos > NUMERO_POSICIONES) {
            Serial.print(F("[ERROR] Indica entre 1 y "));
            Serial.print(NUMERO_POSICIONES);
            Serial.println(F(" pasos."));
            return;
        }

        if (_plataforma.moverPasos(
                static_cast<uint8_t>(pasos),
                SentidoGiro::Horario)) {
            Serial.print(F("[OK] Movimiento horario de "));
            Serial.print(pasos);
            Serial.println(F(" pasos iniciado."));
        } else {
            Serial.println(F("[ERROR] No se puede iniciar el movimiento."));
        }
        return;
    }

    if (strncmp(linea, "ma ", 3) == 0) {
        const int pasos = atoi(linea + 3);

        if (pasos < 1 || pasos > NUMERO_POSICIONES) {
            Serial.print(F("[ERROR] Indica entre 1 y "));
            Serial.print(NUMERO_POSICIONES);
            Serial.println(F(" pasos."));
            return;
        }

        if (_plataforma.moverPasos(
                static_cast<uint8_t>(pasos),
                SentidoGiro::Antihorario)) {
            Serial.print(F("[OK] Movimiento antihorario de "));
            Serial.print(pasos);
            Serial.println(F(" pasos iniciado."));
        } else {
            Serial.println(F("[ERROR] No se puede iniciar el movimiento."));
        }
        return;
    }

    if (strcmp(linea, "180") == 0 || strcmp(linea, "giro 180") == 0) {
        if (_plataforma.moverPasos(
                PASOS_GIRO_180,
                SentidoGiro::Horario)) {
            Serial.println(F("[OK] Giro 180 iniciado."));
        } else {
            Serial.println(F("[ERROR] No se puede iniciar el giro 180."));
        }
        return;
    }

    if (strcmp(linea, "parar") == 0) {
        _plataforma.parar();
        Serial.println(F("[OK] Orden de parada enviada."));
        emitirEstadoESP32();
        return;
    }

    if (strncmp(linea, "velocidad ", 10) == 0) {
        const int velocidad = atoi(linea + 10);

        if (velocidad < VELOCIDAD_PWM_MINIMA ||
            velocidad > VELOCIDAD_PWM_MAXIMA) {
            Serial.println(F("[ERROR] La velocidad debe estar entre 0 y 255."));
            return;
        }

        _hardware.establecerVelocidadPWM(static_cast<uint8_t>(velocidad));

        Serial.print(F("[OK] Velocidad PWM: "));
        Serial.println(velocidad);
        emitirEstadoESP32();
        return;
    }

    // sensor global loconet <0..65535>
    if (strncmp(linea, "sensor global ", 14) == 0) {
        const long valorSensor = atol(linea + 14);

        if (valorSensor < 0 || valorSensor > 65535) {
            Serial.println(F("[ERROR] Sensor debe estar entre 0 y 65535 (0=desactivar)."));
            return;
        }

        const uint16_t sensor = static_cast<uint16_t>(valorSensor);

        if (_loconet != nullptr) {
            _loconet->configurarSensorGlobal(sensor);
        }

        guardarEEPROM();

        if (sensor == 0) {
            Serial.println(F("[OK] Sensor global LocoNet desactivado."));
        } else {
            Serial.print(F("[OK] Sensor global LocoNet configurado: "));
            Serial.println(sensor);
        }
        emitirEstadoESP32();
        return;
    }

    // guardar
    if (strcmp(linea, "guardar") == 0) {
        guardarEEPROM();
        Serial.println(F("[OK] Configuracion guardada en EEPROM."));
        return;
    }

    // reset eeprom
    if (strcmp(linea, "reset eeprom") == 0 || strcmp(linea, "fabrica") == 0) {
        // 1. Reiniciar EEPROM con valores de fábrica
        EepromTT9152::reiniciar();

        // 2. Reiniciar objetos en RAM para que coincidan con la EEPROM limpia
        _indexacion.restablecer();
        _plataforma.restaurarEstado(false, 0);

        if (_loconet != nullptr) {
            _loconet->configurarSensorGlobal(0);
            _loconet->configurarDireccionBase(LOCONET_DIRECCION_INICIAL);
        }

        Serial.println(F("[OK] EEPROM y estado en memoria restaurados a fabrica."));
        Serial.println(F("[OK] Vias eliminadas, via 1 sin configurar, modo NORMAL."));

        emitirEstadoESP32();
        emitirConfigCompleta();
        return;
    }

    // exportar configuracion
    if (strcmp(linea, "exportar") == 0 || strcmp(linea, "config export") == 0) {
        exportarConfiguracion();
        return;
    }

    // importar configuracion (línea IMPORT:...)
    if (strncmp(linea, "import:", 7) == 0) {
        importarConfiguracion(linea + 7);
        return;
    }

    // eeprom dump
    if (strcmp(linea, "eeprom dump") == 0) {
        EepromDatosTT9152 datos;
        EepromTT9152::cargar(datos);
        EepromTT9152::volcadoDebug(datos);
        return;
    }

    Serial.print(F("[ERROR] Comando desconocido: "));
    Serial.println(linea);
}

void MenuSerie::mostrarAyuda() const {
    Serial.println(F("\nComandos disponibles:"));
    Serial.println(F("  ayuda                     Muestra esta ayuda"));
    Serial.println(F("  estado                    Muestra sensores y estado"));
    Serial.println(F("  via1aqui | v1a            Configura la posicion actual como referencia fisica y via logica 1"));
    Serial.println(F("  ph | paso horario         Mueve un paso horario a siguiente vía indexada"));
    Serial.println(F("  pa | paso antihorario     Mueve un paso antihorario a siguiente vía indexada"));
    Serial.println(F("  ir <1..24>                Va a una vía de salida lógica configurada usando el extremo mas cercano"));
    Serial.println(F("  irex <1..48>              Va a una posicion fisica exacta por el extremo de referencia"));
    Serial.println(F("  mh <1..48>                Mueve n posiciones en sentido horario"));
    Serial.println(F("  ma <1..48>                Mueve n posiciones en sentido antihorarios"));
    Serial.println(F("  180 | giro 180            Gira 180 grados (PASOS_GIRO_180 pasos)"));
    Serial.println(F("  parar                     Cancela el movimiento actual"));
    Serial.println(F("  velocidad <0..255>        Ajusta el PWM del motor"));
    Serial.println(F("  modo normal               PH/PA mueven una posicion mecanica"));
    Serial.println(F("  modo indexado             PH/PA van a la siguiente via de salida configurada"));
    Serial.println(F("  via add <1..24> <1..48> [sensor]   Asigna una via de salida a una posicion y opcionalmente un sensor"));
    Serial.println(F("  via del <2..24>           Elimina una via de salida"));
    Serial.println(F("  via list | vias           Muestra las vias configuradas y su posición"));
    Serial.println(F("  via clear                 Elimina todas las vias de salida, conserva posicion 1 y activa modo normal"));
    Serial.println(F("  sensor global <0..65535>  Configura el sensor LocoNet global de ocupacion (0=desactivar)"));
    Serial.println(F("  guardar                   Guarda la configuracion actual en EEPROM"));
    Serial.println(F("  eeprom dump               Muestra el contenido de la EEPROM"));
    Serial.println(F("  reset eeprom | fabrica    Restaura la EEPROM a valores de fabrica"));
    Serial.println(F("  exportar | config export  Exporta configuracion como linea IMPORT: (copiar/pegar)"));
    Serial.println(F("  import:<datos>            Importa configuracion desde linea IMPORT: (pegar el texto)"));
    Serial.println();
}

void MenuSerie::mostrarEstado() const {
    Serial.println(F("\n--- Estado TT9152 ---"));

    Serial.print(F("Puente: "));
    Serial.println(_plataforma.estaOcupada()
        ? F("EN MOVIMIENTO")
        : F("ENCLAVADO"));

    Serial.print(F("Sensor enclavamiento: "));
    Serial.println(_hardware.puenteEnclavado()
        ? F("ACTIVO")
        : F("INACTIVO"));

    Serial.print(F("Sensor via 1: "));
    Serial.println(_hardware.sensorVia1Activo()
        ? F("ACTIVO")
        : F("INACTIVO"));

    Serial.print(F("Referencia via 1: "));
    Serial.println(_plataforma.via1Configurada()
        ? F("CONFIGURADA")
        : F("NO CONFIGURADA"));

    Serial.print(F("Posicion fisica actual: "));
    if (_plataforma.via1Configurada()) {
        Serial.println(_plataforma.obtenerPosicionFisica());
    } else {
        Serial.println(F("DESCONOCIDA"));
    }

    Serial.print(F("Via logica actual: "));

    if (!_plataforma.via1Configurada()) {
        Serial.println(F("DESCONOCIDA"));
    } else {
        const uint8_t posicionActual =
            _plataforma.obtenerPosicionFisica();

        const uint8_t viaActual =
            _indexacion.viaAccesibleEnPosicion(posicionActual);

        if (viaActual == 0) {
            Serial.println(F("NINGUNA VIA CONFIGURADA"));
        } else {
            const uint8_t posicionVia =
                _indexacion.posicionDeVia(viaActual);

            Serial.print(F("Via "));
            Serial.print(viaActual);
            Serial.print(F(" (configurada en posicion "));
            Serial.print(posicionVia);

            if (posicionActual != posicionVia) {
                Serial.print(F(", accesible por extremo opuesto"));
            }

            Serial.println(F(")"));
        }
    }

    Serial.print(F("Modo: "));
    Serial.println(_indexacion.esModoIndexado()
        ? F("INDEXADO")
        : F("NORMAL"));

    Serial.print(F("Velocidad PWM: "));
    Serial.println(_hardware.obtenerVelocidadPWM());

    Serial.print(F("Sensor global LocoNet: "));
    if (_loconet != nullptr) {
        const uint16_t sg = _loconet->obtenerSensorGlobal();
        if (sg == 0) {
            Serial.println(F("desactivado"));
        } else {
            Serial.println(sg);
        }
    } else {
        Serial.println(F("no disponible"));
    }

    Serial.println(F("--------------------\n"));
}

void MenuSerie::imprimirResultadoMovimiento(
    ResultadoPasoHardware resultado) const {

    switch (resultado) {
        case ResultadoPasoHardware::Completado:
            Serial.println(F("[OK] Movimiento completado: puente enclavado."));
            break;

        case ResultadoPasoHardware::Cancelado:
            Serial.println(F("[AVISO] Movimiento cancelado."));
            break;

        case ResultadoPasoHardware::ErrorInicioMovimiento:
            Serial.println(F("[ERROR] No se pudo iniciar el siguiente paso."));
            break;

        case ResultadoPasoHardware::ErrorFinMovimiento:
            Serial.println(F("[ERROR] No se detectó el final de un paso."));
            break;

        case ResultadoPasoHardware::Ninguno:
            break;
    }
}

bool MenuSerie::extraerPosicion(
    const char* texto,
    uint8_t& posicion
) const {
    if (texto == nullptr || *texto == '\0') {
        return false;
    }

    char* fin = nullptr;
    const long valor = strtol(texto, &fin, 10);

    if (fin == texto || *fin != '\0') {
        return false;
    }

    if (valor < 1 || valor > NUMERO_POSICIONES) {
        return false;
    }

    posicion = static_cast<uint8_t>(valor);
    return true;
}

void MenuSerie::mostrarViasConfiguradas() const {
    Serial.println(F("\n--- Vias logicas configuradas ---"));

    Serial.print(F("Modo de recorrido: "));
    Serial.println(_indexacion.esModoIndexado()
        ? F("INDEXADO")
        : F("NORMAL"));

    Serial.print(F("Numero de vias: "));
    Serial.println(_indexacion.contarVias());

    uint8_t viaActual = 0;

    if (_plataforma.via1Configurada()) {
        const uint8_t posicionActual =
            _plataforma.obtenerPosicionFisica();

        viaActual =
            _indexacion.viaAccesibleEnPosicion(posicionActual);
    }

    Serial.println();

    for (uint8_t via = IndexacionVias::VIA_MINIMA;
         via <= IndexacionVias::VIA_MAXIMA;
         ++via) {

        const uint8_t posicion =
            _indexacion.posicionDeVia(via);

        Serial.print(F("Via "));

        if (via < 10) {
            Serial.print('0');
        }

        Serial.print(via);
        Serial.print(F(": "));

        if (posicion == 0) {
            Serial.println(F("sin configurar"));
            continue;
        }

        Serial.print(F("posicion "));
        Serial.print(posicion);
        Serial.print(F("  (opuesta "));
        Serial.print(IndexacionVias::posicionOpuesta(posicion));
        Serial.print(F(")"));

        const uint16_t sensor = _indexacion.sensorDeVia(via);
        if (sensor != IndexacionVias::SENSOR_SIN_ASIGNAR) {
            Serial.print(F("  [sensor "));
            Serial.print(sensor);
            Serial.print(F("]"));
        }

        if (via == viaActual) {
            Serial.print(F("  <-- PUENTE AQUI"));
        }

        Serial.println();
    }

    Serial.println(F("-------------------------------\n"));
}

bool MenuSerie::extraerViaYPosicion(
    const char* texto,
    uint8_t& via,
    uint8_t& posicion
) const {
    if (texto == nullptr || *texto == '\0') {
        return false;
    }

    char* fin = nullptr;

    const long valorVia = strtol(texto, &fin, 10);

    if (fin == texto || valorVia < 1 ||
        valorVia > IndexacionVias::VIA_MAXIMA) {
        return false;
    }

    while (*fin == ' ') {
        ++fin;
    }

    if (*fin == '\0') {
        return false;
    }

    char* finPosicion = nullptr;

    const long valorPosicion = strtol(fin, &finPosicion, 10);

    if (finPosicion == fin ||
        *finPosicion != '\0' ||
        valorPosicion < 1 ||
        valorPosicion > NUMERO_POSICIONES) {
        return false;
    }

    via = static_cast<uint8_t>(valorVia);
    posicion = static_cast<uint8_t>(valorPosicion);

    return true;
}

bool MenuSerie::extraerViaPosicionYSensor(
    const char* texto,
    uint8_t& via,
    uint8_t& posicion,
    uint16_t& sensor
) const {
    if (texto == nullptr || *texto == '\0') {
        return false;
    }

    char* fin = nullptr;

    const long valorVia = strtol(texto, &fin, 10);

    if (fin == texto || valorVia < 1 ||
        valorVia > IndexacionVias::VIA_MAXIMA) {
        return false;
    }

    while (*fin == ' ') {
        ++fin;
    }

    if (*fin == '\0') {
        return false;
    }

    char* finPosicion = nullptr;

    const long valorPosicion = strtol(fin, &finPosicion, 10);

    if (finPosicion == fin ||
        valorPosicion < 1 ||
        valorPosicion > NUMERO_POSICIONES) {
        return false;
    }

    while (*finPosicion == ' ') {
        ++finPosicion;
    }

    via = static_cast<uint8_t>(valorVia);
    posicion = static_cast<uint8_t>(valorPosicion);

    if (*finPosicion == '\0') {
        sensor = IndexacionVias::SENSOR_SIN_ASIGNAR;
        return true;
    }

    char* finSensor = nullptr;
    const long valorSensor = strtol(finPosicion, &finSensor, 10);

    if (finSensor == finPosicion ||
        *finSensor != '\0' ||
        valorSensor < 0 ||
        valorSensor > 65535) {
        return false;
    }

    sensor = static_cast<uint16_t>(valorSensor);
    return true;
}

void MenuSerie::guardarEEPROM() const {
    EepromDatosTT9152 datos;

    datos.version = EepromTT9152::EEPROM_VERSION;
    datos.via1configurada = _plataforma.via1Configurada() ? 1 : 0;
    datos.posicionActualPuente = _plataforma.obtenerPosicionFisica();
    datos.modoRecorrido = static_cast<uint8_t>(_indexacion.obtenerModo());
    datos.modoContador = static_cast<uint8_t>(_plataforma.obtenerModoContador());

    const ConfiguracionIndexacion& cfg = _indexacion.obtenerConfiguracion();
    for (uint8_t i = 0; i < 24; ++i) {
        datos.posicionPorVia[i] = cfg.posicionPorVia[i];
        datos.sensorPorVia[i] = cfg.sensorPorVia[i];
    }

    if (_loconet != nullptr) {
        datos.sensorGlobal = _loconet->obtenerSensorGlobal();
        // Guardar dirección base LN en reservedWeb[0..1]
        const uint16_t lnBase = _loconet->obtenerDireccionBase();
        datos.reservedWeb[0] = static_cast<uint8_t>(lnBase & 0xFF);
        datos.reservedWeb[1] = static_cast<uint8_t>((lnBase >> 8) & 0xFF);
    } else {
        datos.sensorGlobal = IndexacionVias::SENSOR_SIN_ASIGNAR;
        datos.reservedWeb[0] = 0;
        datos.reservedWeb[1] = 0;
    }

    EepromTT9152::grabar(datos);
}

void MenuSerie::procesarComandoESP32(const char* cmd) {
    // Los comandos llegan sin el prefijo "CMD:" porque se cortó en actualizar()

    // CMD:MOVERVIA:<n>
    if (strncmp(cmd, "MOVERVIA:", 9) == 0) {
        const uint8_t via = static_cast<uint8_t>(atoi(cmd + 9));
        if (via > 0 && _indexacion.viaConfigurada(via) &&
            _plataforma.via1Configurada()) {
            const uint8_t pos = _indexacion.posicionDeVia(via);
            _plataforma.irAViaSalida(pos);
        }
        emitirEstadoESP32();
        return;
    }

    // CMD:PARAR
    if (strcmp(cmd, "PARAR") == 0) {
        _plataforma.parar();
        emitirEstadoESP32();
        return;
    }

    // CMD:REANUDAR
    if (strcmp(cmd, "REANUDAR") == 0) {
        _plataforma.resumeUltimaOrden(_indexacion);
        emitirEstadoESP32();
        return;
    }

    // CMD:GIRO180
    if (strcmp(cmd, "GIRO180") == 0) {
        _plataforma.moverPasos(PASOS_GIRO_180, SentidoGiro::Horario);
        emitirEstadoESP32();
        return;
    }

    // CMD:REFERENCIAAQUI
    if (strcmp(cmd, "REFERENCIAAQUI") == 0) {
        if (_plataforma.configurarVia1Aqui()) {
            _indexacion.configurarVia1();
            guardarEEPROM();
        }
        emitirEstadoESP32();
        return;
    }

    // CMD:VIACLEAR
    if (strcmp(cmd, "VIACLEAR") == 0) {
        _indexacion.borrarViasExceptoVia1();
        _indexacion.ponerModo(ModoRecorrido::Normal);
        guardarEEPROM();
        emitirEstadoESP32();
        return;
    }

    // CMD:MODNORMAL
    if (strcmp(cmd, "MODNORMAL") == 0) {
        _indexacion.ponerModo(ModoRecorrido::Normal);
        guardarEEPROM();
        emitirEstadoESP32();
        return;
    }

    // CMD:MODINDEXADO
    if (strcmp(cmd, "MODINDEXADO") == 0) {
        _indexacion.ponerModo(ModoRecorrido::Indexado);
        guardarEEPROM();
        emitirEstadoESP32();
        return;
    }

    // CMD:GUARDAR
    if (strcmp(cmd, "GUARDAR") == 0) {
        guardarEEPROM();
        return;
    }

    // CMD:MH:<n>  (mover n pasos horario)
    if (strncmp(cmd, "MH:", 3) == 0) {
        const uint8_t pasos = static_cast<uint8_t>(atoi(cmd + 3));
        if (pasos > 0) _plataforma.moverPasos(pasos, SentidoGiro::Horario);
        emitirEstadoESP32();
        return;
    }

    // CMD:MA:<n>  (mover n pasos antihorario)
    if (strncmp(cmd, "MA:", 3) == 0) {
        const uint8_t pasos = static_cast<uint8_t>(atoi(cmd + 3));
        if (pasos > 0) _plataforma.moverPasos(pasos, SentidoGiro::Antihorario);
        emitirEstadoESP32();
        return;
    }

    // CMD:VIADEL:<n>
    if (strncmp(cmd, "VIADEL:", 7) == 0) {
        const uint8_t via = static_cast<uint8_t>(atoi(cmd + 7));
        if (via >= 2 && _indexacion.viaConfigurada(via)) {
            _indexacion.eliminarVia(via);
            guardarEEPROM();
        }
        emitirEstadoESP32();
        return;
    }

    // CMD:VIAADDNUM:<n>:<p>:<s>
    if (strncmp(cmd, "VIAADDNUM:", 10) == 0) {
        const char* p = cmd + 10;
        char* fin = nullptr;
        const uint8_t via = static_cast<uint8_t>(strtol(p, &fin, 10));
        if (*fin == ':') ++fin;
        const uint8_t pos = static_cast<uint8_t>(strtol(fin, &fin, 10));
        uint16_t sensor = 0;
        if (*fin == ':') sensor = static_cast<uint16_t>(strtol(fin + 1, nullptr, 10));

        if (_indexacion.viaConfigurada(via)) {
            _indexacion.insertarVia(via, pos, sensor);
        } else {
            if (_indexacion.asignarVia(via, pos)) {
                _indexacion.asignarSensorVia(via, sensor);
            }
        }
        guardarEEPROM();
        emitirEstadoESP32();
        return;
    }

    // CMD:SENSORGLOBAL:<s>
    if (strncmp(cmd, "SENSORGLOBAL:", 13) == 0) {
        const uint16_t sensor = static_cast<uint16_t>(atoi(cmd + 13));
        if (_loconet != nullptr) {
            _loconet->configurarSensorGlobal(sensor);
        }
        guardarEEPROM();
        emitirConfigCompleta();
        return;
    }

    // CMD:GETCFG  — el ESP32 solicita la configuración completa
    if (strcmp(cmd, "GETCFG") == 0) {
        emitirEstadoESP32();
        emitirConfigCompleta();
        return;
    }

    // CMD:LNBASE:<n>  — cambia la dirección base de desvíos LocoNet
    if (strncmp(cmd, "LNBASE:", 7) == 0) {
        const uint16_t base = static_cast<uint16_t>(atoi(cmd + 7));
        if (_loconet != nullptr && base > 0 && base <= 2047) {
            _loconet->configurarDireccionBase(base);
            guardarEEPROM();
        }
        emitirConfigCompleta();
        return;
    }

    // CMD:MODCONTADOR:<0|1>  — cambia el modo de contador de posición
    if (strncmp(cmd, "MODCONTADOR:", 12) == 0) {
        const uint8_t mc = static_cast<uint8_t>(atoi(cmd + 12));
        _plataforma.restaurarModoContador(
            mc == 1 ? ModoContador::Externo : ModoContador::Interno
        );
        guardarEEPROM();
        emitirConfigCompleta();
        return;
    }

    // CMD:MODINDEXADO  (ya existe en el bloque superior, pero asegurar con emitirConfig)
    // CMD:MODNORMAL    (ídem)
}

void MenuSerie::emitirConfigCompleta() const {
    // Línea META con todos los metadatos de configuración
    Serial.print(F("CFG:META:V1:"));
    Serial.print(_plataforma.via1Configurada() ? 1 : 0);
    Serial.print(F(":POS:"));
    Serial.print(_plataforma.via1Configurada()
        ? _plataforma.obtenerPosicionFisica() : 0);
    Serial.print(F(":MODO:"));
    Serial.print(_indexacion.esModoIndexado() ? 1 : 0);
    Serial.print(F(":MC:"));
    Serial.print(static_cast<uint8_t>(_plataforma.obtenerModoContador()));
    Serial.print(F(":SG:"));
    Serial.print(_loconet != nullptr
        ? _loconet->obtenerSensorGlobal() : 0);
    Serial.print(F(":LN:"));
    Serial.print(_loconet != nullptr
        ? _loconet->obtenerDireccionBase()
        : LOCONET_DIRECCION_INICIAL);
    Serial.print(F(":N:"));
    Serial.println(_indexacion.contarVias());

    // Una línea por cada vía configurada
    for (uint8_t via = IndexacionVias::VIA_MINIMA;
         via <= IndexacionVias::VIA_MAXIMA; ++via) {
        const uint8_t pos = _indexacion.posicionDeVia(via);
        if (pos == IndexacionVias::POSICION_SIN_ASIGNAR) continue;
        Serial.print(F("CFG:VIA:"));
        Serial.print(via);
        Serial.print(':');
        Serial.print(pos);
        Serial.print(':');
        Serial.println(_indexacion.sensorDeVia(via));
    }

    Serial.println(F("CFG:END"));
}

void MenuSerie::exportarConfiguracion() const {
    // Formato de una sola línea, listo para copiar y pegar de vuelta:
    // import:sg:<sg>;ln:<ln>;modo:<0|1>;via:<id>:<pos>:<sensor>;...
    //
    // NOTA: la referencia de vía 1 (referenciaaqui) NO se exporta.
    // Debe configurarse manualmente después de importar.

    Serial.println(F("\n--- Exportar configuracion TT9152 ---"));
    Serial.println(F("Copia la siguiente linea y pegala en el monitor serie"));
    Serial.println(F("de otro decodificador para transferir la configuracion."));
    Serial.println(F("Despues ejecuta 'referenciaaqui' para establecer la referencia."));
    Serial.println();

    Serial.print(F("import:sg:"));
    Serial.print(_loconet != nullptr ? _loconet->obtenerSensorGlobal() : 0);
    Serial.print(F(";ln:"));
    Serial.print(_loconet != nullptr
        ? _loconet->obtenerDireccionBase()
        : LOCONET_DIRECCION_INICIAL);
    Serial.print(F(";modo:"));
    Serial.print(_indexacion.esModoIndexado() ? 1 : 0);

    // Vías (se omite vía 1, ya que requiere referenciaaqui)
    for (uint8_t via = 2; via <= IndexacionVias::VIA_MAXIMA; ++via) {
        const uint8_t pos = _indexacion.posicionDeVia(via);
        if (pos == IndexacionVias::POSICION_SIN_ASIGNAR) continue;
        Serial.print(F(";via:"));
        Serial.print(via);
        Serial.print(':');
        Serial.print(pos);
        Serial.print(':');
        Serial.print(_indexacion.sensorDeVia(via));
    }

    Serial.println();
    Serial.println(F("--- Fin exportacion ---\n"));
}

void MenuSerie::importarConfiguracion(char* datos) {
    // Parsea formato: sg:<n>;ln:<n>;modo:<0|1>;via:<id>:<pos>:<sensor>;...
    // Aplica la configuración sin tocar la referencia de vía 1.

    if (datos == nullptr || *datos == '\0') {
        Serial.println(F("[ERROR] Datos de importacion vacios."));
        return;
    }

    // Borrar vías 2..24 existentes (conservar vía 1 si está configurada)
    _indexacion.borrarViasExceptoVia1();

    uint8_t viasImportadas = 0;
    char* p = datos;

    while (p != nullptr && *p != '\0') {
        // Buscar fin del campo actual (';' o fin de cadena)
        char* sep = strchr(p, ';');
        if (sep != nullptr) { *sep = '\0'; }

        // Parsear campo:valor
        if (strncmp(p, "sg:", 3) == 0) {
            const uint16_t sg = static_cast<uint16_t>(atoi(p + 3));
            if (_loconet != nullptr) {
                _loconet->configurarSensorGlobal(sg);
            }
        } else if (strncmp(p, "ln:", 3) == 0) {
            const uint16_t base = static_cast<uint16_t>(atoi(p + 3));
            if (_loconet != nullptr && base > 0 && base <= 2047) {
                _loconet->configurarDireccionBase(base);
            }
        } else if (strncmp(p, "modo:", 5) == 0) {
            const uint8_t m = static_cast<uint8_t>(atoi(p + 5));
            _indexacion.ponerModo(m == 1
                ? ModoRecorrido::Indexado
                : ModoRecorrido::Normal);
        } else if (strncmp(p, "via:", 4) == 0) {
            // via:<id>:<pos>:<sensor>
            char* vp = p + 4;
            char* e = nullptr;
            const uint8_t via = static_cast<uint8_t>(strtol(vp, &e, 10));
            if (*e == ':') ++e;
            const uint8_t pos = static_cast<uint8_t>(strtol(e, &e, 10));
            uint16_t sensor = IndexacionVias::SENSOR_SIN_ASIGNAR;
            if (*e == ':') sensor = static_cast<uint16_t>(strtol(e + 1, nullptr, 10));

            if (via >= 2 && via <= IndexacionVias::VIA_MAXIMA &&
                pos >= 1 && pos <= NUMERO_POSICIONES) {
                if (_indexacion.asignarVia(via, pos)) {
                    _indexacion.asignarSensorVia(via, sensor);
                    ++viasImportadas;
                }
            }
        }

        p = (sep != nullptr) ? sep + 1 : nullptr;
    }

    guardarEEPROM();

    Serial.println(F("[OK] Configuracion importada."));
    Serial.print(F("[OK] Vias importadas: "));
    Serial.println(viasImportadas);
    Serial.println(F("[AVISO] Ejecuta 'referenciaaqui' para establecer la referencia de via 1."));

    emitirEstadoESP32();
    emitirConfigCompleta();
}

void MenuSerie::emitirEstadoESP32() const {
    // ST:ENCLAVADO:VIA:<n>:POS:<p>:MODO:<m>:V1:<0|1>
    Serial.print(F("ST:"));

    if (_plataforma.estaOcupada()) {
        Serial.println(F("MOVIMIENTO"));
        return;
    }

    Serial.print(F("ENCLAVADO:VIA:"));
    Serial.print(
        _plataforma.via1Configurada()
            ? _indexacion.viaAccesibleEnPosicion(
                  _plataforma.obtenerPosicionFisica())
            : 0
    );
    Serial.print(F(":POS:"));
    Serial.print(
        _plataforma.via1Configurada()
            ? _plataforma.obtenerPosicionFisica()
            : 0
    );
    Serial.print(F(":MODO:"));
    Serial.print(_indexacion.esModoIndexado() ? 1 : 0);
    Serial.print(F(":V1:"));
    Serial.println(_plataforma.via1Configurada() ? 1 : 0);
}
