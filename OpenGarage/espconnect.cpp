/* OpenGarage Firmware
 *
 * ESPConnect functions
 * Mar 2016 @ OpenGarage.io
 */
#include "espconnect.h"

String scan_network() {
	DEBUG_PRINTLN(F("scan network"));
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	byte n = WiFi.scanNetworks();
	String wirelessinfo;
	if (n>32) n = 32;
	wirelessinfo.reserve(STRING_RESERVE_SIZE);
	wirelessinfo = "{\"ssids\":[";
	for(int i=0;i<n;i++) {
		wirelessinfo += "\"";
		wirelessinfo += WiFi.SSID(i);
		wirelessinfo += "\"";
		if(i<n-1) wirelessinfo += ",\r\n";
	}
	wirelessinfo += "],";
	wirelessinfo += "\"rssis\":[";
	for(int i=0;i<n;i++) {
		wirelessinfo += "\"";
		wirelessinfo += WiFi.RSSI(i);
		wirelessinfo += "\"";
		if(i<n-1) wirelessinfo += ",\r\n";
	}
	wirelessinfo += "]";
	return wirelessinfo;
}

void start_network_ap(const char *ssid, const char *pass) {
	if(!ssid) return;
	DEBUG_PRINTLN(F("AP mode"));
	if(pass) WiFi.softAP(ssid, pass);
	else WiFi.softAP(ssid);
	WiFi.mode(WIFI_AP_STA);
	WiFi.disconnect();
}

void start_network_sta(const char *ssid, const char *pass, const char *hostname, bool staonly) {
	if(!ssid || !pass) return;
	DEBUG_PRINTLN(F("Sarting start_network_sta"));
	if(staonly){
		DEBUG_PRINTLN(F("Setting STA mode"));
		if(WiFi.getMode() != WIFI_STA) WiFi.mode(WIFI_STA);
	}else{
		if(WiFi.getMode() != WIFI_AP_STA) WiFi.mode(WIFI_AP_STA);
		DEBUG_PRINTLN(F("Setting to AP+STA mode"));
	}
	if(hostname != NULL) WiFi.hostname(hostname);
	WiFi.begin(ssid, pass);
	WiFi.setSleep(false);
	WiFi.setOutputPower(20.5);
	WiFi.setAutoReconnect(true);
}

void start_network_sta_with_ap(const char *ssid, const char *pass, const char *hostname) {
	start_network_sta(ssid, pass, hostname, false);
}

void start_network_sta(const char *ssid, const char *pass, const char *hostname) {
	start_network_sta(ssid, pass, hostname, true);
}
