/*
 *  Code by:  Dimitrios Vlachos
 *            dimitri.j.vlachos@gmail.com
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <TaskScheduler.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <Arduino_JSON.h>
#include <ArduinoJson.h>

// Screen things
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

#ifndef STASSID
#define STASSID "VM1732529"    // WiFi SSID
#define STAPSK "dqg9bbhkRbCq"  // WiFi Password
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

Scheduler userScheduler;

/* * * * * * * * * * * * * * * */
// Function prototypes
void updateReadings();
void updateDisplay();
void oledSaver();

/*
 * blink the onboard LED
 *
 * @param blinks Number of times to blink the LED
 * @param interval Delay between blinks
 */
void blinkLED(int blinks, long interval = TASK_SECOND / 10);  // Prototype for default parameter values
void blinkOn();
void blinkOff();

Task taskUpdate(TASK_SECOND * 1, TASK_FOREVER, &updateReadings);
Task taskUpdateDisplay(TASK_SECOND * 1, TASK_FOREVER, &updateDisplay);
Task taskOLEDSaver(TASK_MINUTE * 5, TASK_FOREVER, &oledSaver);
Task taskLedBlink(TASK_SECOND / 2, 4, &blinkOff);

/* * * * * * * * * * * * * * * */
// Pin declarations
const int led_pin = D4;

/* * * * * * * * * * * * * * * */
// Sensor declarations
Adafruit_BME280 bme;
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* * * * * * * * * * * * * * * */
// Dynamic global variables
float temperature;
float humidity;
float pressure;
// Text positions - see @oledSaver()
int screen_x;
int screen_y;
// Own IP address
String IP;
// Allocate the JSON document
// Allows to allocated memory to the document dynamically.
DynamicJsonDocument doc(128);
// Set the PORT for the web server
ESP8266WebServer server(80);
// Flag for displaying IP address
bool displayIP = true;

/* * * * * * * * * * * * * * * */
void setup() {
  Serial.begin(115200);
  // Set led pin mode (integrated LED)
  pinMode(led_pin, OUTPUT);
  // Initialise onboard sensors
  initBME();
  // Populate initial sensor data
  updateReadings();
  // Initialise text position
  oledSaver();

  #pragma region WiFi Setup
  //Connect to the WiFi network
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Waiting to connect...");
  }

  #pragma region OTA Setup
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("Weather Station");

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  #pragma endregion

  //Print the board IP address
  Serial.print("IP address: ");
  IP = WiFi.localIP().toString();
  Serial.println(IP);

  server.on("/", get_index);     // Get the index page
  server.on("/json", get_json);  // Get the json data
  server.on("/toggleIP", toggle_ip);  // Toggle IP address display

  server.begin();  //Start the server
  Serial.println("Server listening");
  #pragma endregion

  // Initialise display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;  // Don't proceed, loop forever
  }

  delay(2000);
  display.clearDisplay();

  userScheduler.addTask(taskUpdate);
  userScheduler.addTask(taskUpdateDisplay);
  userScheduler.addTask(taskOLEDSaver);
  userScheduler.addTask(taskLedBlink);
  taskUpdate.enable();
  taskUpdateDisplay.enable();
  taskOLEDSaver.enable();
}

void loop() {
  // Handle OTA events
  ArduinoOTA.handle();
  // Handling of incoming client requests
  server.handleClient();
  // Handling async tasks
  userScheduler.execute();
}

/**
 * Updates data on display, using the co-ordinates
 * set by the oledSaver() function as the starting
 * location for writing
 */
void updateDisplay() {
  // Each character is 6x8 pixels

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  /*
   * using setCursor in increments of 8 is equivalent
   * to using display.println(). However, by doing it
   * this way we are able to 'shift' the text around
   * the screen, allowing us to implement the
   * oledSaver() function to prevent burn in on data
   * displayed for extended periods of time.
   */
  if(displayIP) {
    display.setCursor(screen_x, 0); // This stays in the yellow 'header' section of the OLED display, but shifts around horizontally
    display.print("IP: ");
    display.print(IP);
  }

  display.setCursor(screen_x, screen_y);
  display.print("Temp: \t");
  display.print(String(temperature, 2));
  display.println("C");

  display.setCursor(screen_x, screen_y + 8);
  display.print("Hum:  \t");
  display.print(String(humidity, 2));
  display.println("%");

  display.setCursor(screen_x, screen_y + 16);
  display.print("Pres: \t");
  display.print(String(pressure, 2));
  display.println(" hPa");

  display.display();
}

void updateReadings() {
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;

  String readings = "";

  readings += "Temperature: ";
  readings += String(temperature, 2);
  readings += "°C\n";
  readings += "Humidity: ";
  readings += String(humidity, 2);
  readings += "%\n";
  readings += "Pressure: ";
  readings += String(pressure, 2);
  readings += " hpa\n";


  Serial.println(readings);
  Serial.println();
}

/**
 * Randomly shifts the starting location of text on
 * the OLED output in order to prevent burn-in when
 * left on for extended periods.
 *
 * The exact 'start region' possible has been
 * manually set to fit the text displayed by the
 * updateDisplay() function
 */
void oledSaver() {
  int new_x = random(0, 20);
  int new_y = random(16, 41);
  screen_x = new_x;
  screen_y = new_y;
}

//Init BME280
void initBME() {
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * *
 * This region contains functions necessary for the  *
 * webserver to run, enabling the dashboard and REST *
 * requests to be made.                              *
 * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma region Network code
// Web dashboard HTML
#pragma region HTML
/*
 * Here we create the static web dashboard for this controller.
 * By using the variable modifier PROGMEM, we store this code
 * in the flash memory instead of into SRAM. This frees up a
 * considerable amount of RAM that would otherwise just hold
 * onto a static string.
 */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Weather Station</title>
    <style>
      body {
        font-family: Arial, sans-serif;
        margin: 0;
        padding: 0;
      }
      header {
        background-color: #d627e3;
        color: white;
        padding: 20px;
        text-align: center;
      }
      .card {
        border-radius: 25px;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.2);
        padding: 20px;
        text-align: center;
        margin: 20px auto;
        max-width: 400px;
      }
      p {
        margin: 0;
        font-size: 18px;
        text-align: center;
      }
      strong {
        font-size: 24px;
      }
      button {
        margin: 20px auto;
        padding: 10px 20px;
        font-size: 16px;
        border-radius: 25px;
        background-color: #d627e3;
        color: white;
        border: none;
        cursor: pointer;
        display: block;
      }
    </style>
  </head>
  <body>
    <header>
      <h1>Weather Station</h1>
    </header>
    <div>
      <p> </p>
      <p>Welcome to the home weather station dashboard!</p>
      <p>These are the ambient conditions:</p>
    </div>
    <div class="card">
      <p><strong>Temperature:</strong> <span id="temperature"></span> &deg;C</p>
    </div>
    <div class="card">
      <p><strong>Humidity:</strong> <span id="humidity"></span> %</p>
    </div>
    <div class="card">
      <p><strong>Pressure:</strong> <span id="pressure"></span> hPa</p>
    </div>
    <button onclick="toggleIP()">Toggle IP display</button>

    <script>
      function updateValues() {
        const url = window.location.href + 'json';

        fetch(url)
          .then(response => response.json())
          .then(data => {
            const temperature = data.Readings.temperature;
            const humidity = data.Readings.humidity;
            const pressure = data.Readings.pressure;

            document.getElementById('temperature').innerHTML = temperature.toFixed(2);
            document.getElementById('humidity').innerHTML = humidity.toFixed(2);
            document.getElementById('pressure').innerHTML = pressure.toFixed(2);
          })
          .catch(error => {
            console.error('Error fetching data:', error);
          });
      }     

      function toggleIP() {
        const url = window.location.href + 'toggleIP';

        fetch(url, { method: 'POST' })
          .then(response => {
            if (response.ok) {
              console.log('IP toggled succesfully!');
            } else {
              console.error('Error toggling IP:', response.statusText);
            }
          })
          .catch(error => {
            console.error('Error toggling IP:', error);
          });
      } 
      
      updateValues();
      setInterval(updateValues, 5000);
    </script>
  </body>
</html>
)rawliteral";
#pragma endregion
// Function called to send webpage to client
void get_index() {
  // Blink LED for visual queue
  blinkLED(2);
  //Send the webpage
  server.send(200, "text/html", index_html);
}

// Function called to send JSON data to client
void get_json() {
  // Blink LED for visual queue
  //blinkLED(2);
  // Create JSON data
  updateJSON();
  // Make JSON data ready for the http request
  String jsonStr;
  serializeJsonPretty(doc, jsonStr);
  // Send the JSON data
  server.send(200, "application/json", jsonStr);
}

/*
 * Update JSON doc with most up-to-date
 * readings from the sensors.
 */
void updateJSON() {
  doc["Readings"]["temperature"] = temperature;
  doc["Readings"]["humidity"] = humidity;
  doc["Readings"]["pressure"] = pressure;
}

/*
 * Initialise JSON doc file structure and
 * populate the fields with initial
 * sensor values.
 */
void setupJSON() {
  // Add JSON request data
  doc["Content-Type"] = "application/json";
  doc["Status"] = 200;

  // Add distance sensor JSON object data
  JsonObject sensorReadings = doc.createNestedObject("Readings");
  sensorReadings["temperature"] = temperature;
  sensorReadings["humidity"] = humidity;
  sensorReadings["pressure"] = pressure;
}

void toggle_ip() {
  displayIP = !displayIP;
  server.send(200,"application/json",
  "{\"Content-Type\": \"application/json\",\"Status\": 200}");
}
#pragma endregion
/* * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Task based LED blinking functionality.            *
 * This works by using a Task to callback on         *
 * alternating 'on-off' functions.                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma region LED Blink functions
/*
 * blink the onboard LED
 *
 * @param blinks Number of times to blink the LED
 * @param interval Delay between blinks
 */
void blinkLED(int blinks, long interval) {
  taskLedBlink.setInterval(interval);
  taskLedBlink.setIterations(blinks * 2);  // Double the iterations as on->off->on is two cycles, not one
  taskLedBlink.enable();
}

// Turns LED on and changes task callback to turn off on next iteration
void blinkOn() {
  LEDOn();
  taskLedBlink.setCallback(&blinkOff);

  if (taskLedBlink.isLastIteration()) {
    LEDOn();
  }
}

// Turns LED off and changes task callback to turn on on next iteration
void blinkOff() {
  LEDOff();
  taskLedBlink.setCallback(&blinkOn);

  if (taskLedBlink.isLastIteration()) {
    LEDOn();
  }
}

// Writes LED state to pin
inline void LEDOn() {
  digitalWrite(led_pin, HIGH);
}

// Writes LED state to pin
inline void LEDOff() {
  digitalWrite(led_pin, LOW);
}
#pragma endregion