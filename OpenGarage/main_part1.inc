/* OpenGarage Firmware
 *
 * Main loop
 * Mar 2016 @ OpenGarage.io
 *
 * This file is part of the OpenGarage library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

 #if defined(SERIAL_DEBUG)
	#define BLYNK_DEBUG
	#define BLYNK_PRINT Serial
#endif

#include "blynk_compat.h"
#include <DNSServer.h>
#include <PubSubClient.h>
#include <OpenThingsFramework.h>
#include <Request.h>
#include <Response.h>

#include "pitches.h"
#include "OpenGarage.h"
#include "espconnect.h"

OpenGarage og;
OTF::OpenThingsFramework *otf = NULL;
ESP8266WebServer *updateServer = NULL;
DNSServer *dns = NULL;

WidgetLED blynk_door(BLYNK_PIN_DOOR);
WidgetLED blynk_car(BLYNK_PIN_CAR);
WidgetLED blynk_dval(BLYNK_PIN_DVAL);

static Ticker aux_ticker;
static Ticker ip_ticker;
static Ticker restart_ticker;

static WiFiClient wificlient;
static WiFiClient httpclient;
PubSubClient mqttclient(wificlient);
String mqtt_topic;
String mqtt_id;

static String scanned_ssids;
static byte read_cnt = 0;
static uint distance = 0;
static byte sn2_value = 0;
static float tempC = 0;
static float humid = 0;
static byte door_status = DOOR_STATUS_UNKNOWN;
static byte last_door_status = DOOR_STATUS_UNKNOWN;
static byte secplus_door_status = DOOR_STATUS_UNKNOWN;
static byte door_status_hist = 0;
static bool light_status = 0;
static bool lock_status = 0;
static bool obstruction_status = 0;
static uint16_t opening_count = 0;
static int vehicle_status = OG_VEH_ABSENT;
static uint led_blink_ms = LED_FAST_BLINK;
static ulong justopen_timestamp = 0;
static byte curr_mode;
extern bool fullbuffer;
static ulong start_utc_time = 0;
static ulong curr_utc_time = 0;
static ulong curr_utc_hour= 0;
static HTTPClient http;
static bool light_blink_enabled = true;
SecPlus1::Garage secplus1_garage(PIN_SW_RX, PIN_SW_TX);
SecPlus2::Garage secplus2_garage(0x777, PIN_SW_RX, PIN_SW_TX);

void do_setup();

void otf_send_html_P(OTF::Response &res, const __FlashStringHelper *content) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("text/html"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	res.writeHeader(F("Content-Length"), strlen_P((char *) content));
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyData(content, strlen_P((char*)content));
}

void otf_send_compressed_P(OTF::Response& res, const __FlashStringHelper* content, size_t len) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("text/html; charset=utf-8"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	res.writeHeader(F("Content-Length"), len);
	res.writeHeader(F("Vary"), F("Accept-Encoding"));
	res.writeHeader(F("Content-Encoding"), F("gzip"));
	res.writeHeader(F("Cache-Control"), F("public, max-age=86400"));
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyData(content, len);
}

void otf_send_json(OTF::Response &res, String json) {
	res.writeStatus(200, F("OK"));
	res.writeHeader(F("Content-Type"), F("application/json"));
	res.writeHeader(F("Access-Control-Allow-Origin"), F("*"));
	res.writeHeader(F("Content-Length"), json.length());
	res.writeHeader(F("Connection"), F("close"));
	res.writeBodyData(json.c_str(), json.length());
}

void otf_send_result(OTF::Response &res, byte code, const char *item = NULL) {
	String json = F("{\"result\":"); json += code; if (!item) item = "";
	json += F(",\"item\":\""); json += item; json += F("\"}"); otf_send_json(res, json);
}

void updateserver_send_result(byte code, const char* item = NULL) {
	String json = F("{\"result\":"); json += code; if (!item) item = "";
	json += F(",\"item\":\""); json += item; json += F("\"}");
	updateServer->sendHeader("Access-Control-Allow-Origin", "*");
	updateServer->send(200, "application/json", json);
}

bool valid_url(String s) { return (s.length()>0 && s[0]!='-' && s.indexOf('.')>0); }
String ipString;

void report_ip() {
	static uint notes[] = {NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4};
	static byte note = 0; static byte digit = 0;
	if(digit == ipString.length()) { og.play_note(NOTE_C6); digit++; note=0; ip_ticker.once_ms(1000, report_ip); return; }
	else if(digit == ipString.length()+1) { og.play_note(0); note=0; digit=0; return; }
	char c = ipString.charAt(digit);
	if (c==' ') { og.play_note(0); digit++; note=0; ip_ticker.once_ms(1000, report_ip); }
	else if (c=='.') { og.play_note(NOTE_C5); digit++; note=0; ip_ticker.once_ms(500, report_ip); }
	else if (c>='0' && c<='9') {
		byte idx=9; if(c>='1') idx=c-'1';
		if(note==idx+1) { og.play_note(0); note++; ip_ticker.once_ms(1000, report_ip); }
		else if(note==idx+2) { digit++; note=0; ip_ticker.once_ms(100, report_ip); }
		else { og.play_note(notes[note]); note++; ip_ticker.once_ms(500, report_ip); }
	}
}

void restart_in(uint32_t ms) { if(og.state != OG_STATE_WAIT_RESTART) { og.state = OG_STATE_WAIT_RESTART; restart_ticker.once_ms(ms, og.restart); } }

void on_home(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode == OG_MOD_AP) otf_send_compressed_P(res, (const __FlashStringHelper*)ap_home_html_gz, ap_home_html_gz_len);
	else otf_send_compressed_P(res, (const __FlashStringHelper*)sta_home_html_gz, sta_home_html_gz_len);
}
void on_sta_view_options(const OTF::Request &req, OTF::Response &res) { otf_send_compressed_P(res, (const __FlashStringHelper*)sta_options_html_gz, sta_options_html_gz_len); }
void on_sta_view_logs(const OTF::Request &req, OTF::Response &res) { otf_send_compressed_P(res, (const __FlashStringHelper*)sta_logs_html_gz, sta_logs_html_gz_len); }

char dec2hexchar(byte dec) { return dec<10 ? '0'+dec : 'A'+(dec-10); }
String get_mac() {
	static String hex = ""; if(!hex.length()) { byte mac[6]; WiFi.macAddress(mac); for(byte i=0;i<6;i++) { hex += dec2hexchar((mac[i]>>4)&0x0F); hex += dec2hexchar(mac[i]&0x0F); if(i!=5) hex += ":"; } } return hex;
}
String get_ap_ssid() {
	static String ap_ssid = ""; if(!ap_ssid.length()) { byte mac[6]; WiFi.macAddress(mac); ap_ssid = "OG_"; for(byte i=3;i<6;i++) { ap_ssid += dec2hexchar((mac[i]>>4)&0x0F); ap_ssid += dec2hexchar(mac[i]&0x0F); } } return ap_ssid;
}
String get_ip() { IPAddress _ip=WiFi.localIP(); return String(_ip[0])+"."+_ip[1]+"."+_ip[2]+"."+_ip[3]; }

void sta_controller_fill_json(String& json) {
	json.reserve(STRING_RESERVE_SIZE); json = ""; json += F("{\"dist\":"); json += distance;
	if(og.options[OPTION_SN2].ival>OG_SN2_NONE) { json += F(",\"sn2\":"); json += sn2_value; }
	json += F(",\"secv\":"); json += og.options[OPTION_SECV].ival;
	json += F(",\"door\":"); json += door_status;
	json += F(",\"vehicle\":"); json += vehicle_status;
	json += F(",\"rcnt\":"); json += read_cnt;
	json += F(",\"fwv\":"); json += og.options[OPTION_FWV].ival;
	json += F(",\"has_swrx\":"); json += og.has_swrx;
	if(og.has_swrx && og.options[OPTION_SECV].ival>=1) {
		json += F(",\"light\":"); json += light_status; json += F(",\"lock\":"); json += lock_status; json += F(",\"obstruct\":"); json += obstruction_status;
		if(og.options[OPTION_SECV].ival==2) { json += F(",\"nopenings\":"); json += opening_count; }
		if(og.options[OPTION_SECV].ival==1) { json += F(",\"pemu\":"); json += secplus1_garage.get_panel_emu_status(); }
	}
	json += F(",\"name\":\""); json += og.options[OPTION_NAME].sval; json += F("\",\"mac\":\""); json += get_mac();
	json += F("\",\"cid\":"); json += ESP.getChipId(); json += F(",\"rssi\":"); json += (int16_t)WiFi.RSSI();
	json += F(",\"cld\":"); byte cld=og.options[OPTION_CLD].ival; json += cld;
	if(cld>CLOUD_NONE) { json += F(",\"clds\":"); if(cld==CLOUD_BLYNK) json += (Blynk.connected()?1:0); else if(cld==CLOUD_OTC) json += otf->getCloudStatus(); else json += "0"; }
	if(og.options[OPTION_TSN].ival) { json += F(",\"temp\":"); json += tempC; json += F(",\"humid\":"); json += humid; }
	json += F("}");
}

void on_sta_controller(const OTF::Request &req, OTF::Response &res) { if(curr_mode==OG_MOD_AP)return; String json; sta_controller_fill_json(json); otf_send_json(res,json); }
void on_sta_debug(const OTF::Request &req, OTF::Response &res) {
	String json=""; json.reserve(STRING_RESERVE_SIZE); json += F("{\"rcnt\":"); json += read_cnt; json += F(",\"fwv\":"); json += og.options[OPTION_FWV].ival;
	json += F(",\"name\":\""); json += og.options[OPTION_NAME].sval; json += F("\",\"mac\":\""); json += get_mac(); json += F("\",\"mqtt_topic\":\""); json += mqtt_topic;
	json += F("\",\"devip\":\""); json += WiFi.localIP().toString(); json += F("\",\"cid\":"); json += ESP.getChipId(); json += F(",\"rssi\":"); json += (int16_t)WiFi.RSSI();
	json += F(",\"bssid\":\""); json += WiFi.BSSIDstr(); json += F("\",\"build\":\""); json += F(__DATE__); json += F("\",\"Freeheap\":"); json += (uint16_t)ESP.getFreeHeap(); json += F(",\"flash_size\":"); json += (uint32_t)ESP.getFlashChipRealSize(); json += F(",\"has_swrx\":"); json += og.has_swrx; json += F("}"); otf_send_json(res,json);
}

void sta_logs_fill_json(String& json, OTF::Response &res) {
	json=""; json.reserve(2048); json += F("{\"name\":\""); json += og.options[OPTION_NAME].sval; json += F("\",\"starttime\":"); json += start_utc_time; json += F(",\"time\":"); json += curr_utc_time; json += F(",\"ncols\":"); json += og.options[OPTION_SN2].ival>OG_SN2_NONE?4:3; json += F(",\"logs\":[");
	if(!og.read_log_start()){json += F("]}");return;} LogStruct l;
	for(uint i=0;i<og.options[OPTION_LSZ].ival;i++){if(!og.read_log_next(l))break;if(!l.tstamp)continue;json += F("[");json += l.tstamp;json += F(",");json += l.status;json += F(",");json += l.dist;if(og.options[OPTION_SN2].ival>OG_SN2_NONE){json += F(",");json += l.sn2;}json += F("],");}
	og.read_log_end(); json.remove(json.length()-1); json += F("]}");
}
void on_sta_logs(const OTF::Request &req, OTF::Response &res){if(curr_mode==OG_MOD_AP)return;String json;sta_logs_fill_json(json,res);otf_send_json(res,json);}

bool verify_device_key(const OTF::Request &req){if(req.isCloudRequest())return true;const char *dkey=req.getQueryParameter("dkey");return dkey!=NULL&&strcmp(dkey,og.options[OPTION_DKEY].sval.c_str())==0;}
void on_reset_all(const OTF::Request &req, OTF::Response &res){if(!verify_device_key(req)){otf_send_result(res,HTML_UNAUTHORIZED,nullptr);return;}og.state=OG_STATE_RESET;otf_send_result(res,HTML_SUCCESS,nullptr);}
void on_clear_log(const OTF::Request &req, OTF::Response &res){if(!verify_device_key(req)){otf_send_result(res,HTML_UNAUTHORIZED,nullptr);return;}og.log_reset();otf_send_result(res,HTML_SUCCESS,nullptr);}

void secplus_update_door(SecPlusCommon::DoorStatus door_state){switch(door_state){case SecPlusCommon::DoorStatus::OPEN:secplus_door_status=DOOR_STATUS_OPEN;break;case SecPlusCommon::DoorStatus::CLOSED:secplus_door_status=DOOR_STATUS_CLOSED;break;case SecPlusCommon::DoorStatus::STOPPED:secplus_door_status=DOOR_STATUS_STOPPED;break;case SecPlusCommon::DoorStatus::OPENING:secplus_door_status=DOOR_STATUS_OPENING;break;case SecPlusCommon::DoorStatus::CLOSING:secplus_door_status=DOOR_STATUS_CLOSING;break;default:secplus_door_status=DOOR_STATUS_UNKNOWN;}}
void secplus1_state_callback(SecPlus1::state_struct_t state){secplus_update_door(state.door_state);light_status=state.light_state;lock_status=state.lock_state;obstruction_status=state.obstruction_state;}
void secplus2_state_callback(SecPlus2::state_struct_t state){secplus_update_door(state.door_state);light_status=state.light_state;lock_status=state.lock_state;obstruction_status=state.obstruction_state;opening_count=state.openings;}
int run_auto_detect(){if(secplus2_garage.detect())return 2;if(secplus1_garage.detect())return 1;return 0;}
void on_auto_detect(const OTF::Request &req, OTF::Response &res){String json="{\"version\": "+String(run_auto_detect())+"}";otf_send_json(res,json);}

void performDoorAction(uint8_t action,bool force_alarm_off=false){bool valid=false;if((og.options[OPTION_SECV].ival==2)||(action==ACTION_TOGGLE)||(action==ACTION_OPEN&&(door_status==DOOR_STATUS_CLOSED||door_status==DOOR_STATUS_CLOSING))||(action==ACTION_CLOSE&&(door_status==DOOR_STATUS_OPEN||door_status==DOOR_STATUS_STOPPED)))valid=true;if(!valid)return;bool alarm=true;if(force_alarm_off||!og.options[OPTION_ALM].ival||(og.options[OPTION_AOO].ival&&(door_status==DOOR_STATUS_CLOSED||door_status==DOOR_STATUS_CLOSING||door_status==DOOR_STATUS_OPENING)))alarm=false;if(alarm){if(og.options[OPTION_SECV].ival==2)og.set_alarm(0,action);else og.set_alarm();return;}switch(og.options[OPTION_SECV].ival){case 2:if(action==ACTION_OPEN)secplus2_garage.open_door();else if(action==ACTION_CLOSE)secplus2_garage.close_door();else secplus2_garage.toggle_door();break;case 1:secplus1_garage.toggle_door();break;default:og.click_relay();}}
void performLightAction(uint8_t action){if(action!=ACTION_TOGGLE)return;switch(og.options[OPTION_SECV].ival){case 2:secplus2_garage.toggle_light();break;case 1:secplus1_garage.toggle_light();break;}}
void performLockAction(uint8_t action){if(action!=ACTION_TOGGLE)return;switch(og.options[OPTION_SECV].ival){case 2:secplus2_garage.toggle_lock();break;case 1:secplus1_garage.toggle_lock();break;}}

void sta_change_controller_main(const OTF::Request &req, OTF::Response &res){if(curr_mode==OG_MOD_AP)return;if(!verify_device_key(req)){otf_send_result(res,HTML_UNAUTHORIZED,nullptr);return;}uint8_t action=255;if((bool)req.getQueryParameter("click"))action=ACTION_TOGGLE;else if((bool)req.getQueryParameter("close"))action=ACTION_CLOSE;else if((bool)req.getQueryParameter("open"))action=ACTION_OPEN;char* light=req.getQueryParameter("light");char* lock=req.getQueryParameter("lock");if(action<255){otf_send_result(res,HTML_SUCCESS,nullptr);performDoorAction(action);return;}if(light){otf_send_result(res,HTML_SUCCESS,nullptr);if(strcmp(light,"toggle")==0)performLightAction(ACTION_TOGGLE);return;}if(lock){otf_send_result(res,HTML_SUCCESS,nullptr);if(strcmp(lock,"toggle")==0)performLockAction(ACTION_TOGGLE);return;}if(req.getQueryParameter("reboot")!=NULL){otf_send_result(res,HTML_SUCCESS,nullptr);restart_in(1000);}else if(req.getQueryParameter("apmode")!=NULL){otf_send_result(res,HTML_SUCCESS,nullptr);og.reset_to_ap();}else otf_send_result(res,HTML_NOT_PERMITTED,nullptr);}
void on_sta_change_controller(const OTF::Request &req, OTF::Response &res){sta_change_controller_main(req,res);}

void sta_change_options_main(const OTF::Request &req, OTF::Response &res){
	if(curr_mode==OG_MOD_AP)return;
	if(!verify_device_key(req)){otf_send_result(res,HTML_UNAUTHORIZED,nullptr);return;}
	byte i; OptionStruct *o=og.options; byte usi=0;
	for(i=0;i<NUM_OPTIONS;i++,o++){
		const char *key=o->name.c_str();
