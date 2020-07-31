/*

This example defines three datapoints.
The first two are TEMPL type datapoints and have their own callback.
When no specific callback is attached to a datapoint, it uses the global callback.

Note the difference in return value between the callbacks:
for tempCallback uses value.getFloat() as TEMPL datapoints return a float.
globalCallback uses value.getString(char*,size_t). This method is independent of the returned type.

*/

#include <VitoWiFi.h>

VitoWiFi_setProtocol(KW);


/// Vitotronik 200 Typ KW1
/// https://github.com/openv/openv/wiki/Adressen

DPTemp outsideTemp("outsideTemp", "temperature", 0x5525);  // 2-Byte, -60..60

// Kesseltemperatur
DPTemp boilerTemp("boilerTemp", "temperature", 0x0810);  // 2-Byte, 0..150

// WW-Temperatur (Speicher)
DPTemp store1Temp("STS1", "temperature", 0x0812);  // 2-Byte, 0..150

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
DPCount boilerRuns("starts", "burner", 0x088A); // 4-Byte, 0..1150000

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



unsigned long waitPeriod = 60 * 1000UL;
unsigned long lastMillis = 0;




/*
void tempCallbackHandler(const IDatapoint& dp, DPValue value) {
  //float fahrenheit = 0;
  //fahrenheit = (5.0 / 9) * (value.getFloat() + 32);
  Serial1.print(dp.getGroup());
  Serial1.print(" - ");
  Serial1.print(dp.getName());
  Serial1.print(": ");
  Serial1.println(value.getFloat(), 1);  // print with 1 decimal
}


void modeCallbackHandler(const IDatapoint& dp, DPValue value) {
  Serial1.print(dp.getGroup());
  Serial1.print(" - ");
  Serial1.print(dp.getName());
  Serial1.print(" is ");
  Serial1.println(value.getU8());
}
*/


void globalCallbackHandler(const IDatapoint& dp, DPValue value) {
  Serial1.print(dp.getGroup());
  Serial1.print(" - ");
  Serial1.print(dp.getName());
  Serial1.print(" is ");
  char value_str[15] = {0};
  value.getString(value_str, sizeof(value_str));
  Serial1.println(value_str);
}


void doReading() {
  Serial1.println(lastMillis);
  VitoWiFi.readAll();
}


void setup() {
  delay(5*1000);

  
  //outsideTemp.setCallback(tempCallbackHandler);
  //boilerTemp.setCallback(tempCallbackHandler);
  //operatingMode.setCallback(modeCallbackHandler);
  
  
  VitoWiFi.setGlobalCallback(globalCallbackHandler);  // this callback will be used for all DPs without specific callback
                                                      // must be set after adding at least 1 datapoint
  VitoWiFi.setup(&Serial);
  Serial1.begin(115200);
  Serial1.println(F("Setup finished..."));
  
  // initial reading
  delay(500);
  doReading();
}

void loop() {
  if (millis() - lastMillis > waitPeriod) {
    lastMillis = millis();
    doReading();
  }
  VitoWiFi.loop();
}
