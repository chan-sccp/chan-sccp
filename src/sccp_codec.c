/*!
 * \file        sccp_codec.c
 * \brief       SCCP Codec Class
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#include "config.h"
#include "common.h"
#include "sccp_codec.h"
#include "sccp_utils.h"

const struct skinny_codec skinny_codecs[] = {
	/* *INDENT-OFF* */
	{SKINNY_CODEC_NONE,		SKINNY_CODEC_TYPE_UNKNOWN, 	"",		"",		"No codec", 				NULL,		0,	0,	0},	//payload unknown
	{SKINNY_CODEC_NONSTANDARD,	SKINNY_CODEC_TYPE_UNKNOWN,	"",		"",		"Non-standard codec", 			NULL,		0,	0,	0},	//payload unknown
	{SKINNY_CODEC_IS11172,		SKINNY_CODEC_TYPE_AUDIO,	"is11172",	"is11172",	"IS11172 AudioCap", 			NULL,		8000,	1,	0},	//payload unknown
	{SKINNY_CODEC_IS13818,		SKINNY_CODEC_TYPE_AUDIO,	"is13872",      "is13872",	"IS13818 AudioCap", 			NULL,		8000,	1,	0},	//payload unknown
	{SKINNY_CODEC_GSM_FULLRATE,	SKINNY_CODEC_TYPE_AUDIO, 	"gsm",		"gsm/full",	"GSM Full Rate", 			NULL,		8000,	2,	3},
	{SKINNY_CODEC_GSM_HALFRATE,	SKINNY_CODEC_TYPE_AUDIO, 	"gsm",		"gsm/half",	"GSM Half Rate", 			NULL,		8000,	1,	3},
	{SKINNY_CODEC_GSM_ENH_FULLRATE,	SKINNY_CODEC_TYPE_AUDIO,	"gsm",		"gsm/enh",	"GSM Enhanced Full Rate",		NULL,		8000,	2,	3},
	{SKINNY_CODEC_WIDEBAND_256K,	SKINNY_CODEC_TYPE_AUDIO, 	"slin16",	"slin16",	"Wideband 256k", 			"L16",		16000,	3,	118},
	{SKINNY_CODEC_GSM,		SKINNY_CODEC_TYPE_AUDIO, 	"gsm",		"gsm",		"GSM", 					NULL,		8000,	1,	3},
	{SKINNY_CODEC_ACTIVEVOICE,	SKINNY_CODEC_TYPE_AUDIO, 	"activevoice",	"activevoice",	"ActiveVoice", 				NULL,		8000,	1,	0},	//payload unknown
	{SKINNY_CODEC_G711_ALAW_64K,	SKINNY_CODEC_TYPE_AUDIO, 	"alaw",		"alaw/64k",	"G.711 A-law 64k", 			NULL,		8000,	2,	8},
	{SKINNY_CODEC_G711_ALAW_56K,	SKINNY_CODEC_TYPE_AUDIO, 	"alaw",		"alaw/56k",	"G.711 A-law 56k", 			NULL,		8000,	1,	8},
	{SKINNY_CODEC_G711_ULAW_64K,	SKINNY_CODEC_TYPE_AUDIO, 	"ulaw",		"ulaw/64k",	"G.711 u-law 64k", 			NULL,		8000,	2,	0},
	{SKINNY_CODEC_G711_ULAW_56K,	SKINNY_CODEC_TYPE_AUDIO, 	"ulaw",		"ulaw/56k",	"G.711 u-law 56k", 			NULL,		8000,	1,	0},
	{SKINNY_CODEC_G722_64K,		SKINNY_CODEC_TYPE_AUDIO, 	"g722",		"g722/64k",	"G.722 64k", 				NULL,		8000,	3,	9},
	{SKINNY_CODEC_G722_56K,		SKINNY_CODEC_TYPE_AUDIO, 	"g722",		"g722/56k",	"G.722 56k", 				NULL,		8000,	3,	9},
	{SKINNY_CODEC_G722_48K,		SKINNY_CODEC_TYPE_AUDIO, 	"g722",		"g722/48k",	"G.722 48k", 				NULL,		8000,	2,	9},
	{SKINNY_CODEC_G722_1_24K,	SKINNY_CODEC_TYPE_AUDIO, 	"g722.1",	"g722.1/24k",	"G722.1 24k (Siren7)", 			"G7221",	16000,	3,	102},
	{SKINNY_CODEC_G722_1_32K,	SKINNY_CODEC_TYPE_AUDIO, 	"g722.1",	"g722.1/32k",	"G722.1 32k (Siren14)", 		"G7221", 	32000,	4,	115},
	{SKINNY_CODEC_G723_1,		SKINNY_CODEC_TYPE_AUDIO, 	"g723",		"g723",		"G.723.1", 				NULL,		8000,	1,	4},
	{SKINNY_CODEC_G726_16K,		SKINNY_CODEC_TYPE_AUDIO, 	"g726",		"g726/16k",	"G.726 16K", 			 	NULL,		8000,	1,	2},
	{SKINNY_CODEC_G726_24K,		SKINNY_CODEC_TYPE_AUDIO,	"g726",		"g726/24k",	"G.726 24K", 			 	NULL,		8000,	1,	2},
	{SKINNY_CODEC_G726_32K,		SKINNY_CODEC_TYPE_AUDIO, 	"g726",		"g726/32k",	"G.726 32K", 			 	NULL,		8000,	1,	112},	//payload could also be 111
	{SKINNY_CODEC_G728,		SKINNY_CODEC_TYPE_AUDIO,	"g728",		"g728",		"G.728", 			 	NULL,		8000,	1,	0},	//payload unknown
	{SKINNY_CODEC_G729,		SKINNY_CODEC_TYPE_AUDIO,	"g729",		"g729",		"G.729", 				NULL,		8000,	1,	18},
	{SKINNY_CODEC_G729_A,		SKINNY_CODEC_TYPE_AUDIO,	"g729",		"g729a",	"G.729 Annex A", 			NULL,		8000,	1,	18},
	{SKINNY_CODEC_G729_B_LOW,	SKINNY_CODEC_TYPE_AUDIO, 	"ilbc",		"ilbc",		"G.729B Low/ILBC",			NULL,		8000,	1,	97},
	{SKINNY_CODEC_G729_B,		SKINNY_CODEC_TYPE_AUDIO,	"g729",		"g729b",	"G.729 Annex B", 			NULL,		8000,	1,	18},
	{SKINNY_CODEC_G729_AB,		SKINNY_CODEC_TYPE_AUDIO,	"g729",		"g729ab",	"G.729 Annex A + B", 			NULL,		8000,	1,	18},
	{SKINNY_CODEC_G729_ANNEX_B,	SKINNY_CODEC_TYPE_AUDIO, 	"g729",		"g729/annex/b",	"G.729 Annex B", 			NULL,		8000,	1,	18},
	{SKINNY_CODEC_ISAC,		SKINNY_CODEC_TYPE_AUDIO,	"isac",		"isac",		"iSAC", 				NULL,		8000,	1,	0},	//payload unknown
	{SKINNY_CODEC_OPUS,		SKINNY_CODEC_TYPE_AUDIO,	"opus",		"opus",		"Opus", 				NULL,		16000,	1,	0},	//new opus payload support
	{SKINNY_CODEC_H224,		SKINNY_CODEC_TYPE_AUDIO,	"h224",		"h224",		"H.224", 				NULL,		8000,	1,	31},
	{SKINNY_CODEC_AAC,		SKINNY_CODEC_TYPE_AUDIO, 	"aac",		"aac",		"AAC", 					NULL,		8000,	1,	0},
	{SKINNY_CODEC_MP4A_LATM_128,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_128","mp4a_latm_128","mp4a latm 128k",			NULL,		128000,	1,	0},
	{SKINNY_CODEC_MP4A_LATM_64,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_64",	"mp4a_latm_64",	"mp4a latm 64k",			NULL,		64000,	1,	0},
	{SKINNY_CODEC_MP4A_LATM_56,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_56",	"mp4a_latm_56",	"mp4a latm 56k",			NULL,		56000,	1,	0},
	{SKINNY_CODEC_MP4A_LATM_48,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_48",	"mp4a_latm_48",	"mp4a latm 48k",			NULL,		48000,	1,	0},
	{SKINNY_CODEC_MP4A_LATM_32,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_32",	"mp4a_latm_32",	"mp4a latm 32k",			NULL,		32000,	1,	0},
	{SKINNY_CODEC_MP4A_LATM_24,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_24",	"mp4a_latm_24",	"mp4a latm 24k",			NULL,		24000,	1,	0},
	{SKINNY_CODEC_MP4A_LATM_NA,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_na",	"mp4a_latm_na",	"mp4a latm nak",			NULL,		8000,	1,	0},
	{SKINNY_CODEC_AMR,		SKINNY_CODEC_TYPE_AUDIO,	"amr",		"amr",		"amr",					NULL,		8000,	1,	0},
	{SKINNY_CODEC_AMR_WB,		SKINNY_CODEC_TYPE_AUDIO,	"amr_wb",	"amr_wb",	"amr_wb",				NULL,		8000,	1,	0},
	{SKINNY_CODEC_H261,		SKINNY_CODEC_TYPE_VIDEO,	"h261",		"h261",		"H.261", 				NULL,		90000,	1,	34},
	{SKINNY_CODEC_H263,		SKINNY_CODEC_TYPE_VIDEO,	"h263",		"h263",		"H.263", 				"H263",		90000,	1,	34},
	{SKINNY_CODEC_H263P,		SKINNY_CODEC_TYPE_VIDEO,	"h263",		"h263p",	"Vieo H.263+", 				NULL,		90000,	1,	98},
	{SKINNY_CODEC_H264,		SKINNY_CODEC_TYPE_VIDEO,	"h264",		"h264",		"H.264", 				"H264",		90000,	1,	99},
	{SKINNY_CODEC_H264_SVC,		SKINNY_CODEC_TYPE_VIDEO,	"h264_svc",	"h264_svc",	"h264_svc",				NULL,		90000,	1,	0},
	{SKINNY_CODEC_H265,		SKINNY_CODEC_TYPE_VIDEO,	"h265",		"h265",		"h265",					NULL,		90000,	1,	0},
	{SKINNY_CODEC_H264_FEC,		SKINNY_CODEC_TYPE_VIDEO,	"h264f",	"h264f",	"h264_fec", 				NULL,		90000,	1,	0},	//unknown codec type
	{SKINNY_CODEC_H264_UC,		SKINNY_CODEC_TYPE_VIDEO,	"h264uc",	"h264uc",	"h264_uc",				NULL,		90000,	1,	0},
	{SKINNY_CODEC_T120,		SKINNY_CODEC_TYPE_TEXT, 	"t120",		"t120",		"T.140", 				NULL,		1000,	1,	0},	//payload unknown
	{SKINNY_CODEC_DATA_64K,		SKINNY_CODEC_TYPE_DATA, 	"data",		"data/64k",	"Data 64k", 				NULL,		1000,	1,	0},	//payload unknown
	{SKINNY_CODEC_DATA_56K,		SKINNY_CODEC_TYPE_DATA, 	"data",		"data/56k",	"Data 56k", 				NULL,		1000,	1,	0},	//payload unknown
	{SKINNY_CODEC_H224,		SKINNY_CODEC_TYPE_DATA,		"h224",		"h224",		"h224, far-end camera control",		NULL,		1000,	1,	0},
	{SKINNY_CODEC_T38FAX,		SKINNY_CODEC_TYPE_DATA,		"t38fax",	"t38fax",	"T38 fax",				NULL,		1000,	1,	0},
	{SKINNY_CODEC_TOTE,		SKINNY_CODEC_TYPE_DATA,		"tote",		"tote",		"tote",					NULL,		1000,	1,	0},	// unknown codec type
	{SKINNY_CODEC_XV150_MR_711U,	SKINNY_CODEC_TYPE_MIXED,	"xv711u",	"xv711u",	"xv150_mr_711u", 			NULL,		0,	1,	0},	//used for modem traffic over vg224
	{SKINNY_CODEC_NSE_VBD_711U,	SKINNY_CODEC_TYPE_MIXED,	"v711u",	"v711u",	"nse vbd 711u", 			NULL,		0,	1,	0},	//unknown codec type
	{SKINNY_CODEC_XV150_MR_729A,	SKINNY_CODEC_TYPE_MIXED,	"xv729a",	"xv729a",	"xv150_mr_711u", 			NULL,		0,	1,	0},	//used for modem traffic over vg224
	{SKINNY_CODEC_NSE_VBD_729A,	SKINNY_CODEC_TYPE_MIXED,	"v729a",	"v729a",	"nse_vbd_729a", 			NULL,		0,	1,	0},	//unknown codec type
	{SKINNY_CODEC_CLEAR_CHAN,	SKINNY_CODEC_TYPE_MIXED,	"clear_chan",	"clear_chan",	"clear_chan",				NULL,		0,	1,	0},
	{SKINNY_CODEC_UNIVERSAL_XCODER,	SKINNY_CODEC_TYPE_MIXED,	"univ_xcoder",	"univ_xcoder", 	"univ_xcoder",				NULL,		0,	1,	0},
	{SKINNY_CODEC_DTMF_OOB_RFC2833,	SKINNY_CODEC_TYPE_MIXED,	"rfc2833",	"rfc2833",	"DTMF RFC 2833 Dyn Pay Load",		NULL,		0,	1,	0},
	{SKINNY_CODEC_DTMF_PASSTHROUGH,	SKINNY_CODEC_TYPE_MIXED,	"passthrough",	"passthrough",	"DTMF Passthrough",			NULL,		0,	1,	0},
	{SKINNY_CODEC_DTMF_DYNAMIC,	SKINNY_CODEC_TYPE_MIXED,	"dynamic",	"dynamic",	"DTMF Dynamic",				NULL,		0,	1,	0},
	{SKINNY_CODEC_DTMF_OOB,		SKINNY_CODEC_TYPE_MIXED,	"oob",		"oob",		"DTMF Out of Band",			NULL,		0,	1,	0},
	{SKINNY_CODEC_DTMF_IB_RFC2833,	SKINNY_CODEC_TYPE_MIXED,	"rfc2833(ib)",	"rfc2833(ib)",	"DTMF RFC2833 In band",			NULL,		0,	1,	0},
	{SKINNY_CODEC_CFB_TONES,	SKINNY_CODEC_TYPE_MIXED,	"cfb",		"cfb",		"CFB Tones",				NULL,		0,	1,	0},
	{SKINNY_CODEC_DTMF_NOAUDIO,	SKINNY_CODEC_TYPE_MIXED,	"noaudio",	"noaudio",	"DTMF NoAudio",				NULL,		0,	1,	0},
	{SKINNY_CODEC_V150_LC_MODEM_RELAY,SKINNY_CODEC_TYPE_MIXED,	"v150_modem",	"v150_modem", 	"v150_lc_modem_relay",			NULL,		0,	1,	0},
	{SKINNY_CODEC_V150_LC_SPRT,	SKINNY_CODEC_TYPE_MIXED,	"v150_sprt",	"v150_sprt",	"v150_lc_sprt",				NULL,		0,	1,	0},
	{SKINNY_CODEC_V150_LC_SSE,	SKINNY_CODEC_TYPE_MIXED,	"v150_sse",	"v150_sse",	"v150_lc_sse",				NULL,		0,	1,	0},
	/* *INDENT-ON* */
};

uint8_t sccp_codec_getArrayLen()
{
	return ARRAY_LEN(skinny_codecs);
}
#define _ARR2STR(arrayname, lookup_var, lookup_val, return_var) \
        ({ \
        uint32_t i; \
        for (i = 0; i < ARRAY_LEN(arrayname); i++) { \
                if (arrayname[i].lookup_var == lookup_val) { \
                        return arrayname[i].return_var; \
                } \
        } \
        pbx_log(LOG_ERROR, "_ARR2STR Lookup Failed for " #arrayname "." #lookup_var "=%i\n", lookup_val); \
        return ""; \
        })

gcc_inline const char *codec2str(skinny_codec_t value)
{
	_ARR2STR(skinny_codecs, codec, value, text);
}

gcc_inline const char *codec2name(skinny_codec_t value)
{
	_ARR2STR(skinny_codecs, codec, value, name);
}

gcc_inline const skinny_payload_type_t codec2type(skinny_codec_t value)
{
        uint32_t i;
        for (i = 0; i < ARRAY_LEN(skinny_codecs); i++) {
                if (skinny_codecs[i].codec == value) {
                        return skinny_codecs[i].codec_type;
                } \
        } \
        pbx_log(LOG_ERROR, "codec2type lookup failed for skinny_codecs[%i]\n", value);
        return SKINNY_CODEC_TYPE_UNKNOWN;
}

/*!
 * \brief Retrieve the string of format numbers and names from an array of formats
 * Buffer needs to be declared and freed afterwards
 
 * \param buf   Buffer as char array
 * \param size  Size of Buffer
 * \param codecs Array of Skinny Codecs
 * \param clength Max Length
 */
char *sccp_codec_multiple2str(char *buf, size_t size, const skinny_codec_t * codecs, const int clength)
{
	if (!buf || size <= 2) {
		return buf;
	}
	memset(buf, 0, size);
	char *endptr = buf;
	int x, comma = 0;

	snprintf(endptr++, size, "[");
	endptr += strlen(endptr);
	for (x = 0; x < clength; x++) {
		if (codecs[x] == SKINNY_CODEC_NONE || codecs[x] == SKINNY_CODEC_NONSTANDARD) {
			break;
		}
		//snprintf(endptr, size, "%s%s (%d)", comma++ ? ", " : "",codec2name(codecs[x]), codecs[x]);
		snprintf(endptr, size, "%s%s", comma++ ? ", " : "",codec2name(codecs[x]));
		endptr += strlen(endptr);
	}
	if (buf == endptr) {
		snprintf(endptr, size, "nothing)");
	}
	snprintf(endptr++, size, "]");
	return buf;
}

static void codec_pref_remove(skinny_codec_t * skinny_codec_prefs, skinny_codec_t skinny_codec)
{
	int x = 0;
	boolean_t found = FALSE;

	for (x = 0; x < SKINNY_MAX_CAPABILITIES && skinny_codec_prefs[x] != SKINNY_CODEC_NONE; x++) {
		if (!found && skinny_codec_prefs[x] == skinny_codec) {
			found = TRUE;
		}
		if (found) {
			memmove(skinny_codec_prefs + x, skinny_codec_prefs + (x + 1), (SKINNY_MAX_CAPABILITIES - (x + 1)) * sizeof(skinny_codec_t));  // move left
		}
	}
}

/*!
 * \brief Append Codec to Skinny Codec Preferences
 */
static int codec_pref_append(skinny_codec_t * skinny_codec_pref, skinny_codec_t skinny_codec)
{
	int x = 0;

	// append behaviour: remove old entry, move all other entries left, append 
	codec_pref_remove(skinny_codec_pref, skinny_codec);

	for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
		if (SKINNY_CODEC_NONE == skinny_codec_pref[x]) {
			//sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "inserting codec '%s (%d)' at pos %d\n", codec2name(skinny_codec), skinny_codec, x);
			skinny_codec_pref[x] = skinny_codec;
			return x;
		}
	}
	return -1;
}

/*!
 * \brief Parse Skinny Codec Allow / Disallow Config Lines
 */
int sccp_codec_parseAllowDisallow(skinny_codec_t * skinny_codec_prefs, const char *list, int allowing)
{
	int errors = 0;

	if (!skinny_codec_prefs) {
		return -1;
	}
	unsigned int x;
	boolean_t all = FALSE;
	boolean_t found = FALSE;
	char *parse = NULL, *token = NULL;
	skinny_codec_t codec;

	parse = pbx_strdupa(list);
	while ((token = strsep(&parse, ","))) {
		if (!sccp_strlen_zero(token)) {
			all = sccp_strcaseequals(token, "all") ? TRUE : FALSE;
			if (all && !allowing) {									// disallowing all
				memset(skinny_codec_prefs, 0, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
				//sccp_log((DEBUGCAT_CODEC)) ("SCCP: disallow=all => reset codecs\n");
				break;
			}
			for (x = 0; x < sccp_codec_getArrayLen(); x++) {
				if (all || sccp_strcaseequals(skinny_codecs[x].key, token)) {
					codec = skinny_codecs[x].codec;
					found = TRUE;
					if (allowing) {
						//sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "appending codec '%s'\n", codec2name(codec));
						codec_pref_append(skinny_codec_prefs, codec);
					} else {
						//sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "removing codec '%s'\n", codec2name(codec));
						codec_pref_remove(skinny_codec_prefs, codec);
					}
				}
			}
			if (!found) {
				pbx_log(LOG_WARNING, "Cannot %s unknown codec '%s'\n", allowing ? "allow" : "disallow", token);
				errors++;
				continue;
			}
		}
	}
	return errors;
}

/*!
 * \brief Check if Skinny Codec is compatible with Skinny Capabilities Array
 */
boolean_t sccp_codec_isCompatible(skinny_codec_t codec, const skinny_codec_t capabilities[], uint8_t length)
{
	uint8_t i;

	for (i = 0; i < length; i++) {
		if (capabilities[i] == codec) {
			return TRUE;
		}
	}
	return FALSE;
}

/*!
 * \brief get smallest common denominator codecset
 * intersection of two sets
 */
void sccp_codec_reduceSet(skinny_codec_t base[SKINNY_MAX_CAPABILITIES], const skinny_codec_t reduceByCodecs[SKINNY_MAX_CAPABILITIES])
{
	skinny_codec_t temp[SKINNY_MAX_CAPABILITIES] = {0};
	uint8_t x = 0, y = 0, z = 0;
	for (x = 0; x < SKINNY_MAX_CAPABILITIES && (z+1) < SKINNY_MAX_CAPABILITIES && base[x] != SKINNY_CODEC_NONE; x++) {
		for (y = 0; y < SKINNY_MAX_CAPABILITIES && (z+1) < SKINNY_MAX_CAPABILITIES && reduceByCodecs[y] != SKINNY_CODEC_NONE; y++) {
			if (base[x] == reduceByCodecs[y]) {
				temp[z++] = base[x];
				break;
			}
		}
	}
	memcpy(base, temp, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
}

/*!
 * \brief combine two codecs sets skipping duplicates
 * union of two sets
 */
void sccp_codec_combineSets(skinny_codec_t base[SKINNY_MAX_CAPABILITIES], const skinny_codec_t addCodecs[SKINNY_MAX_CAPABILITIES])
{
	uint8_t x = 0, y = 0, z = 0, demarquation = SKINNY_MAX_CAPABILITIES;
	for (y = 0; y < SKINNY_MAX_CAPABILITIES && addCodecs[y] != SKINNY_CODEC_NONE; y++) {
		boolean_t found = FALSE;
		for (x = 0; x < demarquation && base[x] != SKINNY_CODEC_NONE; x++) {
			if (base[x] == addCodecs[y]) {
				found = TRUE;
				break;
			}
		}
		while (!found && z < SKINNY_MAX_CAPABILITIES) {
			if (base[z] == SKINNY_CODEC_NONE) {
				if (demarquation) {		// don't check against newly added codecs
					demarquation = z;
				}
				base[z] = addCodecs[y];
				break;
			}
			z++;
		}
	}
}

/*!
 * \brief Find the best codec match Between Preferences, Capabilities and RemotePeerCapabilities
 * 
 * Returns:
 *  - Best Match If Found
 *  - If not it returns the first jointCapability
 *  - Else SKINNY_CODEC_NONE
 */
skinny_codec_t sccp_codec_findBestJoint(const skinny_codec_t ourPreferences[], int pLength, const skinny_codec_t ourCapabilities[], int cLength, const skinny_codec_t remotePeerCapabilities[], int rLength)
{
	uint8_t r, c, p;
	skinny_codec_t firstJointCapability = SKINNY_CODEC_NONE;						/*!< used to get a default value */

	sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "pLength %d, cLength: %d, rLength: %d\n", pLength, cLength, rLength);

	//char pref_buf[256];
	//sccp_codec_multiple2str(pref_buf, sizeof(pref_buf) - 1, ourPreferences, (int)pLength);
	//char cap_buf[256];
	//sccp_codec_multiple2str(cap_buf, sizeof(pref_buf) - 1, ourCapabilities, cLength);
	//char remote_cap_buf[256];
	//sccp_codec_multiple2str(remote_cap_buf, sizeof(remote_cap_buf) - 1, remotePeerCapabilities, rLength);
	//sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ourPref %s\nourCap: %s\nremoteCap: %s\n", pref_buf, cap_buf, remote_cap_buf);

	/** check if we have a preference codec list */
	if (pLength == 0 || ourPreferences[0] == SKINNY_CODEC_NONE) {
		/* using remote capabilities to */
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "We got an empty preference codec list (exiting)\n");
		return SKINNY_CODEC_NONE;

	}

	/* iterate over our codec preferences */
	for (p = 0; p < pLength; p++) {
		if (ourPreferences[p] == SKINNY_CODEC_NONE) {
			sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "no more preferences at position %d\n", p);
			break;
		}
		/* no more preferences */
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "preference: %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]));

		/* check if we are capable to handle this codec */
		for (c = 0; c < cLength; c++) {
			if (ourCapabilities[c] == SKINNY_CODEC_NONE) {
				/* we reached the end of valide codecs, because we found the first NONE codec */
				sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) ("Breaking at capability: %d\n", c);
				break;
			}

			sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "preference: %d(%s), capability: %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]), ourCapabilities[c], codec2name(ourCapabilities[c]));

			/* we have no capabilities from the remote party, use the best codec from ourPreferences */
			if (ourPreferences[p] == ourCapabilities[c]) {
				if (firstJointCapability == SKINNY_CODEC_NONE) {
					firstJointCapability = ourPreferences[p];
					sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "found first firstJointCapability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
				}

				if (rLength == 0 || remotePeerCapabilities[0] == SKINNY_CODEC_NONE) {
					sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "Empty remote Capabilities, using bestCodec from firstJointCapability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
					return firstJointCapability;
				} 
					/* using capabilities from remote party, that matches our preferences & capabilities */
					for (r = 0; r < rLength; r++) {
						if (remotePeerCapabilities[r] == SKINNY_CODEC_NONE) {
							sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) ("Breaking at remotePeerCapability: %d\n", c);
							break;
						}
						sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "preference: %d(%s), capability: %d(%s), remoteCapability: " UI64FMT "(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]), ourCapabilities[c], codec2name(ourCapabilities[c]), (ULONG) remotePeerCapabilities[r], codec2name(remotePeerCapabilities[r]));
						if (ourPreferences[p] == remotePeerCapabilities[r]) {
							sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "found bestCodec as joint capability with remote peer %d(%s)\n", ourPreferences[p], codec2name(ourPreferences[p]));
							return ourPreferences[p];
						}
					}
				
			}
		}
	}

	if (firstJointCapability != SKINNY_CODEC_NONE) {
		sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "did not find joint capability with remote device, using first joint capability %d(%s)\n", firstJointCapability, codec2name(firstJointCapability));
		return firstJointCapability;
	}

	sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_3 "no joint capability with preference codec list\n");
	return 0;
}
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
