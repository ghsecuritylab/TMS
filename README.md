# TMS
Temperature Measure Station

# Sensor

### Energooszczędność

średnie natężenie prądu

Stan pracy  : 75 mA

Stan uśpienia: 3.1 mA

Quiescent Current: 5 mA dla 12 V  

AMS1117

3.3 LD 518

źródło: http://html.alldatasheet.com/html-pdf/205691/ADMOS/AMS1117-3.3/474/3/AMS1117-3.3.html

konfiguracja:
```c
/* Credentials for local WiFi */
const char *ssid = "esp";
const char *password = "haslo8266";

/* IP of MeasureStation (STM32F7) */
String host = "http://192.168.0.102"; // static routing

/* ID of sensor (ESP8266) */
String id;
int id_set = 0;
```

setup:
```c
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
```
# MeasureStation

Nasz projekt oparliśmy o projekt startowy dla płytki stm32f746. Po odpowiednim uporządkowaniu oraz oczyszczeniu kodu z niepotrzebnych nam funkcjonalności dodaliśmy potrzebne nam funkcje. Na początku zadeklarowaliśmy kilka potrzebnych nam zmiennych oraz zdefiniowaliśmy funkcje pomocnicze, które przydadzą się nam w dalszym projekcie.

```c
static char response[BUFFER_SIZE];
static char id[3];
static char sign;
static char temperatureIntegerPart[3];
static char temperatureDecimalPart[3];
static int sensorMinId = 1;

void clear_response_buffer()
{
  for (int i = 0; i < BUFFER_SIZE; i++)
  {
    response[i] = 0;
  }
}

void save_measurement(int id, double measurement)
{
  for (int i = MAX_MEASUREMENTS - 1; i > 0; i--)
  {
    measurements[id][i] = measurements[id][i - 1];
  }
  measurements[id][0] = measurement;
}
```

W projekcie startowym znajdował się juz szkic serwera http.
Ponizej przedstawiamy zmiany które zostały dokonane w istniejącym projekcie w funkcji http_server_serve.

```c

//based on available code examples
static void http_server_serve(struct netconn *conn)
{
  xprintf(".");
  struct netbuf *inbuf;
  err_t recv_err;
  char *buf;
  u16_t buflen;

  /* Read the data from the port, blocking if nothing yet there. 
   We assume the request (the part we care about) is in one netbuf */
  recv_err = netconn_recv(conn, &inbuf);

  if (recv_err == ERR_OK)
  {
    if (netconn_err(conn) == ERR_OK)
    {
      netbuf_data(inbuf, (void **)&buf, &buflen);
      void clear_response_buffer();
      /* Is this an HTTP GET command? is it request for machine id?*/
      if ((buflen >= 10) && (strncmp(buf, "GET /getid", 10) == 0))
      {
        strcpy(response, "HTTP/1.1 200 OK\r\n\
          Content-Type: text/html\r\n\
          Connection: close\r\n\n"
        );
		    char *id = malloc(sizeof(char) * 2);
        sprintf(id, "0%d", sensorMinId);
		    strcat(response, id);
		    sensorMinId += 1;
        netconn_write(conn, response, sizeof(response), NETCONN_NOCOPY);
      }

      /*
        Post the measure by get request the format is as follow 'GET /id=xx/temp=sxx.xx'
        We will parse the temperature for the machine of given id and then add it to measurements history
      */
      if ((buflen >= 21) && (strncmp(buf, "GET /id=", 8) == 0))
      {
        for (int i = 0; i < 2; i++)
        {
          id[i] = buf[8 + i];
        }
		    sign = buf[16];
        for (int i = 0; i < 2; i++)
        {
          temperatureIntegerPart[i] = buf[17 + i];
          temperatureDecimalPart[i] = buf[20 + i];
        }
        id[2] = 0;
        temperatureIntegerPart[2] = 0;
        temperatureDecimalPart[2] = 0;

        int machineId;
        sscanf(id, "%d", &machineId);
        int temperatureInteger;
        sscanf(temperatureIntegerPart, "%d", &temperatureInteger);
        int temperatureDecimal;
        sscanf(temperatureDecimalPart, "%d", &temperatureDecimal);

        double temperature;
        if (sign == '+') 
        {
          temperature = temperatureInteger + (temperatureDecimal / 100);
        }
        else if (sign == '-')
        {
          temperature = - temperatureInteger - (temperatureDecimal / 100);
        }
        else
        {
          temperature = -404; //error
        }
        save_measurement(machineId - 1, temperature);
        update_sensor_display(machineId, sign, temperatureInteger, temperatureDecimal);
        update_plot();

        response[0] = 0;
        strcpy(response, "HTTP/1.1 200 OK\r\n\
          Content-Type: text/html\r\n\
          Connection: close\r\n\n\
          Ok \r\n"
        );
        netconn_write(conn, response, sizeof(response), NETCONN_NOCOPY);
      }
    }
  }
  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);

  /* Delete the buffer (netconn_recv gives us ownership,
   so we have to make sure to deallocate the buffer) */
  netbuf_delete(inbuf);
}
```

Zgodnie z powyzszym kodem oraz zamieszczonymi w nim komentarzami, przygotowaliśmy nasz serwer do obsługi dwóch rodzajów komend: 
a) GET /getid - która zwraca id dla nowo podłączonej stacji pomiarowej
b) GET /id=xx/temp=sxx.xx - która pozwala na przekazanie pomiaru przy uzyciu wcześniej uzyskanego id oraz pomiaru w zdefiniowanym formacie. Gdzie s oznacza znak, a x jest dowolną cyfrą w systemie dziesiętnym. Do przesłania informacji uywawamy protokołu HTTP/1.1, a wiadomości przesyłane są jako text.
