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

//SCCP_FILE_VERSION(__FILE__, "$Revision: 2235 $")
/************************************************************************************************************ CALLERID **/

/*
 * \brief get callerid_name from pbx
 * \param ast_chan Asterisk Channel
 * \return char * with the callername
 */
char * get_pbx_callerid_name(struct ast_channel *ast_chan)
{
	static char result[StationMaxNameSize];
#ifndef CS_AST_CHANNEL_HAS_CID
	// Implement ast 1.2 version
	char *name, *number, *cidtmp;
	ast_callerid_parse(cidtmp, &name, &number);
	sccp_copy_string(result, name, sizeof(result) - 1);
	if (cidtmp)
        	ast_free(cidtmp);
	cidtmp = NULL;

        if (name)
        	ast_free(name);
	name = NULL;

	if (number)
        	ast_free(number);
        number = NULL;
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

#ifndef CS_AST_CHANNEL_HAS_CID
	// Implement ast 1.2 version
	char *name, *number, *cidtmp;
	ast_callerid_parse(cidtmp, &name, &number);
	sccp_copy_string(result, number, sizeof(result) - 1);
	if (cidtmp)
        	ast_free(cidtmp);
	cidtmp = NULL;

        if (name)
        	ast_free(name);
	name = NULL;

	if (number)
        	ast_free(number);
        number = NULL;
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

#define strcp_callerid(_sccp,_ast)                                     \
        sccp_copy_string(_sccp,_ast,sizeof(_sccp)-1);       	\

        if (callInfo->callingParty_valid) {
		strcp_callerid(callInfo->callingPartyName, ast_chan->cid.cid_num);
		strcp_callerid(callInfo->callingPartyNumber, ast_chan->cid.cid_name);
	}

	if (callInfo->originalCalled_valid) {
		strcp_callerid(callInfo->originalCallingPartyNumber, ast_chan->cid.cid_rdnis);		//RDNIS ? ANI
	}
	
	if (callInfo->lastRedirecting_valid) {
		strcp_callerid(callInfo->lastRedirectingPartyNumber, ast_chan->cid.cid_ani);		//RDNIS ? ANI
	}
	
	if (callInfo->originalCalled_valid) {
		strcp_callerid(callInfo->originalCalledPartyNumber, ast_chan->cid.cid_dnid);		//DNID
	}

	callInfo->presentation = (int)ast_chan->cid.cid_pres;
#endif

#undef strcp_callerid
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
#define strcp_callerid(_ast,_sccp)                              \
	 	sccp_copy_string(_ast,_sccp,sizeof(_ast)-1);    \

#if 0
	if (!strcmp(ast_chan->cid.cid_num, callInfo->callingPartyNumber) && !strcmp(ast_chan->cid.cid_name, callInfo->callingPartyName)) {
		strcp_callerid(ast_chan->cid.cid_num, callInfo->callingPartyNumber);
		strcp_callerid(ast_chan->cid.cid_name, callInfo->callingPartyName);
		callInfo->callingParty_valid=1;
	}
	
	if (!strcmp(ast_chan->cid.cid_ani, callInfo->calledPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_ani, callInfo->calledPartyNumber);
		callInfo->calledParty_valid=1;
	}
	
	if (!strcmp(ast_chan->cid.cid_rdnis, callInfo->originalCallingPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_rdnis, callInfo->originalCallingPartyNumber);		//RDNIS
		callInfo->originalCallingParty_valid=1;
	}
	
	if (!strcmp(ast_chan->cid.cid_ani, callInfo->lastRedirectingPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_ani, callInfo->lastRedirectingPartyNumber);		//ANI
		callInfo->lastRedirectingParty_valid=1;
	}

	if (!strcmp(ast_chan->cid.cid_dnid, callInfo->originalCalledPartyNumber)) {
		strcp_callerid(ast_chan->cid.cid_dnid, callInfo->originalCalledPartyNumber);		//DNID
		callInfo->originalCalledParty_valid;
	}

	ast_chan->cid.cid_pres = (int)callInfo->presentation;
#endif
#endif

#undef strcp_callerid
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
