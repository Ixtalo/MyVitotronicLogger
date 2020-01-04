/*
 * MyVitotronicLogger
 * Vissmann boiler logger with DIY OptoLink interface.  
 * AST, 03/2019 (initial version)
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

// define an own macro for serial output which is only active
// if boolean debug variable is true
// this reduced the CPU time used for printing debug output
#define SER if (debug) Serial1

// debug mode activation switch
// it is set in setup() and can be set via hardware
// by wiring DEBUG_PIN to ground
#define DEBUG_PIN D1
bool debug = false; // possibly changed in setup()

// hostname/clientId
const char *deviceName = "ESP-VitoWiFi";

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

// OTA update settings
// #define OTA_HOST "192.168.99.1"
// #define OTA_PROTOCOL "http"
// #define OTA_PORT 8266
// #define OTA_PATH "firmware.bin"
// #define OTA_VERSION ""
#include "OtaSettings.h"


///------------------------------------------------------------------
/// Data Points
///------------------------------------------------------------------
/// Vitotronik 200 Typ KW1
/// https://github.com/openv/openv/wiki/Adressen
/// kd_list_nr2_20110719.pdf, "Datenpunktliste Vitotronic 200-H; 050 (Typ HK1W)_20AA"

// number of datapoints (must be identical to the defined variables)
const uint8_t numDp = 17;
// number of processed datapoints (will be set later while running)
uint8_t iDpProcessed = 0;

// Aussentemperatur Tiefpass [R]
// Currently calculated low pass outside temperature, Time constant 30 minutes.
DPTemp outsideTemp("outsideTemp", "temperature", 0x5525); // 2-Byte, -60..60
// 0x0800 2-byte ATS Aussentemperatur (Sensor 1)
DPTemp atsTemp("ATS", "temperature", 0x0800);
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
// 0x0802 2-byte KTS Kesseltemperatur (Sensor 3)
DPTemp ktsTemp("KTS", "temperature", 0x0802);
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

// 0x55E3 1-byte aktuelle Leistung %
// DOES NOT SEEM TO WORK, ALWAYS 255// DPMode currentPower("currentPower", "operation", 0x55E3);
// 0x5555 1-byte Drosselklappenposition (0..100%)
// DOES NOT SEEM TO WORK, ALWAYS 255// DPMode throttlePosition("throttlePosition", "operation", 0x5555);

// DOES NOT WORK DPTemp store2Temp("store2Temp", "temperature", 0x0813);  // 2-Byte, 0..150
// (equal to room temp) DPTemp outletTemp("outletTemp", "temperature", 0x0814);  // 2-Byte, 0..150
// DOES NOT WORK (-0.1) DPTemp setFlowTemp("setFlowTemp", "temperature", 0x2544);
// DOES NOT WORK (-0.1) DPTemp setBoilerTemp("setBoilerTemp", "temperature", 0x555A); // 2-Byte
// DOES NOT WORK (-0.1) DPTemp setExtRoomTemp("setExtRoomTemp", "temperature", 0x2321);
// DOES NOT WORK (255) DPMode operatingMode("operatingMode", "operation", 0x2323); // 1-Byte, 0..3

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

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
VitoWiFi_setProtocol(KW);
const ulong vitoTimeout = 10 * 1000UL; // max 10 sec 
bool vitoDoRead = false;
ulong tstart = 0;

// OTA signing
BearSSL::PublicKey signPubKey(pubkey);
BearSSL::HashSHA256 hash;
BearSSL::SigningVerifier sign(&signPubKey);

// OTA callbacks
void ota_update_started()
{
  SER.println("[OTA]  HTTP update process started");
}

void ota_update_finished()
{
  SER.println("[OTA]  HTTP update process finished");
}

void ota_update_progress(int cur, int total)
{
  SER.printf("\r[OTA]  HTTP update process at %d of %d bytes...", cur, total);
}

void ota_update_error(int err)
{
  SER.printf("[OTA]  HTTP update fatal error code %d\n", err);
}

bool mqtt_connect(const char *clientId, const uint8 n_attempts = 3)
{
  SER.print(F("Attempting MQTT connection..."));
  SER.flush();
  for (size_t i = 0; i < n_attempts; i++)
  {
    if (mqttClient.connected())
    {
      return true;
    }
    if (mqttClient.connect(clientId, MQTT_USER, MQTT_PASS))
    {
      SER.println(F("connected."));
    }
    else
    {
      SER.printf("failed, rc=%d\n", mqttClient.state());
      delay(1000);
    }
    SER.flush();
  }
  return false;
}

void globalCallbackHandler(const IDatapoint &dp, DPValue value)
{
  // get value as char
  char value_str[15] = {0};
  value.getString(value_str, sizeof(value_str));
  SER.printf("%s - %s is %s\n", dp.getGroup(), dp.getName(), value_str);

  // MQTT
  char topic[50];
  snprintf(topic, 50, "tele/VitoWiFi/%s", dp.getName());
  mqttClient.publish(topic, value_str);

  // increment processing counter
  SER.printf("iDpProcessed: %d\n", iDpProcessed);
  SER.flush();
  iDpProcessed++;
}

void ota_check()
{
  SER.println("OTA update check ...");
  Update.installSignature(&hash, &sign);
  ESPhttpUpdate.rebootOnUpdate(true);
  ESPhttpUpdate.onStart(ota_update_started);
  ESPhttpUpdate.onEnd(ota_update_finished);
  ESPhttpUpdate.onProgress(ota_update_progress);
  ESPhttpUpdate.onError(ota_update_error);
  
  // MDNS.setHostname(deviceName);
  // MDNS.begin(deviceName);

  char ota_url[255];
  sprintf(ota_url, "%s://%s:%d/%s", OTA_PROTOCOL, OTA_HOST, OTA_PORT, OTA_PATH);
  SER.printf("OTA url: %s\n", ota_url);

  wifiClient.setTimeout(2500);  // 2.5 sec
  if (!wifiClient.connect(OTA_HOST, OTA_PORT)) {
    SER.println("No TCP connection to OTA host!");
  } 
  else
  {
    t_httpUpdate_return ret = ESPhttpUpdate.update(wifiClient, ota_url, OTA_VERSION);
    switch (ret)
    case HTTP_UPDATE_FAILED:
    {
      SER.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      SER.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      SER.println("HTTP_UPDATE_OK");
      break;
    }
  }
  
  // MDNS.end();

  SER.println("OTA update check END.");
  SER.flush();
}

void cleanup_and_sleep()
{
  if (mqttClient.connected())
  {
    SER.println("MQTT disconnect ...");
    mqttClient.disconnect();
  }
  if (WiFi.isConnected())
    SER.println("WiFi disconnect ...");
    WiFi.disconnect();

  // 1e6 = 1.000.000 us = 1 second
  ulong sleepTime = 600 * 1e6; // 10 min
  if (debug)
  {
    sleepTime = 30 * 1e6; // 30 sec
    SER.printf("DeepSleep! (%.1f sec)\n", sleepTime / 1e6f);
    SER.flush();
  }

  // Deep sleep mode, the ESP8266 wakes up by itself when
  // GPIO 16 (D0) is connected to the RESET pin.
  // sleepTime in micro-seconds
  ESP.deepSleep(sleepTime);
  //
  // next awakening the ESP starts again with setup()
  //
}

void setupVitoWiFi()
{
  // VitoWiFi setup
  // this callback will be used for all DPs without specific callback
  // must be set after adding at least 1 datapoint
  VitoWiFi.setGlobalCallback(globalCallbackHandler);
  
  // use the main serial port
  // (debug output must go to Serial1)
  VitoWiFi.setup(&Serial);
  
  if (debug)
  {
    VitoWiFi.setLogger(&Serial1);
    VitoWiFi.enableLogger();
  }

  //
  // VitoWiFi reading continous in loop()
  //
}

void setup()
{
  // Use a DEBUG_PIN as hardware-defined debug on/off switch.
  // To enable serial output, connect GND and DEBUG_PIN (e.g. D1).
  // At startup, pins are configured as INPUT.
  // https://arduino-esp8266.readthedocs.io/en/latest/reference.html#digital-io
  pinMode(DEBUG_PIN, INPUT_PULLUP);
  debug = 1 - digitalRead(DEBUG_PIN);

  if (debug)
  {
    // Initialize Serial1 (Serial*ONE*) for logging/status output.
    // (not Serial/Serial0 because it is used for communication with boiler!)
    // Serial1 uses UART1 which is a transmit-only UART.
    // UART1 TX pin is D4 (GPIO2, LED!!).
    // UART1 can not be used to receive data because normally it’s RX pin is occupied for flash chip connection.
    // https://arduino-esp8266.readthedocs.io/en/latest/reference.html#serial
    SER.begin(115200);

    // diagnostic output from WiFi libraries
    SER.setDebugOutput(true);

    delay(1000);
    SER.printf("%s\n", ESP.getFullVersion().c_str());
    // https://arduino-esp8266.readthedocs.io/en/latest/boards.html#boot-messages-and-modes
    SER.printf("Boot mode: %d\n", ESP.getBootMode());
    SER.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
    SER.printf("tstart: %lu\n", tstart);
  }

  // start WiFi connection
  // (automatic reconnection tries is done by ESP8266WiFi)
  SER.println("Connecting WiFi ...");
  const ulong wifiConStart = millis();
  WiFi.mode(WIFI_STA);
  WiFi.hostname(deviceName);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // try to establish WiFi connection and get DHCP address
  // result: wl_status_t or -1 on timeout
  int8_t wifi_res = WiFi.waitForConnectResult(10000); // timeout 10 seconds (in milliseconds)
  SER.printf("WiFi connect result time: %lu\n", (millis() - wifiConStart));
  SER.printf("WiFi wifi_res: %d\n", wifi_res);

  if (WiFi.isConnected())
  {
    ota_check();

    // MQTT setup/connection
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    if (mqtt_connect(deviceName, 3))
    {
      SER.println(F("MQTT publish ..."));
      mqttClient.publish("tele/VitoWiFi/ip", WiFi.localIP().toString().c_str());
      mqttClient.publish("tele/VitoWiFi/rssi", String(WiFi.RSSI()).c_str());
      setupVitoWiFi();
      vitoDoRead = true;
    }
  } // wifi

  if (!vitoDoRead)
    // not handling VitoWiFi, probably because no WiFi or no MQTT
    cleanup_and_sleep();
}

void loop()
{
  if (vitoDoRead)
  {
    SER.println(F("Initialize Vitotronic reading..."));
    SER.flush();
    // initiate the reading for each defined datapoint
    VitoWiFi.readAll();
    vitoDoRead = false;
    tstart = millis();
  }

  if (iDpProcessed > numDp)
  {
    SER.println("All DPs proccessed.");
    cleanup_and_sleep();
  }
  else if (tstart > 0 && (millis() - tstart > vitoTimeout))
  {
    SER.println("Timeout hit! STOP.");
    SER.println(millis() - tstart);
    cleanup_and_sleep();
  }

  // this processes all datapoints, the previously definde callback
  // is being called for each defined datapoint
  VitoWiFi.loop();
}
