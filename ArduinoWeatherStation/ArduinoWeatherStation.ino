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
#include <SD.h>
///////////////////////////////////////////////////////////////
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

/////////////////////////////////
// SD CARD
// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
const int chipSelect = 4;

// Max filename size in characters
// FAT file systems have a limitation when it comes to naming conventions. 
// You must use the 8.3 format, 
// so that file names look like “NAME001.EXT”, where “NAME001” is an 8 character or fewer string,
// and “EXT” is a 3 character extension. 
// People commonly use the extensions .TXT and .LOG. It is possible to have a shorter file name 
// (for example, mydata.txt, or time.log), but you cannot use longer file names. Read more on the 8.3 convention.
// Any longer than 8 chars + 4 + terminator = 13, and it will start erroring when opening files.
#define MAX_FILENAME_LENGTH 13

// Delay per reading in ms, 1,000 = 1 second. 60,000 = 1 minute. 3600,000 = 1 hour.
#define INTERVAL_MS 10000

// Max index for a log file, ex. log1000.txt given 1000 for example
#define MAX_IDX 1000

// SD File to log data to
File logFile;
String filename;
char filename_buffer[MAX_FILENAME_LENGTH];
int fileIndex = 0;

bool last, do_switch = false;

void doFileSwitch() {
  // Enable switch files in loop
  do_switch = true;
}

// Switches to next incremented log file
void switchFiles() {
  Serial.print("Closed logfile ");
  Serial.println(filename_buffer);
  
  // Go to next index
  fileIndex++;
  getFilename(fileIndex);
  
  Serial.print("Using new logfile ");
  Serial.println(filename_buffer);
}

// Gets filename given an index into filename and filename_buffer global variables
// Since we reserved same space for string filename and buffer no check for size
void getFilename(int i) {
  // log%d.txt where %d is integer i
  filename = "log";
  filename += i;
  filename += ".txt";
  // Load filename to buffer (+ \0 terminator)
  filename.toCharArray(filename_buffer, filename.length()+1 );
}

// Returns index of first unused file or maxIndex
int firstUnusedIndex(int index, int maxIndex) {
  while (index < maxIndex) {
    getFilename(index);
    if (!SD.exists(filename_buffer)) {break;}
    index++;
  }
  return index;
}

///////////////////////////////////////////////////////////////
// SENSORS
/////////////////////////////////
// LED Pin
#define LED_PIN 7
/////////////////////////////////
// Barometric Sensor
BMP085_Sensor bmp;
/////////////////////////////////
// DHT22 Humidity Sensor
dht DHT;
#define DHT22_PIN 2
/////////////////////////////////
// THERMISTORS
// Outdoor thermistor
ThermistorSensor outdoor_temp(A1);
// Indoor thermistor
ThermistorSensor indoor_temp(A2);
/////////////////////////////////
// PHOTOSENSOR
#define PHOTORESISTOR_PIN A0
#define DARK_VAL 988
#define BRIGHT_VAL 22
// Returns brightness as a float from 0 to 1, 0 = dark, 1 = bright
float getLightLevel(int pin)
{
  // 988 - dark, 22 - bright
  int val = analogRead(pin);
  return (float)(DARK_VAL-val)/(float)(DARK_VAL-BRIGHT_VAL);
}
/////////////////////////////////
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
bool sd_card_working = false;
unsigned long last_write;
///////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(9600);
  Serial.println("Initializing...");
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  // Init SD Card

  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output 
  // or the SD library functions will not work. 
  pinMode(53, OUTPUT);     // change this to 53 on a mega

  if (!card.init(SPI_HALF_SPEED, chipSelect)) 
  {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card is inserted?");
    Serial.println("* Is your wiring correct?");
    Serial.println("* did you change the chipSelect pin to match your shield or module?");
    sd_card_working = false;
  } 
  else 
  {
    Serial.println("Wiring is correct and a card is present."); 
    sd_card_working = true;

    if (!volume.init(card)) 
    {
      Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
      sd_card_working = false;
    }
    else
    {
      // print the type and size of the first FAT-type volume
      uint32_t volumesize;
      Serial.print("\nVolume type is FAT");
      Serial.println(volume.fatType(), DEC);
      Serial.println();
      
      volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
      volumesize *= volume.clusterCount();       // we'll have a lot of clusters
      volumesize *= 512;                            // SD card blocks are always 512 bytes
      Serial.print("Volume size (bytes): ");
      Serial.println(volumesize);
      Serial.print("Volume size (Kbytes): ");
      volumesize /= 1024;
      Serial.println(volumesize);
      Serial.print("Volume size (Mbytes): ");
      volumesize /= 1024;
      Serial.println(volumesize);
    }

    
  }
  
  if (sd_card_working)
  {
    // list all files in the card with date and size
    Serial.println("Listing all files in root:");
    root.ls(LS_R | LS_DATE | LS_SIZE);
    Serial.println("---");
  }


  // Reserve & initialize space for string
  message.reserve(BUFFER);
  message = "";

  // Reserve space for filename string
  filename.reserve(MAX_FILENAME_LENGTH);

  // Get initial filename
  fileIndex = firstUnusedIndex(fileIndex, MAX_IDX);
  getFilename(fileIndex);
    
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

  // Last time written to file initialized
  last_write = millis();
  
  Serial.println("Initialized.");
}

///////////////////////////////////////////////////////////////
void loop()
{
  // Write to file if needed
  // if (millis() - last_write > INTERVAL_MS)
  // {
  //   last_write = millis();
  //   serialPrintJSON();
  //   logData();
  // }

  int index = 0;

  // printAllData();
  // serialPrintRaw();

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
          
          switch (chosen_mode)
          {
            case MODE_JSON:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Server: Arduino");
            client.println("Connnection: close");
            client.println();
            clientPrintJSON(client);
            break;
            
            case MODE_RAW:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println();
            clientPrintRaw(client);
            break;
            
            case MODE_NORMAL:
            printHeader(client);
            clientPrintAllData(client);
            printCloser(client);
            break;

            case MODE_ERROR:
            default:
            printHeader(client);
            clientPrintError(client);
            printCloser(client);
            break;
          }

          break; // Break out of while loop
        }
        else if (message.indexOf("GET / ") >= 0) { chosen_mode = MODE_NORMAL; } // Return HTML formatted data
        else if (message.indexOf("GET /json") >= 0) { chosen_mode = MODE_JSON; } // Return JSON data format
        else if (message.indexOf("GET /raw") >= 0) { chosen_mode = MODE_RAW; } // Return just the raw values
        else if (message.indexOf("GET /file/next") >= 0) 
        {
          switchFiles();
          digitalWrite(!digitalRead(LED_PIN), LOW); // blink LED
          chosen_mode = MODE_RAW; // Return just the raw values
        } 
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

void logData()
{
  getFilename(fileIndex);
  logFile = SD.open(filename_buffer, FILE_WRITE);
  // if the file is available, write to it:
  if (logFile) {
    readSensors();
    logFile.print(curr_time);
    logFile.print(" ");
    logFile.print(light_level, 2);
    logFile.print(" ");
    logFile.print(bmp_temperature, 2);
    logFile.print(" ");
    logFile.print(pressure, DEC);
    logFile.print(" ");
    logFile.print(altitude, 2);
    logFile.print(" ");
    logFile.print(dht_status);
    logFile.print(" ");
    logFile.print(dht_humidity, 1);
    logFile.print(" ");
    logFile.print(dht_temp, 1);
    logFile.print(" ");
    logFile.print(in_temp, 2);
    logFile.print(" ");
    logFile.println(out_temp, 2);

    logFile.close();

    // print to the serial port too:
    Serial.print("W ");
    Serial.println(filename_buffer);
  }
  else
  {
    Serial.print("Failed to write to  ");
    Serial.println(filename_buffer);
  }
}

void clientPrintJSON(EthernetClient &client)
{
  readSensors();
  
  client.print("{\n\"curr_time\": ");
  client.print(curr_time);
  client.print(",\n\"light_level\": ");
  client.print(light_level, 2);
  client.print(",\n\"bmp_temperature\": ");
  client.print(bmp_temperature, 2);
  client.print(",\n\"altitude\": ");
  client.print(altitude, 2);
  client.print(",\n\"pressure\": ");
  client.print(pressure, DEC);
  client.print(",\n\"dht_status\": ");
  client.print(dht_status);
  client.print(",\n\"dht_humidity\": ");
  client.print(dht_humidity, 2);
  client.print(",\n\"dht_temp\": ");
  client.print(dht_temp, 2);
  client.print(",\n\"in_temp\": ");
  client.print(in_temp, 2);
  client.print(",\n\"out_temp\": ");
  client.print(out_temp, 2);
  client.println("\n}");
}

void clientPrintAllData(EthernetClient &client)
{
  readSensors();
  
  client.print("Time: ");
  client.println(curr_time);
  client.print("ms since server started<br/>");
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
