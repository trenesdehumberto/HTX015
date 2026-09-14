#pragma once

#include <Arduino.h>
#include "Tipos.h"

struct EepromDatosTT9152 {
    uint8_t version;
    uint8_t reserved1;

    uint8_t posicionPorVia[24];
    uint16_t sensorPorVia[24];

    uint8_t modoRecorrido;      // ModoRecorrido: 0=Normal, 1=Indexado
    uint8_t via1configurada;    // 1=configurada, 0=no configurada
    uint8_t posicionActualPuente;

    uint8_t modoContador;       // ModoContador: 0=Interno, 1=Externo

    // Reservado para web/OLED: futuras configuraciones
    uint8_t reservedWeb[7];

    uint16_t sensorGlobal;
};

namespace EepromTT9152 {

static constexpr uint8_t EEPROM_VERSION = 1;
static constexpr int EEPROM_SIZE = sizeof(EepromDatosTT9152);
static constexpr int EEPROM_BASE = 0;

// Carga los datos de la EEPROM a la estructura
bool cargar(EepromDatosTT9152& datos);
// Graba todos los datos desde la estructura a la EEPROM
void grabar(const EepromDatosTT9152& datos);
// Borra y restablece la EEPROM a valores de fábrica
void reiniciar();
// Devuelve true si los datos en EEPROM son válidos (por versión)
bool validos();
// Opcional: saca volcado debug a Serial
void volcadoDebug(const EepromDatosTT9152& datos);

}
