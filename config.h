#define MIN_TEMP -40.0  // Rango real del DHT22 (-40..80). Antes 0.0: una helada se descartaba como lectura inválida
#define MIN_HUM 0.0     // Rango real del DHT22 (0..100). Antes 35.0: la humedad baja se descartaba como lectura inválida
#define DHT_REINTENTOS 3
#define DHT_REINTENTO_MS 2200
#define DHTPIN 4          // D2 en NodeMCU
#define DHTTYPE DHT22

#define LECTURA_DELAY 5000    // 5 segundos para mostrar en Serial
#define ENVIO_DELAY 300000     // 300 segundos (5 min) entre envíos a la URL
#define PRIMER_ENVIO_MS 10000UL       // espera inicial tras arrancar (deja leer el sensor antes del primer envío)

// Reintentos (nunca se envían valores viejos)
#define REINTENTO_SIN_WIFI_MS 15000UL   // sin WiFi -> reintentar en 15 s
#define REINTENTO_SIN_DATOS_MS 15000UL  // sin lectura reciente -> reintentar en 15 s
#define REINTENTO_ERROR_MS 30000UL      // error de HTTP -> reintentar en 30 s
#define MAX_EDAD_ENVIO_MS 60000UL       // jamás se envía una lectura con más de 60 s de antigüedad
#define ENVIOS_FALLIDOS_REBOOT 5        // 5 envíos fallidos seguidos (~2,5 min) -> reiniciar el ESP

// Autorrecuperación si el sensor deja de dar lecturas válidas
#define DHT_FALLOS_REINIT 6              // 6 lecturas fallidas (~30 s)  -> dht.begin() de nuevo
#define DHT_FALLOS_REBOOT 30             // 30 lecturas fallidas (~2,5 min) -> reiniciar el ESP
#define REBOOT_SIN_LECTURA_MS 180000UL   // ...si lleva 3 min sin lectura válida

// Red: timeouts para no quedarse colgado en el handshake TLS o en el GET
#define HTTP_TIMEOUT_MS 5000

// Watchdog: si loop() no avanza en este tiempo (p.ej. un GET() colgado), se reinicia el ESP
#define WATCHDOG_LIMITE_MS 60000UL

#define APIKEY_PASS "XCDEFGHI1234221390"

// Url de envío al servidor (igual que siempre: Django no cambia nada)
#define SENSOR_URL "https://sensors.pedroreinarojas.com/api/write_data/?apikey=%APIKEY_PASS%&room=%ROOM_ID%&temp=%TEMPERATURE%&hum=%HUMIDITY%&version=%VERSION%"
