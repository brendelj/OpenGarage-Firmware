/* OpenGarage Firmware
 *
 * OpenGarage library
 * Mar 2016 @ OpenGarage.io
 */
#include "OpenGarage.h"

byte  OpenGarage::state = OG_STATE_INITIAL;
File  OpenGarage::log_file;
byte  OpenGarage::alarm = 0;
byte  OpenGarage::alarm_action = 0;
byte  OpenGarage::led_reverse = 0;
byte  OpenGarage::has_swrx = 0;
Ticker ud_ticker;

static const char* config_fname = CONFIG_FNAME;
static const char* log_fname = LOG_FNAME;

DallasTemperature* OpenGarage::ds18b20 = NULL;
DHTesp* OpenGarage::dht = NULL;
extern OpenGarage og;

OptionStruct OpenGarage::options[] = {
	{"fwv", OG_FWV,255,""},{"sn1",OG_SN1_CEILING,1,""},{"sn2",OG_SN2_NONE,2,""},{"sno",OG_SNO_1ONLY,3,""},{"secv",0,2,""},
	{"dth",50,65535,""},{"vth",150,65535,""},{"riv",1,30,""},{"alm",OG_ALM_5,2,""},{"aoo",0,1,""},
	{"lsz",DEFAULT_LOG_SIZE,400,""},{"tsn",OG_TSN_NONE,255,""},{"htp",80,65535,""},{"cdt",1000,5000,""},{"dri",500,3000,""},
	{"sfi",OG_SFI_CONSENSUS,1,""},{"cmr",10,100,""},{"sto",0,1,""},{"mod",OG_MOD_AP,255,""},{"ati",30,720,""},
	{"ato",OG_AUTO_NONE,255,""},{"atib",3,24,""},{"atob",OG_AUTO_NONE,255,""},{"noto",OG_NOTIFY_DO|OG_NOTIFY_DC,255,""},{"bas",OG_LIGHT_BLINK_FOREVER,OG_LIGHT_BLINK_MAX,""},
	{"usi",0,1,""},{"ssid",0,0,""},{"pass",0,0,""},{"cld",0,2,""},{"auth",0,0,""},
	{"bdmn",0,0,DEFAULT_BLYNK_DMN},{"bprt",DEFAULT_BLYNK_PRT,65535,""},{"dkey",0,0,DEFAULT_DKEY},{"name",0,0,DEFAULT_NAME},{"iftt",0,0,""},
	{"mqen",0,1,""},{"mqtt",0,0,""},{"mqpt",1883,65535,""},{"mqur",0,0,""},{"mqpw",0,0,""},{"mqtp",0,0,""},
	{"emen",0,1,""},{"smtp",0,0,DEFAULT_SMTP_SERVER},{"sprt",DEFAULT_SMTP_PORT,65535,""},{"send",0,0,""},{"apwd",0,0,""},{"recp",0,0,""},
	{"dvip",0,0,""},{"gwip",0,0,""},{"subn",0,0,"255.255.255.0"},{"dns1",0,0,"8.8.8.8"},{"ntp1",0,0,""},{"host",0,0,""},
};

#define KAVG 7
volatile uint32_t ud_start = 0;
volatile byte ud_i = 0;
volatile boolean fullbuffer = false;
volatile uint32_t ud_buffer[KAVG];
volatile boolean triggered = false;
static const uint32_t UD_TIMEOUT_US = 26000L;

void ud_start_trigger() {
	noInterrupts();
	if (triggered && og.options[OPTION_STO].ival != 0) {
		ud_buffer[ud_i] = UD_TIMEOUT_US;
		ud_i = (ud_i + 1) % KAVG;
		if (ud_i == 0) fullbuffer = true;
	}
	interrupts();
	digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
	digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(20);
	triggered = true;
	digitalWrite(PIN_TRIG, LOW);
}

IRAM_ATTR void ud_isr() {
	if(!triggered) return;
	if(digitalRead(PIN_ECHO)==HIGH) {
		ud_start = micros();
	} else {
		triggered = false;
		ud_buffer[ud_i] = micros() - ud_start;
		if(ud_buffer[ud_i]>UD_TIMEOUT_US) {
			if(og.options[OPTION_STO].ival==0) return;
			ud_buffer[ud_i]=UD_TIMEOUT_US;
		} else {
			ud_i = (ud_i+1)%KAVG;
			if(ud_i==0) fullbuffer=true;
		}
	}
}

void ud_ticker_cb() { ud_start_trigger(); }

void OpenGarage::begin() {
	digitalWrite(PIN_BUZZER, LOW); pinMode(PIN_BUZZER, OUTPUT);
	digitalWrite(PIN_RELAY, LOW); pinMode(PIN_RELAY, OUTPUT);
	has_swrx = 0;
	pinMode(PIN_SWRX_DETECT, INPUT_PULLUP);
	if(digitalRead(PIN_SWRX_DETECT) == 0) { digitalWrite(PIN_SW_RX, INPUT); has_swrx = 1; }
	pinMode(PIN_LED, INPUT);
	byte nl=0, nh=0;
	for(byte i=0;i<KAVG;i++) { if(digitalRead(PIN_LED)==HIGH) nh++; else nl++; delay(50); }
	if(nh>nl) led_reverse = 1;
	pinMode(PIN_LED, OUTPUT); set_led(LOW);
	digitalWrite(PIN_TRIG, HIGH); pinMode(PIN_TRIG, OUTPUT);
	pinMode(PIN_ECHO, INPUT); pinMode(PIN_BUTTON, INPUT_PULLUP); pinMode(PIN_SWITCH, INPUT_PULLUP);
	state = OG_STATE_INITIAL;
	if(!FILESYS.begin()) DEBUG_PRINTLN(F("failed to mount file system!"));
}

void OpenGarage::options_setup() {
	if(!FILESYS.exists(config_fname)) { options_save(); return; }
	options_load();
	if(options[OPTION_FWV].ival != OG_FWV) { options[OPTION_FWV].ival = OG_FWV; options_save(); }
}
void OpenGarage::options_reset() { if(!FILESYS.remove(config_fname)) return; }
void OpenGarage::log_reset() { if(!FILESYS.remove(log_fname)) return; }
int OpenGarage::find_option(String name) { for(byte i=0;i<NUM_OPTIONS;i++) if(name==options[i].name) return i; return -1; }

void OpenGarage::options_load() {
	File file = FILESYS.open(config_fname, "r"); if(!file) return;
	byte nopts=0;
	while(file.available()) {
		String name=file.readStringUntil(':'); String sval=file.readStringUntil('\n'); sval.trim();
		if(++nopts>NUM_OPTIONS+1) break;
		int idx=find_option(name); if(idx<0) continue;
		if(options[idx].max) options[idx].ival=sval.toInt(); else options[idx].sval=sval;
	}
	file.close();
}

void OpenGarage::options_save() {
	File file = FILESYS.open(config_fname, "w"); if(!file) return;
	OptionStruct *o=options;
	for(byte i=0;i<NUM_OPTIONS;i++,o++) { file.print(o->name + ":"); if(o->max) file.println(o->ival); else file.println(o->sval); }
	file.close();
}

uint OpenGarage::read_distance() {
	static uint last_returned; uint32_t buf[KAVG];
	if(!fullbuffer) { last_returned=(ud_i>0)?(uint)(ud_buffer[ud_i-1]*0.01716f):0; return last_returned; }
	for(byte i=0;i<KAVG;i++) buf[i]=ud_buffer[i];
	if(options[OPTION_SFI].ival==OG_SFI_MEDIAN) {
		for(byte out=1;out<=KAVG/2;out++) { uint32_t temp=buf[out]; byte in=out; while(in>0&&buf[in-1]>temp){buf[in]=buf[in-1];in--;} buf[in]=temp; }
		last_returned=(uint)(buf[KAVG/2]*0.01716f); return last_returned;
	}
	uint32_t vmin=buf[0],vmax=buf[0],sum=buf[0];
	for(byte i=1;i<KAVG;i++){uint32_t v=buf[i];vmin=(v<vmin)?v:vmin;vmax=(v>vmax)?v:vmax;sum+=v;}
	uint32_t margin=(float)options[OPTION_CMR].ival/0.01716f; margin=(margin<60)?60:margin;
	if(vmax-vmin<=margin) last_returned=(sum/KAVG)*0.01716f;
	return last_returned;
}

void OpenGarage::init_sensors() {
	ud_ticker.attach_ms(options[OPTION_DRI].ival, ud_ticker_cb); attachInterrupt(PIN_ECHO, ud_isr, CHANGE);
	switch(options[OPTION_TSN].ival) {
	case OG_TSN_DHT11: dht=new DHTesp(); dht->setup(PIN_TH,DHTesp::DHT11); break;
	case OG_TSN_DHT22: dht=new DHTesp(); dht->setup(PIN_TH,DHTesp::DHT22); break;
	case OG_TSN_DS18B20: { OneWire *oneWire=new OneWire(PIN_TH); ds18b20=new DallasTemperature(oneWire); ds18b20->begin(); break; }
	}
}

void OpenGarage::read_TH_sensor(float& C, float& H) {
	float v;
	switch(options[OPTION_TSN].ival) {
	case OG_TSN_DHT11: case OG_TSN_DHT22:
		if(dht){TempAndHumidity th=dht->getTempAndHumidity();v=th.temperature;if(!isnan(v))C=v;v=th.humidity;if(!isnan(v))H=v;} break;
	case OG_TSN_DS18B20:
		if(ds18b20){ds18b20->requestTemperatures();v=ds18b20->getTempCByIndex(0);if(!isnan(v))C=v;} break;
	}
}

void OpenGarage::write_log(const LogStruct& data) {
	File file; uint curr=0;
	if(!FILESYS.exists(log_fname)) {
		file=FILESYS.open(log_fname,"w"); if(!file) return;
		uint next=curr+1; file.write((const byte*)&next,sizeof(next)); file.write((const byte*)&data,sizeof(LogStruct));
		LogStruct l; l.tstamp=0; for(;next<MAX_LOG_SIZE;next++) file.write((const byte*)&l,sizeof(LogStruct));
	} else {
		file=FILESYS.open(log_fname,"r+"); if(!file) return;
		file.readBytes((char*)&curr,sizeof(curr)); uint next=(curr+1)%options[OPTION_LSZ].ival;
		file.seek(0,SeekSet); file.write((const byte*)&next,sizeof(next));
		file.seek(sizeof(curr)+curr*sizeof(LogStruct),SeekSet); file.write((const byte*)&data,sizeof(LogStruct));
	}
	file.close();
}

bool OpenGarage::read_log_start() { if(log_file)log_file.close(); log_file=FILESYS.open(log_fname,"r"); if(!log_file)return false; uint curr; if(log_file.readBytes((char*)&curr,sizeof(curr))!=sizeof(curr))return false; return curr<MAX_LOG_SIZE; }
bool OpenGarage::read_log_next(LogStruct& data) { if(!log_file)return false; return log_file.readBytes((char*)&data,sizeof(LogStruct))==sizeof(LogStruct); }
bool OpenGarage::read_log_end() { if(!log_file)return false; log_file.close(); return true; }
void OpenGarage::play_note(uint freq) { if(freq>0)tone(PIN_BUZZER,freq); else noTone(PIN_BUZZER); }

void OpenGarage::config_ip() {
	if(options[OPTION_USI].ival) {
		IPAddress dvip,gwip,subn,dns1;
		if(dvip.fromString(options[OPTION_DVIP].sval)&&gwip.fromString(options[OPTION_GWIP].sval)&&subn.fromString(options[OPTION_SUBN].sval)&&dns1.fromString(options[OPTION_DNS1].sval)) WiFi.config(dvip,gwip,subn,dns1,gwip);
	}
}

#include "pitches.h"
void OpenGarage::play_startup_tune() {
	static uint melody[]={NOTE_C4,NOTE_E4,NOTE_G4,NOTE_C5}; static byte duration[]={4,8,8,8};
	for(byte i=0;i<sizeof(melody)/sizeof(uint);i++){uint noteTime=1000/duration[i];tone(PIN_BUZZER,melody[i],noteTime);delay(noteTime*1.2f);noTone(PIN_BUZZER);}
}
