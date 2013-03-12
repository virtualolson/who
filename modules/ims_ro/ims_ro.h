#ifndef CLIENT_RF_IMS_RO_H
#define CLIENT_RF_IMS_RO_H

#include "../../mod_fix.h"
#include "ro_session_hash.h"

void remove_aaa_session(str *session_id);
int Ro_Send_CCR(struct sip_msg *msg, char* reserve_time);
void send_ccr_interim(struct ro_session *ro_session, str* from_uri, str *to_uri, int *new_credit, int *credit_valid_for, unsigned int used, unsigned int reserve, unsigned int *is_final_allocation);
void send_ccr_stop(struct ro_session *ro_session);


#endif /* CLIENT_RF_IMS_RO_H */
