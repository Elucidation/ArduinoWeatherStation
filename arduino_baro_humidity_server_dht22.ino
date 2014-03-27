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


void setup()
{
  Serial.begin(9600);
  Serial.println("Initializing...");
  
  // Reserve & initialize space for string
  message.reserve(1024);
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

float light_level;
float bmp_temperature, altitude;
long pressure;

void printAllData()
{
  bmp_temperature = bmp.getTemperature();
  pressure = bmp.getPressure();
  altitude = bmp.getAltitude();
  light_level = getLightLevel(PHOTORESISTOR_PIN);
  
  Serial.print("BMP Temperature: ");
  Serial.print(bmp_temperature, 2);
  Serial.println(" deg C");
  Serial.print("Pressure: ");
  Serial.print(pressure, DEC);
  Serial.println(" Pa");
  Serial.print("Altitude: ");
  Serial.print(altitude, 2);
  Serial.println(" m");
  
  switch (DHT.read22(DHT22_PIN)) {
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
void clientPrintAllData(EthernetClient client)
{
  bmp_temperature = bmp.getTemperature();
  pressure = bmp.getPressure();
  altitude = bmp.getAltitude();
  light_level = getLightLevel(PHOTORESISTOR_PIN);
  
  client.print("BMP Temp: ");
  client.print(bmp_temperature, 2);
  client.println(" deg C<br/>");
  client.print("Pressure: ");
  client.print(pressure, DEC);
  client.println(" Pa<br/>");
  client.print("Altitude: ");
  client.print(altitude, 2);
  client.println(" m<br/>");
  
  switch (DHT.read22(DHT22_PIN)) {
  case DHTLIB_OK:
      client.print("Humidity: ");
      client.print(DHT.humidity, 1);
      client.println(" %<br/>");

      client.print("DHT Temp: ");
      client.print(DHT.temperature, 1);
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
  client.print("Indoor Temp: ");
  client.print(indoor_temp.getReading(), 2);
  client.println(" deg C<br/>");
  client.print("Outdoor Temp: ");
  client.print(outdoor_temp.getReading(), 2);
  client.println(" deg C<br/>");
}

void loop()
{
  printAllData();
  delay(1000);
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    
    // Checks if glass is connecting
    boolean isGlass = false;
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
	  client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();         
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // Print our data
          clientPrintAllData(client);
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
          // Clear message for next line
          message = "";
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
          
          // Message on current line
          message += c;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected\n-----");
  }
}


