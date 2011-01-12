
/*!
 * \file 	sccp_pbx_wrapper.c
 * \brief 	SCCP PBX Wrapper Class
 * \author 	Diederik de Groot <ddegroot [at] users.sourceforge.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 *
 * $Date: 2010-10-23 20:04:30 +0200 (Sat, 23 Oct 2010) $
 * $Revision: 2044 $  
 */
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision: 2235 $")

/************************************************************************************************************ CALLERID **/

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the callername
 */
char * get_pbx_callerid_name(struct ast_channel *ast_chan)
{
	static char result[StationMaxNameSize];
#if ASTERISK_VERSION_NUM < 10400
	// Implement ast 1.2 version
#else
	sccp_copy_string(result, ast_chan->cid.cid_name, sizeof(result) - 1);
#endif
	return result;
}

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the callername
 */
char * get_pbx_callerid_number(struct ast_channel *ast_chan)
{
	static char result[StationMaxDirnumSize];

#if ASTERISK_VERSION_NUM < 10400
	// Implement ast 1.2 version
#else
	sccp_copy_string(result, ast_chan->cid.cid_num, sizeof(result) - 1);
#endif
	return result;
}

/*
 * \brief get callerid from pbx
 * \param ast_chan Asterisk Channel
 * \return SCCP CallInfo Structure 
 *
 * \todo need to be inspected and tested
 */
sccp_callinfo_t *get_pbx_callerid(struct ast_channel * ast_chan)
{
	sccp_callinfo_t *callInfo = NULL;

	if (!ast_chan) {
		return callInfo;
	}
#if ASTERISK_VERSION_NUM < 10400
	// Implement ast 1.2 version
#else

#define strcmp_callerid(_sccp,_ast)                                     \
        if(_ast.valid) {                                                \
                sccp_copy_string(_sccp,_ast.str,sizeof(_sccp)-1);       \
        }												// MACRO Definition

#if 0
	if (ast_chan->connected.id.name.valid) {
		strcmp_callerid(callInfo.calledPartyName, ast_chan->connected.id.name);
		strcmp_callerid(callInfo.calledPartyNumber, ast_chan->connected.id.number);
	} else if (ast_chan->dialed.id.name.valid) {
		strcmp_callerid(callInfo.calledPartyName, ast_chan->dialed.id.name);
		strcmp_callerid(callInfo.calledPartyNumber, ast_chan->dialed.id.number);
	}

	strcmp_callerid(callInfo.callingPartyName, ast_chan->caller.id.number.str);
	strcmp_callerid(callInfo.callingPartyNumber, ast_chan->caller.id.number.str);

	strcmp_callerid(callInfo.originalCallingPartyName, ast_chan->redirecting.to.name.str);		//RDNIS
	strcmp_callerid(callInfo.originalCallingPartyNumber, ast_chan->redirecting.to.number.str);	//RDNIS

	strcmp_callerid(callInfo.lastRedirectingPartyName, ast_chan->redirecting.from.name.str);	//RDNIS
	strcmp_callerid(callInfo.lastRedirectingPartyNumber, ast_chan->redirecting.from.number.str);	//RDNIS

	/*        strcmp_callerid(callInfo.originalCalledPartyName, ); */				// Not available in ast 1.8 
	strcmp_callerid(callInfo.originalCalledPartyNumber, ast_chan->dialed.number.str);		//DNID

	callInfo.originalCdpnRedirectReason = (uint32_t) ast_chan->redirecting.reason;
	callInfo.lastRedirectingReason = (uint32_t) ast_chan->redirecting.reason;
	callInfo.presentation = (int)ast_chan->caller.id.number.presentation;
#endif

#undef strcmp_callerid
#endif
	return callInfo;
}

/*
 * \brief set callerid on pbx channel
 * \param ast_chan Asterisk Channel
 * \param callInfo SCCP CallInfo Structure
 * \return Fail/Success as int
 *
 * \todo need to be inspected and tested
 */
int set_pbx_callerid(struct ast_channel *ast_chan, sccp_callinfo_t * callInfo)
{
	int RESULT = 0;

	if (!ast_chan) {
		return -1;
	}
	
	if (!callInfo) {
		return -1;
	}

#if ASTERISK_VERSION_NUM < 10400
	// Implement ast 1.2 version
#else
#define strcmp_callerid(_ast,_sccp)                                     \
        if(!strcmp(_ast.str,_sccp)) {                                   \
	 	sccp_copy_string(_ast.str,_sccp,sizeof(_ast.str)-1);     \
	 	_ast.valid = 1;                    			\
	}												// MACRO Definition

#if 0
	strcmp_callerid(ast_chan->dialed.id.number, callInfo.calledPartyNumber);			// Asterisk makes a distinction between dialed and connected
	strcmp_callerid(ast_chan->dialed.id.name, callInfo.calledPartyName);

	strcmp_callerid(ast_chan->connected.id.number, callInfo.calledPartyNumber);			// Asterisk makes a distinction between dialed and connected 
	strcmp_callerid(ast_chan->connected.id.name, callInfo.calledPartyName);

	strcmp_callerid(ast_chan->caller.id.number, callInfo.callingPartyNumber);
	strcmp_callerid(ast_chan->caller.id.name, callInfo.callingPartyName);

	strcmp_callerid(ast_chan->redirecting.to.name, callInfo.originalCallingPartyName);		//RDNIS
	strcmp_callerid(ast_chan->redirecting.to.number, callInfo.originalCallingPartyNumber);		//RDNIS
	strcmp_callerid(ast_chan->redirecting.from.name, callInfo.lastRedirectingPartyName);		//RDNIS
	strcmp_callerid(ast_chan->redirecting.from.number, callInfo.lastRedirectingPartyNumber);	//RDNIS

	strcmp_callerid(ast_chan->dialed.number, callInfo.originalCalledPartyNumber);			//DNID
	/*        strcmp_callerid(, callInfo.originalCalledPartyName); */				// Not available in ast 1.8 

	/*        ast_chan->redirecting.reason = (int)callInfo.originalCdpnRedirectReason; */		// Which one should we send 
	ast_chan->redirecting.reason = (int)callInfo.lastRedirectingReason ast_chan->caller.id.number.presentation = (int)callInfo.presentation;
#endif

#undef strcmp_callerid
#endif
	return RESULT;
}

/*
 * \brief itterate through locked pbx channels
 * \note replacement for ast_channel_walk_locked
 * \param ast_chan Asterisk Channel
 * \return ast_chan Locked Asterisk Channel
 */
struct ast_channel *pbx_channel_walk_locked(struct ast_channel *target)
{
	ast_channel_lock(target);
	return ast_channel_walk_locked(target);
}

/******************************************************************************************************** NET / SOCKET **/
/*!
 * \brief pbx_inet_ntoa
 * Chooses correct asterisk ast_inet_ntoa
 * thread safe replacement of inet_ntoa 
 * \note replacement for ast_inet_ntoa
 * \param ia in address
 * \return const char *
 */
const char *pbx_inet_ntoa(struct in_addr ia)
{
#if ASTERISK_VERSION_NUM < 10400
	char iabuf[INET_ADDRSTRLEN];
	return ast_inet_ntoa(iabuf, sizeof(iabuf), ia);
#else
	return ast_inet_ntoa(ia);
#endif
}

/***************************************************************************************************************** RTP **/
/*!
 * \brief pbx rtp get peer
 * \note replacement for ast_rtp_get_peer
 * \param rtp Asterisk RTP Struct
 * \param them Socket Addr_in of the Peer
 * \return int
 */
int pbx_rtp_get_peer(struct ast_rtp *rtp, struct sockaddr_in *them) {
	return ast_rtp_get_peer(rtp, them);
}

/*!
 * \brief pbx rtp set peer
 * \note replacement for ast_rtp_set_peer
 * \param rtp Asterisk RTP Struct
 * \param them Socket Addr_in of the Peer
 */
void pbx_rtp_set_peer(struct ast_rtp *rtp, struct sockaddr_in *them) {
	ast_rtp_set_peer(rtp, them);
}
