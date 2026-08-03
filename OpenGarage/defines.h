/* OpenGarage Firmware
 *
 * OpenGarage macro defines and hardware pin assignments
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

#ifndef _DEFINES_H
#define _DEFINES_H

/** Firmware version, hardware version, and maximal values */
#define OG_FWV     124 // Firmware version: 124 means 1.2.4

/** GPIO pins */
#define PIN_RELAY  15 // NodeMCU D8. Relay for triggering door action (for non-Security+).
#define PIN_BUTTON  0
#define PIN_TRIG   12 // NodeMCU D6. Distance sensor trigger.
#define PIN_ECHO   14 // NodeMCU D5. Distance sensor echo.
#define PIN_LED     2
#define PIN_SW_TX  15 // NodeMCU D8. Software serial output (for Security+).
#define PIN_SW_RX   5 // NodeMCU D1. Software serial input (for Security+).
#define PIN_BUZZER 13 // NodeMCU D7. Buzzer.
#define PIN_SWITCH  4 // NodeMCU D2. Optional switch sensor.
#define PIN_TH      4 // NodeMCU D2. Optional temperature sensor.
#define PIN_SWRX_DETECT 10 // Hardware version detect (if software rx exists)

#define DEFAULT_NAME    "My OpenGarage"
#define DEFAULT_DKEY    "opendoor"
#define FILESYS         SPIFFS
#define CONFIG_FNAME    "/config.dat"
#define LOG_FNAME       "/log2.dat"

#define DEFAULT_NTP1    "time.google.com"
#define DEFAULT_NTP2    "time.cloudflare.com"
#define DEFAULT_NTP3    "time.windows.com"

#define DEFAULT_BLYNK_DMN  "blynk.openthings.io"
#define DEFAULT_BLYNK_PRT  8080
#define DEFAULT_OTC_DMN    "ws.cloud.openthings.io"
#define DEFAULT_OTC_PRT    80

#define DEFAULT_SMTP_SERVER "smtp.gmail.com"
#define DEFAULT_SMTP_PORT   465

#define STRING_RESERVE_SIZE 400

enum { OG_SN1_CEILING = 0, OG_SN1_SIDE, };
enum { OG_SN2_NONE = 0, OG_SN2_NC, OG_SN2_NO, };
enum { OG_SNO_1ONLY = 0, OG_SNO_2ONLY, OG_SNO_AND, OG_SNO_OR, };
enum { OG_SFI_MEDIAN = 0, OG_SFI_CONSENSUS, };
enum { OG_VEH_ABSENT = 0, OG_VEH_PRESENT, OG_VEH_UNKNOWN, OG_VEH_NOTAVAIL, };
enum { OG_ALM_NONE = 0, OG_ALM_5, OG_ALM_10, };
enum { OG_TSN_NONE = 0, OG_TSN_AM2320_RETIRED, OG_TSN_DHT11, OG_TSN_DHT22, OG_TSN_DS18B20, };
enum { OG_MOD_AP = 0xA9, OG_MOD_STA = 0x2A, };
enum { OG_AUTO_NONE = 0, OG_AUTO_NOTIFY = 1, OG_AUTO_CLOSE = 2, };
enum { OG_NOTIFY_NONE = 0x00, OG_NOTIFY_DO = 0x01, OG_NOTIFY_DC = 0x02, OG_NOTIFY_DS = 0x04, OG_NOTIFY_VL = 0x08, OG_NOTIFY_VA = 0x10, };
enum { OG_STATE_INITIAL = 0, OG_STATE_CONNECTING, OG_STATE_CONNECTED, OG_STATE_TRY_CONNECT, OG_STATE_WAIT_RESTART, OG_STATE_RESET = 9, };

#define OG_LIGHT_BLINK_FOREVER  0
#define OG_LIGHT_BLINK_MAX      99
#define OG_LIGHT_BLINK_TIME     1
#define OG_LIGHT_BLINK_NOTIFY   2000

enum { CLOUD_NONE = 0, CLOUD_BLYNK, CLOUD_OTC, };

#define BLYNK_PIN_DOOR  V0
#define BLYNK_PIN_RELAY V1
#define BLYNK_PIN_DVAL  V2
#define BLYNK_PIN_DIST  V3
#define BLYNK_PIN_CAR   V4
#define BLYNK_PIN_IP    V5
#define BLYNK_PIN_TEMP  V6
#define BLYNK_PIN_HUMID V7
#define BLYNK_PIN_LIGHT V8
#define BLYNK_PIN_LOCK  V9

#define DEFAULT_LOG_SIZE  100
#define MAX_LOG_SIZE      500
#define ALARM_FREQ       1000

enum { DOOR_STATUS_CLOSED = 0, DOOR_STATUS_OPEN, DOOR_STATUS_STOPPED, DOOR_STATUS_CLOSING, DOOR_STATUS_OPENING, DOOR_STATUS_UNKNOWN, };
#define DOOR_STATUS_HIST_K        4
enum { DOOR_EVENT_REMAIN_CLOSED = 0, DOOR_EVENT_REMAIN_OPEN, DOOR_EVENT_JUST_OPENED, DOOR_EVENT_JUST_CLOSED, DOOR_EVENT_NONE, DOOR_EVENT_REMAIN_STOPPED, DOOR_EVENT_JUST_STOPPED, DOOR_EVENT_STILL_OPENING, DOOR_EVENT_START_OPENING, DOOR_EVENT_STILL_CLOSING, DOOR_EVENT_START_CLOSING, };
enum { ACTION_TOGGLE = 0, ACTION_CLOSE, ACTION_OPEN };

typedef enum {
	OPTION_FWV = 0, OPTION_SN1, OPTION_SN2, OPTION_SNO, OPTION_SECV,
	OPTION_DTH, OPTION_VTH, OPTION_RIV, OPTION_ALM, OPTION_AOO,
	OPTION_LSZ, OPTION_TSN, OPTION_HTP, OPTION_CDT, OPTION_DRI,
	OPTION_SFI, OPTION_CMR, OPTION_STO, OPTION_MOD, OPTION_ATI,
	OPTION_ATO, OPTION_ATIB, OPTION_ATOB, OPTION_NOTO, OPTION_BAS,
	OPTION_USI, OPTION_SSID, OPTION_PASS, OPTION_CLD, OPTION_AUTH,
	OPTION_BDMN, OPTION_BPRT, OPTION_DKEY, OPTION_NAME, OPTION_IFTT,
	OPTION_MQEN, OPTION_MQTT, OPTION_MQPT, OPTION_MQUR, OPTION_MQPW,
	OPTION_MQTP, OPTION_EMEN, OPTION_SMTP, OPTION_SPRT, OPTION_SEND,
	OPTION_APWD, OPTION_RECP, OPTION_DVIP, OPTION_GWIP, OPTION_SUBN,
	OPTION_DNS1, OPTION_NTP1, OPTION_HOST, NUM_OPTIONS
} OG_OPTION_enum;

#define BUTTON_REPORTIP_TIMEOUT 800
#define BUTTON_APRESET_TIMEOUT  4500
#define BUTTON_FACRESET_TIMEOUT 9500
#define LED_FAST_BLINK  100
#define LED_SLOW_BLINK  500
#define TIME_SYNC_TIMEOUT 3600
#define TMP_BUFFER_SIZE 100

#if defined(SERIAL_DEBUG)
	#define DEBUG_PRINT(x)   Serial.print(x)
	#define DEBUG_PRINTLN(x) Serial.println(x)
#else
	#define DEBUG_PRINT(x)   {}
	#define DEBUG_PRINTLN(x) {}
#endif

typedef unsigned char byte;
typedef unsigned long ulong;
typedef unsigned int  uint;

#endif
