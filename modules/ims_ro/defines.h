/*
 * defines.h
 *
 *  Created on: 21 Feb 2013
 *      Author: jaybeepee
 */

#ifndef DEFINES_H_
#define DEFINES_H_

#define MOD_NAME "ims_ro"
#define RO_CC_START 1
#define RO_CC_INTERIM 2
#define RO_CC_STOP 3

/** Return and break the execution of routng script */
#define RO_RETURN_BREAK       0
/** Return true in the routing script */
#define RO_RETURN_TRUE        1
/** Return false in the routing script */
#define RO_RETURN_FALSE -1
/** Return error in the routing script */
#define RO_RETURN_ERROR -2

#define RO_UNKNOWN_DIRECTION 0
#define RO_ORIG_DIRECTION 1
#define RO_TERM_DIRECTION 2
#endif /* DEFINES_H_ */
