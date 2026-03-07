#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>

#include "esp8266.h"
#include "config.h"
#include "functions.h"
#include "configuracion.h"
#include "version.h"

extern char WIFI_USER[32];
extern char WIFI_PASS[32];

// Mis wifis
// FibraCanalPilastv_5G
// ttuga-wifi


DHT dht(DHTPIN, DHTTYPE);

// Cliente HTTPS global
WiFiClientSecure client;


void setup()
{
    Serial.begin(115200);
    delay(100); // Pequeña pausa para asegurar que el puerto serie esté listo
    Serial.println("\n--- ARRANQUE ---");
    Serial.print("VERSION: ");
    Serial.println(VERSION);
    cargarConfigDesdeEEPROM();
    mostrarConfigPorSerial();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Apagado (activo bajo)
    dht.begin();
    client.setInsecure(); // Acepta cualquier certificado
    // Solo intentar conectar si hay configuración WiFi válida (no vacía ni solo espacios)
    bool user_ok = false, pass_ok = false;
    for (int i = 0; i < 31; ++i)
    {
        if (WIFI_USER[i] > 32 && WIFI_USER[i] <= 126) user_ok = true;
        if (WIFI_PASS[i] > 32 && WIFI_PASS[i] <= 126) pass_ok = true;
    }
    if (user_ok && pass_ok)
    {
        String wifiName = String("HomeSensor-") + getRoomName(ROOM_ID);
        WiFi.hostname(wifiName);
        WiFi.begin(WIFI_USER, WIFI_PASS);
        Serial.println("Intentando conectar a WiFi en segundo plano...");
    }
    else
    {
        Serial.println("WiFi NO CONFIGURADO. Use 'CONFIG SET WIFI_USER=...' y 'CONFIG SET WIFI_PASS=...' por Serial.");
    }
    lastEnvio = millis() - ENVIO_DELAY;
}

void loop()
{
    procesarComandoSerial();

    // Reinicio preventivo cada 24h
    if (now > 86400000UL)
    {
        Serial.println("Reinicio preventivo");
        ESP.restart();
    }

    // --- LED Flash según estado WiFi ---
    static unsigned long lastLedFlash = 0;
    static int ledFlashState = 0;
    if (now - lastLedFlash > 1000)
    {
        lastLedFlash = now;
        if (WiFi.status() == WL_CONNECTED)
        {
            digitalWrite(LED_BUILTIN, LOW); delay(60); digitalWrite(LED_BUILTIN, HIGH); delay(120);
            digitalWrite(LED_BUILTIN, LOW); delay(60); digitalWrite(LED_BUILTIN, HIGH);
        }
        else
        {
            digitalWrite(LED_BUILTIN, LOW); delay(60); digitalWrite(LED_BUILTIN, HIGH);
        }
    }


    // Llamar a connectWifi() solo si no está conectado y tras un intervalo prudente
    static unsigned long lastWifiCheck = 0;
    const unsigned long WIFI_CHECK_INTERVAL = 3000UL; // 3 segundos
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long nowCheck = millis();
        if (nowCheck - lastWifiCheck > WIFI_CHECK_INTERVAL) {
            lastWifiCheck = nowCheck;
            connectWifi();
        }
    }

    readData();

    sendData();

    yield();
}
