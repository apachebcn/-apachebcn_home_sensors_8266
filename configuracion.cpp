#include <EEPROM.h>
#include "configuracion.h"
#include "version.h"

// Prototipo para poder usar getRoomName en este archivo
const char* getRoomName(int id);

char WIFI_USER[32] = "";
char WIFI_PASS[32] = "";
int ROOM_ID = 1;

#define EEPROM_SIZE 128
#define ADDR_WIFI_USER 0
#define ADDR_WIFI_PASS 32
#define ADDR_SENSOR_NUM 64

void cargarConfigDesdeEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < 32; i++) WIFI_USER[i] = EEPROM.read(ADDR_WIFI_USER + i);
    for (int i = 0; i < 32; i++) WIFI_PASS[i] = EEPROM.read(ADDR_WIFI_PASS + i);
    ROOM_ID = EEPROM.read(ADDR_SENSOR_NUM);
    if (ROOM_ID < 1 || ROOM_ID > 5) ROOM_ID = 0;
    WIFI_USER[31] = '\0';
    WIFI_PASS[31] = '\0';
}

void guardarConfigEnEEPROM() {
    for (int i = 0; i < 32; i++) EEPROM.write(ADDR_WIFI_USER + i, WIFI_USER[i]);
    for (int i = 0; i < 32; i++) EEPROM.write(ADDR_WIFI_PASS + i, WIFI_PASS[i]);
    EEPROM.write(ADDR_SENSOR_NUM, ROOM_ID);
    EEPROM.commit();
}

void mostrarConfigPorSerial() {
    Serial.println("CONFIGURACION ACTUAL:");
    Serial.print("VERSION: ");
    Serial.println(VERSION);
    Serial.print("WIFI_USER: "); Serial.println(WIFI_USER);
    Serial.print("WIFI_PASS: "); Serial.println(WIFI_PASS);
    Serial.print("ROOM_ID: "); Serial.println(ROOM_ID);
    if (ROOM_ID == 0) Serial.println("(NO CONFIGURADO)");
    Serial.println("\nOpciones ROOM_ID:");
    Serial.println("  1: CALLE");
    Serial.println("  2: DORMITORIO MATRIMONIO");
    Serial.println("  3: OFICINA");
    Serial.println("  4: WC ARRIBA");
    Serial.println("  5: COMEDOR");
    Serial.println("\nPara cambiar: CONFIG ROOM_ID=valor");
}

void procesarComandoSerial() {
    static String buffer = "";
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue; // Ignorar retorno de carro
        if (c == '\n') {
            buffer.trim();
            if (buffer.length() > 0) {
                String bufferUpper = buffer;
                bufferUpper.toUpperCase();
                if (bufferUpper == "CONFIG") {
                    mostrarConfigPorSerial();
                } else if (bufferUpper.startsWith("CONFIG ")) {
                    String resto = buffer.substring(7);
                    int eq = resto.indexOf('=');
                    if (eq > 0) {
                        String var = resto.substring(0, eq);
                        String val = resto.substring(eq + 1);
                        var.trim(); val.trim();
                        if (var.equalsIgnoreCase("WIFI_USER")) {
                            val.toCharArray(WIFI_USER, 32);
                            guardarConfigEnEEPROM();
                            Serial.println("WIFI_USER actualizado");
                        } else if (var.equalsIgnoreCase("WIFI_PASS")) {
                            val.toCharArray(WIFI_PASS, 32);
                            guardarConfigEnEEPROM();
                            Serial.println("WIFI_PASS actualizado");
                        } else if (var.equalsIgnoreCase("ROOM_ID")) {
                            int num = val.toInt();
                            if (num >= 1 && num <= 5) {
                                ROOM_ID = num;
                                guardarConfigEnEEPROM();
                                Serial.print("ROOM_ID actualizado a: ");
                                Serial.println(ROOM_ID);
                                Serial.print("Nombre de sala: ");
                                Serial.println(getRoomName(ROOM_ID));
                            } else {
                                Serial.println("Valor de ROOM_ID inválido (1-5)");
                            }
                        } else {
                            Serial.println("Variable desconocida");
                        }
                    } else {
                        Serial.println("Formato incorrecto. Usa: CONFIG VARIABLE=valor");
                    }
                }
            }
            buffer = "";
        } else {
            buffer += c;
        }
    }
}
