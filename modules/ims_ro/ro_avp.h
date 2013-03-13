/*
 * ro_avp.h
 *
 *  Created on: 05 Mar 2013
 *      Author: jaybeepee
 */

#ifndef RO_AVP_H_
#define RO_AVP_H_

#include "../cdp/cdp_load.h"
#include "../cdp_avp/mod_export.h"

struct AAA_AVP_List;
struct AAAMessage;

int ro_add_avp(AAAMessage *m, char *d, int len, int avp_code,
        int flags, int vendorid, int data_do, const char *func);

int ro_add_vendor_specific_application_id_group(AAAMessage *msg, unsigned int vendorid, unsigned int auth_app_id);
int ro_add_destination_realm_avp(AAAMessage *msg, str data);
inline int ro_add_auth_application_id_avp(AAAMessage *msg, unsigned int data);
inline int ro_get_result_code(AAAMessage *msg, unsigned int *data);
unsigned int ro_get_abort_cause(AAAMessage *msg);

#endif /* RO_AVP_H_ */
