#define MIN_TEMP 0.0    // Menos de 0°C no es válido
#define MIN_HUM 35.0    // Menos de 35% no es válido
#define DHT_REINTENTOS 3
#define DHT_REINTENTO_MS 2200
#define DHTPIN 4          // D2 en NodeMCU
#define DHTTYPE DHT22

#define LECTURA_DELAY 5000    // 5 segundos para mostrar en Serial
#define ENVIO_DELAY 300000     // 180 segundos para enviar datos a la URL

#define APIKEY_PASS "XCDEFGHI1234221390"

// Url de envío al servidor
#define SENSOR_URL "https://sensors.pedroreinarojas.com/api/write_data/?apikey=%APIKEY_PASS%&room=%ROOM_ID%&temp=%TEMPERATURE%&hum=%HUMIDITY%&version=%VERSION%"
