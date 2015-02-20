/*!
 * \file        sccp_refcount.c
 * \brief       SCCP Refcount Class
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *              See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$  
 */

/*!
 * \section sccp_refcount   Reference Counted Objects
 * 
 * We started using refcounting in V4.0 to prevent possible deadlock situations where the would not even have to occur. Up till now we had been using lock to prevent objects 
 * from vanishing instead of preventing modification. As a rule there will be at most be one thread modifying a device, line, channel object in such a significant manner 
 * the session thread the device and associated line/channel belongs too). 
 *
 * In this regard locking was not necessary and could be replaced by a method to prevent vanishing objects and preventing dereferencing null pointers, leading to segmentation faults.
 * To solve that we opted to implement reference counting. Reference counting has a number of rules that need to be followed at any and all times for it to work:
 *
 * - Rule 1: During the initial creation / allocation of a refcounted object the refcount is set to 1. Example:
 *   \code
 *   channel = (sccp_channel_t *) sccp_refcount_object_alloc(sizeof(sccp_channel_t), "channel", c->id, __sccp_channel_destroy);
 *   \endcode
 *
 * - Rule 2: Functions that <b><em>return an object</em></b> (e.g. sccp_device, sccp_line, sccp_channel, sccp_event, sccp_linedevice), need to do so <b><em>with</em></b> a retained objects. 
 *   This happens when a object is created and returned to a calling function for example.
 *
 * - Rule 3: Functions that <b><em>receive an object pointer reference</em></b> via a function call expect the object <b><em>is being retained</em></b> in the calling function, during the time the called function lasts. 
 *   The object can <b><em>only</em></b> be released by the calling function not the called function,
 *
 * - Rule 4: When releasing an object the pointer we had toward the object should be nullified immediatly, either of these solutions is possible:
 *   \code
 *   d = sccp_device_release(d);                // sccp_release always returns NULL
 *   \endcode
 *   or 
 *   \code
 *   sccp_device_release(d);
 *   d = NULL;
 *   \endcode
 *   
 * - Rule 5:    You <b><em>cannnot</em></b> use free on a refcounted object. Destruction of the refcounted object and subsequent freeing of the occupied memory is performed by the sccp_release 
 *              when the number of reference reaches 0. To finalize the use of a refcounted object just release the object one final time, to negate the initial refcount of 1 during creation.
 * .
 * These rules need to followed to the letter !
 */

#include <config.h>
#include "common.h"
#include "sccp_channel.h"
#include "sccp_device.h"
#include "sccp_line.h"
#include "sccp_utils.h"
#include "sccp_config.h"
#include "sccp_actions.h"
#include "sccp_features.h"
#include "sccp_socket.h"
#include "sccp_indicate.h"
#include "sccp_mwi.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$");

//nb: SCCP_HASH_PRIME defined in config.h, default 563
#define SCCP_SIMPLE_HASH(_a) (((unsigned long)(_a)) % SCCP_HASH_PRIME)
#define SCCP_LIVE_MARKER 13
#define REF_FILE "/tmp/sccp_refs"
static enum sccp_refcount_runstate runState = SCCP_REF_STOPPED;

static struct sccp_refcount_obj_info {
	int (*destructor) (const void *ptr);
	char datatype[StationMaxDeviceNameSize];
	sccp_debug_category_t debugcat;
} obj_info[] = {
/* *INDENT-OFF* */
	[SCCP_REF_DEVICE] = {NULL, "device", DEBUGCAT_DEVICE},
	[SCCP_REF_LINE] = {NULL, "line", DEBUGCAT_LINE},
	[SCCP_REF_CHANNEL] = {NULL, "channel", DEBUGCAT_CHANNEL},
	[SCCP_REF_LINEDEVICE] = {NULL, "linedevice", DEBUGCAT_LINE},
	[SCCP_REF_EVENT] = {NULL, "event", DEBUGCAT_EVENT},
	[SCCP_REF_TEST] = {NULL, "test", DEBUGCAT_HIGH},
	[SCCP_REF_CONFERENCE] = {NULL, "conference", DEBUGCAT_CONFERENCE},
	[SCCP_REF_PARTICIPANT] = {NULL, "participant", DEBUGCAT_CONFERENCE},
/* *INDENT-ON* */
};

#ifdef SCCP_ATOMIC
#define obj_lock NULL
#else
#define	obj_lock &obj->lock
#endif

struct refcount_object {
#ifndef SCCP_ATOMIC
	ast_mutex_t lock;
#endif
	volatile CAS32_TYPE refcount;
	enum sccp_refcounted_types type;
	char identifier[REFCOUNT_INDENTIFIER_SIZE];
	int alive;
	size_t len;
	SCCP_RWLIST_ENTRY (RefCountedObject) list;
	unsigned char data[0];
};

static ast_rwlock_t objectslock;										// general lock to modify hash table entries
static struct refcount_objentry {
	SCCP_RWLIST_HEAD (, RefCountedObject) refCountedObjects;						//!< one rwlock per hash table entry, used to modify list
} *objects[SCCP_HASH_PRIME];											//!< objects hash table

#if CS_REFCOUNT_DEBUG
static FILE *sccp_ref_debug_log;
#endif

void sccp_refcount_init(void)
{
	sccp_log((DEBUGCAT_REFCOUNT + DEBUGCAT_HIGH)) (VERBOSE_PREFIX_1 "SCCP: (Refcount) init\n");
	pbx_rwlock_init_notracking(&objectslock);								// No tracking to safe cpu cycles
#if CS_REFCOUNT_DEBUG
	sccp_ref_debug_log = fopen(REF_FILE, "w");
	if (!sccp_ref_debug_log) {
		pbx_log(LOG_NOTICE, "SCCP: Failed to open ref debug log file '%s'\n", REF_FILE);
	}
#endif
	runState = SCCP_REF_RUNNING;
}

void sccp_refcount_destroy(void)
{
	int x;
	RefCountedObject *obj;

	pbx_log(LOG_NOTICE, "SCCP: (Refcount) destroying...\n");
	runState = SCCP_REF_STOPPED;

	sched_yield();												//make sure all other threads can finish their work first.

	// cleanup if necessary, if everything is well, this should not be necessary
	ast_rwlock_wrlock(&objectslock);
	for (x = 0; x < SCCP_HASH_PRIME; x++) {
		if (objects[x]) {
			SCCP_RWLIST_WRLOCK(&(objects[x])->refCountedObjects);
			while ((obj = SCCP_RWLIST_REMOVE_HEAD(&(objects[x])->refCountedObjects, list))) {
				pbx_log(LOG_NOTICE, "Cleaning up [%3d]=type:%17s, id:%25s, ptr:%15p, refcount:%4d, alive:%4s, size:%4d\n", x, (obj_info[obj->type]).datatype, obj->identifier, obj, (int) obj->refcount, SCCP_LIVE_MARKER == obj->alive ? "yes" : "no", (int) obj->len);
				if ((&obj_info[obj->type])->destructor) {
					(&obj_info[obj->type])->destructor(obj->data);
				}
#ifndef SCCP_ATOMIC
				ast_mutex_destroy(&obj->lock);
#endif
				memset(obj, 0, sizeof(RefCountedObject));
				sccp_free(obj);
				obj = NULL;
			}
			SCCP_RWLIST_UNLOCK(&(objects[x])->refCountedObjects);
			SCCP_RWLIST_HEAD_DESTROY(&(objects[x])->refCountedObjects);

			sccp_free(objects[x]);									// free hashtable entry
			objects[x] = NULL;
		}
	}
	ast_rwlock_unlock(&objectslock);
	pbx_rwlock_destroy(&objectslock);
#if CS_REFCOUNT_DEBUG
	if (sccp_ref_debug_log) {
		fclose(sccp_ref_debug_log);
		sccp_ref_debug_log = NULL;
		pbx_log(LOG_NOTICE, "SCCP: ref debug log file: %s closed\n", REF_FILE);
	}
#endif
	runState = SCCP_REF_DESTROYED;
}

int sccp_refcount_isRunning(void)
{
	return runState;
}

// not really needed any more
int sccp_refcount_schedule_cleanup(const void *data)
{
	return 0;
}

void *sccp_refcount_object_alloc(size_t size, enum sccp_refcounted_types type, const char *identifier, void *destructor)
{
	RefCountedObject *obj;
	void *ptr = NULL;
	int hash;

	if (!runState) {
		ast_log(LOG_ERROR, "SCCP: (sccp_refcount_object_alloc) Not Running Yet!\n");
		return NULL;
	}

	if (!(obj = sccp_calloc(1, size + sizeof(*obj)))) {
		ast_log(LOG_ERROR, "SCCP: (sccp_refcount_object_alloc) Memory allocation failure (obj)");
		return NULL;
	}

	if (!(&obj_info[type])->destructor) {
		(&obj_info[type])->destructor = destructor;
	}
	// initialize object    
	obj->len = size;
	obj->type = type;
	obj->refcount = 1;
#ifndef SCCP_ATOMIC
	ast_mutex_init(&obj->lock);
#endif
	sccp_copy_string(obj->identifier, identifier, sizeof(obj->identifier));

	// generate hash
	ptr = obj->data;
	hash = SCCP_SIMPLE_HASH(ptr);

	if (!objects[hash]) {
		// create new hashtable head when necessary (should this possibly be moved to refcount_init, to avoid raceconditions ?)
		ast_rwlock_wrlock(&objectslock);
		if (!objects[hash]) {										// check again after getting the lock, to see if another thread did not create the head already
			if (!(objects[hash] = sccp_malloc(sizeof(struct refcount_objentry)))) {
				ast_log(LOG_ERROR, "SCCP: (sccp_refcount_object_alloc) Memory allocation failure (hashtable)");
				sccp_free(obj);
				obj = NULL;
				ast_rwlock_unlock(&objectslock);
				return NULL;
			}
			SCCP_RWLIST_HEAD_INIT(&(objects[hash])->refCountedObjects);
		}
		ast_rwlock_unlock(&objectslock);
	}
	// add object to hash table
	SCCP_RWLIST_WRLOCK(&(objects[hash])->refCountedObjects);
	SCCP_RWLIST_INSERT_HEAD(&(objects[hash])->refCountedObjects, obj, list);
	SCCP_RWLIST_UNLOCK(&(objects[hash])->refCountedObjects);

	sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: (alloc_obj) Creating new %s %s (%p) inside %p at hash: %d\n", (&obj_info[obj->type])->datatype, identifier, ptr, obj, hash);
	obj->alive = SCCP_LIVE_MARKER;

#if CS_REFCOUNT_DEBUG
	if (sccp_ref_debug_log) {
		fprintf(sccp_ref_debug_log, "%p,+1,%d,%s,%d,%s,**constructor**,%s:%s\n", ptr, ast_get_tid(), __FILE__, __LINE__, __PRETTY_FUNCTION__, (&obj_info[obj->type])->datatype, obj->identifier);
		fflush(sccp_ref_debug_log);
	}
#endif
	memset(ptr, 0, size);
	return ptr;
}

#if CS_REFCOUNT_DEBUG
int __sccp_refcount_debug(void *ptr, RefCountedObject * obj, int delta, const char *file, int line, const char *func)
{
	if (!sccp_ref_debug_log) {
		return -1;
	}

	if (ptr == NULL) {
		fprintf(sccp_ref_debug_log, "%p **PTR IS NULL !!** %s:%d:%s\n", ptr, file, line, func);
		// ast_assert(0);
		fflush(sccp_ref_debug_log);
		return -1;
	}
	if (obj == NULL) {
		fprintf(sccp_ref_debug_log, "%p **OBJ ALREADY DESTROYED !!** %s:%d:%s\n", ptr, file, line, func);
		// ast_assert(0);
		fflush(sccp_ref_debug_log);
		return -1;
	}

	if (delta == 0 && obj->alive != SCCP_LIVE_MARKER) {
		fprintf(sccp_ref_debug_log, "%p **OBJ Already destroyed and Declared DEAD !!** %s:%d:%s (%s:%s) [@%d] [%p]\n", ptr, file, line, func, (&obj_info[obj->type])->datatype, obj->identifier, obj->refcount, ptr);
		// ast_assert(0);
		fflush(sccp_ref_debug_log);
		return -1;
	}

	if (delta != 0) {
		fprintf(sccp_ref_debug_log, "%p,%s%d,%d,%s,%d,%s,%d,%s:%s\n", ptr, (delta < 0 ? "" : "+"), delta, ast_get_tid(), file, line, func, obj->refcount, (&obj_info[obj->type])->datatype, obj->identifier);
	}
	if (obj->refcount + delta == 0 && (&obj_info[obj->type])->destructor != NULL) {
		fprintf(sccp_ref_debug_log, "%p,%d,%d,%s,%d,%s,**destructor**,%s:%s\n", ptr, delta, ast_get_tid(), file, line, func, (&obj_info[obj->type])->datatype, obj->identifier);
	}
	fflush(sccp_ref_debug_log);
	return 0;
}
#endif

static gcc_inline RefCountedObject *sccp_refcount_find_obj(const void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj = NULL;
	boolean_t found = FALSE;

	if (ptr == NULL) {
		return NULL;
	}

	int hash = SCCP_SIMPLE_HASH(ptr);

	if (objects[hash]) {
		SCCP_RWLIST_RDLOCK(&(objects[hash])->refCountedObjects);
		SCCP_RWLIST_TRAVERSE(&(objects[hash])->refCountedObjects, obj, list) {
			if (obj->data == ptr) {
				if (SCCP_LIVE_MARKER == obj->alive) {
					found = TRUE;
				} else {
#if CS_REFCOUNT_DEBUG
					__sccp_refcount_debug((void *) ptr, obj, 0, filename, lineno, func);
#endif
					sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: (sccp_refcount_find_obj) %p Already declared dead (hash: %d)\n", obj, hash);
				}
				break;
			}
		}
		SCCP_RWLIST_UNLOCK(&(objects[hash])->refCountedObjects);
	}
	return found ? obj : NULL;
}

static gcc_inline void sccp_refcount_remove_obj(const void *ptr)
{
	RefCountedObject *obj = NULL;

	if (ptr == NULL) {
		return;
	}

	int hash = SCCP_SIMPLE_HASH(ptr);

	sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: (sccp_refcount_remove_obj) Removing %p from hash table at hash: %d\n", ptr, hash);

	if (objects[hash]) {
		SCCP_RWLIST_WRLOCK(&(objects[hash])->refCountedObjects);
		SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&(objects[hash])->refCountedObjects, obj, list) {
			if (obj->data == ptr && SCCP_LIVE_MARKER != obj->alive) {
				SCCP_RWLIST_REMOVE_CURRENT(list);
				break;
			}
		}
		SCCP_RWLIST_TRAVERSE_SAFE_END;
		SCCP_RWLIST_UNLOCK(&(objects[hash])->refCountedObjects);
	}
	if (obj) {
		sched_yield();											// make sure all other threads can finish their work first.
		// should resolve lockless refcount SMP issues
		// BTW we are not allowed to sleep whilst haveing a reference
		// fire destructor
		if (obj && obj->data == ptr && SCCP_LIVE_MARKER != obj->alive) {
			sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: (sccp_refcount_remove_obj) Destroying %p at hash: %d\n", obj, hash);
			if ((&obj_info[obj->type])->destructor) {
				(&obj_info[obj->type])->destructor(ptr);
			}
			memset(obj, 0, sizeof(RefCountedObject));
			sccp_free(obj);
			obj = NULL;
		}
	}
}

#include <asterisk/cli.h>
void sccp_refcount_print_hashtable(int fd)
{
	int x, prev = 0;
	RefCountedObject *obj = NULL;
	unsigned int maxdepth = 0;
	unsigned int numentries = 0;
	float fillfactor = 0.00;

	pbx_cli(fd, "+==============================================================================================+\n");
	pbx_cli(fd, "| %5s | %17s | %25s | %15s | %4s | %4s | %4s |\n", "hash", "type", "id", "ptr", "refc", "live", "size");
	pbx_cli(fd, "|==============================================================================================|\n");
	ast_rwlock_rdlock(&objectslock);
	for (x = 0; x < SCCP_HASH_PRIME; x++) {
		if (objects[x]) {
			SCCP_RWLIST_RDLOCK(&(objects[x])->refCountedObjects);
			SCCP_RWLIST_TRAVERSE(&(objects[x])->refCountedObjects, obj, list) {
				if (prev == x) {
					pbx_cli(fd, "|  +->  ");
				} else {
					pbx_cli(fd, "| [%3d] ", x);
				}
				pbx_cli(fd, "| %17s | %25s | %15p | %4d | %4s | %4d |\n", (obj_info[obj->type]).datatype, obj->identifier, obj, (int) obj->refcount, SCCP_LIVE_MARKER == obj->alive ? "yes" : "no", (int) obj->len);
				prev = x;
				numentries++;
			}
			if (maxdepth < SCCP_RWLIST_GETSIZE(&(objects[x])->refCountedObjects)) {
				maxdepth = SCCP_RWLIST_GETSIZE(&(objects[x])->refCountedObjects);
			}
			SCCP_RWLIST_UNLOCK(&(objects[x])->refCountedObjects);
		}
	}
	ast_rwlock_unlock(&objectslock);
	fillfactor = (float) numentries / SCCP_HASH_PRIME;
	pbx_cli(fd, "+==============================================================================================+\n");
	pbx_cli(fd, "| fillfactor = (%03d / %03d) = %02.2f, maxdepth = %02d                                               |\n", numentries, SCCP_HASH_PRIME, fillfactor, maxdepth);
	if (fillfactor > 1.00) {
		pbx_cli(fd, "| \033[1m\033[41m\033[37mPlease keep fillfactor below 1.00. Check ./configure --with-hash-size.\033[0m                       |\n");
	}
	pbx_cli(fd, "+==============================================================================================+\n");
}

#ifdef CS_EXPERIMENTAL
int sccp_refcount_force_release(long findobj, char *identifier)
{
	int x = 0;
	RefCountedObject *obj = NULL;
	void *ptr = NULL;

	ast_rwlock_rdlock(&objectslock);
	for (x = 0; x < SCCP_HASH_PRIME; x++) {
		if (objects[x]) {
			SCCP_RWLIST_RDLOCK(&(objects[x])->refCountedObjects);
			SCCP_RWLIST_TRAVERSE(&(objects[x])->refCountedObjects, obj, list) {
				if (sccp_strequals(obj->identifier, identifier) && (long) obj == findobj) {
					ptr = obj->data;
				}
			}
			SCCP_RWLIST_UNLOCK(&(objects[x])->refCountedObjects);
		}
	}
	ast_rwlock_unlock(&objectslock);
	if (ptr) {
		sccp_log(DEBUGCAT_CORE) (VERBOSE_PREFIX_1 "Forcefully releasing one instance of %s\n", identifier);
		sccp_refcount_release(ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
		return 1;
	}
	return 0;
}
#endif

void sccp_refcount_updateIdentifier(void *ptr, char *identifier)
{
	RefCountedObject *obj = NULL;

	if ((obj = sccp_refcount_find_obj(ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
		sccp_copy_string(obj->identifier, identifier, sizeof(obj->identifier));
	} else {
		ast_log(LOG_ERROR, "SCCP: (updateIdentifief) Refcount Object %p could not be found\n", ptr);
	}
}

gcc_inline void *sccp_refcount_retain(void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj = NULL;
	volatile int refcountval;
	int newrefcountval;

	if ((obj = sccp_refcount_find_obj(ptr, filename, lineno, func))) {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug(ptr, obj, 1, filename, lineno, func);
#endif
		refcountval = ATOMIC_INCR((&obj->refcount), 1, &obj->lock);
		newrefcountval = refcountval + 1;

		if ((sccp_globals->debug & (((&obj_info[obj->type])->debugcat + DEBUGCAT_REFCOUNT))) == ((&obj_info[obj->type])->debugcat + DEBUGCAT_REFCOUNT)) {
			ast_log(__LOG_VERBOSE, __FILE__, 0, "", " %-15.15s:%-4.4d (%-25.25s) %*.*s> %*s refcount increased %.2d  +> %.2d for %10s: %s (%p)\n", filename, lineno, func, refcountval, refcountval, "--------------------", 20 - refcountval, " ", refcountval, newrefcountval, (&obj_info[obj->type])->datatype, obj->identifier, obj);
		}
		return obj->data;
	} else {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug((void *) ptr, NULL, 1, filename, lineno, func);
#endif
		ast_log(__LOG_VERBOSE, __FILE__, 0, "retain", "SCCP: (%-15.15s:%-4.4d (%-25.25s)) ALARM !! trying to retain a %s: %s (%p) with invalid memory reference! this should never happen !\n", filename, lineno, func, (obj) ? (&obj_info[obj->type])->datatype : "UNREF", (obj) ? obj->identifier : "UNREF", obj);
		ast_log(LOG_ERROR, "SCCP: (release) Refcount Object %p could not be found (Major Logic Error). Please report to developers\n", ptr);
		return NULL;
	}
}

gcc_inline void *sccp_refcount_release(const void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj = NULL;
	volatile int refcountval;
	int newrefcountval, alive;
	sccp_debug_category_t debugcat;

	if ((obj = sccp_refcount_find_obj(ptr, filename, lineno, func))) {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug((void *) ptr, obj, -1, filename, lineno, func);
#endif
		debugcat = (&obj_info[obj->type])->debugcat;

		refcountval = ATOMIC_DECR((&obj->refcount), 1, &obj->lock);
		newrefcountval = refcountval - 1;
		if (newrefcountval == 0) {
			alive = ATOMIC_DECR(&obj->alive, SCCP_LIVE_MARKER, &obj->lock);
			sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_1 "SCCP: %-15.15s:%-4.4d (%-25.25s)) (release) Finalizing %p (%p) (alive:%d)\n", filename, lineno, func, obj, ptr, alive);
			sccp_refcount_remove_obj(ptr);
		} else {
			if ((sccp_globals->debug & ((debugcat + DEBUGCAT_REFCOUNT))) == (debugcat ^ DEBUGCAT_REFCOUNT)) {
				ast_log(__LOG_VERBOSE, __FILE__, 0, "", " %-15.15s:%-4.4d (%-25.25s) <%*.*s %*s refcount decreased %.2d  <- %.2d for %s (%p)\n", filename, lineno, func, newrefcountval, newrefcountval, "--------------------", 20 - newrefcountval, " ", newrefcountval, refcountval, obj->identifier, obj);
			}
		}
	} else {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug((void *) ptr, NULL, -1, filename, lineno, func);
#endif
		ast_log(__LOG_VERBOSE, __FILE__, 0, "release", "SCCP (%-15.15s:%-4.4d (%-25.25s)) ALARM !! trying to release a %s (%p) with invalid memory reference! this should never happen !\n", filename, lineno, func, (obj) ? obj->identifier : "UNREF", obj);
		ast_log(LOG_ERROR, "SCCP: (release) Refcount Object %p could not be found (Major Logic Error). Please report to developers\n", ptr);
	}
	return NULL;
}

gcc_inline void sccp_refcount_replace(void **replaceptr, void *newptr, const char *filename, int lineno, const char *func)
{
	if ((!replaceptr && !newptr) || (&newptr == replaceptr)) {						// nothing changed
		return;
	}

	void *tmpNewPtr = NULL;											// retain new one first
	void *oldPtr = *replaceptr;

	if (newptr) {
		if ((tmpNewPtr = sccp_refcount_retain(newptr, filename, lineno, func))) {
			*replaceptr = tmpNewPtr;
			if (oldPtr) {										// release previous one after
				sccp_refcount_release(oldPtr, filename, lineno, func);
			}
		}
	} else {
		if (oldPtr) {											// release previous one after
			*replaceptr = sccp_refcount_release(oldPtr, filename, lineno, func);
		}
	}
}

/*
 * \brief refence autorelease helper
 * Used together with the cleanup attribute, to handle the automatic reference release of an object when we leave the scope in which the 
 * reference was defined. 
 */
void sccp_refcount_autorelease(void *ptr)
{
	if (*(void **) ptr) {
		sccp_refcount_release(*(void **) ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

// kate: indent-width 8; replace-tabs off; indent-mode cstyle; auto-insert-doxygen on; line-numbers on; tab-indents on; keep-extra-spaces off; auto-brackets off;
