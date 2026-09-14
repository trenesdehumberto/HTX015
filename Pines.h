#pragma once

// Asignación centralizada de pines del Arduino Nano.
// Ningún módulo, salvo Hardware.cpp, debe acceder directamente a estos pines.

namespace Pines {
    // Comunicación serie UART hardware.
    // Reservados para el futuro enlace con ESP8266.
    constexpr uint8_t SERIE_RX = 0;
    constexpr uint8_t SERIE_TX = 1;

    // Salidas de la plataforma.
    constexpr uint8_t MOTOR_PWM            = 3;
    constexpr uint8_t RELE_SENTIDO         = 5;
    constexpr uint8_t RELE_DESENCLAVAMIENTO = 6;
    constexpr uint8_t RELE_POLARIDAD       = 9;
    constexpr uint8_t INDICADOR_DESTINO    = 10;

    // Entradas de sensores.
    constexpr uint8_t SENSOR_ENCLAVAMIENTO = 2;
    constexpr uint8_t SENSOR_CONTADOR_EXTERNO = 4;
    constexpr uint8_t SENSOR_VIA_1         = A0;

    // Reservados para la futura interfaz física LocoNet.
    constexpr uint8_t LOCONET_RX = 7;
    constexpr uint8_t LOCONET_TX = 8;

    // Pantalla OLED + encoder rotatorio + botones (PCB UI)
    // I2C compartido con el bus estándar Arduino (Wire)
    constexpr uint8_t OLED_SDA  = A4;   // Hardware I2C SDA
    constexpr uint8_t OLED_SCL  = A5;   // Hardware I2C SCL

    constexpr uint8_t ENCODER_A = A3;   // TRIMA
    constexpr uint8_t ENCODER_B = A2;   // TRIMB
    constexpr uint8_t BTN_PUSH  = 13;   // Botón encoder (PUSH)
    constexpr uint8_t BTN_BACK  = A1;   // Botón BACK
}
