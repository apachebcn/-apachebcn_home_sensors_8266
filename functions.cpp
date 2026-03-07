#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include "config.h"
#include "esp8266.h"
#include "functions.h"
#include "version.h"
#include "otaUpdate.h"


// Global variable definitions (only here)
unsigned long now = 0;
unsigned long lastLectura = 0;
unsigned long lastEnvio = 0;
float lastTemp = NAN;
float lastHum = NAN;
bool lastSent = false;

extern DHT dht;
extern WiFiClientSecure client;
extern char WIFI_USER[32];
extern char WIFI_PASS[32];
extern int ROOM_ID;


const char* getRoomName(int id)
{
    switch(id)
    {
        case 1: return "CALLE";
        case 2: return "DORMITORIO MATRIMONIO";
        case 3: return "OFICINA";
        case 4: return "WC ARRIBA";
        case 5: return "COMEDOR";
        default: return "DESCONOCIDO";
    }
}


bool testRange(float t, float h)
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


bool readDht(float &t, float &h)
{
    for (uint8_t intento = 1; intento <= DHT_REINTENTOS; intento++)
    {
        float tTmp = dht.readTemperature();
        float hTmp = dht.readHumidity();
        if (testRange(tTmp, hTmp))
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


void readData()
{
    now = millis();
    if (now - lastLectura >= LECTURA_DELAY)
    {
        lastLectura = now;
        float t = NAN, h = NAN;
        if (readDht(t, h))
        {
            lastTemp = t;
            lastHum = h;
        }
        else
        {
            Serial.println("Lectura DHT inválida/inestable, usando último valor válido.");
        }
        Serial.print("ROOM #");
        Serial.print(ROOM_ID);
        Serial.print(" (");
        Serial.print(getRoomName(ROOM_ID));
        Serial.print(") Temp: ");
        Serial.print(lastTemp);
        Serial.print(" C  Hum: ");
        Serial.print(lastHum);
        Serial.print(" % (");
        unsigned long ms_rest = 0;
        if (lastEnvio == 0)
        {
            ms_rest = 0;
        }
        else if (now >= lastEnvio)
        {
            ms_rest = (lastEnvio + ENVIO_DELAY > now) ? (lastEnvio + ENVIO_DELAY - now) : 0;
        }
        else
        {
            ms_rest = 0;
        }
        Serial.print(ms_rest / 1000);
        Serial.println("s para envío)");
    }
}


void sendData()
{
    bool readyToSend = false;
    if (!lastSent && WiFi.status() == WL_CONNECTED && !isnan(lastTemp) && !isnan(lastHum))
    {
        readyToSend = true;
        lastSent = true;
    }
    else if (lastSent && (now - lastEnvio >= ENVIO_DELAY))
    {
        readyToSend = true;
    }
    if (readyToSend)
    {
        if (WiFi.status() == WL_CONNECTED && !isnan(lastTemp) && !isnan(lastHum))
        {
            HTTPClient https;
            String url = SENSOR_URL;
            url.replace("%TEMPERATURE%", String(lastTemp));
            url.replace("%HUMIDITY%",   String(lastHum));
            url.replace("%ROOM_ID%",   String(ROOM_ID));
            url.replace("%APIKEY_PASS%",  APIKEY_PASS);
            url.replace("%VERSION%", VERSION);
            Serial.print("Enviando: ");
            Serial.println(url);
            if (https.begin(client, url))
            {
                int code = https.GET();
                Serial.print("HTTP code: ");
                Serial.println(code);
                String payload = https.getString();
                Serial.print("Respuesta servidor: ");
                Serial.println(payload);
                // Analizar el valor de result
                int pos = payload.indexOf("result=");
                if (pos != -1)
                {
                    int fin = payload.indexOf('&', pos);
                    String result = (fin != -1) ? payload.substring(pos + 7, fin) : payload.substring(pos + 7);
                    if (result == "ok")
                    {
                        Serial.println("Servidor: OK");
                    }
                    else if (result == "error")
                    {
                        // Mostrar el mensaje del servidor si existe
                        int posMsg = payload.indexOf("message=");
                        if (posMsg != -1)
                        {
                            int finMsg = payload.indexOf('&', posMsg);
                            String message = (finMsg != -1) ? payload.substring(posMsg + 8, finMsg) : payload.substring(posMsg + 8);
                            Serial.print("Mensaje del servidor: ");
                            Serial.println(message);
                        }
                        Serial.println("Servidor: ERROR");
                    }
                    else if (result == "update")
                    {
                        Serial.println("Servidor: UPDATE");
                        otaUpdate();
                    }
                    else
                    {
                        Serial.print("Servidor: Respuesta desconocida: ");
                        Serial.println(result);
                    }
                }
                else
                {
                    Serial.println("No se encontró 'result' en la respuesta del servidor.");
                }
                https.end();
                lastEnvio = now;
            }
            else
            {
                Serial.println("Error iniciando HTTPS");
            }
        }
        else
        {
            Serial.println("No se envía: sin WiFi o sin datos válidos.");
        }
    }
}


void connectWifi()
{
    static int wifiReintentos = 0;
    static unsigned long lastWifiAttempt = 0;
    const int MAX_WIFI_REINTENTOS = 10;
    const unsigned long WIFI_RETRY_BASE = 3000UL; // 3s base
    now = millis();
    if (WiFi.status() != WL_CONNECTED)
    {
        if (now - lastWifiAttempt > WIFI_RETRY_BASE + wifiReintentos * 1000UL)
        {
            lastWifiAttempt = now;
            wifiReintentos++;
            Serial.print("[DEBUG] Intento WiFi #");
            Serial.println(wifiReintentos);
            Serial.println("[DEBUG] Intentando WiFi.begin() sin desconectar ni reiniciar modo WiFi");
            if (WIFI_USER == nullptr || WIFI_PASS == nullptr || strlen(WIFI_USER) == 0 || strlen(WIFI_PASS) == 0) {
                Serial.println("[ERROR] Credenciales WiFi vacías. No se intentará conectar. Configure primero por Serial WIFI_USER y WIFI_PASS.");
                return;
            }
            Serial.print("[DEBUG] WIFI_USER: ");
            Serial.println(WIFI_USER);
            Serial.print("[DEBUG] WIFI_PASS: ");
            Serial.println(WIFI_PASS);
            Serial.print("[DEBUG] Llamando a WiFi.begin con usuario: ");
            Serial.println(WIFI_USER);
            WiFi.begin(WIFI_USER, WIFI_PASS);
            delay(10); yield();
            if (wifiReintentos > MAX_WIFI_REINTENTOS)
            {
                Serial.println("Demasiados fallos WiFi. Reinicio deshabilitado temporalmente para depuración.");
                delay(2000);
                ESP.restart();
            }
        }
    }
    else
    {
        if (wifiReintentos > 0) Serial.println("[DEBUG] ¡WiFi conectado!");
        wifiReintentos = 0;
    }
}