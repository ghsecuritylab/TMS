
/*
 * HTTP Client GET Request
 * Copyright (c) 2018, circuits4you.com
 * All rights reserved.
 * https://circuits4you.com 
 * Connects to WiFi HotSpot. */

#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
//#include <WiFiClient.h>

/* Credentials for local WiFi */
const char *ssid = "esp";
const char *password = "haslo8266";

/* IP of MeasureStation (STM32F7) */
String host = "http://192.168.0.102";

/* ID of sensor (ESP8266) */
String id;

//=======================================================================
//                    Wifi connection
//=======================================================================
void connect() 
{
  delay(1000);
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF); //Prevents reconnection issue (taking too long to connect)
  delay(1000);
  WiFi.mode(WIFI_STA); //This line hides the viewing of ESP as wifi hotspot

  WiFi.begin(ssid, password); //Connect to your WiFi router
  Serial.println("");

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) // Wait for connection
  {
    delay(500);
    Serial.print(".");
  }

  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); //IP address assigned to your ESP
}
//=======================================================================
//                    Main with deep-sleep
//=======================================================================

void setup()
{
  connect();

  HTTPClient http; //Declare object of class HTTPClient
  String measure, Link;
  String path = "/id.txt";

  SPIFFS.begin();

  if (SPIFFS.exists(path))
  {
    //read id
    File f = SPIFFS.open(path, "r");
    id = f.readStringUntil('\n');
    f.close();
  }
  else
  {
    Link = host + String("/getid");
    http.begin(Link);
    int httpCode = http.GET();
    String payload = http.getString();
    id = payload; 
    http.end();

    Serial.println(id);
    
    //save id in file
    File f = SPIFFS.open(path, "w");
    
    if(!f){
      Serial.println("failed1");
    }
    f.println(id);
    f.close();  
  }

  /* Read temperature from A0 */
  int temp = analogRead(A0);
  double tempC = (((temp / 1024.0) * 3.2) - 0.5) * 100.0;
  // TODO: check format, trim to xx.xx
  Serial.println();
  Serial.print("Temperature C: ");
  Serial.println(tempC);

  /* Compose string to send via GET method */
  measure = String(tempC);
  if (tempC < 0)
  {
    measure = String("-") + measure;
  }
  else
  {
    measure = String("+") + measure;
  }
  Link = host + String("/id=") +  id.substring(0,2) +  String("/temp=") + measure;
  Serial.println(Link);
  http.begin(Link);          //Specify request destination
  int httpCode = http.GET(); //Send the request

  String payload = http.getString(); //Get the response payload
  Serial.println(httpCode);          //Print HTTP return code
  Serial.println(payload);           //Print request response payload

  http.end();  //Close connection

  Serial.println("Going into deep sleep for 10 seconds");
  ESP.deepSleep(10e6); // 10e6 is 10 microseconds
}

//=======================================================================
//                    Main Program Loop
//=======================================================================
void loop()
{
 
}
//=======================================================================