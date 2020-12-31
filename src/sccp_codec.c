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
#include "sccp_channel.h"

/*
 * references: https://en.wikipedia.org/wiki/RTP_audio_video_profile
 * --DD-- We need an extra column, specifying the RFC number, needed for MultiMediaReceive/MultiMediaTransmission->lel_payload_rfc_number;
 */
const struct skinny_codec skinny_codecs[] = {
	/* clang-format off */
	{SKINNY_CODEC_NONE,		SKINNY_CODEC_TYPE_UNKNOWN, 	"",		"",		"No codec", 				NULL,		0,	0,	-1},	//payload unknown
	{SKINNY_CODEC_NONSTANDARD,	SKINNY_CODEC_TYPE_UNKNOWN,	"",		"",		"Non-standard codec", 			NULL,		0,	0,	-1},	//payload unknown
	{SKINNY_CODEC_IS11172,		SKINNY_CODEC_TYPE_AUDIO,	"is11172",	"is11172",	"IS11172 AudioCap", 			NULL,		8000,	1,	0},	//payload unknown
	{SKINNY_CODEC_IS13818,		SKINNY_CODEC_TYPE_AUDIO,	"is13872",      "is13872",	"IS13818 AudioCap", 			NULL,		8000,	1,	0},	//payload unknown
	{SKINNY_CODEC_GSM_FULLRATE,	SKINNY_CODEC_TYPE_AUDIO, 	"gsm",		"gsm/full",	"GSM Full Rate", 			"GSM",		8000,	2,	3},
	{SKINNY_CODEC_GSM_HALFRATE,	SKINNY_CODEC_TYPE_AUDIO, 	"gsm",		"gsm/half",	"GSM Half Rate", 			"GSM-HR-08",	8000,	1,	3},
	{SKINNY_CODEC_GSM_ENH_FULLRATE,	SKINNY_CODEC_TYPE_AUDIO,	"gsm",		"gsm/enh",	"GSM Enhanced Full Rate",		"GSM-EFR",	8000,	2,	3},
	//{SKINNY_CODEC_WIDEBAND_256K,	SKINNY_CODEC_TYPE_AUDIO, 	"slin16",	"slin16",	"Wideband 1411.2 kbit/s",		"L16",		44100,	3,	10},
	//{SKINNY_CODEC_WIDEBAND_256K,	SKINNY_CODEC_TYPE_AUDIO, 	"slin16",	"slin16",	"Wideband 705.6 kbit/s", 		"L16",		44100,	3,	11},
	{SKINNY_CODEC_WIDEBAND_256K,	SKINNY_CODEC_TYPE_AUDIO, 	"slin16",	"slin16",	"Wideband 256 kbit/k",	 		"L16",		44100,	3,	25},
	{SKINNY_CODEC_GSM,		SKINNY_CODEC_TYPE_AUDIO, 	"gsm",		"gsm",		"GSM", 					"GSM",		8000,	1,	3},
	{SKINNY_CODEC_ACTIVEVOICE,	SKINNY_CODEC_TYPE_AUDIO, 	"activevoice",	"activevoice",	"ActiveVoice", 				NULL,		8000,	1,	-1},	//payload unknown
	{SKINNY_CODEC_G711_ALAW_64K,	SKINNY_CODEC_TYPE_AUDIO, 	"alaw",		"alaw/64k",	"G.711 A-law 64k", 			"PCMA",		8000,	2,	8},
	{SKINNY_CODEC_G711_ALAW_56K,	SKINNY_CODEC_TYPE_AUDIO, 	"alaw",		"alaw/56k",	"G.711 A-law 56k", 			"PCMA",		8000,	1,	8},	// --DD-- which one ?
	{SKINNY_CODEC_G711_ULAW_64K,	SKINNY_CODEC_TYPE_AUDIO, 	"ulaw",		"ulaw/64k",	"G.711 u-law 64k", 			"PCMU",		8000,	2,	0},
	{SKINNY_CODEC_G711_ULAW_56K,	SKINNY_CODEC_TYPE_AUDIO, 	"ulaw",		"ulaw/56k",	"G.711 u-law 56k", 			"PCMU",		8000,	1,	0},	// --DD-- which one ?
	{SKINNY_CODEC_G722_64K,		SKINNY_CODEC_TYPE_AUDIO, 	"g722",		"g722/64k",	"G.722 64k", 				"G722",		8000,	3,	9},
	{SKINNY_CODEC_G722_56K,		SKINNY_CODEC_TYPE_AUDIO, 	"g722",		"g722/56k",	"G.722 56k", 				"G722",		8000,	3,	9},
	{SKINNY_CODEC_G722_48K,		SKINNY_CODEC_TYPE_AUDIO, 	"g722",		"g722/48k",	"G.722 48k", 				"G722",		8000,	2,	9},	// --DD-- which one ?
	{SKINNY_CODEC_G722_1_24K,	SKINNY_CODEC_TYPE_AUDIO, 	"g722.1",	"g722.1/24k",	"G722.1 24k (Siren7)", 			"G7221",	16000,	3,	102},	// --DD-- please check
	{SKINNY_CODEC_G722_1_32K,	SKINNY_CODEC_TYPE_AUDIO, 	"g722.1",	"g722.1/32k",	"G722.1 32k (Siren14)", 		"G7221", 	32000,	4,	115},	// --DD-- please check
	{SKINNY_CODEC_G723_1,		SKINNY_CODEC_TYPE_AUDIO, 	"g723",		"g723",		"G.723.1", 				"G723",		8000,	1,	4},
	{SKINNY_CODEC_G726_16K,		SKINNY_CODEC_TYPE_AUDIO, 	"g726",		"g726/16k",	"G.726 16K", 			 	"G726-16",	8000,	1,	2},
	{SKINNY_CODEC_G726_24K,		SKINNY_CODEC_TYPE_AUDIO,	"g726",		"g726/24k",	"G.726 24K", 			 	"G726-24",	8000,	1,	2},
	{SKINNY_CODEC_G726_32K,		SKINNY_CODEC_TYPE_AUDIO, 	"g726",		"g726/32k",	"G.726 32K", 			 	"G726-32",	8000,	1,	112},	//payload could also be 111
	{SKINNY_CODEC_G728,		SKINNY_CODEC_TYPE_AUDIO,	"g728",		"g728",		"G.728", 			 	"G728",		8000,	1,	-1},	//payload unknown
	{SKINNY_CODEC_G729,		SKINNY_CODEC_TYPE_AUDIO,	"g729",		"g729",		"G.729", 				"G729",		8000,	1,	18},
	{SKINNY_CODEC_G729_A,		SKINNY_CODEC_TYPE_AUDIO,	"g729",		"g729a",	"G.729 Annex A", 			"G729a",	8000,	1,	18},
	{SKINNY_CODEC_G729_B_LOW,	SKINNY_CODEC_TYPE_AUDIO, 	"ilbc",		"ilbc",		"G.729B Low/ILBC",			"iLBC",		8000,	1,	97},
	{SKINNY_CODEC_G729_B,		SKINNY_CODEC_TYPE_AUDIO,	"g729",		"g729b",	"G.729 Annex B", 			"G729",		8000,	1,	18},
	{SKINNY_CODEC_G729_AB,		SKINNY_CODEC_TYPE_AUDIO,	"g729",		"g729ab",	"G.729 Annex A + B", 			"G729",		8000,	1,	18},
	{SKINNY_CODEC_G729_ANNEX_B,	SKINNY_CODEC_TYPE_AUDIO, 	"g729",		"g729/annex/b",	"G.729 Annex B", 			"G729",		8000,	1,	18},
	{SKINNY_CODEC_ISAC,		SKINNY_CODEC_TYPE_AUDIO,	"isac",		"isac",		"iSAC", 				NULL,		8000,	1,	-1},	//payload unknown
	{SKINNY_CODEC_OPUS,		SKINNY_CODEC_TYPE_AUDIO,	"opus",		"opus",		"Opus", 				"opus",		16000,	1,	-2},	//new opus payload // --DD-- please check
	{SKINNY_CODEC_H224,		SKINNY_CODEC_TYPE_AUDIO,	"h224",		"h224",		"H.224", 				NULL,		8000,	1,	31},
	{SKINNY_CODEC_AAC,		SKINNY_CODEC_TYPE_AUDIO, 	"aac",		"aac",		"AAC", 					NULL,		8000,	1,	-1},
	{SKINNY_CODEC_MP4A_LATM_128,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_128","mp4a_latm_128","mp4a latm 128k",			"MP4A-LATM",	128000,	1,	-1},
	{SKINNY_CODEC_MP4A_LATM_64,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_64",	"mp4a_latm_64",	"mp4a latm 64k",			"MP4A-LATM",	64000,	1,	-1},
	{SKINNY_CODEC_MP4A_LATM_56,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_56",	"mp4a_latm_56",	"mp4a latm 56k",			"MP4A-LATM",	56000,	1,	-1},
	{SKINNY_CODEC_MP4A_LATM_48,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_48",	"mp4a_latm_48",	"mp4a latm 48k",			"MP4A-LATM",	48000,	1,	-1},
	{SKINNY_CODEC_MP4A_LATM_32,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_32",	"mp4a_latm_32",	"mp4a latm 32k",			"MP4A-LATM",	32000,	1,	-1},
	{SKINNY_CODEC_MP4A_LATM_24,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_24",	"mp4a_latm_24",	"mp4a latm 24k",			"MP4A-LATM",	24000,	1,	-1},
	{SKINNY_CODEC_MP4A_LATM_NA,	SKINNY_CODEC_TYPE_AUDIO,	"mp4a_latm_na",	"mp4a_latm_na",	"mp4a latm nak",			"MP4A-LATM",	8000,	1,	-1},
	{SKINNY_CODEC_AMR,		SKINNY_CODEC_TYPE_AUDIO,	"amr",		"amr",		"amr",					"AMR",		8000,	1,	-2},	// --DD-- please check
	{SKINNY_CODEC_AMR_WB,		SKINNY_CODEC_TYPE_AUDIO,	"amr_wb",	"amr_wb",	"amr_wb",				"AMR-WB",	16000,	1,	-2},	// --DD-- please check
	{SKINNY_CODEC_H261,		SKINNY_CODEC_TYPE_VIDEO,	"h261",		"h261",		"H.261", 				"H261",		90000,	1,	34},
	{SKINNY_CODEC_H263,		SKINNY_CODEC_TYPE_VIDEO,	"h263",		"h263",		"H.263", 				"H263",		90000,	1,	34},
	{SKINNY_CODEC_H263P,		SKINNY_CODEC_TYPE_VIDEO,	"h263",		"h263p",	"Vieo H.263+", 				"H263p",	90000,	1,	98},
	{SKINNY_CODEC_H264,		SKINNY_CODEC_TYPE_VIDEO,	"h264",		"h264",		"H.264", 				"H264",		90000,	1,	103},
	{SKINNY_CODEC_H264_SVC,		SKINNY_CODEC_TYPE_VIDEO,	"h264",		"h264_svc",	"h264_svc",				"H264 SVC",	90000,	1,	-2},	// --DD-- please check
	{SKINNY_CODEC_H265,		SKINNY_CODEC_TYPE_VIDEO,	"h265",		"h265",		"h265",					"H265",		90000,	1,	-1},
	{SKINNY_CODEC_H264_FEC,		SKINNY_CODEC_TYPE_VIDEO,	"h264",		"h264f",	"h264_fec", 				NULL,		90000,	1,	-2},	//unknown codec type --DD-- please check
	{SKINNY_CODEC_H264_UC,		SKINNY_CODEC_TYPE_VIDEO,	"h264",		"h264uc",	"h264_uc",				NULL,		90000,	1,	-2},	// --DD-- please check
	{SKINNY_CODEC_T120,		SKINNY_CODEC_TYPE_TEXT, 	"t120",		"t120",		"T.140", 				NULL,		1000,	1,	-2},	//payload unknown --DD--
	{SKINNY_CODEC_DATA_64K,		SKINNY_CODEC_TYPE_DATA, 	"data",		"data/64k",	"Data 64k", 				NULL,		1000,	1,	-2},	//payload unknown
	{SKINNY_CODEC_DATA_56K,		SKINNY_CODEC_TYPE_DATA, 	"data",		"data/56k",	"Data 56k", 				NULL,		1000,	1,	-2},	//payload unknown
	{SKINNY_CODEC_H224,		SKINNY_CODEC_TYPE_DATA,		"h224",		"h224",		"h224, far-end camera control",		NULL,		1000,	1,	-2},	// --DD-- please check
	{SKINNY_CODEC_T38FAX,		SKINNY_CODEC_TYPE_DATA,		"t38fax",	"t38fax",	"T38 fax",				NULL,		1000,	1,	-2},	// --DD-- please check
	{SKINNY_CODEC_TOTE,		SKINNY_CODEC_TYPE_DATA,		"tote",		"tote",		"tote",					NULL,		1000,	1,	-1},	// unknown codec type
	{SKINNY_CODEC_XV150_MR_711U,	SKINNY_CODEC_TYPE_MIXED,	"xv711u",	"xv711u",	"xv150_mr_711u", 			NULL,		0,	1,	-1},	//used for modem traffic over vg224
	{SKINNY_CODEC_NSE_VBD_711U,	SKINNY_CODEC_TYPE_MIXED,	"v711u",	"v711u",	"nse vbd 711u", 			NULL,		0,	1,	-1},	//unknown codec type
	{SKINNY_CODEC_XV150_MR_729A,	SKINNY_CODEC_TYPE_MIXED,	"xv729a",	"xv729a",	"xv150_mr_711u", 			NULL,		0,	1,	-1},	//used for modem traffic over vg224
	{SKINNY_CODEC_NSE_VBD_729A,	SKINNY_CODEC_TYPE_MIXED,	"v729a",	"v729a",	"nse_vbd_729a", 			NULL,		0,	1,	-1},	//unknown codec type
	{SKINNY_CODEC_CLEAR_CHAN,	SKINNY_CODEC_TYPE_MIXED,	"clear_chan",	"clear_chan",	"clear_chan",				NULL,		0,	1,	-1},
	{SKINNY_CODEC_UNIVERSAL_XCODER,	SKINNY_CODEC_TYPE_MIXED,	"univ_xcoder",	"univ_xcoder", 	"univ_xcoder",				NULL,		0,	1,	-1},
	{SKINNY_CODEC_DTMF_OOB_RFC2833,	SKINNY_CODEC_TYPE_MIXED,	"rfc2833",	"rfc2833",	"DTMF RFC 2833 Dyn Pay Load",		"telephone-event",	0,	1,	101},	// --DD-- please check 105| 101| 96 which one goes where
	{SKINNY_CODEC_DTMF_PASSTHROUGH,	SKINNY_CODEC_TYPE_MIXED,	"passthrough",	"passthrough",	"DTMF Passthrough",			"CISCO_DTMF pt"	,0,	1,	105},	// --DD-- please check 105| 101| 96
	{SKINNY_CODEC_DTMF_DYNAMIC,	SKINNY_CODEC_TYPE_MIXED,	"dynamic",	"dynamic",	"DTMF Dynamic",				NULL,		0,	1,	101},	// --DD-- please check 105| 101| 96 
	{SKINNY_CODEC_DTMF_OOB,		SKINNY_CODEC_TYPE_MIXED,	"oob",		"oob",		"DTMF Out of Band",			NULL,		0,	1,	101},	// --DD-- please check 105| 101| 96
	{SKINNY_CODEC_DTMF_IB_RFC2833,	SKINNY_CODEC_TYPE_MIXED,	"rfc2833(ib)",	"rfc2833(ib)",	"DTMF RFC2833 In band",			"CISCO_DTMF ib",0,	1,	96},	// --DD-- please check 105| 101| 96
	{SKINNY_CODEC_CFB_TONES,	SKINNY_CODEC_TYPE_MIXED,	"cfb",		"cfb",		"CFB Tones",				NULL,		0,	1,	-1},
	{SKINNY_CODEC_DTMF_NOAUDIO,	SKINNY_CODEC_TYPE_MIXED,	"noaudio",	"noaudio",	"DTMF NoAudio",				NULL,		0,	1,	-1},
	{SKINNY_CODEC_V150_LC_MODEM_RELAY,SKINNY_CODEC_TYPE_MIXED,	"v150_modem",	"v150_modem", 	"v150_lc_modem_relay",			NULL,		0,	1,	-1},
	{SKINNY_CODEC_V150_LC_SPRT,	SKINNY_CODEC_TYPE_MIXED,	"v150_sprt",	"v150_sprt",	"v150_lc_sprt",				NULL,		0,	1,	-1},
	{SKINNY_CODEC_V150_LC_SSE,	SKINNY_CODEC_TYPE_MIXED,	"v150_sse",	"v150_sse",	"v150_lc_sse",				NULL,		0,	1,	-1},
	/* clang-format on */
};

uint8_t __CONST__ sccp_codec_getArrayLen()
{
	return ARRAY_LEN(skinny_codecs);
}
gcc_inline const char * codec2str(skinny_codec_t value)
{
	_ARR2STR(skinny_codecs, codec, value, text);
}

gcc_inline const char * codec2name(skinny_codec_t value)
{
	_ARR2STR(skinny_codecs, codec, value, name);
}

/*! \todo should be called skinny_codec_type_t instead of skinny_payload_type_t */
gcc_inline const skinny_payload_type_t codec2type(skinny_codec_t value)
{
	uint32_t i = 0;
	for (i = 0; i < ARRAY_LEN(skinny_codecs); i++) {
		if (skinny_codecs[i].codec == value) {
			return skinny_codecs[i].codec_type;
		}
	}
	pbx_log(LOG_ERROR, "codec2type lookup failed for skinny_codecs[%i]\n", value);
	return SKINNY_CODEC_TYPE_UNKNOWN;
}

gcc_inline const int32_t codec2rtp_payload_type(skinny_codec_t value)
{
	uint32_t i = 0;
	for (i = 0; i < ARRAY_LEN(skinny_codecs); i++) {
		if (skinny_codecs[i].codec == value) {
			return skinny_codecs[i].rtp_payload_type;
		}
	}
	pbx_log(LOG_ERROR, "codec2rtp_payload_type lookup failed for skinny_codecs[%i]\n", value);
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
char * sccp_codec_multiple2str(char * buf, size_t size, const skinny_codec_t * codecs, const uint clength)
{
	if (!buf || size <= 2) {
		return buf;
	}
	memset(buf, 0, size);
	char * endptr = buf;
	int    comma  = 0;

	snprintf(endptr++, size, "[");
	endptr += strlen(endptr);
	for (uint x = 0; x < clength; x++) {
		if (codecs[x] == SKINNY_CODEC_NONE || codecs[x] == SKINNY_CODEC_NONSTANDARD) {
			break;
		}
		// snprintf(endptr, size, "%s%s (%d)", comma++ ? ", " : "",codec2name(codecs[x]), codecs[x]);
		snprintf(endptr, size, "%s%s", comma++ ? ", " : "", codec2name(codecs[x]));
		endptr += strlen(endptr);
	}
	if (buf == endptr) {
		snprintf(endptr++, size, "nothing)");
	}
	snprintf(endptr, size, "]");
	return buf;
}

static void codec_pref_remove(skinny_codec_t * skinny_codec_prefs, skinny_codec_t skinny_codec)
{
	int       x     = 0;
	boolean_t found = FALSE;

	for (x = 0; x < SKINNY_MAX_CAPABILITIES && skinny_codec_prefs[x] != SKINNY_CODEC_NONE; x++) {
		if (!found && skinny_codec_prefs[x] == skinny_codec) {
			found = TRUE;
		}
		if (found) {
			memmove(skinny_codec_prefs + x, skinny_codec_prefs + (x + 1), (SKINNY_MAX_CAPABILITIES - (x + 1)) * sizeof(skinny_codec_t));                                        // move left
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
			// sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "inserting codec '%s (%d)' at pos %d\n", codec2name(skinny_codec), skinny_codec, x);
			skinny_codec_pref[x] = skinny_codec;
			return x;
		}
	}
	return -1;
}

/*!
 * \brief Parse Skinny Codec Allow / Disallow Config Lines
 */
int sccp_codec_parseAllowDisallow(skinny_codec_t * skinny_codec_prefs, const char * list, int allowing)
{
	int errors = 0;

	if (!skinny_codec_prefs) {
		return -1;
	}
	unsigned int x     = 0;
	boolean_t    all   = FALSE;
	boolean_t    found = FALSE;
	boolean_t    allow = allowing;
	char *       parse = NULL;

	char *         token = NULL;
	skinny_codec_t codec = 0;

	parse = pbx_strdupa(list);
	while ((token = strsep(&parse, ","))) {
		if (!sccp_strlen_zero(token)) {
			if (token[0] == '!') {
				// sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "matched !, token=%s\n", token);
				allow = !allowing;
				token++;                                        // consume !
			}
			all = sccp_strcaseequals(token, "all") ? TRUE : FALSE;
			if (all && !allow) {                                        // disallowing all
				memset(skinny_codec_prefs, 0, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
				// sccp_log((DEBUGCAT_CODEC)) ("SCCP: disallow=all => reset codecs\n");
				allow = allowing;
				continue;
			}
			for (x = 0; x < sccp_codec_getArrayLen(); x++) {
				if (all || sccp_strcaseequals(skinny_codecs[x].key, token)) {
					codec = skinny_codecs[x].codec;
					found = TRUE;
					if (allow) {
						// sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "appending codec '%s'\n", codec2name(codec));
						codec_pref_append(skinny_codec_prefs, codec);
					} else {
						// sccp_log((DEBUGCAT_CODEC)) (VERBOSE_PREFIX_1 "removing codec '%s'\n", codec2name(codec));
						codec_pref_remove(skinny_codec_prefs, codec);
					}
				}
			}
			allow = allowing;
			if (!found) {
				// pbx_log(LOG_WARNING, "Cannot %s unknown codec '%s'\n", allow ? "allow" : "disallow", token);
				errors++;
			}
		}
	}
	return errors;
}

int sccp_get_codecs_bytype(const skinny_codec_t * in_codecs, skinny_codec_t * out_codecs, skinny_payload_type_t type)
{
	int x = 0;

	int y = 0;

	int z = 0;
	for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
		if (SKINNY_CODEC_NONE != in_codecs[x]) {
			for (y = 0; y < sccp_codec_getArrayLen(); y++) {
				if (skinny_codecs[y].codec == in_codecs[x] && skinny_codecs[y].codec_type == type) {
					// sccp_log((DEBUGCAT_CODEC)) ("Adding codec:%s\n", skinny_codecs[y].name);
					out_codecs[z++] = skinny_codecs[y].codec;
				}
			}
		}
	}
	return z;
}

/*!
 * \brief Check if Skinny Codec is compatible with Skinny Capabilities Array
 */
boolean_t __PURE__ sccp_codec_isCompatible(skinny_codec_t codec, const skinny_codec_t capabilities[], uint8_t length)
{
	for (uint8_t i = 0; i < length; i++) {
		if (capabilities[i] == SKINNY_CODEC_NONE) {
			break;
		}
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
int sccp_codec_getReducedSet(const skinny_codec_t base[SKINNY_MAX_CAPABILITIES], const skinny_codec_t reduceByCodecs[SKINNY_MAX_CAPABILITIES], skinny_codec_t result[SKINNY_MAX_CAPABILITIES])
{
	uint8_t x = 0;

	uint8_t y = 0;

	uint8_t z = 0;
	for (x = 0; x < SKINNY_MAX_CAPABILITIES && (z + 1) < SKINNY_MAX_CAPABILITIES && base[x] != SKINNY_CODEC_NONE; x++) {
		for (y = 0; y < SKINNY_MAX_CAPABILITIES && (z + 1) < SKINNY_MAX_CAPABILITIES && reduceByCodecs[y] != SKINNY_CODEC_NONE; y++) {
			if (base[x] == reduceByCodecs[y]) {
				result[z++] = base[x];
			}
		}
	}
	return z; /* no matches / overlap */
}
/*!
 * \brief get smallest common denominator codecset
 * intersection of two sets
 */
void sccp_codec_reduceSet(skinny_codec_t base[SKINNY_MAX_CAPABILITIES], const skinny_codec_t reduceByCodecs[SKINNY_MAX_CAPABILITIES])
{
	skinny_codec_t temp[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE };
	sccp_codec_getReducedSet(base, reduceByCodecs, temp);
	memcpy(base, temp, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
}

/*!
 * \brief combine two codecs sets skipping duplicates
 * union of two sets
 */
void sccp_codec_combineSets(skinny_codec_t base[SKINNY_MAX_CAPABILITIES], const skinny_codec_t addCodecs[SKINNY_MAX_CAPABILITIES])
{
	uint8_t x = 0;

	uint8_t y = 0;

	uint8_t z = 0;

	uint8_t demarquation = SKINNY_MAX_CAPABILITIES;
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
				if (demarquation) {                                        // don't check against newly added codecs
					demarquation = z;
				}
				base[z] = addCodecs[y];
				break;
			}
			z++;
		}
	}
}

skinny_codec_t sccp_codec_findBestJoint(constChannelPtr c, const skinny_codec_t ourPreferences[], const skinny_codec_t remotePeerPreferences[], boolean_t fallback)
{
	skinny_codec_t   res                                = SKINNY_CODEC_NONE;
	skinny_codec_t   leadPrefs[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE };
	skinny_codec_t * followPrefs                        = NULL;

	/* debug */
	// char pref_buf[256]; sccp_codec_multiple2str(pref_buf, sizeof(pref_buf) - 1, ourPreferences, SKINNY_MAX_CAPABILITIES);
	// char remote_buf[256]; sccp_codec_multiple2str(remote_buf, sizeof(remote_buf) - 1, remotePeerPreferences, SKINNY_MAX_CAPABILITIES);
	// sccp_log_and((DEBUGCAT_CODEC + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_3 "ourPref:%s, remoteCap::%s, direction:%s\n", pref_buf, remote_buf, skinny_calltype2str(c->calltype));
	/* end debug */

	if (ourPreferences[0] == SKINNY_CODEC_NONE && remotePeerPreferences[0] == SKINNY_CODEC_NONE) {
		sccp_log(DEBUGCAT_CODEC)(VERBOSE_PREFIX_3 "%s: both preference lists are empty\n", c->designator);
		goto EXIT;
	}

	/* direction of the call determines who leads */
	/*if (SKINNY_CALLTYPE_INBOUND == c->calltype){
	        memcpy(leadPrefs, remotePeerPreferences, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
	        followPrefs = (skinny_codec_t *) ourPreferences;
	} else {*/
	memcpy(leadPrefs, ourPreferences, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
	followPrefs = (skinny_codec_t *)remotePeerPreferences;
	//}

	sccp_codec_reduceSet(leadPrefs, followPrefs);
	res = leadPrefs[0];

EXIT:
	if (res == SKINNY_CODEC_NONE && fallback) {
		sccp_log(DEBUGCAT_CODEC)(VERBOSE_PREFIX_3 "%s, Could not find a common prefered codec (yet), using %s (%d)\n", c->designator, codec2name(ourPreferences[0]), ourPreferences[0]);
		res = ourPreferences[0];
	}
	sccp_log(DEBUGCAT_CODEC)(VERBOSE_PREFIX_3 "%s: (findBestJoint) returning prefered codec %s (%d)\n", c->designator, codec2name(res), res);
	return res;
}

#if CS_TEST_FRAMEWORK
#	include <asterisk/test.h>
AST_TEST_DEFINE(chan_sccp_reduce_codec_set)
{
	switch (cmd) {
		case TEST_INIT:
			info->name        = "reduceCodecSet";
			info->category    = "/channels/chan_sccp/codec/";
			info->summary     = "reduceCodecSet unit test";
			info->description = "reduceCodecSet";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}

	const skinny_codec_t empty[SKINNY_MAX_CAPABILITIES]  = { SKINNY_CODEC_NONE };
	const skinny_codec_t short1[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONSTANDARD, SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_NONE };
	const skinny_codec_t short2[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G722_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_G722_56K,
		                                                 SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_G722_48K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_NONE };
	const skinny_codec_t long1[SKINNY_MAX_CAPABILITIES]  = {
                SKINNY_CODEC_G729_A,   SKINNY_CODEC_G729,          SKINNY_CODEC_G728,          SKINNY_CODEC_G723_1,        SKINNY_CODEC_G722_48K,      SKINNY_CODEC_G722_56K,
                SKINNY_CODEC_G722_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_IS11172,
                SKINNY_CODEC_IS13818,  SKINNY_CODEC_G729_B,        SKINNY_CODEC_G729_AB,       SKINNY_CODEC_GSM_FULLRATE,  SKINNY_CODEC_GSM_HALFRATE,  SKINNY_CODEC_WIDEBAND_256K
	};
	pbx_test_status_update(test, "Executing reduceCodecSet on two default codecArrays...\n");
	{
		uint8_t        x                                       = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE };
		sccp_codec_reduceSet(baseCodecArray, empty);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			pbx_test_validate(test, baseCodecArray[x] == SKINNY_CODEC_NONE);
		}
	}
	pbx_test_status_update(test, "Executing reduceCodecSet on one partially filled and one empty codecArray...\n");
	{
		uint8_t        x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		sccp_codec_reduceSet(baseCodecArray, empty);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			pbx_test_validate(test, baseCodecArray[x] == SKINNY_CODEC_NONE);
		}
	}
	pbx_test_status_update(test, "Executing reduceCodecSet on two partially filled codecArrays...\n");
	{
		uint8_t        x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		const skinny_codec_t result[SKINNY_MAX_CAPABILITIES] = {
			SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_NONE,
		};
		sccp_codec_reduceSet(baseCodecArray, short2);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			// pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(result[x]));
			pbx_test_validate(test, baseCodecArray[x] == result[x]);
		}
	}
	pbx_test_status_update(test, "Executing reduceCodecSet one fully filled and one partially filled codecArrays...\n");
	{
		uint8_t        x = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES];
		memcpy(baseCodecArray, long1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		const skinny_codec_t result[SKINNY_MAX_CAPABILITIES] = {
			SKINNY_CODEC_G722_48K, SKINNY_CODEC_G722_56K, SKINNY_CODEC_G722_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_NONE,
		};
		sccp_codec_reduceSet(baseCodecArray, short2);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(result[x]));
		}
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			pbx_test_validate(test, baseCodecArray[x] == result[x]);
		}
	}
	return AST_TEST_PASS;
}

AST_TEST_DEFINE(chan_sccp_combine_codec_sets)
{
	switch (cmd) {
		case TEST_INIT:
			info->name        = "combineCodecSets";
			info->category    = "/channels/chan_sccp/codec/";
			info->summary     = "combineCodecSets unit test";
			info->description = "combineCodecSets";
			return AST_TEST_NOT_RUN;
		case TEST_EXECUTE:
			break;
	}

	const skinny_codec_t empty[SKINNY_MAX_CAPABILITIES]  = { SKINNY_CODEC_NONE };
	const skinny_codec_t short1[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONSTANDARD, SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_NONE };
	const skinny_codec_t short2[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G722_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_G722_56K,
		                                                 SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_G722_48K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_NONE };
	const skinny_codec_t long1[SKINNY_MAX_CAPABILITIES]  = {
                SKINNY_CODEC_G729_A,   SKINNY_CODEC_G729,          SKINNY_CODEC_G728,          SKINNY_CODEC_G723_1,        SKINNY_CODEC_G722_48K,      SKINNY_CODEC_G722_56K,
                SKINNY_CODEC_G722_64K, SKINNY_CODEC_G711_ULAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_IS11172,
                SKINNY_CODEC_IS13818,  SKINNY_CODEC_G729_B,        SKINNY_CODEC_G729_AB,       SKINNY_CODEC_GSM_FULLRATE,  SKINNY_CODEC_GSM_HALFRATE,  SKINNY_CODEC_WIDEBAND_256K
	};
	pbx_test_status_update(test, "Executing combineCodecSet on two empty codecArrays...\n");
	{
		uint8_t        x                                       = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE };
		sccp_codec_combineSets(baseCodecArray, empty);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			pbx_test_validate(test, baseCodecArray[x] == SKINNY_CODEC_NONE);
		}
	}
	pbx_test_status_update(test, "Executing combineCodecSet on one partially filled and one empty codecArray...\n");
	{
		uint8_t        x                                       = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE };
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		sccp_codec_combineSets(baseCodecArray, empty);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			// pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(short1[x]));
			pbx_test_validate(test, baseCodecArray[x] == short1[x]);
		}
	}
	pbx_test_status_update(test, "Executing combineCodecSet on two partially filled codecArrays...\n");
	{
		uint8_t        x                                       = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE };
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		const skinny_codec_t result[SKINNY_MAX_CAPABILITIES] = {
			SKINNY_CODEC_NONSTANDARD, SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ULAW_56K,
			SKINNY_CODEC_G722_64K,    SKINNY_CODEC_G722_56K,      SKINNY_CODEC_G722_48K,      SKINNY_CODEC_NONE,
		};
		sccp_codec_combineSets(baseCodecArray, short2);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			// pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(result[x]));
			pbx_test_validate(test, baseCodecArray[x] == result[x]);
		}
	}
	pbx_test_status_update(test, "Executing combineCodecSet on one fully and one partially filled codecArray...\n");
	{
		uint8_t        x                                       = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE };
		memcpy(baseCodecArray, long1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		sccp_codec_combineSets(baseCodecArray, short2);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			// pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(long1[x]));
			pbx_test_validate(test, baseCodecArray[x] == long1[x]);
		}
	}
	pbx_test_status_update(test, "Executing combineCodecSet on one partially and one fully filled codecArray...\n");
	{
		uint8_t        x                                       = 0;
		skinny_codec_t baseCodecArray[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONE };
		memcpy(baseCodecArray, short1, sizeof(skinny_codec_t) * SKINNY_MAX_CAPABILITIES);
		const skinny_codec_t result[SKINNY_MAX_CAPABILITIES] = { SKINNY_CODEC_NONSTANDARD, SKINNY_CODEC_G711_ALAW_64K, SKINNY_CODEC_G711_ALAW_56K, SKINNY_CODEC_G711_ULAW_64K, SKINNY_CODEC_G711_ULAW_56K,
			                                                 SKINNY_CODEC_G729_A,      SKINNY_CODEC_G729,          SKINNY_CODEC_G728,          SKINNY_CODEC_G723_1,        SKINNY_CODEC_G722_48K,
			                                                 SKINNY_CODEC_G722_56K,    SKINNY_CODEC_G722_64K,      SKINNY_CODEC_IS11172,       SKINNY_CODEC_IS13818,       SKINNY_CODEC_G729_B,
			                                                 SKINNY_CODEC_G729_AB,     SKINNY_CODEC_GSM_FULLRATE,  SKINNY_CODEC_GSM_HALFRATE };
		sccp_codec_combineSets(baseCodecArray, long1);
		for (x = 0; x < SKINNY_MAX_CAPABILITIES; x++) {
			// pbx_test_status_update(test, "entry:%d = %s == %s\n", x, codec2str(baseCodecArray[x]), codec2str(result[x]));
			pbx_test_validate(test, baseCodecArray[x] == result[x]);
		}
	}
	return AST_TEST_PASS;
}

static void __attribute__((constructor)) sccp_register_tests(void)
{
	AST_TEST_REGISTER(chan_sccp_reduce_codec_set);
	AST_TEST_REGISTER(chan_sccp_combine_codec_sets);
}

static void __attribute__((destructor)) sccp_unregister_tests(void)
{
	AST_TEST_UNREGISTER(chan_sccp_reduce_codec_set);
	AST_TEST_UNREGISTER(chan_sccp_combine_codec_sets);
}
#endif
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
