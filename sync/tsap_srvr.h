/*
 * tsap_srvr.h
 *
 *  Created on: Jan 14, 2019
 *      Author: cryviem
 */

#ifndef SRC_TSAP_TSAP_SRVR_H_
#define SRC_TSAP_TSAP_SRVR_H_

#define MAX_CLIENT_SP				10
typedef struct
{
	uint32_t	con_id;

} clientbox_type;

typedef struct
{
	uint32_t				socketid;
} serverhandler_type;

typedef serverhandler_type* 	psrvrhdlr_t;

#endif /* SRC_TSAP_TSAP_SRVR_H_ */
