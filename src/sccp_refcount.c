
/*!
 * \file 	sccp_refcount.c
 * \brief 	SCCP Refcount Class
 * \note	This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * 
 * $Date$
 * $Revision$  
 */

/*!
 * \file
 * \page sccp_lock Locking and Object Preservation in SCCP
 *
 * \ref refcount   Reference Counting\n
 * 
 * \section refcount Refcounted Objects
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
 *   d = sccp_device_release(d);		// sccp_release always returns NULL
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
 
#include "config.h"
#include "common.h"

SCCP_FILE_VERSION(__FILE__, "$Revision$")

// current implementation
// 
// Using positive live object list to verify existence. This is a heavy weight solution, when the number of devices/lines/hints etc increases. (Garantees existence)
// If we would use negative dead object list to verify non-existence instead, we would only have to verify a much short list, the garbage collection time might have to be increased
// When we only use a negative check the liveobject list could be removed
// 
// To make 100% sure we are not missing out any inflight references that have not been accounted for.
// We migh have to implemented both methods for DEVELOPMENT(livelistcheck) and PRODUCTION(deadlistcheck)
//
// define CS_REFCOUNT_LIVEOBJECTS controls which version is used
#define CS_REFCOUNT_LIVEOBJECTS DEBUG

struct refcount_object {
#if SCCP_ATOMIC_OPS
	volatile size_t refcount;
#else
#  ifdef SCCP_BUILTIN_CAS64
	volatile int32_t refcount;
#  else
	volatile int64_t refcount;
#  endif	
#endif
	int (*destructor) (const void *ptr);
	char datatype[StationMaxDeviceNameSize];
	char identifier[StationMaxDeviceNameSize];
	void *data;
	SCCP_RWLIST_ENTRY(RefCountedObject) list;
	time_t dead_since;
};
boolean_t refcount_destroyed = TRUE;

#if CS_REFCOUNT_LIVEOBJECTS
SCCP_RWLIST_HEAD(, RefCountedObject) refcount_liveobjects;
#endif
SCCP_RWLIST_HEAD(, RefCountedObject) refcount_deadobjects;

void sccp_refcount_init(void)
{
       	sccp_log ((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) init\n");
#if CS_REFCOUNT_LIVEOBJECTS
        SCCP_RWLIST_HEAD_INIT(&refcount_liveobjects);
#endif        
        SCCP_RWLIST_HEAD_INIT(&refcount_deadobjects);
        refcount_destroyed = FALSE;
}

#if CS_REFCOUNT_LIVEOBJECTS
static void sccp_refcount_addToLiveObjects(RefCountedObject *obj)
{
	sccp_log ((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) addToLiveObjects: %p\n", obj);
        SCCP_RWLIST_WRLOCK(&refcount_liveobjects);
        SCCP_RWLIST_INSERT_HEAD(&refcount_liveobjects, obj, list);
        SCCP_RWLIST_UNLOCK(&refcount_liveobjects);
}
#endif

#if !CS_REFCOUNT_LIVEOBJECTS
static void sccp_refcount_addToDeadObjects(RefCountedObject *obj)
{
	sccp_log ((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) addToDeadObjects: %p\n", obj);
        SCCP_RWLIST_WRLOCK(&refcount_deadobjects);
        SCCP_RWLIST_INSERT_HEAD(&refcount_deadobjects, obj, list);
        SCCP_RWLIST_UNLOCK(&refcount_deadobjects);
}
#endif

/* positive / livelist check */
#if CS_REFCOUNT_LIVEOBJECTS
static boolean_t sccp_refcount_isLiveObject(RefCountedObject *obj)	// check if in alive objects list
{
//	sccp_log ((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) isLiveObject: %p\n", obj);
        RefCountedObject *list_obj = NULL;
        boolean_t res = FALSE;
        SCCP_RWLIST_RDLOCK(&refcount_liveobjects);
        SCCP_RWLIST_TRAVERSE(&refcount_liveobjects, list_obj, list) {
                if (list_obj == obj)
                        res = TRUE;
        }
        SCCP_RWLIST_UNLOCK(&refcount_liveobjects);
        return res;
}
#endif

int sccp_refcount_isRunning(void) {
        return !refcount_destroyed;
}

/* negative / deadlist check */
#if !CS_REFCOUNT_LIVEOBJECTS
static boolean_t sccp_refcount_isDeadObject(RefCountedObject *obj)	// check if NOT in deadobjects list
{
//	sccp_log ((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) isDeadObject: %p\n", obj);
        RefCountedObject *list_obj = NULL;
        boolean_t res = FALSE;
        SCCP_RWLIST_RDLOCK(&refcount_deadobjects);
        SCCP_RWLIST_TRAVERSE(&refcount_deadobjects, list_obj, list) {
                if (list_obj == obj)
                        res = TRUE;
        }
        SCCP_RWLIST_UNLOCK(&refcount_deadobjects);
        return res;
}
#endif

#if CS_REFCOUNT_LIVEOBJECTS
static void sccp_refcount_moveFromLiveToDeadObjects(RefCountedObject *obj)
{
	sccp_log ((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) moveFromLiveToDeadObjects: %p\n", obj);
        SCCP_RWLIST_WRLOCK(&refcount_liveobjects);
        SCCP_RWLIST_WRLOCK(&refcount_deadobjects);
        obj = SCCP_RWLIST_REMOVE(&refcount_liveobjects, obj, list);
        obj->dead_since = time(0);
        SCCP_RWLIST_INSERT_TAIL(&refcount_deadobjects, obj, list);
        SCCP_RWLIST_UNLOCK(&refcount_deadobjects);
        SCCP_RWLIST_UNLOCK(&refcount_liveobjects);
}
#endif

static int RefCountedObjectDestroy(const void *data) {
        RefCountedObject *obj=(RefCountedObject *)data;
	sccp_log((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) RefCountedObjectDestroy for %p\n\n", obj);
        sccp_free(obj);
        obj=NULL;
        return 0;
}

static void sccp_refcount_cleanDeadObjects(boolean_t force)
{
	sccp_log ((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) cleanDeadObjects\n");
        RefCountedObject *list_obj = NULL;
        int num_dead_objects = 0;
        int total_dead_objects = 0;
        SCCP_RWLIST_WRLOCK(&refcount_deadobjects);
        SCCP_LIST_TRAVERSE_SAFE_BEGIN(&refcount_deadobjects, list_obj, list) {
                if (force || (time(0) - list_obj->dead_since > (SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT/1000) && num_dead_objects < 30)) {	// force or (expired and 30 object per round)
                        SCCP_LIST_REMOVE_CURRENT(list);
                        RefCountedObjectDestroy(list_obj);
                        num_dead_objects++;
                        if (!force && ((num_dead_objects % 5)==0)) {									// sleep a little so that garbage collection does not interfere with audio
                                sccp_safe_sleep(10);
                        }
                }
                total_dead_objects++;
        }
        SCCP_LIST_TRAVERSE_SAFE_END;
        SCCP_RWLIST_UNLOCK(&refcount_deadobjects);
	sccp_log(((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH))) (VERBOSE_PREFIX_3 "SCCP: (Refcount) cleaned %d/%d dead objects\n", num_dead_objects, total_dead_objects);
}

int sccp_refcount_schedule_cleanup(const void *data) 
{
        if (SCCP_RWLIST_GETSIZE(refcount_deadobjects) > 0) {
                sccp_refcount_cleanDeadObjects(FALSE);
        }
        if (!refcount_destroyed) {
        	sccp_log(DEBUGCAT_HIGH) (VERBOSE_PREFIX_3 "SCCP: (Refcount) schedule garbage collection\n");
                PBX(sched_add)(SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT * 10, sccp_refcount_schedule_cleanup, (const void *)0);
        }
        return 0;
}

#if CS_REFCOUNT_LIVEOBJECTS
static void sccp_refcount_dumpLiveObjects() 
{
	sccp_log ((DEBUGCAT_REFCOUNT | DEBUGCAT_HIGH)) ("SCCP: (Refcount) dumpLiveObjects\n");
        RefCountedObject *list_obj = NULL;

        SCCP_RWLIST_RDLOCK(&refcount_liveobjects);
        SCCP_RWLIST_TRAVERSE(&refcount_liveobjects, list_obj, list) {
                if (list_obj)
               	        pbx_log(LOG_ERROR, "SCCP: (Refcount) dumpLiveObject type=%s, identifier=%s, obj pointer=%p, data pointer=%p\n", list_obj->datatype, list_obj->identifier, list_obj, list_obj->data);
        }
        SCCP_RWLIST_UNLOCK(&refcount_liveobjects);
}
#endif

#if CS_REFCOUNT_LIVEOBJECTS
void sccp_refcount_destroy(void)
{
       	pbx_log (LOG_NOTICE, "SCCP: (Refcount) destroying: live objects %i, dead objects %i\n", SCCP_RWLIST_GETSIZE(refcount_liveobjects), SCCP_RWLIST_GETSIZE(refcount_deadobjects));
        refcount_destroyed = TRUE;
        int num_tries = 0;
        while (SCCP_RWLIST_GETSIZE(refcount_liveobjects) != 0 || SCCP_RWLIST_GETSIZE(refcount_deadobjects) != 0) {
               	pbx_log(LOG_VERBOSE, "SCCP: (Refcount) live objects %i, dead objects %i. Destroying...\n", SCCP_RWLIST_GETSIZE(refcount_liveobjects), SCCP_RWLIST_GETSIZE(refcount_deadobjects));
               	if (SCCP_RWLIST_GETSIZE(refcount_liveobjects) != 0) {
               	        if (num_tries > 1) 
                       	        pbx_log(LOG_NOTICE, "SCCP: (Refcount) There is still a live %i refcounted objects in use.\n", SCCP_RWLIST_GETSIZE(refcount_liveobjects));
               	        sccp_refcount_dumpLiveObjects();
                        num_tries++;
                        if (num_tries > 3) {
                       	        pbx_log(LOG_ERROR, "SCCP: (Refcount) Forcing Exit -> Leaking Memory for %d Objects. Notify developers\n", SCCP_RWLIST_GETSIZE(refcount_liveobjects));
                                goto EXIT;
                        }
               	}
                if (SCCP_RWLIST_GETSIZE(refcount_deadobjects) > 0) {
                        sccp_refcount_cleanDeadObjects(TRUE);
                }
                sccp_safe_sleep(SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT);
        }
EXIT:        
        SCCP_RWLIST_WRLOCK(&refcount_liveobjects);
        SCCP_RWLIST_WRLOCK(&refcount_deadobjects);
        SCCP_RWLIST_HEAD_DESTROY(&refcount_liveobjects);
        SCCP_RWLIST_HEAD_DESTROY(&refcount_deadobjects);
        SCCP_RWLIST_UNLOCK(&refcount_deadobjects);
        SCCP_RWLIST_UNLOCK(&refcount_liveobjects);
}
#else
void sccp_refcount_destroy(void)
{
       	pbx_log (LOG_NOTICE, "SCCP: (Refcount) destroying: dead objects %i\n", SCCP_RWLIST_GETSIZE(refcount_deadobjects));
        refcount_destroyed = TRUE;
        int num_tries = 0;
        while (SCCP_RWLIST_GETSIZE(refcount_deadobjects) != 0) {
               	pbx_log(LOG_VERBOSE, "SCCP: (Refcount) dead objects %i. Destroying...\n", SCCP_RWLIST_GETSIZE(refcount_deadobjects));
                if (SCCP_RWLIST_GETSIZE(refcount_deadobjects) > 0) {
                        sccp_refcount_cleanDeadObjects(TRUE);
                }
                sccp_safe_sleep(SCCP_TIME_TO_KEEP_REFCOUNTEDOBJECT);
                num_tries++;
                if (num_tries > 20) {
                        pbx_log(LOG_ERROR, "SCCP: (Refcount) Forcing Exit -> Possibly Leaking Memory for Objects. Notify developers\n");
                        goto EXIT;
                }
        }
EXIT:        
        SCCP_RWLIST_WRLOCK(&refcount_deadobjects);
        SCCP_RWLIST_HEAD_DESTROY(&refcount_deadobjects);
        SCCP_RWLIST_UNLOCK(&refcount_deadobjects);
}
#endif

void *sccp_refcount_object_alloc(size_t size, const char *datatype, const char *identifier, void *destructor)
{
	RefCountedObject *obj;
	char *ptr;
	if (refcount_destroyed)
	        return NULL;

	obj = (RefCountedObject *) sccp_calloc(1, sizeof(RefCountedObject) + size);
	if (!obj) {
        	pbx_log(LOG_ERROR, "SCCP: (Refcount) RefCountedObject Memory Allocation Error for size %d\n", (int) size);
	        return NULL;
	}        
	ptr = (char *)obj;
	ptr += sizeof(RefCountedObject);
	obj->refcount = 1;
	obj->data = ptr;
	obj->destructor = destructor;
	sccp_copy_string(obj->datatype, datatype, sizeof(obj->datatype));
	sccp_copy_string(obj->identifier, identifier, sizeof(obj->identifier));
#if CS_REFCOUNT_LIVEOBJECTS
	sccp_refcount_addToLiveObjects(obj);
#endif
	sccp_log((DEBUGCAT_REFCOUNT)) (VERBOSE_PREFIX_3 "SCCP: (Refcount) Refcount initialized for object %p in %p to %d\n", ptr, obj, (int)obj->refcount);
	return (void *)ptr;
}

void sccp_refcount_updateIdentifier(void *ptr, char *identifier)
{
	RefCountedObject *obj;
	char *cptr;

	cptr = (char *)ptr;
	cptr -= sizeof(RefCountedObject);
	obj = (RefCountedObject *) cptr;

#if CS_REFCOUNT_LIVEOBJECTS
	if (obj && sccp_refcount_isLiveObject(obj)) {
#else
	if (obj && !sccp_refcount_isDeadObject(obj)) {
#endif
        	sccp_copy_string(obj->identifier, identifier, sizeof(obj->identifier));
	}
}

inline void *sccp_refcount_retain(void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj;
	char *cptr;
	int refcountval, newrefcountval;

	cptr = (char *)ptr;
	cptr -= sizeof(RefCountedObject);
	obj = (RefCountedObject *) cptr;

#if DEBUG
        sccp_log((DEBUGCAT_REFCOUNT)) ("%-15.15s:%-4.4d (%-25.25s) ", filename, lineno, func);
#endif	
	if (ptr && cptr && obj) {
	        do {
#if CS_REFCOUNT_LIVEOBJECTS
	                if (NULL == obj || !sccp_refcount_isLiveObject(obj) || NULL == obj->data || obj->refcount <= 0)
#else	                
	                if (NULL == obj || sccp_refcount_isDeadObject(obj) || NULL == obj->data || obj->refcount <= 0)
#endif
	                        return NULL;
	                refcountval = obj->refcount;
	                newrefcountval = refcountval+1;
	                if(refcountval < 1) {
        			sccp_log (0) ("SCCP: (Refcount) ALARM !! refcount already reached 0 for %s: %s (%p)-> obj is fading! (refcountval = %d)\n", obj->datatype, obj->identifier, obj, refcountval);
	                        return NULL;
                        }
// 	        } while (!__sync_bool_compare_and_swap(&obj->refcount, refcountval, newrefcountval));
#ifdef SCCP_ATOMIC
#  ifdef SCCP_BUILTIN_CAS32
		} while (!__sync_bool_compare_and_swap(&obj->refcount, refcountval, newrefcountval));		// Atomic Swap In newmsgptr
#  else
                } while (!AO_compare_and_swap(&obj->refcount, refcountval, newrefcountval));           		// Atomic Swap  using boemc atomic_ops library
#  endif                
#else
                        // would require lock
        		#warning "Atomic functions not implemented, please install libatomic_ops package to remedy. Otherwise problems are to be expected."
			obj->refcount = newrefcountval;
		} while (0);
#endif
		
                sccp_log((DEBUGCAT_REFCOUNT)) ("%s: %*.*s> refcount for %s: %s(%p) increased to: %d\n", obj->identifier, refcountval, refcountval, "------------", obj->datatype, obj->identifier, obj, newrefcountval);
		return ptr;
	} else {
		sccp_log (0) ("SCCP: (Refcount) ALARM !! trying to retain a %s: %s (%p) with invalid memory reference! this should never happen !\n", (obj && obj->datatype) ? obj->datatype : "UNREF",(obj && obj->identifier) ? obj->identifier : "UNREF", obj);
		return NULL;
	}
}


inline void *sccp_refcount_release(const void *ptr, const char *filename, int lineno, const char *func)
{
	RefCountedObject *obj;
	char *cptr;
	int refcountval, newrefcountval;

	cptr = (char *)ptr;
	cptr -= sizeof(RefCountedObject);
	obj = (RefCountedObject *) cptr;

#if DEBUG
        sccp_log((DEBUGCAT_REFCOUNT)) ("%-15.15s:%-4.4d (%-25.25s) ", filename, lineno, func);
#endif	
	if (ptr && cptr && obj) {
	        do {
#if CS_REFCOUNT_LIVEOBJECTS
	                if (NULL == obj || !sccp_refcount_isLiveObject(obj) || NULL == obj->data || obj->refcount < 0) {
#else
	                if (NULL == obj || sccp_refcount_isDeadObject(obj) || NULL == obj->data || obj->refcount < 0) {
#endif
        			sccp_log (0) ("SCCP: (Refcount) ALARM !! refcount would go below 0 for %s: %s (%p)-> obj is fading! (refcountval = %d)\n", obj->datatype, obj->identifier, obj, refcountval);
	                        return NULL;
                        }
	                refcountval = obj->refcount;
	                newrefcountval = refcountval-1;
// 	        } while (!__sync_bool_compare_and_swap(&obj->refcount, refcountval, newrefcountval));
#ifdef SCCP_ATOMIC
#  ifdef SCCP_BUILTIN_CAS32
		} while (!__sync_bool_compare_and_swap(&obj->refcount, refcountval, newrefcountval));		// Atomic Swap In newmsgptr
#  else
                } while (!AO_compare_and_swap(&obj->refcount, refcountval, newrefcountval));			// Atomic Swap  using boemc atomic_ops library
#  endif                
#else
        		#warning "Atomic functions not implemented, please install libatomic_ops package to remedy. Otherwise problems are to be expected."
			obj->refcount = newrefcountval;
		} while (0);
#endif
		
	        if (0 == newrefcountval) {
			sccp_log((DEBUGCAT_REFCOUNT)) ("%s: refcount for object(%p) has reached 0 -> cleaning up!\n", obj->identifier, obj);
#if CS_REFCOUNT_LIVEOBJECTS
			sccp_refcount_moveFromLiveToDeadObjects(obj);
#else
                        // would require lock
                        sccp_refcount_addToDeadObjects(obj);
#endif
//			usleep(1000);
			obj->destructor(ptr);
			memset(obj->data, 0, sizeof(obj->data));		// leave cleaned memory segment, just in case another thread is in neighborhood (reentrancy)
		} else {
			sccp_log((DEBUGCAT_REFCOUNT)) ("%s: <%*.*s refcount for %s: %s(%p) decreased to: %d\n", obj->identifier, refcountval, refcountval, "------------", obj->datatype, obj->identifier, obj, refcountval);
		}
	} else {
		sccp_log (0) ("SCCP: (Refcount) ALARM !! trying to release a %s: %s (%p) with invalid memory reference! this should never happen !\n", (obj && obj->datatype) ? obj->datatype : "UNREF",(obj && obj->identifier) ? obj->identifier : "UNREF", obj);
	}

	return NULL;
}
