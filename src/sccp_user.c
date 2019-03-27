/*!
 * \file        sccp_user.c
 * \brief       SCCP User Class
 * \author      Diederik de Groot <ddegroot [at] sf.net>
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 */

#include "config.h"
#include "common.h"
#include "sccp_user.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_config.h"
#include "sccp_utils.h"
#include "sccp_vector.h"

SCCP_FILE_VERSION(__FILE__, "");

int __sccp_user_destroy(const void *ptr);
#define SCCP_MAX_SESSION_ID  8
#define SCCP_MAX_COOKIE 8
#define SCCP_USERSESSION_TIMEOUT 60
#define SESSIONSTR_LEN 160
static const uint32_t appID = APPID_EXTENSION_MOBILITY;

struct sccp_usersession {
	char deviceid[StationMaxDeviceNameSize];
	sccp_user_t *user;
	sccp_usersession_state_t state;
	long sessionid;
	long timestamp;		/* time_t */
	long timeout;		/* time_t */
};

/* Notes/Ideas:
 - Allow alpha/numberic userid
 - Clear phonelogs during login / logout (Yes/No)
 - Remember last user logged in
 - allow/disallow emcc on particular device
 - default user on device, without a buttontemplate for itself.
*/

/*!
 * \brief run before reload is start on users
 * \note See \ref sccp_config_reload
 *
 * \callgraph
 * \callergraph
 *
 */
void sccp_user_pre_reload(void)
{
	sccp_user_t *user = NULL;
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(users), user, list) {
#ifdef CS_SCCP_REALTIME
		if (user->realtime == FALSE)
		{
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "%s: Setting User to Pending Delete=1\n", user->name);
			user->pendingDelete = 1;
		} else
#endif
		{
			user->pendingUpdate = 1;
		}
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
}

/*!
 * \brief run after the new user config is loaded during the reload process
 * \note See \ref sccp_config_reload
 * \todo to be implemented correctly (***)
 *
 * \callgraph
 * \callergraph
 *
 */
void sccp_user_post_reload(void)
{
	sccp_user_t *user = NULL;

	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(users), user, list) {
		if (!user->pendingDelete && !user->pendingUpdate) {
			continue;
		}
		AUTO_RELEASE(sccp_user_t, u, sccp_user_retain(user));
		if (u) {
			if (u->pendingDelete) {
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "%s: Deleting User (post_reload)\n", u->name);
				sccp_user_clean(u, TRUE);
			} else {
				sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "%s: Cleaning User (post_reload)\n", u->name);
				sccp_user_clean(u, FALSE);
			}
			sccp_log((DEBUGCAT_CONFIG + DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "%s: User (user_post_reload) update:%d, delete:%d\n", u->name, u->pendingUpdate, u->pendingDelete);
		}
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
}

/* ================================================================================================================ user */

/*!
 * \brief Build Default SCCP User.
 *
 * Creates an SCCP User with default/global values
 *
 * \return Default SCCP User
 *
 * \callgraph
 * \callergraph
 */
sccp_user_t *sccp_user_create(const char *userid)
{
	sccp_user_t *user = NULL;

	if ((user = sccp_user_find_byid(userid, FALSE))) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: User '%s' already exists.\n", userid);
		sccp_user_release(&user);						/* explicit release of found user */
		return NULL;
	}

	user = (sccp_user_t *) sccp_refcount_object_alloc(sizeof(sccp_user_t), SCCP_REF_USER, userid, __sccp_user_destroy);
	if (!user) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, userid);
		return NULL;
	}
	memset(user, 0, sizeof(sccp_user_t));
	SCCP_LIST_HEAD_INIT(&user->buttondefinition);
	sccp_copy_string(user->id, userid, sizeof(user->id));

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: User '%s' created.\n", userid);

	return user;
}

/*!
 * Add a user to global user list.
 * \param user user pointer
 *
 * \note needs to be called with a retained user
 * \note adds a retained user to the list (refcount + 1)
 */
void sccp_user_addToGlobals(sccp_user_t * user)
{
	AUTO_RELEASE(sccp_user_t, u, sccp_user_retain(user));

	SCCP_RWLIST_WRLOCK(&GLOB(users));
	if (u) {
		/* add to list */
		sccp_user_retain(u);										/* add retained user to the list */
		SCCP_RWLIST_INSERT_SORTALPHA(&GLOB(users), u, list, id);
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Added user '%s' to Glob(users)\n", u->id);

		/* emit event */
		/*
		sccp_event_t *event = sccp_event_allocate(SCCP_EVENT_USERINSTANCE_CREATED);
		if (event) {
			event->userInstance.user = sccp_user_retain(u);
			sccp_event_fire(event);
		}
		*/
	} else {
		pbx_log(LOG_ERROR, "Adding null to global user list is not allowed!\n");
	}
	SCCP_RWLIST_UNLOCK(&GLOB(users));
}

/*!
 * Remove a user from the global user list.
 * \param user SCCP user pointer
 *
 * \note needs to be called with a retained user
 * \note removes the retained user withing the list (refcount - 1)
 */
void sccp_user_removeFromGlobals(sccp_user_t * user)
{
	sccp_user_t *removed_user = NULL;
	if (user) {
		SCCP_RWLIST_WRLOCK(&GLOB(users));
		removed_user = SCCP_RWLIST_REMOVE(&GLOB(users), user, list);
		SCCP_RWLIST_UNLOCK(&GLOB(users));

		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "Removed user '%s' from Glob(users)\n", removed_user->id);

		sccp_user_release(&removed_user);								/* explicit release */
	} else {
		pbx_log(LOG_ERROR, "Removing null from global user list is not allowed!\n");
	}
}

/*!
 * \brief Clean User
 *
 *  clean up memory allocated by the user.
 *  if destroy is true, user will be removed from global device list
 *
 * \param u SCCP User
 * \param remove_from_global as boolean_t
 *
 * \callgraph
 * \callergraph
 *
 */
void sccp_user_clean(sccp_user_t * user, boolean_t remove_from_global)
{
	if (remove_from_global) {
/*
		sccp_event_t *event = sccp_event_allocate(SCCP_EVENT_USERINSTANCE_DESTROYED);
		if (event) {
			event->userInstance.user = sccp_user_retain(u);
			sccp_event_fire(event);
		}
*/
		sccp_user_removeFromGlobals(user);									// final release
	}
}

/*!
 * \brief Free a User as scheduled command
 * \param ptr SCCP User Pointer
 * \return success as int
 *
 * \callgraph
 * \callergraph
 *
 */
#include "sccp_config.h"
int __sccp_user_destroy(const void *ptr)
{
	sccp_user_t *user = (sccp_user_t *) ptr;
	if (!user) {
		return -1;
	}
	sccp_log((DEBUGCAT_USER + DEBUGCAT_CONFIG)) (VERBOSE_PREFIX_1 "%s: User FREE\n", user->id);

	// cleanup dynamically allocated memory by sccp_config
	sccp_config_cleanup_dynamically_allocated_memory(user, SCCP_CONFIG_USER_SEGMENT);
	
	// clean button config (only generated on read config, so do not remove during device clean)
	{
		sccp_buttonconfig_t *config = NULL;
		SCCP_LIST_LOCK(&user->buttondefinition);
		while ((config = SCCP_LIST_REMOVE_HEAD(&user->buttondefinition, list))) {
			sccp_buttonconfig_destroy(config);
		}
		SCCP_LIST_UNLOCK(&user->buttondefinition);
		if (!SCCP_LIST_EMPTY(&user->buttondefinition)) {
			pbx_log(LOG_WARNING, "%s: (user_destroy) there are connected buttonconfigs left during user destroy\n", user->id);
		}
		SCCP_LIST_HEAD_DESTROY(&user->buttondefinition);
	}

	return 0;
}

#ifdef CS_SCCP_REALTIME
/*!
 * \brief Find User via Realtime
 *
 * \callgraph
 * \callergraph
 * \param name User Name
 * \return SCCP User
 */
static sccp_user_t *sccp_user_find_realtime_byname(const char *username)
{
	sccp_user_t *user = NULL;
	PBX_VARIABLE_TYPE *v = NULL, *variable = NULL;

	if (sccp_strlen_zero(GLOB(realtimeusertable))) {
		return NULL;
	}

	if (sccp_strlen_zero(username)) {
		sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for user with name ''\n");
		return NULL;
	}

	if ((variable = pbx_load_realtime(GLOB(realtimeusertable), "name", username, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_USER + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: User '%s' found in realtime table '%s'\n", username, GLOB(realtimeusertable));

		sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_4 "SCCP: creating realtime user '%s'\n", username);

		// SCCP_RWLIST_WRLOCK(&GLOB(users));
		PBX_VARIABLE_TYPE *tmpv = variable;
		const char *userid = ast_variable_find_in_list(tmpv, "id");
		if (!sccp_strlen_zero(userid) && (user = sccp_user_create(userid))) {				/* already retained */
			sccp_config_applyUserConfiguration(user, variable);
			user->realtime = TRUE;
			sccp_user_addToGlobals(user);								// can return previous instance on doubles
			pbx_variables_destroy(v);
		} else {
			pbx_log(LOG_ERROR, "SCCP: Unable to build realtime user '%s'\n", username);
		}
		// SCCP_RWLIST_UNLOCK(&GLOB(users));
		return user;
	}

	sccp_log((DEBUGCAT_USER + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: User '%s' not found in realtime table '%s'\n", username, GLOB(realtimeusertable));
	return NULL;
}
#endif

/*!
 * \brief Find User by Name
 *
 * \callgraph
 * \callergraph
 *
 */
sccp_user_t *sccp_user_find_byname(const char *username, uint8_t useRealtime)
{
	sccp_user_t *user = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(users));
	user = SCCP_RWLIST_FIND(&GLOB(users), sccp_user_t, tmpl, list, (sccp_strcaseequals(tmpl->name, username)), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	SCCP_RWLIST_UNLOCK(&GLOB(users));
#ifdef CS_SCCP_REALTIME
	if (!user && useRealtime) {
		user = sccp_user_find_realtime_byname(username);
	}
#endif
	if (!user) {
		sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "SCCP: User '%s' not found.\n", username);
		return NULL;
	}
	return sccp_user_retain(user);
}


#ifdef CS_SCCP_REALTIME
/*!
 * \brief Find User via Realtime
 *
 * \callgraph
 * \callergraph
 * \param name User Name
 * \return SCCP User
 */
static sccp_user_t *sccp_user_find_realtime_byid(const char *userid)
{
	sccp_user_t *user = NULL;
	PBX_VARIABLE_TYPE *v = NULL, *variable = NULL;

	if (sccp_strlen_zero(GLOB(realtimeusertable))) {
		return NULL;
	}

	if (sccp_strlen_zero(userid)) {
		sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "SCCP: Not allowed to search for user with name ''\n");
		return NULL;
	}

	if ((variable = pbx_load_realtime(GLOB(realtimeusertable), "id", userid, NULL))) {
		v = variable;
		sccp_log((DEBUGCAT_USER + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: User '%s' found in realtime table '%s'\n", userid, GLOB(realtimeusertable));

		sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_4 "SCCP: creating realtime user '%s'\n", userid);

		// SCCP_RWLIST_WRLOCK(&GLOB(users));
		if ((user = sccp_user_create(userid))) {							/* already retained */
			sccp_config_applyUserConfiguration(user, variable);
			user->realtime = TRUE;
			sccp_user_addToGlobals(user);								// can return previous instance on doubles
			pbx_variables_destroy(v);
		} else {
			pbx_log(LOG_ERROR, "SCCP: Unable to build realtime user '%s'\n", userid);
		}
		// SCCP_RWLIST_UNLOCK(&GLOB(users));
		return user;
	}

	sccp_log((DEBUGCAT_USER + DEBUGCAT_REALTIME)) (VERBOSE_PREFIX_3 "SCCP: User '%s' not found in realtime table '%s'\n", userid, GLOB(realtimeusertable));
	return NULL;
}
#endif
/*!
 * \brief Find User by Name
 *
 * \callgraph
 * \callergraph
 *
 */
sccp_user_t *sccp_user_find_byid(const char *userid, uint8_t useRealtime)
{
	sccp_user_t *user = NULL;

	SCCP_RWLIST_RDLOCK(&GLOB(users));
	user = SCCP_RWLIST_FIND(&GLOB(users), sccp_user_t, tmpl, list, (sccp_strcaseequals(tmpl->id, userid)), TRUE, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	SCCP_RWLIST_UNLOCK(&GLOB(users));
#ifdef CS_SCCP_REALTIME
	if (!user && useRealtime) {
		user = sccp_user_find_realtime_byid(userid);
	}
#endif
	if (!user) {
		sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "SCCP: User '%s' not found.\n", userid);
		return NULL;
	}
	return sccp_user_retain(user);
}

/* ================================================================================================================ usersession */
#ifndef ASTDB_FAMILY_KEY_LEN
#define ASTDB_FAMILY_KEY_LEN 100
#endif
#ifndef ASTDB_RESULT_LEN
#define ASTDB_RESULT_LEN 80
#endif

/*!
 * usersession_parseDBString
 * returns malloced sccp_usersession_t
 * make sure to free after use
 */
static sccp_usersession_t *usersession_parseDBString(char *dbstring)
{
	sccp_usersession_t *usersession = (sccp_usersession_t *)sccp_calloc(sizeof *usersession, 1);
	if (!usersession) {
		// error
		return NULL;
	}
	unsigned int state = 0;
	char userid[16] = "";
	//dbString:'deviceid=SEPE0D173E11D95;state=1;sessionid=3828827;timestamp=1551726863'
	sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "SCCP: (parseDBString) '%s'\n", dbstring);
	int res = sscanf(dbstring, "deviceid=%15[^;];state=%u;sessionid=%lx;timestamp=%ld;timeout=%ld;userid=%15s", 
			usersession->deviceid, &state,
			&usersession->sessionid,
			&usersession->timestamp, &usersession->timeout,
			userid);
	if (res	== 5) {			// state = processlogin
		sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "SCCP: (parseDBString) matched state processlogin\n")
	} else if (res	== 6) {		// state = logged in
		sccp_log((DEBUGCAT_USER)) (VERBOSE_PREFIX_3 "SCCP: (parseDBString) matched state logged in\n")
		usersession->user = sccp_user_find_byid(userid, FALSE);
		if (!usersession->user) {
			pbx_log(LOG_ERROR, "SCCP: (parseDBString) could not find provided userid:%s. giving up.\n", userid);
			sccp_free(usersession);
			return NULL;
		}
		sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_3 "SCCP: (parseDBString) matched to user:%s\n", usersession->user->id);
	} else {
		pbx_log(LOG_ERROR, "dbstring could not be parsed (only matched:%d)\n", res);
		sccp_free(usersession);
		return NULL;
	};
	return usersession;
}

static char * usersession_createDBString(sccp_usersession_t *usersession, char *buffer, size_t buflen)
{
	snprintf(buffer, buflen, "deviceid=%s;state=%u;sessionid=%lx;timestamp=%ld;timeout=%ld;%s%s",
		usersession->deviceid, usersession->state,
		usersession->sessionid,
		usersession->timestamp, usersession->timeout,
		usersession->user ? "userid=" : "", usersession->user ? usersession->user->id : "");
	return buffer;
}

/*!
 * sccp_usersession_findByUserId
 * returns malloced sccp_usersession_t
 * make sure to free after use
 */
sccp_usersession_t * sccp_usersession_findByUserId(char userid[SCCP_MAX_USER_ID])
{
	char deviceid[ASTDB_RESULT_LEN] = { 0 };
	char sessionstr[SESSIONSTR_LEN] = { 0 };
	if (iPbx.feature_getFromDatabase) {
		if (iPbx.feature_getFromDatabase("SCCP/emsession/userid", userid, deviceid, sizeof(deviceid)) && !sccp_strlen_zero(deviceid)) {
			if (iPbx.feature_getFromDatabase("SCCP/emsession/deviceid", deviceid, sessionstr, sizeof(sessionstr)) && !sccp_strlen_zero(sessionstr)) {
				return usersession_parseDBString(sessionstr);
			}		
		}
	}
	return NULL;
}

/*!
 * sccp_usersession_findByUsername
 * returns malloced sccp_usersession_t
 * make sure to free after use
 */
sccp_usersession_t * sccp_usersession_findByUsername(char username[SCCP_MAX_USERNAME])
{
	sccp_user_t *user=sccp_user_find_byname(username, TRUE);
	if (user) {
		return sccp_usersession_findByUserId(user->id);
	}
	return NULL;
}

/*!
 * sccp_usersession_findByDevice
 * returns malloced sccp_usersession_t
 * make sure to free after use
 */
sccp_usersession_t * sccp_usersession_findByDevice(sccp_device_t *d)
{
	return d ? sccp_usersession_findByDeviceId(d->id) : NULL;
}

/*!
 * sccp_usersession_findByDeviceId
 * returns malloced sccp_usersession_t
 * make sure to free after use
 */
sccp_usersession_t * sccp_usersession_findByDeviceId(char deviceid[StationMaxDeviceNameSize])
{
	char sessionstr[SESSIONSTR_LEN] = { 0 };
	if (iPbx.feature_getFromDatabase) {
		if (iPbx.feature_getFromDatabase("SCCP/emsession/deviceid", deviceid, sessionstr, sizeof(sessionstr)) && !sccp_strlen_zero(sessionstr)) {
			return usersession_parseDBString(sessionstr);
		}		
	}
	return NULL;
}


/*!
 * sccp_usersession_findByDeviceUser
 * returns malloced sccp_usersession_t
 * make sure to free after use
 */
static sccp_usersession_t * sccp_usersession_findByDeviceUser(constDevicePtr device, const sccp_user_t *user)
{
	char deviceid[ASTDB_RESULT_LEN] = { 0 };
	char sessionstr[SESSIONSTR_LEN] = { 0 };
	if (iPbx.feature_getFromDatabase) {
		if (iPbx.feature_getFromDatabase("SCCP/emsession/userid", user->id, deviceid, sizeof(deviceid)) && !sccp_strlen_zero(deviceid)) {
			if (sccp_strcaseequals(deviceid, device->id)) {
				if (iPbx.feature_getFromDatabase("SCCP/emsession/deviceid", deviceid, sessionstr, sizeof(sessionstr)) && !sccp_strlen_zero(sessionstr)) {
					return usersession_parseDBString(sessionstr);
				}
			}
		}
	}
	return NULL;
}


/*!
 * sccp_usersession_getButtonconfig
 * returns a pointer to the user buttonconfig if found
 * make sure to free after use
 */
/*
sccp_buttonconfig_list_t * sccp_usersession_getButtonconfig(sccp_usersession_t *usersession)
{
	if (usersession && usersession->user) {
		sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_3 "Assigning new buttonconfig: %p", &(usersession->user->buttondefinition));
		// check logout_timestamp;
		return &(usersession->user->buttondefinition);
	}
	return NULL;
}
*/
void sccp_usersession_replaceButtonconfig(sccp_device_t *device)
{
	sccp_usersession_t *usersession = sccp_usersession_findByDevice(device);
	if (usersession && usersession->user) {
		// revalidate / check timestamp etc ;
		sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_3 "Replacing device->buttonconfig with %p from %s\n", &(usersession->user->buttondefinition), usersession->user->id);
		device->buttonconfig = &(usersession->user->buttondefinition);
		if (device->userid) {
			sccp_free(device->userid);
		}
		device->userid = pbx_strdup(usersession->user->id);
		sccp_free(usersession);
	} else {
		device->buttonconfig = &device->buttondefinition;
		if (device->userid) {
			sccp_free(device->userid);
		}
	}
}

int sccp_user_handle_login(sccp_device_t *d, sccp_user_t *u);
void sccp_user_handle_logout(sccp_usersession_t *usersession);

int sccp_user_handle_login(sccp_device_t *d, sccp_user_t *u)
{
	int res = -2;
	sccp_usersession_t * usersession = sccp_usersession_findByDeviceId(d->id);
	if (!usersession) {
		return -2;
	}
	sccp_free(usersession);
	return res;
}

/*
/SCCP/emsession/deviceid/SEP0023043403F9          : deviceid=SEP0023043403F9;state=2;sessionid=71207243;timestamp=1553709336;timeout=1553712936;userid=1000
/SCCP/emsession/deviceid/SEPE0D173E11D95          : deviceid=SEPE0D173E11D95;state=2;sessionid=54a70d02;timestamp=1551837393;timeout=1551840993;userid=1234
/SCCP/emsession/userid/1000                       : SEP0023043403F9          
/SCCP/emsession/userid/1234                       : SEPE0D173E11D95          
*/
void sccp_user_handle_logout(sccp_usersession_t *usersession)
{
	/*
	if (usersession->user) {
		iPbx.feature_removeFromDatabase("SCCP/emsession/userid", usersession->user->id);
	}*/
	iPbx.feature_removeFromDatabase("SCCP/emsession/deviceid", usersession->deviceid);
}

/*! \todo split into create_tmpsession & pushurl */
static sccp_usersession_t * sccp_user_handle_sessionrequest(sccp_device_t *d)
{
	char sessionstr[SESSIONSTR_LEN] = { 0 };
	if (!d) {
		return NULL;
	}
	sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "handle pre login on %s\n", d->id);
	
	// create temp session
	sccp_usersession_t *usersession = (sccp_usersession_t *)sccp_calloc(sizeof *usersession, 1);
	sccp_copy_string(usersession->deviceid, d->id, sizeof(usersession->deviceid));
	usersession->user = NULL;
	usersession->state = SCCP_USERSESSION_PROGRESS;
	usersession->sessionid = sccp_random();
	time_t current_time = time(NULL);
	usersession->timestamp = (long)current_time;
	usersession->timeout = (long)current_time + (5*60);		// add 5 min for timeout
	
	// update database
	usersession_createDBString(usersession, sessionstr, SESSIONSTR_LEN);
	if (!sccp_strlen_zero(sessionstr)) {
		iPbx.feature_removeFromDatabase("SCCP/emsession/deviceid", d->id);
		if (iPbx.feature_addToDatabase("SCCP/emsession/deviceid", d->id, sessionstr)) {
			sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "%s: pushed to database: '%s'\n", d->id, sessionstr);
		}
	}
	return usersession;
}

static int sccp_user_pushurl(sccp_device_t *d, char *url, long sessionid)
{
	pbx_str_t *xmlStr = pbx_str_alloca(2048);
	if (!d || !d->session || !d->protocol) {
		return -1;
	}

	// close any open forms
	if (d->protocolversion >= 15) {
		pbx_str_append(&xmlStr, 0, "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"App:Close:0\"/></CiscoIPPhoneExecute>");
	} else {
		pbx_str_append(&xmlStr, 0, "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"Init:Services\"/></CiscoIPPhoneExecute>");
	}
	d->protocol->sendUserToDeviceDataVersionMessage(d, appID, 0 /*callreference*/, 0 /*lineinstance*/, sessionid, pbx_str_buffer(xmlStr), 2);

	// push login form	
	pbx_str_reset(xmlStr);
	pbx_str_append(&xmlStr, 0, "<CiscoIPPhoneExecute><ExecuteItem Priority=\"0\" URL=\"%s\"/></CiscoIPPhoneExecute>", url);
	d->protocol->sendUserToDeviceDataVersionMessage(d, appID, 0 /*callreference*/, 0 /*lineinstance*/, sessionid, pbx_str_buffer(xmlStr), 2);
	sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "%s: pushed cxml:\n%s\n", d->id, pbx_str_buffer(xmlStr));
	return 0;
}
static int sccp_user_handle_loginform_request(sccp_device_t *d)
{
	char buffer[SESSIONSTR_LEN] = { 0 };
	if (!d || !d->session || !d->protocol || sccp_strlen_zero(GLOB(mobility_url))) {
		return -1;
	}
	sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "handle pre login on %s\n", d->id);
	sccp_usersession_t *usersession = sccp_user_handle_sessionrequest(d);
	if (usersession) {
		// \todo check if provided url already contains a '?" and use instead '&amp';
		snprintf(buffer, SESSIONSTR_LEN, "%s?name=%s&amp;action=loginform&amp;sessionid=%lx", GLOB(mobility_url), d->id, usersession->sessionid);
		sccp_user_pushurl(d, buffer, usersession->sessionid);
		sccp_free(usersession);
	}
	return 0;
}

static boolean_t usersession_isValid(const sccp_usersession_t *usersession, const char *userid, const char *pincode)
{
	boolean_t res = FALSE;
	sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "session:%p, userid:%s, pincode:%s\n", usersession, userid, pincode);
	do {
		if (!usersession) {
			pbx_log(LOG_ERROR, "session missing\n");
			break;
		}
		if (time(NULL) > usersession->timeout) {
			pbx_log(LOG_ERROR, "session timed-out\n");
			break;
		}
		if (!usersession->user) {
			pbx_log(LOG_ERROR, "no user assigned to this session yet (%p)\n", usersession->user);
			break;
		}
		if (!sccp_strequals(usersession->user->id, userid)) {
			pbx_log(LOG_ERROR, "userid does not match (%s <=> %s)\n", usersession->user->id, userid);
			break;
		}
		if (!sccp_strequals(usersession->user->pin, pincode)) {
			pbx_log(LOG_ERROR, "pincode does not match (%s <=> %s)\n", usersession->user->pin, pincode);
			break;
		}
		res = TRUE;
	} while (0);
	
	return res;
}

// time_t current_time;
// char* c_time_string;
//
// current_time = time(NULL);
// c_time_string = ctime(&current_time);
/* --------------------------------------------------------------------------------------------------------SHOW USERS- */
#include <asterisk/cli.h>
/*!
 * \brief Complete Conference
 * \param line Line as char
 * \param word Word as char
 * \param pos Pos as int
 * \param state State as int
 * \return Result as char
 * 
 * \called_from_asterisk
 * 
 */
// sccp user login diederik sepxxxxxx
// sccp user logout diederik
// sccp user show diederik
// sccp user sessions
// sccp device logout sepxxxxx
char *sccp_complete_user(OLDCONST char *line, OLDCONST char *word, int pos, int state)
{
	int wordlen = strlen(word), which = 0;
	uint i = 0;
	char *ret = NULL;
	//char tmpname[21];
	char *actions[] = { "show", "loginform", "processlogin", "login", "logout", "sessions" };

	switch (pos) {
		case 2:											// action
			for (i = 0; i < ARRAY_LEN(actions); i++) {
				if (!strncasecmp(word, actions[i], wordlen) && ++which > state) {
					return pbx_strdup(actions[i]);
				}
			}
			break;
		case 3:											// user
			{
				if (!strncasecmp(line, "sccp user loginform", 18))
				{
					sccp_device_t *d = NULL;
					SCCP_RWLIST_RDLOCK(&GLOB(devices));
					SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
						if (!strncasecmp(word, d->id, wordlen) && ++which > state && d->session) {
							ret = pbx_strdup(d->id);
							break;
						}
					}
					SCCP_RWLIST_UNLOCK(&GLOB(devices));
					break;
				}
				sccp_user_t *user = NULL;
				SCCP_RWLIST_RDLOCK(&GLOB(users));
				SCCP_RWLIST_TRAVERSE(&GLOB(users), user, list) {
					if (!strncasecmp(word, user->name, wordlen) && ++which > state) {
						ret = pbx_strdup(user->name);
						break;
					}
				}
				SCCP_RWLIST_UNLOCK(&GLOB(users));
				break;
			}
		case 4:											// device
			{
				if (!strncasecmp(line, "sccp user login", 15))
				{
					sccp_device_t *d = NULL;
					SCCP_RWLIST_RDLOCK(&GLOB(devices));
					SCCP_RWLIST_TRAVERSE(&GLOB(devices), d, list) {
						if (!strncasecmp(word, d->id, wordlen) && ++which > state && d->session) {
							ret = pbx_strdup(d->id);
							break;
						}
					}
					SCCP_RWLIST_UNLOCK(&GLOB(devices));
					break;
				}
				char *username = NULL;
				char *deviceid = NULL;
				if (sscanf(line, "sccp user logout %33s %17s", username, deviceid) > 0)
				{
					break;
				}
				break;
			}
		default:
			break;
	}
	return ret;
}
/*!
 * \brief Show Users
 * \param fd Fd as int
 * \param totals Total number of lines as int
 * \param s AMI Session
 * \param m Message
 * \param argc Argc as int
 * \param argv[] Argv[] as char
 * \return Result as int
 * 
 * \called_from_asterisk
 * 
 */
int sccp_show_users(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int local_line_total = 0;

	// table definition
#define CLI_AMI_TABLE_NAME Users
#define CLI_AMI_TABLE_PER_ENTRY_NAME User

#define CLI_AMI_TABLE_LIST_ITER_TYPE sccp_user_t
#define CLI_AMI_TABLE_LIST_ITER_HEAD &GLOB(users)
#define CLI_AMI_TABLE_LIST_ITER_VAR user
#define CLI_AMI_TABLE_LIST_LOCK SCCP_RWLIST_RDLOCK
#define CLI_AMI_TABLE_LIST_ITERATOR SCCP_RWLIST_TRAVERSE
#define CLI_AMI_TABLE_BEFORE_ITERATION 																\
	{																			\
		AUTO_RELEASE(sccp_user_t, u, sccp_user_retain(user));												\
		if (u) {
#define CLI_AMI_TABLE_AFTER_ITERATION 																\
		}																		\
	}
#define CLI_AMI_TABLE_LIST_UNLOCK SCCP_RWLIST_UNLOCK

#define CLI_AMI_TABLE_FIELDS 																	\
		CLI_AMI_TABLE_FIELD(id,			"-5.5",		s,	5,	u->id)									\
		CLI_AMI_TABLE_FIELD(name,		"32.32",	s,	32,	u->name)								\
		CLI_AMI_TABLE_FIELD(pin,		"-8.8",		s,	8,	u->pin)									\
		CLI_AMI_TABLE_FIELD(password,		"-16.16",	s,	16, 	u->password)								\
		CLI_AMI_TABLE_FIELD(mlogin,		"-8.8",		s,	8,	u->login_multiple ? "Yes" : "No") 					\
		CLI_AMI_TABLE_FIELD(realtime,		"-8.8",		s,	8, 	u->realtime ? "Yes" : "No")
#include "sccp_cli_table.h"
	// end of table definition
	if (s) {
		totals->lines = local_line_total;
		totals->tables = 1;
	}
	return RESULT_SUCCESS;
}

int sccp_user_command(int fd, sccp_cli_totals_t *totals, struct mansession *s, const struct message *m, int argc, char *argv[])
{
	int local_line_total = 0;
	int local_table_total = 0;
	int res = RESULT_FAILURE;
	AUTO_RELEASE(sccp_device_t,device, NULL);
	AUTO_RELEASE(sccp_user_t,user, NULL);
	char error[100];

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "User Command:%s, %s, %s\n", argv[2], argv[3], argc >= 5 ? argv[4] : "");
	if (argc < 2 || argc > 8 || sccp_strlen_zero(argv[2])) {
		return RESULT_SHOWUSAGE;
	}

	if (sccp_strcaseequals("loginform", argv[2]) && argc == 4) {
		if (sccp_strlen_zero(argv[3])) {
			return RESULT_SHOWUSAGE;
		}
		device = sccp_device_find_byid(argv[3], FALSE);
		if (device && sccp_user_handle_loginform_request(device) == 0) {
			sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Pushed login form to %s\n", argv[3]);
			res = RESULT_SUCCESS;
		} else {
			// handle error
		}
	}
	else if (sccp_strcaseequals("login", argv[2]) && argc == 5) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "User Login %s, %s\n", argv[3], argv[4]);
		if (sccp_strlen_zero(argv[3]) || sccp_strlen_zero(argv[4])) {
			return RESULT_SHOWUSAGE;
		}
		user = sccp_user_find_byname(argv[3], FALSE);
		device = sccp_device_find_byid(argv[4], FALSE);
		
		res = RESULT_SUCCESS;
	}
	else if (sccp_strcaseequals("logout", argv[2]) ) {			// move implementation to seperate logout handler
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "User Logout %s %s\n", argv[3], argv[4]);
		if (argc < 4 || sccp_strlen_zero(argv[3]) || argc > 5) {
			return RESULT_SHOWUSAGE;
		}
		user = sccp_user_find_byid(argv[3], FALSE);
		if (argc == 5 && sccp_strlen_zero(argv[4])) {
			device = sccp_device_find_byid(argv[4], FALSE);
			if (!device) {
				// error: device could not be found
				return RESULT_SHOWUSAGE;
			}
		}
		if (user) {
			RAII(sccp_usersession_t *, usersession, NULL, sccp_free);
			if (device) {
				usersession = sccp_usersession_findByDeviceUser(device, user);
			} else {
				usersession = sccp_usersession_findByUserId(user->id);
			}
			if (usersession) {
				sccp_user_handle_logout(usersession);
				if (device || (device = sccp_device_find_byid(usersession->deviceid, FALSE))) {			// restart device
					if (device->session) {
						if (device->active_channel) {
							// schedule restart
						} else {
							sccp_device_sendReset(device, SKINNY_RESETTYPE_RESTART);
						}
					}
				}
			}
		} else {
			// error user could not be found
		}
		res = RESULT_SUCCESS;
	}
	else if (sccp_strcaseequals("show", argv[2]) && argc == 4) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "User Show %s\n", argv[3]);
		if (sccp_strlen_zero(argv[3])) {
			return RESULT_SHOWUSAGE;
		}
		res = RESULT_SUCCESS;
	}
	else if (sccp_strcaseequals("sessions", argv[2]) && argc == 3) {
		sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "User Sessions\n");
/*
		sccp_usersession_t  *usersession;
#define CLI_AMI_TABLE_NAME Usersessions
#define CLI_AMI_TABLE_PER_ENTRY_NAME Session
#define CLI_AMI_TABLE_ITERATOR for(uint32_t idx = 0; idx < SCCP_VECTOR_SIZE(&usersessions); idx++)
#define CLI_AMI_TABLE_BEFORE_ITERATION usersession = SCCP_VECTOR_GET(&usersessions, idx);
#define CLI_AMI_TABLE_FIELDS 																						\
			CLI_AMI_TABLE_FIELD(User,		"32.32",	s,	32,	usersession->user->name)		\
			CLI_AMI_TABLE_FIELD(Device,		"-16.16",	s,	16,	usersession->device_id)			\
			CLI_AMI_TABLE_FIELD(SessionId,		"-16.16",	s,	16,	usersession->session_id)		\
			CLI_AMI_TABLE_FIELD(Timestamp,		"-16.16",	s,	16,	usersession->logout_timestamp)
#include "sccp_cli_table.h"
		local_table_total++;
*/	
		res = RESULT_SUCCESS;
	}
	else {
		return RESULT_SHOWUSAGE;
	}
	if (res == RESULT_FAILURE && !sccp_strlen_zero(error)) {
		CLI_AMI_RETURN_ERROR(fd, s, m, "%s\n", error);
	}
	if (s) {
		totals->lines = local_line_total;
		totals->tables = local_table_total;
	}
	return res;
}

static int sccp_user_processlogin(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");
	const char *deviceid = astman_get_header(m, "DeviceID");
	const char *sessionid = astman_get_header(m, "SessionID");
	const char *userid = astman_get_header(m, "UserID");
	const char *pincode = astman_get_header(m, "Pincode");

	//sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "%s: (processlogin) processing login (ActionID:%s, DeviceID:%s, SessionID:%lx, UserID:%s, Pincode:%s)\n", id, deviceid, sessionid, userid, pincode);

	if (sccp_strlen_zero(deviceid)) {
		pbx_log(LOG_ERROR, "(processlogin) DeviceID is required\n");
		astman_send_error(s, m, "DeviceID is required");
	}
	AUTO_RELEASE(sccp_device_t, device, sccp_device_find_byid(deviceid, FALSE));
	if (!device) {
		pbx_log(LOG_ERROR, "(processlogin) Device:%s could not be retrieved\n", deviceid);
		astman_send_error_va(s, m, "Device:%s could not be retrieved", deviceid);
		// handle error
		return -3;
	}
	RAII(sccp_usersession_t *, usersession, sccp_usersession_findByDeviceId(device->id), sccp_free);
	if (!usersession) {
		pbx_log(LOG_ERROR, "(processlogin) Session:%s could not be retrieved\n", sessionid);
		astman_send_error_va(s, m, "Session:%s could not be retrieved", sessionid);
		// handle error
		return -2;
	}
	
	// user assignment to this temp session
	usersession->user = sccp_user_find_byid(userid, FALSE);
	if (!usersession->user) {
		pbx_log(LOG_ERROR, "(processlogin) User:%s could not be retrieved\n", userid);
		astman_send_error_va(s, m, "User:%s could not be retrieved", userid);
	}

	if (!usersession_isValid(usersession, userid, pincode)) {
		pbx_log(LOG_ERROR, "(processlogin) Provided sessionid/userid/pincode could not be matched\n");
		astman_send_error(s, m, "Provided sessionid/userid/pincode could not be matched");
		sccp_user_handle_logout(usersession);
		return -1;
	}

	// update session
	usersession->state = SCCP_USERSESSION_LOGGEDIN;
	time_t current_time = time(NULL);
	usersession->timestamp = (long int)current_time;
	usersession->timeout = (long int)current_time + (SCCP_USERSESSION_TIMEOUT * 60);

	sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "%s: (processlogin) userid:%s, sessionid:%lx, new state:%s\n", device->id, usersession->user->id, usersession->sessionid, sccp_usersession_state2str(usersession->state));
	
	// update db
	char sessionstr[SESSIONSTR_LEN] = { 0 };
	usersession_createDBString(usersession, sessionstr, SESSIONSTR_LEN);
	if (!sccp_strlen_zero(sessionstr)) {
		iPbx.feature_removeFromDatabase("SCCP/emsession/deviceid", device->id);
		iPbx.feature_removeFromDatabase("SCCP/emsession/userid", usersession->user->id);
		if (iPbx.feature_addToDatabase("SCCP/emsession/userid",usersession->user->id, device->id)) {
			sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "%s: pushed to database: '%s'\n", device->id, sessionstr);
		}
		if (iPbx.feature_addToDatabase("SCCP/emsession/deviceid", device->id, sessionstr)) {
			sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "%s: pushed to database: '%s'\n", device->id, sessionstr);
		}
	}
	
	//astman_start_ack(s, m);
	astman_append(s, "Response: Success\r\n");
	if (!sccp_strlen_zero(id)) {
		astman_append(s, "ActionID: %s\r\n", id);
	}
	astman_append(s, 
		"Message: SCCPUserProgressLogin\r\n"
		"DeviceID: %s\r\n"
		"UserID: %s\r\n"
		"SessionID: %lx\r\n"
		"Status: %d\r\n"
		"StatusText: %s\r\n"
		"TimeOut: %ld\r\n"
		"\r\n",
		usersession->user->id, deviceid, usersession->sessionid, usersession->state, sccp_usersession_state2str(usersession->state), usersession->timeout 
	);

	// restart device
	if (device->session && !device->active_channel) {
		sccp_device_sendReset(device, SKINNY_RESETTYPE_RESTART);
	}
	sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "%s: (processlogin) device restarting\n", device->id);
	return 0;
}

static int sccp_user_requestsession(struct mansession *s, const struct message *m)
{
	const char *id = astman_get_header(m, "ActionID");
	const char *deviceid = astman_get_header(m, "DeviceID");

	sccp_log(DEBUGCAT_USER)(VERBOSE_PREFIX_2 "SCCP: (requestsession) request temporary session (ActionID:%s, DeviceID:%s)\n", id, deviceid);
	if (sccp_strlen_zero(deviceid)) {
		pbx_log(LOG_ERROR, "(requestsession) DeviceID is required\n");
		astman_send_error(s, m, "DeviceID is required");
		return -1;
	}
	AUTO_RELEASE(sccp_device_t, device, sccp_device_find_byid(deviceid, FALSE));
	if (!device) {
		pbx_log(LOG_ERROR, "(requestsession) Device:%s could not be retrieved\n", deviceid);
		astman_send_error_va(s, m, "Device:%s could not be retrieved", deviceid);
		// handle error
		return -2;
	}
	sccp_usersession_t *usersession = sccp_user_handle_sessionrequest(device);
	if (!usersession) {
		astman_send_error_va(s, m, "UserSession for %s could not be created", deviceid);
		return -3;
	}
	
	//astman_start_ack(s, m);
	
	astman_append(s, "Response: Success\r\n");
	if (!sccp_strlen_zero(id)) {
		astman_append(s, "ActionID: %s\r\n", id);
	}
	astman_append(s, 
		"Message: SCCPUserRequestSession\r\n"
		"DeviceID: %s\r\n"
		"prevUserID: %s\r\n" /* not yet implemented */
		"SessionID: %lx\r\n"
		"Status: %d\r\n"
		"StatusText: %s\r\n"
		"TimeOut: %ld\r\n"
		"\r\n",
		usersession->deviceid, "", usersession->sessionid, usersession->state, sccp_usersession_state2str(usersession->state), usersession->timeout
	);

	sccp_free(usersession);
	return 0;
}

static char user_processlogin_desc[] = "Description: process extension mobility user login form data\n" "\n" "Variables:\n" "  DeviceID: provide a valid deviceid\n" "  SessionID: valid sessionid required\n" "  UserID: valid userid required\n" "  Pincode: the correct user pincode should be supplied\n";
static char user_requestsession_desc[] = "Description: request an extension mobility user session\n" "\n" "Variables:\n" "  DeviceID: provide a valid deviceid\n";
static void __attribute__((constructor)) module_start(void)
{
	pbx_manager_register("SCCPUserProgressLogin", (EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG), sccp_user_processlogin, "process loginform data for extension mobility", user_processlogin_desc);
	pbx_manager_register("SCCPUserRequestSession", (EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG), sccp_user_requestsession, "request a new session (required before any further interaction)", user_requestsession_desc);
}

static void __attribute__((destructor)) module_stop(void)
{
	pbx_manager_unregister("SCCPUserProgressLogin");
	pbx_manager_unregister("SCCPUserRequestSession");
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; user-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
