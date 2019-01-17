/*
 * ss.h
 *
 *  Created on: Jan 17, 2019
 *      Author: cryviem
 */

#ifndef SRC_SYNC_SS_H_
#define SRC_SYNC_SS_H_

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

#endif /* SRC_SYNC_SS_H_ */
