/*
 * MyVitotronicLogger
 * Vissmann boiler logger with DIY OptoLink interface.  
 * AST, 12.03.2019
 * Last update: 26.12.2019 (MQTT authentication)
 * 
 * Based on https://github.com/bertmelis/VitoWiFi.git
 * 
 */
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // MQTT, https://pubsubclient.knolleary.net/
#include <VitoWiFi.h> // https://github.com/bertmelis/VitoWiFi.git

// declare WIFI_SSID in external header file
// const char* WIFI_SSID = "...";
// const char* WIFI_PASS = "...";
#include "WifiCredentials.h"

// MQTT
// const char* MQTT_HOST = "192.168.3.1";
// const unsigned int MQTT_PORT = 1883;
// const char* MQTT_USER = "...";
// const char* MQTT_PASS = "...";
#include "EspKlingelMqttSettings.h"


// WiFi static IP configuration (instead of DHCP)
IPAddress staticIP(192, 168, 3, 9);
IPAddress gateway(192, 168, 3, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(gateway);




WiFiClient espClient;
PubSubClient client(espClient);
VitoWiFi_setProtocol(KW);

//unsigned long waitPeriod = 60 * 1000UL;
//unsigned long lastMillis = 0;

/// Vitotronik 200 Typ KW1
/// https://github.com/openv/openv/wiki/Adressen

DPTemp outsideTemp("outsideTemp", "temperature", 0x5525); // 2-Byte, -60..60

// Kesseltemperatur
DPTemp boilerTemp("boilerTemp", "temperature", 0x0810); // 2-Byte, 0..150

// WW-Temperatur (Speicher)
DPTemp store1Temp("STS1", "temperature", 0x0812); // 2-Byte, 0..150

DPTemp roomTemp("RTS", "temperature", 0x0896);
DPTemp agtsTemp("AGTS", "temperature", 0x0816);
DPTemp rltsTemp("RLTS", "temperature", 0x080A);
DPTemp vltsTemp("VLTS", "temperature", 0x080C);

// DOES NOT WORK DPTemp store2Temp("store2Temp", "temperature", 0x0813);  // 2-Byte, 0..150
// (equal to room temp) DPTemp outletTemp("outletTemp", "temperature", 0x0814);  // 2-Byte, 0..150
// DOES NOT WORK (-0.1) DPTemp setFlowTemp("setFlowTemp", "temperature", 0x2544);
// DOES NOT WORK (-0.1) DPTemp setBoilerTemp("setBoilerTemp", "temperature", 0x555A);

/*
room temp
22	309.4
21	258.1
18	257.8	283.4

reduced room temp
10	615.4
11	615.5
12	615.6
*/
DPTemp setRoomTemp("setRoomTemp", "temperature", 0x2306);
DPTemp setRoomReducedTemp("setRoomReducedTemp", "temperature", 0x2307);
// DOES NOT WORK (-0.1) DPTemp setExtRoomTemp("setExtRoomTemp", "temperature", 0x2321);

/**
 * TODO
 * SOLL Reduz. Raumtemperatur
 * SOLL WW-Temperatur
 **/

DPHours boilerRunHours("runHours", "burner", 0x08A7); // 4-Byte, 0..1150000
DPCount boilerRuns("starts", "burner", 0x088A);       // 4-Byte, 0..1150000

DPMode currentOperatingMode("currentOperatingMode", "operation", 0x2500); // 1-Byte, 0..3
// DOES NOT WORK (255) DPMode operatingMode("operatingMode", "operation", 0x2323); // 1-Byte, 0..3

DPStat aim1Pump("a1m1Pump", "pump", 0x2906);
DPStat storePump("storePump", "pump", 0x0845);
DPStat circuitPump("circuitPump", "pump", 0x0846);

/*
Example output:

temperature - boilerTemp is 32.4
temperature - STS1 is 48.5
temperature - RTS is 20.0
temperature - AGTS is 20.0
temperature - RLTS is 20.0
temperature - VLTS is 20.0
temperature - setRoomTemp is 309.2
temperature - setRoomReducedTemp is 513.2
burner - runHours is 10272.3
burner - starts is 199182
operation - currentOperatingMode is 2
pump - a1m1Pump is true
pump - storePump is false
pump - circuitPump is false

*/

bool setup_wifi(uint8 n_attempts = 3)
{
  WiFi.disconnect();
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.hostname("ESP-VitoWiFi");
  WiFi.config(staticIP, subnet, gateway, dns);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (size_t i = 0; i < n_attempts; i++)
  {
    delay(500);
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial1.println("");
      Serial1.print(F("IP address: "));
      Serial1.println(WiFi.localIP());
      return true;
    }
    else
    {
      Serial1.print(".");
    }
  }
  return false;
}

bool mqtt_connect(uint8 n_attempts = 3)
{
  Serial1.print(F("Attempting MQTT connection..."));
  const String clientId = "VitoWiFi-" + String(ESP.getChipId());
  for (size_t i = 0; i < n_attempts; i++)
  {
    if (client.connected())
    {
      return true;
    }
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS))
    {
      Serial1.println(F("connected"));
    }
    else
    {
      Serial1.print(F("failed, rc="));
      Serial1.println(client.state());
      delay(1000);
    }
  }
  return false;
}

void globalCallbackHandler(const IDatapoint &dp, DPValue value)
{
  Serial1.print(dp.getGroup());
  Serial1.print(" - ");
  Serial1.print(dp.getName());
  Serial1.print(" is ");
  char value_str[15] = {0};
  value.getString(value_str, sizeof(value_str));
  Serial1.println(value_str);

  // MQTT
  char topic[50];
  snprintf(topic, 50, "/esp/VitoWiFi/%s", dp.getName());
  client.publish(topic, value_str);
}

void setup()
{
  // initial grace wait time
  delay(2 * 1000);

  // initialize Serial1 (Serial*ONE*) for logging/status output
  // (not Serial/Serial0 because it is used for communication with boiler!)
  Serial1.begin(115200);

  // WiFi setup/connection
  if (setup_wifi(10))
  {

    // MQTT setup/connection
    client.setServer(MQTT_HOST, MQTT_PORT);
    if (mqtt_connect(3))
    {
      client.publish("/esp/VitoWiFi/ip", WiFi.localIP().toString().c_str());

      // VitoWiFi setup
      // this callback will be used for all DPs without specific callback
      // must be set after adding at least 1 datapoint
      VitoWiFi.setGlobalCallback(globalCallbackHandler);
      VitoWiFi.setup(&Serial);

      Serial1.println(F("Vitotronic reading..."));
      delay(500);
      VitoWiFi.readAll();

      // simulate Arduino "loop()" function
      // i.e., must give VitoWiFi a chance to process all datapoint queues
      const unsigned long tstart = millis();
      const unsigned long duration_sec = 3 * 1000UL;  // 3 seconds
      while (millis() - tstart <= duration_sec)
      {
        VitoWiFi.loop();
      }
    }
  }

  Serial1.println(F("DeepSleep..."));
  // micro seconds
  // 1e6 = 1.000.000 = 1 second
  // 6e8 = 10 * 60 * 1e6 = 600 sec = 10 min
  ESP.deepSleep(6e8);   // 10 * 60 sec = 10 min
  //ESP.deepSleep(10 * 1e6); // 10 sec
}

void loop()
{
  // not used because of DeepSleep
}
