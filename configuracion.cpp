#include <EEPROM.h>
#include "configuracion.h"

char WIFI_USER[32] = "";
char WIFI_PASS[32] = "";
int SENSOR_NUM = 1;

#define EEPROM_SIZE 128
#define ADDR_WIFI_USER 0
#define ADDR_WIFI_PASS 32
#define ADDR_SENSOR_NUM 64

void cargarConfigDesdeEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < 32; i++) WIFI_USER[i] = EEPROM.read(ADDR_WIFI_USER + i);
    for (int i = 0; i < 32; i++) WIFI_PASS[i] = EEPROM.read(ADDR_WIFI_PASS + i);
    SENSOR_NUM = EEPROM.read(ADDR_SENSOR_NUM);
    if (SENSOR_NUM < 1 || SENSOR_NUM > 5) SENSOR_NUM = 1;
    WIFI_USER[31] = '\0';
    WIFI_PASS[31] = '\0';
}

void guardarConfigEnEEPROM() {
    for (int i = 0; i < 32; i++) EEPROM.write(ADDR_WIFI_USER + i, WIFI_USER[i]);
    for (int i = 0; i < 32; i++) EEPROM.write(ADDR_WIFI_PASS + i, WIFI_PASS[i]);
    EEPROM.write(ADDR_SENSOR_NUM, SENSOR_NUM);
    EEPROM.commit();
}

void mostrarConfigPorSerial() {
    Serial.println("CONFIGURACION ACTUAL:");
    Serial.print("WIFI_USER: "); Serial.println(WIFI_USER);
    Serial.print("WIFI_PASS: "); Serial.println(WIFI_PASS);
    Serial.print("SENSOR_NUM: "); Serial.println(SENSOR_NUM);
}

void procesarComandoSerial() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd.equalsIgnoreCase("CONFIG")) {
            mostrarConfigPorSerial();
        } else if (cmd.startsWith("CONFIG SET ")) {
            String resto = cmd.substring(11);
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
                } else if (var.equalsIgnoreCase("SENSOR_NUM")) {
                    int num = val.toInt();
                    if (num >= 1 && num <= 5) {
                        SENSOR_NUM = num;
                        guardarConfigEnEEPROM();
                        Serial.println("SENSOR_NUM actualizado");
                    } else {
                        Serial.println("Valor de SENSOR_NUM inválido (1-5)");
                    }
                } else {
                    Serial.println("Variable desconocida");
                }
            } else {
                Serial.println("Formato incorrecto. Usa: CONFIG SET VARIABLE=valor");
            }
        }
    }
}
