
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
 * - Rule 2: Function that <b><em>return an object</em></b> (e.g. sccp_device, sccp_line, sccp_channel, sccp_event, sccp_linedevice), need to do so <b><em>with</em></b> a retained objects. 
 *   This happens when a object is created and returned to a calling function for example.
 *
 * - Rule 3: Functins that <b><em>receive an object pointer reference</em></b> via a function call expect the object <b><em>is being retained</em></b> in the calling function, during the time the called function lasts. 
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
 * - Rule 5: You <b><em>cannnot</em></b> use free a refcounted object. Destruction of the refcounted object and subsequent freeing of the occupied memory is performed by the sccp_release 
 *           when the number of reference reaches 0. To finalize the use of a refcounted object just release the object one final time, to negate the initial refcount of 1 during creation.
 * .
 * These rules need to followed to the letter !
 */

#include <config.h>
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")
#define SCCP_HASH_PRIME 563
#define SCCP_SIMPLE_HASH(_a) (((unsigned long)(_a)) % SCCP_HASH_PRIME)
#define SCCP_LIVE_MARKER 13
#define REF_FILE "/tmp/sccp_refs"
enum sccp_refcount_runstate runState = SCCP_REF_STOPPED;

struct sccp_refcount_obj_info {
	int (*destructor) (const void *ptr);
	char datatype[StationMaxDeviceNameSize];
	sccp_debug_category_t debugcat;
} obj_info[] =
{
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
#define        	obj_lock	NULL
#else
#define		obj_lock	&obj->lock
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

ast_rwlock_t objectslock;											// general lock to modify hash table entries
static struct refcount_objentry {
	SCCP_RWLIST_HEAD (, RefCountedObject) refCountedObjects;						//!< one rwlock per hash table entry, used to modify list
} *objects[SCCP_HASH_PRIME];											//!< objects hash table

void sccp_refcount_init(void)
{
	sccp_log((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) init\n");
	pbx_rwlock_init_notracking(&objectslock);								// No tracking to safe cpu cycles
	runState = SCCP_REF_RUNNING;
}

void sccp_refcount_destroy(void)
{
	int x;
	RefCountedObject *obj;

	pbx_log(LOG_NOTICE, "SCCP: (Refcount) destroying...\n");
	runState = SCCP_REF_STOPPED;

	sched_yield();				//make sure all other threads can finish their work first.
	
	// cleanup if necessary, if everything is well, this should not be necessary
	ast_rwlock_wrlock(&objectslock);
	for (x = 0; x < SCCP_HASH_PRIME; x++) {
		if (objects[x]) {
			SCCP_RWLIST_WRLOCK(&(objects[x])->refCountedObjects);
			while ((obj = SCCP_RWLIST_REMOVE_HEAD(&(objects[x])->refCountedObjects, list))) {
				pbx_log(LOG_NOTICE, "Cleaning up [%3d]=type:%17s, id:%25s, ptr:%15p, refcount:%4d, alive:%4s, size:%4d\n", x, (obj_info[obj->type]).datatype, obj->identifier, obj, (int) obj->refcount, SCCP_LIVE_MARKER == obj->alive ? "yes" : "no", (int) obj->len);
				if ((&obj_info[obj->type])->destructor)
					(&obj_info[obj->type])->destructor(obj->data);
#ifndef SCCP_ATOMIC
				ast_mutex_destroy(&obj->lock);
#endif
				memset(obj, 0, sizeof(RefCountedObject));
				sccp_free(obj);
				obj = NULL;
			}
			SCCP_RWLIST_UNLOCK(&(objects[x])->refCountedObjects);
			SCCP_RWLIST_HEAD_DESTROY(&(objects[x])->refCountedObjects);
		}
	}
	ast_rwlock_unlock(&objectslock);
	pbx_rwlock_destroy(&objectslock);
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

	if (!(obj = calloc(1, size + sizeof(*obj)))) {
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
			if (!(objects[hash] = malloc(sizeof(struct refcount_objentry)))) {
				ast_log(LOG_ERROR, "SCCP: (sccp_refcount_object_alloc) Memory allocation failure (hashtable)");
				free(obj);
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

	sccp_log(DEBUGCAT_REFCOUNT) (VERBOSE_PREFIX_1 "SCCP: (alloc_obj) Creating new %s %s (%p) inside %p at hash: %d\n", (&obj_info[obj->type])->datatype, identifier, ptr, obj, hash);
	obj->alive = SCCP_LIVE_MARKER;

#if CS_REFCOUNT_DEBUG
	FILE *refo = fopen(REF_FILE, "a");

	if (refo) {
		fprintf(refo, "%p =1   %s:%d:%s (allocate new %s, len: %d, destructor: %p) [%p]\n", obj, __FILE__, __LINE__, __PRETTY_FUNCTION__, (&obj_info[obj->type])->datatype, (int) obj->len, (&obj_info[type])->destructor, ptr);
		fclose(refo);
	}
#endif

	memset(ptr, 0, size);
	return ptr;
}

#if CS_REFCOUNT_DEBUG
int __sccp_refcount_debug(void *ptr, RefCountedObject * obj, int delta, const char *file, int line, const char *func)
{
	if (ptr == NULL) {
		FILE *refo = fopen(REF_FILE, "a");

		if (refo) {
			fprintf(refo, "%p **PTR IS NULL !!** %s:%d:%s\n", ptr, file, line, func);
			fclose(refo);
		}
		//                ast_assert(0);
		return -1;
	}
	if (obj == NULL) {
		FILE *refo = fopen(REF_FILE, "a");

		if (refo) {
			fprintf(refo, "%p **OBJ ALREADY DESTROYED !!** %s:%d:%s\n", ptr, file, line, func);
			fclose(refo);
		}
		//                ast_assert(0);
		return -1;
	}

	if (delta == 0 && obj->alive != SCCP_LIVE_MARKER) {
		FILE *refo = fopen(REF_FILE, "a");

		if (refo) {
			fprintf(refo, "%p **OBJ Already destroyed and Declared DEAD !!** %s:%d:%s (%s:%s) [@%d] [%p]\n", ptr, file, line, func, (&obj_info[obj->type])->datatype, obj->identifier, obj->refcount, ptr);
			fclose(refo);
		}
		//                ast_assert(0);
		return -1;
	}

	if (delta != 0) {
		FILE *refo = fopen(REF_FILE, "a");

		if (refo) {
			fprintf(refo, "%p %s%d   %s:%d:%s (%s:%s) [@%d] [%p]\n", obj, (delta < 0 ? "" : "+"), delta, file, line, func, (&obj_info[obj->type])->datatype, obj->identifier, obj->refcount, ptr);
			fclose(refo);
		}
	}
	if (obj->refcount + delta == 0 && (&obj_info[obj->type])->destructor != NULL) {
		FILE *refo = fopen(REF_FILE, "a");

		if (refo) {
			fprintf(refo, "%p **call destructor** %s:%d:%s (%s:%s)\n", ptr, file, line, func, (&obj_info[obj->type])->datatype, obj->identifier);
			fclose(refo);
		}
	}
	return 0;
}
#endif

static inline RefCountedObject *find_obj(const void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj = NULL;
	boolean_t found = FALSE;

	if (!ptr)
		return NULL;

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
					sccp_log(DEBUGCAT_REFCOUNT) (VERBOSE_PREFIX_1 "SCCP: (find_obj) %p Already declared dead (hash: %d)\n", obj, hash);
				}
				break;
			}
		}
		SCCP_RWLIST_UNLOCK(&(objects[hash])->refCountedObjects);
	}
	return found ? obj : NULL;
}

static inline void remove_obj(const void *ptr)
{
	RefCountedObject *obj = NULL;

	if (!ptr)
		return;

	int hash = SCCP_SIMPLE_HASH(ptr);

	sccp_log(DEBUGCAT_REFCOUNT) (VERBOSE_PREFIX_1 "SCCP: (release) Removing %p from hash table at hash: %d\n", ptr, hash);

	if (objects[hash]) {
		SCCP_RWLIST_WRLOCK(&(objects[hash])->refCountedObjects);
		SCCP_RWLIST_TRAVERSE_SAFE_BEGIN(&(objects[hash])->refCountedObjects, obj, list) {
			if (obj->data == ptr) {
				SCCP_RWLIST_REMOVE_CURRENT(list);
				break;
			}
		}
		SCCP_RWLIST_TRAVERSE_SAFE_END;
		SCCP_RWLIST_UNLOCK(&(objects[hash])->refCountedObjects);
	}
	if (obj) {
		sched_yield();				// make sure all other threads can finish their work first.
		                                        // should resolve lockless refcount SMP issues
		                                        // BTW we are not allowed to sleep whilst haveing a reference
		// fire destructor
		sccp_log(DEBUGCAT_REFCOUNT) (VERBOSE_PREFIX_1 "SCCP: (release) Destroying %p at hash: %d\n", obj, hash);
		if ((&obj_info[obj->type])->destructor)
			(&obj_info[obj->type])->destructor(ptr);
		memset(obj, 0, sizeof(RefCountedObject));
		sccp_free(obj);
		obj = NULL;
	}
}

#include <asterisk/cli.h>
void sccp_refcount_print_hashtable(int fd)
{
	int x, prev = 0;
	RefCountedObject *obj = NULL;

	pbx_cli(fd, "+==============================================================================================+\n");
	pbx_cli(fd, "| %5s | %17s | %25s | %15s | %4s | %4s | %4s |\n", "hash", "type", "id", "ptr", "refc", "live", "size");
	pbx_cli(fd, "|==============================================================================================|\n");
	ast_rwlock_rdlock(&objectslock);
	for (x = 0; x < SCCP_HASH_PRIME; x++) {
		if (objects[x] && &(objects[x])->refCountedObjects) {
			SCCP_RWLIST_RDLOCK(&(objects[x])->refCountedObjects);
			SCCP_RWLIST_TRAVERSE(&(objects[x])->refCountedObjects, obj, list) {
				if (prev == x) {
					pbx_cli(fd, "|  +->  ");
				} else {
					pbx_cli(fd, "| [%3d] ", x);
				}
				pbx_cli(fd, "| %17s | %25s | %15p | %4d | %4s | %4d |\n", (obj_info[obj->type]).datatype, obj->identifier, obj, (int) obj->refcount, SCCP_LIVE_MARKER == obj->alive ? "yes" : "no", (int) obj->len);
				prev = x;
			}
			SCCP_RWLIST_UNLOCK(&(objects[x])->refCountedObjects);
		}
	}
	ast_rwlock_unlock(&objectslock);
	pbx_cli(fd, "+==============================================================================================+\n");
}

void sccp_refcount_updateIdentifier(void *ptr, char *identifier)
{
	RefCountedObject *obj = NULL;

	if ((obj = find_obj(ptr, __FILE__, __LINE__, __PRETTY_FUNCTION__))) {
		sccp_copy_string(obj->identifier, identifier, sizeof(obj->identifier));
	} else {
		ast_log(LOG_ERROR, "SCCP: (updateIdentifief) Refcount Object %p could not be found\n", ptr);
	}
}

inline void *sccp_refcount_retain(void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj = NULL;
	volatile int refcountval;
	int newrefcountval;

	if ((obj = find_obj(ptr, filename, lineno, func))) {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug(ptr, obj, 1, filename, lineno, func);
#endif
		refcountval = ATOMIC_INCR((&obj->refcount), 1, &obj->lock);
		newrefcountval = refcountval + 1;

		if ((sccp_globals->debug & (((&obj_info[obj->type])->debugcat + DEBUGCAT_REFCOUNT)))
		    == ((&obj_info[obj->type])->debugcat + DEBUGCAT_REFCOUNT)) {
			ast_log(__LOG_VERBOSE, __FILE__, 0, "", " %-15.15s:%-4.4d (%-25.25s) %*.*s> %*s refcount increased %.2d  +> %.2d for %10s: %s (%p)\n", filename, lineno, func, refcountval, refcountval, "--------------------", 20 - refcountval, " ", refcountval, newrefcountval, (&obj_info[obj->type])->datatype, obj->identifier, obj);
		}
		return obj->data;
	} else {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug((void *) ptr, NULL, 1, filename, lineno, func);
#endif
		ast_log(__LOG_VERBOSE, __FILE__, 0, "retain", "SCCP: (%-15.15s:%-4.4d (%-25.25s)) ALARM !! trying to retain a %s: %s (%p) with invalid memory reference! this should never happen !\n", filename, lineno, func, (obj && (&obj_info[obj->type])->datatype) ? (&obj_info[obj->type])->datatype : "UNREF", (obj && obj->identifier) ? obj->identifier : "UNREF", obj);
		ast_log(LOG_ERROR, "SCCP: (release) Refcount Object %p could not be found (Major Logic Error). Please report to developers\n", ptr);
		return NULL;
	}
}

inline void *sccp_refcount_release(const void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj = NULL;
	volatile int refcountval;
	int newrefcountval;

	if ((obj = find_obj(ptr, filename, lineno, func))) {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug((void *) ptr, obj, -1, filename, lineno, func);
#endif
		refcountval = ATOMIC_DECR((&obj->refcount), 1, &obj->lock);
		newrefcountval = refcountval - 1;
		if (newrefcountval == 0) {
			ATOMIC_DECR(&obj->alive, SCCP_LIVE_MARKER, &obj->lock);
			sccp_log(DEBUGCAT_REFCOUNT) (VERBOSE_PREFIX_1 "SCCP: %-15.15s:%-4.4d (%-25.25s) (release) Finalizing %p (%p)\n", filename, lineno, func, obj, ptr);
			remove_obj(ptr);
		} else {
			if ((sccp_globals->debug & (((&obj_info[obj->type])->debugcat + DEBUGCAT_REFCOUNT))) == ((&obj_info[obj->type])->debugcat + DEBUGCAT_REFCOUNT)) {
				ast_log(__LOG_VERBOSE, __FILE__, 0, "", " %-15.15s:%-4.4d (%-25.25s) <%*.*s %*s refcount decreased %.2d  <- %.2d for %10s: %s (%p)\n", filename, lineno, func, newrefcountval, newrefcountval, "--------------------", 20 - newrefcountval, " ", newrefcountval, refcountval, (&obj_info[obj->type])->datatype, obj->identifier, obj);
			}
		}
	} else {
#if CS_REFCOUNT_DEBUG
		__sccp_refcount_debug((void *) ptr, NULL, -1, filename, lineno, func);
#endif
		ast_log(__LOG_VERBOSE, __FILE__, 0, "release", "SCCP (%-15.15s:%-4.4d (%-25.25s)) ALARM !! trying to release a %s: %s (%p) with invalid memory reference! this should never happen !\n", filename, lineno, func, (obj && (&obj_info[obj->type])->datatype) ? (&obj_info[obj->type])->datatype : "UNREF", (obj && obj->identifier) ? obj->identifier : "UNREF", obj);
		ast_log(LOG_ERROR, "SCCP: (release) Refcount Object %p could not be found (Major Logic Error). Please report to developers\n", ptr);
	}
	return NULL;
}
