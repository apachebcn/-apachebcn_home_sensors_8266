#define DHTPIN 4          // D2 en NodeMCU
#define DHTTYPE DHT22

#define LECTURA_DELAY 5000    // 5 segundos para mostrar en Serial
#define ENVIO_DELAY 300000     // 180 segundos para enviar datos a la URL

// Url de envío al servidor
#define SENSOR_URL "https://sensors.pedroreinarojas.com/api/write_data/?apikey=APIKEY_PASS&room=ROOM_ID&temp=%TEMPERATURE%&hum=%HUMIDITY%"
