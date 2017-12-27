#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <Adafruit_ssd1306syp.h>
#include <Wire.h>


#define SDA_PIN D2
#define SCL_PIN D1

char ssid[] = "blackdiamond";
char pass[] = "$a112304%B061111";
const char* host = "time.nist.gov";

Adafruit_ssd1306syp display(SDA_PIN,SCL_PIN);

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
  display.clear();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.println("Local IP:");
  display.println(WiFi.localIP());
  delay(2000);
}

void blink(){
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
}

String getTime() {
  WiFiClient client;
  const int httpPort = 13;

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return "Unable to Connect";
  }

  // This will send the request to the server
  client.print("HEAD / HTTP/1.1\r\nAccept: */*\r\nUser-Agent: Mozilla/4.0 (compatible; ESP8266 NodeMcu Lua;)\r\n\r\n");

  delay(100);

  // Read all the lines of the reply from server and print them to Serial
  // expected line is like : Date: Thu, 01 Jan 2015 22:00:14 GMT
  char buffer[12];
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
      // dateTime.toCharArray(buffer, 10);
      // dateTime = line.substring(16, 24);
      // Serial.println(dateTime);
      // dateTime.toCharArray(buffer, 10);
    }
  }
  return dateTime;
}

void loop(){
  Serial.print("connecting to ");
  Serial.println(host);

  getTime();

  // Use WiFiClient class to create TCP connections
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
  display.println(WiFi.localIP());
  display.update();

  delay(5000);
  blink();
}
