ho// Constantes.h
#pragma once

#include <Arduino.h>

// -------------------------
// Identificación del firmware
// -------------------------
static constexpr const char* NOMBRE_FIRMWARE = "TT9152-ESP32C3";
static constexpr const char* VERSION_FIRMWARE = "0.1.0-hw";

// -------------------------
// Serial / consola
// -------------------------
static constexpr uint32_t BAUDIOS_CONSOLA = 115200;

// -------------------------
// Seguridad / tiempos genéricos
// -------------------------
static constexpr uint32_t MS_ANTIRREBOTE_ENTRADAS = 10;

// -------------------------
// Lógica por defecto (si no se configura)
// -------------------------
static constexpr bool RELES_ACTIVOS_ALTO_POR_DEFECTO = true;
static constexpr bool ENTRADAS_ACTIVAS_BAJO_POR_DEFECTO = true;

