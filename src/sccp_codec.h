/*!
 * \file        sccp_protocol.h
 * \brief       SCCP Protocol Header
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 * \note        Thank you Federico Santulli for SPCP support
 */
#pragma once
#include "define.h"

#define SKINNY_MAX_CAPABILITIES       			18							/*!< max capabilities allowed in Cap response message */
#define SKINNY_MAX_VIDEO_CAPABILITIES			10
#define SKINNY_MAX_DATA_CAPABILITIES   			5

__BEGIN_C_EXTERN__

/*!
 * \brief SKINNY Codecs (ENUM) 
 */
typedef enum {
	/* *INDENT-OFF* */
	SKINNY_CODEC_NONE 				= 0x0000,
	SKINNY_CODEC_NONSTANDARD 			= 0x0001,
	SKINNY_CODEC_G711_ALAW_64K 			= 0x0002,
	SKINNY_CODEC_G711_ALAW_56K 			= 0x0003,
	SKINNY_CODEC_G711_ULAW_64K 			= 0x0004,
	SKINNY_CODEC_G711_ULAW_56K 			= 0x0005,
	SKINNY_CODEC_G722_64K 				= 0x0006,
	SKINNY_CODEC_G722_56K 				= 0x0007,
	SKINNY_CODEC_G722_48K 				= 0x0008,
	SKINNY_CODEC_G723_1 				= 0x0009,
	SKINNY_CODEC_G728 				= 0x000A,
	SKINNY_CODEC_G729 				= 0x000B,
	SKINNY_CODEC_G729_A 				= 0x000C,
	SKINNY_CODEC_IS11172 				= 0x000D,
	SKINNY_CODEC_IS13818 				= 0x000E,
	SKINNY_CODEC_G729_B 				= 0x000F,
	SKINNY_CODEC_G729_AB 				= 0x0010,
	SKINNY_CODEC_GSM_FULLRATE 			= 0x0012,
	SKINNY_CODEC_GSM_HALFRATE 			= 0x0013,
	SKINNY_CODEC_GSM_ENH_FULLRATE 			= 0x0014,
	SKINNY_CODEC_WIDEBAND_256K 			= 0x0019,
	SKINNY_CODEC_DATA_64K 				= 0x0020,
	SKINNY_CODEC_DATA_56K 				= 0x0021,
	SKINNY_CODEC_G722_1_32K 			= 0x0028,
	SKINNY_CODEC_G722_1_24K 			= 0x0029,
	SKINNY_CODEC_AAC 				= 0x002A,
	SKINNY_CODEC_MP4A_LATM_128 			= 0x002B,		// AAC-LD
	SKINNY_CODEC_MP4A_LATM_64 			= 0x002C,
	SKINNY_CODEC_MP4A_LATM_56 			= 0x002D,
	SKINNY_CODEC_MP4A_LATM_48 			= 0x002E,
	SKINNY_CODEC_MP4A_LATM_32 			= 0x002F,
	SKINNY_CODEC_MP4A_LATM_24 			= 0x0030,
	SKINNY_CODEC_MP4A_LATM_NA 			= 0x0031,
	SKINNY_CODEC_GSM 				= 0x0050,
	SKINNY_CODEC_ACTIVEVOICE 			= 0x0051,
	SKINNY_CODEC_G726_32K 				= 0x0052,
	SKINNY_CODEC_G726_24K 				= 0x0053,
	SKINNY_CODEC_G726_16K 				= 0x0054,
	SKINNY_CODEC_G729_ANNEX_B 			= 0x0055,
	SKINNY_CODEC_G729_B_LOW 			= 0x0056,		// ILBC
	SKINNY_CODEC_ISAC 				= 0x0059,
	SKINNY_CODEC_OPUS				= 0x005a,		// New
	SKINNY_CODEC_AMR				= 0x0061,
	SKINNY_CODEC_AMR_WB				= 0x0062,
	SKINNY_CODEC_H261 				= 0x0064,
	SKINNY_CODEC_H263 				= 0x0065,
	SKINNY_CODEC_H263P 				= 0x0066,
	SKINNY_CODEC_H264 				= 0x0067,
	SKINNY_CODEC_H264_SVC				= 0x0068,
	SKINNY_CODEC_T120 				= 0x0069,
	SKINNY_CODEC_H224 				= 0x006A,
	SKINNY_CODEC_T38FAX				= 0x006B,
	SKINNY_CODEC_TOTE				= 0x006C,
	SKINNY_CODEC_H265				= 0x006D,
	SKINNY_CODEC_H264_UC  				= 0x006E,
	SKINNY_CODEC_XV150_MR_711U			= 0x006F,
	SKINNY_CODEC_NSE_VBD_711U			= 0x0070,
	SKINNY_CODEC_XV150_MR_729A			= 0x0071,
	SKINNY_CODEC_NSE_VBD_729A			= 0x0072,
	SKINNY_CODEC_H264_FEC				= 0x0073,
	SKINNY_CODEC_CLEAR_CHAN				= 0x0078,
	SKINNY_CODEC_UNIVERSAL_XCODER			= 0x00DE,
	SKINNY_CODEC_DTMF_OOB_RFC2833			= 0x0101,		// OUTOFBAND / DTMF 0x101 / RFC2833_DYNAMIC_PAYLOAD
	SKINNY_CODEC_DTMF_PASSTHROUGH			= 0x0102,
	SKINNY_CODEC_DTMF_DYNAMIC    			= 0x0103,
	SKINNY_CODEC_DTMF_OOB				= 0x0104,		// OUTOFBAND
	SKINNY_CODEC_DTMF_IB_RFC2833			= 0x0105,		// INBAND
	SKINNY_CODEC_CFB_TONES				= 0x0106,
	SKINNY_CODEC_DTMF_NOAUDIO  			= 0x012B,
	SKINNY_CODEC_V150_LC_MODEM_RELAY		= 0x012C,
	SKINNY_CODEC_V150_LC_SPRT			= 0x012D,
	SKINNY_CODEC_V150_LC_SSE			= 0x012E,
	/* *INDENT-ON* */
} skinny_codec_t;

/*! 
 * \brief SKINNY Codec Types (ENUM)
 */
typedef enum {
	/* *INDENT-OFF* */
	SKINNY_CODEC_TYPE_UNKNOWN 			= 0,
	SKINNY_CODEC_TYPE_AUDIO 			= 1,
	SKINNY_CODEC_TYPE_VIDEO 			= 2,
	SKINNY_CODEC_TYPE_TEXT 				= 3,
	SKINNY_CODEC_TYPE_DATA 				= 4,
	SKINNY_CODEC_TYPE_MIXED 			= 5,
	/* *INDENT-ON* */
} skinny_payload_type_t;

/*!
 * \brief SKINNY Codec Structure
 */
struct skinny_codec {
	skinny_codec_t codec;
	skinny_payload_type_t codec_type;
	const char *const key;											// used in sccp.conf
	const char *const name;											// used in sccp.conf
	const char *const text;											// used to display the codec name
	const char *mimesubtype;										// rfc mime sub-type e.g. L16
	unsigned int sample_rate;
	unsigned int sound_quality;
	unsigned int rtp_payload_type;
};

static const struct skinny_codec skinny_codecs[] = {
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
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
