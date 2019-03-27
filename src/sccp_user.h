/*!
 * \file        sccp_user.h
 * \brief       SCCP Line Header
 * \author      Diederik de Groot <ddegroot [at] sf.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */
#pragma once

#define sccp_user_retain(_x)			sccp_refcount_retain_type(sccp_user_t, _x)
#define sccp_user_release(_x)			sccp_refcount_release_type(sccp_user_t, _x)
#define sccp_user_refreplace(_x, _y)		sccp_refcount_refreplace_type(sccp_user_t, _x, _y)

#include "sccp_device.h"
//#include "forward_declarations.h"

__BEGIN_C_EXTERN__
/*!
 * \brief SCCP Line Structure
 * \note A user is the equivalent of a 'phone user' going to the phone.
 */
struct sccp_user {
	//sccp_mutex_t lock;											/*!< Asterisk: Lock Me Up and Tie me Down */
	char id[SCCP_MAX_USER_ID];										/*!< This user's ID, used for login (for mobility) */
	char name[SCCP_MAX_USERNAME];										/*!< The name of the user */
	char pin[SCCP_MAX_USER_PIN];										/*!< PIN number for mobility/roaming cxml interface */
	char *password;												/*!< Password for mobility webinterface */
	boolean_t login_multiple;										/*!< Allow user to be logged into multiple devices */
	sccp_buttonconfig_list_t buttondefinition;								/*!< SCCP Button Config Definition for this User */

	SCCP_RWLIST_ENTRY (sccp_user_t) list;									/*!< list entry */

#ifdef CS_SCCP_REALTIME
	boolean_t realtime;											/*!< is it a realtimeconfiguration */
#endif
	/* this is for reload routines */
	boolean_t pendingDelete;										/*!< this bit will tell the scheduler to delete this user when unused */
	boolean_t pendingUpdate;										/*!< this bit will tell the scheduler to update this user when unused */
};														/*!< SCCP User Structure */

SCCP_API void SCCP_CALL sccp_user_pre_reload(void);
SCCP_API void SCCP_CALL sccp_user_post_reload(void);

/* live cycle */
SCCP_API sccp_user_t * SCCP_CALL sccp_user_create(const char *name);
SCCP_API void SCCP_CALL sccp_user_addToGlobals(sccp_user_t * user);
SCCP_API void SCCP_CALL sccp_user_removeFromGlobals(sccp_user_t * user);
SCCP_API void SCCP_CALL sccp_user_clean(sccp_user_t * user, boolean_t remove_from_global);

// find user
SCCP_API sccp_user_t * SCCP_CALL sccp_user_find_byname(const char *name, uint8_t useRealtime);
//SCCP_API sccp_user_t * SCCP_CALL sccp_user_find_realtime_byname(const char *name);
SCCP_API sccp_user_t * SCCP_CALL sccp_user_find_byid(const char *name, uint8_t useRealtime);
//SCCP_API sccp_user_t * SCCP_CALL sccp_user_find_realtime_byid(const char *name);

SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findByUserId(char userid[SCCP_MAX_USER_ID]);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findByUsername(char username[SCCP_MAX_USERNAME]);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findByDevice(sccp_device_t *device);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findByDeviceId(char deviceid[StationMaxDeviceNameSize]);
SCCP_API void SCCP_CALL sccp_usersession_replaceButtonconfig(sccp_device_t *device);

/*
SCCP_API int SCCP_CALL sccp_usersession_add(sccp_user_t *user, sccp_device_t *device);
SCCP_API int SCCP_CALL sccp_usersession_remove(sccp_usersession_t *usersession);
SCCP_API int SCCP_CALL sccp_usersession_removeByUser(sccp_user_t *user);
SCCP_API int SCCP_CALL sccp_usersession_removeByDevice(sccp_device_t *device);
SCCP_API int SCCP_CALL sccp_usersession_setTimeout(sccp_usersession_t *usersession, int timeout);
SCCP_API int SCCP_CALL sccp_usersession_handleTimeout(void);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_find(sccp_user_t *user, sccp_device_t *device);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findByDevice(sccp_device_t *device);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findBySessionId(char *session_id);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findByCookie(char *cookie);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findByUser(sccp_user_t *user);
SCCP_API sccp_usersession_t * SCCP_CALL sccp_usersession_findByUsername(char *username);
SCCP_API sccp_buttonconfig_list_t * SCCP_CALL sccp_usersession_getButtonconfig(sccp_usersession_t *usersession);
*/

// CLI/AMI Commands
SCCP_API char * SCCP_CALL sccp_complete_user(OLDCONST char *line, OLDCONST char *word, int pos, int state);
SCCP_API int SCCP_CALL sccp_show_users(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
SCCP_API int SCCP_CALL sccp_user_command(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[]);
__END_C_EXTERN__
// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; user-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
