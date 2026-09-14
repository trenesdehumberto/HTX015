#include "EepromTT9152.h"

#include <EEPROM.h>
#include <string.h>

namespace {

void cargarPorDefecto(EepromDatosTT9152& datos) {
    memset(&datos, 0, sizeof(datos));
    datos.version = EepromTT9152::EEPROM_VERSION;
    datos.modoRecorrido = 0;
    datos.via1configurada = 0;
    datos.posicionActualPuente = 1;
    datos.sensorGlobal = 0;
    datos.modoContador = static_cast<uint8_t>(ModoContador::Interno);
}

}

namespace EepromTT9152 {

bool cargar(EepromDatosTT9152& datos) {
    EEPROM.get(EEPROM_BASE, datos);

    if (datos.version != EEPROM_VERSION) {
        cargarPorDefecto(datos);
        return false;
    }

    return true;
}

void grabar(const EepromDatosTT9152& datos) {
    EEPROM.put(EEPROM_BASE, datos);
}

void reiniciar() {
    EepromDatosTT9152 datos;
    cargarPorDefecto(datos);
    grabar(datos);
}

bool validos() {
    EepromDatosTT9152 datos;
    EEPROM.get(EEPROM_BASE, datos);
    return datos.version == EEPROM_VERSION;
}

void volcadoDebug(const EepromDatosTT9152& datos) {
    Serial.println(F("\n--- EEPROM TT9152 ---"));
    Serial.print(F("Version: "));
    Serial.println(datos.version);

    Serial.print(F("Modo recorrido: "));
    Serial.println(datos.modoRecorrido);

    Serial.print(F("Via1 configurada: "));
    Serial.println(datos.via1configurada);

    Serial.print(F("Posicion actual puente: "));
    Serial.println(datos.posicionActualPuente);

    Serial.print(F("Sensor global: "));
    Serial.println(datos.sensorGlobal);

    Serial.print(F("Modo contador: "));
    Serial.println(datos.modoContador == static_cast<uint8_t>(ModoContador::Externo)
        ? F("EXTERNO (D4)")
        : F("INTERNO (leva enclavamiento)"));

    for (uint8_t i = 0; i < 24; ++i) {
        Serial.print(F("Via "));
        Serial.print(i + 1);
        Serial.print(F(": pos="));
        Serial.print(datos.posicionPorVia[i]);
        Serial.print(F(" sensor="));
        Serial.println(datos.sensorPorVia[i]);
    }

    Serial.println(F("---------------------\n"));
}

}
