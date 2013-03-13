/* 
 * File:   ro_session_hash.c
 * Author: Jason Penton
 *
 * Created on 08 April 2011, 1:10 PM
 */

#include "ro_session_hash.h"

#define MAX_LDG_LOCKS  2048
#define MIN_LDG_LOCKS  2

/*! global ro_session table */
struct ro_session_table *ro_session_table = 0;

/*!
 * \brief Link a ro_session structure
 * \param ro_session Ro Session
 * \param n extra increments for the reference counter
 */
void link_ro_session(struct ro_session *ro_session, int n) {
    struct ro_session_entry *ro_session_entry;

    ro_session_entry = &(ro_session_table->entries[ro_session->h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);

    ro_session->h_id = ro_session_entry->next_id++;
    if (ro_session_entry->first == 0) {
        ro_session_entry->first = ro_session_entry->last = ro_session;
    } else {
        ro_session_entry->last->next = ro_session;
        ro_session->prev = ro_session_entry->last;
        ro_session_entry->last = ro_session;
    }

    ro_session->ref += 1 + n;
    
    ro_session_unlock(ro_session_table, ro_session_entry);

    return;
}

/*!
 * \brief Reference an ro_session without locking
 * \param _ro_session Ro Session
 * \param _cnt increment for the reference counter
 */
#define ref_ro_session_unsafe(_session,_cnt)     \
	do { \
		(_session)->ref += (_cnt); \
		LM_DBG("ref ro_session %p with %d -> %d\n", \
			(_session),(_cnt),(_session)->ref); \
	}while(0)


/*!
 * \brief Unreference an ro_session without locking
 * \param _ro_session Ro Session
 * \param _cnt decrement for the reference counter
 */
#define unref_ro_session_unsafe(_ro_session,_cnt,_ro_session_entry)   \
	do { \
		(_ro_session)->ref -= (_cnt); \
		LM_DBG("unref ro_session %p with %d -> %d\n",\
			(_ro_session),(_cnt),(_ro_session)->ref);\
		if ((_ro_session)->ref<0) {\
			LM_CRIT("bogus ref for session id < 0\n");\
		}\
		if ((_ro_session)->ref<=0) { \
			unlink_unsafe_ro_session( _ro_session_entry, _ro_session);\
			LM_DBG("ref <=0 for ro_session %p\n",_ro_session);\
			destroy_ro_session(_ro_session);\
		}\
	}while(0)

/*!
 * \brief Refefence an ro_session with locking
 * \see ref_ro_session_unsafe
 * \param ro_session Ro Session
 * \param cnt increment for the reference counter
 */
void ref_ro_session(struct ro_session *ro_session, unsigned int cnt) {
    struct ro_session_entry *ro_session_entry;

    ro_session_entry = &(ro_session_table->entries[ro_session->h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);
    ref_ro_session_unsafe(ro_session, cnt);
    ro_session_unlock(ro_session_table, ro_session_entry);
}

/*!
 * \brief Unreference an ro_session with locking
 * \see unref_ro_session_unsafe
 * \param ro_session Ro Session
 * \param cnt decrement for the reference counter
 */
void unref_ro_session(struct ro_session *ro_session, unsigned int cnt) {
    struct ro_session_entry *ro_session_entry;

    ro_session_entry = &(ro_session_table->entries[ro_session->h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);
    unref_ro_session_unsafe(ro_session, cnt, ro_session_entry);
    ro_session_unlock(ro_session_table, ro_session_entry);
}

/*!
 * \brief Initialize the global ro_session table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_ro_session_table(unsigned int size) {
    unsigned int n;
    unsigned int i;

    ro_session_table = (struct ro_session_table*) shm_malloc(sizeof (struct ro_session_table) +size * sizeof (struct ro_session_entry));
    if (ro_session_table == 0) {
        LM_ERR("no more shm mem (1)\n");
        goto error0;
    }

    memset(ro_session_table, 0, sizeof (struct ro_session_table));
    ro_session_table->size = size;
    ro_session_table->entries = (struct ro_session_entry*) (ro_session_table + 1);

    n = (size < MAX_LDG_LOCKS) ? size : MAX_LDG_LOCKS;
    for (; n >= MIN_LDG_LOCKS; n--) {
        ro_session_table->locks = lock_set_alloc(n);
        if (ro_session_table->locks == 0)
            continue;
        if (lock_set_init(ro_session_table->locks) == 0) {
            lock_set_dealloc(ro_session_table->locks);
            ro_session_table->locks = 0;
            continue;
        }
        ro_session_table->locks_no = n;
        break;
    }

    if (ro_session_table->locks == 0) {
        LM_ERR("unable to allocted at least %d locks for the hash table\n",
                MIN_LDG_LOCKS);
        goto error1;
    }

    for (i = 0; i < size; i++) {
        memset(&(ro_session_table->entries[i]), 0, sizeof (struct ro_session_entry));
        ro_session_table->entries[i].next_id = rand();
        ro_session_table->entries[i].lock_idx = i % ro_session_table->locks_no;
    }

    return 0;
error1:
    shm_free(ro_session_table);
error0:
    return -1;
}

/*!
 * \brief Destroy an ro_session and free memory
 * \param ro_session destroyed Ro Session
 */
inline void destroy_ro_session(struct ro_session *ro_session) {

    LM_DBG("destroying Ro Session %p\n", ro_session);

    remove_ro_timer(&ro_session->ro_tl);

    if (ro_session->ro_session_id.s && (ro_session->ro_session_id.len > 0)) {
        shm_free(ro_session->ro_session_id.s);
    }
    shm_free(ro_session);
    ro_session = 0;
}

/*!
 * \brief Destroy the global Ro session table
 */
void destroy_dlg_table(void) {
    struct ro_session *ro_session, *l_ro_session;
    unsigned int i;

    if (ro_session_table == 0)
        return;

    if (ro_session_table->locks) {
        lock_set_destroy(ro_session_table->locks);
        lock_set_dealloc(ro_session_table->locks);
    }

    for (i = 0; i < ro_session_table->size; i++) {
        ro_session = ro_session_table->entries[i].first;
        while (ro_session) {
            l_ro_session = ro_session;
            ro_session = ro_session->next;
            destroy_ro_session(l_ro_session);
        }

    }

    shm_free(ro_session_table);
    ro_session_table = 0;

    return;
}

struct ro_session* build_new_ro_session(int direction, int auth_appid, int auth_session_type, str *session_id, str *callid, str *from_uri, str* to_uri, unsigned int dlg_h_entry, unsigned int dlg_h_id, unsigned int requested_secs, unsigned int validity_timeout){
    LM_DBG("Building Ro Session **********");
    char *p;
    unsigned int len = session_id->len + callid->len + from_uri->len + to_uri->len + sizeof (struct ro_session);
    struct ro_session *new_ro_session = (struct ro_session*) shm_malloc(len);

    if (!new_ro_session) {
        LM_ERR("no more shm mem.\n");
        shm_free(new_ro_session);
        return 0;
    }

    memset(new_ro_session, 0, len);

    new_ro_session->dir = direction;
    new_ro_session->auth_appid = auth_appid;
    new_ro_session->auth_session_type = auth_session_type;

    new_ro_session->ro_tl.next = new_ro_session->ro_tl.prev;
    new_ro_session->ro_tl.timeout = 0; //requested_secs;

    new_ro_session->reserved_secs = requested_secs;
    new_ro_session->valid_for = validity_timeout;

    new_ro_session->hop_by_hop = 1;
    new_ro_session->next = 0;
    new_ro_session->dlg_h_entry = dlg_h_entry;
    new_ro_session->dlg_h_id = dlg_h_id;

    new_ro_session->h_entry = dlg_h_entry; /* we will use the same entry ID as the dlg - saves us using our own hash function */
    new_ro_session->h_id = 0;
    new_ro_session->ref = 0;

    p = (char*) (new_ro_session + 1);
    new_ro_session->callid.s = p;
    new_ro_session->callid.len = callid->len;
    memcpy(p, callid->s, callid->len);
    p += callid->len;

    new_ro_session->ro_session_id.s = p;
    new_ro_session->ro_session_id.len = session_id->len;
    memcpy(p, session_id->s, session_id->len);
    p += session_id->len;

    new_ro_session->from_uri.s = p;
    new_ro_session->from_uri.len = from_uri->len;
    memcpy(p, from_uri->s, from_uri->len);
    p += from_uri->len;

    new_ro_session->to_uri.s = p;
    new_ro_session->to_uri.len = to_uri->len;
    memcpy(p, to_uri->s, to_uri->len);
    p += to_uri->len;


    if (p != (((char*) new_ro_session) + len)) {
        LM_ERR("buffer overflow\n");
        shm_free(new_ro_session);
        return 0;
    }

    return new_ro_session;

}

/*!
 * \brief Lookup a dialog in the global list
 * \param h_entry number of the hash table entry
 * \param h_id id of the hash table entry
 * \return dialog on success, NULL on failure
 */
struct ro_session* lookup_ro_session(unsigned int h_entry, str* callid, int direction, unsigned int *del) {
    struct ro_session *ro_session;
    struct ro_session_entry *ro_session_entry;

    if (del != NULL)
        *del = 0;

    if (h_entry >= ro_session_table->size)
        goto not_found;
    ro_session_entry = &(ro_session_table->entries[h_entry]);

    ro_session_lock(ro_session_table, ro_session_entry);

    for (ro_session = ro_session_entry->first; ro_session; ro_session = ro_session->next) {
        if ((direction==0 || direction==ro_session->dir) && (strncmp(ro_session->callid.s, callid->s, callid->len)==0)) {
            ro_session->ref++;
            LM_DBG("ref dlg %p with 1 -> %d\n", ro_session, ro_session->ref);
            ro_session_unlock(ro_session_table, ro_session_entry);
            LM_DBG("ro_session id=%u found on entry %u\n", ro_session->h_id, h_entry);
            return ro_session;
        }
    }

    ro_session_unlock(ro_session_table, ro_session_entry);
not_found:
    LM_DBG("no ro_session for callid=%.*s found on entry %u\n", callid->len, callid->s, h_entry);
    return 0;
}


