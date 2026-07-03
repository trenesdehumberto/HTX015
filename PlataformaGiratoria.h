// PlataformaGiratoria.h
#pragma once

#include <Arduino.h>
#include "Tipos.h"
#include "Hardware.h"

class PlataformaGiratoria {
public:
  static constexpr uint8_t NUM_POSICIONES = 48;

  enum class Estado : uint8_t {
    Idle = 0,
    Preparando,
    EsperandoInicioMovimiento,
    Moviendo,
    FinalizandoPaso,
    Error
  };

  struct Opciones {
    uint32_t timeoutInicioMovimientoMs;
    uint32_t timeoutMovimientoMs;
    uint32_t antirreboteMs;
    uint32_t antirreboteVia1Ms;
    uint32_t retardoCambioPolaridadMs;


    constexpr Opciones(uint32_t tInicio = 1200,
                       uint32_t tMov    = 6000,
                       uint32_t antiPaso = 20,
                       uint32_t antiV1   = 30,
                       uint32_t tPol     = 500)  //Ajusta el tiempo de desfase para cambiar polaridad entre vías 11 a 12 y 35 a 36
    : timeoutInicioMovimientoMs(tInicio),
      timeoutMovimientoMs(tMov),
      antirreboteMs(antiPaso),
      antirreboteVia1Ms(antiV1),
      retardoCambioPolaridadMs(tPol) {}
  };

  PlataformaGiratoria(Hardware& hw, const ConfigHardware& cfg, const Opciones& opt = Opciones());

  void iniciar();
  void actualizar();

  // Movimiento por pasos
  bool step(SentidoGiro sentido);                 // 1 paso
  bool moveSteps(uint16_t pasos, SentidoGiro s);  // N pasos
  void stop(); // Stop seguro
  bool gotoVia(uint8_t via);  // 1..48 (requiere cal1)
  bool turn180();             // gira 180º (24 pasos)


  bool estaOcupado() const { return _ocupado; }
  Estado estado() const { return _estado; }
  const char* estadoStr() const;

  // Posición interna 0..47 (no es "vía")
  uint8_t posicion() const { return _pos; }
  void fijarPosicion(uint8_t pos);

  // ---- Calibración vía 1 ----
  bool estaCalibrado() const { return _calibrado; }
  void setAutoCalibracionVia1(bool en) { _autoCalVia1 = en; }
  bool autoCalibracionVia1() const { return _autoCalVia1; }

  // Manual: la posición actual pasa a ser la nueva vía 1
  void calibrarVia1EnPosicionActual();

  // Devuelve 1..48 si calibrado, 0 si no calibrado
  uint8_t viaActual() const;

  uint8_t salidaActual() const; // 1..24 (0 si no calibrado)


  // Evento para que el .ino pueda imprimir que se ha calibrado
  bool consumirEventoCalibracionVia1();

private:
  Hardware& _hw;
  const ConfigHardware& _cfg;
  Opciones _opt;

  Estado _estado = Estado::Idle;
  bool _ocupado = false;

  SentidoGiro _sentido = SentidoGiro::Horario;
  uint16_t _restantes = 0;

  uint8_t _pos = 0;

  // Sensor PASO: ACTIVO = moviendo, INACTIVO = parado (como has decidido)
  bool _pasoActivo = false;
  bool _pasoActivoPrev = false;
  uint32_t _tCambioPaso = 0;

  // Sensor Vía 1 (autoajuste)
  bool _via1Activa = false;
  bool _via1Prev = false;
  uint32_t _tCambioVia1 = 0;

  // Calibración
  bool _calibrado = false;
  uint8_t _posDeVia1 = 0;             // posición interna (0..47) donde está la vía 1
  bool _autoCalVia1 = true;
  bool _eventoCalVia1 = false;

  uint32_t _tEstado = 0;

  // Polaridad (estado que se mantiene hasta el siguiente cambio)
  bool _polaridadActiva = false;

  bool _togglePolPendiente = false;
  bool _togglePolHecho = false;
  uint32_t _tInicioMovimientoReal = 0;

  void _planificarTogglePolaridadParaEstePaso();
  void _procesarTogglePolaridadEnMovimiento();

  uint8_t _viaDePos(uint8_t pos) const;     // 1..48 (requiere calibración)
  void _prepararPolaridadAntesDePaso();     // toggle si el siguiente destino es via 12 o 36


  void _entrar(Estado e);
  void _aplicarSentido();
  void _motor(bool on);
  void _destino(bool enDestino);

  // lecturas filtradas
  void _leerSensorPaso();
  void _leerSensorVia1();

  void _procesarAutoCalVia1();

  void _onPasoCompletado();
  void _fallo(Estado siguiente);

  // --- Helpers para elegir destino por el camino más corto (salidas 1..24 con dos destinos físicos) ---
  struct Ruta {
    uint16_t pasos = 0;
    SentidoGiro sentido = SentidoGiro::Horario;
    uint8_t destinoPos = 0;     // 0..47
    uint8_t cambiosPol = 0;     // nº de cruces 11<->12 y 35<->36 en esa ruta
  };

  static uint8_t _salida24DeVia48(uint8_t via48);  // 1..48 -> 1..24
  uint8_t _via48DePos(uint8_t pos) const;          // ya la tienes como _viaDePos (la usamos)
  bool _esCruceCambioPolaridad(uint8_t curVia48, uint8_t nextVia48) const;
  uint8_t _contarCambiosPolaridadEnRuta(uint8_t startPos, SentidoGiro s, uint16_t pasos) const;

  uint8_t _posDeVia48(uint8_t via48) const;  // via 1..48 -> pos 0..47
  void _calcularPasosMasCorto(uint8_t destinoPos, uint16_t& pasos, SentidoGiro& sentido) const;


  //void _actualizarPolaridadPorVia();  //Se elimina para cambiar polaridad antes de llegar a posiciones de cambio

};
