/*!
 * \file 	sccp_adv_features.h
 * \brief 	SCCP Advanced Features Headers
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \date
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License. 
 *		See the LICENSE file at the top of the source tree.
 * \date        $Date$
 * \version     $Revision$
 */

#define DUMMY 1

void dummyproc1(void);
void dummyproc2(void);
void sccp_advfeat_conference(sccp_device_t *d, sccp_line_t *l, sccp_channel_t *c);
void sccp_advfeat_join(sccp_device_t *d, sccp_line_t *l, sccp_channel_t *c);
int sccp_advfeat_barge(sccp_channel_t * c, char * exten);
int sccp_advfeat_cbarge(sccp_channel_t * c, char * conferencenum);
