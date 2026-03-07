void limpiarEEPROM();
#pragma once
#include <Arduino.h>

void limpiarEEPROM();
void cargarConfigDesdeEEPROM();
void guardarConfigEnEEPROM();
void mostrarConfigPorSerial();
void procesarComandoSerial();

extern char WIFI_USER[32];
extern char WIFI_PASS[32];
extern int ROOM_ID;
