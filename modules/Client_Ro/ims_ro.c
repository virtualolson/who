/*
 * $Id: ims_rf.c 1050 2011-03-23 16:56:26Z aon $
 *
 * Copyright (C) 2008-2009 FhG Fokus
 *
 * This file is part of Open IMS Core - an open source IMS CSCFs & HSS
 * implementation
 *
 * Open IMS Core is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * For a license to use the Open IMS Core software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact Fraunhofer FOKUS by e-mail at the following
 * addresses:
 *     info@open-ims.org
 *
 * Open IMS Core is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * It has to be noted that this Open Source IMS Core System is not 
 * intended to become or act as a product in a commercial context! Its 
 * sole purpose is to provide an IMS core reference implementation for 
 * IMS technology testing and IMS application prototyping for research 
 * purposes, typically performed in IMS test-beds.
 * 
 * Users of the Open Source IMS Core System have to be aware that IMS
 * technology may be subject of patents and licence terms, as being 
 * specified within the various IMS-related IETF, ITU-T, ETSI, and 3GPP
 * standards. Thus all Open IMS Core users have to take notice of this 
 * fact and have to agree to check out carefully before installing, 
 * using and extending the Open Source IMS Core System, if related 
 * patents and licences may become applicable to the intended usage 
 * context.  
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 * author Ancuta Corici, 
 * 	email andreea dot ancuta dot corici -at- fokus dot fraunhofer dot de
 */
/**
 * Client_Rf - implementation of the Rf interface from the CTF side, according to TS32.299 R7
 * 
 * Scope:
 * - Exports parameters and functions
 * - Initialization functions
 * 
 */

#include "mod.h"

#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../sr_module.h"
#include "../../socket_info.h"
#include "../../timer.h"
#include "../../locking.h"
#include "../../modules/tm/tm_load.h"

#include "../../modules_k/dialog/dlg_hash.h"
#include "../../modules_k/dialog/dlg_load.h"


#include "../cdp/cdp_load.h"
#include "../cdp_avp/mod_export.h"
#include "../../mod_fix.h"

#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"

#include "diameter_ro.h"
#include "ims_ro.h"
#include "Ro_data.h"
#include "sip.h"
#include "ccr.h"
#include "config.h"
#include "ro_session_hash.h"

extern struct tm_binds tmb;
extern cdp_avp_bind_t *cavpb;
extern client_rf_cfg cfg;

struct dlg_binds* dlgb_p;

int interim_request_credits;

/**
 * Retrieves the SIP request that generated a diameter transaction
 * @param hash - the tm hash value for this request
 * @param label - the tm label value for this request
 * @returns the SIP request
 */
struct sip_msg * trans_get_request_from_current_reply() {
    struct cell *t;
    t = tmb.t_gett();
    if (!t || t == (void*) - 1) {
        LM_ERR("ERR:"M_NAME":trans_get_request_from_current_reply: Reply without transaction\n");
        return 0;
    }
    if (t) return t->uas.request;
    else return 0;
}

/**
 * Create and add an AVP to a Diameter message.
 * @param m - Diameter message to add to
 * @param d - the payload data
 * @param len - length of the payload data
 * @param avp_code - the code of the AVP
 * @param flags - flags for the AVP
 * @param vendorid - the value of the vendor id or 0 if none
 * @param data_do - what to do with the data when done
 * @param func - the name of the calling function, for debugging purposes
 * @returns 1 on success or 0 on failure
 */
static inline int Ro_add_avp(AAAMessage *m, char *d, int len, int avp_code, int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cavpb->cdp->AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LM_ERR("ERR:"M_NAME":%s: Failed creating avp\n", func);
        return 0;
    }
    if (cavpb->cdp->AAAAddAVPToMessage(m, avp, m->avpList.tail) != AAA_ERR_SUCCESS) {
        LM_ERR("ERR:"M_NAME":%s: Failed adding avp to message\n", func);
        cavpb->cdp->AAAFreeAVP(&avp);
        return 0;
    }
    return 1;
}

/**
 * Create and add an AVP to a list of AVPs.
 * @param list - the AVP list to add to
 * @param d - the payload data
 * @param len - length of the payload data
 * @param avp_code - the code of the AVP
 * @param flags - flags for the AVP
 * @param vendorid - the value of the vendor id or 0 if none
 * @param data_do - what to do with the data when done
 * @param func - the name of the calling function, for debugging purposes
 * @returns 1 on success or 0 on failure
 */
static inline int Ro_add_avp_list(AAA_AVP_LIST *list, char *d, int len, int avp_code,
        int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cavpb->cdp->AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LOG(L_ERR, "ERR:"M_NAME":%s: Failed creating avp\n", func);
        return 0;
    }
    if (list->tail) {
        avp->prev = list->tail;
        avp->next = 0;
        list->tail->next = avp;
        list->tail = avp;
    } else {
        list->head = avp;
        list->tail = avp;
        avp->next = 0;
        avp->prev = 0;
    }

    return 1;
}

inline int Ro_add_cc_request(AAAMessage *msg, unsigned int cc_request_type, unsigned int cc_request_number) {
    char x[4];
    set_4bytes(x, cc_request_type);
    int success = Ro_add_avp(msg, x, 4, AVP_CC_Request_Type, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    char y[4];
    set_4bytes(y, cc_request_number);

    return success && Ro_add_avp(msg, y, 4, AVP_CC_Request_Number, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

}

inline int Ro_add_event_timestamp(AAAMessage *msg, time_t now) {
    char x[4];
    str s = {x, 4};
    uint32_t ntime = htonl(now + EPOCH_UNIX_TO_EPOCH_NTP);
    memcpy(x, &ntime, sizeof (uint32_t));

    return Ro_add_avp(msg, s.s, s.len, AVP_Event_Timestamp, AAA_AVP_FLAG_NONE, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

}

inline int Ro_add_user_equipment_info(AAAMessage *msg, unsigned int type, str value) {
    AAA_AVP_LIST list;
    str group;
    char x[4];

    list.head = 0;
    list.tail = 0;

    set_4bytes(x, type);
    Ro_add_avp_list(&list, x, 4, AVP_User_Equipment_Info_Type, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    Ro_add_avp_list(&list, value.s, value.len, AVP_User_Equipment_Info_Value, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    group = cavpb->cdp->AAAGroupAVPS(list);

    cavpb->cdp->AAAFreeAVPList(&list);

    return Ro_add_avp(msg, group.s, group.len, AVP_User_Equipment_Info, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

inline int Ro_add_termination_casue(AAAMessage *msg, unsigned int term_code) {
    char x[4];
    str s = {x, 4};
    uint32_t code = htonl(term_code);
    memcpy(x, &code, sizeof (uint32_t));

    return Ro_add_avp(msg, s.s, s.len, AVP_Termination_Cause, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
}

/* called only when building stop record AVPS */
inline int Ro_add_multiple_service_credit_Control_stop(AAAMessage *msg, int used_unit) {
    AAA_AVP_LIST used_list, mscc_list;
    str used_group;
    char x[4];

    used_list.head = 0;
    used_list.tail = 0;
    mscc_list.head = 0;
    mscc_list.tail = 0;

    /* if we must Used-Service-Unit */
    if (used_unit >= 0) {
        set_4bytes(x, used_unit);
        Ro_add_avp_list(&used_list, x, 4, AVP_CC_Time, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
        used_group = cavpb->cdp->AAAGroupAVPS(used_list);
        cavpb->cdp->AAAFreeAVPList(&used_list);
        Ro_add_avp_list(&mscc_list, used_group.s, used_group.len, AVP_Used_Service_Unit, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    }

    used_group = cavpb->cdp->AAAGroupAVPS(mscc_list);
    cavpb->cdp->AAAFreeAVPList(&mscc_list);

    return Ro_add_avp(msg, used_group.s, used_group.len, AVP_Multiple_Services_Credit_Control, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

inline int Ro_add_multiple_service_credit_Control(AAAMessage *msg, unsigned int requested_unit, int used_unit) {
    AAA_AVP_LIST list, used_list, mscc_list;
    str group, used_group;
    char x[4];

    list.head = 0;
    list.tail = 0;
    used_list.head = 0;
    used_list.tail = 0;
    mscc_list.head = 0;
    mscc_list.tail = 0;

    set_4bytes(x, requested_unit);
    Ro_add_avp_list(&list, x, 4, AVP_CC_Time, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    group = cavpb->cdp->AAAGroupAVPS(list);
    cavpb->cdp->AAAFreeAVPList(&list);



    Ro_add_avp_list(&mscc_list, group.s, group.len, AVP_Requested_Service_Unit, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    /* if we must Used-Service-Unit */
    if (used_unit >= 0) {
        set_4bytes(x, used_unit);
        Ro_add_avp_list(&used_list, x, 4, AVP_CC_Time, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
        used_group = cavpb->cdp->AAAGroupAVPS(used_list);
        cavpb->cdp->AAAFreeAVPList(&used_list);
        Ro_add_avp_list(&mscc_list, used_group.s, used_group.len, AVP_Used_Service_Unit, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    }

    group = cavpb->cdp->AAAGroupAVPS(mscc_list);
    cavpb->cdp->AAAFreeAVPList(&mscc_list);

    return Ro_add_avp(msg, group.s, group.len, AVP_Multiple_Services_Credit_Control, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

inline int Ro_add_subscription_id(AAAMessage *msg, unsigned int type, str *subscription_id)//, struct sip_msg* sip_msg)
{
    AAA_AVP_LIST list;
    str group;
    str from_sip_uri = str_init("sip:jason@ims.smilecoms.com"); //TODO fix
    char x[4];

    list.head = 0;
    list.tail = 0;

    set_4bytes(x, type);
    Ro_add_avp_list(&list, x, 4, AVP_Subscription_Id_Type, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    Ro_add_avp_list(&list, from_sip_uri.s, from_sip_uri.len, AVP_Subscription_Id_Data, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    group = cavpb->cdp->AAAGroupAVPS(list);

    cavpb->cdp->AAAFreeAVPList(&list);

    return Ro_add_avp(msg, group.s, group.len, AVP_Subscription_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

/**
 * Creates and adds a Vendor-Specifig-Application-ID AVP.
 * @param msg - the Diameter message to add to.
 * @param vendor_id - the value of the vendor_id,
 * @param auth_id - the authorization application id
 * @param acct_id - the accounting application id
 * @returns 1 on success or 0 on error
 */
inline int Ro_add_vendor_specific_appid(AAAMessage *msg, unsigned int vendor_id, unsigned int auth_id, unsigned int acct_id) {
    AAA_AVP_LIST list;
    str group;
    char x[4];

    list.head = 0;
    list.tail = 0;

    set_4bytes(x, vendor_id);
    Ro_add_avp_list(&list, x, 4, AVP_Vendor_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);

    if (auth_id) {
        set_4bytes(x, auth_id);
        Ro_add_avp_list(&list, x, 4, AVP_Auth_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    }
    if (acct_id) {
        set_4bytes(x, acct_id);
        Ro_add_avp_list(&list, x, 4, AVP_Acct_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_DUPLICATE_DATA, __FUNCTION__);
    }

    group = cavpb->cdp->AAAGroupAVPS(list);

    cavpb->cdp->AAAFreeAVPList(&list);

    return Ro_add_avp(msg, group.s, group.len, AVP_Vendor_Specific_Application_Id, AAA_AVP_FLAG_MANDATORY, 0, AVP_FREE_DATA, __FUNCTION__);
}

int get_sip_header_info(struct sip_msg * req,
        struct sip_msg * reply,
        int32_t * acc_record_type,
        str * sip_method,
        str * event, uint32_t * expires,
        str * callid, str * from_uri, str * to_uri) {

    LM_ERR("retrieving sip info\n");
    sip_method->s = req->first_line.u.request.method.s;
    sip_method->len = req->first_line.u.request.method.len;

    if (strncmp(sip_method->s, "INVITE", 6) == 0)
        *acc_record_type = AAA_ACCT_START;
    else if (strncmp(sip_method->s, "BYE", 3) == 0)
        *acc_record_type = AAA_ACCT_STOP;
    else
        *acc_record_type = AAA_ACCT_EVENT;

    *event = cscf_get_event(req);
    *expires = cscf_get_expires_hdr(req, 0);
    *callid = cscf_get_call_id(req, NULL);

    if (!cscf_get_from_uri(req, from_uri))
        goto error;

    if (!cscf_get_to_uri(req, to_uri))
        goto error;

    LM_ERR("retrieved sip info : sip_method %.*s acc_record_type %i, event %.*s expires %u "
            "call_id %.*s from_uri %.*s to_uri %.*s\n",
            sip_method->len, sip_method->s, *acc_record_type, event->len, event->s, *expires,
            callid->len, callid->s, from_uri->len, from_uri->s, to_uri->len, to_uri->s);

    return 1;
error:
    return 0;
}

int get_ims_charging_info(struct sip_msg *req, struct sip_msg * reply, str * icid, str * orig_ioi, str * term_ioi) {

    LM_ERR("get ims charging info\n");
    if (req)
        cscf_get_p_charging_vector(req, icid, orig_ioi, term_ioi);
    if (reply)
        cscf_get_p_charging_vector(reply, icid, orig_ioi, term_ioi);

    return 1;
}

int get_timestamps(struct sip_msg * req, struct sip_msg * reply, time_t * req_timestamp, time_t * reply_timestamp) {

    if (reply)
        *reply_timestamp = time(NULL);
    if (req)
        *req_timestamp = time(NULL);
    return 1;
}

/*
 * creates the rf session for a session establishment
 * @param aor: the aor to be handled
 * @return: 0 - ok, -1 - error, -2 - out of memory
 */

Ro_CCR_t * dlg_create_ro_session(struct sip_msg * req, struct sip_msg * reply, AAASession ** authp, int dir) {

    Ro_CCR_t * ro_ccr_data = 0;
    AAASession * auth = NULL;
    str user_name/* ={0,0}*/, sip_method = {0, 0}, event = {0, 0};
    uint32_t expires = 0;
    str callid = {0, 0}, to_uri = {0, 0}, from_uri = {0, 0},
    icid = {0, 0}, orig_ioi = {0, 0}, term_ioi = {0, 0};

    event_type_t * event_type = 0;
    ims_information_t * ims_info = 0;
    time_stamps_t * time_stamps = 0;
    time_t req_timestamp = 0, reply_timestamp = 0;
    int32_t acc_record_type;
    subscription_id_t subscr;

    *authp = 0;

    if (!get_sip_header_info(req, reply, &acc_record_type, &sip_method, &event, &expires, &callid, &from_uri, &to_uri))
        goto error;
    if (dir == 0) {
        user_name.s = ("sip:jason@ims.smilecoms.com"); //from_uri;
        user_name.len = strlen(user_name.s);
    } else {
        user_name.s = ("sip:jason@ims.smilecoms.com"); //to_uri;
        user_name.len = strlen(user_name.s);
    }

    /*	if(!get_ims_charging_info(req, reply, &icid, &orig_ioi, &term_ioi))
                    goto error;
     */
    LM_ERR("retrieved ims charging info icid %.*s orig_ioi %.*s term_ioi %.*s\n", icid.len, icid.s, orig_ioi.len, orig_ioi.s, term_ioi.len, term_ioi.s);

    if (!get_timestamps(req, reply, &req_timestamp, &reply_timestamp))
        goto error;

    if (!(event_type = new_event_type(&sip_method, &event, &expires)))
        goto error;

    if (!(time_stamps = new_time_stamps(&req_timestamp, NULL, &reply_timestamp, NULL)))
        goto error;

    str tmp_from = str_init("sip:jason@ims.smilecoms.com");
    str tmp_to = str_init("sip:27832837000@ims.smilecoms.com");
    if (!(ims_info = new_ims_information(event_type, time_stamps, &callid, &callid, &tmp_from/*&from_uri*/, &tmp_to/*&to_uri*/, &icid, &orig_ioi, &term_ioi, dir)))
        goto error;
    event_type = 0;
    time_stamps = 0;

    subscr.type = Subscription_Type_IMPU;
    subscr.id.s = "sip:jason@ims.smilecoms.com"; //from_uri.s;
    subscr.id.len = strlen("sip:jason@ims.smilecoms.com"); //from_uri.len;

    ro_ccr_data = new_Ro_CCR(acc_record_type, &user_name, ims_info, &subscr);
    if (!ro_ccr_data) {
        LM_ERR("ERR:"M_NAME":dlg_create_ro_session: no memory left for generic\n");
        goto out_of_memory;
    }
    ims_info = 0;

    if (strncmp(req->first_line.u.request.method.s, "INVITE", 6) == 0) {
        auth = cavpb->cdp->AAACreateClientAuthSession(1, NULL, (void *) ro_ccr_data);
        LM_INFO("INFO:"M_NAME":dlg_create_ro_session: created Ro Session with id [%*.s]\n", auth->id.len, auth->id.s);
        //save_session = auth->id;

    }
    /*if (strncmp(req->first_line.u.request.method.s, "BYE", 3) == 0) {
        auth = cavpb->cdp->AAAGetAuthSession(save_session);
    }*/


    if (!auth) {
        LM_ERR("INFO:"M_NAME":dlg_create_ro_session: unable to create the Ro Session\n");
        goto error;
    }

    *authp = auth;
    return ro_ccr_data;

out_of_memory:
    error :
            time_stamps_free(time_stamps);
    event_type_free(event_type);
    ims_information_free(ims_info);
    Ro_free_CCR(ro_ccr_data);
    return NULL;
}

int sip_create_ro_ccr_data(struct sip_msg * msg, int dir, Ro_CCR_t ** ro_ccr_data, AAASession ** auth) {

    LM_DBG("creating ro data and message is %i\n", msg->first_line.type);

    if (msg->first_line.type == SIP_REQUEST) {
        /*end of session*/
        if (strncmp(msg->first_line.u.request.method.s, "INVITE", 6) == 0) {
            if (!(*ro_ccr_data = dlg_create_ro_session(msg, NULL, auth, dir)))
                goto error;
        }
    } else {
        goto error; //We only support Request (INVITE) messages on this interface
    }

    return 1;
error:
    return 0;
}

void send_ccr_interim(struct ro_session* ro_session, str* from_uri, str *to_uri, int *new_credit, int *credit_valid_for, unsigned int used, unsigned int reserve, unsigned int *is_final_allocation) {

    AAASession * auth = 0;
    Ro_CCR_t * ro_ccr_data = 0;
    AAAMessage * acr = 0;
    Ro_CCA_t *ro_cca_data = 0;
    ims_information_t *ims_info = 0;
    int32_t acc_record_type;
    subscription_id_t subscr;
    time_stamps_t *time_stamps;

    event_type_t *event_type;
    int node_role = 0;

    str sip_method = str_init("dummy");
    str sip_event = str_init("dummy");

    time_t req_timestamp;

    event_type = new_event_type(&sip_method, &sip_event, 0);

    LM_DBG("Sending interim CCR request for (usage:new) [%i:%i] seconds for user [%.*s] using session id [%.*s]", used, reserve, from_uri->len, from_uri->s, ro_session->ro_session_id.len, ro_session->ro_session_id.s);

    req_timestamp = time(0);

    if (!(time_stamps = new_time_stamps(&req_timestamp, NULL, NULL, NULL)))
        goto error;

    if (!(ims_info = new_ims_information(event_type, time_stamps, &ro_session->callid, &ro_session->callid, &ro_session->from_uri, &ro_session->to_uri, 0, 0, 0, node_role)))
        goto error;
    LM_DBG("Created IMS information\n");

    event_type = 0;

    subscr.type = Subscription_Type_IMPU;
    subscr.id.s = "sip:jason@ims.smilecoms.com"; //from_uri.s;  /*TODO: hardwired for testing*/
    subscr.id.len = strlen("sip:jason@ims.smilecoms.com"); //from_uri.len;

    acc_record_type = AAA_ACCT_INTERIM;

    ro_ccr_data = new_Ro_CCR(acc_record_type, from_uri, ims_info, &subscr);
    if (!ro_ccr_data) {
        LM_ERR("ERR:"M_NAME":dlg_create_ro_session: no memory left for generic\n");
        goto out_of_memory;
    }
    ims_info = 0;

    LM_DBG("Created Ro data\n");

    auth = cavpb->cdp->AAAGetAuthSession(ro_session->ro_session_id);

    if (!ro_ccr_data)
        goto error;

    if (!auth) {
        LM_DBG("Diameter Auth Session has timed out.... creating a new one.\n");
        /* lets try and recreate this session */
        auth = cavpb->cdp->AAAMakeSession(ro_session->auth_appid, ro_session->auth_session_type, ro_session->ro_session_id); //TODO: would like this session to last longer (see session timeout in cdp
        if (!auth)
            goto error;
    }


    if (!(acr = Ro_new_ccr(auth, ro_ccr_data)))
        goto error;
    if (!Ro_add_vendor_specific_appid(acr, IMS_vendor_id_3GPP, IMS_Ro, 0 /*IMS_Ro*/)) {
        LM_ERR("Problem adding Vendor specific ID\n");
    }
    ro_session->hop_by_hop += 1;
    if (!Ro_add_cc_request(acr, RO_CC_INTERIM, ro_session->hop_by_hop)) {
        LM_ERR("Problem adding CC-Request data\n");
    }
    if (!Ro_add_event_timestamp(acr, time(NULL))) {
        LM_ERR("Problem adding Event-Timestamp data\n");
    }
    str mac;
    mac.s = "00:00:00:00:00:00";
    mac.len = strlen(mac.s); //TODO - this is terrible

    if (!Ro_add_user_equipment_info(acr, AVP_EPC_User_Equipment_Info_Type_MAC, mac)) {
        LM_ERR("Problem adding User-Equipment data\n");
    }

    //str sip_uri;
    //sip_uri.s = "sip:jason@ims.smilecoms.com";
    //sip_uri.len = strlen(sip_uri.s);
    if (!Ro_add_subscription_id(acr, AVP_EPC_Subscription_Id_Type_End_User_SIP_URI, from_uri)) {
        LM_ERR("Problem adding Subscription ID data\n");
    }

    if (!Ro_add_multiple_service_credit_Control(acr, interim_request_credits/*INTERIM_CREDIT_REQ_AMOUNT*/, used)) {
        LM_ERR("Problem adding Multiple Service Credit Control data\n");
    }

    LM_DBG("Sending CCR Diameter message (Synchronously).\n");
    /* send sunchronously so we can respond to callplan (cfg file), so a decision can be made to process the invite */
    cavpb->cdp->AAASessionsUnlock(auth->hash);
    AAAMessage *cca = cavpb->cdp->AAASendRecvMessageToPeer(acr, &cfg.destination_host);

    /* check response */

    if (cca == NULL) {
        LM_ERR("Error reserving credit for CCA.\n");
        goto error_no_cca;
    }

    ro_cca_data = Ro_parse_CCA_avps(cca);

    if (ro_cca_data == NULL) {
        LM_ERR("Could not parse CCA message response.\n");
        goto error;
    }
    if (ro_cca_data->resultcode != 2001) {
        LM_ERR("Got bad CCA result code - reservation failed");
        goto error;
    } else {
        LM_DBG("Valid CCA response with time chunk of [%i] and validity [%i].\n", ro_cca_data->mscc->granted_service_unit->cc_time, ro_cca_data->mscc->validity_time);
    }

    *new_credit = ro_cca_data->mscc->granted_service_unit->cc_time;
    *credit_valid_for = ro_cca_data->mscc->validity_time;

    if (ro_cca_data->mscc->final_unit_action && (ro_cca_data->mscc->final_unit_action->action == 0)) {
        *is_final_allocation = 1;
    }

    Ro_free_CCR(ro_ccr_data);
    Ro_free_CCA(ro_cca_data);
    return;

out_of_memory:
    error :
            Ro_free_CCA(ro_cca_data);
error_no_cca:
    *new_credit = *credit_valid_for = 0;
    LM_ERR("error trying to reserve interim credit\n");
    Ro_free_CCR(ro_ccr_data);

    if (auth) {
        cavpb->cdp->AAASessionsUnlock(auth->hash);
        cavpb->cdp->AAADropSession(auth);
    }
    return;
}

void send_ccr_stop(struct ro_session *ro_session) {
    AAASession * auth = 0;
    Ro_CCR_t * ro_ccr_data = 0;
    AAAMessage * acr = 0;
    Ro_CCA_t *ro_cca_data = 0;
    ims_information_t *ims_info = 0;
    int32_t acc_record_type;
    subscription_id_t subscr;
    time_stamps_t *time_stamps;
    //time_stamps_t *ptime_stamps = &time_stamps;
    unsigned int used = 0;

    if (ro_session->event_type != pending) {
        used = time(0) - ro_session->last_event_timestamp;
    }

    event_type_t *event_type;
    int node_role = 0;

    str sip_method = str_init("dummy");
    str sip_event = str_init("dummy");

    time_t req_timestamp;

    event_type = new_event_type(&sip_method, &sip_event, 0);

    LM_DBG("Sending CCR STOP request for for user [%.*s] using session id [%.*s]", ro_session->from_uri.len, ro_session->from_uri.s, ro_session->ro_session_id.len, ro_session->ro_session_id.s);

    req_timestamp = time(0);

    if (!(time_stamps = new_time_stamps(&req_timestamp, NULL, NULL, NULL)))
        goto error;

    if (!(ims_info = new_ims_information(event_type, time_stamps, &ro_session->callid, &ro_session->callid, &ro_session->from_uri, &ro_session->to_uri, 0, 0, 0, node_role)))
        goto error;
    
    event_type = 0;

    subscr.type = Subscription_Type_IMPU;
    subscr.id.s = "sip:jason@ims.smilecoms.com"; //from_uri.s;
    subscr.id.len = strlen("sip:jason@ims.smilecoms.com"); //from_uri.len;

    acc_record_type = AAA_ACCT_STOP;

    ro_ccr_data = new_Ro_CCR(acc_record_type, &ro_session->from_uri, ims_info, &subscr);
    if (!ro_ccr_data) {
        LM_ERR("ERR:"M_NAME":dlg_create_ro_session: no memory left for generic\n");
        goto out_of_memory;
    }
    ims_info = 0;

    LM_DBG("Created Ro data\n");

    auth = cavpb->cdp->AAAGetAuthSession(ro_session->ro_session_id);

    if (!ro_ccr_data)
        goto error;

    if (!auth) {
        LM_DBG("Diameter Auth Session has timed out.... creating a new one.\n");
        /* lets try and recreate this session */
        auth = cavpb->cdp->AAAMakeSession(ro_session->auth_appid, ro_session->auth_session_type, ro_session->ro_session_id); //TODO: would like this session to last longer (see session timeout in cdp
        if (!auth)
            goto error;
    }


    if (!(acr = Ro_new_ccr(auth, ro_ccr_data)))
        goto error;

    LM_DBG("Created new CCR\n");

    if (!Ro_add_vendor_specific_appid(acr, IMS_vendor_id_3GPP, IMS_Ro, 0 /*IMS_Ro*/)) {
        LM_ERR("Problem adding Vendor specific ID\n");
    }
   
    ro_session->hop_by_hop += 1;
    if (!Ro_add_cc_request(acr, RO_CC_STOP, ro_session->hop_by_hop)) {
        LM_ERR("Problem adding CC-Request data\n");
    }
   
    if (!Ro_add_event_timestamp(acr, time(NULL))) {
        LM_ERR("Problem adding Event-Timestamp data\n");
    }
   
    str mac;
    mac.s = "00:00:00:00:00:00"; /*TODO: this is just a hack becuase we dont use this avp right now - if yuo like you can get the mac or some other info */
    mac.len = strlen(mac.s);

    if (!Ro_add_user_equipment_info(acr, AVP_EPC_User_Equipment_Info_Type_MAC, mac)) {
        LM_ERR("Problem adding User-Equipment data\n");
    }
    
    if (!Ro_add_subscription_id(acr, AVP_EPC_Subscription_Id_Type_End_User_SIP_URI, &ro_session->from_uri)) {
        LM_ERR("Problem adding Subscription ID data\n");
    }
    
    if (!Ro_add_multiple_service_credit_Control_stop(acr, used)) {
        LM_ERR("Problem adding Multiple Service Credit Control data\n");
    }
    
    if (!Ro_add_termination_casue(acr, 4)) {
        LM_ERR("problem add Termination cause AVP to STOP record.\n");
    }

    LM_DBG("Sending CCR Diameter message.\n");
    /* send sunchronously so we can respond to callplan (cfg file), so a decision can be made to process the invite */
    cavpb->cdp->AAASessionsUnlock(auth->hash);
    AAAMessage *cca = cavpb->cdp->AAASendRecvMessageToPeer(acr, &cfg.destination_host);

    /* check response */
    if (cca == NULL) {
        LM_ERR("Error reserving credit for CCA.\n");
        goto error_no_cca;
    }

    ro_cca_data = Ro_parse_CCA_avps(cca);

    if (ro_cca_data == NULL) {
        LM_DBG("Could not parse CCA message response.\n");
        goto error;
    }
    if (ro_cca_data->resultcode != 2001) {
        LM_ERR("Got bad CCA result code - reservation failed");
        goto error;
    } else {
        LM_DBG("Valid CCA response with time chunk of [%i] and validity [%i].\n", ro_cca_data->mscc->granted_service_unit->cc_time, ro_cca_data->mscc->validity_time);
    }

    Ro_free_CCR(ro_ccr_data);
    Ro_free_CCA(ro_cca_data);
    return;

out_of_memory:
    error :
            Ro_free_CCA(ro_cca_data);
error_no_cca:
    LM_ERR("error Ro STOP record\n");
    Ro_free_CCR(ro_ccr_data);

    if (auth) {
        cavpb->cdp->AAASessionsUnlock(auth->hash);
        cavpb->cdp->AAADropSession(auth);
    }
    return;

}

/**
 * Send a CCR to the OCS based on the SIP message (INVITE ONLY)
 * @param msg - SIP message
 * @param str1 - number of seconds to reserve
 * @param str2 - not used
 * @returns #CSCF_RETURN_TRUE if OK, #CSCF_RETURN_ERROR on error
 */
int Ro_Send_CCR(struct sip_msg *msg, char *reserve_time) {

    int reserve_secs = 0;
    AAASession * auth = 0;
    Ro_CCR_t * ro_ccr_data = 0;
    AAAMessage * acr = 0;
    int dir = 0;
    Ro_CCA_t *ro_cca_data = 0;
    struct ro_session *new_session = 0;
    struct ro_session_entry *ro_session_entry = 0;

    struct dlg_cell* dlg = 0;
    unsigned int _dir = 0;
    unsigned int _del = 0;

    /* lets see if a session has actually been created for us */
    dlg = dlgb_p->get_dlg(&msg->callid->body, &get_from(msg)->tag_value, &get_to(msg)->tag_value, &_dir, &_del);
    if (dlg != 0) {
        /* now lets get our session object */
        new_session = lookup_ro_session(dlg->h_entry, &msg->callid->body, 0);
        dlgb_p->unref_dlg(dlg, 1);

        if (!new_session) {
            //no point in continuing
            goto error;
        }

        /* lock the session */
        ro_session_entry = &(ro_session_table->entries[new_session->h_entry]);
        ro_session_lock(ro_session_table, ro_session_entry);
    } else {
        //no point continuing
        goto error;
    }

    if (get_int_fparam(&reserve_secs, msg, (fparam_t*) reserve_time) < 0) {
        LM_ERR("invalid requested time value\n");
        goto error;
    }

    int cc_event_number = 1;
    int cc_event_type = RO_CC_START;
    if (strncmp(msg->first_line.u.request.method.s, "INVITE", 6) != 0) {
        goto error; //function should only be called with an INVITE message
    }
    LM_DBG("Initial invite has requested %i seconds.\n", reserve_secs);

    if (!sip_create_ro_ccr_data(msg, dir, &ro_ccr_data, &auth))
        goto error;

    if (!ro_ccr_data)
        goto error;

    if (!auth) goto error;

    if (!(acr = Ro_new_ccr(auth, ro_ccr_data)))
        goto error;

    if (!Ro_add_vendor_specific_appid(acr, IMS_vendor_id_3GPP, IMS_Ro, 0 /*IMS_Ro*/)) {
        LM_ERR("Problem adding Vendor specific ID\n");
    }
    if (!Ro_add_cc_request(acr, cc_event_type, cc_event_number)) {
        LM_ERR("Problem adding CC-Request data\n");
    }
    if (!Ro_add_event_timestamp(acr, time(NULL))) {
        LM_ERR("Problem adding Event-Timestamp data\n");
    }
    str mac; //TODO - this is terrible
    mac.s = "00:00:00:00:00:00";
    mac.len = strlen(mac.s);

    if (!Ro_add_user_equipment_info(acr, AVP_EPC_User_Equipment_Info_Type_MAC, mac)) {
        LM_ERR("Problem adding User-Equipment data\n");
    }

    //get sip from URI for request
    str from_sip_uri;

    if (!cscf_get_from_uri(msg, &from_sip_uri)) {//TODO fix
        return 0;
    }
    if (!Ro_add_subscription_id(acr, AVP_EPC_Subscription_Id_Type_End_User_SIP_URI, &from_sip_uri)) {
        LM_ERR("Problem adding Subscription ID data\n");
    }
    if (!Ro_add_multiple_service_credit_Control(acr, reserve_secs, -1)) {
        LM_ERR("Problem adding Multiple Service Credit Control data\n");
    }

    /* before we send, update our session object with auth and session id */
    new_session->auth_appid = auth->application_id;
    new_session->auth_session_type = auth->type;
    new_session->ro_session_id.s = (char*) shm_malloc(auth->id.len);
    new_session->ro_session_id.len = auth->id.len;
    memcpy(new_session->ro_session_id.s, auth->id.s, auth->id.len);

    LM_DBG("Sending CCR Diameter message.\n");
    /* send sunchronously so we can respond to callplan (cfg file), so a decision can be made to process the invite */
    cavpb->cdp->AAASessionsUnlock(auth->hash);
    AAAMessage *cca = cavpb->cdp->AAASendRecvMessageToPeer(acr, &cfg.destination_host);
    
    if (cca == NULL) {
        LM_ERR("Error reserving credit for CCA.\n");
        goto error;
    }
    ro_cca_data = Ro_parse_CCA_avps(cca);

    if (ro_cca_data == NULL) {
        LM_ERR("Could not parse CCA message response.\n");
        goto error;
    }
    if (ro_cca_data->resultcode != 2001) {
        LM_ERR("Got bad CCA result code - reservation failed");
        goto error;
    }

    LM_DBG("Valid CCA response with time chunk of [%i] and validity [%i].\n", ro_cca_data->mscc->granted_service_unit->cc_time, ro_cca_data->mscc->validity_time);

    new_session->last_event_timestamp = time(0);
    new_session->event_type = pending;
    new_session->reserved_secs = ro_cca_data->mscc->granted_service_unit->cc_time;
    new_session->valid_for = ro_cca_data->mscc->validity_time;

    Ro_free_CCR(ro_ccr_data);
    Ro_free_CCA(ro_cca_data);


    ro_session_unlock(ro_session_table, ro_session_entry);
    unref_ro_session(new_session, 1);

    cavpb->cdp->AAASessionsUnlock(auth->hash);
    return CSCF_RETURN_TRUE;
error:
    LM_ERR("Ro_Send_CCR: Error sending request\n");
    if (new_session) {
        ro_session_unlock(ro_session_table, ro_session_entry);
        unref_ro_session(new_session, 1);
    }

    Ro_free_CCR(ro_ccr_data);
    Ro_free_CCA(ro_cca_data);
    if (auth) {
        cavpb->cdp->AAASessionsUnlock(auth->hash);
        cavpb->cdp->AAADropSession(auth);
    }
    LM_DBG("Trying to reserve credit on initial INVITE failed.\n");
    return CSCF_RETURN_ERROR;
}

void remove_aaa_session(str *session_id) {
    AAASession *session;

    if ((session = cavpb->cdp->AAAGetSession(*session_id))) {
        LM_DBG("Found AAA Auth session to delete.\n");
        cavpb->cdp->AAASessionsUnlock(session->hash);
        cavpb->cdp->AAADropSession(session);
    }
}
