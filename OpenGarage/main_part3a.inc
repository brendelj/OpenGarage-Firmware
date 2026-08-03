		if(curr_utc_hour==og.options[OPTION_ATIB].ival&&!automationclose_triggered){automationclose_triggered=true;if(atob&OG_AUTO_NOTIFY){String s=og.options[OPTION_NAME].sval+" is open after ";s+=og.options[OPTION_ATIB].ival;s+=" UTC. Current hour:";s+=curr_utc_hour;s+=(atob&OG_AUTO_CLOSE)?" It will be auto-closed shortly":" This is a reminder for you.";perform_notify(s);}if(atob&OG_AUTO_CLOSE){if(!og.options[OPTION_ALM].ival)og.set_alarm(OG_ALM_5);else og.set_alarm();}justopen_timestamp=0;}
		else if(curr_utc_hour>og.options[OPTION_ATIB].ival&&automationclose_triggered)automationclose_triggered=false;
	}} else justopen_timestamp=0;
}

byte check_door_event(){
	const byte allones=(1<<DOOR_STATUS_HIST_K)-1,lowones=(1<<(DOOR_STATUS_HIST_K/2))-1,highones=lowones<<(DOOR_STATUS_HIST_K/2);
	if(!og.options[OPTION_SECV].ival){byte h=door_status_hist&allones;if(h==0)return DOOR_EVENT_REMAIN_CLOSED;if(h==allones)return DOOR_EVENT_REMAIN_OPEN;if(h==lowones)return DOOR_EVENT_JUST_OPENED;if(h==highones)return DOOR_EVENT_JUST_CLOSED;return DOOR_EVENT_NONE;}
	if(door_status<DOOR_STATUS_UNKNOWN&&last_door_status<DOOR_STATUS_UNKNOWN){if(door_status==last_door_status){switch(door_status){case DOOR_STATUS_CLOSED:return DOOR_EVENT_REMAIN_CLOSED;case DOOR_STATUS_OPEN:return DOOR_EVENT_REMAIN_OPEN;case DOOR_STATUS_STOPPED:return DOOR_EVENT_REMAIN_STOPPED;case DOOR_STATUS_CLOSING:return DOOR_EVENT_STILL_CLOSING;case DOOR_STATUS_OPENING:return DOOR_EVENT_STILL_OPENING;}}else{switch(door_status){case DOOR_STATUS_CLOSED:return DOOR_EVENT_JUST_CLOSED;case DOOR_STATUS_OPEN:return DOOR_EVENT_JUST_OPENED;case DOOR_STATUS_STOPPED:return DOOR_EVENT_JUST_STOPPED;case DOOR_STATUS_CLOSING:return DOOR_EVENT_START_CLOSING;case DOOR_STATUS_OPENING:return DOOR_EVENT_START_OPENING;}}}return DOOR_EVENT_NONE;
}

void check_status(){
	static ulong checkstatus_timeout=0;if(!((curr_utc_time>checkstatus_timeout)||(checkstatus_timeout==0)))return;
	last_door_status=door_status;if(light_blink_enabled){og.set_led(HIGH);aux_ticker.once_ms(OG_LIGHT_BLINK_TIME,og.set_led,(byte)LOW);}
	uint dth=og.options[OPTION_DTH].ival,vth=og.options[OPTION_VTH].ival;bool sn1_status;distance=og.read_distance();
	if((distance==0||distance>500||!fullbuffer)&&og.options[OPTION_SNO].ival!=OG_SNO_2ONLY){checkstatus_timeout=curr_utc_time+og.options[OPTION_RIV].ival;return;}
	sn1_status=(distance>dth)?0:1;
	if(og.options[OPTION_SN1].ival==OG_SN1_SIDE){sn1_status=1-sn1_status;vehicle_status=OG_VEH_NOTAVAIL;}else{vehicle_status=OG_VEH_NOTAVAIL;if(vth>0){if(og.options[OPTION_SECV].ival>0||og.options[OPTION_SNO].ival==OG_SNO_2ONLY)vehicle_status=(distance<=vth)?OG_VEH_PRESENT:OG_VEH_ABSENT;else if(!sn1_status)vehicle_status=((distance>dth)&&(distance<=vth))?OG_VEH_PRESENT:OG_VEH_ABSENT;else vehicle_status=OG_VEH_UNKNOWN;}}
	sn2_value=og.get_switch();byte sn2_status=0;if(og.options[OPTION_SN2].ival==OG_SN2_NC)sn2_status=sn2_value;else if(og.options[OPTION_SN2].ival==OG_SN2_NO)sn2_status=1-sn2_value;
	switch(og.options[OPTION_SECV].ival){case 1:case 2:door_status=secplus_door_status;break;default:{bool status=false;if(og.options[OPTION_SN2].ival==OG_SN2_NONE||og.options[OPTION_SNO].ival==OG_SNO_1ONLY)status=sn1_status;else if(og.options[OPTION_SNO].ival==OG_SNO_2ONLY)status=sn2_status;else if(og.options[OPTION_SNO].ival==OG_SNO_AND)status=sn1_status&&sn2_status;else if(og.options[OPTION_SNO].ival==OG_SNO_OR)status=sn1_status||sn2_status;door_status=status?DOOR_STATUS_OPEN:DOOR_STATUS_CLOSED;break;}}
	if(checkstatus_timeout==0){door_status_hist=door_status?B11111111:B00000000;last_door_status=door_status;}else door_status_hist=(door_status_hist<<1)|door_status;
	og.read_TH_sensor(tempC,humid);read_cnt=(read_cnt+1)%100;
	byte blink_limit=og.options[OPTION_BAS].ival;bool oldblink=light_blink_enabled;light_blink_enabled=blink_limit==OG_LIGHT_BLINK_FOREVER||(light_blink_enabled&&read_cnt<=blink_limit);if(oldblink&&!light_blink_enabled){og.set_led(HIGH);aux_ticker.once_ms(OG_LIGHT_BLINK_NOTIFY,og.set_led,(byte)LOW);}
	byte event=check_door_event();
	if(event==DOOR_EVENT_JUST_OPENED||event==DOOR_EVENT_JUST_CLOSED||event==DOOR_EVENT_JUST_STOPPED){LogStruct l;l.tstamp=curr_utc_time;l.status=door_status;l.dist=distance;l.sn2=255;if(og.options[OPTION_SN2].ival>OG_SN2_NONE)l.sn2=sn2_value;og.write_log(l);}
	static ulong force_mqtt_update_timeout=0;
	if(curr_utc_time>force_mqtt_update_timeout||event==DOOR_EVENT_JUST_OPENED||event==DOOR_EVENT_JUST_CLOSED||event==DOOR_EVENT_JUST_STOPPED||event==DOOR_EVENT_START_OPENING||event==DOOR_EVENT_START_CLOSING){if(og.options[OPTION_MQEN].ival>0&&valid_url(og.options[OPTION_MQTT].sval)&&mqttclient.connected()){if(event==DOOR_EVENT_REMAIN_OPEN){mqttclient.publish((mqtt_topic+"/OUT/STATE").c_str(),"OPEN");mqttclient.publish(mqtt_topic.c_str(),"Open");}else if(event==DOOR_EVENT_REMAIN_CLOSED){mqttclient.publish((mqtt_topic+"/OUT/STATE").c_str(),"CLOSED");mqttclient.publish(mqtt_topic.c_str(),"Closed");}else if(event==DOOR_EVENT_REMAIN_STOPPED){mqttclient.publish((mqtt_topic+"/OUT/STATE").c_str(),"STOPPED");mqttclient.publish(mqtt_topic.c_str(),"Stopped");}String msg;sta_controller_fill_json(msg);mqttclient.publish((mqtt_topic+"/OUT/JSON").c_str(),msg.c_str());}force_mqtt_update_timeout=curr_utc_time+15;}
	if(og.options[OPTION_CLD].ival==CLOUD_BLYNK&&Blynk.connected()){
		static uint old_distance=0;static byte old_door_status=0xff,old_vehicle_status=0xff;static float old_tempC=-100,old_humid=-100;static byte old_light_status=255,old_lock_status=255;
		if(distance!=old_distance){Blynk.virtualWrite(BLYNK_PIN_DIST,distance);old_distance=distance;}
		if(door_status!=old_door_status){switch(door_status){case DOOR_STATUS_OPEN:blynk_door.setColor("#E02040");blynk_door.on();break;case DOOR_STATUS_STOPPED:blynk_door.setColor("#E0E000");blynk_door.on();break;case DOOR_STATUS_OPENING:blynk_door.setColor("#D000D0");blynk_door.on();break;case DOOR_STATUS_CLOSING:blynk_door.setColor("#20A0E0");blynk_door.on();break;case DOOR_STATUS_CLOSED:blynk_door.off();break;default:blynk_door.setColor("#808080");blynk_door.on();break;}blynk_dval.setValue(door_status);old_door_status=door_status;}
		if(vehicle_status!=old_vehicle_status){(vehicle_status==1)?blynk_car.on():blynk_car.off();old_vehicle_status=vehicle_status;}if(old_tempC!=tempC){Blynk.virtualWrite(BLYNK_PIN_TEMP,tempC);old_tempC=tempC;}if(old_humid!=humid){Blynk.virtualWrite(BLYNK_PIN_HUMID,humid);old_humid=humid;}if(old_light_status!=light_status){Blynk.virtualWrite(BLYNK_PIN_LIGHT,light_status);old_light_status=light_status;}if(old_lock_status!=lock_status){Blynk.virtualWrite(BLYNK_PIN_LOCK,lock_status);old_lock_status=lock_status;}
	}
	process_dynamics(event);checkstatus_timeout=curr_utc_time+og.options[OPTION_RIV].ival;
}

void time_keeping(){
	static bool configured=false;static ulong prev_millis=0,time_keeping_timeout=0;static unsigned char failed_ntp_calls=0;const unsigned char MAX_FAILED_NTP_CALLS=20;const ulong NTP_VALID_TIME=1577836800UL;
	if(!configured){if(valid_url(og.options[OPTION_NTP1].sval))configTime(0,0,og.options[OPTION_NTP1].sval.c_str(),DEFAULT_NTP1,DEFAULT_NTP2);else configTime(0,0,DEFAULT_NTP1,DEFAULT_NTP2,DEFAULT_NTP3);configured=true;delay(1000);}
	if(!curr_utc_time||curr_utc_time>time_keeping_timeout){ulong gt=time(nullptr);if(gt<NTP_VALID_TIME){if(failed_ntp_calls>=MAX_FAILED_NTP_CALLS)time_keeping_timeout=curr_utc_time+60;else{failed_ntp_calls++;time_keeping_timeout=curr_utc_time+2;}}else{failed_ntp_calls=0;curr_utc_time=gt;curr_utc_hour=(curr_utc_time%86400)/3600;time_keeping_timeout=curr_utc_time+TIME_SYNC_TIMEOUT;prev_millis=millis();if(!start_utc_time)start_utc_time=curr_utc_time-millis()/1000;}}
	while(millis()-prev_millis>=1000){curr_utc_time++;curr_utc_hour=(curr_utc_time%86400)/3600;prev_millis+=1000;}
}
