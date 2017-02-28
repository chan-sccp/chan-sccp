/*!
 * \file        sccp_codec.h
 * \brief       SCCP Codec Header
 * \author      Diederik de Groot < ddegroot@users.sourceforge.net >
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
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
extern const struct skinny_codec skinny_codecs[];
SCCP_API uint8_t SCCP_CALL sccp_codec_getArrayLen(void);
SCCP_INLINE const char * SCCP_CALL codec2str(skinny_codec_t value);
SCCP_INLINE const char * SCCP_CALL codec2name(skinny_codec_t value);
SCCP_INLINE const skinny_payload_type_t codec2type(skinny_codec_t value);
SCCP_API char * SCCP_CALL sccp_codec_multiple2str(char *buf, size_t size, const skinny_codec_t * codecs, const int clength);
SCCP_API int SCCP_CALL sccp_codec_parseAllowDisallow(skinny_codec_t * skinny_codec_prefs, const char *list, int allowing);
SCCP_API boolean_t SCCP_CALL sccp_codec_isCompatible(skinny_codec_t codec, const skinny_codec_t capabilities[], uint8_t length);
SCCP_API void SCCP_CALL sccp_codec_reduceSet(skinny_codec_t base[SKINNY_MAX_CAPABILITIES], const skinny_codec_t reduceByCodecs[SKINNY_MAX_CAPABILITIES]);
SCCP_API void SCCP_CALL sccp_codec_combineSets(skinny_codec_t base[SKINNY_MAX_CAPABILITIES], const skinny_codec_t addCodecs[SKINNY_MAX_CAPABILITIES]);
SCCP_API skinny_codec_t SCCP_CALL sccp_codec_findBestJoint(const skinny_codec_t ourPreferences[], int pLength, const skinny_codec_t ourCapabilities[], int cLength, const skinny_codec_t remotePeerCapabilities[], int rLength);

__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
