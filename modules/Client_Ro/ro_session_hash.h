/* 
 * File:   ro_session_hash.h
 * Author: Jason Penton
 *
 * Created on 07 April 2011, 4:12 PM
 */

#ifndef RO_SESSION_HASH_H
#define	RO_SESSION_HASH_H

#include "ro_timer.h"
#include "../../mem/shm_mem.h"
#include <stdlib.h>

enum ro_session_event_type {
    pending,
    answered,
    no_more_credit,
    unknown_error
};

struct ro_session {
    volatile int ref;
    struct ro_session* next;
    struct ro_session* prev;
    str ro_session_id;
    str callid;
    str from_uri;
    str to_uri;
    unsigned int hop_by_hop;
    struct ro_tl ro_tl;
    unsigned int reserved_secs;
    unsigned int valid_for;
    unsigned int dlg_h_entry;
    unsigned int dlg_h_id;
    unsigned int h_entry;
    unsigned int h_id;
    time_t start_time;
    time_t last_event_timestamp;
    enum ro_session_event_type event_type;
    int auth_appid;
    int auth_session_type;
};

/*! entries in the main ro_session table */
struct ro_session_entry {
    struct ro_session *first; /*!< dialog list */
    struct ro_session *last; /*!< optimisation, end of the dialog list */
    unsigned int next_id; /*!< next id */
    unsigned int lock_idx; /*!< lock index */
};

/*! main ro_sesion table */
struct ro_session_table {
    unsigned int size; /*!< size of the dialog table */
    struct ro_session_entry *entries; /*!< dialog hash table */
    unsigned int locks_no; /*!< number of locks */
    gen_lock_set_t *locks; /*!< lock table */
};


/*! global ro_session table */
extern struct ro_session_table *ro_session_table;


/*!
 * \brief Set a ro_session lock
 * \param _table ro_session table
 * \param _entry locked entry
 */
#define ro_session_lock(_table, _entry) \
		lock_set_get( (_table)->locks, (_entry)->lock_idx);


/*!
 * \brief Release a ro_session lock
 * \param _table ro_session table
 * \param _entry locked entry
 */
#define ro_session_unlock(_table, _entry) \
		lock_set_release( (_table)->locks, (_entry)->lock_idx);

/*!
 * \brief Unlink a ro_session from the list without locking
 * \see unref_ro_session_unsafe
 * \param ro_session_entry unlinked entry
 * \param ro_session unlinked ro_session
 */
static inline void unlink_unsafe_ro_session(struct ro_session_entry *ro_session_entry, struct ro_session *ro_session) {
    if (ro_session->next)
        ro_session->next->prev = ro_session->prev;
    else
        ro_session_entry->last = ro_session->prev;
    if (ro_session->prev)
        ro_session->prev->next = ro_session->next;
    else
        ro_session_entry->first = ro_session->next;

    ro_session->next = ro_session->prev = 0;

    return;
}

/*!
 * \brief Initialize the global ro_session table
 * \param size size of the table
 * \return 0 on success, -1 on failure
 */
int init_ro_session_table(unsigned int size);

/*!
 * \brief Destroy a ro_session and free memory
 * \param ro_session destroyed Ro Session
 */
inline void destroy_ro_session(struct ro_session *ro_session);

/*!
 * \brief Destroy the ro_session dialog table
 */
void destroy_ro_session_table(void);

/*!
 * \brief Link a ro_session structure
 * \param ro_session Ro Session
 * \param n extra increments for the reference counter
 */
void link_ro_session(struct ro_session *ro_session, int n);

void remove_aaa_session(str *session_id);

static inline struct ro_session* build_new_ro_session(int auth_appid, int auth_session_type, str *session_id, str *callid, str *from_uri, str* to_uri, unsigned int dlg_h_entry, unsigned int dlg_h_id, unsigned int requested_secs, unsigned int validity_timeout) {

    char *p;
    unsigned int len = session_id->len + callid->len + from_uri->len + to_uri->len + sizeof (struct ro_session);
    struct ro_session *new_ro_session = (struct ro_session*) shm_malloc(len);
    if (!new_ro_session) {
        LM_ERR("no more shm mem.\n");
        shm_free(new_ro_session);
        return 0;
    }

    memset(new_ro_session, 0, len);

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
 * \brief Refefence a ro_session with locking
 * \see ref_ro_session_unsafe
 * \param ro_session Ro Session
 * \param cnt increment for the reference counter
 */
void ref_ro_session(struct ro_session *ro_session, unsigned int cnt);

/*!
 * \brief Unreference a ro_session with locking
 * \see unref_ro_session_unsafe
 * \param ro_session Ro Session
 * \param cnt decrement for the reference counter
 */
void unref_ro_session(struct ro_session *ro_session, unsigned int cnt);

struct ro_session* lookup_ro_session(unsigned int h_entry, str *callid, unsigned int *del);


#endif	/* RO_SESSION_HASH_H */

