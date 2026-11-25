#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include "otaUpdate.h"
#include <Ticker.h>

// Watchdog del loop (definido en functions.cpp)
extern volatile bool watchdogActivo;
extern volatile unsigned long ultimoLoopMs;

Ticker otaTicker;
volatile bool otaLedState = false;

void otaLedBlink()
{
    otaLedState = !otaLedState;
    digitalWrite(LED_BUILTIN, otaLedState ? LOW : HIGH); // Activo bajo
}

void otaUpdate()
{
    // La descarga del firmware puede tardar más que el watchdog: desactivarlo
    watchdogActivo = false;

    Serial.printf("Iniciando actualización OTA desde URL: %s\n", OTA_URL);
    WiFiClient client;
    // Iniciar parpadeo rápido del LED
    pinMode(LED_BUILTIN, OUTPUT);
    otaLedState = false;
    otaTicker.attach_ms(100, otaLedBlink); // Parpadeo cada 100ms

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, OTA_URL);

    otaTicker.detach();
    digitalWrite(LED_BUILTIN, HIGH); // Apagar LED (activo bajo)

    watchdogActivo = true;
    ultimoLoopMs = millis();

    switch (ret)
    {
        case HTTP_UPDATE_FAILED:
            Serial.printf("OTA Error [%d]: %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No hay actualizaciones disponibles.");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("Actualización OTA exitosa. Reiniciando...");
            break;
    }
}
