#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "mod.h"

#include "../../parser/parse_uri.h"
#include "../../sr_module.h"
#include "../../socket_info.h"
#include "../../timer.h"
#include "../../locking.h"
#include "../../modules/tm/tm_load.h"

#include "../../modules_s/dialog/dlg_mod.h"


#include "../cdp/cdp_load.h"
#include "../cdp_avp/mod_export.h"
#include "../../modules_k/dialog/dlg_load.h"
#include "../../modules_k/dialog/dlg_hash.h"
#include "../../mem/shm_mem.h"

#include "diameter_ro.h"
#include "ims_ro.h"
#include "config.h"
#include "ro_load.h"
#include "ro_timer.h"
#include "ro_fixup.h"
#include "ro_session_hash.h"


MODULE_VERSION

static int mod_init(void);
static int mod_child_init(int rank);
static void mod_destroy(void);

static int ro_session_hash_size = 4096;
static int ro_timer_buffer = 5;
extern int interim_request_credits;
int interim_request_credits = 30;

/* Global variables and imported functions */
cdp_avp_bind_t *cavpb = 0;

/**< link to the stateless reply function in sl module */
int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);

/**< Structure with pointers to tm funcs 		*/
struct tm_binds tmb;
struct dlg_binds dlgb;
extern struct dlg_binds *dlgb_p;

struct dlg_binds *dlgb_p = &dlgb;


/* parameters storage */
int * shutdown_singleton;
char * ro_origin_host_s = "test.ims.smilecoms.com";
char * ro_origin_realm_s = "ims.smilecoms.com";
char * ro_destination_realm_s = "ims.smilecoms.com";
char * ro_destination_host_s = "hss.ims.smilecoms.com";
char * ro_service_context_id_root_s = "32260@3gpp.org";
char * ro_service_context_id_ext_s = "ext";
char * ro_service_context_id_mnc_s = "01";
char * ro_service_context_id_mcc_s = "001";
char * ro_service_context_id_release_s = "8";
client_rf_cfg cfg;

/**
 * Exported functions.
 *
 */
static cmd_export_t client_ro_cmds[] = {
    {"Ro_Send_CCR", (cmd_function) Ro_Send_CCR, 1, ro_send_ccr_fixup, REQUEST_ROUTE},
    {"load_ro", (cmd_function) load_ro, NO_SCRIPT, 0, 0},
    {0, 0, 0, 0, 0}
};

/** 
 * Exported parameters.
 */
static param_export_t client_ro_params[] = {
    {"hash_size", INT_PARAM, &ro_session_hash_size},
    {"interim_update_credits", INT_PARAM, &interim_request_credits},
    {"timer_buffer", INT_PARAM, &ro_timer_buffer},
    {"node_functionality", INT_PARAM, &(cfg.node_func)},
    {"origin_host", STR_PARAM, &ro_origin_host_s},
    {"origin_realm", STR_PARAM, &ro_origin_realm_s},
    {"destination_realm", STR_PARAM, &ro_destination_realm_s},
    {"destination_host", STR_PARAM, &ro_destination_host_s},
    {"service_context_id_root", STR_PARAM, &ro_service_context_id_root_s},
    {"service_context_id_ext", STR_PARAM, &ro_service_context_id_ext_s},
    {"service_context_id_mnc", STR_PARAM, &ro_service_context_id_mnc_s},
    {"service_context_id_mcc", STR_PARAM, &ro_service_context_id_mcc_s},
    {"service_context_id_release", STR_PARAM, &ro_service_context_id_release_s},
    {0, 0, 0}
};

/** module exports */
struct module_exports exports = {
    "Client_Ro",
    client_ro_cmds,
    0,
    client_ro_params,
    mod_init, /* module initialization function */
    0, /* response function*/
    mod_destroy, /* destroy function */
    0,
    mod_child_init /* per-child init function */
};

static void dlg_reply(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
    struct sip_msg *reply = _params->msg;
    struct ro_session* session = 0;
    struct ro_session_entry* ro_session_entry;
    time_t now = time(0);
    time_t time_since_last_event;

    if (reply != FAKED_REPLY && reply->REPLY_STATUS == 200) {
        LM_DBG("Call answered - search for Ro Session and initialise timers.\n");

        /* find the session for this call*/
        if ((session = lookup_ro_session(dlg->h_entry, &dlg->callid, 0))) {

            ro_session_entry = &(ro_session_table->entries[session->h_entry]);
            ro_session_lock(ro_session_table, ro_session_entry);

            time_since_last_event = now - session->last_event_timestamp;
            session->start_time = session->last_event_timestamp = now;
            session->event_type = answered;

            ro_session_unlock(ro_session_table, ro_session_entry);

            /* check to make sure that the validity of the credit is enough for the bundle */
            if (session->reserved_secs < (session->valid_for - time_since_last_event)) {
                if (session->reserved_secs > ro_timer_buffer/*TIMEOUTBUFFER*/)
                    insert_ro_timer(&session->ro_tl, session->reserved_secs - ro_timer_buffer/*TIMEOUTBUFFER*/); //subtract 5 seconds so as to get more credit before we run out
                else
                    insert_ro_timer(&session->ro_tl, session->reserved_secs); //subtract 5 seconds so as to get more credit before we run out

            } else {
                if (session->valid_for > ro_timer_buffer/*TIMEOUTBUFFER*/)
                    insert_ro_timer(&session->ro_tl, session->valid_for - ro_timer_buffer/*TIMEOUTBUFFER*/); //subtract 5 seconds so as to get more credit before we run out

                else
                    insert_ro_timer(&session->ro_tl, session->valid_for);
            }
            unref_ro_session(session, 1);
        }
    }
}

static void dlg_terminated(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
    
    struct ro_session *ro_session = 0;
    struct ro_session_entry *ro_session_entry;

    if (!_params) {
        return;
    }

    struct sip_msg *request = _params->msg;
    if (!request) {
        LM_WARN("dlg_terminated has no SIP request associated.\n");
    }

    if (dlg && (dlg->callid.s && dlg->callid.len > 0)) {
        /* find the session for this call*/
        if ((ro_session = lookup_ro_session(dlg->h_entry, &dlg->callid, 0))) {
            ro_session_entry = &(ro_session_table->entries[ro_session->h_entry]);
            ro_session_lock(ro_session_table, ro_session_entry);
            if (ro_session->event_type != pending) {
                send_ccr_stop(ro_session);
            }
            ro_session_unlock(ro_session_table, ro_session_entry);
            unref_ro_session(ro_session, 2);
        }
    }
}

static void dlg_created(struct dlg_cell *dlg, int type, struct dlg_cb_params *_params) {
    str session_id = {0, 0};
    struct ro_session *ro_session = 0;
    LM_DBG("ro_client: new dlg created with id [%i:%i].\n", dlg->h_entry, dlg->h_id);


    struct sip_msg *request = _params->msg;

    if (request->REQ_METHOD != METHOD_INVITE)
        return;

    //create a session object without auth and diameter session id - we will add this later.
    ro_session = build_new_ro_session(0, 0, &session_id, &dlg->callid, &dlg->from_uri, &dlg->to_uri, dlg->h_entry, dlg->h_id, 0, 0);
    if (!ro_session) {
        LM_ERR("Couldnt create new Ro Session - this is BAD!\n");
        return;
    }
    link_ro_session(ro_session, 0);

    if (dlgb.register_dlgcb(dlg, DLGCB_RESPONSE_FWDED, dlg_reply, NULL, NULL) != 0)
        LOG(L_ERR, "cannot register callback for dialog confirmation\n");
    if (dlgb.register_dlgcb(dlg, DLGCB_TERMINATED | DLGCB_FAILED | DLGCB_EXPIRED | DLGCB_DESTROY, dlg_terminated, NULL, NULL) != 0)
        LOG(L_ERR, "cannot register callback for dialog termination\n");
}

/**
 * Fix the configuration parameters.
 */
int fix_parameters() {
    cfg.origin_host.s = ro_origin_host_s;
    cfg.origin_host.len = strlen(ro_origin_host_s);

    cfg.origin_realm.s = ro_origin_realm_s;
    cfg.origin_realm.len = strlen(ro_origin_realm_s);

    cfg.destination_realm.s = ro_destination_realm_s;
    cfg.destination_realm.len = strlen(ro_destination_realm_s);

    cfg.destination_host.s = ro_destination_host_s;
    cfg.destination_host.len = strlen(ro_destination_host_s);


    cfg.service_context_id = shm_malloc(sizeof (str));
    if (!cfg.service_context_id) {
        LM_ERR("ERR:"M_NAME":fix_parameters:not enough shm memory\n");
        return 0;
    }
    cfg.service_context_id->len = strlen(ro_service_context_id_ext_s) + strlen(ro_service_context_id_mnc_s) +
            strlen(ro_service_context_id_mcc_s) + strlen(ro_service_context_id_release_s) +
            strlen(ro_service_context_id_root_s) + 5;
    cfg.service_context_id->s = pkg_malloc(cfg.service_context_id->len * sizeof (char));
    if (!cfg.service_context_id->s) {
        LM_ERR("ERR:"M_NAME":fix_parameters: not enough memory!\n");
        return 0;
    }
    cfg.service_context_id->len = sprintf(cfg.service_context_id->s, "%s.%s.%s.%s.%s",
            ro_service_context_id_ext_s, ro_service_context_id_mnc_s,
            ro_service_context_id_mcc_s, ro_service_context_id_release_s,
            ro_service_context_id_root_s);
    if (cfg.service_context_id->len < 0) {
        LM_ERR("ERR:"M_NAME":fix_parameters: error while creating service_context_id\n");
        return 0;
    }

    return 1;
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
    ro_session = ((struct ro_session*) ((char *) (tl) - (unsigned long) (&((struct ro_session*) 0)->ro_tl)));

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

            if (ro_session->callid.s != NULL && ro_session->dlg_h_entry > 0 && ro_session->dlg_h_id > 0 && ro_session->ro_session_id.s != NULL) {
                LM_DBG("Found a session to reapply for timing [%.*s] and user is [%.*s]\n", ro_session->ro_session_id.len, ro_session->ro_session_id.s, ro_session->from_uri.len, ro_session->from_uri.s);

                LM_DBG("Call session has been active for %i seconds. this last reserved secs was [%i] and the last event was [%i seconds] ago",
                        (unsigned int) call_time, (unsigned int) ro_session->reserved_secs, (unsigned int) used_secs);

                LM_DBG("Call session: we will now make a request for another [%i] of credit with a usage of [%i] seconds from the last bundle.\n",
                        interim_request_credits/*INTERIM_CREDIT_REQ_AMOUNT*/, (unsigned int) used_secs);
                /* apply for more credit */
                send_ccr_interim(ro_session, &ro_session->from_uri, &ro_session->to_uri, &new_credit, &credit_valid_for, (unsigned int) used_secs, interim_request_credits/*INTERIM_CREDIT_REQ_AMOUNT*/, &is_final_allocation);

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
                            insert_ro_timer(&ro_session->ro_tl, new_credit - ro_timer_buffer/*TIMEOUTBUFFER*/);
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
                        dlg = dlgb_p->lookup_dlg(ro_session->dlg_h_entry, ro_session->dlg_h_id, &i);
                        if (dlg) {
                            dlgb_p->terminate_dlg(dlg, NULL);
                            dlgb_p->unref_dlg(dlg, 1);
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
                insert_ro_timer(&ro_session->ro_tl, ro_session->reserved_secs - used_secs);

                LM_ERR("Immediately killing call due to unknown error\n");
                unsigned int i = 0;
                dlg = dlgb_p->lookup_dlg(ro_session->dlg_h_entry, ro_session->dlg_h_id, &i);
                if (dlg) {
                    dlgb_p->terminate_dlg(dlg, NULL);
                    dlgb_p->unref_dlg(dlg, 1);
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
            dlg = dlgb_p->lookup_dlg(ro_session->dlg_h_entry, ro_session->dlg_h_id, &i);
            if (dlg) {
                dlgb_p->terminate_dlg(dlg, NULL);
                dlgb_p->unref_dlg(dlg, 1);
            }
    }
    ro_session_unlock(ro_session_table, ro_session_entry);

    return;
}

/**
 * Initializes the module.
 */
static int mod_init(void) {

    unsigned int n;
    load_tm_f load_tm;
    load_cdp_f load_cdp;
    load_dlg_f load_dlg;

    cdp_avp_get_bind_f load_cdp_avp;

    LM_INFO("INFO:"M_NAME":mod_init: Initialization of module\n");
    shutdown_singleton = shm_malloc(sizeof (int));
    *shutdown_singleton = 0;

    /* fix the parameters */
    if (!fix_parameters()) goto error;

    /* load the send_reply function from sl module */
    sl_reply = find_export("sl_send_reply", 2, 0);
    if (!sl_reply) {
        LM_ERR("ERR"M_NAME":mod_init: This module requires sl module\n");
        goto error;
    }

    /* bind to the tm module */
    if (!(load_tm = (load_tm_f) find_export("load_tm", NO_SCRIPT, 0))) {
        LM_ERR("ERR:"M_NAME":mod_init: Can not import load_tm. This module requires tm module\n");
        goto error;
    }
    if (load_tm(&tmb) == -1)
        goto error;

    if (!(load_cdp = (load_cdp_f) find_export("load_cdp", NO_SCRIPT, 0))) {
        LM_ERR("DBG:"M_NAME":mod_init: Can not import load_cdp. This module requires cdp module.\n");
        goto error;
    }

    if (!(load_cdp_avp = (cdp_avp_get_bind_f) find_export("cdp_avp_get_bind", NO_SCRIPT, 0))) {
        LM_ERR("DBG:"M_NAME":mod_init: loading cdp_avp module unsuccessful. exiting\n");
        goto error;
    }
    cavpb = load_cdp_avp();
    if (!cavpb)
        goto error;

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

    /* initialized the hash table */
    for (n = 0; n < (8 * sizeof (n)); n++) {
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

    /* bind to dialog module */
    if (!(load_dlg = (load_dlg_f) find_export("load_dlg", 0, 0))) {
        LM_ERR("mod_init: can not import load_dlg. This module requires Kamailio dialog moduile.\n");
    }

    if (load_dlg(&dlgb) == -1) {
        goto error;
    }
    // register dialog creation callback
    if (dlgb.register_dlgcb(NULL, DLGCB_CREATED, dlg_created, NULL, NULL) != 0) {
        LOG(L_CRIT, "cannot register callback for dialog creation\n");
        return -1;
    }

    return 0;
error:
    LM_ERR("Error in initialising module\n");
    return -1;
}

extern gen_lock_t* process_lock; /* lock on the process table */

/**
 * Initializes the module in child.
 */
static int mod_child_init(int rank) {
    LM_INFO("INFO:"M_NAME":mod_init: Initialization of module in child [%d] \n",
            rank);
    /* don't do anything for main process and TCP manager process */
    if (rank == PROC_MAIN || rank == PROC_TCP_MAIN)
        return 0;

    /*lock_get(process_lock);
    cavpb->cdp->AAAAddResponseHandler(RoChargingResponseHandler, NULL);
    lock_release(process_lock);*/

    return 0;
}

extern gen_lock_t* process_lock; /* lock on the process table */

/**
 * Destroys the module.
 */
static void mod_destroy(void) {
    int do_destroy = 0;
    LM_INFO("INFO:"M_NAME":mod_destroy: child exit\n");

    lock_get(process_lock);
    if ((*shutdown_singleton) == 0) {
        *shutdown_singleton = 1;
        do_destroy = 1;
    }
    lock_release(process_lock);

    destroy_dlg_table();

    if (do_destroy) {
        /* Then nuke it all */
    }

}

/**
 * Checks if the transaction is in processing.
 * @param msg - the SIP message to check
 * @param str1 - not used
 * @param str2 - not used
 * @returns #CSCF_RETURN_TRUE if the transaction is already in processing, #CSCF_RETURN_FALSE if not
 */
int trans_in_processing(struct sip_msg* msg, char* str1, char* str2) {
    unsigned int hash, label;
    if (tmb.t_get_trans_ident(msg, &hash, &label) < 0)
        return CSCF_RETURN_FALSE;
    return CSCF_RETURN_TRUE;
}
