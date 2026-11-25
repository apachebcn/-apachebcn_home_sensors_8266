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
unsigned long proximoEnvio = 0;
float lastTemp = NAN;
float lastHum = NAN;

// Estado para la autorrecuperación del sensor
unsigned long ultimaLecturaOk = 0;  // millis() de la última lectura válida
unsigned int fallosDht = 0;         // lecturas fallidas consecutivas
unsigned int fallosEnvio = 0;       // envíos fallidos consecutivos
bool avisadoSinDatos = false;       // para no repetir el aviso en cada reintento

// Watchdog del loop
volatile unsigned long ultimoLoopMs = 0;
volatile bool watchdogActivo = true;

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
    // Un DHT22 averiado devuelve 0,0 / 0,0: eso no es una lectura real
    if (t == 0.0 && h == 0.0)
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
            ultimaLecturaOk = now;
            fallosDht = 0;
        }
        else
        {
            fallosDht++;
            Serial.print("Lectura DHT inválida/inestable (fallo ");
            Serial.print(fallosDht);
            Serial.println("), usando último valor válido.");

            // Recuperación 1: reinicializar el sensor (se queda colgado con frecuencia)
            if (fallosDht % DHT_FALLOS_REINIT == 0)
            {
                Serial.println("Reinicializando sensor DHT...");
                dht.begin();
                delay(1000);
            }

            // Recuperación 2: reiniciar el ESP, pero sólo si lleva mucho tiempo
            // sin lectura válida (si no, se quedaría en un bucle de reinicios)
            if (fallosDht >= DHT_FALLOS_REBOOT)
            {
                unsigned long sinLectura = (ultimaLecturaOk == 0) ? now : (now - ultimaLecturaOk);
                if (sinLectura >= REBOOT_SIN_LECTURA_MS)
                {
                    Serial.println("Sensor sin respuesta: reiniciando dispositivo...");
                    delay(100);
                    ESP.restart();
                }
            }
        }
        Serial.print("ROOM #");
        Serial.print(ROOM_ID);
        Serial.print(" (");
        Serial.print(getRoomName(ROOM_ID));
        Serial.print(") Temp: ");
        Serial.print(lastTemp);
        Serial.print("C  Hum: ");
        Serial.print(lastHum);
        Serial.print("% (edad ");
        Serial.print(edadUltimaLecturaSeg());
        Serial.print("s, fallos ");
        Serial.print(fallosDht);
        Serial.print(", ");
        unsigned long ahora = millis();
        unsigned long ms_rest = (proximoEnvio > ahora) ? (proximoEnvio - ahora) : 0;
        Serial.print(ms_rest / 1000);
        Serial.println("s para envío)");
    }
}


long edadUltimaLecturaSeg()
{
    if (ultimaLecturaOk == 0)
    {
        return -1;
    }
    return (long)((millis() - ultimaLecturaOk) / 1000UL);
}


void sendData(bool force)
{
    if (!force && now < proximoEnvio)
    {
        return;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("No se envía: sin WiFi. Reintentando en 15 s.");
        proximoEnvio = now + REINTENTO_SIN_WIFI_MS;
        return;
    }

    // Si se fuerza el envío, intentar leer el sensor ahora
    if (force)
    {
        float t = NAN, h = NAN;
        if (readDht(t, h))
        {
            lastTemp = t;
            lastHum = h;
            ultimaLecturaOk = now;
            fallosDht = 0;
        }
    }

    // Nunca se envían valores viejos: si la última lectura válida no es reciente,
    // no se manda nada (Django no cambia) y se reintenta en 15 s
    long edad = edadUltimaLecturaSeg();
    if (edad < 0 || (unsigned long)edad * 1000UL > MAX_EDAD_ENVIO_MS)
    {
        if (!avisadoSinDatos)
        {
            Serial.print("No se envía: sin lectura válida reciente (edad ");
            Serial.print(edad);
            Serial.println(" s). Reintentando cada 15 s.");
            avisadoSinDatos = true;
        }
        proximoEnvio = now + REINTENTO_SIN_DATOS_MS;
        return;
    }
    avisadoSinDatos = false;

    HTTPClient https;
    String url = SENSOR_URL;
    url.replace("%TEMPERATURE%", String(lastTemp));
    url.replace("%HUMIDITY%",   String(lastHum));
    url.replace("%ROOM_ID%",   String(ROOM_ID));
    url.replace("%APIKEY_PASS%",  APIKEY_PASS);
    url.replace("%VERSION%", VERSION);
    Serial.print("Enviando: ");
    Serial.println(url);

    // Acotar el tiempo de espera y cerrar cualquier sesión TLS previa: sin esto
    // una conexión atascada deja el equipo sin enviar para siempre
    client.stop();
    https.setTimeout(HTTP_TIMEOUT_MS);

    if (https.begin(client, url))
    {
        int code = https.GET();
        ESP.wdtFeed();
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

        // Si el servidor no ha contestado bien, reintentar antes (30 s)
        if (code == 200)
        {
            fallosEnvio = 0;
            proximoEnvio = now + ENVIO_DELAY;
        }
        else
        {
            fallosEnvio++;
            proximoEnvio = now + REINTENTO_ERROR_MS;
        }
    }
    else
    {
        fallosEnvio++;
        Serial.println("Error iniciando HTTPS. Reintentando en 30 s.");
        proximoEnvio = now + REINTENTO_ERROR_MS;
    }

    if (fallosEnvio > 0)
    {
        Serial.print("Envíos fallidos seguidos: ");
        Serial.println(fallosEnvio);
    }

    // WiFi arriba pero el servidor no responde: reiniciar (la red del ESP puede
    // haberse quedado en un estado del que no sale sola)
    if (fallosEnvio >= ENVIOS_FALLIDOS_REBOOT)
    {
        Serial.println("Demasiados envíos fallidos: reiniciando dispositivo...");
        delay(100);
        ESP.restart();
    }
}


// Watchdog: lo llama un Ticker cada segundo. Si loop() lleva demasiado tiempo
// sin avanzar (p.ej. un GET() colgado), reinicia el equipo.
void watchdogComprobar()
{
    if (!watchdogActivo)
    {
        return;
    }
    if (millis() - ultimoLoopMs > WATCHDOG_LIMITE_MS)
    {
        ESP.restart();
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