/*
Ethernet server that provides data using:
BMP085 - Barometric Pressure, Temperature, Altitude
DHT22 - Humidity, Temperature
Thermistors - Indoor Temperature, Outdoor Temperature
Photoresistor - Light Level

One LED pin to be controlled
*/

#include <SPI.h>
#include <Ethernet.h>

#include <Wire.h>
#include <BMP085_Sensor.h>
#include <ThermistorSensor.h>
#include <dht.h>

/////////////// SERVER

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,62,177);

// Initialize the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(80);
String message;
#define MODE_NORMAL 0
#define MODE_JSON 1
#define MODE_RAW 2
#define MODE_ERROR 3
int chosen_mode = MODE_NORMAL;

#define BUFFER 1024 // bytes

//////////////////// SENSORS

// LED Pin
#define LED_PIN 7

// Barometric Sensor
BMP085_Sensor bmp;

// DHT22 Humidity Sensor
dht DHT;
#define DHT22_PIN 2

// Light sensor A0
#define PHOTORESISTOR_PIN A0
#define DARK_VAL 988
#define BRIGHT_VAL 22

// Outdoor thermistor
ThermistorSensor outdoor_temp(A1);
// Indoor thermistor
ThermistorSensor indoor_temp(A2);

// Returns brightness as a float from 0 to 1, 0 = dark, 1 = bright
float getLightLevel(int pin)
{
  // 988 - dark, 22 - bright
  int val = analogRead(pin);
  return (float)(DARK_VAL-val)/(float)(DARK_VAL-BRIGHT_VAL);
}

/// SENSOR GLOBAL VARS
float light_level;
float bmp_temperature;
float altitude;
long pressure;
float dht_humidity;
float dht_temp;
float in_temp;
float out_temp;
int dht_status;

unsigned long curr_time; // milliseconds sine hardware start

///////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(9600);
  Serial.println("Initializing...");
  
  // Reserve & initialize space for string
  message.reserve(BUFFER);
  message = "";
    
  // Initialize LED off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
    
  // Initialize Barometric Pressure Sensor
  bmp.init();
  
    
  // Initialize Thermistors
  outdoor_temp.init();
  indoor_temp.init();
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  
  Serial.println("Initialized.");
}

///////////////////////////////////////////////////////////////
void loop()
{
  int index = 0;

  // printAllData();
  serialPrintRaw();
  
  delay(1000);
  serialPrintJSON();
  delay(1000);

  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        // Read into message string
        if (c != '\n' && c != '\r') {
          index++;

          // Message on current line, ignore past buffer
          if (index < BUFFER) message += c;

          // Keep going through loop
          continue;
        }

        Serial.println(message);

        // If line is blank, reached end of message
        if (message == "") {
          printHeader(client);
          switch (chosen_mode)
          {
            case MODE_JSON:
            clientPrintJSON(client);
            break;
            case MODE_RAW:
            clientPrintRaw(client);
            break;
            case MODE_NORMAL:
            clientPrintAllData(client);
            break;
            case MODE_ERROR:
            default:
            clientPrintError(client);
            break;
          }
          printCloser(client);
          chosen_mode = 0; // reset mode after printing
        }
        else if (message.indexOf("GET /json") >= 0) { chosen_mode = MODE_JSON; } // Return JSON data format
        else if (message.indexOf("GET /raw") >= 0) { chosen_mode = MODE_RAW; } // Return just the raw values
        else { chosen_mode = MODE_ERROR;}

        // Start next line
        message = "";
        index = 0;
      }
    }

    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected\n-----");
  }
}

/////////////////////////////////////////////////////////////////////

void printAllData()
{
  bmp_temperature = bmp.getTemperature();
  pressure = bmp.getPressure();
  altitude = bmp.getAltitude();
  light_level = getLightLevel(PHOTORESISTOR_PIN);
  dht_status = DHT.read22(DHT22_PIN);
  
  Serial.print("BMP Temperature: ");
  Serial.print(bmp_temperature, 2);
  Serial.println(" deg C");
  Serial.print("Pressure: ");
  Serial.print(pressure, DEC);
  Serial.println(" Pa");
  Serial.print("Altitude: ");
  Serial.print(altitude, 2);
  Serial.println(" m");

  switch (dht_status) {
  case DHTLIB_OK:
      Serial.print("Humidity: ");
      Serial.print(DHT.humidity, 1);
      Serial.println(" %");

      Serial.print("DHT Temperature: ");
      Serial.print(DHT.temperature, 1);
      Serial.println(" deg C");
      break;

  case DHTLIB_ERROR_CHECKSUM:
      Serial.println("DHT Checksum error");
      break;

  case  DHTLIB_ERROR_TIMEOUT:
      Serial.println("DHT Timeout error");
      break;

  default:
      Serial.println("DHT Unknown error");
      break;
  }
  
  Serial.print("Light: ");
  Serial.println(light_level, 2);
  
  Serial.print("Outdoor: ");
  Serial.print(outdoor_temp.getReading(), 2);
  Serial.println(" deg C");
  Serial.print("Indoor: ");
  Serial.print(indoor_temp.getReading(), 2);
  Serial.println(" deg C");
  Serial.println();
}

// Read all sensors to global holders
void readSensors()
{
  curr_time = millis(); // Start time
  bmp_temperature = bmp.getTemperature();
  pressure = bmp.getPressure();
  altitude = bmp.getAltitude();
  light_level = getLightLevel(PHOTORESISTOR_PIN);
  in_temp = indoor_temp.getReading();
  out_temp = outdoor_temp.getReading();
  dht_status = DHT.read22(DHT22_PIN);

  switch (dht_status) {
  case DHTLIB_OK:
      dht_humidity = DHT.humidity;
      dht_temp = DHT.temperature;
      break;

  case DHTLIB_ERROR_CHECKSUM:
      dht_humidity = -100;
      dht_temp = -100;
      break;

  case DHTLIB_ERROR_TIMEOUT:
      dht_humidity = -200;
      dht_temp = -200;
      break;

  default:
      dht_humidity = -300;
      dht_temp = -300;
      break;
  }
}

void clientPrintError(EthernetClient &client)
{
  client.print("Unexpected request.");
}
// Print BMP_temp pressure altitude light_level DHT_humidty DHT_temperature in_temp out_temp
void clientPrintRaw(EthernetClient &client)
{
  readSensors();
  client.print(curr_time);
  client.print(" ");
  client.print(light_level, 2);
  client.print(" ");
  client.print(bmp_temperature, 2);
  client.print(" ");
  client.print(pressure, DEC);
  client.print(" ");
  client.print(altitude, 2);
  client.print(" ");
  client.print(dht_status);
  client.print(" ");
  client.print(dht_humidity, 1);
  client.print(" ");
  client.print(dht_temp, 1);
  client.print(" ");
  client.print(in_temp, 2);
  client.print(" ");
  client.println(out_temp, 2);
}

void clientPrintJSON(EthernetClient &client)
{
  readSensors();
  
  client.print("{'curr_time':");
  client.print(curr_time);
  client.print(",'light_level':");
  client.print(light_level, 2);
  client.print(",'bmp_temperature':");
  client.print(bmp_temperature, 2);
  client.print(",'altitude':");
  client.print(altitude, 2);
  client.print(",'pressure':");
  client.print(pressure, DEC);
  client.print(",'dht_status':");
  client.print(dht_status);
  client.print(",'dht_humidity':");
  client.print(dht_humidity, 2);
  client.print(",'dht_temp':");
  client.print(dht_temp, 2);
  client.print(",'in_temp':");
  client.print(in_temp, 2);
  client.print(",'out_temp':");
  client.print(out_temp, 2);
  client.println("}");
}

void clientPrintAllData(EthernetClient &client)
{
  readSensors();
  
  client.print("Time: ");
  client.println(curr_time);
  client.print("BMP Temp: ");
  client.print(bmp_temperature, 2);
  client.println(" deg C<br/>");
  client.print("Indoor Temp: ");
  client.print(in_temp, 2);
  client.println(" deg C<br/>");
  client.print("Pressure: ");
  client.print(pressure, DEC);
  client.println(" Pa<br/>");
  client.print("Altitude: ");
  client.print(altitude, 2);
  client.println(" m<br/>");
  
  switch (dht_status) {
  case DHTLIB_OK:
      client.print("Humidity: ");
      client.print(dht_humidity, 1);
      client.println(" %<br/>");

      client.print("DHT Temp: ");
      client.print(dht_temp, 1);
      client.println(" deg C<br/>");
      break;

  case DHTLIB_ERROR_CHECKSUM:
      client.println("DHT Checksum error<br/>");
      break;

  case DHTLIB_ERROR_TIMEOUT:
      client.println("DHT Timeout error<br/>");
      break;

  default:
      client.println("DHT Unknown error<br/>");
      break;
  }
  
  client.print("Light: ");
  client.print(light_level, 2);
  client.println("<br/>");
  client.print("Outdoor Temp: ");
  client.print(out_temp, 2);
  client.println(" deg C<br/>");
}

void printHeader(EthernetClient &client)
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println("Refresh: 5");  // refresh the page automatically every 5 sec
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
}

void printCloser(EthernetClient &client)
{
  client.println("</html>");
}

void serialPrintRaw()
{
  readSensors();
  Serial.print(curr_time);
  Serial.print(" ");
  Serial.print(light_level, 2);
  Serial.print(" ");
  Serial.print(bmp_temperature, 2);
  Serial.print(" ");
  Serial.print(pressure, DEC);
  Serial.print(" ");
  Serial.print(altitude, 2);
  Serial.print(" ");
  Serial.print(dht_status);
  Serial.print(" ");
  Serial.print(dht_humidity, 1);
  Serial.print(" ");
  Serial.print(dht_temp, 1);
  Serial.print(" ");
  Serial.print(in_temp, 2);
  Serial.print(" ");
  Serial.println(out_temp, 2);
}

void serialPrintJSON()
{
  readSensors();
  
  Serial.print("{'curr_time':");
  Serial.print(curr_time);
  Serial.print(",'light_level':");
  Serial.print(light_level, 2);
  Serial.print(",'bmp_temperature':");
  Serial.print(bmp_temperature, 2);
  Serial.print(",'altitude':");
  Serial.print(altitude, 2);
  Serial.print(",'pressure':");
  Serial.print(pressure, DEC);
  Serial.print(",'dht_status':");
  Serial.print(dht_status);
  Serial.print(",'dht_humidity':");
  Serial.print(dht_humidity, 2);
  Serial.print(",'dht_temp':");
  Serial.print(dht_temp, 2);
  Serial.print(",'in_temp':");
  Serial.print(in_temp, 2);
  Serial.print(",'out_temp':");
  Serial.print(out_temp, 2);
  Serial.println("}");
}