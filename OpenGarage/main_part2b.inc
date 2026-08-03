		char *secv=req.getQueryParameter("secv");
		if(og.has_swrx&&secv!=NULL&&strlen(secv)!=0){uint8_t v=String(secv).toInt();if(v>2)v=2;og.options[OPTION_SECV].ival=v;}
		otf_send_result(res,HTML_SUCCESS,nullptr); og.state=OG_STATE_TRY_CONNECT;
	} else otf_send_result(res,HTML_DATA_MISSING,"ssid");
}

void on_ap_try_connect(const OTF::Request &req, OTF::Response &res){if(curr_mode==OG_MOD_STA)return;String json="{\"ip\":";json+=(WiFi.status()==WL_CONNECTED)?(uint32_t)WiFi.localIP():0;json+="}";otf_send_json(res,json);if(WiFi.status()==WL_CONNECTED&&WiFi.localIP())restart_in(1000);}
void on_ap_debug(const OTF::Request &req, OTF::Response &res){String json="{\"dist\":";json+=og.read_distance();json+=",\"fwv\":";json+=og.options[OPTION_FWV].ival;json+=",\"has_swrx\":";json+=og.has_swrx;json+="}";otf_send_json(res,json);}

void mqtt_callback(char* topic, byte* payload, unsigned int length){payload[length]=0;String Payload((char*)payload);String Topic(topic);if(Payload=="Button"){performDoorAction(ACTION_TOGGLE);return;}if(Topic==(mqtt_topic+"/IN/STATE")){if(Payload=="click")performDoorAction(ACTION_TOGGLE);else if(Payload=="close")performDoorAction(ACTION_CLOSE);else if(Payload=="open")performDoorAction(ACTION_OPEN);else if(Payload=="togglelight")performLightAction(ACTION_TOGGLE);else if(Payload=="togglelock")performLockAction(ACTION_TOGGLE);return;}}

void do_setup(){
	Serial.begin(115200);
	if(otf){delete otf;otf=NULL;} if(updateServer){delete updateServer;updateServer=NULL;}
	WiFi.persistent(false); og.begin(); og.options_setup(); og.init_sensors(); if(og.get_mode()==OG_MOD_AP)og.play_startup_tune(); curr_mode=og.get_mode();
	switch(og.options[OPTION_SECV].ival){case 2:secplus2_garage.begin();secplus2_garage.reset_state();secplus2_garage.enable_callback(secplus2_state_callback);break;case 1:secplus1_garage.begin();secplus1_garage.reset_state();secplus1_garage.enable_callback(secplus1_state_callback);break;}
	if(!otf){const String key=og.options[OPTION_AUTH].sval;if((og.options[OPTION_CLD].ival==CLOUD_OTC)&&(key.length()>=32))otf=new OTF::OpenThingsFramework(og.options[OPTION_HTP].ival,og.options[OPTION_BDMN].sval,og.options[OPTION_BPRT].ival,key,false);else otf=new OTF::OpenThingsFramework(og.options[OPTION_HTP].ival);if(curr_mode==OG_MOD_AP)dns=new DNSServer();}
	if(!updateServer)updateServer=new ESP8266WebServer(8080); led_blink_ms=LED_FAST_BLINK;
}

void process_ui(){
	static ulong button_down_time=0;
	if(og.get_button()==LOW){if(!button_down_time)button_down_time=millis();else{ulong curr=millis();long diff=(long)(curr-button_down_time);if(diff>BUTTON_FACRESET_TIMEOUT){led_blink_ms=0;og.set_led(LOW);}else if(diff>BUTTON_APRESET_TIMEOUT){led_blink_ms=0;og.set_led(HIGH);}}}
	else if(button_down_time>0){ulong curr=millis();long diff=(long)(curr-button_down_time);if(diff>BUTTON_FACRESET_TIMEOUT)og.state=OG_STATE_RESET;else if(diff>BUTTON_APRESET_TIMEOUT)og.reset_to_ap();else if(diff>BUTTON_REPORTIP_TIMEOUT){ipString=get_ip();ipString.replace(".",". ");report_ip();}else if(curr>button_down_time+50)performDoorAction(ACTION_TOGGLE,true);button_down_time=0;}
	static ulong led_toggle_timeout=0;if(led_blink_ms&&(long)(millis()-led_toggle_timeout)>0){og.set_led(1-og.get_led());led_toggle_timeout=millis()+led_blink_ms;}
}

void on_update(const OTF::Request &req, OTF::Response &res){if(curr_mode==OG_MOD_AP)otf_send_compressed_P(res,(const __FlashStringHelper*)ap_update_html_gz,ap_update_html_gz_len);else{if(req.isCloudRequest())otf_send_result(res,HTML_NOT_PERMITTED,"fw update");otf_send_compressed_P(res,(const __FlashStringHelper*)sta_update_html_gz,sta_update_html_gz_len);}}

void on_firmware_upload_fin(){if(!(updateServer->hasArg("dkey")&&(updateServer->arg("dkey")==og.options[OPTION_DKEY].sval))){updateserver_send_result(HTML_UNAUTHORIZED);Update.end(false);return;}if(!Update.end(true)||Update.hasError()){updateserver_send_result(HTML_UPLOAD_FAILED);return;}updateserver_send_result(HTML_SUCCESS);restart_in(1000);}
void on_update_options(){updateServer->sendHeader("Access-Control-Allow-Origin","*");updateServer->sendHeader("Access-Control-Max-Age","10000");updateServer->sendHeader("Access-Control-Allow-Methods","POST,GET,OPTIONS");updateServer->sendHeader("Access-Control-Allow-Headers","Origin, X-Requested-With, Content-Type, Accept");updateServer->send(200,"text/plain","");}
void on_firmware_upload(){HTTPUpload& upload=updateServer->upload();if(upload.status==UPLOAD_FILE_START){if(curr_mode==OG_MOD_STA){WiFiUDP::stopAll();Blynk.disconnect();mqttclient.disconnect();}uint32_t maxSketchSpace=(ESP.getFreeSketchSpace()-0x1000)&0xFFFFF000;Update.begin(maxSketchSpace);}else if(upload.status==UPLOAD_FILE_WRITE)Update.write(upload.buf,upload.currentSize);else if(upload.status==UPLOAD_FILE_ABORTED)Update.end();delay(0);}

void check_status_ap(){static ulong cs_timeout=0;if((long)(millis()-cs_timeout)>0){Serial.print(og.read_distance());Serial.print("/");Serial.println(OG_FWV);if(og.has_swrx)Serial.println(F("secplus"));cs_timeout=millis()+2000;}}

bool mqtt_connect_subscribe(){
	static ulong timeout=0;if(curr_utc_time>timeout&&!mqttclient.connected()){
		boolean ret;if(og.options[OPTION_MQUR].sval.length()>0)ret=mqttclient.connect(mqtt_id.c_str(),og.options[OPTION_MQUR].sval.c_str(),og.options[OPTION_MQPW].sval.c_str(),(mqtt_topic+"/OUT/STATUS").c_str(),1,true,"offline");else ret=mqttclient.connect(mqtt_id.c_str(),(mqtt_topic+"/OUT/STATUS").c_str(),1,true,"offline");
		if(ret){mqttclient.subscribe(mqtt_topic.c_str());mqttclient.subscribe((mqtt_topic+"/IN/#").c_str());mqttclient.publish((mqtt_topic+"/OUT/STATUS").c_str(),"online",true);timeout=curr_utc_time+5;return true;}timeout=curr_utc_time+60;return false;
	}return false;
}

void blynkNotify(String s){og_blynk_notify(s);}
void iftttNotify(String s){http.begin(httpclient,"http://maker.ifttt.com/trigger/opengarage/with/key/"+og.options[OPTION_IFTT].sval);http.addHeader("Content-Type","application/json");http.POST("{\"value1\":\""+s+"\"}");http.getString();http.end();}
void emailNotify(String s){EMailSender::EMailMessage msg;msg.subject=og.options[OPTION_NAME].sval.c_str();msg.message=s;const char *host=og.options[OPTION_SMTP].sval.c_str(),*pwd=og.options[OPTION_APWD].sval.c_str(),*sender=og.options[OPTION_SEND].sval.c_str(),*recip=og.options[OPTION_RECP].sval.c_str();if(host&&pwd&&sender&&recip){EMailSender es(sender,pwd);es.setSMTPServer(host);es.setSMTPPort(og.options[OPTION_SPRT].ival);es.send(recip,msg);}}
void mqttNotify(String s){if(mqttclient.connected())mqttclient.publish((mqtt_topic+"/OUT/NOTIFY").c_str(),s.c_str());}
void perform_notify(String s){if(og.options[OPTION_CLD].ival==CLOUD_BLYNK&&Blynk.connected())blynkNotify(s);if(og.options[OPTION_IFTT].sval.length()>7)iftttNotify(s);if(og.options[OPTION_EMEN].ival>0)emailNotify(s);if(og.options[OPTION_MQEN].ival>0&&valid_url(og.options[OPTION_MQTT].sval))mqttNotify(s);}

void process_dynamics(byte event){
	static bool automationclose_triggered=false;byte ato=og.options[OPTION_ATO].ival,atob=og.options[OPTION_ATOB].ival,noto=og.options[OPTION_NOTO].ival;if(!ato&&!atob&&!noto){justopen_timestamp=0;return;}
	if(event==DOOR_EVENT_JUST_OPENED){justopen_timestamp=curr_utc_time;if(noto&OG_NOTIFY_DO)perform_notify(og.options[OPTION_NAME].sval+" just OPENED!");if(curr_utc_hour==og.options[OPTION_ATIB].ival&&!automationclose_triggered)automationclose_triggered=true;}
	else if(event==DOOR_EVENT_JUST_CLOSED){justopen_timestamp=0;if(noto&OG_NOTIFY_DC)perform_notify(og.options[OPTION_NAME].sval+" just CLOSED!");}
	else if(event==DOOR_EVENT_JUST_STOPPED){justopen_timestamp=0;if(noto&OG_NOTIFY_DS)perform_notify(og.options[OPTION_NAME].sval+" just STOPPED!");}
	else if(event==DOOR_EVENT_REMAIN_OPEN){if(!justopen_timestamp)justopen_timestamp=curr_utc_time;else{
		if(curr_utc_time>justopen_timestamp+(ulong)og.options[OPTION_ATI].ival*60L){if(ato&OG_AUTO_NOTIFY){String s=og.options[OPTION_NAME].sval+" is left open for more than ";s+=og.options[OPTION_ATI].ival;s+=" minutes.";s+=(ato&OG_AUTO_CLOSE)?" It will be auto-closed shortly":" This is a reminder for you.";perform_notify(s);}if(ato&OG_AUTO_CLOSE){if(!og.options[OPTION_ALM].ival)og.set_alarm(OG_ALM_5);else og.set_alarm();}justopen_timestamp=0;}
