/*
 * tsap_srvr.h
 *
 *  Created on: Jan 14, 2019
 *      Author: cryviem
 */
#include "../thpool_lib/thpool.h"

#ifndef SRC_TSAP_TSAP_SRVR_H_
#define SRC_TSAP_TSAP_SRVR_H_

#define MAX_CLIENT_SP					10
#define MAX_SERVER_SP					5

#define MAX_BUFFER_SIZE					1024

#define CLIENTBOX_FREE					0
#define CLIENTBOX_READY					1
#define CLIENTBOX_TXREQ					2

#define SERVER_STS_STOPPED				0
#define SERVER_STS_RUNNING				1
#define SERVER_STS_STOPREQ				2

typedef struct
{
	uint32_t	con_id;
	uint8_t		status;
	uint8_t		rxbuff[MAX_BUFFER_SIZE];
	uint8_t		txbuff[MAX_BUFFER_SIZE];
	uint16_t	rxbuflen;
	uint16_t	txbuflen;
} clientbox_type;

typedef struct
{
	uint32_t				socketid;
	uint8_t					status;
	threadpool				thpoolinst;
	void (*connected_cb)(void* con_id, uint32_t client_ip);
	void (*disconnected_cb)(void* con_id, uint32_t error_code);
	void (*receive_cb)(void* con_id, void* mesg, uint16_t len);
	uint16_t				clientindx;
	clientbox_type			clientlist[MAX_CLIENT_SP];
} serverhandler_type;



typedef serverhandler_type* 	psrvrhdlr_t;

typedef struct
{
	psrvrhdlr_t 		serverpf;
	uint8_t				status;
} serverbox_type;
#endif /* SRC_TSAP_TSAP_SRVR_H_ */
