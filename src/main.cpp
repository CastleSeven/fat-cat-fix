#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Adafruit_ssd1306syp.h>
#include <Wire.h>
#include <EEPROM.h>
#include <config.h>

#define EEPROM_SCHEMA 0xbb
#define EEPROM_TIME_ADDR 1

#define SDA_PIN D2
#define SCL_PIN D1
#define UTC_OFFSET -6

#define DEFAULT_FEEDING_TIME "17:30"
#define DEFAULT_FEEDING_AMOUNT "2"


const char* host = "time.nist.gov";

unsigned long previousLoopTime = 0;
unsigned int loopDelay = 30000;

String Argument_Name;
String clientTimeAsString = DEFAULT_FEEDING_TIME;
String clientAmountAsString = DEFAULT_FEEDING_AMOUNT;
bool requestDisplayUpdate = true;

ESP8266WebServer server(80);

Adafruit_ssd1306syp display(SDA_PIN,SCL_PIN);

void HandleClient();
void ShowClientResponse();

void setup(){
  Serial.begin(115200);
  delay(1000);
  display.initialize();
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

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
    schema = EEPROM_SCHEMA;
    EEPROM.write(0, schema);
    char timeBuffer[5];
    clientTimeAsString.toCharArray(timeBuffer, sizeof(timeBuffer));
    for(int i = 0; i < 5; i++) {
      EEPROM.write(EEPROM_TIME_ADDR+i, timeBuffer[i]);
    }
    char amountBuffer[1];
    clientAmountAsString.toCharArray(amountBuffer, sizeof(amountBuffer));
    EEPROM.write(EEPROM_TIME_ADDR+5, amountBuffer[0]);
    EEPROM.commit();
  } else {
    char timeBuffer[6];
    for(int i = 0; i < 5; i++) {
      timeBuffer[i] = EEPROM.read(EEPROM_TIME_ADDR+i);
    }
    timeBuffer[5] = '\0';
    char amountBuffer[2];
    amountBuffer[0] = EEPROM.read(EEPROM_TIME_ADDR+5);
    amountBuffer[1] = '\0';
    Serial.println("EEPROM Time: " + String(timeBuffer) + " EEPROM Amount: " + String(amountBuffer));
  }
  EEPROM.end();

  // Start the server
  server.begin();
  server.on("/", HandleClient);
  server.on("/result", ShowClientResponse);
  Serial.println("Server started");

}

void storeEEPROMValues() {
  Serial.println("Storing new values in EEPROM");
  EEPROM.begin(512);
  char timeBuffer[5];
  clientTimeAsString.toCharArray(timeBuffer, sizeof(timeBuffer));
  for(int i = 0; i < 5; i++) {
    EEPROM.write(EEPROM_TIME_ADDR+i, timeBuffer[i]);
  }
  char amountBuffer[1];
  clientAmountAsString.toCharArray(amountBuffer, sizeof(amountBuffer));
  EEPROM.write(EEPROM_TIME_ADDR+5, amountBuffer[0]);
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
    webpage += "<h1><br>ESP8266 Server - Getting input from a client</h1>";
    webpage += "<h2><br>Currently Set Feeding Time: "+clientTimeAsString+"</h2>";
    webpage += "<h2><br>Currently Set Feeding Amount (&frac14; cups): "+clientAmountAsString+"</h2>";
    String IPaddress = WiFi.localIP().toString();
    webpage += "<form action='http://"+IPaddress+"/result' method='POST'>";
     webpage += "Feeding Time:<input type='text' name='time_input'><BR>";
     webpage += "Amount to feed (in &frac14;, e.g., enter \"4\" to feed 4 quarters, or 1 cup):<input type='text' name='amount_input'>&nbsp;<input type='submit' value='Enter'>"; // omit <input type='submit' value='Enter'> for just 'enter'
    webpage += "</form>";
   webpage += "</body>";
  webpage += "</html>";
  server.send(200, "text/html", webpage); // Send a response to the client asking for input
}

bool validateClientInput() {
  bool valid = true;
  if(clientTimeAsString.length() != 5 || clientTimeAsString.charAt(2) != ':') {
    Serial.println("Time must be of format XX:XX!");
    clientTimeAsString = DEFAULT_FEEDING_TIME;
    valid = false;
  }
  if(clientAmountAsString.toInt() == 0 || clientAmountAsString.length() > 1) {
    Serial.println("Amount must be a non-zero integer from 1 - 9!");
    clientAmountAsString = DEFAULT_FEEDING_AMOUNT;
    valid = false;
  }

  return valid;
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
        clientTimeAsString = server.arg(i);
        // e.g. range_maximum = server.arg(i).toInt();   // use string.toInt()   if you wanted to convert the input to an integer number
        // e.g. range_maximum = server.arg(i).toFloat(); // use string.toFloat() if you wanted to convert the input to a floating point number
      }
      if (server.argName(i) == "amount_input") {
        Serial.print(" Input received was: ");
        Serial.println(server.arg(i));
        clientAmountAsString = server.arg(i);
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
    webpage += "<p>New Feeding Time: " + clientTimeAsString + "</p>";
    webpage += "<p>New Amount (quarter cups): " + clientAmountAsString + "</p>";
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
      Serial.print("Result: " + line);
      //  time starts at pos 14
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
  display.println("Feeding Time: " + clientTimeAsString);
  display.setCursor(0,15);
  display.println("Amount: " + clientAmountAsString + ((clientAmountAsString.toInt() > 1) ? " cups" : " cup"));
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

void loop(){
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
    updateDisplay(localTime);
  }

}
