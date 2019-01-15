/*
 * tsap.h
 *
 *  Created on: Jan 8, 2019
 *      Author: cryviem
 */

#ifndef SRC_TSAP_TSAP_H_
#define SRC_TSAP_TSAP_H_


#define TSAP_RET_OK				0
#define TSAP_RET_NOK			-1


#define MAX_CLIENT_SP			10

#define TSAP_START_SEGMENT		do{
#define TSAP_END_SEGMENT		}while(false);

#define TSAP_EXIT_IF(con, action_on_exit)		\
if(con)											\
{												\
	action_on_exit;								\
	break;										\
}

typedef struct {
	struct sockaddr_in		client_adr;
	int						client_id;
	int						status;
} clienthandler_type;

#endif /* SRC_TSAP_TSAP_H_ */
