/*
 * ro_avp.c
 *
 *  Created on: 05 Mar 2013
 *      Author: jaybeepee
 */

#include "ro_avp.h"

/**< Structure with pointers to cdp funcs, global variable defined in mod.c  */
extern struct cdp_binds cdpb;
extern cdp_avp_bind_t *cdp_avp;

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
inline int ro_add_avp(AAAMessage *m, char *d, int len, int avp_code,
        int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LM_ERR("Rx: :%s: Failed creating avp\n", func);
        return 0;
    }
    if (cdpb.AAAAddAVPToMessage(m, avp, m->avpList.tail) != AAA_ERR_SUCCESS) {
        LM_ERR(":%s: Failed adding avp to message\n", func);
        cdpb.AAAFreeAVP(&avp);
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
static inline int ro_add_avp_list(AAA_AVP_LIST *list, char *d, int len, int avp_code,
        int flags, int vendorid, int data_do, const char *func) {
    AAA_AVP *avp;
    if (vendorid != 0) flags |= AAA_AVP_FLAG_VENDOR_SPECIFIC;
    avp = cdpb.AAACreateAVP(avp_code, flags, vendorid, d, len, data_do);
    if (!avp) {
        LM_ERR(":%s: Failed creating avp\n", func);
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

/**
 * Returns the value of a certain AVP from a Diameter message.
 * @param m - Diameter message to look into
 * @param avp_code - the code to search for
 * @param vendorid - the value of the vendor id to look for or 0 if none
 * @param func - the name of the calling function, for debugging purposes
 * @returns the str with the payload on success or an empty string on failure
 */
static inline str ro_get_avp(AAAMessage *msg, int avp_code, int vendor_id,
        const char *func) {
    AAA_AVP *avp;
    str r = {0, 0};

    avp = cdpb.AAAFindMatchingAVP(msg, 0, AVP_Result_Code, 0, 0);
    if (avp == 0) {
        //LOG(L_INFO,"INFO:"M_NAME":%s: Failed finding avp\n",func);
        return r;
    } else
        return avp->data;
}

/**
 * Creates and adds a Destination-Realm AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int ro_add_destination_realm_avp(AAAMessage *msg, str data) {
    return
    ro_add_avp(msg, data.s, data.len,
            AVP_Destination_Realm,
            AAA_AVP_FLAG_MANDATORY,
            0,
            AVP_DUPLICATE_DATA,
            __FUNCTION__);
}



