/*
 * mod.c
 *
 *  Created on: 21 Feb 2013
 *      Author: jaybeepee
 */

#include "mod.h"
#include "../../sr_module.h"
#include "../../modules/dialog_ng/dlg_load.h"
#include "../../modules/dialog_ng/dlg_hash.h"
#include "../cdp/cdp_load.h"
#include "../cdp_avp/mod_export.h"
#include "../../parser/parse_to.h"
#include "ro_timer.h"
#include "ro_session_hash.h"
#include "ims_ro.h"
#include "config.h"
#include "dialog.h"

MODULE_VERSION

/* parameters */
str orig_session_key = { "originating_ro", 14 };
str term_session_key = { "terminating_ro", 14 };
char* ro_origin_host_s = "scscf.ims.smilecoms.com";
char* ro_origin_realm_s = "ims.smilecoms.com";
char* ro_destination_realm_s = "ims.smilecoms.com";
char* ro_destination_host_s = "hss.ims.smilecoms.com";
char* ro_service_context_id_root_s = "32260@3gpp.org";
char* ro_service_context_id_ext_s = "ext";
char* ro_service_context_id_mnc_s = "01";
char* ro_service_context_id_mcc_s = "001";
char* ro_service_context_id_release_s = "8";
static int ro_session_hash_size = 4096;
int ro_timer_buffer = 5;
extern int interim_request_credits;
int interim_request_credits = 30;
client_ro_cfg cfg;

struct cdp_binds cdpb;
struct dlg_binds dlgb;
cdp_avp_bind_t *cdp_avp;
struct tm_binds tmb;

char* rx_dest_realm_s = "ims.smilecoms.com";
str rx_dest_realm;
/* Only used if we want to force the Ro peer usually this is configured at a stack level and the first request uses realm routing */
//char* rx_forced_peer_s = "";
str ro_forced_peer;
int ro_auth_expiry = 7200;
int cdp_event_latency = 1; /*flag: report slow processing of CDP callback events or not - default enabled */
int cdp_event_threshold = 500; /*time in ms above which we should report slow processing of CDP callback event - default 500ms*/
int cdp_event_latency_loglevel = 0; /*log-level to use to report slow processing of CDP callback event - default ERROR*/

/** module functions */
static int mod_init(void);
static int mod_child_init(int);
static void mod_destroy(void);
static int w_ro_ccr(struct sip_msg *msg, str* direction, str* charge_type, str* unit_type, int reservation_units);
void ro_session_ontimeout(struct ro_tl *tl);

static int ro_fixup(void **param, int param_no);

static cmd_export_t cmds[] = {
		{ "Ro_CCR", 	(cmd_function) w_ro_ccr, 4, ro_fixup, 0, REQUEST_ROUTE },
		{ 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[] = {
		{ "hash_size", 				INT_PARAM,			&ro_session_hash_size 		},
		{ "interim_update_credits",	INT_PARAM,			&interim_request_credits 	},
		{ "timer_buffer", 			INT_PARAM,			&ro_timer_buffer 			},
		{ "ro_forced_peer", 		STR_PARAM, 			&ro_forced_peer.s 			},
		{ "ro_auth_expiry",			INT_PARAM, 			&ro_auth_expiry 			},
		{ "cdp_event_latency", 		INT_PARAM,			&cdp_event_latency 			}, /*flag: report slow processing of CDP
																						callback events or not */
		{ "cdp_event_threshold", 	INT_PARAM, 			&cdp_event_threshold 		}, /*time in ms above which we should
																						report slow processing of CDP callback event*/
		{ "cdp_event_latency_log", 	INT_PARAM, 			&cdp_event_latency_loglevel },/*log-level to use to report
																						slow processing of CDP callback event*/
		{ "origin_host", 			STR_PARAM, 			&ro_origin_host_s 			},
		{ "origin_realm", 			STR_PARAM,			&ro_origin_realm_s 			},
		{ "destination_realm", 		STR_PARAM,			&ro_destination_realm_s 	},
		{ "destination_host", 		STR_PARAM,			&ro_destination_host_s 		},
		{ "service_context_id_root",STR_PARAM,			&ro_service_context_id_root_s 	},
		{ "service_context_id_ext", STR_PARAM,			&ro_service_context_id_ext_s 	},
		{ "service_context_id_mnc", STR_PARAM,			&ro_service_context_id_mnc_s 	},
		{ "service_context_id_mcc", STR_PARAM,			&ro_service_context_id_mcc_s 	},
		{ "service_context_id_release",	STR_PARAM, 		&ro_service_context_id_release_s},
		{ 0, 0, 0 }
};

stat_export_t mod_stats[] = {
		/*{"ccr_avg_response_time" ,  STAT_IS_FUNC, 	(stat_var**)get_avg_ccr_response_time	},*/
		/*{"ccr_timeouts" ,  			0, 				(stat_var**)&stat_ccr_timeouts  		},*/
		{ 0, 0, 0 }
};

/** module exports */
struct module_exports exports = { "ims_ro", DEFAULT_DLFLAGS, /* dlopen flags */
		cmds, 		/* Exported functions */
		params, 	/* Exported params */
		0, 			/* exported statistics */
		0, 			/* exported MI functions */
		0, 			/* exported pseudo-variables */
		0, 			/* extra processes */
		mod_init, 	/* module initialization function */
		0,
		mod_destroy, 	/* module destroy functoin */
		mod_child_init 	/* per-child init function */
};

int fix_parameters() {
	cfg.origin_host.s = ro_origin_host_s;
	cfg.origin_host.len = strlen(ro_origin_host_s);

	cfg.origin_realm.s = ro_origin_realm_s;
	cfg.origin_realm.len = strlen(ro_origin_realm_s);

	cfg.destination_realm.s = ro_destination_realm_s;
	cfg.destination_realm.len = strlen(ro_destination_realm_s);

	cfg.destination_host.s = ro_destination_host_s;
	cfg.destination_host.len = strlen(ro_destination_host_s);

	cfg.service_context_id = shm_malloc(sizeof(str));
	if (!cfg.service_context_id) {
		LM_ERR("fix_parameters:not enough shm memory\n");
		return 0;
	}
	cfg.service_context_id->len = strlen(ro_service_context_id_ext_s)
			+ strlen(ro_service_context_id_mnc_s)
			+ strlen(ro_service_context_id_mcc_s)
			+ strlen(ro_service_context_id_release_s)
			+ strlen(ro_service_context_id_root_s) + 5;
	cfg.service_context_id->s =
			pkg_malloc(cfg.service_context_id->len * sizeof (char));
	if (!cfg.service_context_id->s) {
		LM_ERR("fix_parameters: not enough memory!\n");
		return 0;
	}
	cfg.service_context_id->len = sprintf(cfg.service_context_id->s,
			"%s.%s.%s.%s.%s", ro_service_context_id_ext_s,
			ro_service_context_id_mnc_s, ro_service_context_id_mcc_s,
			ro_service_context_id_release_s, ro_service_context_id_root_s);
	if (cfg.service_context_id->len < 0) {
		LM_ERR("fix_parameters: error while creating service_context_id\n");
		return 0;
	}

	return 1;
}

static int mod_init(void) {
	int n;
	load_dlg_f load_dlg;
	load_tm_f load_tm;

	if (!fix_parameters()) {
		LM_ERR("unable to set Ro configuration parameters correctly\n");
		goto error;
	}

	/* bind to the tm module */
	if (!(load_tm = (load_tm_f) find_export("load_tm", NO_SCRIPT, 0))) {
		LM_ERR("Can not import load_tm. This module requires tm module\n");
		goto error;
	}
	if (load_tm(&tmb) == -1)
		goto error;

	if (!(load_dlg = (load_dlg_f) find_export("load_dlg", 0, 0))) { /* bind to dialog module */
		LM_ERR("can not import load_dlg. This module requires Kamailio dialog module.\n");
	}
	if (load_dlg(&dlgb) == -1) {
		goto error;
	}

	if (load_cdp_api(&cdpb) != 0) { /* load the CDP API */
		LM_ERR("can't load CDP API\n");
		goto error;
	}

	if (load_dlg_api(&dlgb) != 0) { /* load the dialog API */
		LM_ERR("can't load Dialog API\n");
		goto error;
	}

	cdp_avp = load_cdp_avp(); /* load CDP_AVP API */
	if (!cdp_avp) {
		LM_ERR("can't load CDP_AVP API\n");
		goto error;
	}

	/* init timer lists*/
	if (init_ro_timer(ro_session_ontimeout) != 0) {
		LM_ERR("cannot init timer list\n");
		return -1;
	}

	/* initialized the hash table */
	for (n = 0; n < (8 * sizeof(n)); n++) {
		if (ro_session_hash_size == (1 << n))
			break;
		if (ro_session_hash_size < (1 << n)) {
			LM_WARN("hash_size is not a power of 2 as it should be -> rounding from %d to %d\n", ro_session_hash_size, 1 << (n - 1));
			ro_session_hash_size = 1 << (n - 1);
		}
	}

	if (init_ro_session_table(ro_session_hash_size) < 0) {
		LM_ERR("failed to create ro session hash table\n");
		return -1;
	}

	/* register global timer */
	if (register_timer(ro_timer_routine, 0/*(void*)ro_session_list*/, 1) < 0) {
		LM_ERR("failed to register timer \n");
		return -1;
	}

	/* init timer lists*/
	if (init_ro_timer(ro_session_ontimeout) != 0) {
		LM_ERR("cannot init timer list\n");
		return -1;
	}


	/* bind to dialog module */
	if (!(load_dlg = (load_dlg_f) find_export("load_dlg", 0, 0))) {
		LM_ERR("mod_init: can not import load_dlg. This module requires Kamailio dialog moduile.\n");
	}

	if (load_dlg(&dlgb) == -1) {
		goto error;
	}

	return 0;

	error:
	LM_ERR("Failed to initialise ims_qos module\n");
	return RO_RETURN_FALSE;

}

static int mod_child_init(int rank) {
	LM_DBG("Initialization of module in child [%d] \n", rank);
	return 0;
}

static void mod_destroy(void) {

}

static int w_ro_ccr(struct sip_msg *msg, str* direction, str* charge_type, str* unit_type, int reservation_units) {
	/* PSEUDOCODE/NOTES
	 * 1. What mode are we in - terminating or originating
	 * 2. check request type - 	IEC - Immediate Event Charging
	 * 							ECUR - Event Charging with Unit Reservation
	 * 							SCUR - Session Charging with Unit Reservation
	 * 3. probably only do SCUR in this module for now - can see event based charging in another component instead (AS for SMS for example, etc)
	 * 4. Check a dialog exists for call, if not we fail
	 * 5. make sure we dont already have an Ro Session for this dialog
	 * 6. create new Ro Session
	 * 7. register for DLG callback passing new Ro session as parameter - (if dlg torn down we know which Ro session it is associated with)
	 *
	 *
	 */
	LM_DBG("Ro CCR initiated: direction:%.*s, charge_type:%.*s, unit_type:%.*s, reservation_units:%i",
			direction->len, direction->s,
			charge_type->len, charge_type->s,
			unit_type->len, unit_type->s,
			reservation_units);

	if (msg->REQ_METHOD != METHOD_INVITE)
		return RO_RETURN_FALSE;

	int ret = Ro_Send_CCR(msg, direction, charge_type, unit_type, reservation_units);

	if (ret != 0) {
		LM_DBG("RO_CCR failed\n");
		return RO_RETURN_FALSE;
	} else {
		LM_DBG("RO_CCR_success\n");
		return RO_RETURN_TRUE;
	}
}

/* this is the functino called when a we need to request more funds/credit. We need to try ansd reserve more credit. If we cant we need to put a new timer to kill
 the call at the appropriate time */

void ro_session_ontimeout(struct ro_tl *tl) {
	time_t now;
	time_t used_secs;
	time_t call_time;

	struct dlg_cell *dlg = 0;

	int new_credit = 0;
	int credit_valid_for = 0;
	unsigned int is_final_allocation = 0;

	LM_DBG("We have a fired timer and tl=[%i].\n", tl->timeout);

	/* find the session id for this timer*/
	struct ro_session_entry *ro_session_entry = 0;

	struct ro_session* ro_session = 0;
	ro_session = ((struct ro_session*) ((char *) (tl)
			- (unsigned long) (&((struct ro_session*) 0)->ro_tl)));

	if (!ro_session) {
		LM_ERR("cant find a session. This is bad");
		return;
	}

	ro_session_entry = &(ro_session_table->entries[ro_session->h_entry]);
	ro_session_lock(ro_session_table, ro_session_entry);

	switch (ro_session->event_type) {
	case answered:
		now = time(0);
		used_secs = now - ro_session->last_event_timestamp;
		call_time = now - ro_session->start_time;

		if (ro_session->callid.s != NULL
				&& ro_session->dlg_h_entry
						> 0&& ro_session->dlg_h_id > 0 && ro_session->ro_session_id.s != NULL) {LM_DBG
			("Found a session to reapply for timing [%.*s] and user is [%.*s]\n", ro_session->ro_session_id.len, ro_session->ro_session_id.s, ro_session->from_uri.len, ro_session->from_uri.s);

			LM_DBG("Call session has been active for %i seconds. this last reserved secs was [%i] and the last event was [%i seconds] ago",
					(unsigned int) call_time, (unsigned int) ro_session->reserved_secs, (unsigned int) used_secs);

			LM_DBG("Call session: we will now make a request for another [%i] of credit with a usage of [%i] seconds from the last bundle.\n",
					interim_request_credits/*INTERIM_CREDIT_REQ_AMOUNT*/, (unsigned int) used_secs);
			/* apply for more credit */
			send_ccr_interim(ro_session, &ro_session->from_uri,
					&ro_session->to_uri, &new_credit, &credit_valid_for,
					(unsigned int) used_secs,
					interim_request_credits/*INTERIM_CREDIT_REQ_AMOUNT*/,
					&is_final_allocation);

			/* check to make sure diameter server is giving us sane values */
			if (new_credit > credit_valid_for) {
				LM_WARN("That's weird, Diameter server gave us credit with a lower validity period :D. Setting reserved time to validity perioud instead \n");
				new_credit = credit_valid_for;
			}

			if (new_credit > 0) {
				//now insert the new timer
				ro_session->last_event_timestamp = time(0);
				ro_session->event_type = answered;
				ro_session->reserved_secs = new_credit;
				ro_session->valid_for = credit_valid_for;

				if (is_final_allocation) {
					LM_DBG("This is a final allocation and call will end in %i seconds\n", new_credit);
					ro_session->event_type = no_more_credit;
					insert_ro_timer(&ro_session->ro_tl, new_credit);
				} else {
					if (new_credit > ro_timer_buffer /*TIMEOUTBUFFER*/)
						insert_ro_timer(&ro_session->ro_tl,
								new_credit - ro_timer_buffer/*TIMEOUTBUFFER*/);
					else
						insert_ro_timer(&ro_session->ro_tl, new_credit);
				}
			} else {
				/* just put the timer back in with however many seconds are left (if any!!! in which case we need to kill */
				/* also update the event type to no_more_credit to save on processing the next time we get here */
				ro_session->event_type = no_more_credit;
				int whatsleft = ro_session->reserved_secs - used_secs;
				if (whatsleft <= 0) {
					LM_DBG("Immediately killing call due to no more credit\n");
					unsigned int i = 0;
					dlg = 0;//dlgb_p->lookup_dlg(ro_session->dlg_h_entry,
							//ro_session->dlg_h_id, &i);
					if (dlg) {
						//dlgb.terminate_dlg(dlg, NULL );
						//dlgb_p->unref_dlg(dlg, 1);
					}

				} else {
					LM_DBG("No more credit for user - letting call run out of money in [%i] seconds", whatsleft);
					insert_ro_timer(&ro_session->ro_tl, whatsleft);
				}
			}
		} else {
			LM_DBG("Hmmm, the session we have either dosent have all the data or something else has gone wrong.\n");
			/* put the timer back so the call will be killed accordign to previous timeout. */
			ro_session->event_type = unknown_error;
			insert_ro_timer(&ro_session->ro_tl,
					ro_session->reserved_secs - used_secs);

			LM_ERR("Immediately killing call due to unknown error\n");
			unsigned int i = 0;
//			dlg = dlgb_p->lookup_dlg(ro_session->dlg_h_entry,
//					ro_session->dlg_h_id, &i);
			if (dlg) {
//				dlgb_p->terminate_dlg(dlg, NULL );
//				dlgb_p->unref_dlg(dlg, 1);
			}
		}
		break;
	case no_more_credit:
		LM_DBG("Call/session must be ended - no more funds.\n");
	case unknown_error:
		LM_ERR("last event casused an error. We will now tear down this session.\n");
	default:
		LM_ERR("Diameter call session in unknown state.\n");
		unsigned int i = 0;
//		dlg = dlgb_p->lookup_dlg(ro_session->dlg_h_entry, ro_session->dlg_h_id,
//				&i);
		if (dlg) {
//			dlgb_p->terminate_dlg(dlg, NULL );
//			dlgb_p->unref_dlg(dlg, 1);
		}
	}
	ro_session_unlock(ro_session_table, ro_session_entry);

	return;
}

static int ro_fixup(void **param, int param_no) {
	str s;
	unsigned int num;

	if (param_no > 0 && param_no <= 3) {
		return fixup_var_str_12(param, param_no);
	} else if (param_no == 4) {
		/*convert to int */
		s.s = (char*)*param;
		s.len = strlen(s.s);
		if (str2int(&s, &num)==0) {
			pkg_free(*param);
			*param = (void*)(unsigned long)num;
			return 0;
		}
		LM_ERR("Bad reservation units: <%s>n", (char*)(*param));
		return E_CFG;
	}
	return 0;
}
