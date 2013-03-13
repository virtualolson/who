#include "dialog.h"
#include "ro_session_hash.h"

void dlg_reply(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
	struct sip_msg *reply = _params->rpl;
	struct ro_session* session = 0;
	struct ro_session_entry* ro_session_entry;
	time_t now = time(0);
	time_t time_since_last_event;

	if (reply != FAKED_REPLY && reply->REPLY_STATUS == 200) {
		LM_DBG("Call answered - search for Ro Session and initialise timers.\n");

		session = (struct ro_session*)*_params->param;
		if (!session) {
			LM_ERR("Ro Session object is NULL...... aborting\n");
			return;
		}
		ro_session_entry = &(ro_session_table->entries[session->h_entry]);
		ro_session_lock(ro_session_table, ro_session_entry);

		time_since_last_event = now - session->last_event_timestamp;
		session->start_time = session->last_event_timestamp = now;
		session->event_type = answered;

		ro_session_unlock(ro_session_table, ro_session_entry);

		/* check to make sure that the validity of the credit is enough for the bundle */
		if (session->reserved_secs < (session->valid_for - time_since_last_event)) {
			if (session->reserved_secs > ro_timer_buffer/*TIMEOUTBUFFER*/) {
				insert_ro_timer(&session->ro_tl, session->reserved_secs - ro_timer_buffer); //subtract 5 seconds so as to get more credit before we run out
			} else {
				insert_ro_timer(&session->ro_tl, session->reserved_secs);
			}
		} else {
			if (session->valid_for > ro_timer_buffer) {
				insert_ro_timer(&session->ro_tl, session->valid_for - ro_timer_buffer); //subtract 5 seconds so as to get more credit before we run out
			} else {
				insert_ro_timer(&session->ro_tl, session->valid_for);
			}
		}
//		unref_ro_session(session, 1);	DONT need this anymore because we dont do lookup so no addition to ref counter
	}
}

void dlg_terminated(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
	int i;
	struct ro_session *ro_session = 0;
	struct ro_session_entry *ro_session_entry;

	if (!_params) {
		return;
	}

	struct sip_msg *request = _params->req;
	if (!request) {
		LM_WARN("dlg_terminated has no SIP request associated.\n");
	}

	if (dlg && (dlg->callid.s && dlg->callid.len > 0)) {
		/* find the session for this call, possibly both for orig and term*/
		for (i=0; i<2; i++) {
			if ((ro_session = lookup_ro_session(dlg->h_entry, &dlg->callid, 0, 0))) {
				ro_session_entry =
						&(ro_session_table->entries[ro_session->h_entry]);
				ro_session_lock(ro_session_table, ro_session_entry);
				if (ro_session->event_type != pending) {
					send_ccr_stop(ro_session);
				}
				ro_session_unlock(ro_session_table, ro_session_entry);
				unref_ro_session(ro_session, 2);
			}
		}
	}
}
