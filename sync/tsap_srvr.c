/*
 * tsap_srvr.c
 *
 *  Created on: Jan 14, 2019
 *      Author: cryviem
 */

#include "tsap/tsap.h"
#include "tsap/tsap_srvr.h"
#include "common/commonthing.h"

static threadpool svr_thpl_hlr;


int16_t server_init(uint16_t port, uint16_t maxclnt)
{
	int ret, fdflags, socketid;
	psrvrhdlr_t	psrvrhdlr;
	/*create socket*/
	START_SEGMENT

	socketid = socket(AF_INET, SOCK_STREAM, 0);
	EXIT_IF(socketid == -1, printf("tsap: create socket fail!\n"))

	if (socketid == -1)
	{
		printf("tsap: create socket fail!\n");
		break;
	}
	else
	{
		printf("tsap: create socket successful!\n");
	}

	/*bind IP and PORT address*/
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	ret = bind(socketid, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (ret != 0)
	{
		printf("tsap: bind socket fail!\n");
		return TSAP_RET_NOK;
	}
	else
	{
		printf("tsap: bind socket successful!\n");
	}

	/*turn socket file descriptor to non-blocking*/
	fdflags = fcntl(socketid, F_GETFL, 0);
	if (fdflags < 0)
	{
		printf("tsap: fcntl get flag fail!\n");
		return TSAP_RET_NOK;
	}
	else
	{
		printf("tsap: fcntl get flag successful!\n");
	}

	ret = fcntl(socketid, F_SETFL, fdflags | O_NONBLOCK);
	if (ret < 0)
	{
		printf("tsap: fcntl set non-blocking fail!\n");
		return TSAP_RET_NOK;
	}
	else
	{
		printf("tsap: fcntl set non-blocking successful!\n");
	}

	/*start listen for connection*/
	ret = listen(socketid, maxclnt);
	if (ret < 0)
	{
		printf("tsap: start listen fail!\n");
		return TSAP_RET_NOK;
	}
	else
	{
		printf("tsap: listening...\n");
	}

	/*create server handler*/
	psrvrhdlr = (psrvrhdlr_t)malloc(sizeof(serverhandler_type));
	if (psrvrhdlr == NULL)
	{
		printf("tsap: server handler allocate fail!\n");
		return TSAP_RET_NOK;
	}
	else
	{
		printf("tsap: server handler allocate successful\n");
	}

	/*Prepare handler data*/
	psrvrhdlr->socketid = socketid;

	/*create thread pool*/
	svr_thpl_hlr = thpool_init((maxclnt+1));
	if (svr_thpl_hlr == NULL)
	{
		printf("tsap: thread pool create fail!\n");
		free(psrvrhdlr);
		return TSAP_RET_NOK;
	}
	else
	{
		printf("tsap: thread pool is created\n");
	}

	/*run check connection task*/
	ret = thpool_add_work(svr_thpl_hlr, (void*)svrproc_checknewconnection, psrvrhdlr);
	if (ret == -1)
	{
		printf("tsap: add task check_new_connection fail!\n");
		free(psrvrhdlr);
		thpool_destroy(svr_thpl_hlr);
		return TSAP_RET_NOK;
	}
	else
	{
		printf("tsap: add check_new_connection successful\n");
	}

	END_SEGMENT

}

void svrproc_checknewconnection(psrvrhdlr_t srvrhdlr)
{

}

