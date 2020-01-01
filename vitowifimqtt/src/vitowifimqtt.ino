/*
 * MyVitotronicLogger
 * Vissmann boiler logger with DIY OptoLink interface.  
 * AST, 03/2019
 * 
 * Based on https://github.com/bertmelis/VitoWiFi.git
 * 
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h> // MQTT, https://pubsubclient.knolleary.net/
#include <VitoWiFi.h>     // https://github.com/bertmelis/VitoWiFi.git


// debug mode activation switch
// it is set in setup() and can be set via hardware 
// by wiring DEBUG_PIN to ground
#define DEBUG_PIN D1
bool debug = false;

// declare WIFI_SSID in external header file
// const char* WIFI_SSID = "...";
// const char* WIFI_PASS = "...";
#include "WifiCredentials.h"

// MQTT
// const char* MQTT_HOST = "192.168.3.1";
// const unsigned int MQTT_PORT = 1883;
// const char* MQTT_USER = "...";
// const char* MQTT_PASS = "...";
#include "SensorNodeMqttSettings.h"

// WiFi static IP configuration (instead of DHCP)
const IPAddress staticIP(192, 168, 3, 8);
const IPAddress gateway(192, 168, 3, 1);
const IPAddress subnet(255, 255, 255, 0);
const IPAddress dns(gateway);


// OTA update settings
#define OTA_URL "http://musca.local:8266/firmware.bin"
#define OTA_VERSION ""

// OTA signing
const char pubkey[] PROGMEM = R"EOF(
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAw41BEMOgGz2AktKMETlH
nVLDDxGr/CxRJWL4FEqJz5k2zLDWQdUQGUGsD0lYNS5mV3kz+W83U4Qh3VpHRlnn
J0/4yXOLo46KR3+MqlHXbiyOMBZdi0/i+OP8octQDGLb880hNhHt6Ix0CtdHKsaQ
ju6gIcKcz2VXbDV4AZ0qRqrj85vAx+Vl5j/dHOzQKJy43XHZD/RuEkjjjqsXIZe6
NdVKMGjNMYzRF1y7NME6PHQicIuFjd9pwdrlGWgYwdKIC5OVKrHrEG/DnQ0gH7lw
fLqKojGgTEJSBtZKEjcAM6VhnGJ5ra2KwcRnyOFbpHQBTRwPzSuLdxywWCyJ8Xmy
TQIDAQAB
-----END PUBLIC KEY-----
)EOF";
BearSSL::PublicKey signPubKey(pubkey);
BearSSL::HashSHA256 hash;
BearSSL::SigningVerifier sign( &signPubKey );

void ota_update_started()
{
  Serial1.println("[OTA]  HTTP update process started");
}

void ota_update_finished()
{
  Serial1.println("[OTA]  HTTP update process finished");
}

void ota_update_progress(int cur, int total)
{
  Serial1.printf("\r[OTA]  HTTP update process at %d of %d bytes...", cur, total);
}

void ota_update_error(int err)
{
  Serial1.printf("[OTA]  HTTP update fatal error code %d\n", err);
}


WiFiClient wifiClient;
PubSubClient client(wifiClient);
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
DPTemp roomTemp("RTS", "temperature", 0x0896); // 2-Byte, 0..127
// Abgastemperatur Tiefpass AGTS [R]
DPTemp agtsTemp("AGTS", "temperature", 0x0816); // 2-Byte
// Rücklauftemperatur (17A) [R]
DPTemp rltsTemp("RLTS", "temperature", 0x080A); // 2-Byte
// Vorlauftemperatur (17B) [R]
DPTemp vltsTemp("VLTS", "temperature", 0x080C); // 2-Byte

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
DPCount boilerRuns("starts", "burner", 0x088A); // 4-Byte, 0..1150000
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
  if (debug)
  {
    Serial1.print(msg);
  }
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
  // Use a DEBUG_PIN as hardware-defined debug on/off switch.
  // To enable serial output, connect GND and DEBUG_PIN (e.g. D1).
  // At startup, pins are configured as INPUT.
  // https://arduino-esp8266.readthedocs.io/en/latest/reference.html#digital-io
  pinMode(DEBUG_PIN, INPUT_PULLUP);
  debug = 1 - digitalRead(DEBUG_PIN);

  if (debug) {
    // Initialize Serial1 (Serial*ONE*) for logging/status output.
    // (not Serial/Serial0 because it is used for communication with boiler!)
    // Serial1 uses UART1 which is a transmit-only UART.
    // UART1 TX pin is D4 (GPIO2, LED!!).
    // UART1 can not be used to receive data because normally it’s RX pin is occupied for flash chip connection.
    // https://arduino-esp8266.readthedocs.io/en/latest/reference.html#serial
    Serial1.begin(115200);

    // diagnostic output from WiFi libraries
    //Serial1.setDebugOutput(true);

    delay(1000);
    Serial1.printf("DEBUG: %d\n", debug);
    Serial1.printf("%s\n", ESP.getFullVersion().c_str());
    // https://arduino-esp8266.readthedocs.io/en/latest/boards.html#boot-messages-and-modes
    Serial1.printf("Boot mode: %d\n", ESP.getBootMode());
    Serial1.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
    //////Serial.printf("Reset info: %s\n", ESP.getResetInfo().c_str());
    Serial1.flush();
  }


  // start WiFi connection
  // (automatic reconnection tries is done by ESP8266WiFi)
  WiFi.mode(WIFI_STA);
  WiFi.hostname("ESP-VitoWiFi");
  WiFi.begin(WIFI_SSID, WIFI_SSID);
  int8_t wifi_res = WiFi.waitForConnectResult(3e6); // timeout 3e6 = 3 seconds
  if (debug) Serial1.printf("WiFi wifi_res: %d\n", wifi_res);

  if (WiFi.status() == WL_CONNECTED)
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
      if (debug)
      {
        VitoWiFi.setLogger(&Serial1);
        VitoWiFi.enableLogger();
      }

      trc(F("Vitotronic reading...\n"));
      delay(500);
      VitoWiFi.readAll();

      // simulate Arduino "loop()" function
      // i.e., must give VitoWiFi a chance to process all datapoints
      const unsigned long tstart = millis();
      const unsigned long duration_sec = 5 * 1000UL;
      while (millis() - tstart <= duration_sec)
      {
        // this processes all datapoints, the previously definde callback
        // is being called for each defined datapoint
        VitoWiFi.loop();
      }

      // OTA
      {
        Update.installSignature(&hash, &sign);
        ESPhttpUpdate.rebootOnUpdate(true);
        ESPhttpUpdate.onStart(ota_update_started);
        ESPhttpUpdate.onEnd(ota_update_finished);
        ESPhttpUpdate.onProgress(ota_update_progress);
        ESPhttpUpdate.onError(ota_update_error);
        WiFiClient client;
        const char *hostname = "ESP-" + ESP.getChipId();
        MDNS.setHostname(hostname);
        MDNS.begin(hostname);
        t_httpUpdate_return ret = ESPhttpUpdate.update(client, OTA_URL, OTA_VERSION);
        switch (ret)
        {
        case HTTP_UPDATE_FAILED:
          Serial1.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial1.println("HTTP_UPDATE_NO_UPDATES");
          break;

        case HTTP_UPDATE_OK:
          Serial1.println("HTTP_UPDATE_OK");
          break;
        }
        Serial1.flush();
        delay(1000);
      } // OTA

    } // mqtt

    delay(50);
    WiFi.disconnect();
  } // wifi

  delay(50);
  trc(F("DeepSleep!\n"));
  // 1e6 = 1.000.000 micro senconds = 1 second
  // 6e8 = 10 * 60 * 1e6 = 600 sec = 10 min
  ESP.deepSleep(6e8);
}

void loop()
{
  // not used because of DeepSleep
}
