#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_ssd1306syp.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include "config.h"

#define EEPROM_SCHEMA 0xab
#define EEPROM_TIME_ADDR 1

#define SDA_PIN D2
#define SCL_PIN D1
#define L9110S_B_IA D5
#define L9110S_B_IB D6
#define MOTOR_B_PWM L9110S_B_IA
#define MOTOR_B_DIR L9110S_B_IB
#define DIR_DELAY 1000
#define PWM_SPEED 255
#define MS_TO_DISPENSE_QUARTER_CUP 7000

#define UTC_OFFSET -6

#define DEFAULT_FEEDING_TIME "17:30"
#define DEFAULT_FEEDING_AMOUNT "2"
#define DEFAULT_DISPENSE_DELAY_MS "7000"


const char* host = "time.nist.gov";

unsigned long previousLoopTime = 0;
unsigned int loopDelay = 30000;
unsigned int oneMinute = 60000;
unsigned long lastFeedingTime = 0;

String Argument_Name;
char rawClientTime[] = DEFAULT_FEEDING_TIME;
char rawClientAmount[] = DEFAULT_FEEDING_AMOUNT;
char rawClientDelay[] = DEFAULT_DISPENSE_DELAY_MS;
bool requestDisplayUpdate = true;

ESP8266WebServer server(80);

Adafruit_ssd1306syp display(SDA_PIN,SCL_PIN);

void HandleClient();
void ShowClientResponse();
void ClientFeedNow();
void blink();
unsigned int convertCArrayToInt(char array[]);
void feed(unsigned int quarterCupsToFeed);

void setup(){
  Serial.begin(115200);
  delay(1000);
  display.initialize();
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  pinMode(MOTOR_B_PWM, OUTPUT);
  pinMode(MOTOR_B_DIR, OUTPUT);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  short schema;
  EEPROM.begin(512);
  schema = EEPROM.read(0);
  Serial.println("SCHEMA = " + schema);
  if(schema != EEPROM_SCHEMA) {
    Serial.println("SCHEMA not found, initializing EEPROM with default values");
    for (int i = 0 ; i < 512 ; i++) {
      EEPROM.write(i, 0xFF);
    }
    schema = EEPROM_SCHEMA;
    EEPROM.write(0, schema);

    int i = 0;
    do {
      EEPROM.write(EEPROM_TIME_ADDR+i, rawClientTime[i]);
      i++;
    } while(rawClientTime[i - 1]);

    i = 0;
    do{
      EEPROM.write(EEPROM_TIME_ADDR+sizeof(rawClientTime)+i, rawClientAmount[i]);
      i++;
    } while(rawClientAmount[i - 1]);

    i = 0;
    do{
      EEPROM.write(EEPROM_TIME_ADDR+sizeof(rawClientTime)+sizeof(rawClientAmount)+i, rawClientDelay[i]);
      i++;
    } while(rawClientDelay[i - 1]);

    EEPROM.commit();
  } else {
    for(int i = 0; i < sizeof(rawClientTime); i++) {
      rawClientTime[i] = EEPROM.read(EEPROM_TIME_ADDR+i);
    }
    for(int i = 0; i < sizeof(rawClientAmount); i++) {
      rawClientAmount[i] = EEPROM.read(EEPROM_TIME_ADDR+sizeof(rawClientTime)+i);
    }
    for(int i = 0; i < sizeof(rawClientDelay); i++) {
      rawClientDelay[i] = EEPROM.read(EEPROM_TIME_ADDR+sizeof(rawClientTime)+sizeof(rawClientAmount)+i);
    }

    Serial.println("EEPROM Time: " + String(rawClientTime) + " EEPROM Amount: " + String(rawClientAmount));
  }
  EEPROM.end();

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    display.clear();
    display.setCursor(0,0);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.println("OTA Update in Progress!");
    display.setCursor(0,15);
    display.println("Type: " + type);
    display.update();
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    display.clear();
    display.setCursor(0,0);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.println("Rebooting...");
    display.update();
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char buffer[100];
    display.clear();
    display.setCursor(0,0);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    snprintf(buffer, sizeof(buffer), "Progress: %u%%\r", (progress / (total / 100)));
    display.println(buffer);
    display.update();
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  // Start the server
  server.begin();
  server.on("/", HandleClient);
  server.on("/result", ShowClientResponse);
  server.on("/feednow", ClientFeedNow);
  Serial.println("Server started");

}

void storeEEPROMValues() {
  Serial.println("Storing new values in EEPROM");
  EEPROM.begin(512);
  for(int i = 0; i < sizeof(rawClientTime); i++) {
    EEPROM.write(EEPROM_TIME_ADDR+i, rawClientTime[i]);
  }
  for(int i = 0; i < sizeof(rawClientAmount); i++) {
    EEPROM.write(EEPROM_TIME_ADDR+i+sizeof(rawClientTime), rawClientAmount[i]);
    Serial.println();
  }
  for(int i = 0; i < sizeof(rawClientDelay); i++) {
    EEPROM.write(EEPROM_TIME_ADDR+i+sizeof(rawClientTime)+sizeof(rawClientAmount), rawClientDelay[i]);
    Serial.println();
  }
  EEPROM.commit();
  EEPROM.end();
}

void HandleClient() {
  String webpage;
  webpage =  "<html>";
   webpage += "<head><title>Fat Cat Fix</title>";
    webpage += "<style>";
     webpage += "body { background-color: #E6E6FA; font-family: Arial, Helvetica, Sans-Serif; Color: blue;}";
    webpage += "</style>";
   webpage += "</head>";
   webpage += "<body>";
    webpage += "<h1><br>Fat Cat Fix - Scheduling</h1>";
    webpage += "<h2><br>Currently Set Feeding Time: ";
    webpage.concat(rawClientTime);
    webpage += "</h2>";
    webpage += "<h2><br>Currently Set Feeding Amount (&frac14; cups): ";
    webpage.concat(rawClientAmount);
    webpage += "</h2>";
    webpage += "<h2><br>Currently Set Feeding Delay (ms): ";
    webpage.concat(rawClientDelay);
    webpage += "</h2>";
    String IPaddress = WiFi.localIP().toString();
    webpage += "<form action='http://"+IPaddress+"/result' method='POST'>";
     webpage += "Feeding Time:<input type='text' name='time_input'><BR>";
     webpage += "Amount to feed (in &frac14;, e.g., enter \"4\" to feed 4 quarters, or 1 cup):<input type='text' name='amount_input'><BR>Motor Delay(ms)<input type='text' name='delay_input'>&nbsp;<input type='submit' value='Enter'>"; // omit <input type='submit' value='Enter'> for just 'enter'
    webpage += "</form><BR><BR><BR>";
    webpage += "<form action='http://"+IPaddress+"/feednow' method='POST'><input type='submit' value='Feed Now!'></form>";
   webpage += "</body>";
  webpage += "</html>";
  server.send(200, "text/html", webpage); // Send a response to the client asking for input
}

bool validateClientInput() {
  bool valid = true;
  if(strlen(rawClientTime) != 5 || rawClientTime[2] != ':') {
    Serial.println("Time must be of format XX:XX!");
    strcpy(rawClientTime, DEFAULT_FEEDING_TIME);
    valid = false;
  }
  if(rawClientAmount[0] == '0' || strlen(rawClientAmount) > 1 || !isdigit(rawClientAmount[0])) {
    Serial.println("Amount must be a non-zero integer from 1 - 9!");
    strcpy(rawClientAmount, DEFAULT_FEEDING_AMOUNT);
    valid = false;
  }
  if(rawClientDelay[0] == '0' || strlen(rawClientDelay) != 4 || !isdigit(rawClientAmount[0])) {
    Serial.println("Amount must be a 4 digit integer value!");
    strcpy(rawClientAmount, DEFAULT_DISPENSE_DELAY_MS);
    valid = false;
  }

  return valid;
}

void ClientFeedNow() {
  String webpage;
  webpage =  "<html>";
  webpage += "<meta http-equiv=\"refresh\" content=\"3;url=http://"+WiFi.localIP().toString()+"/\" />";
   webpage += "<head><title>Fat Cat Fix - Feed Now!</title>";
    webpage += "<style>";
     webpage += "body { background-color: #E6E6FA; font-family: Arial, Helvetica, Sans-Serif; Color: blue;}";
    webpage += "</style>";
   webpage += "</head>";
   webpage += "<body>";
   webpage += "<h1><br>Feeding ";
   webpage.concat(rawClientAmount);
  webpage += " quarter cups now!</h1>";
   webpage += "</body>";
  webpage += "</html>";
  server.send(200, "text/html", webpage);
  unsigned int quarterCups = convertCArrayToInt(rawClientAmount);
  feed(quarterCups);
}

void ShowClientResponse() {
  bool validInput = false;
  if (server.args() > 0 ) { // Arguments were received
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      Serial.print(server.argName(i)); // Display the argument
      Argument_Name = server.argName(i);
      if (server.argName(i) == "time_input") {
        Serial.print(" Input received was: ");
        Serial.println(server.arg(i));
        server.arg(i).toCharArray(rawClientTime, sizeof(rawClientTime));
        // e.g. range_maximum = server.arg(i).toInt();   // use string.toInt()   if you wanted to convert the input to an integer number
        // e.g. range_maximum = server.arg(i).toFloat(); // use string.toFloat() if you wanted to convert the input to a floating point number
      }
      if (server.argName(i) == "amount_input") {
        Serial.print(" Input received was: ");
        Serial.println(server.arg(i));
        server.arg(i).toCharArray(rawClientAmount, sizeof(rawClientAmount));
        // e.g. range_maximum = server.arg(i).toInt();   // use string.toInt()   if you wanted to convert the input to an integer number
        // e.g. range_maximum = server.arg(i).toFloat(); // use string.toFloat() if you wanted to convert the input to a floating point number
      }
      if (server.argName(i) == "delay_input") {
        Serial.print(" Input received was: ");
        Serial.println(server.arg(i));
        server.arg(i).toCharArray(rawClientDelay, sizeof(rawClientDelay));
        // e.g. range_maximum = server.arg(i).toInt();   // use string.toInt()   if you wanted to convert the input to an integer number
        // e.g. range_maximum = server.arg(i).toFloat(); // use string.toFloat() if you wanted to convert the input to a floating point number
      }
    }
    validInput = validateClientInput();
    if(validInput) {
      storeEEPROMValues();
    }

    requestDisplayUpdate = true;
  }
  String webpage;
  webpage =  "<html>";
  webpage += "<meta http-equiv=\"refresh\" content=\"5;url=http://"+WiFi.localIP().toString()+"/\" />";
   webpage += "<head><title>Fat Cat Fix - Data Received!</title>";
    webpage += "<style>";
     webpage += "body { background-color: #E6E6FA; font-family: Arial, Helvetica, Sans-Serif; Color: blue;}";
    webpage += "</style>";
   webpage += "</head>";
   webpage += "<body>";
    if(validInput) {
      webpage += "<h1><br>Fat Cat Fix - Data Received!</h1>";
    }
    else {
      webpage += "<h1><br>Oops! Looks like there was something wrong with your input. Using default values...</h1>";
    }
    webpage += "<p>New Feeding Time: ";
    webpage.concat(rawClientTime);
    webpage += "</p>";
    webpage += "<p>New Amount (quarter cups): ";
    webpage.concat(rawClientAmount);
    webpage += "</p>";
    webpage += "<p>New Delay (ms): ";
    webpage.concat(rawClientDelay);
    webpage += "</p>";
   webpage += "</body>";
  webpage += "</html>";
  server.send(200, "text/html", webpage);
}

void blink(){
    for(int i = 0; i < 3; i++) {
      display.clear();
      display.setCursor(0,0);
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.println("");
      display.setTextSize(4);
      display.println("=- -=");
      display.setTextSize(4);
      display.setCursor(0,30);
      display.println("  -  ");
      display.update();
      delay(50);
      display.clear();
      display.setCursor(0,0);
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.println("");
      display.setTextSize(4);
      display.println("=^ ^=");
      display.setTextSize(4);
      display.setCursor(0,30);
      display.println("  -  ");
      display.update();
      delay(2000);
  }
}

String getUTCTime() {
  WiFiClient client;
  const int httpPort = 13;

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return "Unable to Connect";
  }

  // This will send the request to the server
  client.print("HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Mozilla/4.0 (compatible; ESP8266 NodeMcu Lua;)\r\n\r\n");

  delay(100);
  String dateTime = "";

  while(client.available())
  {
    String line = client.readStringUntil('\r');

    if (line.indexOf("Date") != -1)
    {
      Serial.print("=====>");
    } else
    {
      dateTime = line.substring(7, 24);
      Serial.println(dateTime);
    }
  }
  return dateTime;
}

void updateDisplay(String timeString) {
  display.clear();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Feeding Time: " + String(rawClientTime));
  display.setCursor(0,15);
  display.println("Amount: " + String(rawClientAmount) + ((String(rawClientAmount).toInt() > 1) ? " q-cups" : " q-cup"));
  display.setCursor(0,40);
  display.println(WiFi.localIP());
  display.setCursor(0,55);
  display.println(timeString);
  display.update();
}

String convertUTCtoLocal(String nistTime) {
  String localTime = nistTime.substring(9,17);
  int hour = localTime.substring(0,2).toInt();
  int localHour = hour + UTC_OFFSET;
  if(localHour < 0) {
    localHour += 24;
  }
  char localHourChar[3];
  if(localHour > 9) {
    itoa(localHour,localHourChar,10);
  }
  else {
    localHourChar[0] = '0';
    itoa(localHour,localHourChar+1,10);
  }
  localTime.setCharAt(0, localHourChar[0]);
  localTime.setCharAt(1, localHourChar[1]);
  Serial.println("Converted Time: " + localTime);
  return localTime;
}


void motorStop() {
  digitalWrite( MOTOR_B_DIR, LOW );
  digitalWrite( MOTOR_B_PWM, LOW );
  delay( DIR_DELAY );
}

void motorForward() {
  digitalWrite(MOTOR_B_DIR, HIGH);
  analogWrite(MOTOR_B_PWM, 255-PWM_SPEED);
}

void dispenseQuarterCup() {
  unsigned int delayMS = convertCArrayToInt(rawClientDelay);
  motorStop();
  motorForward();
  delay(delayMS);
  motorStop();
}

unsigned int convertCArrayToInt(char array[]) {
  char *pEnd;
  unsigned int retVal;
  retVal = strtol(array, &pEnd, 10);
  return retVal;
}

void feed(unsigned int quarterCupsToFeed) {
  blink();
  for(int i = 0; i < quarterCupsToFeed; i++) {
    dispenseQuarterCup();
  }
  requestDisplayUpdate = true;
}

bool isFeedingTime(String localTime) {
  String timeNoSeconds = localTime.substring(0,5);
  String clientTime = String(rawClientTime);
  if(timeNoSeconds.equals(clientTime) && (millis() - lastFeedingTime > oneMinute)) {
    Serial.println("FEEDING TIME!");
    lastFeedingTime = millis();
    return true;
  }

  return false;
}

void loop(){
  ArduinoOTA.handle();
  server.handleClient();
  unsigned long currentLoopTime = millis();
  if(currentLoopTime - previousLoopTime >= loopDelay || previousLoopTime == 0 || requestDisplayUpdate) {
    if(requestDisplayUpdate) {
      requestDisplayUpdate = false;
    }
    previousLoopTime = currentLoopTime;
    Serial.print("connecting to ");
    Serial.println(host);
    String nistTime = getUTCTime();
    String localTime = convertUTCtoLocal(nistTime);
    if(isFeedingTime(localTime)) {
      unsigned int quarterCups = convertCArrayToInt(rawClientAmount);
      feed(quarterCups);
    }
    updateDisplay(localTime);
  }

}
