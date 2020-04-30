/*
 * ss.h
 *
 *  Created on: Jan 17, 2019
 *      Author: cryviem
 */

#ifndef SRC_SYNC_SS_H_
#define SRC_SYNC_SS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "thpool.h"

#define MAX_BUFFER_SIZE				1024
#define MAX_CLIENT_SP				10


#define SERVER_STS_STOPPED			0
#define SERVER_STS_RUNNING			1
#define SERVER_STS_REQSTOP			2

#define CLIENT_STS_FREE				0
#define CLIENT_STS_READY			1
#define CLIENT_STS_TXREQ			2

typedef void (*connected_cb)(void* con_id, uint32_t client_ip);
typedef void (*disconnected_cb)(void* con_id, int32_t error_code);
typedef void (*receive_cb)(void* con_id, void* msg, uint16_t len);

typedef struct
{
	uint32_t	con_id;
	volatile uint8_t		status;
	uint8_t		rxbuff[MAX_BUFFER_SIZE];
	uint8_t		txbuff[MAX_BUFFER_SIZE];
	uint16_t	rxbuflen;
	uint16_t	txbuflen;
} clientbox_type;

typedef struct
{
	uint32_t				socketid;
	volatile uint8_t					status;
	threadpool				thpoolinst;
	connected_cb			connectedcbFun;
	disconnected_cb			disconnectedcbFun;
	receive_cb				receivedcbFun;
	uint16_t				clientindx;
	clientbox_type			clientlist[MAX_CLIENT_SP];
} serverhandler_type;




int16_t server_init(uint16_t port, 	connected_cb con_cb, disconnected_cb discon_cb, receive_cb rcv_cb);
void server_send(void* indx, void* buff, uint16_t len);
void server_closeconn(void* indx);
void server_shutdown(void);

#endif /* SRC_SYNC_SS_H_ */
