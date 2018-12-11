
/*
 * HTTP Client GET Request
 * Copyright (c) 2018, circuits4you.com
 * All rights reserved.
 * https://circuits4you.com 
 * Connects to WiFi HotSpot. */

#include "FS.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//------------------------------------------
//DS18B20
#define ONE_WIRE_BUS D3     //Pin to which is attached a temperature sensor
#define ONE_WIRE_MAX_DEV 15 //The maximum number of devices

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
int numberOfDevices;                     //Number of temperature devices found
DeviceAddress devAddr[ONE_WIRE_MAX_DEV]; //An array device temperature sensors

/* Credentials for local WiFi */
const char *ssid = "esp";
const char *password = "haslo8266";

/* IP of MeasureStation (STM32F7) */
String host = "http://192.168.0.102";

/* ID of sensor (ESP8266) */
String id;
int id_set = 0;

//=======================================================================
//                    Set temperature sensor
//=======================================================================
//Convert device id to String
String GetAddressToString(DeviceAddress deviceAddress)
{
  String str = "";
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      str += String(0, HEX);
    str += String(deviceAddress[i], HEX);
  }
  return str;
}

void SetupDS18B20()
{
  DS18B20.begin();

  Serial.print("Parasite power is: ");
  if (DS18B20.isParasitePowerMode())
  {
    Serial.println("ON");
  }
  else
  {
    Serial.println("OFF");
  }

  numberOfDevices = DS18B20.getDeviceCount();
  Serial.print("Device count: ");
  Serial.println(numberOfDevices);

  //lastTemp = millis();
  DS18B20.requestTemperatures();

  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++)
  {
    // Search the wire for address
    if (DS18B20.getAddress(devAddr[i], i))
    {
      //devAddr[i] = tempDeviceAddress;
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: " + GetAddressToString(devAddr[i]));
      Serial.println();
    }
    else
    {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }

    //Get resolution of DS18b20
    Serial.print("Resolution: ");
    Serial.print(DS18B20.getResolution(devAddr[i]));
    Serial.println();

    //Read temperature from DS18b20
    float tempC = DS18B20.getTempC(devAddr[i]);
    Serial.print("Temp C: ");
    Serial.println(tempC);
  }
}

//=======================================================================
//                    Power on setup
//=======================================================================

void setup()
{
  delay(1000);
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF); //Prevents reconnection issue (taking too long to connect)
  delay(1000);
  WiFi.mode(WIFI_STA); //This line hides the viewing of ESP as wifi hotspot

  WiFi.begin(ssid, password); //Connect to your WiFi router
  Serial.println("");

  Serial.print("Connecting");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
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

  //Setup DS18b20 temperature sensor
  SetupDS18B20();

  //-------------------------------------------------

  HTTPClient http; //Declare object of class HTTPClient
  String measure, Link;
  float tempC;
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

  /* Get measure from DS18B20 sensor */
  for (int i = 0; i < numberOfDevices; i++)
  {
    tempC = DS18B20.getTempC(devAddr[i]); //Measuring temperature in Celsius
  }
  DS18B20.setWaitForConversion(false); //No waiting for measurement
  DS18B20.requestTemperatures();       //Initiate the temperature measurement

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

  Serial.println("Going into deep sleep for 5 seconds");
  ESP.deepSleep(5e6); // 5e6 is 5 microseconds

}

//=======================================================================
//                    Main Program Loop
//=======================================================================
void loop()
{
 
}
//=======================================================================