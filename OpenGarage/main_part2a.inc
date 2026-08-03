		// these options cannot be modified here
		if(i==OPTION_FWV || i==OPTION_MOD  || i==OPTION_SSID ||
			i==OPTION_PASS || i==OPTION_DKEY)
			continue;

		if(o->max) {
			char *sval = req.getQueryParameter(key);
			if(sval != NULL) {
				uint ival = String(sval).toInt();
				if(ival>o->max) { otf_send_result(res, HTML_DATA_OUTOFBOUND, key); return; }
				if(i==OPTION_DRI && ival < 50) { otf_send_result(res, HTML_DATA_OUTOFBOUND, key); return; }
				if(i==OPTION_RIV && ival < 1) { otf_send_result(res, HTML_DATA_OUTOFBOUND, key); return; }
				if(i==OPTION_LSZ && ival < 20) { otf_send_result(res, HTML_DATA_OUTOFBOUND, key); return; }
				if(i==OPTION_CDT && ival < 50) { otf_send_result(res, HTML_DATA_OUTOFBOUND, key); return; }
				if(i==OPTION_USI && ival==1) usi = 1;
			}
		}
	}

	const char* _dvip = "dvip"; const char* _gwip = "gwip"; const char* _subn = "subn"; const char* _dns1 = "dns1";
	String dvip = req.getQueryParameter(_dvip); String gwip = req.getQueryParameter(_gwip); String subn = req.getQueryParameter(_subn); String dns1 = req.getQueryParameter(_dns1);
	if(usi) {
		if(dvip != NULL) {
			if(gwip != NULL) {
				IPAddress ip;
				if(!ip.fromString(dvip)) { otf_send_result(res, HTML_DATA_FORMATERROR, _dvip); return; }
				if(!ip.fromString(gwip)) { otf_send_result(res, HTML_DATA_FORMATERROR, _gwip); return; }
				if(subn != NULL && !ip.fromString(subn)) { otf_send_result(res, HTML_DATA_FORMATERROR, _subn); return; }
				if(dns1 != NULL && !ip.fromString(dns1)) { otf_send_result(res, HTML_DATA_FORMATERROR, _dns1); return; }
			} else { otf_send_result(res, HTML_DATA_MISSING, _gwip); return; }
		} else { otf_send_result(res, HTML_DATA_MISSING, _dvip); return; }
	}
	const char* _nkey = "nkey"; const char* _ckey = "ckey";
	char* nkey = req.getQueryParameter(_nkey); char* ckey = req.getQueryParameter(_ckey);
	if(nkey != NULL) { if(ckey != NULL) { if(strcmp(nkey,ckey)!=0) { otf_send_result(res, HTML_MISMATCH, _ckey); return; } } else { otf_send_result(res, HTML_DATA_MISSING, _ckey); return; } }

	uint old_secv = og.options[OPTION_SECV].ival;
	o = og.options;
	for(i=0;i<NUM_OPTIONS;i++,o++) {
		const char *key = o->name.c_str();
		if(i==OPTION_FWV || i==OPTION_MOD || i==OPTION_SSID || i==OPTION_PASS || i==OPTION_DKEY) continue;
		char *sval = req.getQueryParameter(key);
		if(sval != NULL) { if(o->max) o->ival = String(sval).toInt(); else o->sval = String(sval); }
	}
	if(usi) { og.options[OPTION_DVIP].sval=dvip; og.options[OPTION_GWIP].sval=gwip; if(subn!=NULL) og.options[OPTION_SUBN].sval=subn; if(dns1!=NULL) og.options[OPTION_DNS1].sval=dns1; }
	if(nkey != NULL) og.options[OPTION_DKEY].sval = nkey;
	og.options_save();
	uint new_secv = og.options[OPTION_SECV].ival;
	if(old_secv != new_secv) {
		switch(new_secv) {
		case 2: secplus2_garage.begin(); secplus2_garage.reset_state(); secplus_door_status=DOOR_STATUS_UNKNOWN; secplus2_garage.enable_callback(secplus2_state_callback); break;
		case 1: secplus1_garage.begin(); secplus1_garage.reset_state(); secplus_door_status=DOOR_STATUS_UNKNOWN; secplus1_garage.enable_callback(secplus1_state_callback); break;
		}
	}
	otf_send_result(res, HTML_SUCCESS, nullptr);
}

void on_sta_change_options(const OTF::Request &req, OTF::Response &res) { sta_change_options_main(req, res); }

void sta_options_fill_json(String& json) {
	json.reserve(STRING_RESERVE_SIZE*2); json="{"; OptionStruct *o=og.options;
	for(byte i=0;i<NUM_OPTIONS;i++,o++) {
		if(!o->max) {
			if(i==OPTION_PASS || i==OPTION_DKEY || i==OPTION_MQPW) continue;
			json += F("\""); json += o->name; json += F("\":\"");
			if(o->sval.length()==0) { switch(i) { case OPTION_MQTP: json += og.options[OPTION_NAME].sval; break; case OPTION_HOST: json += get_ap_ssid(); break; } }
			else json += o->sval;
			json += F("\",");
		} else { json += F("\""); json += o->name; json += F("\":"); json += o->ival; json += ","; }
	}
	json += F("\"has_swrx\":"); json += og.has_swrx; json += F("}");
}

void on_sta_options(const OTF::Request &req, OTF::Response &res) { if(curr_mode==OG_MOD_AP)return; String json; sta_options_fill_json(json); otf_send_json(res,json); }
void on_ap_scan(const OTF::Request &req, OTF::Response &res) { if(curr_mode==OG_MOD_STA)return; otf_send_json(res,scanned_ssids); }

void on_ap_change_config(const OTF::Request &req, OTF::Response &res) {
	if(curr_mode==OG_MOD_STA)return;
	char *ssid=req.getQueryParameter("ssid");
	if(ssid!=NULL&&strlen(ssid)!=0) {
		og.options[OPTION_SSID].sval=ssid; og.options[OPTION_PASS].sval=req.getQueryParameter("pass"); og.options[OPTION_HOST].sval=req.getQueryParameter("host");
		char *cld=req.getQueryParameter("cld"); char *auth=req.getQueryParameter("auth");
		if(cld!=NULL&&strlen(cld)!=0&&auth!=NULL&&strlen(auth)!=0) {
			if(strcmp(cld,"blynk")==0||strcmp(cld,"otc")==0) {
				char *bdmn=req.getQueryParameter("bdmn"); char *bprt=req.getQueryParameter("bprt");
				if(strcmp(cld,"blynk")==0) { og.options[OPTION_BDMN].sval=(bdmn==NULL||strlen(bdmn)==0)?DEFAULT_BLYNK_DMN:bdmn; og.options[OPTION_BPRT].ival=(bprt==NULL||strlen(bprt)==0)?DEFAULT_BLYNK_PRT:String(bprt).toInt(); og.options[OPTION_CLD].ival=CLOUD_BLYNK; }
				else { og.options[OPTION_BDMN].sval=(bdmn==NULL||strlen(bdmn)==0)?DEFAULT_OTC_DMN:bdmn; og.options[OPTION_BPRT].ival=(bprt==NULL||strlen(bprt)==0)?DEFAULT_OTC_PRT:String(bprt).toInt(); og.options[OPTION_CLD].ival=CLOUD_OTC; }
				og.options[OPTION_AUTH].sval=auth; og.options_save();
			} else { otf_send_result(res,HTML_DATA_OUTOFBOUND,"cld"); return; }
		}
