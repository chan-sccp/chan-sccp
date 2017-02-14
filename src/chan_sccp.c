/*!
 * \file        chan_sccp.c
 * \brief       An implementation of Skinny Client Control Protocol (SCCP)
 * \author      Sergio Chersovani <mlists [at] c-net.it>
 * \brief       Main chan_sccp Class
 * \note        Reworked, but based on chan_sccp code.
 *              The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *              Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * \remarks
 * Purpose:     This source file should be used only for asterisk module related content.
 * When to use: Methods communicating to asterisk about module initialization, status, destruction
 * Relations:   Main hub for all other sourcefiles.
 *
 */
#include "config.h"
#include "common.h"
#include "chan_sccp.h"
#include "sccp_channel.h"
#include "sccp_config.h"
#include "sccp_device.h"
#include "sccp_globals.h"
#include "sccp_line.h"
#include "sccp_mwi.h"		// use __constructor__ to remove this entry
#include "sccp_netsock.h"
#include "sccp_session.h"	// use __constructor__ to remove this entry
#include "sccp_utils.h"
#include "sccp_hint.h"		// use __constructor__ to remove this entry
#include "sccp_conference.h"	// use __constructor__ to remove this entry
#include "revision.h"
#ifdef CS_DEVSTATE_FEATURE
#include "sccp_devstate.h"
#endif
#include "sccp_management.h"	// use __constructor__ to remove this entry
#include <signal.h>

SCCP_FILE_VERSION(__FILE__, "");

/*!
 * \brief       Global null frame
 */
static PBX_FRAME_TYPE sccp_null_frame;										/*!< Asterisk Structure */

/**
 * \brief load the configuration from sccp.conf
 * \todo should be pbx independent
 */
int load_config(void)
{
	int oldPort = 0;											//ntohs(GLOB(bindaddr));
	int newPort = 0;
	int returnvalue = FALSE;
	char addrStr[INET6_ADDRSTRLEN];

	oldPort = sccp_netsock_getPort(&GLOB(bindaddr));

	/* Setup the monitor thread default */
#if ASTERISK_VERSION_GROUP < 110
	GLOB(monitor_thread) = AST_PTHREADT_NULL;								// ADDED IN SVN 414 -FS
#endif
	GLOB(mwiMonitorThread) = AST_PTHREADT_NULL;

	memset(&GLOB(bindaddr), 0, sizeof(GLOB(bindaddr)));
	GLOB(allowAnonymous) = TRUE;

#if defined(SCCP_LITTLE_ENDIAN) && defined(SCCP_BIG_ENDIAN)
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Platform byte order   : LITTLE/BIG ENDIAN\n");
#elif defined(SCCP_LITTLE_ENDIAN)
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Platform byte order   : LITTLE ENDIAN\n");
#else
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "Platform byte order   : BIG ENDIAN\n");
#endif
	if (sccp_config_getConfig(TRUE) > CONFIG_STATUS_FILE_OK) {
		pbx_log(LOG_ERROR, "Error loading configfile !\n");
		return FALSE;
	}

	if (!sccp_config_general(SCCP_CONFIG_READINITIAL)) {
		pbx_log(LOG_ERROR, "Error parsing configfile !\n");
		return FALSE;
	}
	sccp_config_readDevicesLines(SCCP_CONFIG_READINITIAL);

	/* ok the config parse is done */
	newPort = sccp_netsock_getPort(&GLOB(bindaddr));
	if ((GLOB(descriptor) > -1) && (newPort != oldPort)) {
		close(GLOB(descriptor));
		GLOB(descriptor) = -1;
	}

	if (GLOB(descriptor) < 0) {
		int status;
		struct addrinfo hints, *res;
		char port_str[15] = "";

		memset(&hints, 0, sizeof hints);								// make sure the struct is empty
		hints.ai_family = AF_UNSPEC;									// don't care IPv4 or IPv6
		hints.ai_socktype = SOCK_STREAM;								// TCP stream sockets
		hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;					// fill in my IP for me

		if (sccp_netsock_getPort(&GLOB(bindaddr)) > 0) {
			snprintf(port_str, sizeof(port_str), "%d", sccp_netsock_getPort(&GLOB(bindaddr)));
		} else {
			snprintf(port_str, sizeof(port_str), "%s", "cisco-sccp");
		}

		sccp_copy_string(addrStr, sccp_netsock_stringify_addr(&GLOB(bindaddr)), sizeof(addrStr));

		if ((status = getaddrinfo(sccp_netsock_stringify_addr(&GLOB(bindaddr)), port_str, &hints, &res)) != 0) {
			pbx_log(LOG_ERROR, "Failed to get addressinfo for %s:%s, error: %s!\n", sccp_netsock_stringify_addr(&GLOB(bindaddr)), port_str, gai_strerror(status));
			return FALSE;
		}
		do {
			GLOB(descriptor) = socket(res->ai_family, res->ai_socktype, res->ai_protocol);			// need to add code to handle multiple interfaces (multi homed server) -> multiple socket descriptors
			if (GLOB(descriptor) < 0) {
				pbx_log(LOG_ERROR, "Unable to create SCCP socket: %s\n", strerror(errno));
				break;
			} else {
				sccp_netsock_setoptions(GLOB(descriptor), /*reuse*/ 1, /*linger*/ 1, 0 /*keepalive*/);
				/* get ip-address string */
				if (bind(GLOB(descriptor), res->ai_addr, res->ai_addrlen) < 0) {
					pbx_log(LOG_ERROR, "Failed to bind to %s:%d: %s!\n", addrStr, sccp_netsock_getPort(&GLOB(bindaddr)), strerror(errno));
					close(GLOB(descriptor));
					GLOB(descriptor) = -1;
					break;
				}
				ast_verbose(VERBOSE_PREFIX_3 "SCCP channel driver up and running on %s:%d\n", addrStr, sccp_netsock_getPort(&GLOB(bindaddr)));

				if (listen(GLOB(descriptor), DEFAULT_SCCP_BACKLOG)) {
					pbx_log(LOG_ERROR, "Failed to start listening to %s:%d: %s\n", addrStr, sccp_netsock_getPort(&GLOB(bindaddr)), strerror(errno));
					close(GLOB(descriptor));
					GLOB(descriptor) = -1;
					break;
				}
				sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP listening on %s:%d\n", addrStr, sccp_netsock_getPort(&GLOB(bindaddr)));
				GLOB(reload_in_progress) = FALSE;
				pbx_pthread_create(&GLOB(socket_thread), NULL, sccp_netsock_thread, NULL);
				returnvalue = TRUE;
			}
		} while(0);
		freeaddrinfo(res);
	}

	return returnvalue;
}

/*!
 * \brief       Load the actual chan_sccp module
 * \return      Success as int
 */
boolean_t sccp_prePBXLoad(void)
{
	pbx_log(LOG_NOTICE, "preloading pbx module\n");

	/* make globals */
	sccp_globals = sccp_calloc(sizeof *sccp_globals, 1);
	if (!sccp_globals) {
		pbx_log(LOG_ERROR, SS_Memory_Allocation_Error, "SCCP");
		return FALSE;
	}

	/* Initialize memory */
	memset(&sccp_null_frame, 0, sizeof(sccp_null_frame));
	GLOB(debug) = DEBUGCAT_CORE;

	pbx_rwlock_init(&GLOB(lock));
#ifndef SCCP_ATOMIC	
	pbx_mutex_init(&GLOB(usecnt_lock));
#endif
#if ASTERISK_VERSION_GROUP < 110
	pbx_mutex_init(&GLOB(monitor_lock));
#endif

	/* init refcount */
	sccp_refcount_init();

	SCCP_RWLIST_HEAD_INIT(&GLOB(sessions));
	SCCP_RWLIST_HEAD_INIT(&GLOB(devices));
	SCCP_RWLIST_HEAD_INIT(&GLOB(lines));

	GLOB(general_threadpool) = sccp_threadpool_init(THREADPOOL_MIN_SIZE);

	sccp_event_module_start();
#if defined(CS_DEVSTATE_FEATURE)
	sccp_devstate_module_start();
#endif
	sccp_mwi_module_start();
	sccp_hint_module_start();
	sccp_manager_module_start();
#ifdef CS_SCCP_CONFERENCE
	sccp_conference_module_start();
#endif
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_device_featureChangedDisplay, TRUE);
	sccp_event_subscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_util_featureStorageBackend, TRUE);

	GLOB(descriptor) = -1;

	GLOB(bindaddr).ss_family = AF_INET;
	((struct sockaddr_in *) &GLOB(bindaddr))->sin_port = DEFAULT_SCCP_PORT;

	GLOB(externrefresh) = 60;
	GLOB(keepalive) = SCCP_KEEPALIVE;

	/* Wait up to 16 seconds for first digit */
	GLOB(firstdigittimeout) = 16;
	/* How long to wait for following digits */
	GLOB(digittimeout) = 8;

	GLOB(debug) = 1;
	GLOB(sccp_tos) = (0x68 & 0xff);										// AF31
	GLOB(audio_tos) = (0xB8 & 0xff);									// EF
	GLOB(video_tos) = (0x88 & 0xff);									// AF41
	GLOB(sccp_cos) = 4;
	GLOB(audio_cos) = 6;
	GLOB(video_cos) = 5;
	GLOB(echocancel) = TRUE;
	GLOB(silencesuppression) = TRUE;
	GLOB(dndFeature) = TRUE;
	GLOB(autoanswer_tone) = SKINNY_TONE_ZIP;
	GLOB(remotehangup_tone) = SKINNY_TONE_ZIP;
	GLOB(callwaiting_tone) = SKINNY_TONE_CALLWAITINGTONE;
	GLOB(privacy) = TRUE;											/* permit private function */
	GLOB(mwilamp) = SKINNY_LAMP_ON;
	GLOB(ringtype) = SKINNY_RINGTYPE_OUTSIDE;
	#if 0
	GLOB(protocolversion) = SCCP_DRIVER_SUPPORTED_PROTOCOL_HIGH;
	#endif
	GLOB(amaflags) = pbx_channel_string2amaflag("documentation");
	GLOB(callanswerorder) = SCCP_ANSWER_OLDEST_FIRST;
	GLOB(socket_thread) = AST_PTHREADT_NULL;
	GLOB(earlyrtp) = SCCP_EARLYRTP_PROGRESS;
	GLOB(global_jbconf) = sccp_calloc(sizeof(struct ast_jb_conf),1);
	if (GLOB(global_jbconf)) {
		GLOB(global_jbconf)->max_size = -1;
		GLOB(global_jbconf)->resync_threshold = -1;
#ifdef CS_AST_JB_TARGETEXTRA
		GLOB(global_jbconf)->target_extra = -1;
#endif
	}

	sccp_create_hotline();
	return TRUE;
}

boolean_t sccp_postPBX_load(void)
{
	pbx_rwlock_wrlock(&GLOB(lock));

	// initialize SCCP_REVISIONSTR and SCCP_REVISIONSTR
	
#ifdef VCS_SHORT_HASH
#ifdef VCS_WC_MODIFIED
	snprintf(SCCP_REVISIONSTR, sizeof(SCCP_REVISIONSTR), "%sM", VCS_SHORT_HASH);
#else
	snprintf(SCCP_REVISIONSTR, sizeof(SCCP_REVISIONSTR), "%s", VCS_SHORT_HASH);
#endif
#else
	snprintf(SCCP_REVISIONSTR, sizeof(SCCP_REVISIONSTR), "%s", SCCP_REVISION);
#endif
	snprintf(SCCP_VERSIONSTR, sizeof(SCCP_VERSIONSTR), "Skinny Client Control Protocol (SCCP). Release: %s %s - %s (built by '%s' on '%s')\n", SCCP_VERSION, SCCP_BRANCH, SCCP_REVISIONSTR, BUILD_USER, BUILD_DATE);

	GLOB(module_running) = TRUE;
	sccp_refcount_schedule_cleanup((const void *) 0);
	pbx_rwlock_unlock(&GLOB(lock));
	return TRUE;
}

#if UNUSEDCODE // 2015-11-01
/*!
 * \brief Schedule free memory
 * \param ptr pointer
 * \return Success as int
 */
int sccp_sched_free(void *ptr)
{
	if (!ptr) {
		return -1;
	}
	sccp_free(ptr);
	return 0;

}
#endif
/*!
 * \brief PBX Independent Function to be called before unloading the module
 * \return Success as int
 */
int sccp_preUnload(void)
{
	sccp_device_t *d = NULL;
	sccp_line_t *l = NULL;

	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_1 "SCCP: Unloading Module\n");

	/* copy some of the required global variables */
	pbx_rwlock_wrlock(&GLOB(lock));
	GLOB(module_running) = FALSE;
	int descriptor = GLOB(descriptor);
	pthread_t socket_thread = GLOB(socket_thread);
	pbx_rwlock_unlock(&GLOB(lock));

	/* unsubscribe from services */
	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_device_featureChangedDisplay);
	sccp_event_unsubscribe(SCCP_EVENT_FEATURE_CHANGED, sccp_util_featureStorageBackend);

	/* close accept thread by shutdown the socket descriptor read side -> interrupt polling and break accept loop */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Closing Socket Accept Descriptor\n");
	shutdown(descriptor, SHUT_RD);
	if (socket_thread != AST_PTHREADT_NULL) {
		int s = pthread_cancel(socket_thread);
		if (s != 0) {
			pbx_log(LOG_NOTICE, "SCCP: (preUnload) pthread_cancel error\n");
		}
		void *res;
		if (pthread_join(socket_thread, &res) == 0) {
			if (res != PTHREAD_CANCELED) {
				pbx_log(LOG_ERROR, "SCCP: (session_crossdevice_cleanup) pthread join failed\n");
			}
		}
		GLOB(socket_thread) = AST_PTHREADT_NULL;
	}
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_2 "SCCP: Hangup open channels\n");				//! \todo make this pbx independend

	/* removing devices */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Removing Devices\n");
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(devices), d, list) {
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_DEVICE)) (VERBOSE_PREFIX_3 "SCCP: Removing device %s\n", d->id);
		d->realtime = TRUE;										// use realtime, to fully clear the device configuration
		sccp_dev_clean_restart(d, TRUE);								// performs a device reset if it has a session
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
	if (SCCP_RWLIST_EMPTY(&GLOB(devices))) {
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(devices));
	}

	/* hotline will be removed by line removing function */
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_3 "SCCP: Removing Lines\n");
	sccp_log((DEBUGCAT_CORE)) (VERBOSE_PREFIX_4 "SCCP: Removing Hotline\n");
	if (GLOB(hotline)) {
		if (GLOB(hotline)->line) {
			sccp_line_removeFromGlobals(GLOB(hotline)->line);
			if (GLOB(hotline)->line) {
				sccp_line_release(&GLOB(hotline)->line);					// explicit release of hotline->line
			}
		}
		sccp_free(GLOB(hotline));
	}

	/* removing lines */
	SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&GLOB(lines), l, list) {
		sccp_log((DEBUGCAT_CORE + DEBUGCAT_LINE)) (VERBOSE_PREFIX_4 "SCCP: Removing line %s\n", l->name);
		sccp_line_clean(l, TRUE);
	}
	SCCP_RWLIST_TRAVERSE_SAFE_END;
	if (SCCP_RWLIST_EMPTY(&GLOB(lines))) {
		SCCP_RWLIST_HEAD_DESTROY(&GLOB(lines));
	}
	usleep(100);												// wait for events to finalize

	/* stop services */
	sccp_session_terminateAll();
	sccp_manager_module_stop();
#ifdef CS_DEVSTATE_FEATURE	
	sccp_devstate_module_stop();
#endif
#ifdef CS_SCCP_CONFERENCE
	sccp_conference_module_stop();
#endif
	sccp_softkey_clear();
	sccp_hint_module_stop();
	sccp_event_module_stop();
	sccp_threadpool_destroy(GLOB(general_threadpool));
	sccp_refcount_destroy();

	/* free resources */
	if (GLOB(config_file_name)) {
		sccp_free(GLOB(config_file_name));
	}
	if (GLOB(global_jbconf)) {
		sccp_free(GLOB(global_jbconf));
	}
	if (GLOB(ha)) {
		sccp_free_ha(GLOB(ha));
	}
	if (GLOB(localaddr)) {
		sccp_free_ha(GLOB(localaddr));
	}
	sccp_config_cleanup_dynamically_allocated_memory(sccp_globals, SCCP_CONFIG_GLOBAL_SEGMENT);
	/* */

	/* destroy locks */
#ifndef SCCP_ATOMIC
	pbx_mutex_destroy(&GLOB(usecnt_lock));
#endif	
	pbx_rwlock_destroy(&GLOB(lock));
	return 0;
}

/*!
 * \brief PBX Independent Function to be called when starting module reload
 * \return Success as int
 */
int sccp_reload(void)
{
	sccp_readingtype_t readingtype;
	int returnval = 0;

	pbx_rwlock_wrlock(&GLOB(lock));
	if (GLOB(reload_in_progress) == TRUE) {
		pbx_log(LOG_ERROR, "SCCP reloading already in progress.\n");
		returnval = 1;
		goto EXIT;
	}

	sccp_config_file_status_t cfg = sccp_config_getConfig(FALSE);

	switch (cfg) {
		case CONFIG_STATUS_FILE_NOT_CHANGED:
			pbx_log(LOG_NOTICE, "config file '%s' has not change, skipping reload.\n", GLOB(config_file_name));
			returnval = 0;
			break;
		case CONFIG_STATUS_FILE_OK:
			pbx_log(LOG_NOTICE, "SCCP reloading configuration.\n");
			readingtype = SCCP_CONFIG_READRELOAD;
			GLOB(reload_in_progress) = TRUE;
			if (!sccp_config_general(readingtype)) {
				pbx_log(LOG_ERROR, "Unable to reload configuration.\n");
				returnval = 2;
				break;
			}
			sccp_config_readDevicesLines(readingtype);
			returnval = 3;
			break;
		case CONFIG_STATUS_FILE_OLD:
			pbx_log(LOG_ERROR, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_log(LOG_ERROR, "\n\n --> You are using an old configuration format, please update '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			returnval = 4;
			break;
		case CONFIG_STATUS_FILE_NOT_SCCP:
			pbx_log(LOG_ERROR, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_log(LOG_ERROR, "\n\n --> You are using an configuration file is not following the sccp format, please check '%s'!!\n --> Loading of module chan_sccp with current sccp.conf has terminated\n --> Check http://chan-sccp-b.sourceforge.net/doc_setup.shtml for more information.\n\n", GLOB(config_file_name));
			returnval = 4;
			break;
		case CONFIG_STATUS_FILE_NOT_FOUND:
			pbx_log(LOG_ERROR, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_log(LOG_ERROR, "Config file '%s' not found, aborting reload.\n", GLOB(config_file_name));
			returnval = 4;
			break;
		case CONFIG_STATUS_FILE_INVALID:
			pbx_log(LOG_ERROR, "Error reloading from '%s'\n", GLOB(config_file_name));
			pbx_log(LOG_ERROR, "Config file '%s' specified is not a valid config file, aborting reload.\n", GLOB(config_file_name));
			returnval = 4;
			break;
	}
EXIT:
	GLOB(reload_in_progress) = FALSE;
	pbx_rwlock_unlock(&GLOB(lock));
	return returnval;
}

#ifdef CS_DEVSTATE_FEATURE
const char devstate_db_family[] = "CustomDevstate";
#endif

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
