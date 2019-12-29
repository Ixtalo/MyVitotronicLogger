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
#include <VitoWiFi.h>     // https://github.com/bertmelis/VitoWiFi.git
#include <ArduinoOTA.h>

// declare WIFI_SSID in external header file
// const char* WIFI_SSID = "...";
// const char* WIFI_PASS = "...";
#include "/home/stc/Work/SmartHomeWorkspace/WifiCredentials.h"

// MQTT
// const char* MQTT_HOST = "192.168.3.1";
// const unsigned int MQTT_PORT = 1883;
// const char* MQTT_USER = "...";
// const char* MQTT_PASS = "...";
#include "/home/stc/Work/SmartHomeWorkspace/SensorNodeMqttSettings.h"

// WiFi static IP configuration (instead of DHCP)
IPAddress staticIP(192, 168, 3, 8);
IPAddress gateway(192, 168, 3, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(gateway);

WiFiClient espClient;
PubSubClient client(espClient);
VitoWiFi_setProtocol(KW);



///------------------------------------------------------------------
/// Data Points
///------------------------------------------------------------------
/// Vitotronik 200 Typ KW1
/// https://github.com/openv/openv/wiki/Adressen
/// kd_list_nr2_20110719.pdf, "Datenpunktliste Vitotronic 200-H; 050 (Typ HK1W)_20AA"

// Aussentemperatur Tiefpass [R]
// Currently calculated low pass outside temperature, Time constant 30 minutes.
DPTemp outsideTemp("outsideTemp", "temperature", 0x5525); // 2-Byte, -60..60
// Kesseltemperatur Tiefpass [R]
DPTemp boilerTemp("boilerTemp", "temperature", 0x0810); // 2-Byte, 0..127
// WW-Temperatur (Speicher) [R]
DPTemp store1Temp("STS1", "temperature", 0x0812); // 2-Byte, 0..150
// Raumtemperatur A1M1 Tiefpass RTS [R]
DPTemp roomTemp("RTS", "temperature", 0x0896);   // 2-Byte, 0..127
// Abgastemperatur Tiefpass AGTS [R]
DPTemp agtsTemp("AGTS", "temperature", 0x0816);  // 2-Byte
// Rücklauftemperatur (17A) [R]
DPTemp rltsTemp("RLTS", "temperature", 0x080A);  // 2-Byte
// Vorlauftemperatur (17B) [R]
DPTemp vltsTemp("VLTS", "temperature", 0x080C);  // 2-Byte

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

// Raumtemperatur Soll
DPTemp setRoomTemp("setRoomTemp", "temperature", 0x2306); // 1-Byte, 3..37
// Reduizierte Raumtemperatur Soll [RW]
DPTemp setRoomReducedTemp("setRoomReducedTemp", "temperature", 0x2307); // 1-Byte, 3..37
// Betriebsstunden Stufe 1
DPHours boilerRunHours("runHours", "burner", 0x08A7); // 4-Byte, 0..1150000
// Brennerstarts [R]
DPCount boilerRuns("starts", "burner", 0x088A);       // 4-Byte, 0..1150000
// aktuelle Betriebsart A1M1 [R]; 0=Standby mode, 1=Reduced mode, 2=Standard mode, 3=Standard mode
DPMode currentOperatingMode("currentOperatingMode", "operation", 0x2500); // 1-Byte, 0..3
// Heizkreispumpe A1M1 [R]
DPStat aim1Pump("a1m1Pump", "pump", 0x2906);
// Speicherladepumpe [R]
DPStat storePump("storePump", "pump", 0x0845);
// Zirkulationspumpe [R]
DPStat circuitPump("circuitPump", "pump", 0x0846);

// DOES NOT WORK DPTemp store2Temp("store2Temp", "temperature", 0x0813);  // 2-Byte, 0..150
// (equal to room temp) DPTemp outletTemp("outletTemp", "temperature", 0x0814);  // 2-Byte, 0..150
// DOES NOT WORK (-0.1) DPTemp setFlowTemp("setFlowTemp", "temperature", 0x2544);
// DOES NOT WORK (-0.1) DPTemp setBoilerTemp("setBoilerTemp", "temperature", 0x555A); // 2-Byte
// DOES NOT WORK (-0.1) DPTemp setExtRoomTemp("setExtRoomTemp", "temperature", 0x2321);
// DOES NOT WORK (255) DPMode operatingMode("operatingMode", "operation", 0x2323); // 1-Byte, 0..3

/*
TODO

0x55E3 1-byte aktuelle Leistung %
0x5555 1-byte Drosselklappenposition (0..100%)
0x0800 2-byte ATS Aussentemperatur (Sensor 1)
0x0802 2-byte KTS Kesseltemperatur (Sensor 3)
0x0804 2-byte STS1 Speichertemperatur (Sensor 5)
AGTS Abgastemperatur (Sensor 15)

*/


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


///------------------------------------------------------------------
///------------------------------------------------------------------
///------------------------------------------------------------------



void trc(String msg)
{
  Serial1.print(msg);
}

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
      trc(F("\nIP address: "));
      trc(WiFi.localIP().toString());
      trc("\n");
      return true;
    }
    else
    {
      trc(".");
    }
  }
  return false;
}

bool mqtt_connect(uint8 n_attempts = 3)
{
  trc(F("Attempting MQTT connection..."));
  const String clientId = "VitoWiFi-" + String(ESP.getChipId());
  for (size_t i = 0; i < n_attempts; i++)
  {
    if (client.connected())
    {
      return true;
    }
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS))
    {
      trc(F("connected.\n"));
    }
    else
    {
      trc(F("failed, rc="));
      trc("" + client.state());
      trc("\n");
      delay(1000);
    }
  }
  return false;
}

void globalCallbackHandler(const IDatapoint &dp, DPValue value)
{
  trc(dp.getGroup());
  trc(" - ");
  trc(dp.getName());
  trc(" is ");
  char value_str[15] = {0};
  value.getString(value_str, sizeof(value_str));
  trc(value_str);
  trc("\n");

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
  // Serial1 uses UART1 which is a transmit-only UART.
  // UART1 TX pin is D4 (GPIO2, LED!!).
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
      VitoWiFi.setLogger(&Serial1);
      VitoWiFi.enableLogger();

      trc(F("Vitotronic reading...\n"));
      delay(500);
      VitoWiFi.readAll();

      // simulate Arduino "loop()" function
      // i.e., must give VitoWiFi a chance to process all datapoint queues
      const unsigned long tstart = millis();
      const unsigned long duration_sec = 5 * 1000UL;
      while (millis() - tstart <= duration_sec)
      {
        VitoWiFi.loop();
      }

      // OTA
      {
        ArduinoOTA.setPassword("sdjsa93274zfdh");
        ArduinoOTA.onStart([]() {
          trc(F("OTA start\n"));
          client.publish("/esp/VitoWiFi/ota", "start");
        });
        ArduinoOTA.onEnd([]() {
          trc(F("OTA end\n"));
          client.publish("/esp/VitoWiFi/ota", "end");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
          Serial1.printf("Progress: %u%%\r", (progress / (total / 100)));
        });
        ArduinoOTA.onError([](ota_error_t error) {
          Serial1.printf("Error[%u]: ", error);
          if (error == OTA_AUTH_ERROR)
            trc(F("Auth Failed"));
          else if (error == OTA_BEGIN_ERROR)
            trc(F("Begin Failed"));
          else if (error == OTA_CONNECT_ERROR)
            trc(F("Connect Failed"));
          else if (error == OTA_RECEIVE_ERROR)
            trc(F("Receive Failed"));
          else if (error == OTA_END_ERROR)
            trc(F("End Failed"));
          trc("\n");
          client.publish("/esp/VitoWiFi/ota", "error:" + error);
        });

        ArduinoOTA.begin();

        unsigned int timeout_OTA = 200; // 200*100 = 20000 msec = 20 sec
        while (timeout_OTA > 0)
        {
          ArduinoOTA.handle();
          delay(100);
          timeout_OTA--;
        }
      } // OTA

    } // mqtt

    delay(50);
    WiFi.disconnect();
  } // wifi

  delay(50);
  trc(F("DeepSleep!\n"));
  // micro seconds
  // 1e6 = 1.000.000 = 1 second
  // 6e8 = 10 * 60 * 1e6 = 600 sec = 10 min
  ESP.deepSleep(6e8); // 10 * 60 sec = 10 min
  //ESP.deepSleep(10 * 1e6); // 10 sec
}

void loop()
{
  // not used because of DeepSleep
}
