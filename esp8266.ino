#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>



#include "config.h"
#include "wifi.h"
#include "configuracion.h"
#include "version.h"

// Prototipo para evitar error de declaración
void gestionarWiFi();
const char* getRoomName(int id);

DHT dht(DHTPIN, DHTTYPE);

const uint8_t DHT_REINTENTOS = 3;
const unsigned long DHT_REINTENTO_MS = 2200;
const float MIN_TEMP = 0.0;    // Menos de 0°C no es válido
const float MIN_HUM = 35.0;    // Menos de 35% no es válido

// Timers
unsigned long lastLectura = 0;
unsigned long lastEnvio = 0;
unsigned long lastWifiCheck = 0;

// Últimos valores válidos
float temp = NAN;
float hum  = NAN;

#define APIKEY_PASS "XCDEFGHI1234221390"

// Cliente HTTPS global
WiFiClientSecure client;

// Devuelve el nombre de la sala según ROOM_ID
const char* getRoomName(int id) {
    switch(id) {
        case 1: return "CALLE";
        case 2: return "DORMITORIO MATRIMONIO";
        case 3: return "OFICINA";
        case 4: return "WC ARRIBA";
        case 5: return "COMEDOR";
        default: return "DESCONOCIDO";
    }
}

bool lecturaEnRango(float t, float h)
{
    if (isnan(t) || isnan(h))
    {
        return false;
    }
    if (t < MIN_TEMP || t > 60.0)
    {
        return false;
    }
    if (h < MIN_HUM || h > 100.0)
    {
        return false;
    }
    return true;
}

bool leerDHTRobusto(float &t, float &h)
{
    for (uint8_t intento = 1; intento <= DHT_REINTENTOS; intento++)
    {
        float tTmp = dht.readTemperature();
        float hTmp = dht.readHumidity();

        if (lecturaEnRango(tTmp, hTmp))
        {
            t = tTmp;
            h = hTmp;
            return true;
        }

        if (intento < DHT_REINTENTOS)
        {
            Serial.print("Lectura inestable (intento ");
            Serial.print(intento);
            Serial.println("). Reintentando...");
            delay(DHT_REINTENTO_MS);
            yield();
        }
    }

    return false;
}

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
    for (int i = 0; i < 31; ++i) {
        if (WIFI_USER[i] > 32 && WIFI_USER[i] <= 126) user_ok = true;
        if (WIFI_PASS[i] > 32 && WIFI_PASS[i] <= 126) pass_ok = true;
    }
    if (user_ok && pass_ok) {
        String wifiName = String("HomeSensor-") + getRoomName(ROOM_ID);
        WiFi.hostname(wifiName);
        WiFi.begin(WIFI_USER, WIFI_PASS);
        Serial.println("Intentando conectar a WiFi en segundo plano...");
    } else {
        Serial.println("WiFi NO CONFIGURADO. Use 'CONFIG SET WIFI_USER=...' y 'CONFIG SET WIFI_PASS=...' por Serial.");
    }

    lastEnvio = millis() - ENVIO_DELAY;
}

void loop()
{

    procesarComandoSerial();
    unsigned long now = millis();

    // Reinicio preventivo cada 24h
    if (now > 86400000UL)
    {
        Serial.println("Reinicio preventivo");
        ESP.restart();
    }


    // --- LED Flash según estado WiFi ---
    static unsigned long lastLedFlash = 0;
    static int ledFlashState = 0;
    if (now - lastLedFlash > 1000) { // Cada segundo
        lastLedFlash = now;
        if (WiFi.status() == WL_CONNECTED) {
            // Doble flash
            digitalWrite(LED_BUILTIN, LOW); delay(60); digitalWrite(LED_BUILTIN, HIGH); delay(120);
            digitalWrite(LED_BUILTIN, LOW); delay(60); digitalWrite(LED_BUILTIN, HIGH);
        } else {
            // Un solo flash
            digitalWrite(LED_BUILTIN, LOW); delay(60); digitalWrite(LED_BUILTIN, HIGH);
        }
    }

    // Gestión robusta de WiFi
    gestionarWiFi();

    // Lectura del DHT
    if (now - lastLectura >= LECTURA_DELAY)
    {
        lastLectura = now;

        float t = NAN;
        float h = NAN;

        if (leerDHTRobusto(t, h))
        {
            temp = t;
            hum  = h;

            // Identificación textual según ROOM_ID usando función común
            Serial.print("ROOM #");
            Serial.print(ROOM_ID);
            Serial.print(" (");
            Serial.print(getRoomName(ROOM_ID));
            Serial.print(") Temp: ");
            Serial.print(temp);
            Serial.print(" C  Hum: ");
            Serial.print(hum);
            Serial.println(" %");
        }
        else
        {
            Serial.println("Lectura DHT inválida/inestable, usando último valor válido.");
        }
    }

    // Envío de datos
    if (now - lastEnvio >= ENVIO_DELAY)
    {
        lastEnvio = now;

        if (WiFi.status() == WL_CONNECTED && !isnan(temp) && !isnan(hum))
        {
            HTTPClient https;
            // ...envío de datos...
        }
        else
        {
            Serial.println("No se envía: sin WiFi o sin datos válidos.");
        }
    }

    yield();
}


// --- Gestión robusta de WiFi ---
void gestionarWiFi() {
    static int wifiReintentos = 0;
    static unsigned long lastWifiAttempt = 0;
    const int MAX_WIFI_REINTENTOS = 10;
    const unsigned long WIFI_RETRY_BASE = 3000UL; // 3s base
    unsigned long now = millis();
    if (WiFi.status() != WL_CONNECTED) {
        if (now - lastWifiAttempt > WIFI_RETRY_BASE + wifiReintentos * 1000UL) {
            lastWifiAttempt = now;
            wifiReintentos++;
            Serial.print("Intento WiFi #");
            Serial.println(wifiReintentos);
            WiFi.disconnect();
            WiFi.begin(WIFI_USER, WIFI_PASS);
            // No imprimir puntos ni bloquear, solo un mensaje por intento
            if (wifiReintentos > MAX_WIFI_REINTENTOS) {
                Serial.println("Demasiados fallos WiFi. Reiniciando...");
                delay(2000);
                ESP.restart();
            }
        }
    } else {
        wifiReintentos = 0;
    }
}