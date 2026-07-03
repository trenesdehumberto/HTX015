#include <Arduino.h>

#include "Constantes.h"
#include "Tipos.h"
#include "Hardware.h"
#include "PlataformaGiratoria.h"

ConfigHardware cfg;
Hardware hw(cfg);
PlataformaGiratoria plt(hw, cfg);

static void imprimirEstado() {
  Serial.print("Estado=");
  Serial.print(plt.estadoStr());
  Serial.print(" ocupado=");
  Serial.print(plt.estaOcupado() ? "SI" : "NO");

  Serial.print(" pos=");
  Serial.print(plt.posicion());

  Serial.print(" via=");
  uint8_t v = plt.viaActual();
  if (v == 0) Serial.print("NO_CAL");
  else Serial.print(v);

  Serial.print(" salida=");
  uint8_t s = plt.salidaActual();
  if (s == 0) Serial.print("NO_CAL");
  else Serial.print(s);


  Serial.print(" pasoSensor=");
  Serial.print(hw.leerPulsoPaso() ? "ACTIVO(moviendo)" : "inactivo(parado)");

  Serial.print(" via1Sensor=");
  Serial.print(hw.leerAutoajusteVia1() ? "ACTIVO" : "inactivo"); 

  Serial.print(" autoCalVia1=");
  Serial.println(plt.autoCalibracionVia1() ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.print("Firmware: "); Serial.print(NOMBRE_FIRMWARE);
  Serial.print(" v"); Serial.println(VERSION_FIRMWARE);

  hw.iniciar();
  plt.iniciar();

  Serial.println("Comandos:");
  Serial.println("  step h|a");
  Serial.println("  move <n> h|a");
  Serial.println("  goto <via>     (1..48, requiere cal1)");
  Serial.println("  home          (equivale a goto 1)");
  Serial.println("  turn180       (gira 180 grados)");
  Serial.println("  stop");
  Serial.println("  pos");
  Serial.println("  cal1          (pos actual -> Via 1 manual)");
  Serial.println("  autocal on|off");
  Serial.println("  test          (prueba salidas hardware)");
  Serial.println();
}

void loop() {
  hw.actualizar();
  plt.actualizar();

  if (plt.consumirEventoCalibracionVia1()) {
    Serial.print("Calibrada VIA 1 en pos=");
    Serial.print(plt.posicion());
    Serial.print(" (viaActual=");
    Serial.print(plt.viaActual());
    Serial.println(")");
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "test") {
      Serial.println("Prueba de salidas...");
      hw.probarSalidas(150);
      Serial.println("Fin prueba.");
    }
    else if (cmd == "pos") {
      imprimirEstado();
    }
    else if (cmd == "stop") {
      plt.stop();
      Serial.println("STOP");
      imprimirEstado();
    }
    else if (cmd == "cal1") {
      plt.calibrarVia1EnPosicionActual();
      Serial.println("OK: Via 1 calibrada manualmente en la posicion actual");
      imprimirEstado();
    }
    else if (cmd == "autocal on") {
      plt.setAutoCalibracionVia1(true);
      Serial.println("OK: autoCal Via1 ON");
      imprimirEstado();
    }
    else if (cmd == "autocal off") {
      plt.setAutoCalibracionVia1(false);
      Serial.println("OK: autoCal Via1 OFF");
      imprimirEstado();
    }
    else if (cmd == "step h") {
      if (!plt.step(SentidoGiro::Horario)) Serial.println("Ocupado/no se pudo iniciar");
      else Serial.println("OK: paso Horario");
    }
    else if (cmd == "step a") {
      if (!plt.step(SentidoGiro::Antihorario)) Serial.println("Ocupado/no se pudo iniciar");
      else Serial.println("OK: paso Antihorario");
    }
    else if (cmd == "home") {
      if (!plt.gotoVia(1)) Serial.println("ERROR: no calibrado / ocupado");
      else Serial.println("OK: home (goto 1)");
    }
    else if (cmd == "gira180") {
      if (!plt.turn180()) Serial.println("ERROR: ocupado");
      else Serial.println("OK: gira180");
    }
    else if (cmd.startsWith("goto ")) {
      int via = cmd.substring(5).toInt(); // despues de "goto "
      if (via < 1 || via > 48) {
        Serial.println("ERROR: formato goto <1..48>");
      } else {
        if (!plt.gotoVia((uint8_t)via)) Serial.println("ERROR: no calibrado / ocupado");
        else {
          Serial.print("OK: goto ");
          Serial.println(via);
        }
      }
    }
    else if (cmd.startsWith("move ")) {
      int sp1 = cmd.indexOf(' ');
      int sp2 = cmd.indexOf(' ', sp1 + 1);
      if (sp2 < 0) {
        Serial.println("Formato: move <n> h|a");
      } else {
        uint16_t n = (uint16_t)cmd.substring(sp1 + 1, sp2).toInt();
        String dir = cmd.substring(sp2 + 1);
        SentidoGiro s = (dir == "a") ? SentidoGiro::Antihorario : SentidoGiro::Horario;

        if (!plt.moveSteps(n, s)) Serial.println("Ocupado/no se pudo iniciar");
        else {
          Serial.print("OK: move "); Serial.print(n);
          Serial.print(" "); Serial.println(dir);
        }
      }
    }
    else {
      Serial.println("Comandos: step h|a | move <n> h|a | goto <1..48> | home | gira180 | stop | pos | cal1 | autocal on|off | test");

    }
  }
}
