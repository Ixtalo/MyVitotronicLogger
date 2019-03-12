

MyVitotronicLogger
==================
Vissmann boiler logger with DIY OptoLink interface.  

AST, 12.03.2019


Motivation
----------
We have a Vissmann boiler with a Vitotronic 200 KW1 (V200KW1), Witterungsgeführte Kesselregelung für gleitend abgesenkte Kesselwassertemperatur.
* How much energy is consumed? 
* How to monitor the boiler's temperatures?
* What is the boiler doing over time? 
* How to interface with the boiler?


Solution Approach
-----------------
* Use the optical interface.
	* https://github.com/openv/openv/wiki/Die-Optolink-Schnittstelle
	* KW Protocol, https://github.com/openv/openv/wiki/Protokoll-KW
* Use Arduino ESP8266 with a DIY interface, see https://github.com/openv/openv/wiki/Bauanleitung-ESP8266
* Use fantastic VitoWiFi
	* Arduino Library for ESP8266 to communicate with Viessmann boilers using a (DIY) serial optolink.
	* https://github.com/bertmelis/VitoWiFi.git
	* based on fantastic OpenV library




Links
-----
* OpenV, https://github.com/openv/openv/wiki/
* https://www.harrykellner.de/index.php/projekte2/81-optolink
* https://www.edom-plc.pl/index.php/en/1-wire-i-rpi-en/175-komunikacja-z-viessmann-em-vitodens-200
* http://www.rainer-rebhan.de/proj_vvt_manager.html

