#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>


#include "config.h"
#include "ident.h"
#include "wifi.h"
#include "configuracion.h"

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

// Cliente HTTPS global
WiFiClientSecure client;

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
    cargarConfigDesdeEEPROM();


    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Apagado (activo bajo)

    dht.begin();

    client.setInsecure(); // Acepta cualquier certificado


    String wifiName = "HomeSensor-" + String(SENSOR_NUM);
    WiFi.hostname(wifiName);
    WiFi.begin(WIFI_USER, WIFI_PASS);


    Serial.println("Conectando a WiFi...");
    while (WiFi.status() != WL_CONNECTED)
    {
        digitalWrite(LED_BUILTIN, LOW); // Encender LED mientras conecta
        delay(100);
        Serial.print(".");
        yield();
    }
    digitalWrite(LED_BUILTIN, HIGH); // Apagar LED al conectar
    Serial.println();
    Serial.println("WiFi conectado!");

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

    // Comprobar WiFi cada 10s
    if (now - lastWifiCheck > 10000UL)
    {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("WiFi caído. Reintentando...");
            digitalWrite(LED_BUILTIN, LOW); // LED encendido durante reconexión
            WiFi.disconnect();
            WiFi.begin(WIFI_USER, WIFI_PASS);
        }
    }

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

            Serial.print("Temp: ");
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

            String url = SENSOR_URL;
            url.replace("%TEMPERATURE%", String(temp));
            url.replace("%HUMIDITY%",   String(hum));
            url.replace("SENSOR_NUM",   String(SENSOR_NUM));
            url.replace("APIKEY_PASS",  APIKEY_PASS);

            Serial.print("Enviando: ");
            Serial.println(url);

            if (https.begin(client, url))
            {
                int code = https.GET();
                Serial.print("HTTP code: ");
                Serial.println(code);
                https.end();
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

    yield();
}
