/*
 * Auto-Generated File, do not modify. Changes will be destroyed.
 * $Date: $
 * $Revision: $
 */
#include <config.h>
#include "common.h"
#include "sccp_enum.h"
#include "sccp_utils.h"

/* = Begin =======================================================================================       sparse sccp_channelstate === */

/*
 * SCCP Channel State
 */
static const char *sccp_channelstate_map[] = {"DOWN",
"ONHOOK",
"OFFHOOK",
"GETDIGITS",
"DIGITSFOLL",
"SPEEDDIAL",
"DIALING",
"RINGOUT",
"RINGING",
"PROCEED",
"PROGRESS",
"CONNECTED",
"CONNECTEDCONFERENCE",
"HOLD",
"CALLWAITING",
"CALLPARK",
"CALLREMOTEMULTILINE",
"CALLCONFERENCE",
"CALLTRANSFER",
"BLINDTRANSFER",
"DND",
"BUSY",
"CONGESTION",
"INVALIDNUMBER",
"INVALIDCONFERENCE",
"ZOMBIE",
};

int sccp_channelstate_exists(int sccp_channelstate_int_value) {
	static const int sccp_channelstates[] = {SCCP_CHANNELSTATE_DOWN,SCCP_CHANNELSTATE_ONHOOK,SCCP_CHANNELSTATE_OFFHOOK,SCCP_CHANNELSTATE_GETDIGITS,SCCP_CHANNELSTATE_DIGITSFOLL,SCCP_CHANNELSTATE_SPEEDDIAL,SCCP_CHANNELSTATE_DIALING,SCCP_CHANNELSTATE_RINGOUT,SCCP_CHANNELSTATE_RINGING,SCCP_CHANNELSTATE_PROCEED,SCCP_CHANNELSTATE_PROGRESS,SCCP_CHANNELSTATE_CONNECTED,SCCP_CHANNELSTATE_CONNECTEDCONFERENCE,SCCP_CHANNELSTATE_HOLD,SCCP_CHANNELSTATE_CALLWAITING,SCCP_CHANNELSTATE_CALLPARK,SCCP_CHANNELSTATE_CALLREMOTEMULTILINE,SCCP_CHANNELSTATE_CALLCONFERENCE,SCCP_CHANNELSTATE_CALLTRANSFER,SCCP_CHANNELSTATE_BLINDTRANSFER,SCCP_CHANNELSTATE_DND,SCCP_CHANNELSTATE_BUSY,SCCP_CHANNELSTATE_CONGESTION,SCCP_CHANNELSTATE_INVALIDNUMBER,SCCP_CHANNELSTATE_INVALIDCONFERENCE,SCCP_CHANNELSTATE_ZOMBIE,};
	uint32_t idx;
	for (idx=0; idx < ARRAY_LEN(sccp_channelstates); idx++) {
		if (sccp_channelstates[idx]==sccp_channelstate_int_value) {
			return 1;
		}
	}
	return 0;
}

const char * sccp_channelstate2str(sccp_channelstate_t enum_value) {
	switch(enum_value) {
		case SCCP_CHANNELSTATE_DOWN: return sccp_channelstate_map[0];
		case SCCP_CHANNELSTATE_ONHOOK: return sccp_channelstate_map[1];
		case SCCP_CHANNELSTATE_OFFHOOK: return sccp_channelstate_map[2];
		case SCCP_CHANNELSTATE_GETDIGITS: return sccp_channelstate_map[3];
		case SCCP_CHANNELSTATE_DIGITSFOLL: return sccp_channelstate_map[4];
		case SCCP_CHANNELSTATE_SPEEDDIAL: return sccp_channelstate_map[5];
		case SCCP_CHANNELSTATE_DIALING: return sccp_channelstate_map[6];
		case SCCP_CHANNELSTATE_RINGOUT: return sccp_channelstate_map[7];
		case SCCP_CHANNELSTATE_RINGING: return sccp_channelstate_map[8];
		case SCCP_CHANNELSTATE_PROCEED: return sccp_channelstate_map[9];
		case SCCP_CHANNELSTATE_PROGRESS: return sccp_channelstate_map[10];
		case SCCP_CHANNELSTATE_CONNECTED: return sccp_channelstate_map[11];
		case SCCP_CHANNELSTATE_CONNECTEDCONFERENCE: return sccp_channelstate_map[12];
		case SCCP_CHANNELSTATE_HOLD: return sccp_channelstate_map[13];
		case SCCP_CHANNELSTATE_CALLWAITING: return sccp_channelstate_map[14];
		case SCCP_CHANNELSTATE_CALLPARK: return sccp_channelstate_map[15];
		case SCCP_CHANNELSTATE_CALLREMOTEMULTILINE: return sccp_channelstate_map[16];
		case SCCP_CHANNELSTATE_CALLCONFERENCE: return sccp_channelstate_map[17];
		case SCCP_CHANNELSTATE_CALLTRANSFER: return sccp_channelstate_map[18];
		case SCCP_CHANNELSTATE_BLINDTRANSFER: return sccp_channelstate_map[19];
		case SCCP_CHANNELSTATE_DND: return sccp_channelstate_map[20];
		case SCCP_CHANNELSTATE_BUSY: return sccp_channelstate_map[21];
		case SCCP_CHANNELSTATE_CONGESTION: return sccp_channelstate_map[22];
		case SCCP_CHANNELSTATE_INVALIDNUMBER: return sccp_channelstate_map[23];
		case SCCP_CHANNELSTATE_INVALIDCONFERENCE: return sccp_channelstate_map[24];
		case SCCP_CHANNELSTATE_ZOMBIE: return sccp_channelstate_map[25];
		default:
			pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_channelstate2str\n", enum_value);
			return "SCCP: OutOfBounds Error during lookup of sparse sccp_channelstate2str\n";
	}
}

sccp_channelstate_t sccp_channelstate_str2val(const char *lookup_str) {
	if        (sccp_strcaseequals(sccp_channelstate_map[0], lookup_str)) {
		return SCCP_CHANNELSTATE_DOWN;
	} else if (sccp_strcaseequals(sccp_channelstate_map[1], lookup_str)) {
		return SCCP_CHANNELSTATE_ONHOOK;
	} else if (sccp_strcaseequals(sccp_channelstate_map[2], lookup_str)) {
		return SCCP_CHANNELSTATE_OFFHOOK;
	} else if (sccp_strcaseequals(sccp_channelstate_map[3], lookup_str)) {
		return SCCP_CHANNELSTATE_GETDIGITS;
	} else if (sccp_strcaseequals(sccp_channelstate_map[4], lookup_str)) {
		return SCCP_CHANNELSTATE_DIGITSFOLL;
	} else if (sccp_strcaseequals(sccp_channelstate_map[5], lookup_str)) {
		return SCCP_CHANNELSTATE_SPEEDDIAL;
	} else if (sccp_strcaseequals(sccp_channelstate_map[6], lookup_str)) {
		return SCCP_CHANNELSTATE_DIALING;
	} else if (sccp_strcaseequals(sccp_channelstate_map[7], lookup_str)) {
		return SCCP_CHANNELSTATE_RINGOUT;
	} else if (sccp_strcaseequals(sccp_channelstate_map[8], lookup_str)) {
		return SCCP_CHANNELSTATE_RINGING;
	} else if (sccp_strcaseequals(sccp_channelstate_map[9], lookup_str)) {
		return SCCP_CHANNELSTATE_PROCEED;
	} else if (sccp_strcaseequals(sccp_channelstate_map[10], lookup_str)) {
		return SCCP_CHANNELSTATE_PROGRESS;
	} else if (sccp_strcaseequals(sccp_channelstate_map[11], lookup_str)) {
		return SCCP_CHANNELSTATE_CONNECTED;
	} else if (sccp_strcaseequals(sccp_channelstate_map[12], lookup_str)) {
		return SCCP_CHANNELSTATE_CONNECTEDCONFERENCE;
	} else if (sccp_strcaseequals(sccp_channelstate_map[13], lookup_str)) {
		return SCCP_CHANNELSTATE_HOLD;
	} else if (sccp_strcaseequals(sccp_channelstate_map[14], lookup_str)) {
		return SCCP_CHANNELSTATE_CALLWAITING;
	} else if (sccp_strcaseequals(sccp_channelstate_map[15], lookup_str)) {
		return SCCP_CHANNELSTATE_CALLPARK;
	} else if (sccp_strcaseequals(sccp_channelstate_map[16], lookup_str)) {
		return SCCP_CHANNELSTATE_CALLREMOTEMULTILINE;
	} else if (sccp_strcaseequals(sccp_channelstate_map[17], lookup_str)) {
		return SCCP_CHANNELSTATE_CALLCONFERENCE;
	} else if (sccp_strcaseequals(sccp_channelstate_map[18], lookup_str)) {
		return SCCP_CHANNELSTATE_CALLTRANSFER;
	} else if (sccp_strcaseequals(sccp_channelstate_map[19], lookup_str)) {
		return SCCP_CHANNELSTATE_BLINDTRANSFER;
	} else if (sccp_strcaseequals(sccp_channelstate_map[20], lookup_str)) {
		return SCCP_CHANNELSTATE_DND;
	} else if (sccp_strcaseequals(sccp_channelstate_map[21], lookup_str)) {
		return SCCP_CHANNELSTATE_BUSY;
	} else if (sccp_strcaseequals(sccp_channelstate_map[22], lookup_str)) {
		return SCCP_CHANNELSTATE_CONGESTION;
	} else if (sccp_strcaseequals(sccp_channelstate_map[23], lookup_str)) {
		return SCCP_CHANNELSTATE_INVALIDNUMBER;
	} else if (sccp_strcaseequals(sccp_channelstate_map[24], lookup_str)) {
		return SCCP_CHANNELSTATE_INVALIDCONFERENCE;
	} else if (sccp_strcaseequals(sccp_channelstate_map[25], lookup_str)) {
		return SCCP_CHANNELSTATE_ZOMBIE;
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_channelstate_str2val(%s) not found\n", lookup_str);
	return SCCP_CHANNELSTATE_SENTINEL;
}

int sccp_channelstate_str2intval(const char *lookup_str) {
	int res = sccp_channelstate_str2val(lookup_str);
	return (int)res != SCCP_CHANNELSTATE_SENTINEL ? res : -1;
}

char *sccp_channelstate_all_entries(void) {
	static char res[] = "DOWN,ONHOOK,OFFHOOK,GETDIGITS,DIGITSFOLL,SPEEDDIAL,DIALING,RINGOUT,RINGING,PROCEED,PROGRESS,CONNECTED,CONNECTEDCONFERENCE,HOLD,CALLWAITING,CALLPARK,CALLREMOTEMULTILINE,CALLCONFERENCE,CALLTRANSFER,BLINDTRANSFER,DND,BUSY,CONGESTION,INVALIDNUMBER,INVALIDCONFERENCE,ZOMBIE";
	return res;
}
/* = End =========================================================================================       sparse sccp_channelstate === */


/* = Begin =======================================================================================        sccp_channelstatereason === */

/*
 * \brief internal chan_sccp call state (c->callstate) (Enum)
 */
static const char *sccp_channelstatereason_map[] = {
	[SCCP_CHANNELSTATEREASON_NORMAL] = "NORMAL",
	[SCCP_CHANNELSTATEREASON_TRANSFER] = "TRANSFER",
	[SCCP_CHANNELSTATEREASON_CALLFORWARD] = "CALLFORWARD",
	[SCCP_CHANNELSTATEREASON_CONFERENCE] = "CONFERENCE",
	[SCCP_CHANNELSTATEREASON_SENTINEL] = "LOOKUPERROR"
};

int sccp_channelstatereason_exists(int sccp_channelstatereason_int_value) {
	if ((SCCP_CHANNELSTATEREASON_TRANSFER <=sccp_channelstatereason_int_value) && (sccp_channelstatereason_int_value < SCCP_CHANNELSTATEREASON_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_channelstatereason2str(sccp_channelstatereason_t enum_value) {
	if ((SCCP_CHANNELSTATEREASON_NORMAL <= enum_value) && (enum_value <= SCCP_CHANNELSTATEREASON_SENTINEL)) {
		return sccp_channelstatereason_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_channelstatereason2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_channelstatereason2str\n";
}

sccp_channelstatereason_t sccp_channelstatereason_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_channelstatereason_map); idx++) {
		if (sccp_strcaseequals(sccp_channelstatereason_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_channelstatereason_str2val(%s) not found\n", lookup_str);
	return SCCP_CHANNELSTATEREASON_SENTINEL;
}

int sccp_channelstatereason_str2intval(const char *lookup_str) {
	int res = sccp_channelstatereason_str2val(lookup_str);
	return (int)res != SCCP_CHANNELSTATEREASON_SENTINEL ? res : -1;
}

char *sccp_channelstatereason_all_entries(void) {
	static char res[] = "NORMAL,TRANSFER,CALLFORWARD,CONFERENCE";
	return res;
}
/* = End =========================================================================================        sccp_channelstatereason === */


/* = Begin =======================================================================================                  sccp_earlyrtp === */


/*
 * \brief enum sccp_earlyrtp
 */
static const char *sccp_earlyrtp_map[] = {
	[SCCP_EARLYRTP_IMMEDIATE] = "Immediate",
	[SCCP_EARLYRTP_OFFHOOK] = "OffHook",
	[SCCP_EARLYRTP_DIALING] = "Dialing",
	[SCCP_EARLYRTP_RINGOUT] = "Ringout",
	[SCCP_EARLYRTP_PROGRESS] = "Progress",
	[SCCP_EARLYRTP_NONE] = "None",
	[SCCP_EARLYRTP_SENTINEL] = "LOOKUPERROR"
};

int sccp_earlyrtp_exists(int sccp_earlyrtp_int_value) {
	if ((SCCP_EARLYRTP_OFFHOOK <=sccp_earlyrtp_int_value) && (sccp_earlyrtp_int_value < SCCP_EARLYRTP_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_earlyrtp2str(sccp_earlyrtp_t enum_value) {
	if ((SCCP_EARLYRTP_IMMEDIATE <= enum_value) && (enum_value <= SCCP_EARLYRTP_SENTINEL)) {
		return sccp_earlyrtp_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_earlyrtp2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_earlyrtp2str\n";
}

sccp_earlyrtp_t sccp_earlyrtp_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_earlyrtp_map); idx++) {
		if (sccp_strcaseequals(sccp_earlyrtp_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_earlyrtp_str2val(%s) not found\n", lookup_str);
	return SCCP_EARLYRTP_SENTINEL;
}

int sccp_earlyrtp_str2intval(const char *lookup_str) {
	int res = sccp_earlyrtp_str2val(lookup_str);
	return (int)res != SCCP_EARLYRTP_SENTINEL ? res : -1;
}

char *sccp_earlyrtp_all_entries(void) {
	static char res[] = "Immediate,OffHook,Dialing,Ringout,Progress,None";
	return res;
}
/* = End =========================================================================================                  sccp_earlyrtp === */


/* = Begin =======================================================================================               sccp_devicestate === */


/*
 * \brief enum sccp_devicestate
 */
static const char *sccp_devicestate_map[] = {
	[SCCP_DEVICESTATE_ONHOOK] = "On Hook",
	[SCCP_DEVICESTATE_OFFHOOK] = "Off Hook",
	[SCCP_DEVICESTATE_UNAVAILABLE] = "Unavailable",
	[SCCP_DEVICESTATE_DND] = "Do Not Disturb",
	[SCCP_DEVICESTATE_FWDALL] = "Forward All",
	[SCCP_DEVICESTATE_SENTINEL] = "LOOKUPERROR"
};

int sccp_devicestate_exists(int sccp_devicestate_int_value) {
	if ((SCCP_DEVICESTATE_OFFHOOK <=sccp_devicestate_int_value) && (sccp_devicestate_int_value < SCCP_DEVICESTATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_devicestate2str(sccp_devicestate_t enum_value) {
	if ((SCCP_DEVICESTATE_ONHOOK <= enum_value) && (enum_value <= SCCP_DEVICESTATE_SENTINEL)) {
		return sccp_devicestate_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_devicestate2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_devicestate2str\n";
}

sccp_devicestate_t sccp_devicestate_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_devicestate_map); idx++) {
		if (sccp_strcaseequals(sccp_devicestate_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_devicestate_str2val(%s) not found\n", lookup_str);
	return SCCP_DEVICESTATE_SENTINEL;
}

int sccp_devicestate_str2intval(const char *lookup_str) {
	int res = sccp_devicestate_str2val(lookup_str);
	return (int)res != SCCP_DEVICESTATE_SENTINEL ? res : -1;
}

char *sccp_devicestate_all_entries(void) {
	static char res[] = "On Hook,Off Hook,Unavailable,Do Not Disturb,Forward All";
	return res;
}
/* = End =========================================================================================               sccp_devicestate === */


/* = Begin =======================================================================================               sccp_callforward === */


/*
 * \brief enum sccp_callforward
 */
static const char *sccp_callforward_map[] = {
	[SCCP_CFWD_NONE] = "None",
	[SCCP_CFWD_ALL] = "All",
	[SCCP_CFWD_BUSY] = "Busy",
	[SCCP_CFWD_NOANSWER] = "NoAnswer",
	[SCCP_CALLFORWARD_SENTINEL] = "LOOKUPERROR"
};

int sccp_callforward_exists(int sccp_callforward_int_value) {
	if ((SCCP_CFWD_ALL <=sccp_callforward_int_value) && (sccp_callforward_int_value < SCCP_CALLFORWARD_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_callforward2str(sccp_callforward_t enum_value) {
	if ((SCCP_CFWD_NONE <= enum_value) && (enum_value <= SCCP_CALLFORWARD_SENTINEL)) {
		return sccp_callforward_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_callforward2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_callforward2str\n";
}

sccp_callforward_t sccp_callforward_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_callforward_map); idx++) {
		if (sccp_strcaseequals(sccp_callforward_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_callforward_str2val(%s) not found\n", lookup_str);
	return SCCP_CALLFORWARD_SENTINEL;
}

int sccp_callforward_str2intval(const char *lookup_str) {
	int res = sccp_callforward_str2val(lookup_str);
	return (int)res != SCCP_CALLFORWARD_SENTINEL ? res : -1;
}

char *sccp_callforward_all_entries(void) {
	static char res[] = "None,All,Busy,NoAnswer";
	return res;
}
/* = End =========================================================================================               sccp_callforward === */


/* = Begin =======================================================================================                  sccp_dtmfmode === */

/*!
 * \brief SCCP Dtmf Mode (ENUM)
 */
static const char *sccp_dtmfmode_map[] = {
	[SCCP_DTMFMODE_AUTO] = "AUTO",
	[SCCP_DTMFMODE_RFC2833] = "RFC2833",
	[SCCP_DTMFMODE_SKINNY] = "SKINNY",
	[SCCP_DTMFMODE_SENTINEL] = "LOOKUPERROR"
};

int sccp_dtmfmode_exists(int sccp_dtmfmode_int_value) {
	if ((SCCP_DTMFMODE_RFC2833 <=sccp_dtmfmode_int_value) && (sccp_dtmfmode_int_value < SCCP_DTMFMODE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_dtmfmode2str(sccp_dtmfmode_t enum_value) {
	if ((SCCP_DTMFMODE_AUTO <= enum_value) && (enum_value <= SCCP_DTMFMODE_SENTINEL)) {
		return sccp_dtmfmode_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_dtmfmode2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_dtmfmode2str\n";
}

sccp_dtmfmode_t sccp_dtmfmode_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_dtmfmode_map); idx++) {
		if (sccp_strcaseequals(sccp_dtmfmode_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_dtmfmode_str2val(%s) not found\n", lookup_str);
	return SCCP_DTMFMODE_SENTINEL;
}

int sccp_dtmfmode_str2intval(const char *lookup_str) {
	int res = sccp_dtmfmode_str2val(lookup_str);
	return (int)res != SCCP_DTMFMODE_SENTINEL ? res : -1;
}

char *sccp_dtmfmode_all_entries(void) {
	static char res[] = "AUTO,RFC2833,SKINNY";
	return res;
}
/* = End =========================================================================================                  sccp_dtmfmode === */


/* = Begin =======================================================================================                sccp_autoanswer === */

/*!
 * \brief SCCP Autoanswer (ENUM)
 */
static const char *sccp_autoanswer_map[] = {
	[SCCP_AUTOANSWER_NONE] = "AutoAnswer None",
	[SCCP_AUTOANSWER_1W] = "AutoAnswer 1-Way",
	[SCCP_AUTOANSWER_2W] = "AutoAnswer Both Ways",
	[SCCP_AUTOANSWER_SENTINEL] = "LOOKUPERROR"
};

int sccp_autoanswer_exists(int sccp_autoanswer_int_value) {
	if ((SCCP_AUTOANSWER_1W <=sccp_autoanswer_int_value) && (sccp_autoanswer_int_value < SCCP_AUTOANSWER_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_autoanswer2str(sccp_autoanswer_t enum_value) {
	if ((SCCP_AUTOANSWER_NONE <= enum_value) && (enum_value <= SCCP_AUTOANSWER_SENTINEL)) {
		return sccp_autoanswer_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_autoanswer2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_autoanswer2str\n";
}

sccp_autoanswer_t sccp_autoanswer_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_autoanswer_map); idx++) {
		if (sccp_strcaseequals(sccp_autoanswer_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_autoanswer_str2val(%s) not found\n", lookup_str);
	return SCCP_AUTOANSWER_SENTINEL;
}

int sccp_autoanswer_str2intval(const char *lookup_str) {
	int res = sccp_autoanswer_str2val(lookup_str);
	return (int)res != SCCP_AUTOANSWER_SENTINEL ? res : -1;
}

char *sccp_autoanswer_all_entries(void) {
	static char res[] = "AutoAnswer None,AutoAnswer 1-Way,AutoAnswer Both Ways";
	return res;
}
/* = End =========================================================================================                sccp_autoanswer === */


/* = Begin =======================================================================================                   sccp_dndmode === */

/*!
 * \brief SCCP DNDMode (ENUM)
 */
static const char *sccp_dndmode_map[] = {
	[SCCP_DNDMODE_OFF] = "Off",
	[SCCP_DNDMODE_REJECT] = "Reject",
	[SCCP_DNDMODE_SILENT] = "Silent",
	[SCCP_DNDMODE_USERDEFINED] = "UserDefined",
	[SCCP_DNDMODE_SENTINEL] = "LOOKUPERROR"
};

int sccp_dndmode_exists(int sccp_dndmode_int_value) {
	if ((SCCP_DNDMODE_REJECT <=sccp_dndmode_int_value) && (sccp_dndmode_int_value < SCCP_DNDMODE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_dndmode2str(sccp_dndmode_t enum_value) {
	if ((SCCP_DNDMODE_OFF <= enum_value) && (enum_value <= SCCP_DNDMODE_SENTINEL)) {
		return sccp_dndmode_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_dndmode2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_dndmode2str\n";
}

sccp_dndmode_t sccp_dndmode_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_dndmode_map); idx++) {
		if (sccp_strcaseequals(sccp_dndmode_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_dndmode_str2val(%s) not found\n", lookup_str);
	return SCCP_DNDMODE_SENTINEL;
}

int sccp_dndmode_str2intval(const char *lookup_str) {
	int res = sccp_dndmode_str2val(lookup_str);
	return (int)res != SCCP_DNDMODE_SENTINEL ? res : -1;
}

char *sccp_dndmode_all_entries(void) {
	static char res[] = "Off,Reject,Silent,UserDefined";
	return res;
}
/* = End =========================================================================================                   sccp_dndmode === */


/* = Begin =======================================================================================                 sccp_accessory === */


/*
 * \brief enum sccp_accessory
 */
static const char *sccp_accessory_map[] = {
	[SCCP_ACCESSORY_NONE] = "None",
	[SCCP_ACCESSORY_HEADSET] = "Headset",
	[SCCP_ACCESSORY_HANDSET] = "Handset",
	[SCCP_ACCESSORY_SPEAKER] = "Speaker",
	[SCCP_ACCESSORY_SENTINEL] = "LOOKUPERROR"
};

int sccp_accessory_exists(int sccp_accessory_int_value) {
	if ((SCCP_ACCESSORY_HEADSET <=sccp_accessory_int_value) && (sccp_accessory_int_value < SCCP_ACCESSORY_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_accessory2str(sccp_accessory_t enum_value) {
	if ((SCCP_ACCESSORY_NONE <= enum_value) && (enum_value <= SCCP_ACCESSORY_SENTINEL)) {
		return sccp_accessory_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_accessory2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_accessory2str\n";
}

sccp_accessory_t sccp_accessory_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_accessory_map); idx++) {
		if (sccp_strcaseequals(sccp_accessory_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_accessory_str2val(%s) not found\n", lookup_str);
	return SCCP_ACCESSORY_SENTINEL;
}

int sccp_accessory_str2intval(const char *lookup_str) {
	int res = sccp_accessory_str2val(lookup_str);
	return (int)res != SCCP_ACCESSORY_SENTINEL ? res : -1;
}

char *sccp_accessory_all_entries(void) {
	static char res[] = "None,Headset,Handset,Speaker";
	return res;
}
/* = End =========================================================================================                 sccp_accessory === */


/* = Begin =======================================================================================            sccp_accessorystate === */


/*
 * \brief enum sccp_accessorystate
 */
static const char *sccp_accessorystate_map[] = {
	[SCCP_ACCESSORYSTATE_NONE] = "None",
	[SCCP_ACCESSORYSTATE_OFFHOOK] = "Off Hook",
	[SCCP_ACCESSORYSTATE_ONHOOK] = "On Hook",
	[SCCP_ACCESSORYSTATE_SENTINEL] = "LOOKUPERROR"
};

int sccp_accessorystate_exists(int sccp_accessorystate_int_value) {
	if ((SCCP_ACCESSORYSTATE_OFFHOOK <=sccp_accessorystate_int_value) && (sccp_accessorystate_int_value < SCCP_ACCESSORYSTATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_accessorystate2str(sccp_accessorystate_t enum_value) {
	if ((SCCP_ACCESSORYSTATE_NONE <= enum_value) && (enum_value <= SCCP_ACCESSORYSTATE_SENTINEL)) {
		return sccp_accessorystate_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_accessorystate2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_accessorystate2str\n";
}

sccp_accessorystate_t sccp_accessorystate_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_accessorystate_map); idx++) {
		if (sccp_strcaseequals(sccp_accessorystate_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_accessorystate_str2val(%s) not found\n", lookup_str);
	return SCCP_ACCESSORYSTATE_SENTINEL;
}

int sccp_accessorystate_str2intval(const char *lookup_str) {
	int res = sccp_accessorystate_str2val(lookup_str);
	return (int)res != SCCP_ACCESSORYSTATE_SENTINEL ? res : -1;
}

char *sccp_accessorystate_all_entries(void) {
	static char res[] = "None,Off Hook,On Hook";
	return res;
}
/* = End =========================================================================================            sccp_accessorystate === */


/* = Begin =======================================================================================         sccp_config_buttontype === */


/*
 * \brief enum sccp_config_buttontype
 */
static const char *sccp_config_buttontype_map[] = {
	[LINE] = "Line",
	[SPEEDDIAL] = "Speeddial",
	[SERVICE] = "Service",
	[FEATURE] = "Feature",
	[EMPTY] = "Empty",
	[SCCP_CONFIG_BUTTONTYPE_SENTINEL] = "LOOKUPERROR"
};

int sccp_config_buttontype_exists(int sccp_config_buttontype_int_value) {
	if ((SPEEDDIAL <=sccp_config_buttontype_int_value) && (sccp_config_buttontype_int_value < SCCP_CONFIG_BUTTONTYPE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_config_buttontype2str(sccp_config_buttontype_t enum_value) {
	if ((LINE <= enum_value) && (enum_value <= SCCP_CONFIG_BUTTONTYPE_SENTINEL)) {
		return sccp_config_buttontype_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_config_buttontype2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_config_buttontype2str\n";
}

sccp_config_buttontype_t sccp_config_buttontype_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_config_buttontype_map); idx++) {
		if (sccp_strcaseequals(sccp_config_buttontype_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_config_buttontype_str2val(%s) not found\n", lookup_str);
	return SCCP_CONFIG_BUTTONTYPE_SENTINEL;
}

int sccp_config_buttontype_str2intval(const char *lookup_str) {
	int res = sccp_config_buttontype_str2val(lookup_str);
	return (int)res != SCCP_CONFIG_BUTTONTYPE_SENTINEL ? res : -1;
}

char *sccp_config_buttontype_all_entries(void) {
	static char res[] = "Line,Speeddial,Service,Feature,Empty";
	return res;
}
/* = End =========================================================================================         sccp_config_buttontype === */


/* = Begin =======================================================================================            sccp_devstate_state === */


/*
 * \brief enum sccp_devstate_state
 */
static const char *sccp_devstate_state_map[] = {
	[SCCP_DEVSTATE_IDLE] = "IDLE",
	[SCCP_DEVSTATE_INUSE] = "INUSE",
	[SCCP_DEVSTATE_STATE_SENTINEL] = "LOOKUPERROR"
};

int sccp_devstate_state_exists(int sccp_devstate_state_int_value) {
	if ((SCCP_DEVSTATE_INUSE <=sccp_devstate_state_int_value) && (sccp_devstate_state_int_value < SCCP_DEVSTATE_STATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_devstate_state2str(sccp_devstate_state_t enum_value) {
	if ((SCCP_DEVSTATE_IDLE <= enum_value) && (enum_value <= SCCP_DEVSTATE_STATE_SENTINEL)) {
		return sccp_devstate_state_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_devstate_state2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_devstate_state2str\n";
}

sccp_devstate_state_t sccp_devstate_state_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_devstate_state_map); idx++) {
		if (sccp_strcaseequals(sccp_devstate_state_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_devstate_state_str2val(%s) not found\n", lookup_str);
	return SCCP_DEVSTATE_STATE_SENTINEL;
}

int sccp_devstate_state_str2intval(const char *lookup_str) {
	int res = sccp_devstate_state_str2val(lookup_str);
	return (int)res != SCCP_DEVSTATE_STATE_SENTINEL ? res : -1;
}

char *sccp_devstate_state_all_entries(void) {
	static char res[] = "IDLE,INUSE";
	return res;
}
/* = End =========================================================================================            sccp_devstate_state === */


/* = Begin =======================================================================================   sccp_blindtransferindication === */


/*
 * \brief enum sccp_blindtransferindication
 */
static const char *sccp_blindtransferindication_map[] = {
	[SCCP_BLINDTRANSFER_RING] = "RING",
	[SCCP_BLINDTRANSFER_MOH] = "MOH",
	[SCCP_BLINDTRANSFERINDICATION_SENTINEL] = "LOOKUPERROR"
};

int sccp_blindtransferindication_exists(int sccp_blindtransferindication_int_value) {
	if ((SCCP_BLINDTRANSFER_MOH <=sccp_blindtransferindication_int_value) && (sccp_blindtransferindication_int_value < SCCP_BLINDTRANSFERINDICATION_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_blindtransferindication2str(sccp_blindtransferindication_t enum_value) {
	if ((SCCP_BLINDTRANSFER_RING <= enum_value) && (enum_value <= SCCP_BLINDTRANSFERINDICATION_SENTINEL)) {
		return sccp_blindtransferindication_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_blindtransferindication2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_blindtransferindication2str\n";
}

sccp_blindtransferindication_t sccp_blindtransferindication_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_blindtransferindication_map); idx++) {
		if (sccp_strcaseequals(sccp_blindtransferindication_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_blindtransferindication_str2val(%s) not found\n", lookup_str);
	return SCCP_BLINDTRANSFERINDICATION_SENTINEL;
}

int sccp_blindtransferindication_str2intval(const char *lookup_str) {
	int res = sccp_blindtransferindication_str2val(lookup_str);
	return (int)res != SCCP_BLINDTRANSFERINDICATION_SENTINEL ? res : -1;
}

char *sccp_blindtransferindication_all_entries(void) {
	static char res[] = "RING,MOH";
	return res;
}
/* = End =========================================================================================   sccp_blindtransferindication === */


/* = Begin =======================================================================================         sccp_call_answer_order === */


/*
 * \brief enum sccp_call_answer_order
 */
static const char *sccp_call_answer_order_map[] = {
	[SCCP_ANSWER_OLDEST_FIRST] = "OldestFirst",
	[SCCP_ANSWER_LAST_FIRST] = "LastFirst",
	[SCCP_CALL_ANSWER_ORDER_SENTINEL] = "LOOKUPERROR"
};

int sccp_call_answer_order_exists(int sccp_call_answer_order_int_value) {
	if ((SCCP_ANSWER_LAST_FIRST <=sccp_call_answer_order_int_value) && (sccp_call_answer_order_int_value < SCCP_CALL_ANSWER_ORDER_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_call_answer_order2str(sccp_call_answer_order_t enum_value) {
	if ((SCCP_ANSWER_OLDEST_FIRST <= enum_value) && (enum_value <= SCCP_CALL_ANSWER_ORDER_SENTINEL)) {
		return sccp_call_answer_order_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_call_answer_order2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_call_answer_order2str\n";
}

sccp_call_answer_order_t sccp_call_answer_order_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_call_answer_order_map); idx++) {
		if (sccp_strcaseequals(sccp_call_answer_order_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_call_answer_order_str2val(%s) not found\n", lookup_str);
	return SCCP_CALL_ANSWER_ORDER_SENTINEL;
}

int sccp_call_answer_order_str2intval(const char *lookup_str) {
	int res = sccp_call_answer_order_str2val(lookup_str);
	return (int)res != SCCP_CALL_ANSWER_ORDER_SENTINEL ? res : -1;
}

char *sccp_call_answer_order_all_entries(void) {
	static char res[] = "OldestFirst,LastFirst";
	return res;
}
/* = End =========================================================================================         sccp_call_answer_order === */


/* = Begin =======================================================================================                       sccp_nat === */


/*
 * \brief enum sccp_nat
 */
static const char *sccp_nat_map[] = {
	[SCCP_NAT_AUTO] = "Auto",
	[SCCP_NAT_OFF] = "Off",
	[SCCP_NAT_AUTO_OFF] = "(Auto)Off",
	[SCCP_NAT_ON] = "On",
	[SCCP_NAT_AUTO_ON] = "(Auto)On",
	[SCCP_NAT_SENTINEL] = "LOOKUPERROR"
};

int sccp_nat_exists(int sccp_nat_int_value) {
	if ((SCCP_NAT_OFF <=sccp_nat_int_value) && (sccp_nat_int_value < SCCP_NAT_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_nat2str(sccp_nat_t enum_value) {
	if ((SCCP_NAT_AUTO <= enum_value) && (enum_value <= SCCP_NAT_SENTINEL)) {
		return sccp_nat_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_nat2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_nat2str\n";
}

sccp_nat_t sccp_nat_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_nat_map); idx++) {
		if (sccp_strcaseequals(sccp_nat_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_nat_str2val(%s) not found\n", lookup_str);
	return SCCP_NAT_SENTINEL;
}

int sccp_nat_str2intval(const char *lookup_str) {
	int res = sccp_nat_str2val(lookup_str);
	return (int)res != SCCP_NAT_SENTINEL ? res : -1;
}

char *sccp_nat_all_entries(void) {
	static char res[] = "Auto,Off,(Auto)Off,On,(Auto)On";
	return res;
}
/* = End =========================================================================================                       sccp_nat === */


/* = Begin =======================================================================================                sccp_video_mode === */


/*
 * \brief enum sccp_video_mode
 */
static const char *sccp_video_mode_map[] = {
	[SCCP_VIDEO_MODE_OFF] = "Off",
	[SCCP_VIDEO_MODE_USER] = "User",
	[SCCP_VIDEO_MODE_AUTO] = "Auto",
	[SCCP_VIDEO_MODE_SENTINEL] = "LOOKUPERROR"
};

int sccp_video_mode_exists(int sccp_video_mode_int_value) {
	if ((SCCP_VIDEO_MODE_USER <=sccp_video_mode_int_value) && (sccp_video_mode_int_value < SCCP_VIDEO_MODE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_video_mode2str(sccp_video_mode_t enum_value) {
	if ((SCCP_VIDEO_MODE_OFF <= enum_value) && (enum_value <= SCCP_VIDEO_MODE_SENTINEL)) {
		return sccp_video_mode_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_video_mode2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_video_mode2str\n";
}

sccp_video_mode_t sccp_video_mode_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_video_mode_map); idx++) {
		if (sccp_strcaseequals(sccp_video_mode_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_video_mode_str2val(%s) not found\n", lookup_str);
	return SCCP_VIDEO_MODE_SENTINEL;
}

int sccp_video_mode_str2intval(const char *lookup_str) {
	int res = sccp_video_mode_str2val(lookup_str);
	return (int)res != SCCP_VIDEO_MODE_SENTINEL ? res : -1;
}

char *sccp_video_mode_all_entries(void) {
	static char res[] = "Off,User,Auto";
	return res;
}
/* = End =========================================================================================                sccp_video_mode === */


/* = Begin =======================================================================================                sccp_event_type === */


/*
 * \brief enum sccp_event_type
 */
static const char *sccp_event_type_map[] = {
	"Line Created",
	"Line Changed",
	"Line Deleted",
	"Device Attached",
	"Device Detached",
	"Device Preregistered",
	"Device Registered",
	"Device Unregistered",
	"Feature Changed",
	"LineStatus Changed",
	"LOOKUPERROR"
};

int sccp_event_type_exists(int sccp_event_type_int_value) {
	int res = 0, i;
	for (i = 0; i < SCCP_EVENT_TYPE_SENTINEL; i++) {
		if ((sccp_event_type_int_value & 1 << i) == 1 << i) {
			res |= 1;
		}
	}
	return res;
}

const char * sccp_event_type2str(int sccp_event_type_int_value) {
	static char res[90] = "";
	uint32_t i;
	int pos = 0;
	for (i = 0; i < ARRAY_LEN(sccp_event_type_map) - 1; i++) {
		if ((sccp_event_type_int_value & 1 << i) == 1 << i) {
			pos += snprintf(res + pos, 90, "%s%s", pos ? "," : "", sccp_event_type_map[i]);
		}
	}
	if (!strlen(res)) {
		pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_event_type2str\n", sccp_event_type_int_value);
		return "SCCP: OutOfBounds Error during lookup of sparse sccp_event_type2str\n";
	}
	return res;
}

sccp_event_type_t sccp_event_type_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_event_type_map); idx++) {
		if (sccp_strcaseequals(sccp_event_type_map[idx], lookup_str)) {
			return 1 << idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_event_type_str2val(%s) not found\n", lookup_str);
	return SCCP_EVENT_TYPE_SENTINEL;
}

int sccp_event_type_str2intval(const char *lookup_str) {
	int res = sccp_event_type_str2val(lookup_str);
	return (int)res != SCCP_EVENT_TYPE_SENTINEL ? res : -1;
}

char *sccp_event_type_all_entries(void) {
	static char res[] = "Line Created,Line Changed,Line Deleted,Device Attached,Device Detached,Device Preregistered,Device Registered,Device Unregistered,Feature Changed,LineStatus Changed";
	return res;
}
/* = End =========================================================================================                sccp_event_type === */


/* = Begin =======================================================================================                sccp_parkresult === */


/*
 * \brief enum sccp_parkresult
 */
static const char *sccp_parkresult_map[] = {
	[PARK_RESULT_FAIL] = "Park Failed",
	[PARK_RESULT_SUCCESS] = "Park Successfull",
	[SCCP_PARKRESULT_SENTINEL] = "LOOKUPERROR"
};

int sccp_parkresult_exists(int sccp_parkresult_int_value) {
	if ((PARK_RESULT_SUCCESS <=sccp_parkresult_int_value) && (sccp_parkresult_int_value < SCCP_PARKRESULT_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_parkresult2str(sccp_parkresult_t enum_value) {
	if ((PARK_RESULT_FAIL <= enum_value) && (enum_value <= SCCP_PARKRESULT_SENTINEL)) {
		return sccp_parkresult_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_parkresult2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_parkresult2str\n";
}

sccp_parkresult_t sccp_parkresult_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_parkresult_map); idx++) {
		if (sccp_strcaseequals(sccp_parkresult_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_parkresult_str2val(%s) not found\n", lookup_str);
	return SCCP_PARKRESULT_SENTINEL;
}

int sccp_parkresult_str2intval(const char *lookup_str) {
	int res = sccp_parkresult_str2val(lookup_str);
	return (int)res != SCCP_PARKRESULT_SENTINEL ? res : -1;
}

char *sccp_parkresult_all_entries(void) {
	static char res[] = "Park Failed,Park Successfull";
	return res;
}
/* = End =========================================================================================                sccp_parkresult === */


/* = Begin =======================================================================================     sccp_callerid_presentation === */


/*
 * \brief enum sccp_callerid_presentation
 */
static const char *sccp_callerid_presentation_map[] = {
	[CALLERID_PRESENTATION_FORBIDDEN] = "CalledId Presentation Forbidden",
	[CALLERID_PRESENTATION_ALLOWED] = "CallerId Presentation Allowed",
	[SCCP_CALLERID_PRESENTATION_SENTINEL] = "LOOKUPERROR"
};

int sccp_callerid_presentation_exists(int sccp_callerid_presentation_int_value) {
	if ((CALLERID_PRESENTATION_ALLOWED <=sccp_callerid_presentation_int_value) && (sccp_callerid_presentation_int_value < SCCP_CALLERID_PRESENTATION_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_callerid_presentation2str(sccp_callerid_presentation_t enum_value) {
	if ((CALLERID_PRESENTATION_FORBIDDEN <= enum_value) && (enum_value <= SCCP_CALLERID_PRESENTATION_SENTINEL)) {
		return sccp_callerid_presentation_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_callerid_presentation2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_callerid_presentation2str\n";
}

sccp_callerid_presentation_t sccp_callerid_presentation_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_callerid_presentation_map); idx++) {
		if (sccp_strcaseequals(sccp_callerid_presentation_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_callerid_presentation_str2val(%s) not found\n", lookup_str);
	return SCCP_CALLERID_PRESENTATION_SENTINEL;
}

int sccp_callerid_presentation_str2intval(const char *lookup_str) {
	int res = sccp_callerid_presentation_str2val(lookup_str);
	return (int)res != SCCP_CALLERID_PRESENTATION_SENTINEL ? res : -1;
}

char *sccp_callerid_presentation_all_entries(void) {
	static char res[] = "CalledId Presentation Forbidden,CallerId Presentation Allowed";
	return res;
}
/* = End =========================================================================================     sccp_callerid_presentation === */


/* = Begin =======================================================================================                sccp_rtp_status === */


/*
 * \brief enum sccp_rtp_status
 */
static const char *sccp_rtp_status_map[] = {
	"Rtp Inactive",
	"Rtp In Progress",
	"Rtp Active",
	"LOOKUPERROR"
};

int sccp_rtp_status_exists(int sccp_rtp_status_int_value) {
	int res = 0, i;
	for (i = 0; i < SCCP_RTP_STATUS_SENTINEL; i++) {
		if ((sccp_rtp_status_int_value & 1 << i) == 1 << i) {
			res |= 1;
		}
	}
	return res;
}

const char * sccp_rtp_status2str(int sccp_rtp_status_int_value) {
	static char res[138] = "";
	uint32_t i;
	int pos = 0;
	for (i = 0; i < ARRAY_LEN(sccp_rtp_status_map) - 1; i++) {
		if ((sccp_rtp_status_int_value & 1 << i) == 1 << i) {
			pos += snprintf(res + pos, 138, "%s%s", pos ? "," : "", sccp_rtp_status_map[i]);
		}
	}
	if (!strlen(res)) {
		pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_rtp_status2str\n", sccp_rtp_status_int_value);
		return "SCCP: OutOfBounds Error during lookup of sparse sccp_rtp_status2str\n";
	}
	return res;
}

sccp_rtp_status_t sccp_rtp_status_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_rtp_status_map); idx++) {
		if (sccp_strcaseequals(sccp_rtp_status_map[idx], lookup_str)) {
			return 1 << idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_rtp_status_str2val(%s) not found\n", lookup_str);
	return SCCP_RTP_STATUS_SENTINEL;
}

int sccp_rtp_status_str2intval(const char *lookup_str) {
	int res = sccp_rtp_status_str2val(lookup_str);
	return (int)res != SCCP_RTP_STATUS_SENTINEL ? res : -1;
}

char *sccp_rtp_status_all_entries(void) {
	static char res[] = "Rtp Inactive,Rtp In Progress,Rtp Active";
	return res;
}
/* = End =========================================================================================                sccp_rtp_status === */


/* = Begin =======================================================================================             sccp_sccp_rtp_type === */


/*
 * \brief enum sccp_sccp_rtp_type
 */
static const char *sccp_sccp_rtp_type_map[] = {
	"Audio RTP",
	"Video RTP",
	"Text RTP",
	"LOOKUPERROR"
};

int sccp_sccp_rtp_type_exists(int sccp_sccp_rtp_type_int_value) {
	int res = 0, i;
	for (i = 0; i < SCCP_SCCP_RTP_TYPE_SENTINEL; i++) {
		if ((sccp_sccp_rtp_type_int_value & 1 << i) == 1 << i) {
			res |= 1;
		}
	}
	return res;
}

const char * sccp_sccp_rtp_type2str(int sccp_sccp_rtp_type_int_value) {
	static char res[186] = "";
	uint32_t i;
	int pos = 0;
	for (i = 0; i < ARRAY_LEN(sccp_sccp_rtp_type_map) - 1; i++) {
		if ((sccp_sccp_rtp_type_int_value & 1 << i) == 1 << i) {
			pos += snprintf(res + pos, 186, "%s%s", pos ? "," : "", sccp_sccp_rtp_type_map[i]);
		}
	}
	if (!strlen(res)) {
		pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_sccp_rtp_type2str\n", sccp_sccp_rtp_type_int_value);
		return "SCCP: OutOfBounds Error during lookup of sparse sccp_sccp_rtp_type2str\n";
	}
	return res;
}

sccp_sccp_rtp_type_t sccp_sccp_rtp_type_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_sccp_rtp_type_map); idx++) {
		if (sccp_strcaseequals(sccp_sccp_rtp_type_map[idx], lookup_str)) {
			return 1 << idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_sccp_rtp_type_str2val(%s) not found\n", lookup_str);
	return SCCP_SCCP_RTP_TYPE_SENTINEL;
}

int sccp_sccp_rtp_type_str2intval(const char *lookup_str) {
	int res = sccp_sccp_rtp_type_str2val(lookup_str);
	return (int)res != SCCP_SCCP_RTP_TYPE_SENTINEL ? res : -1;
}

char *sccp_sccp_rtp_type_all_entries(void) {
	static char res[] = "Audio RTP,Video RTP,Text RTP";
	return res;
}
/* = End =========================================================================================             sccp_sccp_rtp_type === */


/* = Begin =======================================================================================          sccp_extension_status === */


/*
 * \brief enum sccp_extension_status
 */
static const char *sccp_extension_status_map[] = {
	[SCCP_EXTENSION_NOTEXISTS] = "Extension does not exist",
	[SCCP_EXTENSION_MATCHMORE] = "Matches more than one extension",
	[SCCP_EXTENSION_EXACTMATCH] = "Exact Extension Match",
	[SCCP_EXTENSION_STATUS_SENTINEL] = "LOOKUPERROR"
};

int sccp_extension_status_exists(int sccp_extension_status_int_value) {
	if ((SCCP_EXTENSION_MATCHMORE <=sccp_extension_status_int_value) && (sccp_extension_status_int_value < SCCP_EXTENSION_STATUS_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_extension_status2str(sccp_extension_status_t enum_value) {
	if ((SCCP_EXTENSION_NOTEXISTS <= enum_value) && (enum_value <= SCCP_EXTENSION_STATUS_SENTINEL)) {
		return sccp_extension_status_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_extension_status2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_extension_status2str\n";
}

sccp_extension_status_t sccp_extension_status_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_extension_status_map); idx++) {
		if (sccp_strcaseequals(sccp_extension_status_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_extension_status_str2val(%s) not found\n", lookup_str);
	return SCCP_EXTENSION_STATUS_SENTINEL;
}

int sccp_extension_status_str2intval(const char *lookup_str) {
	int res = sccp_extension_status_str2val(lookup_str);
	return (int)res != SCCP_EXTENSION_STATUS_SENTINEL ? res : -1;
}

char *sccp_extension_status_all_entries(void) {
	static char res[] = "Extension does not exist,Matches more than one extension,Exact Extension Match";
	return res;
}
/* = End =========================================================================================          sccp_extension_status === */


/* = Begin =======================================================================================    sccp_channel_request_status === */


/*
 * \brief enum sccp_channel_request_status
 */
static const char *sccp_channel_request_status_map[] = {
	[SCCP_REQUEST_STATUS_ERROR] = "Request Status Error",
	[SCCP_REQUEST_STATUS_LINEUNKNOWN] = "Request Line Unknown",
	[SCCP_REQUEST_STATUS_LINEUNAVAIL] = "Request Line Unavailable",
	[SCCP_REQUEST_STATUS_SUCCESS] = "Request Success",
	[SCCP_CHANNEL_REQUEST_STATUS_SENTINEL] = "LOOKUPERROR"
};

int sccp_channel_request_status_exists(int sccp_channel_request_status_int_value) {
	if ((SCCP_REQUEST_STATUS_LINEUNKNOWN <=sccp_channel_request_status_int_value) && (sccp_channel_request_status_int_value < SCCP_CHANNEL_REQUEST_STATUS_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_channel_request_status2str(sccp_channel_request_status_t enum_value) {
	if ((SCCP_REQUEST_STATUS_ERROR <= enum_value) && (enum_value <= SCCP_CHANNEL_REQUEST_STATUS_SENTINEL)) {
		return sccp_channel_request_status_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_channel_request_status2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_channel_request_status2str\n";
}

sccp_channel_request_status_t sccp_channel_request_status_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_channel_request_status_map); idx++) {
		if (sccp_strcaseequals(sccp_channel_request_status_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_channel_request_status_str2val(%s) not found\n", lookup_str);
	return SCCP_CHANNEL_REQUEST_STATUS_SENTINEL;
}

int sccp_channel_request_status_str2intval(const char *lookup_str) {
	int res = sccp_channel_request_status_str2val(lookup_str);
	return (int)res != SCCP_CHANNEL_REQUEST_STATUS_SENTINEL ? res : -1;
}

char *sccp_channel_request_status_all_entries(void) {
	static char res[] = "Request Status Error,Request Line Unknown,Request Line Unavailable,Request Success";
	return res;
}
/* = End =========================================================================================    sccp_channel_request_status === */


/* = Begin =======================================================================================          sccp_message_priority === */


/*
 * \brief enum sccp_message_priority
 */
static const char *sccp_message_priority_map[] = {
	[SCCP_MESSAGE_PRIORITY_IDLE] = "Message Priority Idle",
	[SCCP_MESSAGE_PRIORITY_VOICEMAIL] = "Message Priority Voicemail",
	[SCCP_MESSAGE_PRIORITY_MONITOR] = "Message Priority Monitor",
	[SCCP_MESSAGE_PRIORITY_PRIVACY] = "Message Priority Privacy",
	[SCCP_MESSAGE_PRIORITY_DND] = "Message Priority Do not disturb",
	[SCCP_MESSAGE_PRIORITY_CFWD] = "Message Priority Call Forward",
	[SCCP_MESSAGE_PRIORITY_SENTINEL] = "LOOKUPERROR"
};

int sccp_message_priority_exists(int sccp_message_priority_int_value) {
	if ((SCCP_MESSAGE_PRIORITY_VOICEMAIL <=sccp_message_priority_int_value) && (sccp_message_priority_int_value < SCCP_MESSAGE_PRIORITY_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_message_priority2str(sccp_message_priority_t enum_value) {
	if ((SCCP_MESSAGE_PRIORITY_IDLE <= enum_value) && (enum_value <= SCCP_MESSAGE_PRIORITY_SENTINEL)) {
		return sccp_message_priority_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_message_priority2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_message_priority2str\n";
}

sccp_message_priority_t sccp_message_priority_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_message_priority_map); idx++) {
		if (sccp_strcaseequals(sccp_message_priority_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_message_priority_str2val(%s) not found\n", lookup_str);
	return SCCP_MESSAGE_PRIORITY_SENTINEL;
}

int sccp_message_priority_str2intval(const char *lookup_str) {
	int res = sccp_message_priority_str2val(lookup_str);
	return (int)res != SCCP_MESSAGE_PRIORITY_SENTINEL ? res : -1;
}

char *sccp_message_priority_all_entries(void) {
	static char res[] = "Message Priority Idle,Message Priority Voicemail,Message Priority Monitor,Message Priority Privacy,Message Priority Do not disturb,Message Priority Call Forward";
	return res;
}
/* = End =========================================================================================          sccp_message_priority === */


/* = Begin =======================================================================================               sccp_push_result === */


/*
 * \brief enum sccp_push_result
 */
static const char *sccp_push_result_map[] = {
	[SCCP_PUSH_RESULT_FAIL] = "Push Failed",
	[SCCP_PUSH_RESULT_NOT_SUPPORTED] = "Push Not Supported",
	[SCCP_PUSH_RESULT_SUCCESS] = "Pushed Successfully",
	[SCCP_PUSH_RESULT_SENTINEL] = "LOOKUPERROR"
};

int sccp_push_result_exists(int sccp_push_result_int_value) {
	if ((SCCP_PUSH_RESULT_NOT_SUPPORTED <=sccp_push_result_int_value) && (sccp_push_result_int_value < SCCP_PUSH_RESULT_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_push_result2str(sccp_push_result_t enum_value) {
	if ((SCCP_PUSH_RESULT_FAIL <= enum_value) && (enum_value <= SCCP_PUSH_RESULT_SENTINEL)) {
		return sccp_push_result_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_push_result2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_push_result2str\n";
}

sccp_push_result_t sccp_push_result_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_push_result_map); idx++) {
		if (sccp_strcaseequals(sccp_push_result_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_push_result_str2val(%s) not found\n", lookup_str);
	return SCCP_PUSH_RESULT_SENTINEL;
}

int sccp_push_result_str2intval(const char *lookup_str) {
	int res = sccp_push_result_str2val(lookup_str);
	return (int)res != SCCP_PUSH_RESULT_SENTINEL ? res : -1;
}

char *sccp_push_result_all_entries(void) {
	static char res[] = "Push Failed,Push Not Supported,Pushed Successfully";
	return res;
}
/* = End =========================================================================================               sccp_push_result === */


/* = Begin =======================================================================================                sccp_tokenstate === */


/*
 * \brief enum sccp_tokenstate
 */
static const char *sccp_tokenstate_map[] = {
	[SCCP_TOKEN_STATE_NOTOKEN] = "No Token",
	[SCCP_TOKEN_STATE_ACK] = "Token Acknowledged",
	[SCCP_TOKEN_STATE_REJ] = "Token Rejected",
	[SCCP_TOKENSTATE_SENTINEL] = "LOOKUPERROR"
};

int sccp_tokenstate_exists(int sccp_tokenstate_int_value) {
	if ((SCCP_TOKEN_STATE_ACK <=sccp_tokenstate_int_value) && (sccp_tokenstate_int_value < SCCP_TOKENSTATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_tokenstate2str(sccp_tokenstate_t enum_value) {
	if ((SCCP_TOKEN_STATE_NOTOKEN <= enum_value) && (enum_value <= SCCP_TOKENSTATE_SENTINEL)) {
		return sccp_tokenstate_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_tokenstate2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_tokenstate2str\n";
}

sccp_tokenstate_t sccp_tokenstate_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_tokenstate_map); idx++) {
		if (sccp_strcaseequals(sccp_tokenstate_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_tokenstate_str2val(%s) not found\n", lookup_str);
	return SCCP_TOKENSTATE_SENTINEL;
}

int sccp_tokenstate_str2intval(const char *lookup_str) {
	int res = sccp_tokenstate_str2val(lookup_str);
	return (int)res != SCCP_TOKENSTATE_SENTINEL ? res : -1;
}

char *sccp_tokenstate_all_entries(void) {
	static char res[] = "No Token,Token Acknowledged,Token Rejected";
	return res;
}
/* = End =========================================================================================                sccp_tokenstate === */


/* = Begin =======================================================================================                sccp_softswitch === */


/*
 * \brief enum sccp_softswitch
 */
static const char *sccp_softswitch_map[] = {
	[SCCP_SOFTSWITCH_DIAL] = "Softswitch Dial",
	[SCCP_SOFTSWITCH_GETFORWARDEXTEN] = "Softswitch Get Forward Extension",
#ifdef CS_SCCP_PICKUP
	[SCCP_SOFTSWITCH_GETPICKUPEXTEN] = "Softswitch Get Pickup Extension",
#endif
	[SCCP_SOFTSWITCH_GETMEETMEROOM] = "Softswitch Get Meetme Room",
	[SCCP_SOFTSWITCH_GETBARGEEXTEN] = "Softswitch Get Barge Extension",
	[SCCP_SOFTSWITCH_GETCBARGEROOM] = "Softswitch Get CBarrge Room",
#ifdef CS_SCCP_CONFERENCE
	[SCCP_SOFTSWITCH_GETCONFERENCEROOM] = "Softswitch Get Conference Room",
#endif
	[SCCP_SOFTSWITCH_SENTINEL] = "LOOKUPERROR"
};

int sccp_softswitch_exists(int sccp_softswitch_int_value) {
	if ((SCCP_SOFTSWITCH_GETFORWARDEXTEN <=sccp_softswitch_int_value) && (sccp_softswitch_int_value < SCCP_SOFTSWITCH_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_softswitch2str(sccp_softswitch_t enum_value) {
	if ((SCCP_SOFTSWITCH_DIAL <= enum_value) && (enum_value <= SCCP_SOFTSWITCH_SENTINEL)) {
		return sccp_softswitch_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_softswitch2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_softswitch2str\n";
}

sccp_softswitch_t sccp_softswitch_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_softswitch_map); idx++) {
		if (sccp_strcaseequals(sccp_softswitch_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_softswitch_str2val(%s) not found\n", lookup_str);
	return SCCP_SOFTSWITCH_SENTINEL;
}

int sccp_softswitch_str2intval(const char *lookup_str) {
	int res = sccp_softswitch_str2val(lookup_str);
	return (int)res != SCCP_SOFTSWITCH_SENTINEL ? res : -1;
}

char *sccp_softswitch_all_entries(void) {
	static char res[] = "Softswitch Dial,Softswitch Get Forward Extension,Token Rejected,Softswitch Get Pickup Extension,Softswitch Get Meetme Room,Softswitch Get Barge Extension,Softswitch Get CBarrge Room,Device Unregistered,Softswitch Get Conference Room";
	return res;
}
/* = End =========================================================================================                sccp_softswitch === */


/* = Begin =======================================================================================                 sccp_phonebook === */


/*
 * \brief enum sccp_phonebook
 */
static const char *sccp_phonebook_map[] = {
	[SCCP_PHONEBOOK_NONE] = "Phonebook None",
	[SCCP_PHONEBOOK_MISSED] = "Phonebook Missed",
	[SCCP_PHONEBOOK_RECEIVED] = "Phonebook Received",
	[SCCP_PHONEBOOK_SENTINEL] = "LOOKUPERROR"
};

int sccp_phonebook_exists(int sccp_phonebook_int_value) {
	if ((SCCP_PHONEBOOK_MISSED <=sccp_phonebook_int_value) && (sccp_phonebook_int_value < SCCP_PHONEBOOK_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_phonebook2str(sccp_phonebook_t enum_value) {
	if ((SCCP_PHONEBOOK_NONE <= enum_value) && (enum_value <= SCCP_PHONEBOOK_SENTINEL)) {
		return sccp_phonebook_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_phonebook2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_phonebook2str\n";
}

sccp_phonebook_t sccp_phonebook_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_phonebook_map); idx++) {
		if (sccp_strcaseequals(sccp_phonebook_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_phonebook_str2val(%s) not found\n", lookup_str);
	return SCCP_PHONEBOOK_SENTINEL;
}

int sccp_phonebook_str2intval(const char *lookup_str) {
	int res = sccp_phonebook_str2val(lookup_str);
	return (int)res != SCCP_PHONEBOOK_SENTINEL ? res : -1;
}

char *sccp_phonebook_all_entries(void) {
	static char res[] = "Phonebook None,Phonebook Missed,Phonebook Received";
	return res;
}
/* = End =========================================================================================                 sccp_phonebook === */


/* = Begin =======================================================================================     sccp_feature_monitor_state === */


/*
 * \brief enum sccp_feature_monitor_state
 */
static const char *sccp_feature_monitor_state_map[] = {
	[SCCP_FEATURE_MONITOR_STATE_DISABLED] = "Feature Monitor Disabled",
	[SCCP_FEATURE_MONITOR_STATE_ACTIVE] = "Feature Monitor Active",
	[SCCP_FEATURE_MONITOR_STATE_REQUESTED] = "Feature Monitor Requested",
	[SCCP_FEATURE_MONITOR_STATE_SENTINEL] = "LOOKUPERROR"
};

int sccp_feature_monitor_state_exists(int sccp_feature_monitor_state_int_value) {
	if ((SCCP_FEATURE_MONITOR_STATE_ACTIVE <=sccp_feature_monitor_state_int_value) && (sccp_feature_monitor_state_int_value < SCCP_FEATURE_MONITOR_STATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_feature_monitor_state2str(sccp_feature_monitor_state_t enum_value) {
	if ((SCCP_FEATURE_MONITOR_STATE_DISABLED <= enum_value) && (enum_value <= SCCP_FEATURE_MONITOR_STATE_SENTINEL)) {
		return sccp_feature_monitor_state_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_feature_monitor_state2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_feature_monitor_state2str\n";
}

sccp_feature_monitor_state_t sccp_feature_monitor_state_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_feature_monitor_state_map); idx++) {
		if (sccp_strcaseequals(sccp_feature_monitor_state_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_feature_monitor_state_str2val(%s) not found\n", lookup_str);
	return SCCP_FEATURE_MONITOR_STATE_SENTINEL;
}

int sccp_feature_monitor_state_str2intval(const char *lookup_str) {
	int res = sccp_feature_monitor_state_str2val(lookup_str);
	return (int)res != SCCP_FEATURE_MONITOR_STATE_SENTINEL ? res : -1;
}

char *sccp_feature_monitor_state_all_entries(void) {
	static char res[] = "Feature Monitor Disabled,Feature Monitor Active,Feature Monitor Requested";
	return res;
}
/* = End =========================================================================================     sccp_feature_monitor_state === */


/* = Begin =======================================================================================               sccp_readingtype === */

/*!
 * \brief Config Reading Type Enum
 */
static const char *sccp_readingtype_map[] = {
	[SCCP_CONFIG_READINITIAL] = "Read Initial Config",
	[SCCP_CONFIG_READRELOAD] = "Reloading Config",
	[SCCP_READINGTYPE_SENTINEL] = "LOOKUPERROR"
};

int sccp_readingtype_exists(int sccp_readingtype_int_value) {
	if ((SCCP_CONFIG_READRELOAD <=sccp_readingtype_int_value) && (sccp_readingtype_int_value < SCCP_READINGTYPE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_readingtype2str(sccp_readingtype_t enum_value) {
	if ((SCCP_CONFIG_READINITIAL <= enum_value) && (enum_value <= SCCP_READINGTYPE_SENTINEL)) {
		return sccp_readingtype_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_readingtype2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_readingtype2str\n";
}

sccp_readingtype_t sccp_readingtype_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_readingtype_map); idx++) {
		if (sccp_strcaseequals(sccp_readingtype_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_readingtype_str2val(%s) not found\n", lookup_str);
	return SCCP_READINGTYPE_SENTINEL;
}

int sccp_readingtype_str2intval(const char *lookup_str) {
	int res = sccp_readingtype_str2val(lookup_str);
	return (int)res != SCCP_READINGTYPE_SENTINEL ? res : -1;
}

char *sccp_readingtype_all_entries(void) {
	static char res[] = "Read Initial Config,Reloading Config";
	return res;
}
/* = End =========================================================================================               sccp_readingtype === */


/* = Begin =======================================================================================       sccp_configurationchange === */

/*!
 * \brief Status of configuration change
 */
static const char *sccp_configurationchange_map[] = {
	"Config: No Update Needed",
	"Config: Device Reset Needed",
	"Warning while reading Config",
	"Error while reading Config",
	"LOOKUPERROR"
};

int sccp_configurationchange_exists(int sccp_configurationchange_int_value) {
	int res = 0, i;
	for (i = 0; i < SCCP_CONFIGURATIONCHANGE_SENTINEL; i++) {
		if ((sccp_configurationchange_int_value & 1 << i) == 1 << i) {
			res |= 1;
		}
	}
	return res;
}

const char * sccp_configurationchange2str(int sccp_configurationchange_int_value) {
	static char res[294] = "";
	uint32_t i;
	int pos = 0;
	for (i = 0; i < ARRAY_LEN(sccp_configurationchange_map) - 1; i++) {
		if ((sccp_configurationchange_int_value & 1 << i) == 1 << i) {
			pos += snprintf(res + pos, 294, "%s%s", pos ? "," : "", sccp_configurationchange_map[i]);
		}
	}
	if (!strlen(res)) {
		pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_configurationchange2str\n", sccp_configurationchange_int_value);
		return "SCCP: OutOfBounds Error during lookup of sparse sccp_configurationchange2str\n";
	}
	return res;
}

sccp_configurationchange_t sccp_configurationchange_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_configurationchange_map); idx++) {
		if (sccp_strcaseequals(sccp_configurationchange_map[idx], lookup_str)) {
			return 1 << idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_configurationchange_str2val(%s) not found\n", lookup_str);
	return SCCP_CONFIGURATIONCHANGE_SENTINEL;
}

int sccp_configurationchange_str2intval(const char *lookup_str) {
	int res = sccp_configurationchange_str2val(lookup_str);
	return (int)res != SCCP_CONFIGURATIONCHANGE_SENTINEL ? res : -1;
}

char *sccp_configurationchange_all_entries(void) {
	static char res[] = "Config: No Update Needed,Config: Device Reset Needed,Warning while reading Config,Error while reading Config";
	return res;
}
/* = End =========================================================================================       sccp_configurationchange === */


/* = Begin =======================================================================================      sccp_call_statistics_type === */


/*
 * \brief enum sccp_call_statistics_type
 */
static const char *sccp_call_statistics_type_map[] = {
	[SCCP_CALLSTATISTIC_LAST] = "CallStatistics last Call",
	[SCCP_CALLSTATISTIC_AVG] = "CallStatistics average",
	[SCCP_CALL_STATISTICS_TYPE_SENTINEL] = "LOOKUPERROR"
};

int sccp_call_statistics_type_exists(int sccp_call_statistics_type_int_value) {
	if ((SCCP_CALLSTATISTIC_AVG <=sccp_call_statistics_type_int_value) && (sccp_call_statistics_type_int_value < SCCP_CALL_STATISTICS_TYPE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_call_statistics_type2str(sccp_call_statistics_type_t enum_value) {
	if ((SCCP_CALLSTATISTIC_LAST <= enum_value) && (enum_value <= SCCP_CALL_STATISTICS_TYPE_SENTINEL)) {
		return sccp_call_statistics_type_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_call_statistics_type2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_call_statistics_type2str\n";
}

sccp_call_statistics_type_t sccp_call_statistics_type_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_call_statistics_type_map); idx++) {
		if (sccp_strcaseequals(sccp_call_statistics_type_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_call_statistics_type_str2val(%s) not found\n", lookup_str);
	return SCCP_CALL_STATISTICS_TYPE_SENTINEL;
}

int sccp_call_statistics_type_str2intval(const char *lookup_str) {
	int res = sccp_call_statistics_type_str2val(lookup_str);
	return (int)res != SCCP_CALL_STATISTICS_TYPE_SENTINEL ? res : -1;
}

char *sccp_call_statistics_type_all_entries(void) {
	static char res[] = "CallStatistics last Call,CallStatistics average";
	return res;
}
/* = End =========================================================================================      sccp_call_statistics_type === */


/* = Begin =======================================================================================                  sccp_rtp_info === */


/*
 * \brief enum sccp_rtp_info
 */
static const char *sccp_rtp_info_map[] = {
	"RTP Info: None",
	"RTP Info: Available",
	"RTP Info: Allow DirectMedia",
	"LOOKUPERROR"
};

int sccp_rtp_info_exists(int sccp_rtp_info_int_value) {
	int res = 0, i;
	for (i = 0; i < SCCP_RTP_INFO_SENTINEL; i++) {
		if ((sccp_rtp_info_int_value & 1 << i) == 1 << i) {
			res |= 1;
		}
	}
	return res;
}

const char * sccp_rtp_info2str(int sccp_rtp_info_int_value) {
	static char res[375] = "";
	uint32_t i;
	int pos = 0;
	for (i = 0; i < ARRAY_LEN(sccp_rtp_info_map) - 1; i++) {
		if ((sccp_rtp_info_int_value & 1 << i) == 1 << i) {
			pos += snprintf(res + pos, 375, "%s%s", pos ? "," : "", sccp_rtp_info_map[i]);
		}
	}
	if (!strlen(res)) {
		pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_rtp_info2str\n", sccp_rtp_info_int_value);
		return "SCCP: OutOfBounds Error during lookup of sparse sccp_rtp_info2str\n";
	}
	return res;
}

sccp_rtp_info_t sccp_rtp_info_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_rtp_info_map); idx++) {
		if (sccp_strcaseequals(sccp_rtp_info_map[idx], lookup_str)) {
			return 1 << idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_rtp_info_str2val(%s) not found\n", lookup_str);
	return SCCP_RTP_INFO_SENTINEL;
}

int sccp_rtp_info_str2intval(const char *lookup_str) {
	int res = sccp_rtp_info_str2val(lookup_str);
	return (int)res != SCCP_RTP_INFO_SENTINEL ? res : -1;
}

char *sccp_rtp_info_all_entries(void) {
	static char res[] = "RTP Info: None,RTP Info: Available,RTP Info: Allow DirectMedia";
	return res;
}
/* = End =========================================================================================                  sccp_rtp_info === */


/* = Begin =======================================================================================              sccp_feature_type === */


/*
 * \brief enum sccp_feature_type
 */
static const char *sccp_feature_type_map[] = {
	[SCCP_FEATURE_UNKNOWN] = "FEATURE_UNKNOWN",
	[SCCP_FEATURE_CFWDNONE] = "cfwd off",
	[SCCP_FEATURE_CFWDALL] = "cfwdall",
	[SCCP_FEATURE_CFWDBUSY] = "cfwdbusy",
	[SCCP_FEATURE_DND] = "dnd",
	[SCCP_FEATURE_PRIVACY] = "privacy",
	[SCCP_FEATURE_MONITOR] = "monitor",
	[SCCP_FEATURE_HOLD] = "hold",
	[SCCP_FEATURE_TRANSFER] = "transfer",
	[SCCP_FEATURE_MULTIBLINK] = "multiblink",
	[SCCP_FEATURE_MOBILITY] = "mobility",
	[SCCP_FEATURE_CONFERENCE] = "conference",
	[SCCP_FEATURE_DO_NOT_DISTURB] = "do not disturb",
	[SCCP_FEATURE_CONF_LIST] = "ConfList",
	[SCCP_FEATURE_REMOVE_LAST_PARTICIPANT] = "RemoveLastParticipant",
	[SCCP_FEATURE_HLOG] = "Hunt Group Log-in/out",
	[SCCP_FEATURE_QRT] = "QRT",
	[SCCP_FEATURE_CALLBACK] = "CallBack",
	[SCCP_FEATURE_OTHER_PICKUP] = "OtherPickup",
	[SCCP_FEATURE_VIDEO_MODE] = "VideoMode",
	[SCCP_FEATURE_NEW_CALL] = "NewCall",
	[SCCP_FEATURE_END_CALL] = "EndCall",
	[SCCP_FEATURE_TESTE] = "FEATURE_TESTE",
	[SCCP_FEATURE_TESTF] = "FEATURE_TESTF",
	[SCCP_FEATURE_TESTI] = "FEATURE_TESTI",
	[SCCP_FEATURE_TESTG] = "Messages",
	[SCCP_FEATURE_TESTH] = "Directory",
	[SCCP_FEATURE_TESTJ] = "Application",
#ifdef CS_DEVSTATE_FEATURE
	[SCCP_FEATURE_DEVSTATE] = "devstate",
#endif
	[SCCP_FEATURE_PICKUP] = "pickup",
	[SCCP_FEATURE_TYPE_SENTINEL] = "LOOKUPERROR"
};

int sccp_feature_type_exists(int sccp_feature_type_int_value) {
	if ((SCCP_FEATURE_CFWDNONE <=sccp_feature_type_int_value) && (sccp_feature_type_int_value < SCCP_FEATURE_TYPE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_feature_type2str(sccp_feature_type_t enum_value) {
	if ((SCCP_FEATURE_UNKNOWN <= enum_value) && (enum_value <= SCCP_FEATURE_TYPE_SENTINEL)) {
		return sccp_feature_type_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_feature_type2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_feature_type2str\n";
}

sccp_feature_type_t sccp_feature_type_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_feature_type_map); idx++) {
		if (sccp_strcaseequals(sccp_feature_type_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_feature_type_str2val(%s) not found\n", lookup_str);
	return SCCP_FEATURE_TYPE_SENTINEL;
}

int sccp_feature_type_str2intval(const char *lookup_str) {
	int res = sccp_feature_type_str2val(lookup_str);
	return (int)res != SCCP_FEATURE_TYPE_SENTINEL ? res : -1;
}

char *sccp_feature_type_all_entries(void) {
	static char res[] = "FEATURE_UNKNOWN,cfwd off,cfwdall,cfwdbusy,dnd,privacy,monitor,hold,transfer,multiblink,mobility,conference,do not disturb,ConfList,RemoveLastParticipant,Hunt Group Log-in/out,QRT,CallBack,OtherPickup,VideoMode,NewCall,EndCall,FEATURE_TESTE,FEATURE_TESTF,FEATURE_TESTI,Messages,Directory,Application,,devstate,pickup";
	return res;
}
/* = End =========================================================================================              sccp_feature_type === */


/* = Begin =======================================================================================              sccp_callinfo_key === */


/*
 * \brief enum sccp_callinfo_key
 */
static const char *sccp_callinfo_key_map[] = {
	[SCCP_CALLINFO_NONE] = "none",
	[SCCP_CALLINFO_CALLEDPARTY_NAME] = "calledparty name",
	[SCCP_CALLINFO_CALLEDPARTY_NUMBER] = "calledparty number",
	[SCCP_CALLINFO_CALLEDPARTY_VOICEMAIL] = "calledparty voicemail",
	[SCCP_CALLINFO_CALLINGPARTY_NAME] = "callingparty name",
	[SCCP_CALLINFO_CALLINGPARTY_NUMBER] = "callingparty number",
	[SCCP_CALLINFO_CALLINGPARTY_VOICEMAIL] = "callingparty voicemail",
	[SCCP_CALLINFO_ORIG_CALLEDPARTY_NAME] = "orig_calledparty name",
	[SCCP_CALLINFO_ORIG_CALLEDPARTY_NUMBER] = "orig_calledparty number",
	[SCCP_CALLINFO_ORIG_CALLEDPARTY_VOICEMAIL] = "orig_calledparty voicemail",
	[SCCP_CALLINFO_ORIG_CALLINGPARTY_NAME] = "orig_callingparty name",
	[SCCP_CALLINFO_ORIG_CALLINGPARTY_NUMBER] = "orig_callingparty number",
	[SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NAME] = "last_redirectingparty name",
	[SCCP_CALLINFO_LAST_REDIRECTINGPARTY_NUMBER] = "last_redirectingparty number",
	[SCCP_CALLINFO_LAST_REDIRECTINGPARTY_VOICEMAIL] = "last_redirectingparty voicemail",
	[SCCP_CALLINFO_HUNT_PILOT_NAME] = "hunt pilot name",
	[SCCP_CALLINFO_HUNT_PILOT_NUMBER] = "hunt pilor number",
	[SCCP_CALLINFO_ORIG_CALLEDPARTY_REDIRECT_REASON] = "orig_calledparty_redirect reason",
	[SCCP_CALLINFO_LAST_REDIRECT_REASON] = "last_redirect reason",
	[SCCP_CALLINFO_PRESENTATION] = "presentation",
	[SCCP_CALLINFO_KEY_SENTINEL] = "LOOKUPERROR"
};

int sccp_callinfo_key_exists(int sccp_callinfo_key_int_value) {
	if ((SCCP_CALLINFO_CALLEDPARTY_NAME <=sccp_callinfo_key_int_value) && (sccp_callinfo_key_int_value < SCCP_CALLINFO_KEY_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * sccp_callinfo_key2str(sccp_callinfo_key_t enum_value) {
	if ((SCCP_CALLINFO_NONE <= enum_value) && (enum_value <= SCCP_CALLINFO_KEY_SENTINEL)) {
		return sccp_callinfo_key_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in sccp_callinfo_key2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of sccp_callinfo_key2str\n";
}

sccp_callinfo_key_t sccp_callinfo_key_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(sccp_callinfo_key_map); idx++) {
		if (sccp_strcaseequals(sccp_callinfo_key_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, sccp_callinfo_key_str2val(%s) not found\n", lookup_str);
	return SCCP_CALLINFO_KEY_SENTINEL;
}

int sccp_callinfo_key_str2intval(const char *lookup_str) {
	int res = sccp_callinfo_key_str2val(lookup_str);
	return (int)res != SCCP_CALLINFO_KEY_SENTINEL ? res : -1;
}

char *sccp_callinfo_key_all_entries(void) {
	static char res[] = "none,calledparty name,calledparty number,calledparty voicemail,callingparty name,callingparty number,callingparty voicemail,orig_calledparty name,orig_calledparty number,orig_calledparty voicemail,orig_callingparty name,orig_callingparty number,last_redirectingparty name,last_redirectingparty number,last_redirectingparty voicemail,hunt pilot name,hunt pilor number,orig_calledparty_redirect reason,last_redirect reason,presentation";
	return res;
}
/* = End =========================================================================================              sccp_callinfo_key === */


/* = Begin =======================================================================================                skinny_lampmode === */

/*!
 * \brief Skinny Lamp Mode (ENUM)
 */
static const char *skinny_lampmode_map[] = {
	[SKINNY_LAMP_OFF] = "Off",
	[SKINNY_LAMP_ON] = "On",
	[SKINNY_LAMP_WINK] = "Wink",
	[SKINNY_LAMP_FLASH] = "Flash",
	[SKINNY_LAMP_BLINK] = "Blink",
	[SKINNY_LAMPMODE_SENTINEL] = "LOOKUPERROR"
};

int skinny_lampmode_exists(int skinny_lampmode_int_value) {
	if ((SKINNY_LAMP_ON <=skinny_lampmode_int_value) && (skinny_lampmode_int_value < SKINNY_LAMPMODE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_lampmode2str(skinny_lampmode_t enum_value) {
	if ((SKINNY_LAMP_OFF <= enum_value) && (enum_value <= SKINNY_LAMPMODE_SENTINEL)) {
		return skinny_lampmode_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_lampmode2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_lampmode2str\n";
}

skinny_lampmode_t skinny_lampmode_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_lampmode_map); idx++) {
		if (sccp_strcaseequals(skinny_lampmode_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_lampmode_str2val(%s) not found\n", lookup_str);
	return SKINNY_LAMPMODE_SENTINEL;
}

int skinny_lampmode_str2intval(const char *lookup_str) {
	int res = skinny_lampmode_str2val(lookup_str);
	return (int)res != SKINNY_LAMPMODE_SENTINEL ? res : -1;
}

char *skinny_lampmode_all_entries(void) {
	static char res[] = "Off,On,Wink,Flash,Blink";
	return res;
}
/* = End =========================================================================================                skinny_lampmode === */


/* = Begin =======================================================================================                skinny_calltype === */

/*!
 * \brief Skinny Protocol Call Type (ENUM)
 */
static const char *skinny_calltype_map[] = {
	[SKINNY_CALLTYPE_INBOUND] = "Inbound",
	[SKINNY_CALLTYPE_OUTBOUND] = "Outbound",
	[SKINNY_CALLTYPE_FORWARD] = "Forward",
	[SKINNY_CALLTYPE_SENTINEL] = "LOOKUPERROR"
};

int skinny_calltype_exists(int skinny_calltype_int_value) {
	if ((SKINNY_CALLTYPE_OUTBOUND <=skinny_calltype_int_value) && (skinny_calltype_int_value < SKINNY_CALLTYPE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_calltype2str(skinny_calltype_t enum_value) {
	if ((SKINNY_CALLTYPE_INBOUND <= enum_value) && (enum_value <= SKINNY_CALLTYPE_SENTINEL)) {
		return skinny_calltype_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_calltype2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_calltype2str\n";
}

skinny_calltype_t skinny_calltype_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_calltype_map); idx++) {
		if (sccp_strcaseequals(skinny_calltype_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_calltype_str2val(%s) not found\n", lookup_str);
	return SKINNY_CALLTYPE_SENTINEL;
}

int skinny_calltype_str2intval(const char *lookup_str) {
	int res = skinny_calltype_str2val(lookup_str);
	return (int)res != SKINNY_CALLTYPE_SENTINEL ? res : -1;
}

char *skinny_calltype_all_entries(void) {
	static char res[] = "Inbound,Outbound,Forward";
	return res;
}
/* = End =========================================================================================                skinny_calltype === */


/* = Begin =======================================================================================               skinny_callstate === */

/*!
 * \brief Skinny Protocol Call Type (ENUM)
 */
static const char *skinny_callstate_map[] = {
	[SKINNY_CALLSTATE_OFFHOOK] = "offhook",
	[SKINNY_CALLSTATE_ONHOOK] = "onhook",
	[SKINNY_CALLSTATE_RINGOUT] = "ring-out",
	[SKINNY_CALLSTATE_RINGIN] = "ring-in",
	[SKINNY_CALLSTATE_CONNECTED] = "connected",
	[SKINNY_CALLSTATE_BUSY] = "busy",
	[SKINNY_CALLSTATE_CONGESTION] = "congestion",
	[SKINNY_CALLSTATE_HOLD] = "hold",
	[SKINNY_CALLSTATE_CALLWAITING] = "call waiting",
	[SKINNY_CALLSTATE_CALLTRANSFER] = "call transfer",
	[SKINNY_CALLSTATE_CALLPARK] = "call park",
	[SKINNY_CALLSTATE_PROCEED] = "proceed",
	[SKINNY_CALLSTATE_CALLREMOTEMULTILINE] = "call remote multiline",
	[SKINNY_CALLSTATE_INVALIDNUMBER] = "invalid number",
	[SKINNY_CALLSTATE_HOLDYELLOW] = "hold yellow",
	[SKINNY_CALLSTATE_INTERCOMONEWAY] = "intercom one-way",
	[SKINNY_CALLSTATE_HOLDRED] = "hold red",
	[SKINNY_CALLSTATE_SENTINEL] = "LOOKUPERROR"
};

int skinny_callstate_exists(int skinny_callstate_int_value) {
	if ((SKINNY_CALLSTATE_ONHOOK <=skinny_callstate_int_value) && (skinny_callstate_int_value < SKINNY_CALLSTATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_callstate2str(skinny_callstate_t enum_value) {
	if ((SKINNY_CALLSTATE_OFFHOOK <= enum_value) && (enum_value <= SKINNY_CALLSTATE_SENTINEL)) {
		return skinny_callstate_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_callstate2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_callstate2str\n";
}

skinny_callstate_t skinny_callstate_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_callstate_map); idx++) {
		if (sccp_strcaseequals(skinny_callstate_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_callstate_str2val(%s) not found\n", lookup_str);
	return SKINNY_CALLSTATE_SENTINEL;
}

int skinny_callstate_str2intval(const char *lookup_str) {
	int res = skinny_callstate_str2val(lookup_str);
	return (int)res != SKINNY_CALLSTATE_SENTINEL ? res : -1;
}

char *skinny_callstate_all_entries(void) {
	static char res[] = "offhook,onhook,ring-out,ring-in,connected,busy,congestion,hold,call waiting,call transfer,call park,proceed,call remote multiline,invalid number,hold yellow,intercom one-way,hold red";
	return res;
}
/* = End =========================================================================================               skinny_callstate === */


/* = Begin =======================================================================================            skinny_callpriority === */

/*!
 * \brief Skinny Protocol Call Priority (ENUM)
 */
static const char *skinny_callpriority_map[] = {
	[SKINNY_CALLPRIORITY_HIGHEST] = "highest priority",
	[SKINNY_CALLPRIORITY_HIGH] = "high priority",
	[SKINNY_CALLPRIORITY_MEDIUM] = "medium priority",
	[SKINNY_CALLPRIORITY_LOW] = "low priority",
	[SKINNY_CALLPRIORITY_NORMAL] = "normal priority",
	[SKINNY_CALLPRIORITY_SENTINEL] = "LOOKUPERROR"
};

int skinny_callpriority_exists(int skinny_callpriority_int_value) {
	if ((SKINNY_CALLPRIORITY_HIGH <=skinny_callpriority_int_value) && (skinny_callpriority_int_value < SKINNY_CALLPRIORITY_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_callpriority2str(skinny_callpriority_t enum_value) {
	if ((SKINNY_CALLPRIORITY_HIGHEST <= enum_value) && (enum_value <= SKINNY_CALLPRIORITY_SENTINEL)) {
		return skinny_callpriority_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_callpriority2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_callpriority2str\n";
}

skinny_callpriority_t skinny_callpriority_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_callpriority_map); idx++) {
		if (sccp_strcaseequals(skinny_callpriority_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_callpriority_str2val(%s) not found\n", lookup_str);
	return SKINNY_CALLPRIORITY_SENTINEL;
}

int skinny_callpriority_str2intval(const char *lookup_str) {
	int res = skinny_callpriority_str2val(lookup_str);
	return (int)res != SKINNY_CALLPRIORITY_SENTINEL ? res : -1;
}

char *skinny_callpriority_all_entries(void) {
	static char res[] = "highest priority,high priority,medium priority,low priority,normal priority";
	return res;
}
/* = End =========================================================================================            skinny_callpriority === */


/* = Begin =======================================================================================     skinny_callinfo_visibility === */

/*!
 * \brief Skinny Protocol CallInfo Visibility (ENUM)
 */
static const char *skinny_callinfo_visibility_map[] = {
	[SKINNY_CALLINFO_VISIBILITY_DEFAULT] = "default",
	[SKINNY_CALLINFO_VISIBILITY_COLLAPSED] = "collapsed",
	[SKINNY_CALLINFO_VISIBILITY_HIDDEN] = "hidden",
	[SKINNY_CALLINFO_VISIBILITY_SENTINEL] = "LOOKUPERROR"
};

int skinny_callinfo_visibility_exists(int skinny_callinfo_visibility_int_value) {
	if ((SKINNY_CALLINFO_VISIBILITY_COLLAPSED <=skinny_callinfo_visibility_int_value) && (skinny_callinfo_visibility_int_value < SKINNY_CALLINFO_VISIBILITY_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_callinfo_visibility2str(skinny_callinfo_visibility_t enum_value) {
	if ((SKINNY_CALLINFO_VISIBILITY_DEFAULT <= enum_value) && (enum_value <= SKINNY_CALLINFO_VISIBILITY_SENTINEL)) {
		return skinny_callinfo_visibility_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_callinfo_visibility2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_callinfo_visibility2str\n";
}

skinny_callinfo_visibility_t skinny_callinfo_visibility_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_callinfo_visibility_map); idx++) {
		if (sccp_strcaseequals(skinny_callinfo_visibility_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_callinfo_visibility_str2val(%s) not found\n", lookup_str);
	return SKINNY_CALLINFO_VISIBILITY_SENTINEL;
}

int skinny_callinfo_visibility_str2intval(const char *lookup_str) {
	int res = skinny_callinfo_visibility_str2val(lookup_str);
	return (int)res != SKINNY_CALLINFO_VISIBILITY_SENTINEL ? res : -1;
}

char *skinny_callinfo_visibility_all_entries(void) {
	static char res[] = "default,collapsed,hidden";
	return res;
}
/* = End =========================================================================================     skinny_callinfo_visibility === */


/* = Begin =======================================================================================       skinny_callsecuritystate === */

/*!
 * \brief Skinny Protocol Call Security State (ENUM)
 */
static const char *skinny_callsecuritystate_map[] = {
	[SKINNY_CALLSECURITYSTATE_UNKNOWN] = "unknown",
	[SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED] = "not authenticated",
	[SKINNY_CALLSECURITYSTATE_AUTHENTICATED] = "authenticated",
	[SKINNY_CALLSECURITYSTATE_SENTINEL] = "LOOKUPERROR"
};

int skinny_callsecuritystate_exists(int skinny_callsecuritystate_int_value) {
	if ((SKINNY_CALLSECURITYSTATE_NOTAUTHENTICATED <=skinny_callsecuritystate_int_value) && (skinny_callsecuritystate_int_value < SKINNY_CALLSECURITYSTATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_callsecuritystate2str(skinny_callsecuritystate_t enum_value) {
	if ((SKINNY_CALLSECURITYSTATE_UNKNOWN <= enum_value) && (enum_value <= SKINNY_CALLSECURITYSTATE_SENTINEL)) {
		return skinny_callsecuritystate_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_callsecuritystate2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_callsecuritystate2str\n";
}

skinny_callsecuritystate_t skinny_callsecuritystate_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_callsecuritystate_map); idx++) {
		if (sccp_strcaseequals(skinny_callsecuritystate_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_callsecuritystate_str2val(%s) not found\n", lookup_str);
	return SKINNY_CALLSECURITYSTATE_SENTINEL;
}

int skinny_callsecuritystate_str2intval(const char *lookup_str) {
	int res = skinny_callsecuritystate_str2val(lookup_str);
	return (int)res != SKINNY_CALLSECURITYSTATE_SENTINEL ? res : -1;
}

char *skinny_callsecuritystate_all_entries(void) {
	static char res[] = "unknown,not authenticated,authenticated";
	return res;
}
/* = End =========================================================================================       skinny_callsecuritystate === */


/* = Begin =======================================================================================     skinny_busylampfield_state === */

/*!
 * \brief Skinny Busy Lamp Field Status (ENUM)
 */
static const char *skinny_busylampfield_state_map[] = {
	[SKINNY_BLF_STATUS_UNKNOWN] = "Unknown",
	[SKINNY_BLF_STATUS_IDLE] = "Not-in-use",
	[SKINNY_BLF_STATUS_INUSE] = "In-use",
	[SKINNY_BLF_STATUS_DND] = "DND",
	[SKINNY_BLF_STATUS_ALERTING] = "Alerting",
	[SKINNY_BUSYLAMPFIELD_STATE_SENTINEL] = "LOOKUPERROR"
};

int skinny_busylampfield_state_exists(int skinny_busylampfield_state_int_value) {
	if ((SKINNY_BLF_STATUS_IDLE <=skinny_busylampfield_state_int_value) && (skinny_busylampfield_state_int_value < SKINNY_BUSYLAMPFIELD_STATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_busylampfield_state2str(skinny_busylampfield_state_t enum_value) {
	if ((SKINNY_BLF_STATUS_UNKNOWN <= enum_value) && (enum_value <= SKINNY_BUSYLAMPFIELD_STATE_SENTINEL)) {
		return skinny_busylampfield_state_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_busylampfield_state2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_busylampfield_state2str\n";
}

skinny_busylampfield_state_t skinny_busylampfield_state_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_busylampfield_state_map); idx++) {
		if (sccp_strcaseequals(skinny_busylampfield_state_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_busylampfield_state_str2val(%s) not found\n", lookup_str);
	return SKINNY_BUSYLAMPFIELD_STATE_SENTINEL;
}

int skinny_busylampfield_state_str2intval(const char *lookup_str) {
	int res = skinny_busylampfield_state_str2val(lookup_str);
	return (int)res != SKINNY_BUSYLAMPFIELD_STATE_SENTINEL ? res : -1;
}

char *skinny_busylampfield_state_all_entries(void) {
	static char res[] = "Unknown,Not-in-use,In-use,DND,Alerting";
	return res;
}
/* = End =========================================================================================     skinny_busylampfield_state === */


/* = Begin =======================================================================================            sparse skinny_alarm === */

/*!
 * \brief Skinny Busy Lamp Field Status (ENUM)
 */
static const char *skinny_alarm_map[] = {"Critical",
"Warning",
"Informational",
"Unknown",
"Major",
"Minor",
"Marginal",
"TraceInfo",
};

int skinny_alarm_exists(int skinny_alarm_int_value) {
	static const int skinny_alarms[] = {SKINNY_ALARM_CRITICAL,SKINNY_ALARM_WARNING,SKINNY_ALARM_INFORMATIONAL,SKINNY_ALARM_UNKNOWN,SKINNY_ALARM_MAJOR,SKINNY_ALARM_MINOR,SKINNY_ALARM_MARGINAL,SKINNY_ALARM_TRACEINFO,};
	uint32_t idx;
	for (idx=0; idx < ARRAY_LEN(skinny_alarms); idx++) {
		if (skinny_alarms[idx]==skinny_alarm_int_value) {
			return 1;
		}
	}
	return 0;
}

const char * skinny_alarm2str(skinny_alarm_t enum_value) {
	switch(enum_value) {
		case SKINNY_ALARM_CRITICAL: return skinny_alarm_map[0];
		case SKINNY_ALARM_WARNING: return skinny_alarm_map[1];
		case SKINNY_ALARM_INFORMATIONAL: return skinny_alarm_map[2];
		case SKINNY_ALARM_UNKNOWN: return skinny_alarm_map[3];
		case SKINNY_ALARM_MAJOR: return skinny_alarm_map[4];
		case SKINNY_ALARM_MINOR: return skinny_alarm_map[5];
		case SKINNY_ALARM_MARGINAL: return skinny_alarm_map[6];
		case SKINNY_ALARM_TRACEINFO: return skinny_alarm_map[7];
		default:
			pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_alarm2str\n", enum_value);
			return "SCCP: OutOfBounds Error during lookup of sparse skinny_alarm2str\n";
	}
}

skinny_alarm_t skinny_alarm_str2val(const char *lookup_str) {
	if        (sccp_strcaseequals(skinny_alarm_map[0], lookup_str)) {
		return SKINNY_ALARM_CRITICAL;
	} else if (sccp_strcaseequals(skinny_alarm_map[1], lookup_str)) {
		return SKINNY_ALARM_WARNING;
	} else if (sccp_strcaseequals(skinny_alarm_map[2], lookup_str)) {
		return SKINNY_ALARM_INFORMATIONAL;
	} else if (sccp_strcaseequals(skinny_alarm_map[3], lookup_str)) {
		return SKINNY_ALARM_UNKNOWN;
	} else if (sccp_strcaseequals(skinny_alarm_map[4], lookup_str)) {
		return SKINNY_ALARM_MAJOR;
	} else if (sccp_strcaseequals(skinny_alarm_map[5], lookup_str)) {
		return SKINNY_ALARM_MINOR;
	} else if (sccp_strcaseequals(skinny_alarm_map[6], lookup_str)) {
		return SKINNY_ALARM_MARGINAL;
	} else if (sccp_strcaseequals(skinny_alarm_map[7], lookup_str)) {
		return SKINNY_ALARM_TRACEINFO;
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_alarm_str2val(%s) not found\n", lookup_str);
	return SKINNY_ALARM_SENTINEL;
}

int skinny_alarm_str2intval(const char *lookup_str) {
	int res = skinny_alarm_str2val(lookup_str);
	return (int)res != SKINNY_ALARM_SENTINEL ? res : -1;
}

char *skinny_alarm_all_entries(void) {
	static char res[] = "Critical,Warning,Informational,Unknown,Major,Minor,Marginal,TraceInfo";
	return res;
}
/* = End =========================================================================================            sparse skinny_alarm === */


/* = Begin =======================================================================================             sparse skinny_tone === */

/*!
 * \brief Skinny Tone (ENUM)
 */
static const char *skinny_tone_map[] = {"Silence",
"DTMF 1",
"DTMF 2",
"DTMF 3",
"DTMF 4",
"DTMF 5",
"DTMF 6",
"DTMF 7",
"DTMF 8",
"DTMF 9",
"DTMF 0",
"DTMF Star",
"DTMF Pound",
"DTMF A",
"DTMF B",
"DTMF C",
"DTMF D",
"Inside Dial Tone",
"Outside Dial Tone",
"Line Busy Tone",
"Alerting Tone",
"Reorder Tone",
"Recorder Warning Tone",
"Recorder Detected Tone",
"Reverting Tone",
"Receiver OffHook Tone",
"Partial Dial Tone",
"No Such Number Tone",
"Busy Verification Tone",
"Call Waiting Tone",
"Confirmation Tone",
"Camp On Indication Tone",
"Recall Dial Tone",
"Zip Zip",
"Zip",
"Beep Bonk",
"Music Tone",
"Hold Tone",
"Test Tone",
"DT Monitor Warning Tone",
"Add Call Waiting",
"Priority Call Wait",
"Recall Dial",
"Barg In",
"Distinct Alert",
"Priority Alert",
"Reminder Ring",
"Precedence RingBank",
"Pre-EmptionTone",
"MF1",
"MF2",
"MF3",
"MF4",
"MF5",
"MF6",
"MF7",
"MF8",
"MF9",
"MF0",
"MFKP1",
"MFST",
"MFKP2",
"MFSTP",
"MFST3P",
"MILLIWATT",
"MILLIWATT TEST",
"HIGH TONE",
"FLASH OVERRIDE",
"FLASH",
"PRIORITY",
"IMMEDIATE",
"PRE-AMP WARN",
"2105 HZ",
"2600 HZ",
"440 HZ",
"300 HZ",
"MLPP Pala",
"MLPP Ica",
"MLPP Vca",
"MLPP Bpa",
"MLPP Bnea",
"MLPP Upa",
"No Tone",
"Meetme Greeting Tone",
"Meetme Number Invalid Tone",
"Meetme Number Failed Tone",
"Meetme Enter Pin Tone",
"Meetme Invalid Pin Tone",
"Meetme Failed Pin Tone",
"Meetme CFB Failed Tone",
"Meetme Enter Access Code Tone",
"Meetme Access Code Invalid Tone",
"Meetme Access Code Failed Tone",
};

int skinny_tone_exists(int skinny_tone_int_value) {
	static const int skinny_tones[] = {SKINNY_TONE_SILENCE,SKINNY_TONE_DTMF1,SKINNY_TONE_DTMF2,SKINNY_TONE_DTMF3,SKINNY_TONE_DTMF4,SKINNY_TONE_DTMF5,SKINNY_TONE_DTMF6,SKINNY_TONE_DTMF7,SKINNY_TONE_DTMF8,SKINNY_TONE_DTMF9,SKINNY_TONE_DTMF0,SKINNY_TONE_DTMFSTAR,SKINNY_TONE_DTMFPOUND,SKINNY_TONE_DTMFA,SKINNY_TONE_DTMFB,SKINNY_TONE_DTMFC,SKINNY_TONE_DTMFD,SKINNY_TONE_INSIDEDIALTONE,SKINNY_TONE_OUTSIDEDIALTONE,SKINNY_TONE_LINEBUSYTONE,SKINNY_TONE_ALERTINGTONE,SKINNY_TONE_REORDERTONE,SKINNY_TONE_RECORDERWARNINGTONE,SKINNY_TONE_RECORDERDETECTEDTONE,SKINNY_TONE_REVERTINGTONE,SKINNY_TONE_RECEIVEROFFHOOKTONE,SKINNY_TONE_PARTIALDIALTONE,SKINNY_TONE_NOSUCHNUMBERTONE,SKINNY_TONE_BUSYVERIFICATIONTONE,SKINNY_TONE_CALLWAITINGTONE,SKINNY_TONE_CONFIRMATIONTONE,SKINNY_TONE_CAMPONINDICATIONTONE,SKINNY_TONE_RECALLDIALTONE,SKINNY_TONE_ZIPZIP,SKINNY_TONE_ZIP,SKINNY_TONE_BEEPBONK,SKINNY_TONE_MUSICTONE,SKINNY_TONE_HOLDTONE,SKINNY_TONE_TESTTONE,SKINNY_TONE_DTMONITORWARNINGTONE,SKINNY_TONE_ADDCALLWAITING,SKINNY_TONE_PRIORITYCALLWAIT,SKINNY_TONE_RECALLDIAL,SKINNY_TONE_BARGIN,SKINNY_TONE_DISTINCTALERT,SKINNY_TONE_PRIORITYALERT,SKINNY_TONE_REMINDERRING,SKINNY_TONE_PRECEDENCE_RINGBACK,SKINNY_TONE_PREEMPTIONTONE,SKINNY_TONE_MF1,SKINNY_TONE_MF2,SKINNY_TONE_MF3,SKINNY_TONE_MF4,SKINNY_TONE_MF5,SKINNY_TONE_MF6,SKINNY_TONE_MF7,SKINNY_TONE_MF8,SKINNY_TONE_MF9,SKINNY_TONE_MF0,SKINNY_TONE_MFKP1,SKINNY_TONE_MFST,SKINNY_TONE_MFKP2,SKINNY_TONE_MFSTP,SKINNY_TONE_MFST3P,SKINNY_TONE_MILLIWATT,SKINNY_TONE_MILLIWATTTEST,SKINNY_TONE_HIGHTONE,SKINNY_TONE_FLASHOVERRIDE,SKINNY_TONE_FLASH,SKINNY_TONE_PRIORITY,SKINNY_TONE_IMMEDIATE,SKINNY_TONE_PREAMPWARN,SKINNY_TONE_2105HZ,SKINNY_TONE_2600HZ,SKINNY_TONE_440HZ,SKINNY_TONE_300HZ,SKINNY_TONE_MLPP_PALA,SKINNY_TONE_MLPP_ICA,SKINNY_TONE_MLPP_VCA,SKINNY_TONE_MLPP_BPA,SKINNY_TONE_MLPP_BNEA,SKINNY_TONE_MLPP_UPA,SKINNY_TONE_NOTONE,SKINNY_TONE_MEETME_GREETING,SKINNY_TONE_MEETME_NUMBER_INVALID,SKINNY_TONE_MEETME_NUMBER_FAILED,SKINNY_TONE_MEETME_ENTER_PIN,SKINNY_TONE_MEETME_INVALID_PIN,SKINNY_TONE_MEETME_FAILED_PIN,SKINNY_TONE_MEETME_CFB_FAILED,SKINNY_TONE_MEETME_ENTER_ACCESS_CODE,SKINNY_TONE_MEETME_ACCESS_CODE_INVALID,SKINNY_TONE_MEETME_ACCESS_CODE_FAILED,};
	uint32_t idx;
	for (idx=0; idx < ARRAY_LEN(skinny_tones); idx++) {
		if (skinny_tones[idx]==skinny_tone_int_value) {
			return 1;
		}
	}
	return 0;
}

const char * skinny_tone2str(skinny_tone_t enum_value) {
	switch(enum_value) {
		case SKINNY_TONE_SILENCE: return skinny_tone_map[0];
		case SKINNY_TONE_DTMF1: return skinny_tone_map[1];
		case SKINNY_TONE_DTMF2: return skinny_tone_map[2];
		case SKINNY_TONE_DTMF3: return skinny_tone_map[3];
		case SKINNY_TONE_DTMF4: return skinny_tone_map[4];
		case SKINNY_TONE_DTMF5: return skinny_tone_map[5];
		case SKINNY_TONE_DTMF6: return skinny_tone_map[6];
		case SKINNY_TONE_DTMF7: return skinny_tone_map[7];
		case SKINNY_TONE_DTMF8: return skinny_tone_map[8];
		case SKINNY_TONE_DTMF9: return skinny_tone_map[9];
		case SKINNY_TONE_DTMF0: return skinny_tone_map[10];
		case SKINNY_TONE_DTMFSTAR: return skinny_tone_map[11];
		case SKINNY_TONE_DTMFPOUND: return skinny_tone_map[12];
		case SKINNY_TONE_DTMFA: return skinny_tone_map[13];
		case SKINNY_TONE_DTMFB: return skinny_tone_map[14];
		case SKINNY_TONE_DTMFC: return skinny_tone_map[15];
		case SKINNY_TONE_DTMFD: return skinny_tone_map[16];
		case SKINNY_TONE_INSIDEDIALTONE: return skinny_tone_map[17];
		case SKINNY_TONE_OUTSIDEDIALTONE: return skinny_tone_map[18];
		case SKINNY_TONE_LINEBUSYTONE: return skinny_tone_map[19];
		case SKINNY_TONE_ALERTINGTONE: return skinny_tone_map[20];
		case SKINNY_TONE_REORDERTONE: return skinny_tone_map[21];
		case SKINNY_TONE_RECORDERWARNINGTONE: return skinny_tone_map[22];
		case SKINNY_TONE_RECORDERDETECTEDTONE: return skinny_tone_map[23];
		case SKINNY_TONE_REVERTINGTONE: return skinny_tone_map[24];
		case SKINNY_TONE_RECEIVEROFFHOOKTONE: return skinny_tone_map[25];
		case SKINNY_TONE_PARTIALDIALTONE: return skinny_tone_map[26];
		case SKINNY_TONE_NOSUCHNUMBERTONE: return skinny_tone_map[27];
		case SKINNY_TONE_BUSYVERIFICATIONTONE: return skinny_tone_map[28];
		case SKINNY_TONE_CALLWAITINGTONE: return skinny_tone_map[29];
		case SKINNY_TONE_CONFIRMATIONTONE: return skinny_tone_map[30];
		case SKINNY_TONE_CAMPONINDICATIONTONE: return skinny_tone_map[31];
		case SKINNY_TONE_RECALLDIALTONE: return skinny_tone_map[32];
		case SKINNY_TONE_ZIPZIP: return skinny_tone_map[33];
		case SKINNY_TONE_ZIP: return skinny_tone_map[34];
		case SKINNY_TONE_BEEPBONK: return skinny_tone_map[35];
		case SKINNY_TONE_MUSICTONE: return skinny_tone_map[36];
		case SKINNY_TONE_HOLDTONE: return skinny_tone_map[37];
		case SKINNY_TONE_TESTTONE: return skinny_tone_map[38];
		case SKINNY_TONE_DTMONITORWARNINGTONE: return skinny_tone_map[39];
		case SKINNY_TONE_ADDCALLWAITING: return skinny_tone_map[40];
		case SKINNY_TONE_PRIORITYCALLWAIT: return skinny_tone_map[41];
		case SKINNY_TONE_RECALLDIAL: return skinny_tone_map[42];
		case SKINNY_TONE_BARGIN: return skinny_tone_map[43];
		case SKINNY_TONE_DISTINCTALERT: return skinny_tone_map[44];
		case SKINNY_TONE_PRIORITYALERT: return skinny_tone_map[45];
		case SKINNY_TONE_REMINDERRING: return skinny_tone_map[46];
		case SKINNY_TONE_PRECEDENCE_RINGBACK: return skinny_tone_map[47];
		case SKINNY_TONE_PREEMPTIONTONE: return skinny_tone_map[48];
		case SKINNY_TONE_MF1: return skinny_tone_map[49];
		case SKINNY_TONE_MF2: return skinny_tone_map[50];
		case SKINNY_TONE_MF3: return skinny_tone_map[51];
		case SKINNY_TONE_MF4: return skinny_tone_map[52];
		case SKINNY_TONE_MF5: return skinny_tone_map[53];
		case SKINNY_TONE_MF6: return skinny_tone_map[54];
		case SKINNY_TONE_MF7: return skinny_tone_map[55];
		case SKINNY_TONE_MF8: return skinny_tone_map[56];
		case SKINNY_TONE_MF9: return skinny_tone_map[57];
		case SKINNY_TONE_MF0: return skinny_tone_map[58];
		case SKINNY_TONE_MFKP1: return skinny_tone_map[59];
		case SKINNY_TONE_MFST: return skinny_tone_map[60];
		case SKINNY_TONE_MFKP2: return skinny_tone_map[61];
		case SKINNY_TONE_MFSTP: return skinny_tone_map[62];
		case SKINNY_TONE_MFST3P: return skinny_tone_map[63];
		case SKINNY_TONE_MILLIWATT: return skinny_tone_map[64];
		case SKINNY_TONE_MILLIWATTTEST: return skinny_tone_map[65];
		case SKINNY_TONE_HIGHTONE: return skinny_tone_map[66];
		case SKINNY_TONE_FLASHOVERRIDE: return skinny_tone_map[67];
		case SKINNY_TONE_FLASH: return skinny_tone_map[68];
		case SKINNY_TONE_PRIORITY: return skinny_tone_map[69];
		case SKINNY_TONE_IMMEDIATE: return skinny_tone_map[70];
		case SKINNY_TONE_PREAMPWARN: return skinny_tone_map[71];
		case SKINNY_TONE_2105HZ: return skinny_tone_map[72];
		case SKINNY_TONE_2600HZ: return skinny_tone_map[73];
		case SKINNY_TONE_440HZ: return skinny_tone_map[74];
		case SKINNY_TONE_300HZ: return skinny_tone_map[75];
		case SKINNY_TONE_MLPP_PALA: return skinny_tone_map[76];
		case SKINNY_TONE_MLPP_ICA: return skinny_tone_map[77];
		case SKINNY_TONE_MLPP_VCA: return skinny_tone_map[78];
		case SKINNY_TONE_MLPP_BPA: return skinny_tone_map[79];
		case SKINNY_TONE_MLPP_BNEA: return skinny_tone_map[80];
		case SKINNY_TONE_MLPP_UPA: return skinny_tone_map[81];
		case SKINNY_TONE_NOTONE: return skinny_tone_map[82];
		case SKINNY_TONE_MEETME_GREETING: return skinny_tone_map[83];
		case SKINNY_TONE_MEETME_NUMBER_INVALID: return skinny_tone_map[84];
		case SKINNY_TONE_MEETME_NUMBER_FAILED: return skinny_tone_map[85];
		case SKINNY_TONE_MEETME_ENTER_PIN: return skinny_tone_map[86];
		case SKINNY_TONE_MEETME_INVALID_PIN: return skinny_tone_map[87];
		case SKINNY_TONE_MEETME_FAILED_PIN: return skinny_tone_map[88];
		case SKINNY_TONE_MEETME_CFB_FAILED: return skinny_tone_map[89];
		case SKINNY_TONE_MEETME_ENTER_ACCESS_CODE: return skinny_tone_map[90];
		case SKINNY_TONE_MEETME_ACCESS_CODE_INVALID: return skinny_tone_map[91];
		case SKINNY_TONE_MEETME_ACCESS_CODE_FAILED: return skinny_tone_map[92];
		default:
			pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_tone2str\n", enum_value);
			return "SCCP: OutOfBounds Error during lookup of sparse skinny_tone2str\n";
	}
}

skinny_tone_t skinny_tone_str2val(const char *lookup_str) {
	if        (sccp_strcaseequals(skinny_tone_map[0], lookup_str)) {
		return SKINNY_TONE_SILENCE;
	} else if (sccp_strcaseequals(skinny_tone_map[1], lookup_str)) {
		return SKINNY_TONE_DTMF1;
	} else if (sccp_strcaseequals(skinny_tone_map[2], lookup_str)) {
		return SKINNY_TONE_DTMF2;
	} else if (sccp_strcaseequals(skinny_tone_map[3], lookup_str)) {
		return SKINNY_TONE_DTMF3;
	} else if (sccp_strcaseequals(skinny_tone_map[4], lookup_str)) {
		return SKINNY_TONE_DTMF4;
	} else if (sccp_strcaseequals(skinny_tone_map[5], lookup_str)) {
		return SKINNY_TONE_DTMF5;
	} else if (sccp_strcaseequals(skinny_tone_map[6], lookup_str)) {
		return SKINNY_TONE_DTMF6;
	} else if (sccp_strcaseequals(skinny_tone_map[7], lookup_str)) {
		return SKINNY_TONE_DTMF7;
	} else if (sccp_strcaseequals(skinny_tone_map[8], lookup_str)) {
		return SKINNY_TONE_DTMF8;
	} else if (sccp_strcaseequals(skinny_tone_map[9], lookup_str)) {
		return SKINNY_TONE_DTMF9;
	} else if (sccp_strcaseequals(skinny_tone_map[10], lookup_str)) {
		return SKINNY_TONE_DTMF0;
	} else if (sccp_strcaseequals(skinny_tone_map[11], lookup_str)) {
		return SKINNY_TONE_DTMFSTAR;
	} else if (sccp_strcaseequals(skinny_tone_map[12], lookup_str)) {
		return SKINNY_TONE_DTMFPOUND;
	} else if (sccp_strcaseequals(skinny_tone_map[13], lookup_str)) {
		return SKINNY_TONE_DTMFA;
	} else if (sccp_strcaseequals(skinny_tone_map[14], lookup_str)) {
		return SKINNY_TONE_DTMFB;
	} else if (sccp_strcaseequals(skinny_tone_map[15], lookup_str)) {
		return SKINNY_TONE_DTMFC;
	} else if (sccp_strcaseequals(skinny_tone_map[16], lookup_str)) {
		return SKINNY_TONE_DTMFD;
	} else if (sccp_strcaseequals(skinny_tone_map[17], lookup_str)) {
		return SKINNY_TONE_INSIDEDIALTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[18], lookup_str)) {
		return SKINNY_TONE_OUTSIDEDIALTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[19], lookup_str)) {
		return SKINNY_TONE_LINEBUSYTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[20], lookup_str)) {
		return SKINNY_TONE_ALERTINGTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[21], lookup_str)) {
		return SKINNY_TONE_REORDERTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[22], lookup_str)) {
		return SKINNY_TONE_RECORDERWARNINGTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[23], lookup_str)) {
		return SKINNY_TONE_RECORDERDETECTEDTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[24], lookup_str)) {
		return SKINNY_TONE_REVERTINGTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[25], lookup_str)) {
		return SKINNY_TONE_RECEIVEROFFHOOKTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[26], lookup_str)) {
		return SKINNY_TONE_PARTIALDIALTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[27], lookup_str)) {
		return SKINNY_TONE_NOSUCHNUMBERTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[28], lookup_str)) {
		return SKINNY_TONE_BUSYVERIFICATIONTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[29], lookup_str)) {
		return SKINNY_TONE_CALLWAITINGTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[30], lookup_str)) {
		return SKINNY_TONE_CONFIRMATIONTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[31], lookup_str)) {
		return SKINNY_TONE_CAMPONINDICATIONTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[32], lookup_str)) {
		return SKINNY_TONE_RECALLDIALTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[33], lookup_str)) {
		return SKINNY_TONE_ZIPZIP;
	} else if (sccp_strcaseequals(skinny_tone_map[34], lookup_str)) {
		return SKINNY_TONE_ZIP;
	} else if (sccp_strcaseequals(skinny_tone_map[35], lookup_str)) {
		return SKINNY_TONE_BEEPBONK;
	} else if (sccp_strcaseequals(skinny_tone_map[36], lookup_str)) {
		return SKINNY_TONE_MUSICTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[37], lookup_str)) {
		return SKINNY_TONE_HOLDTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[38], lookup_str)) {
		return SKINNY_TONE_TESTTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[39], lookup_str)) {
		return SKINNY_TONE_DTMONITORWARNINGTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[40], lookup_str)) {
		return SKINNY_TONE_ADDCALLWAITING;
	} else if (sccp_strcaseequals(skinny_tone_map[41], lookup_str)) {
		return SKINNY_TONE_PRIORITYCALLWAIT;
	} else if (sccp_strcaseequals(skinny_tone_map[42], lookup_str)) {
		return SKINNY_TONE_RECALLDIAL;
	} else if (sccp_strcaseequals(skinny_tone_map[43], lookup_str)) {
		return SKINNY_TONE_BARGIN;
	} else if (sccp_strcaseequals(skinny_tone_map[44], lookup_str)) {
		return SKINNY_TONE_DISTINCTALERT;
	} else if (sccp_strcaseequals(skinny_tone_map[45], lookup_str)) {
		return SKINNY_TONE_PRIORITYALERT;
	} else if (sccp_strcaseequals(skinny_tone_map[46], lookup_str)) {
		return SKINNY_TONE_REMINDERRING;
	} else if (sccp_strcaseequals(skinny_tone_map[47], lookup_str)) {
		return SKINNY_TONE_PRECEDENCE_RINGBACK;
	} else if (sccp_strcaseequals(skinny_tone_map[48], lookup_str)) {
		return SKINNY_TONE_PREEMPTIONTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[49], lookup_str)) {
		return SKINNY_TONE_MF1;
	} else if (sccp_strcaseequals(skinny_tone_map[50], lookup_str)) {
		return SKINNY_TONE_MF2;
	} else if (sccp_strcaseequals(skinny_tone_map[51], lookup_str)) {
		return SKINNY_TONE_MF3;
	} else if (sccp_strcaseequals(skinny_tone_map[52], lookup_str)) {
		return SKINNY_TONE_MF4;
	} else if (sccp_strcaseequals(skinny_tone_map[53], lookup_str)) {
		return SKINNY_TONE_MF5;
	} else if (sccp_strcaseequals(skinny_tone_map[54], lookup_str)) {
		return SKINNY_TONE_MF6;
	} else if (sccp_strcaseequals(skinny_tone_map[55], lookup_str)) {
		return SKINNY_TONE_MF7;
	} else if (sccp_strcaseequals(skinny_tone_map[56], lookup_str)) {
		return SKINNY_TONE_MF8;
	} else if (sccp_strcaseequals(skinny_tone_map[57], lookup_str)) {
		return SKINNY_TONE_MF9;
	} else if (sccp_strcaseequals(skinny_tone_map[58], lookup_str)) {
		return SKINNY_TONE_MF0;
	} else if (sccp_strcaseequals(skinny_tone_map[59], lookup_str)) {
		return SKINNY_TONE_MFKP1;
	} else if (sccp_strcaseequals(skinny_tone_map[60], lookup_str)) {
		return SKINNY_TONE_MFST;
	} else if (sccp_strcaseequals(skinny_tone_map[61], lookup_str)) {
		return SKINNY_TONE_MFKP2;
	} else if (sccp_strcaseequals(skinny_tone_map[62], lookup_str)) {
		return SKINNY_TONE_MFSTP;
	} else if (sccp_strcaseequals(skinny_tone_map[63], lookup_str)) {
		return SKINNY_TONE_MFST3P;
	} else if (sccp_strcaseequals(skinny_tone_map[64], lookup_str)) {
		return SKINNY_TONE_MILLIWATT;
	} else if (sccp_strcaseequals(skinny_tone_map[65], lookup_str)) {
		return SKINNY_TONE_MILLIWATTTEST;
	} else if (sccp_strcaseequals(skinny_tone_map[66], lookup_str)) {
		return SKINNY_TONE_HIGHTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[67], lookup_str)) {
		return SKINNY_TONE_FLASHOVERRIDE;
	} else if (sccp_strcaseequals(skinny_tone_map[68], lookup_str)) {
		return SKINNY_TONE_FLASH;
	} else if (sccp_strcaseequals(skinny_tone_map[69], lookup_str)) {
		return SKINNY_TONE_PRIORITY;
	} else if (sccp_strcaseequals(skinny_tone_map[70], lookup_str)) {
		return SKINNY_TONE_IMMEDIATE;
	} else if (sccp_strcaseequals(skinny_tone_map[71], lookup_str)) {
		return SKINNY_TONE_PREAMPWARN;
	} else if (sccp_strcaseequals(skinny_tone_map[72], lookup_str)) {
		return SKINNY_TONE_2105HZ;
	} else if (sccp_strcaseequals(skinny_tone_map[73], lookup_str)) {
		return SKINNY_TONE_2600HZ;
	} else if (sccp_strcaseequals(skinny_tone_map[74], lookup_str)) {
		return SKINNY_TONE_440HZ;
	} else if (sccp_strcaseequals(skinny_tone_map[75], lookup_str)) {
		return SKINNY_TONE_300HZ;
	} else if (sccp_strcaseequals(skinny_tone_map[76], lookup_str)) {
		return SKINNY_TONE_MLPP_PALA;
	} else if (sccp_strcaseequals(skinny_tone_map[77], lookup_str)) {
		return SKINNY_TONE_MLPP_ICA;
	} else if (sccp_strcaseequals(skinny_tone_map[78], lookup_str)) {
		return SKINNY_TONE_MLPP_VCA;
	} else if (sccp_strcaseequals(skinny_tone_map[79], lookup_str)) {
		return SKINNY_TONE_MLPP_BPA;
	} else if (sccp_strcaseequals(skinny_tone_map[80], lookup_str)) {
		return SKINNY_TONE_MLPP_BNEA;
	} else if (sccp_strcaseequals(skinny_tone_map[81], lookup_str)) {
		return SKINNY_TONE_MLPP_UPA;
	} else if (sccp_strcaseequals(skinny_tone_map[82], lookup_str)) {
		return SKINNY_TONE_NOTONE;
	} else if (sccp_strcaseequals(skinny_tone_map[83], lookup_str)) {
		return SKINNY_TONE_MEETME_GREETING;
	} else if (sccp_strcaseequals(skinny_tone_map[84], lookup_str)) {
		return SKINNY_TONE_MEETME_NUMBER_INVALID;
	} else if (sccp_strcaseequals(skinny_tone_map[85], lookup_str)) {
		return SKINNY_TONE_MEETME_NUMBER_FAILED;
	} else if (sccp_strcaseequals(skinny_tone_map[86], lookup_str)) {
		return SKINNY_TONE_MEETME_ENTER_PIN;
	} else if (sccp_strcaseequals(skinny_tone_map[87], lookup_str)) {
		return SKINNY_TONE_MEETME_INVALID_PIN;
	} else if (sccp_strcaseequals(skinny_tone_map[88], lookup_str)) {
		return SKINNY_TONE_MEETME_FAILED_PIN;
	} else if (sccp_strcaseequals(skinny_tone_map[89], lookup_str)) {
		return SKINNY_TONE_MEETME_CFB_FAILED;
	} else if (sccp_strcaseequals(skinny_tone_map[90], lookup_str)) {
		return SKINNY_TONE_MEETME_ENTER_ACCESS_CODE;
	} else if (sccp_strcaseequals(skinny_tone_map[91], lookup_str)) {
		return SKINNY_TONE_MEETME_ACCESS_CODE_INVALID;
	} else if (sccp_strcaseequals(skinny_tone_map[92], lookup_str)) {
		return SKINNY_TONE_MEETME_ACCESS_CODE_FAILED;
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_tone_str2val(%s) not found\n", lookup_str);
	return SKINNY_TONE_SENTINEL;
}

int skinny_tone_str2intval(const char *lookup_str) {
	int res = skinny_tone_str2val(lookup_str);
	return (int)res != SKINNY_TONE_SENTINEL ? res : -1;
}

char *skinny_tone_all_entries(void) {
	static char res[] = "Silence,DTMF 1,DTMF 2,DTMF 3,DTMF 4,DTMF 5,DTMF 6,DTMF 7,DTMF 8,DTMF 9,DTMF 0,DTMF Star,DTMF Pound,DTMF A,DTMF B,DTMF C,DTMF D,Inside Dial Tone,Outside Dial Tone,Line Busy Tone,Alerting Tone,Reorder Tone,Recorder Warning Tone,Recorder Detected Tone,Reverting Tone,Receiver OffHook Tone,Partial Dial Tone,No Such Number Tone,Busy Verification Tone,Call Waiting Tone,Confirmation Tone,Camp On Indication Tone,Recall Dial Tone,Zip Zip,Zip,Beep Bonk,Music Tone,Hold Tone,Test Tone,DT Monitor Warning Tone,Add Call Waiting,Priority Call Wait,Recall Dial,Barg In,Distinct Alert,Priority Alert,Reminder Ring,Precedence RingBank,Pre-EmptionTone,MF1,MF2,MF3,MF4,MF5,MF6,MF7,MF8,MF9,MF0,MFKP1,MFST,MFKP2,MFSTP,MFST3P,MILLIWATT,MILLIWATT TEST,HIGH TONE,FLASH OVERRIDE,FLASH,PRIORITY,IMMEDIATE,PRE-AMP WARN,2105 HZ,2600 HZ,440 HZ,300 HZ,MLPP Pala,MLPP Ica,MLPP Vca,MLPP Bpa,MLPP Bnea,MLPP Upa,No Tone,Meetme Greeting Tone,Meetme Number Invalid Tone,Meetme Number Failed Tone,Meetme Enter Pin Tone,Meetme Invalid Pin Tone,Meetme Failed Pin Tone,Meetme CFB Failed Tone,Meetme Enter Access Code Tone,Meetme Access Code Invalid Tone,Meetme Access Code Failed Tone";
	return res;
}
/* = End =========================================================================================             sparse skinny_tone === */


/* = Begin =======================================================================================      sparse skinny_videoformat === */

/*!
 * \brief Skinny Video Format (ENUM)
 */
static const char *skinny_videoformat_map[] = {"undefined",
"sqcif (128x96)",
"qcif (176x144)",
"cif (352x288)",
"4cif (704x576)",
"16cif (1408x1152)",
"custom_base",
"unknown",
};

int skinny_videoformat_exists(int skinny_videoformat_int_value) {
	static const int skinny_videoformats[] = {SKINNY_VIDEOFORMAT_UNDEFINED,SKINNY_VIDEOFORMAT_SQCIF,SKINNY_VIDEOFORMAT_QCIF,SKINNY_VIDEOFORMAT_CIF,SKINNY_VIDEOFORMAT_4CIF,SKINNY_VIDEOFORMAT_16CIF,SKINNY_VIDEOFORMAT_CUSTOM,SKINNY_VIDEOFORMAT_UNKNOWN,};
	uint32_t idx;
	for (idx=0; idx < ARRAY_LEN(skinny_videoformats); idx++) {
		if (skinny_videoformats[idx]==skinny_videoformat_int_value) {
			return 1;
		}
	}
	return 0;
}

const char * skinny_videoformat2str(skinny_videoformat_t enum_value) {
	switch(enum_value) {
		case SKINNY_VIDEOFORMAT_UNDEFINED: return skinny_videoformat_map[0];
		case SKINNY_VIDEOFORMAT_SQCIF: return skinny_videoformat_map[1];
		case SKINNY_VIDEOFORMAT_QCIF: return skinny_videoformat_map[2];
		case SKINNY_VIDEOFORMAT_CIF: return skinny_videoformat_map[3];
		case SKINNY_VIDEOFORMAT_4CIF: return skinny_videoformat_map[4];
		case SKINNY_VIDEOFORMAT_16CIF: return skinny_videoformat_map[5];
		case SKINNY_VIDEOFORMAT_CUSTOM: return skinny_videoformat_map[6];
		case SKINNY_VIDEOFORMAT_UNKNOWN: return skinny_videoformat_map[7];
		default:
			pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_videoformat2str\n", enum_value);
			return "SCCP: OutOfBounds Error during lookup of sparse skinny_videoformat2str\n";
	}
}

skinny_videoformat_t skinny_videoformat_str2val(const char *lookup_str) {
	if        (sccp_strcaseequals(skinny_videoformat_map[0], lookup_str)) {
		return SKINNY_VIDEOFORMAT_UNDEFINED;
	} else if (sccp_strcaseequals(skinny_videoformat_map[1], lookup_str)) {
		return SKINNY_VIDEOFORMAT_SQCIF;
	} else if (sccp_strcaseequals(skinny_videoformat_map[2], lookup_str)) {
		return SKINNY_VIDEOFORMAT_QCIF;
	} else if (sccp_strcaseequals(skinny_videoformat_map[3], lookup_str)) {
		return SKINNY_VIDEOFORMAT_CIF;
	} else if (sccp_strcaseequals(skinny_videoformat_map[4], lookup_str)) {
		return SKINNY_VIDEOFORMAT_4CIF;
	} else if (sccp_strcaseequals(skinny_videoformat_map[5], lookup_str)) {
		return SKINNY_VIDEOFORMAT_16CIF;
	} else if (sccp_strcaseequals(skinny_videoformat_map[6], lookup_str)) {
		return SKINNY_VIDEOFORMAT_CUSTOM;
	} else if (sccp_strcaseequals(skinny_videoformat_map[7], lookup_str)) {
		return SKINNY_VIDEOFORMAT_UNKNOWN;
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_videoformat_str2val(%s) not found\n", lookup_str);
	return SKINNY_VIDEOFORMAT_SENTINEL;
}

int skinny_videoformat_str2intval(const char *lookup_str) {
	int res = skinny_videoformat_str2val(lookup_str);
	return (int)res != SKINNY_VIDEOFORMAT_SENTINEL ? res : -1;
}

char *skinny_videoformat_all_entries(void) {
	static char res[] = "undefined,sqcif (128x96),qcif (176x144),cif (352x288),4cif (704x576),16cif (1408x1152),custom_base,unknown";
	return res;
}
/* = End =========================================================================================      sparse skinny_videoformat === */


/* = Begin =======================================================================================                skinny_ringtype === */

/*!
 * \brief Skinny Video Format (ENUM)
 */
static const char *skinny_ringtype_map[] = {
	[SKINNY_RINGTYPE_OFF] = "Ring Off",
	[SKINNY_RINGTYPE_INSIDE] = "Inside",
	[SKINNY_RINGTYPE_OUTSIDE] = "Outside",
	[SKINNY_RINGTYPE_FEATURE] = "Feature",
	[SKINNY_RINGTYPE_SILENT] = "Silent",
	[SKINNY_RINGTYPE_URGENT] = "Urgent",
	[SKINNY_RINGTYPE_BELLCORE_1] = "Bellcore 1",
	[SKINNY_RINGTYPE_BELLCORE_2] = "Bellcore 2",
	[SKINNY_RINGTYPE_BELLCORE_3] = "Bellcore 3",
	[SKINNY_RINGTYPE_BELLCORE_4] = "Bellcore 4",
	[SKINNY_RINGTYPE_BELLCORE_5] = "Bellcore 5",
	[SKINNY_RINGTYPE_SENTINEL] = "LOOKUPERROR"
};

int skinny_ringtype_exists(int skinny_ringtype_int_value) {
	if ((SKINNY_RINGTYPE_INSIDE <=skinny_ringtype_int_value) && (skinny_ringtype_int_value < SKINNY_RINGTYPE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_ringtype2str(skinny_ringtype_t enum_value) {
	if ((SKINNY_RINGTYPE_OFF <= enum_value) && (enum_value <= SKINNY_RINGTYPE_SENTINEL)) {
		return skinny_ringtype_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_ringtype2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_ringtype2str\n";
}

skinny_ringtype_t skinny_ringtype_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_ringtype_map); idx++) {
		if (sccp_strcaseequals(skinny_ringtype_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_ringtype_str2val(%s) not found\n", lookup_str);
	return SKINNY_RINGTYPE_SENTINEL;
}

int skinny_ringtype_str2intval(const char *lookup_str) {
	int res = skinny_ringtype_str2val(lookup_str);
	return (int)res != SKINNY_RINGTYPE_SENTINEL ? res : -1;
}

char *skinny_ringtype_all_entries(void) {
	static char res[] = "Ring Off,Inside,Outside,Feature,Silent,Urgent,Bellcore 1,Bellcore 2,Bellcore 3,Bellcore 4,Bellcore 5";
	return res;
}
/* = End =========================================================================================                skinny_ringtype === */


/* = Begin =======================================================================================         skinny_receivetransmit === */

/*!
 * \brief Skinny Station Receive/Transmit (ENUM)
 */
static const char *skinny_receivetransmit_map[] = {
	[SKINNY_TRANSMITRECEIVE_NONE] = "None",
	[SKINNY_TRANSMITRECEIVE_RECEIVE] = "Receive",
	[SKINNY_TRANSMITRECEIVE_TRANSMIT] = "Transmit",
	[SKINNY_TRANSMITRECEIVE_BOTH] = "Transmit & Receive",
	[SKINNY_RECEIVETRANSMIT_SENTINEL] = "LOOKUPERROR"
};

int skinny_receivetransmit_exists(int skinny_receivetransmit_int_value) {
	if ((SKINNY_TRANSMITRECEIVE_RECEIVE <=skinny_receivetransmit_int_value) && (skinny_receivetransmit_int_value < SKINNY_RECEIVETRANSMIT_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_receivetransmit2str(skinny_receivetransmit_t enum_value) {
	if ((SKINNY_TRANSMITRECEIVE_NONE <= enum_value) && (enum_value <= SKINNY_RECEIVETRANSMIT_SENTINEL)) {
		return skinny_receivetransmit_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_receivetransmit2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_receivetransmit2str\n";
}

skinny_receivetransmit_t skinny_receivetransmit_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_receivetransmit_map); idx++) {
		if (sccp_strcaseequals(skinny_receivetransmit_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_receivetransmit_str2val(%s) not found\n", lookup_str);
	return SKINNY_RECEIVETRANSMIT_SENTINEL;
}

int skinny_receivetransmit_str2intval(const char *lookup_str) {
	int res = skinny_receivetransmit_str2val(lookup_str);
	return (int)res != SKINNY_RECEIVETRANSMIT_SENTINEL ? res : -1;
}

char *skinny_receivetransmit_all_entries(void) {
	static char res[] = "None,Receive,Transmit,Transmit & Receive";
	return res;
}
/* = End =========================================================================================         skinny_receivetransmit === */


/* = Begin =======================================================================================                 skinny_keymode === */

/*!
 * \brief Skinny KeyMode (ENUM)
 */
static const char *skinny_keymode_map[] = {
	[KEYMODE_ONHOOK] = "ONHOOK",
	[KEYMODE_CONNECTED] = "CONNECTED",
	[KEYMODE_ONHOLD] = "ONHOLD",
	[KEYMODE_RINGIN] = "RINGIN",
	[KEYMODE_OFFHOOK] = "OFFHOOK",
	[KEYMODE_CONNTRANS] = "CONNTRANS",
	[KEYMODE_DIGITSFOLL] = "DIGITSFOLL",
	[KEYMODE_CONNCONF] = "CONNCONF",
	[KEYMODE_RINGOUT] = "RINGOUT",
	[KEYMODE_OFFHOOKFEAT] = "OFFHOOKFEAT",
	[KEYMODE_INUSEHINT] = "INUSEHINT",
	[KEYMODE_ONHOOKSTEALABLE] = "ONHOOKSTEALABLE",
	[KEYMODE_EMPTY] = "",
	[SKINNY_KEYMODE_SENTINEL] = "LOOKUPERROR"
};

int skinny_keymode_exists(int skinny_keymode_int_value) {
	if ((KEYMODE_CONNECTED <=skinny_keymode_int_value) && (skinny_keymode_int_value < SKINNY_KEYMODE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_keymode2str(skinny_keymode_t enum_value) {
	if ((KEYMODE_ONHOOK <= enum_value) && (enum_value <= SKINNY_KEYMODE_SENTINEL)) {
		return skinny_keymode_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_keymode2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_keymode2str\n";
}

skinny_keymode_t skinny_keymode_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_keymode_map); idx++) {
		if (sccp_strcaseequals(skinny_keymode_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_keymode_str2val(%s) not found\n", lookup_str);
	return SKINNY_KEYMODE_SENTINEL;
}

int skinny_keymode_str2intval(const char *lookup_str) {
	int res = skinny_keymode_str2val(lookup_str);
	return (int)res != SKINNY_KEYMODE_SENTINEL ? res : -1;
}

char *skinny_keymode_all_entries(void) {
	static char res[] = "ONHOOK,CONNECTED,ONHOLD,RINGIN,OFFHOOK,CONNTRANS,DIGITSFOLL,CONNCONF,RINGOUT,OFFHOOKFEAT,INUSEHINT,ONHOOKSTEALABLE,";
	return res;
}
/* = End =========================================================================================                 skinny_keymode === */


/* = Begin =======================================================================================       skinny_registrationstate === */

/*!
 * \brief Skinny Device Registration (ENUM)
 */
static const char *skinny_registrationstate_map[] = {
	[SKINNY_DEVICE_RS_FAILED] = "Failed",
	[SKINNY_DEVICE_RS_TIMEOUT] = "Time Out",
	[SKINNY_DEVICE_RS_NONE] = "None",
	[SKINNY_DEVICE_RS_TOKEN] = "Token",
	[SKINNY_DEVICE_RS_PROGRESS] = "Progress",
	[SKINNY_DEVICE_RS_OK] = "OK",
	[SKINNY_REGISTRATIONSTATE_SENTINEL] = "LOOKUPERROR"
};

int skinny_registrationstate_exists(int skinny_registrationstate_int_value) {
	if ((SKINNY_DEVICE_RS_TIMEOUT <=skinny_registrationstate_int_value) && (skinny_registrationstate_int_value < SKINNY_REGISTRATIONSTATE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_registrationstate2str(skinny_registrationstate_t enum_value) {
	if ((SKINNY_DEVICE_RS_FAILED <= enum_value) && (enum_value <= SKINNY_REGISTRATIONSTATE_SENTINEL)) {
		return skinny_registrationstate_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_registrationstate2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_registrationstate2str\n";
}

skinny_registrationstate_t skinny_registrationstate_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_registrationstate_map); idx++) {
		if (sccp_strcaseequals(skinny_registrationstate_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_registrationstate_str2val(%s) not found\n", lookup_str);
	return SKINNY_REGISTRATIONSTATE_SENTINEL;
}

int skinny_registrationstate_str2intval(const char *lookup_str) {
	int res = skinny_registrationstate_str2val(lookup_str);
	return (int)res != SKINNY_REGISTRATIONSTATE_SENTINEL ? res : -1;
}

char *skinny_registrationstate_all_entries(void) {
	static char res[] = "Failed,Time Out,None,Token,Progress,OK";
	return res;
}
/* = End =========================================================================================       skinny_registrationstate === */


/* = Begin =======================================================================================             skinny_mediastatus === */

/*!
 * \brief Skinny Media Status (Enum)
 */
static const char *skinny_mediastatus_map[] = {
	[SKINNY_MEDIASTATUS_Ok] = "Media Status: OK",
	[SKINNY_MEDIASTATUS_Unknown] = "Media Error: Unknown",
	[SKINNY_MEDIASTATUS_OutOfChannels] = "Media Error: Out of Channels",
	[SKINNY_MEDIASTATUS_CodecTooComplex] = "Media Error: Codec Too Complex",
	[SKINNY_MEDIASTATUS_InvalidPartyId] = "Media Error: Invalid Party ID",
	[SKINNY_MEDIASTATUS_InvalidCallReference] = "Media Error: Invalid Call Reference",
	[SKINNY_MEDIASTATUS_InvalidCodec] = "Media Error: Invalid Codec",
	[SKINNY_MEDIASTATUS_InvalidPacketSize] = "Media Error: Invalid Packet Size",
	[SKINNY_MEDIASTATUS_OutOfSockets] = "Media Error: Out of Sockets",
	[SKINNY_MEDIASTATUS_EncoderOrDecoderFailed] = "Media Error: Encoder Or Decoder Failed",
	[SKINNY_MEDIASTATUS_InvalidDynPayloadType] = "Media Error: Invalid Dynamic Payload Type",
	[SKINNY_MEDIASTATUS_RequestedIpAddrTypeUnavailable] = "Media Error: Requested IP Address Type if not available",
	[SKINNY_MEDIASTATUS_DeviceOnHook] = "Media Error: Device is on hook",
	[SKINNY_MEDIASTATUS_SENTINEL] = "LOOKUPERROR"
};

int skinny_mediastatus_exists(int skinny_mediastatus_int_value) {
	if ((SKINNY_MEDIASTATUS_Unknown <=skinny_mediastatus_int_value) && (skinny_mediastatus_int_value < SKINNY_MEDIASTATUS_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_mediastatus2str(skinny_mediastatus_t enum_value) {
	if ((SKINNY_MEDIASTATUS_Ok <= enum_value) && (enum_value <= SKINNY_MEDIASTATUS_SENTINEL)) {
		return skinny_mediastatus_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_mediastatus2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_mediastatus2str\n";
}

skinny_mediastatus_t skinny_mediastatus_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_mediastatus_map); idx++) {
		if (sccp_strcaseequals(skinny_mediastatus_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_mediastatus_str2val(%s) not found\n", lookup_str);
	return SKINNY_MEDIASTATUS_SENTINEL;
}

int skinny_mediastatus_str2intval(const char *lookup_str) {
	int res = skinny_mediastatus_str2val(lookup_str);
	return (int)res != SKINNY_MEDIASTATUS_SENTINEL ? res : -1;
}

char *skinny_mediastatus_all_entries(void) {
	static char res[] = "Media Status: OK,Media Error: Unknown,Media Error: Out of Channels,Media Error: Codec Too Complex,Media Error: Invalid Party ID,Media Error: Invalid Call Reference,Media Error: Invalid Codec,Media Error: Invalid Packet Size,Media Error: Out of Sockets,Media Error: Encoder Or Decoder Failed,Media Error: Invalid Dynamic Payload Type,Media Error: Requested IP Address Type if not available,Media Error: Device is on hook";
	return res;
}
/* = End =========================================================================================             skinny_mediastatus === */


/* = Begin =======================================================================================         sparse skinny_stimulus === */

/*!
 * \brief Skinny Stimulus (ENUM)
 * Almost the same as Skinny buttontype !!
 */
static const char *skinny_stimulus_map[] = {"Unused",
"Last Number Redial",
"SpeedDial",
"Hold",
"Transfer",
"Forward All",
"Forward Busy",
"Forward No Answer",
"Display",
"Line",
"T120 Chat",
"T120 Whiteboard",
"T120 Application Sharing",
"T120 File Transfer",
"Video",
"Voicemail",
"Answer Release",
"Auto Answer",
"Select",
"Feature",
"ServiceURL",
"BusyLampField Speeddial",
"Malicious Call",
"Generic App B1",
"Generic App B2",
"Generic App B3",
"Generic App B4",
"Generic App B5",
"Monitor/Multiblink",
"Meet Me Conference",
"Conference",
"Call Park",
"Call Pickup",
"Group Call Pickup",
"Mobility",
"DoNotDisturb",
"ConfList",
"RemoveLastParticipant",
"QRT",
"CallBack",
"OtherPickup",
"VideoMode",
"NewCall",
"EndCall",
"HLog",
"Queuing",
"Test E",
"Test F",
"Test I",
"Messages",
"Directory",
"Application",
"Headset",
"Keypad",
"Aec",
"Undefined",
};

int skinny_stimulus_exists(int skinny_stimulus_int_value) {
	static const int skinny_stimuluss[] = {SKINNY_STIMULUS_UNUSED,SKINNY_STIMULUS_LASTNUMBERREDIAL,SKINNY_STIMULUS_SPEEDDIAL,SKINNY_STIMULUS_HOLD,SKINNY_STIMULUS_TRANSFER,SKINNY_STIMULUS_FORWARDALL,SKINNY_STIMULUS_FORWARDBUSY,SKINNY_STIMULUS_FORWARDNOANSWER,SKINNY_STIMULUS_DISPLAY,SKINNY_STIMULUS_LINE,SKINNY_STIMULUS_T120CHAT,SKINNY_STIMULUS_T120WHITEBOARD,SKINNY_STIMULUS_T120APPLICATIONSHARING,SKINNY_STIMULUS_T120FILETRANSFER,SKINNY_STIMULUS_VIDEO,SKINNY_STIMULUS_VOICEMAIL,SKINNY_STIMULUS_ANSWERRELEASE,SKINNY_STIMULUS_AUTOANSWER,SKINNY_STIMULUS_SELECT,SKINNY_STIMULUS_FEATURE,SKINNY_STIMULUS_SERVICEURL,SKINNY_STIMULUS_BLFSPEEDDIAL,SKINNY_STIMULUS_MALICIOUSCALL,SKINNY_STIMULUS_GENERICAPPB1,SKINNY_STIMULUS_GENERICAPPB2,SKINNY_STIMULUS_GENERICAPPB3,SKINNY_STIMULUS_GENERICAPPB4,SKINNY_STIMULUS_GENERICAPPB5,SKINNY_STIMULUS_MULTIBLINKFEATURE,SKINNY_STIMULUS_MEETMECONFERENCE,SKINNY_STIMULUS_CONFERENCE,SKINNY_STIMULUS_CALLPARK,SKINNY_STIMULUS_CALLPICKUP,SKINNY_STIMULUS_GROUPCALLPICKUP,SKINNY_STIMULUS_MOBILITY,SKINNY_STIMULUS_DO_NOT_DISTURB,SKINNY_STIMULUS_CONF_LIST,SKINNY_STIMULUS_REMOVE_LAST_PARTICIPANT,SKINNY_STIMULUS_QRT,SKINNY_STIMULUS_CALLBACK,SKINNY_STIMULUS_OTHER_PICKUP,SKINNY_STIMULUS_VIDEO_MODE,SKINNY_STIMULUS_NEW_CALL,SKINNY_STIMULUS_END_CALL,SKINNY_STIMULUS_HLOG,SKINNY_STIMULUS_QUEUING,SKINNY_STIMULUS_TESTE,SKINNY_STIMULUS_TESTF,SKINNY_STIMULUS_TESTI,SKINNY_STIMULUS_MESSAGES,SKINNY_STIMULUS_DIRECTORY,SKINNY_STIMULUS_APPLICATION,SKINNY_STIMULUS_HEADSET,SKINNY_STIMULUS_KEYPAD,SKINNY_STIMULUS_AEC,SKINNY_STIMULUS_UNDEFINED,};
	uint32_t idx;
	for (idx=0; idx < ARRAY_LEN(skinny_stimuluss); idx++) {
		if (skinny_stimuluss[idx]==skinny_stimulus_int_value) {
			return 1;
		}
	}
	return 0;
}

const char * skinny_stimulus2str(skinny_stimulus_t enum_value) {
	switch(enum_value) {
		case SKINNY_STIMULUS_UNUSED: return skinny_stimulus_map[0];
		case SKINNY_STIMULUS_LASTNUMBERREDIAL: return skinny_stimulus_map[1];
		case SKINNY_STIMULUS_SPEEDDIAL: return skinny_stimulus_map[2];
		case SKINNY_STIMULUS_HOLD: return skinny_stimulus_map[3];
		case SKINNY_STIMULUS_TRANSFER: return skinny_stimulus_map[4];
		case SKINNY_STIMULUS_FORWARDALL: return skinny_stimulus_map[5];
		case SKINNY_STIMULUS_FORWARDBUSY: return skinny_stimulus_map[6];
		case SKINNY_STIMULUS_FORWARDNOANSWER: return skinny_stimulus_map[7];
		case SKINNY_STIMULUS_DISPLAY: return skinny_stimulus_map[8];
		case SKINNY_STIMULUS_LINE: return skinny_stimulus_map[9];
		case SKINNY_STIMULUS_T120CHAT: return skinny_stimulus_map[10];
		case SKINNY_STIMULUS_T120WHITEBOARD: return skinny_stimulus_map[11];
		case SKINNY_STIMULUS_T120APPLICATIONSHARING: return skinny_stimulus_map[12];
		case SKINNY_STIMULUS_T120FILETRANSFER: return skinny_stimulus_map[13];
		case SKINNY_STIMULUS_VIDEO: return skinny_stimulus_map[14];
		case SKINNY_STIMULUS_VOICEMAIL: return skinny_stimulus_map[15];
		case SKINNY_STIMULUS_ANSWERRELEASE: return skinny_stimulus_map[16];
		case SKINNY_STIMULUS_AUTOANSWER: return skinny_stimulus_map[17];
		case SKINNY_STIMULUS_SELECT: return skinny_stimulus_map[18];
		case SKINNY_STIMULUS_FEATURE: return skinny_stimulus_map[19];
		case SKINNY_STIMULUS_SERVICEURL: return skinny_stimulus_map[20];
		case SKINNY_STIMULUS_BLFSPEEDDIAL: return skinny_stimulus_map[21];
		case SKINNY_STIMULUS_MALICIOUSCALL: return skinny_stimulus_map[22];
		case SKINNY_STIMULUS_GENERICAPPB1: return skinny_stimulus_map[23];
		case SKINNY_STIMULUS_GENERICAPPB2: return skinny_stimulus_map[24];
		case SKINNY_STIMULUS_GENERICAPPB3: return skinny_stimulus_map[25];
		case SKINNY_STIMULUS_GENERICAPPB4: return skinny_stimulus_map[26];
		case SKINNY_STIMULUS_GENERICAPPB5: return skinny_stimulus_map[27];
		case SKINNY_STIMULUS_MULTIBLINKFEATURE: return skinny_stimulus_map[28];
		case SKINNY_STIMULUS_MEETMECONFERENCE: return skinny_stimulus_map[29];
		case SKINNY_STIMULUS_CONFERENCE: return skinny_stimulus_map[30];
		case SKINNY_STIMULUS_CALLPARK: return skinny_stimulus_map[31];
		case SKINNY_STIMULUS_CALLPICKUP: return skinny_stimulus_map[32];
		case SKINNY_STIMULUS_GROUPCALLPICKUP: return skinny_stimulus_map[33];
		case SKINNY_STIMULUS_MOBILITY: return skinny_stimulus_map[34];
		case SKINNY_STIMULUS_DO_NOT_DISTURB: return skinny_stimulus_map[35];
		case SKINNY_STIMULUS_CONF_LIST: return skinny_stimulus_map[36];
		case SKINNY_STIMULUS_REMOVE_LAST_PARTICIPANT: return skinny_stimulus_map[37];
		case SKINNY_STIMULUS_QRT: return skinny_stimulus_map[38];
		case SKINNY_STIMULUS_CALLBACK: return skinny_stimulus_map[39];
		case SKINNY_STIMULUS_OTHER_PICKUP: return skinny_stimulus_map[40];
		case SKINNY_STIMULUS_VIDEO_MODE: return skinny_stimulus_map[41];
		case SKINNY_STIMULUS_NEW_CALL: return skinny_stimulus_map[42];
		case SKINNY_STIMULUS_END_CALL: return skinny_stimulus_map[43];
		case SKINNY_STIMULUS_HLOG: return skinny_stimulus_map[44];
		case SKINNY_STIMULUS_QUEUING: return skinny_stimulus_map[45];
		case SKINNY_STIMULUS_TESTE: return skinny_stimulus_map[46];
		case SKINNY_STIMULUS_TESTF: return skinny_stimulus_map[47];
		case SKINNY_STIMULUS_TESTI: return skinny_stimulus_map[48];
		case SKINNY_STIMULUS_MESSAGES: return skinny_stimulus_map[49];
		case SKINNY_STIMULUS_DIRECTORY: return skinny_stimulus_map[50];
		case SKINNY_STIMULUS_APPLICATION: return skinny_stimulus_map[51];
		case SKINNY_STIMULUS_HEADSET: return skinny_stimulus_map[52];
		case SKINNY_STIMULUS_KEYPAD: return skinny_stimulus_map[53];
		case SKINNY_STIMULUS_AEC: return skinny_stimulus_map[54];
		case SKINNY_STIMULUS_UNDEFINED: return skinny_stimulus_map[55];
		default:
			pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_stimulus2str\n", enum_value);
			return "SCCP: OutOfBounds Error during lookup of sparse skinny_stimulus2str\n";
	}
}

skinny_stimulus_t skinny_stimulus_str2val(const char *lookup_str) {
	if        (sccp_strcaseequals(skinny_stimulus_map[0], lookup_str)) {
		return SKINNY_STIMULUS_UNUSED;
	} else if (sccp_strcaseequals(skinny_stimulus_map[1], lookup_str)) {
		return SKINNY_STIMULUS_LASTNUMBERREDIAL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[2], lookup_str)) {
		return SKINNY_STIMULUS_SPEEDDIAL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[3], lookup_str)) {
		return SKINNY_STIMULUS_HOLD;
	} else if (sccp_strcaseequals(skinny_stimulus_map[4], lookup_str)) {
		return SKINNY_STIMULUS_TRANSFER;
	} else if (sccp_strcaseequals(skinny_stimulus_map[5], lookup_str)) {
		return SKINNY_STIMULUS_FORWARDALL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[6], lookup_str)) {
		return SKINNY_STIMULUS_FORWARDBUSY;
	} else if (sccp_strcaseequals(skinny_stimulus_map[7], lookup_str)) {
		return SKINNY_STIMULUS_FORWARDNOANSWER;
	} else if (sccp_strcaseequals(skinny_stimulus_map[8], lookup_str)) {
		return SKINNY_STIMULUS_DISPLAY;
	} else if (sccp_strcaseequals(skinny_stimulus_map[9], lookup_str)) {
		return SKINNY_STIMULUS_LINE;
	} else if (sccp_strcaseequals(skinny_stimulus_map[10], lookup_str)) {
		return SKINNY_STIMULUS_T120CHAT;
	} else if (sccp_strcaseequals(skinny_stimulus_map[11], lookup_str)) {
		return SKINNY_STIMULUS_T120WHITEBOARD;
	} else if (sccp_strcaseequals(skinny_stimulus_map[12], lookup_str)) {
		return SKINNY_STIMULUS_T120APPLICATIONSHARING;
	} else if (sccp_strcaseequals(skinny_stimulus_map[13], lookup_str)) {
		return SKINNY_STIMULUS_T120FILETRANSFER;
	} else if (sccp_strcaseequals(skinny_stimulus_map[14], lookup_str)) {
		return SKINNY_STIMULUS_VIDEO;
	} else if (sccp_strcaseequals(skinny_stimulus_map[15], lookup_str)) {
		return SKINNY_STIMULUS_VOICEMAIL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[16], lookup_str)) {
		return SKINNY_STIMULUS_ANSWERRELEASE;
	} else if (sccp_strcaseequals(skinny_stimulus_map[17], lookup_str)) {
		return SKINNY_STIMULUS_AUTOANSWER;
	} else if (sccp_strcaseequals(skinny_stimulus_map[18], lookup_str)) {
		return SKINNY_STIMULUS_SELECT;
	} else if (sccp_strcaseequals(skinny_stimulus_map[19], lookup_str)) {
		return SKINNY_STIMULUS_FEATURE;
	} else if (sccp_strcaseequals(skinny_stimulus_map[20], lookup_str)) {
		return SKINNY_STIMULUS_SERVICEURL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[21], lookup_str)) {
		return SKINNY_STIMULUS_BLFSPEEDDIAL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[22], lookup_str)) {
		return SKINNY_STIMULUS_MALICIOUSCALL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[23], lookup_str)) {
		return SKINNY_STIMULUS_GENERICAPPB1;
	} else if (sccp_strcaseequals(skinny_stimulus_map[24], lookup_str)) {
		return SKINNY_STIMULUS_GENERICAPPB2;
	} else if (sccp_strcaseequals(skinny_stimulus_map[25], lookup_str)) {
		return SKINNY_STIMULUS_GENERICAPPB3;
	} else if (sccp_strcaseequals(skinny_stimulus_map[26], lookup_str)) {
		return SKINNY_STIMULUS_GENERICAPPB4;
	} else if (sccp_strcaseequals(skinny_stimulus_map[27], lookup_str)) {
		return SKINNY_STIMULUS_GENERICAPPB5;
	} else if (sccp_strcaseequals(skinny_stimulus_map[28], lookup_str)) {
		return SKINNY_STIMULUS_MULTIBLINKFEATURE;
	} else if (sccp_strcaseequals(skinny_stimulus_map[29], lookup_str)) {
		return SKINNY_STIMULUS_MEETMECONFERENCE;
	} else if (sccp_strcaseequals(skinny_stimulus_map[30], lookup_str)) {
		return SKINNY_STIMULUS_CONFERENCE;
	} else if (sccp_strcaseequals(skinny_stimulus_map[31], lookup_str)) {
		return SKINNY_STIMULUS_CALLPARK;
	} else if (sccp_strcaseequals(skinny_stimulus_map[32], lookup_str)) {
		return SKINNY_STIMULUS_CALLPICKUP;
	} else if (sccp_strcaseequals(skinny_stimulus_map[33], lookup_str)) {
		return SKINNY_STIMULUS_GROUPCALLPICKUP;
	} else if (sccp_strcaseequals(skinny_stimulus_map[34], lookup_str)) {
		return SKINNY_STIMULUS_MOBILITY;
	} else if (sccp_strcaseequals(skinny_stimulus_map[35], lookup_str)) {
		return SKINNY_STIMULUS_DO_NOT_DISTURB;
	} else if (sccp_strcaseequals(skinny_stimulus_map[36], lookup_str)) {
		return SKINNY_STIMULUS_CONF_LIST;
	} else if (sccp_strcaseequals(skinny_stimulus_map[37], lookup_str)) {
		return SKINNY_STIMULUS_REMOVE_LAST_PARTICIPANT;
	} else if (sccp_strcaseequals(skinny_stimulus_map[38], lookup_str)) {
		return SKINNY_STIMULUS_QRT;
	} else if (sccp_strcaseequals(skinny_stimulus_map[39], lookup_str)) {
		return SKINNY_STIMULUS_CALLBACK;
	} else if (sccp_strcaseequals(skinny_stimulus_map[40], lookup_str)) {
		return SKINNY_STIMULUS_OTHER_PICKUP;
	} else if (sccp_strcaseequals(skinny_stimulus_map[41], lookup_str)) {
		return SKINNY_STIMULUS_VIDEO_MODE;
	} else if (sccp_strcaseequals(skinny_stimulus_map[42], lookup_str)) {
		return SKINNY_STIMULUS_NEW_CALL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[43], lookup_str)) {
		return SKINNY_STIMULUS_END_CALL;
	} else if (sccp_strcaseequals(skinny_stimulus_map[44], lookup_str)) {
		return SKINNY_STIMULUS_HLOG;
	} else if (sccp_strcaseequals(skinny_stimulus_map[45], lookup_str)) {
		return SKINNY_STIMULUS_QUEUING;
	} else if (sccp_strcaseequals(skinny_stimulus_map[46], lookup_str)) {
		return SKINNY_STIMULUS_TESTE;
	} else if (sccp_strcaseequals(skinny_stimulus_map[47], lookup_str)) {
		return SKINNY_STIMULUS_TESTF;
	} else if (sccp_strcaseequals(skinny_stimulus_map[48], lookup_str)) {
		return SKINNY_STIMULUS_TESTI;
	} else if (sccp_strcaseequals(skinny_stimulus_map[49], lookup_str)) {
		return SKINNY_STIMULUS_MESSAGES;
	} else if (sccp_strcaseequals(skinny_stimulus_map[50], lookup_str)) {
		return SKINNY_STIMULUS_DIRECTORY;
	} else if (sccp_strcaseequals(skinny_stimulus_map[51], lookup_str)) {
		return SKINNY_STIMULUS_APPLICATION;
	} else if (sccp_strcaseequals(skinny_stimulus_map[52], lookup_str)) {
		return SKINNY_STIMULUS_HEADSET;
	} else if (sccp_strcaseequals(skinny_stimulus_map[53], lookup_str)) {
		return SKINNY_STIMULUS_KEYPAD;
	} else if (sccp_strcaseequals(skinny_stimulus_map[54], lookup_str)) {
		return SKINNY_STIMULUS_AEC;
	} else if (sccp_strcaseequals(skinny_stimulus_map[55], lookup_str)) {
		return SKINNY_STIMULUS_UNDEFINED;
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_stimulus_str2val(%s) not found\n", lookup_str);
	return SKINNY_STIMULUS_SENTINEL;
}

int skinny_stimulus_str2intval(const char *lookup_str) {
	int res = skinny_stimulus_str2val(lookup_str);
	return (int)res != SKINNY_STIMULUS_SENTINEL ? res : -1;
}

char *skinny_stimulus_all_entries(void) {
	static char res[] = "Unused,Last Number Redial,SpeedDial,Hold,Transfer,Forward All,Forward Busy,Forward No Answer,Display,Line,T120 Chat,T120 Whiteboard,T120 Application Sharing,T120 File Transfer,Video,Voicemail,Answer Release,Auto Answer,Select,Feature,ServiceURL,BusyLampField Speeddial,Malicious Call,Generic App B1,Generic App B2,Generic App B3,Generic App B4,Generic App B5,Monitor/Multiblink,Meet Me Conference,Conference,Call Park,Call Pickup,Group Call Pickup,Mobility,DoNotDisturb,ConfList,RemoveLastParticipant,QRT,CallBack,OtherPickup,VideoMode,NewCall,EndCall,HLog,Queuing,Test E,Test F,Test I,Messages,Directory,Application,Headset,Keypad,Aec,Undefined";
	return res;
}
/* = End =========================================================================================         sparse skinny_stimulus === */


/* = Begin =======================================================================================       sparse skinny_buttontype === */

/*!
 * \brief Skinny ButtonType (ENUM)
 * Almost the same as Skinny Stimulus !!
 */
static const char *skinny_buttontype_map[] = {"Unused",
"Last Number Redial",
"SpeedDial",
"Hold",
"Transfer",
"Forward All",
"Forward Busy",
"Forward No Answer",
"Display",
"Line",
"T120 Chat",
"T120 Whiteboard",
"T120 Application Sharing",
"T120 File Transfer",
"Video",
"Voicemail",
"Answer Release",
"Auto Answer",
"Feature",
"ServiceURL",
"BusyLampField Speeddial",
"Generic App B1",
"Generic App B2",
"Generic App B3",
"Generic App B4",
"Generic App B5",
"Monitor/Multiblink",
"Meet Me Conference",
"Conference",
"Call Park",
"Call Pickup",
"Group Call Pickup",
"Mobility",
"DoNotDisturb",
"ConfList",
"RemoveLastParticipant",
"QRT",
"CallBack",
"OtherPickup",
"VideoMode",
"NewCall",
"EndCall",
"HLog",
"Queuing",
"Test E",
"Test F",
"Test I",
"Messages",
"Directory",
"Application",
"Headset",
"Keypad",
"Aec",
"Undefined",
};

int skinny_buttontype_exists(int skinny_buttontype_int_value) {
	static const int skinny_buttontypes[] = {SKINNY_BUTTONTYPE_UNUSED,SKINNY_BUTTONTYPE_LASTNUMBERREDIAL,SKINNY_BUTTONTYPE_SPEEDDIAL,SKINNY_BUTTONTYPE_HOLD,SKINNY_BUTTONTYPE_TRANSFER,SKINNY_BUTTONTYPE_FORWARDALL,SKINNY_BUTTONTYPE_FORWARDBUSY,SKINNY_BUTTONTYPE_FORWARDNOANSWER,SKINNY_BUTTONTYPE_DISPLAY,SKINNY_BUTTONTYPE_LINE,SKINNY_BUTTONTYPE_T120CHAT,SKINNY_BUTTONTYPE_T120WHITEBOARD,SKINNY_BUTTONTYPE_T120APPLICATIONSHARING,SKINNY_BUTTONTYPE_T120FILETRANSFER,SKINNY_BUTTONTYPE_VIDEO,SKINNY_BUTTONTYPE_VOICEMAIL,SKINNY_BUTTONTYPE_ANSWERRELEASE,SKINNY_BUTTONTYPE_AUTOANSWER,SKINNY_BUTTONTYPE_FEATURE,SKINNY_BUTTONTYPE_SERVICEURL,SKINNY_BUTTONTYPE_BLFSPEEDDIAL,SKINNY_BUTTONTYPE_GENERICAPPB1,SKINNY_BUTTONTYPE_GENERICAPPB2,SKINNY_BUTTONTYPE_GENERICAPPB3,SKINNY_BUTTONTYPE_GENERICAPPB4,SKINNY_BUTTONTYPE_GENERICAPPB5,SKINNY_BUTTONTYPE_MULTIBLINKFEATURE,SKINNY_BUTTONTYPE_MEETMECONFERENCE,SKINNY_BUTTONTYPE_CONFERENCE,SKINNY_BUTTONTYPE_CALLPARK,SKINNY_BUTTONTYPE_CALLPICKUP,SKINNY_BUTTONTYPE_GROUPCALLPICKUP,SKINNY_BUTTONTYPE_MOBILITY,SKINNY_BUTTONTYPE_DO_NOT_DISTURB,SKINNY_BUTTONTYPE_CONF_LIST,SKINNY_BUTTONTYPE_REMOVE_LAST_PARTICIPANT,SKINNY_BUTTONTYPE_QRT,SKINNY_BUTTONTYPE_CALLBACK,SKINNY_BUTTONTYPE_OTHER_PICKUP,SKINNY_BUTTONTYPE_VIDEO_MODE,SKINNY_BUTTONTYPE_NEW_CALL,SKINNY_BUTTONTYPE_END_CALL,SKINNY_BUTTONTYPE_HLOG,SKINNY_BUTTONTYPE_QUEUING,SKINNY_BUTTONTYPE_TESTE,SKINNY_BUTTONTYPE_TESTF,SKINNY_BUTTONTYPE_TESTI,SKINNY_BUTTONTYPE_MESSAGES,SKINNY_BUTTONTYPE_DIRECTORY,SKINNY_BUTTONTYPE_APPLICATION,SKINNY_BUTTONTYPE_HEADSET,SKINNY_BUTTONTYPE_KEYPAD,SKINNY_BUTTONTYPE_AEC,SKINNY_BUTTONTYPE_UNDEFINED,};
	uint32_t idx;
	for (idx=0; idx < ARRAY_LEN(skinny_buttontypes); idx++) {
		if (skinny_buttontypes[idx]==skinny_buttontype_int_value) {
			return 1;
		}
	}
	return 0;
}

const char * skinny_buttontype2str(skinny_buttontype_t enum_value) {
	switch(enum_value) {
		case SKINNY_BUTTONTYPE_UNUSED: return skinny_buttontype_map[0];
		case SKINNY_BUTTONTYPE_LASTNUMBERREDIAL: return skinny_buttontype_map[1];
		case SKINNY_BUTTONTYPE_SPEEDDIAL: return skinny_buttontype_map[2];
		case SKINNY_BUTTONTYPE_HOLD: return skinny_buttontype_map[3];
		case SKINNY_BUTTONTYPE_TRANSFER: return skinny_buttontype_map[4];
		case SKINNY_BUTTONTYPE_FORWARDALL: return skinny_buttontype_map[5];
		case SKINNY_BUTTONTYPE_FORWARDBUSY: return skinny_buttontype_map[6];
		case SKINNY_BUTTONTYPE_FORWARDNOANSWER: return skinny_buttontype_map[7];
		case SKINNY_BUTTONTYPE_DISPLAY: return skinny_buttontype_map[8];
		case SKINNY_BUTTONTYPE_LINE: return skinny_buttontype_map[9];
		case SKINNY_BUTTONTYPE_T120CHAT: return skinny_buttontype_map[10];
		case SKINNY_BUTTONTYPE_T120WHITEBOARD: return skinny_buttontype_map[11];
		case SKINNY_BUTTONTYPE_T120APPLICATIONSHARING: return skinny_buttontype_map[12];
		case SKINNY_BUTTONTYPE_T120FILETRANSFER: return skinny_buttontype_map[13];
		case SKINNY_BUTTONTYPE_VIDEO: return skinny_buttontype_map[14];
		case SKINNY_BUTTONTYPE_VOICEMAIL: return skinny_buttontype_map[15];
		case SKINNY_BUTTONTYPE_ANSWERRELEASE: return skinny_buttontype_map[16];
		case SKINNY_BUTTONTYPE_AUTOANSWER: return skinny_buttontype_map[17];
		case SKINNY_BUTTONTYPE_FEATURE: return skinny_buttontype_map[18];
		case SKINNY_BUTTONTYPE_SERVICEURL: return skinny_buttontype_map[19];
		case SKINNY_BUTTONTYPE_BLFSPEEDDIAL: return skinny_buttontype_map[20];
		case SKINNY_BUTTONTYPE_GENERICAPPB1: return skinny_buttontype_map[21];
		case SKINNY_BUTTONTYPE_GENERICAPPB2: return skinny_buttontype_map[22];
		case SKINNY_BUTTONTYPE_GENERICAPPB3: return skinny_buttontype_map[23];
		case SKINNY_BUTTONTYPE_GENERICAPPB4: return skinny_buttontype_map[24];
		case SKINNY_BUTTONTYPE_GENERICAPPB5: return skinny_buttontype_map[25];
		case SKINNY_BUTTONTYPE_MULTIBLINKFEATURE: return skinny_buttontype_map[26];
		case SKINNY_BUTTONTYPE_MEETMECONFERENCE: return skinny_buttontype_map[27];
		case SKINNY_BUTTONTYPE_CONFERENCE: return skinny_buttontype_map[28];
		case SKINNY_BUTTONTYPE_CALLPARK: return skinny_buttontype_map[29];
		case SKINNY_BUTTONTYPE_CALLPICKUP: return skinny_buttontype_map[30];
		case SKINNY_BUTTONTYPE_GROUPCALLPICKUP: return skinny_buttontype_map[31];
		case SKINNY_BUTTONTYPE_MOBILITY: return skinny_buttontype_map[32];
		case SKINNY_BUTTONTYPE_DO_NOT_DISTURB: return skinny_buttontype_map[33];
		case SKINNY_BUTTONTYPE_CONF_LIST: return skinny_buttontype_map[34];
		case SKINNY_BUTTONTYPE_REMOVE_LAST_PARTICIPANT: return skinny_buttontype_map[35];
		case SKINNY_BUTTONTYPE_QRT: return skinny_buttontype_map[36];
		case SKINNY_BUTTONTYPE_CALLBACK: return skinny_buttontype_map[37];
		case SKINNY_BUTTONTYPE_OTHER_PICKUP: return skinny_buttontype_map[38];
		case SKINNY_BUTTONTYPE_VIDEO_MODE: return skinny_buttontype_map[39];
		case SKINNY_BUTTONTYPE_NEW_CALL: return skinny_buttontype_map[40];
		case SKINNY_BUTTONTYPE_END_CALL: return skinny_buttontype_map[41];
		case SKINNY_BUTTONTYPE_HLOG: return skinny_buttontype_map[42];
		case SKINNY_BUTTONTYPE_QUEUING: return skinny_buttontype_map[43];
		case SKINNY_BUTTONTYPE_TESTE: return skinny_buttontype_map[44];
		case SKINNY_BUTTONTYPE_TESTF: return skinny_buttontype_map[45];
		case SKINNY_BUTTONTYPE_TESTI: return skinny_buttontype_map[46];
		case SKINNY_BUTTONTYPE_MESSAGES: return skinny_buttontype_map[47];
		case SKINNY_BUTTONTYPE_DIRECTORY: return skinny_buttontype_map[48];
		case SKINNY_BUTTONTYPE_APPLICATION: return skinny_buttontype_map[49];
		case SKINNY_BUTTONTYPE_HEADSET: return skinny_buttontype_map[50];
		case SKINNY_BUTTONTYPE_KEYPAD: return skinny_buttontype_map[51];
		case SKINNY_BUTTONTYPE_AEC: return skinny_buttontype_map[52];
		case SKINNY_BUTTONTYPE_UNDEFINED: return skinny_buttontype_map[53];
		default:
			pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_buttontype2str\n", enum_value);
			return "SCCP: OutOfBounds Error during lookup of sparse skinny_buttontype2str\n";
	}
}

skinny_buttontype_t skinny_buttontype_str2val(const char *lookup_str) {
	if        (sccp_strcaseequals(skinny_buttontype_map[0], lookup_str)) {
		return SKINNY_BUTTONTYPE_UNUSED;
	} else if (sccp_strcaseequals(skinny_buttontype_map[1], lookup_str)) {
		return SKINNY_BUTTONTYPE_LASTNUMBERREDIAL;
	} else if (sccp_strcaseequals(skinny_buttontype_map[2], lookup_str)) {
		return SKINNY_BUTTONTYPE_SPEEDDIAL;
	} else if (sccp_strcaseequals(skinny_buttontype_map[3], lookup_str)) {
		return SKINNY_BUTTONTYPE_HOLD;
	} else if (sccp_strcaseequals(skinny_buttontype_map[4], lookup_str)) {
		return SKINNY_BUTTONTYPE_TRANSFER;
	} else if (sccp_strcaseequals(skinny_buttontype_map[5], lookup_str)) {
		return SKINNY_BUTTONTYPE_FORWARDALL;
	} else if (sccp_strcaseequals(skinny_buttontype_map[6], lookup_str)) {
		return SKINNY_BUTTONTYPE_FORWARDBUSY;
	} else if (sccp_strcaseequals(skinny_buttontype_map[7], lookup_str)) {
		return SKINNY_BUTTONTYPE_FORWARDNOANSWER;
	} else if (sccp_strcaseequals(skinny_buttontype_map[8], lookup_str)) {
		return SKINNY_BUTTONTYPE_DISPLAY;
	} else if (sccp_strcaseequals(skinny_buttontype_map[9], lookup_str)) {
		return SKINNY_BUTTONTYPE_LINE;
	} else if (sccp_strcaseequals(skinny_buttontype_map[10], lookup_str)) {
		return SKINNY_BUTTONTYPE_T120CHAT;
	} else if (sccp_strcaseequals(skinny_buttontype_map[11], lookup_str)) {
		return SKINNY_BUTTONTYPE_T120WHITEBOARD;
	} else if (sccp_strcaseequals(skinny_buttontype_map[12], lookup_str)) {
		return SKINNY_BUTTONTYPE_T120APPLICATIONSHARING;
	} else if (sccp_strcaseequals(skinny_buttontype_map[13], lookup_str)) {
		return SKINNY_BUTTONTYPE_T120FILETRANSFER;
	} else if (sccp_strcaseequals(skinny_buttontype_map[14], lookup_str)) {
		return SKINNY_BUTTONTYPE_VIDEO;
	} else if (sccp_strcaseequals(skinny_buttontype_map[15], lookup_str)) {
		return SKINNY_BUTTONTYPE_VOICEMAIL;
	} else if (sccp_strcaseequals(skinny_buttontype_map[16], lookup_str)) {
		return SKINNY_BUTTONTYPE_ANSWERRELEASE;
	} else if (sccp_strcaseequals(skinny_buttontype_map[17], lookup_str)) {
		return SKINNY_BUTTONTYPE_AUTOANSWER;
	} else if (sccp_strcaseequals(skinny_buttontype_map[18], lookup_str)) {
		return SKINNY_BUTTONTYPE_FEATURE;
	} else if (sccp_strcaseequals(skinny_buttontype_map[19], lookup_str)) {
		return SKINNY_BUTTONTYPE_SERVICEURL;
	} else if (sccp_strcaseequals(skinny_buttontype_map[20], lookup_str)) {
		return SKINNY_BUTTONTYPE_BLFSPEEDDIAL;
	} else if (sccp_strcaseequals(skinny_buttontype_map[21], lookup_str)) {
		return SKINNY_BUTTONTYPE_GENERICAPPB1;
	} else if (sccp_strcaseequals(skinny_buttontype_map[22], lookup_str)) {
		return SKINNY_BUTTONTYPE_GENERICAPPB2;
	} else if (sccp_strcaseequals(skinny_buttontype_map[23], lookup_str)) {
		return SKINNY_BUTTONTYPE_GENERICAPPB3;
	} else if (sccp_strcaseequals(skinny_buttontype_map[24], lookup_str)) {
		return SKINNY_BUTTONTYPE_GENERICAPPB4;
	} else if (sccp_strcaseequals(skinny_buttontype_map[25], lookup_str)) {
		return SKINNY_BUTTONTYPE_GENERICAPPB5;
	} else if (sccp_strcaseequals(skinny_buttontype_map[26], lookup_str)) {
		return SKINNY_BUTTONTYPE_MULTIBLINKFEATURE;
	} else if (sccp_strcaseequals(skinny_buttontype_map[27], lookup_str)) {
		return SKINNY_BUTTONTYPE_MEETMECONFERENCE;
	} else if (sccp_strcaseequals(skinny_buttontype_map[28], lookup_str)) {
		return SKINNY_BUTTONTYPE_CONFERENCE;
	} else if (sccp_strcaseequals(skinny_buttontype_map[29], lookup_str)) {
		return SKINNY_BUTTONTYPE_CALLPARK;
	} else if (sccp_strcaseequals(skinny_buttontype_map[30], lookup_str)) {
		return SKINNY_BUTTONTYPE_CALLPICKUP;
	} else if (sccp_strcaseequals(skinny_buttontype_map[31], lookup_str)) {
		return SKINNY_BUTTONTYPE_GROUPCALLPICKUP;
	} else if (sccp_strcaseequals(skinny_buttontype_map[32], lookup_str)) {
		return SKINNY_BUTTONTYPE_MOBILITY;
	} else if (sccp_strcaseequals(skinny_buttontype_map[33], lookup_str)) {
		return SKINNY_BUTTONTYPE_DO_NOT_DISTURB;
	} else if (sccp_strcaseequals(skinny_buttontype_map[34], lookup_str)) {
		return SKINNY_BUTTONTYPE_CONF_LIST;
	} else if (sccp_strcaseequals(skinny_buttontype_map[35], lookup_str)) {
		return SKINNY_BUTTONTYPE_REMOVE_LAST_PARTICIPANT;
	} else if (sccp_strcaseequals(skinny_buttontype_map[36], lookup_str)) {
		return SKINNY_BUTTONTYPE_QRT;
	} else if (sccp_strcaseequals(skinny_buttontype_map[37], lookup_str)) {
		return SKINNY_BUTTONTYPE_CALLBACK;
	} else if (sccp_strcaseequals(skinny_buttontype_map[38], lookup_str)) {
		return SKINNY_BUTTONTYPE_OTHER_PICKUP;
	} else if (sccp_strcaseequals(skinny_buttontype_map[39], lookup_str)) {
		return SKINNY_BUTTONTYPE_VIDEO_MODE;
	} else if (sccp_strcaseequals(skinny_buttontype_map[40], lookup_str)) {
		return SKINNY_BUTTONTYPE_NEW_CALL;
	} else if (sccp_strcaseequals(skinny_buttontype_map[41], lookup_str)) {
		return SKINNY_BUTTONTYPE_END_CALL;
	} else if (sccp_strcaseequals(skinny_buttontype_map[42], lookup_str)) {
		return SKINNY_BUTTONTYPE_HLOG;
	} else if (sccp_strcaseequals(skinny_buttontype_map[43], lookup_str)) {
		return SKINNY_BUTTONTYPE_QUEUING;
	} else if (sccp_strcaseequals(skinny_buttontype_map[44], lookup_str)) {
		return SKINNY_BUTTONTYPE_TESTE;
	} else if (sccp_strcaseequals(skinny_buttontype_map[45], lookup_str)) {
		return SKINNY_BUTTONTYPE_TESTF;
	} else if (sccp_strcaseequals(skinny_buttontype_map[46], lookup_str)) {
		return SKINNY_BUTTONTYPE_TESTI;
	} else if (sccp_strcaseequals(skinny_buttontype_map[47], lookup_str)) {
		return SKINNY_BUTTONTYPE_MESSAGES;
	} else if (sccp_strcaseequals(skinny_buttontype_map[48], lookup_str)) {
		return SKINNY_BUTTONTYPE_DIRECTORY;
	} else if (sccp_strcaseequals(skinny_buttontype_map[49], lookup_str)) {
		return SKINNY_BUTTONTYPE_APPLICATION;
	} else if (sccp_strcaseequals(skinny_buttontype_map[50], lookup_str)) {
		return SKINNY_BUTTONTYPE_HEADSET;
	} else if (sccp_strcaseequals(skinny_buttontype_map[51], lookup_str)) {
		return SKINNY_BUTTONTYPE_KEYPAD;
	} else if (sccp_strcaseequals(skinny_buttontype_map[52], lookup_str)) {
		return SKINNY_BUTTONTYPE_AEC;
	} else if (sccp_strcaseequals(skinny_buttontype_map[53], lookup_str)) {
		return SKINNY_BUTTONTYPE_UNDEFINED;
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_buttontype_str2val(%s) not found\n", lookup_str);
	return SKINNY_BUTTONTYPE_SENTINEL;
}

int skinny_buttontype_str2intval(const char *lookup_str) {
	int res = skinny_buttontype_str2val(lookup_str);
	return (int)res != SKINNY_BUTTONTYPE_SENTINEL ? res : -1;
}

char *skinny_buttontype_all_entries(void) {
	static char res[] = "Unused,Last Number Redial,SpeedDial,Hold,Transfer,Forward All,Forward Busy,Forward No Answer,Display,Line,T120 Chat,T120 Whiteboard,T120 Application Sharing,T120 File Transfer,Video,Voicemail,Answer Release,Auto Answer,Feature,ServiceURL,BusyLampField Speeddial,Generic App B1,Generic App B2,Generic App B3,Generic App B4,Generic App B5,Monitor/Multiblink,Meet Me Conference,Conference,Call Park,Call Pickup,Group Call Pickup,Mobility,DoNotDisturb,ConfList,RemoveLastParticipant,QRT,CallBack,OtherPickup,VideoMode,NewCall,EndCall,HLog,Queuing,Test E,Test F,Test I,Messages,Directory,Application,Headset,Keypad,Aec,Undefined";
	return res;
}
/* = End =========================================================================================       sparse skinny_buttontype === */


/* = Begin =======================================================================================       sparse skinny_devicetype === */

/*!
 * \brief Skinny DeviceType (ENUM)
 */
static const char *skinny_devicetype_map[] = {"Undefined: Maybe you forgot the devicetype in your config",
"VGC",
"Cisco Ata 186",
"Cisco Ata 188",
"Virtual 30SP plus",
"Phone Application",
"Analog Access",
"Digital Access PRI",
"Digital Access T1",
"Digital Access Titan2",
"Analog Access Elvis",
"Digital Access Lennon",
"Conference Bridge",
"Conference Bridge Yoko",
"Conference Bridge Dixieland",
"Conference Bridge Summit",
"H225",
"H323 Phone",
"H323 Trunk",
"Music On Hold",
"Pilot",
"Tapi Port",
"Tapi Route Point",
"Voice In Box",
"Voice Inbox Admin",
"Line Annunciator",
"Line Annunciator",
"Line Annunciator",
"Line Annunciator",
"Route List",
"Load Simulator",
"Media Termination Point",
"Media Termination Point Yoko",
"Media Termination Point Dixieland",
"Media Termination Point Summit",
"MGCP Station",
"MGCP Trunk",
"RAS Proxy",
"Trunk",
"Annuciator",
"Monitor Bridge",
"Recorder",
"Monitor Bridge Yoko",
"Sip Trunk",
"Analog Gateway",
"BRI Gateway",
"30SP plus",
"12SP plus",
"12SP",
"12",
"30 VIP",
"Cisco 7902",
"Cisco 7905",
"Cisco 7906",
"Cisco 7910",
"Cisco 7911",
"Cisco 7912",
"Cisco 7920",
"Cisco 7921",
"Cisco 7925",
"Cisco 7926",
"Cisco 7931",
"Cisco 7935",
"Cisco 7936 Conference",
"Cisco 7937 Conference",
"Cisco 7940",
"Cisco 7941",
"Cisco 7941 GE",
"Cisco 7942",
"Cisco 7945",
"Cisco 7960",
"Cisco 7961",
"Cisco 7961 GE",
"Cisco 7962",
"Cisco 7965",
"Cisco 7970",
"Cisco 7971",
"Cisco 7975",
"Cisco 7985",
"Nokia E Series",
"Cisco IP Communicator",
"Nokia ICC client",
"Cisco 6901",
"Cisco 6911",
"Cisco 6921",
"Cisco 6941",
"Cisco 6945",
"Cisco 6961",
"Cisco 8941",
"Cisco 8945",
"Cisco SPA 303G",
"Cisco SPA 502G",
"Cisco SPA 504G",
"Cisco SPA 509G",
"Cisco SPA 521S",
"Cisco SPA 524SG",
"Cisco SPA 525G",
"Cisco SPA 525G2",
"Cisco 7914 AddOn",
"Cisco 7915 AddOn (12 Buttons)",
"Cisco 7915 AddOn (24 Buttons)",
"Cisco 7916 AddOn (12 Buttons)",
"Cisco 7916 AddOn (24 Buttons)",
"Cisco SPA500DS (32 Buttons)",
"Cisco SPA500DS (32 Buttons)",
"Cisco SPA932DS (32 Buttons)",
};

int skinny_devicetype_exists(int skinny_devicetype_int_value) {
	static const int skinny_devicetypes[] = {SKINNY_DEVICETYPE_UNDEFINED,SKINNY_DEVICETYPE_VGC,SKINNY_DEVICETYPE_ATA186,SKINNY_DEVICETYPE_ATA188,SKINNY_DEVICETYPE_VIRTUAL30SPPLUS,SKINNY_DEVICETYPE_PHONEAPPLICATION,SKINNY_DEVICETYPE_ANALOGACCESS,SKINNY_DEVICETYPE_DIGITALACCESSPRI,SKINNY_DEVICETYPE_DIGITALACCESST1,SKINNY_DEVICETYPE_DIGITALACCESSTITAN2,SKINNY_DEVICETYPE_ANALOGACCESSELVIS,SKINNY_DEVICETYPE_DIGITALACCESSLENNON,SKINNY_DEVICETYPE_CONFERENCEBRIDGE,SKINNY_DEVICETYPE_CONFERENCEBRIDGEYOKO,SKINNY_DEVICETYPE_CONFERENCEBRIDGEDIXIELAND,SKINNY_DEVICETYPE_CONFERENCEBRIDGESUMMIT,SKINNY_DEVICETYPE_H225,SKINNY_DEVICETYPE_H323PHONE,SKINNY_DEVICETYPE_H323TRUNK,SKINNY_DEVICETYPE_MUSICONHOLD,SKINNY_DEVICETYPE_PILOT,SKINNY_DEVICETYPE_TAPIPORT,SKINNY_DEVICETYPE_TAPIROUTEPOINT,SKINNY_DEVICETYPE_VOICEINBOX,SKINNY_DEVICETYPE_VOICEINBOXADMIN,SKINNY_DEVICETYPE_LINEANNUNCIATOR,SKINNY_DEVICETYPE_SOFTWAREMTPDIXIELAND,SKINNY_DEVICETYPE_CISCOMEDIASERVER,SKINNY_DEVICETYPE_CONFERENCEBRIDGEFLINT,SKINNY_DEVICETYPE_ROUTELIST,SKINNY_DEVICETYPE_LOADSIMULATOR,SKINNY_DEVICETYPE_MEDIA_TERM_POINT,SKINNY_DEVICETYPE_MEDIA_TERM_POINTYOKO,SKINNY_DEVICETYPE_MEDIA_TERM_POINTDIXIELAND,SKINNY_DEVICETYPE_MEDIA_TERM_POINTSUMMIT,SKINNY_DEVICETYPE_MGCPSTATION,SKINNY_DEVICETYPE_MGCPTRUNK,SKINNY_DEVICETYPE_RASPROXY,SKINNY_DEVICETYPE_TRUNK,SKINNY_DEVICETYPE_ANNUNCIATOR,SKINNY_DEVICETYPE_MONITORBRIDGE,SKINNY_DEVICETYPE_RECORDER,SKINNY_DEVICETYPE_MONITORBRIDGEYOKO,SKINNY_DEVICETYPE_SIPTRUNK,SKINNY_DEVICETYPE_ANALOG_GATEWAY,SKINNY_DEVICETYPE_BRI_GATEWAY,SKINNY_DEVICETYPE_30SPPLUS,SKINNY_DEVICETYPE_12SPPLUS,SKINNY_DEVICETYPE_12SP,SKINNY_DEVICETYPE_12,SKINNY_DEVICETYPE_30VIP,SKINNY_DEVICETYPE_CISCO7902,SKINNY_DEVICETYPE_CISCO7905,SKINNY_DEVICETYPE_CISCO7906,SKINNY_DEVICETYPE_CISCO7910,SKINNY_DEVICETYPE_CISCO7911,SKINNY_DEVICETYPE_CISCO7912,SKINNY_DEVICETYPE_CISCO7920,SKINNY_DEVICETYPE_CISCO7921,SKINNY_DEVICETYPE_CISCO7925,SKINNY_DEVICETYPE_CISCO7926,SKINNY_DEVICETYPE_CISCO7931,SKINNY_DEVICETYPE_CISCO7935,SKINNY_DEVICETYPE_CISCO7936,SKINNY_DEVICETYPE_CISCO7937,SKINNY_DEVICETYPE_CISCO7940,SKINNY_DEVICETYPE_CISCO7941,SKINNY_DEVICETYPE_CISCO7941GE,SKINNY_DEVICETYPE_CISCO7942,SKINNY_DEVICETYPE_CISCO7945,SKINNY_DEVICETYPE_CISCO7960,SKINNY_DEVICETYPE_CISCO7961,SKINNY_DEVICETYPE_CISCO7961GE,SKINNY_DEVICETYPE_CISCO7962,SKINNY_DEVICETYPE_CISCO7965,SKINNY_DEVICETYPE_CISCO7970,SKINNY_DEVICETYPE_CISCO7971,SKINNY_DEVICETYPE_CISCO7975,SKINNY_DEVICETYPE_CISCO7985,SKINNY_DEVICETYPE_NOKIA_E_SERIES,SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR,SKINNY_DEVICETYPE_NOKIA_ICC,SKINNY_DEVICETYPE_CISCO6901,SKINNY_DEVICETYPE_CISCO6911,SKINNY_DEVICETYPE_CISCO6921,SKINNY_DEVICETYPE_CISCO6941,SKINNY_DEVICETYPE_CISCO6945,SKINNY_DEVICETYPE_CISCO6961,SKINNY_DEVICETYPE_CISCO8941,SKINNY_DEVICETYPE_CISCO8945,SKINNY_DEVICETYPE_SPA_303G,SKINNY_DEVICETYPE_SPA_502G,SKINNY_DEVICETYPE_SPA_504G,SKINNY_DEVICETYPE_SPA_509G,SKINNY_DEVICETYPE_SPA_521S,SKINNY_DEVICETYPE_SPA_524SG,SKINNY_DEVICETYPE_SPA_525G,SKINNY_DEVICETYPE_SPA_525G2,SKINNY_DEVICETYPE_CISCO_ADDON_7914,SKINNY_DEVICETYPE_CISCO_ADDON_7915_12BUTTON,SKINNY_DEVICETYPE_CISCO_ADDON_7915_24BUTTON,SKINNY_DEVICETYPE_CISCO_ADDON_7916_12BUTTON,SKINNY_DEVICETYPE_CISCO_ADDON_7916_24BUTTON,SKINNY_DEVICETYPE_CISCO_ADDON_SPA500S,SKINNY_DEVICETYPE_CISCO_ADDON_SPA500DS,SKINNY_DEVICETYPE_CISCO_ADDON_SPA932DS,};
	uint32_t idx;
	for (idx=0; idx < ARRAY_LEN(skinny_devicetypes); idx++) {
		if (skinny_devicetypes[idx]==skinny_devicetype_int_value) {
			return 1;
		}
	}
	return 0;
}

const char * skinny_devicetype2str(skinny_devicetype_t enum_value) {
	switch(enum_value) {
		case SKINNY_DEVICETYPE_UNDEFINED: return skinny_devicetype_map[0];
		case SKINNY_DEVICETYPE_VGC: return skinny_devicetype_map[1];
		case SKINNY_DEVICETYPE_ATA186: return skinny_devicetype_map[2];
		case SKINNY_DEVICETYPE_ATA188: return skinny_devicetype_map[3];
		case SKINNY_DEVICETYPE_VIRTUAL30SPPLUS: return skinny_devicetype_map[4];
		case SKINNY_DEVICETYPE_PHONEAPPLICATION: return skinny_devicetype_map[5];
		case SKINNY_DEVICETYPE_ANALOGACCESS: return skinny_devicetype_map[6];
		case SKINNY_DEVICETYPE_DIGITALACCESSPRI: return skinny_devicetype_map[7];
		case SKINNY_DEVICETYPE_DIGITALACCESST1: return skinny_devicetype_map[8];
		case SKINNY_DEVICETYPE_DIGITALACCESSTITAN2: return skinny_devicetype_map[9];
		case SKINNY_DEVICETYPE_ANALOGACCESSELVIS: return skinny_devicetype_map[10];
		case SKINNY_DEVICETYPE_DIGITALACCESSLENNON: return skinny_devicetype_map[11];
		case SKINNY_DEVICETYPE_CONFERENCEBRIDGE: return skinny_devicetype_map[12];
		case SKINNY_DEVICETYPE_CONFERENCEBRIDGEYOKO: return skinny_devicetype_map[13];
		case SKINNY_DEVICETYPE_CONFERENCEBRIDGEDIXIELAND: return skinny_devicetype_map[14];
		case SKINNY_DEVICETYPE_CONFERENCEBRIDGESUMMIT: return skinny_devicetype_map[15];
		case SKINNY_DEVICETYPE_H225: return skinny_devicetype_map[16];
		case SKINNY_DEVICETYPE_H323PHONE: return skinny_devicetype_map[17];
		case SKINNY_DEVICETYPE_H323TRUNK: return skinny_devicetype_map[18];
		case SKINNY_DEVICETYPE_MUSICONHOLD: return skinny_devicetype_map[19];
		case SKINNY_DEVICETYPE_PILOT: return skinny_devicetype_map[20];
		case SKINNY_DEVICETYPE_TAPIPORT: return skinny_devicetype_map[21];
		case SKINNY_DEVICETYPE_TAPIROUTEPOINT: return skinny_devicetype_map[22];
		case SKINNY_DEVICETYPE_VOICEINBOX: return skinny_devicetype_map[23];
		case SKINNY_DEVICETYPE_VOICEINBOXADMIN: return skinny_devicetype_map[24];
		case SKINNY_DEVICETYPE_LINEANNUNCIATOR: return skinny_devicetype_map[25];
		case SKINNY_DEVICETYPE_SOFTWAREMTPDIXIELAND: return skinny_devicetype_map[26];
		case SKINNY_DEVICETYPE_CISCOMEDIASERVER: return skinny_devicetype_map[27];
		case SKINNY_DEVICETYPE_CONFERENCEBRIDGEFLINT: return skinny_devicetype_map[28];
		case SKINNY_DEVICETYPE_ROUTELIST: return skinny_devicetype_map[29];
		case SKINNY_DEVICETYPE_LOADSIMULATOR: return skinny_devicetype_map[30];
		case SKINNY_DEVICETYPE_MEDIA_TERM_POINT: return skinny_devicetype_map[31];
		case SKINNY_DEVICETYPE_MEDIA_TERM_POINTYOKO: return skinny_devicetype_map[32];
		case SKINNY_DEVICETYPE_MEDIA_TERM_POINTDIXIELAND: return skinny_devicetype_map[33];
		case SKINNY_DEVICETYPE_MEDIA_TERM_POINTSUMMIT: return skinny_devicetype_map[34];
		case SKINNY_DEVICETYPE_MGCPSTATION: return skinny_devicetype_map[35];
		case SKINNY_DEVICETYPE_MGCPTRUNK: return skinny_devicetype_map[36];
		case SKINNY_DEVICETYPE_RASPROXY: return skinny_devicetype_map[37];
		case SKINNY_DEVICETYPE_TRUNK: return skinny_devicetype_map[38];
		case SKINNY_DEVICETYPE_ANNUNCIATOR: return skinny_devicetype_map[39];
		case SKINNY_DEVICETYPE_MONITORBRIDGE: return skinny_devicetype_map[40];
		case SKINNY_DEVICETYPE_RECORDER: return skinny_devicetype_map[41];
		case SKINNY_DEVICETYPE_MONITORBRIDGEYOKO: return skinny_devicetype_map[42];
		case SKINNY_DEVICETYPE_SIPTRUNK: return skinny_devicetype_map[43];
		case SKINNY_DEVICETYPE_ANALOG_GATEWAY: return skinny_devicetype_map[44];
		case SKINNY_DEVICETYPE_BRI_GATEWAY: return skinny_devicetype_map[45];
		case SKINNY_DEVICETYPE_30SPPLUS: return skinny_devicetype_map[46];
		case SKINNY_DEVICETYPE_12SPPLUS: return skinny_devicetype_map[47];
		case SKINNY_DEVICETYPE_12SP: return skinny_devicetype_map[48];
		case SKINNY_DEVICETYPE_12: return skinny_devicetype_map[49];
		case SKINNY_DEVICETYPE_30VIP: return skinny_devicetype_map[50];
		case SKINNY_DEVICETYPE_CISCO7902: return skinny_devicetype_map[51];
		case SKINNY_DEVICETYPE_CISCO7905: return skinny_devicetype_map[52];
		case SKINNY_DEVICETYPE_CISCO7906: return skinny_devicetype_map[53];
		case SKINNY_DEVICETYPE_CISCO7910: return skinny_devicetype_map[54];
		case SKINNY_DEVICETYPE_CISCO7911: return skinny_devicetype_map[55];
		case SKINNY_DEVICETYPE_CISCO7912: return skinny_devicetype_map[56];
		case SKINNY_DEVICETYPE_CISCO7920: return skinny_devicetype_map[57];
		case SKINNY_DEVICETYPE_CISCO7921: return skinny_devicetype_map[58];
		case SKINNY_DEVICETYPE_CISCO7925: return skinny_devicetype_map[59];
		case SKINNY_DEVICETYPE_CISCO7926: return skinny_devicetype_map[60];
		case SKINNY_DEVICETYPE_CISCO7931: return skinny_devicetype_map[61];
		case SKINNY_DEVICETYPE_CISCO7935: return skinny_devicetype_map[62];
		case SKINNY_DEVICETYPE_CISCO7936: return skinny_devicetype_map[63];
		case SKINNY_DEVICETYPE_CISCO7937: return skinny_devicetype_map[64];
		case SKINNY_DEVICETYPE_CISCO7940: return skinny_devicetype_map[65];
		case SKINNY_DEVICETYPE_CISCO7941: return skinny_devicetype_map[66];
		case SKINNY_DEVICETYPE_CISCO7941GE: return skinny_devicetype_map[67];
		case SKINNY_DEVICETYPE_CISCO7942: return skinny_devicetype_map[68];
		case SKINNY_DEVICETYPE_CISCO7945: return skinny_devicetype_map[69];
		case SKINNY_DEVICETYPE_CISCO7960: return skinny_devicetype_map[70];
		case SKINNY_DEVICETYPE_CISCO7961: return skinny_devicetype_map[71];
		case SKINNY_DEVICETYPE_CISCO7961GE: return skinny_devicetype_map[72];
		case SKINNY_DEVICETYPE_CISCO7962: return skinny_devicetype_map[73];
		case SKINNY_DEVICETYPE_CISCO7965: return skinny_devicetype_map[74];
		case SKINNY_DEVICETYPE_CISCO7970: return skinny_devicetype_map[75];
		case SKINNY_DEVICETYPE_CISCO7971: return skinny_devicetype_map[76];
		case SKINNY_DEVICETYPE_CISCO7975: return skinny_devicetype_map[77];
		case SKINNY_DEVICETYPE_CISCO7985: return skinny_devicetype_map[78];
		case SKINNY_DEVICETYPE_NOKIA_E_SERIES: return skinny_devicetype_map[79];
		case SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR: return skinny_devicetype_map[80];
		case SKINNY_DEVICETYPE_NOKIA_ICC: return skinny_devicetype_map[81];
		case SKINNY_DEVICETYPE_CISCO6901: return skinny_devicetype_map[82];
		case SKINNY_DEVICETYPE_CISCO6911: return skinny_devicetype_map[83];
		case SKINNY_DEVICETYPE_CISCO6921: return skinny_devicetype_map[84];
		case SKINNY_DEVICETYPE_CISCO6941: return skinny_devicetype_map[85];
		case SKINNY_DEVICETYPE_CISCO6945: return skinny_devicetype_map[86];
		case SKINNY_DEVICETYPE_CISCO6961: return skinny_devicetype_map[87];
		case SKINNY_DEVICETYPE_CISCO8941: return skinny_devicetype_map[88];
		case SKINNY_DEVICETYPE_CISCO8945: return skinny_devicetype_map[89];
		case SKINNY_DEVICETYPE_SPA_303G: return skinny_devicetype_map[90];
		case SKINNY_DEVICETYPE_SPA_502G: return skinny_devicetype_map[91];
		case SKINNY_DEVICETYPE_SPA_504G: return skinny_devicetype_map[92];
		case SKINNY_DEVICETYPE_SPA_509G: return skinny_devicetype_map[93];
		case SKINNY_DEVICETYPE_SPA_521S: return skinny_devicetype_map[94];
		case SKINNY_DEVICETYPE_SPA_524SG: return skinny_devicetype_map[95];
		case SKINNY_DEVICETYPE_SPA_525G: return skinny_devicetype_map[96];
		case SKINNY_DEVICETYPE_SPA_525G2: return skinny_devicetype_map[97];
		case SKINNY_DEVICETYPE_CISCO_ADDON_7914: return skinny_devicetype_map[98];
		case SKINNY_DEVICETYPE_CISCO_ADDON_7915_12BUTTON: return skinny_devicetype_map[99];
		case SKINNY_DEVICETYPE_CISCO_ADDON_7915_24BUTTON: return skinny_devicetype_map[100];
		case SKINNY_DEVICETYPE_CISCO_ADDON_7916_12BUTTON: return skinny_devicetype_map[101];
		case SKINNY_DEVICETYPE_CISCO_ADDON_7916_24BUTTON: return skinny_devicetype_map[102];
		case SKINNY_DEVICETYPE_CISCO_ADDON_SPA500S: return skinny_devicetype_map[103];
		case SKINNY_DEVICETYPE_CISCO_ADDON_SPA500DS: return skinny_devicetype_map[104];
		case SKINNY_DEVICETYPE_CISCO_ADDON_SPA932DS: return skinny_devicetype_map[105];
		default:
			pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_devicetype2str\n", enum_value);
			return "SCCP: OutOfBounds Error during lookup of sparse skinny_devicetype2str\n";
	}
}

skinny_devicetype_t skinny_devicetype_str2val(const char *lookup_str) {
	if        (sccp_strcaseequals(skinny_devicetype_map[0], lookup_str)) {
		return SKINNY_DEVICETYPE_UNDEFINED;
	} else if (sccp_strcaseequals(skinny_devicetype_map[1], lookup_str)) {
		return SKINNY_DEVICETYPE_VGC;
	} else if (sccp_strcaseequals(skinny_devicetype_map[2], lookup_str)) {
		return SKINNY_DEVICETYPE_ATA186;
	} else if (sccp_strcaseequals(skinny_devicetype_map[3], lookup_str)) {
		return SKINNY_DEVICETYPE_ATA188;
	} else if (sccp_strcaseequals(skinny_devicetype_map[4], lookup_str)) {
		return SKINNY_DEVICETYPE_VIRTUAL30SPPLUS;
	} else if (sccp_strcaseequals(skinny_devicetype_map[5], lookup_str)) {
		return SKINNY_DEVICETYPE_PHONEAPPLICATION;
	} else if (sccp_strcaseequals(skinny_devicetype_map[6], lookup_str)) {
		return SKINNY_DEVICETYPE_ANALOGACCESS;
	} else if (sccp_strcaseequals(skinny_devicetype_map[7], lookup_str)) {
		return SKINNY_DEVICETYPE_DIGITALACCESSPRI;
	} else if (sccp_strcaseequals(skinny_devicetype_map[8], lookup_str)) {
		return SKINNY_DEVICETYPE_DIGITALACCESST1;
	} else if (sccp_strcaseequals(skinny_devicetype_map[9], lookup_str)) {
		return SKINNY_DEVICETYPE_DIGITALACCESSTITAN2;
	} else if (sccp_strcaseequals(skinny_devicetype_map[10], lookup_str)) {
		return SKINNY_DEVICETYPE_ANALOGACCESSELVIS;
	} else if (sccp_strcaseequals(skinny_devicetype_map[11], lookup_str)) {
		return SKINNY_DEVICETYPE_DIGITALACCESSLENNON;
	} else if (sccp_strcaseequals(skinny_devicetype_map[12], lookup_str)) {
		return SKINNY_DEVICETYPE_CONFERENCEBRIDGE;
	} else if (sccp_strcaseequals(skinny_devicetype_map[13], lookup_str)) {
		return SKINNY_DEVICETYPE_CONFERENCEBRIDGEYOKO;
	} else if (sccp_strcaseequals(skinny_devicetype_map[14], lookup_str)) {
		return SKINNY_DEVICETYPE_CONFERENCEBRIDGEDIXIELAND;
	} else if (sccp_strcaseequals(skinny_devicetype_map[15], lookup_str)) {
		return SKINNY_DEVICETYPE_CONFERENCEBRIDGESUMMIT;
	} else if (sccp_strcaseequals(skinny_devicetype_map[16], lookup_str)) {
		return SKINNY_DEVICETYPE_H225;
	} else if (sccp_strcaseequals(skinny_devicetype_map[17], lookup_str)) {
		return SKINNY_DEVICETYPE_H323PHONE;
	} else if (sccp_strcaseequals(skinny_devicetype_map[18], lookup_str)) {
		return SKINNY_DEVICETYPE_H323TRUNK;
	} else if (sccp_strcaseequals(skinny_devicetype_map[19], lookup_str)) {
		return SKINNY_DEVICETYPE_MUSICONHOLD;
	} else if (sccp_strcaseequals(skinny_devicetype_map[20], lookup_str)) {
		return SKINNY_DEVICETYPE_PILOT;
	} else if (sccp_strcaseequals(skinny_devicetype_map[21], lookup_str)) {
		return SKINNY_DEVICETYPE_TAPIPORT;
	} else if (sccp_strcaseequals(skinny_devicetype_map[22], lookup_str)) {
		return SKINNY_DEVICETYPE_TAPIROUTEPOINT;
	} else if (sccp_strcaseequals(skinny_devicetype_map[23], lookup_str)) {
		return SKINNY_DEVICETYPE_VOICEINBOX;
	} else if (sccp_strcaseequals(skinny_devicetype_map[24], lookup_str)) {
		return SKINNY_DEVICETYPE_VOICEINBOXADMIN;
	} else if (sccp_strcaseequals(skinny_devicetype_map[25], lookup_str)) {
		return SKINNY_DEVICETYPE_LINEANNUNCIATOR;
	} else if (sccp_strcaseequals(skinny_devicetype_map[26], lookup_str)) {
		return SKINNY_DEVICETYPE_SOFTWAREMTPDIXIELAND;
	} else if (sccp_strcaseequals(skinny_devicetype_map[27], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCOMEDIASERVER;
	} else if (sccp_strcaseequals(skinny_devicetype_map[28], lookup_str)) {
		return SKINNY_DEVICETYPE_CONFERENCEBRIDGEFLINT;
	} else if (sccp_strcaseequals(skinny_devicetype_map[29], lookup_str)) {
		return SKINNY_DEVICETYPE_ROUTELIST;
	} else if (sccp_strcaseequals(skinny_devicetype_map[30], lookup_str)) {
		return SKINNY_DEVICETYPE_LOADSIMULATOR;
	} else if (sccp_strcaseequals(skinny_devicetype_map[31], lookup_str)) {
		return SKINNY_DEVICETYPE_MEDIA_TERM_POINT;
	} else if (sccp_strcaseequals(skinny_devicetype_map[32], lookup_str)) {
		return SKINNY_DEVICETYPE_MEDIA_TERM_POINTYOKO;
	} else if (sccp_strcaseequals(skinny_devicetype_map[33], lookup_str)) {
		return SKINNY_DEVICETYPE_MEDIA_TERM_POINTDIXIELAND;
	} else if (sccp_strcaseequals(skinny_devicetype_map[34], lookup_str)) {
		return SKINNY_DEVICETYPE_MEDIA_TERM_POINTSUMMIT;
	} else if (sccp_strcaseequals(skinny_devicetype_map[35], lookup_str)) {
		return SKINNY_DEVICETYPE_MGCPSTATION;
	} else if (sccp_strcaseequals(skinny_devicetype_map[36], lookup_str)) {
		return SKINNY_DEVICETYPE_MGCPTRUNK;
	} else if (sccp_strcaseequals(skinny_devicetype_map[37], lookup_str)) {
		return SKINNY_DEVICETYPE_RASPROXY;
	} else if (sccp_strcaseequals(skinny_devicetype_map[38], lookup_str)) {
		return SKINNY_DEVICETYPE_TRUNK;
	} else if (sccp_strcaseequals(skinny_devicetype_map[39], lookup_str)) {
		return SKINNY_DEVICETYPE_ANNUNCIATOR;
	} else if (sccp_strcaseequals(skinny_devicetype_map[40], lookup_str)) {
		return SKINNY_DEVICETYPE_MONITORBRIDGE;
	} else if (sccp_strcaseequals(skinny_devicetype_map[41], lookup_str)) {
		return SKINNY_DEVICETYPE_RECORDER;
	} else if (sccp_strcaseequals(skinny_devicetype_map[42], lookup_str)) {
		return SKINNY_DEVICETYPE_MONITORBRIDGEYOKO;
	} else if (sccp_strcaseequals(skinny_devicetype_map[43], lookup_str)) {
		return SKINNY_DEVICETYPE_SIPTRUNK;
	} else if (sccp_strcaseequals(skinny_devicetype_map[44], lookup_str)) {
		return SKINNY_DEVICETYPE_ANALOG_GATEWAY;
	} else if (sccp_strcaseequals(skinny_devicetype_map[45], lookup_str)) {
		return SKINNY_DEVICETYPE_BRI_GATEWAY;
	} else if (sccp_strcaseequals(skinny_devicetype_map[46], lookup_str)) {
		return SKINNY_DEVICETYPE_30SPPLUS;
	} else if (sccp_strcaseequals(skinny_devicetype_map[47], lookup_str)) {
		return SKINNY_DEVICETYPE_12SPPLUS;
	} else if (sccp_strcaseequals(skinny_devicetype_map[48], lookup_str)) {
		return SKINNY_DEVICETYPE_12SP;
	} else if (sccp_strcaseequals(skinny_devicetype_map[49], lookup_str)) {
		return SKINNY_DEVICETYPE_12;
	} else if (sccp_strcaseequals(skinny_devicetype_map[50], lookup_str)) {
		return SKINNY_DEVICETYPE_30VIP;
	} else if (sccp_strcaseequals(skinny_devicetype_map[51], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7902;
	} else if (sccp_strcaseequals(skinny_devicetype_map[52], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7905;
	} else if (sccp_strcaseequals(skinny_devicetype_map[53], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7906;
	} else if (sccp_strcaseequals(skinny_devicetype_map[54], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7910;
	} else if (sccp_strcaseequals(skinny_devicetype_map[55], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7911;
	} else if (sccp_strcaseequals(skinny_devicetype_map[56], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7912;
	} else if (sccp_strcaseequals(skinny_devicetype_map[57], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7920;
	} else if (sccp_strcaseequals(skinny_devicetype_map[58], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7921;
	} else if (sccp_strcaseequals(skinny_devicetype_map[59], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7925;
	} else if (sccp_strcaseequals(skinny_devicetype_map[60], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7926;
	} else if (sccp_strcaseequals(skinny_devicetype_map[61], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7931;
	} else if (sccp_strcaseequals(skinny_devicetype_map[62], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7935;
	} else if (sccp_strcaseequals(skinny_devicetype_map[63], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7936;
	} else if (sccp_strcaseequals(skinny_devicetype_map[64], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7937;
	} else if (sccp_strcaseequals(skinny_devicetype_map[65], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7940;
	} else if (sccp_strcaseequals(skinny_devicetype_map[66], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7941;
	} else if (sccp_strcaseequals(skinny_devicetype_map[67], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7941GE;
	} else if (sccp_strcaseequals(skinny_devicetype_map[68], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7942;
	} else if (sccp_strcaseequals(skinny_devicetype_map[69], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7945;
	} else if (sccp_strcaseequals(skinny_devicetype_map[70], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7960;
	} else if (sccp_strcaseequals(skinny_devicetype_map[71], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7961;
	} else if (sccp_strcaseequals(skinny_devicetype_map[72], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7961GE;
	} else if (sccp_strcaseequals(skinny_devicetype_map[73], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7962;
	} else if (sccp_strcaseequals(skinny_devicetype_map[74], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7965;
	} else if (sccp_strcaseequals(skinny_devicetype_map[75], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7970;
	} else if (sccp_strcaseequals(skinny_devicetype_map[76], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7971;
	} else if (sccp_strcaseequals(skinny_devicetype_map[77], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7975;
	} else if (sccp_strcaseequals(skinny_devicetype_map[78], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO7985;
	} else if (sccp_strcaseequals(skinny_devicetype_map[79], lookup_str)) {
		return SKINNY_DEVICETYPE_NOKIA_E_SERIES;
	} else if (sccp_strcaseequals(skinny_devicetype_map[80], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_IP_COMMUNICATOR;
	} else if (sccp_strcaseequals(skinny_devicetype_map[81], lookup_str)) {
		return SKINNY_DEVICETYPE_NOKIA_ICC;
	} else if (sccp_strcaseequals(skinny_devicetype_map[82], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO6901;
	} else if (sccp_strcaseequals(skinny_devicetype_map[83], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO6911;
	} else if (sccp_strcaseequals(skinny_devicetype_map[84], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO6921;
	} else if (sccp_strcaseequals(skinny_devicetype_map[85], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO6941;
	} else if (sccp_strcaseequals(skinny_devicetype_map[86], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO6945;
	} else if (sccp_strcaseequals(skinny_devicetype_map[87], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO6961;
	} else if (sccp_strcaseequals(skinny_devicetype_map[88], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO8941;
	} else if (sccp_strcaseequals(skinny_devicetype_map[89], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO8945;
	} else if (sccp_strcaseequals(skinny_devicetype_map[90], lookup_str)) {
		return SKINNY_DEVICETYPE_SPA_303G;
	} else if (sccp_strcaseequals(skinny_devicetype_map[91], lookup_str)) {
		return SKINNY_DEVICETYPE_SPA_502G;
	} else if (sccp_strcaseequals(skinny_devicetype_map[92], lookup_str)) {
		return SKINNY_DEVICETYPE_SPA_504G;
	} else if (sccp_strcaseequals(skinny_devicetype_map[93], lookup_str)) {
		return SKINNY_DEVICETYPE_SPA_509G;
	} else if (sccp_strcaseequals(skinny_devicetype_map[94], lookup_str)) {
		return SKINNY_DEVICETYPE_SPA_521S;
	} else if (sccp_strcaseequals(skinny_devicetype_map[95], lookup_str)) {
		return SKINNY_DEVICETYPE_SPA_524SG;
	} else if (sccp_strcaseequals(skinny_devicetype_map[96], lookup_str)) {
		return SKINNY_DEVICETYPE_SPA_525G;
	} else if (sccp_strcaseequals(skinny_devicetype_map[97], lookup_str)) {
		return SKINNY_DEVICETYPE_SPA_525G2;
	} else if (sccp_strcaseequals(skinny_devicetype_map[98], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7914;
	} else if (sccp_strcaseequals(skinny_devicetype_map[99], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7915_12BUTTON;
	} else if (sccp_strcaseequals(skinny_devicetype_map[100], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7915_24BUTTON;
	} else if (sccp_strcaseequals(skinny_devicetype_map[101], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7916_12BUTTON;
	} else if (sccp_strcaseequals(skinny_devicetype_map[102], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_7916_24BUTTON;
	} else if (sccp_strcaseequals(skinny_devicetype_map[103], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA500S;
	} else if (sccp_strcaseequals(skinny_devicetype_map[104], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA500DS;
	} else if (sccp_strcaseequals(skinny_devicetype_map[105], lookup_str)) {
		return SKINNY_DEVICETYPE_CISCO_ADDON_SPA932DS;
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_devicetype_str2val(%s) not found\n", lookup_str);
	return SKINNY_DEVICETYPE_SENTINEL;
}

int skinny_devicetype_str2intval(const char *lookup_str) {
	int res = skinny_devicetype_str2val(lookup_str);
	return (int)res != SKINNY_DEVICETYPE_SENTINEL ? res : -1;
}

char *skinny_devicetype_all_entries(void) {
	static char res[] = "Undefined: Maybe you forgot the devicetype in your config,VGC,Cisco Ata 186,Cisco Ata 188,Virtual 30SP plus,Phone Application,Analog Access,Digital Access PRI,Digital Access T1,Digital Access Titan2,Analog Access Elvis,Digital Access Lennon,Conference Bridge,Conference Bridge Yoko,Conference Bridge Dixieland,Conference Bridge Summit,H225,H323 Phone,H323 Trunk,Music On Hold,Pilot,Tapi Port,Tapi Route Point,Voice In Box,Voice Inbox Admin,Line Annunciator,Line Annunciator,Line Annunciator,Line Annunciator,Route List,Load Simulator,Media Termination Point,Media Termination Point Yoko,Media Termination Point Dixieland,Media Termination Point Summit,MGCP Station,MGCP Trunk,RAS Proxy,Trunk,Annuciator,Monitor Bridge,Recorder,Monitor Bridge Yoko,Sip Trunk,Analog Gateway,BRI Gateway,30SP plus,12SP plus,12SP,12,30 VIP,Cisco 7902,Cisco 7905,Cisco 7906,Cisco 7910,Cisco 7911,Cisco 7912,Cisco 7920,Cisco 7921,Cisco 7925,Cisco 7926,Cisco 7931,Cisco 7935,Cisco 7936 Conference,Cisco 7937 Conference,Cisco 7940,Cisco 7941,Cisco 7941 GE,Cisco 7942,Cisco 7945,Cisco 7960,Cisco 7961,Cisco 7961 GE,Cisco 7962,Cisco 7965,Cisco 7970,Cisco 7971,Cisco 7975,Cisco 7985,Nokia E Series,Cisco IP Communicator,Nokia ICC client,Cisco 6901,Cisco 6911,Cisco 6921,Cisco 6941,Cisco 6945,Cisco 6961,Cisco 8941,Cisco 8945,Cisco SPA 303G,Cisco SPA 502G,Cisco SPA 504G,Cisco SPA 509G,Cisco SPA 521S,Cisco SPA 524SG,Cisco SPA 525G,Cisco SPA 525G2,Cisco 7914 AddOn,Cisco 7915 AddOn (12 Buttons),Cisco 7915 AddOn (24 Buttons),Cisco 7916 AddOn (12 Buttons),Cisco 7916 AddOn (24 Buttons),Cisco SPA500DS (32 Buttons),Cisco SPA500DS (32 Buttons),Cisco SPA932DS (32 Buttons)";
	return res;
}
/* = End =========================================================================================       sparse skinny_devicetype === */


/* = Begin =======================================================================================        skinny_encryptionmethod === */

/*!
 * \brief Skinny Device Registration (ENUM)
 */
static const char *skinny_encryptionmethod_map[] = {
	[SKINNY_ENCRYPTIONMETHOD_NONE] = "No Encryption",
	[SKINNY_ENCRYPTIONMETHOD_AES_128_HMAC_SHA1_32] = "AES128 SHA1 32",
	[SKINNY_ENCRYPTIONMETHOD_AES_128_HMAC_SHA1_80] = "AES128 SHA1 80",
	[SKINNY_ENCRYPTIONMETHOD_F8_128_HMAC_SHA1_32] = "HMAC_SHA1_32",
	[SKINNY_ENCRYPTIONMETHOD_F8_128_HMAC_SHA1_80] = "HMAC_SHA1_80",
	[SKINNY_ENCRYPTIONMETHOD_AEAD_AES_128_GCM] = "AES 128 GCM",
	[SKINNY_ENCRYPTIONMETHOD_AEAD_AES_256_GCM] = "AES 256 GCM",
	[SKINNY_ENCRYPTIONMETHOD_SENTINEL] = "LOOKUPERROR"
};

int skinny_encryptionmethod_exists(int skinny_encryptionmethod_int_value) {
	if ((SKINNY_ENCRYPTIONMETHOD_AES_128_HMAC_SHA1_32 <=skinny_encryptionmethod_int_value) && (skinny_encryptionmethod_int_value < SKINNY_ENCRYPTIONMETHOD_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_encryptionmethod2str(skinny_encryptionmethod_t enum_value) {
	if ((SKINNY_ENCRYPTIONMETHOD_NONE <= enum_value) && (enum_value <= SKINNY_ENCRYPTIONMETHOD_SENTINEL)) {
		return skinny_encryptionmethod_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_encryptionmethod2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_encryptionmethod2str\n";
}

skinny_encryptionmethod_t skinny_encryptionmethod_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_encryptionmethod_map); idx++) {
		if (sccp_strcaseequals(skinny_encryptionmethod_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_encryptionmethod_str2val(%s) not found\n", lookup_str);
	return SKINNY_ENCRYPTIONMETHOD_SENTINEL;
}

int skinny_encryptionmethod_str2intval(const char *lookup_str) {
	int res = skinny_encryptionmethod_str2val(lookup_str);
	return (int)res != SKINNY_ENCRYPTIONMETHOD_SENTINEL ? res : -1;
}

char *skinny_encryptionmethod_all_entries(void) {
	static char res[] = "No Encryption,AES128 SHA1 32,AES128 SHA1 80,HMAC_SHA1_32,HMAC_SHA1_80,AES 128 GCM,AES 256 GCM";
	return res;
}
/* = End =========================================================================================        skinny_encryptionmethod === */


/* = Begin =======================================================================================         skinny_miscCommandType === */

/*!
 * \brief Skinny Miscellaneous Command Type (Enum)
 */
static const char *skinny_miscCommandType_map[] = {
	[SKINNY_MISCCOMMANDTYPE_VIDEOFREEZEPICTURE] = "videoFreezePicture",
	[SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE] = "videoFastUpdatePicture",
	[SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEGOB] = "videoFastUpdateGOB",
	[SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEMB] = "videoFastUpdateMB",
	[SKINNY_MISCCOMMANDTYPE_LOSTPICTURE] = "lostPicture",
	[SKINNY_MISCCOMMANDTYPE_LOSTPARTIALPICTURE] = "lostPartialPicture",
	[SKINNY_MISCCOMMANDTYPE_RECOVERYREFERENCEPICTURE] = "recoveryReferencePicture",
	[SKINNY_MISCCOMMANDTYPE_TEMPORALSPATIALTRADEOFF] = "temporalSpatialTradeOff",
	[SKINNY_MISCCOMMANDTYPE_SENTINEL] = "LOOKUPERROR"
};

int skinny_miscCommandType_exists(int skinny_miscCommandType_int_value) {
	if ((SKINNY_MISCCOMMANDTYPE_VIDEOFASTUPDATEPICTURE <=skinny_miscCommandType_int_value) && (skinny_miscCommandType_int_value < SKINNY_MISCCOMMANDTYPE_SENTINEL )) {
		return 1;
	}
	return 0;
}

const char * skinny_miscCommandType2str(skinny_miscCommandType_t enum_value) {
	if ((SKINNY_MISCCOMMANDTYPE_VIDEOFREEZEPICTURE <= enum_value) && (enum_value <= SKINNY_MISCCOMMANDTYPE_SENTINEL)) {
		return skinny_miscCommandType_map[enum_value];
	}
	pbx_log(LOG_ERROR, "SCCP: Error during lookup of '%d' in skinny_miscCommandType2str\n", enum_value);
	return "SCCP: OutOfBounds Error during lookup of skinny_miscCommandType2str\n";
}

skinny_miscCommandType_t skinny_miscCommandType_str2val(const char *lookup_str) {
	uint32_t idx;
	for (idx = 0; idx < ARRAY_LEN(skinny_miscCommandType_map); idx++) {
		if (sccp_strcaseequals(skinny_miscCommandType_map[idx], lookup_str)) {
			return idx;
		}
	}
	pbx_log(LOG_ERROR, "SCCP: LOOKUP ERROR, skinny_miscCommandType_str2val(%s) not found\n", lookup_str);
	return SKINNY_MISCCOMMANDTYPE_SENTINEL;
}

int skinny_miscCommandType_str2intval(const char *lookup_str) {
	int res = skinny_miscCommandType_str2val(lookup_str);
	return (int)res != SKINNY_MISCCOMMANDTYPE_SENTINEL ? res : -1;
}

char *skinny_miscCommandType_all_entries(void) {
	static char res[] = "videoFreezePicture,videoFastUpdatePicture,videoFastUpdateGOB,videoFastUpdateMB,lostPicture,lostPartialPicture,recoveryReferencePicture,temporalSpatialTradeOff";
	return res;
}
/* = End =========================================================================================         skinny_miscCommandType === */

