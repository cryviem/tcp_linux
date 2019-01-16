/*
 * tsap_srvr.c
 *
 *  Created on: Jan 14, 2019
 *      Author: cryviem
 */

#include "tsap/tsap.h"
#include "tsap/tsap_srvr.h"
#include "common/commonthing.h"


int16_t server_init(uint16_t port, uint16_t maxclnt)
{
	int ret, fdflags, socketid, err = 0;
	struct sockaddr_in servaddr;
	psrvrhdlr_t	psrvrhdlr;

	START_SEGMENT
	/*create socket*/
	socketid = socket(AF_INET, SOCK_STREAM, 0);
	EXIT_IF(socketid == -1, err = -1)

	/*bind IP and PORT address*/
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	ret = bind(socketid, (struct sockaddr*)&servaddr, sizeof(servaddr));
	EXIT_IF(ret, err = -2)

	/*turn socket file descriptor to non-blocking*/
	fdflags = fcntl(socketid, F_GETFL, 0);
	EXIT_IF(fdflags == -1, err = -3)

	ret = fcntl(socketid, F_SETFL, fdflags | O_NONBLOCK);
	EXIT_IF(ret == -1, err = -4)

	/*start listen for connection*/
	ret = listen(socketid, maxclnt);
	EXIT_IF(ret == -1, err = -5)

	/*create server handler*/
	psrvrhdlr = (psrvrhdlr_t)malloc(sizeof(serverhandler_type));
	EXIT_IF(psrvrhdlr == NULL, err = -6)
	/*initialize*/
	bzero(psrvrhdlr, sizeof(serverhandler_type));
	/*Prepare handler data*/
	psrvrhdlr->socketid = socketid;

	/*create thread pool*/
	psrvrhdlr->thpoolinst = thpool_init((maxclnt+1));
	EXIT_IF(psrvrhdlr->thpoolinst == NULL, err = -7)

	/*run check connection task*/
	ret = thpool_add_work(psrvrhdlr->thpoolinst, (void*)svrproc_checknewconnection, psrvrhdlr);
	EXIT_IF(ret == -1, err = -8)

	END_SEGMENT

	if (err < 0)
	{
		/*got error, destroy section*/
		free(psrvrhdlr);
		thpool_destroy(svr_thpl_hlr);
		//call disconnected_cb with con_id invalid
	}
	return err;
}

void svrproc_checknewconnection(psrvrhdlr_t srvrhdlr)
{
	int ret;
	int16_t clientindx;
	struct sockaddr_in clientaddr;
	srvrhdlr->clientindx = MAX_CLIENT_SP;
	while(1)
	{
		if (srvrhdlr->clientindx < MAX_CLIENT_SP)
		{
			/*previous communication task hasn't response*/
			continue;
		}

		clientindx = get_clientbox(srvrhdlr->clientlist);
		if (clientindx >= 0)
		{
			bzero(&clientaddr, sizeof(clientaddr));
			ret = accept(srvrhdlr->socketid, (struct sockaddr *)&clientaddr, sizeof(clientaddr));
			if(ret >= 0)
			{
				/*new connection*/
				srvrhdlr->clientindx = clientindx;
				srvrhdlr->clientlist[clientindx].con_id = ret;
				srvrhdlr->clientlist[clientindx].status = CLIENTBOX_READY;

				thpool_add_work(srvrhdlr->thpoolinst, (void*)svrproc_communicate, srvrhdlr);
				/*call connected*/
				srvrhdlr->connected_cb(&ret, clientaddr.sin_addr.s_addr);
			}
			else
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					/*system error*/
					LOG("tsap: accept() got error: %s\n", strerror(errno));
					break;
				}
				else
				{
					/* no any new connection, just wait*/
					sleep(1);
				}
			}
		}
	}
}

void svrproc_communicate(psrvrhdlr_t srvrhdlr)
{
	clientbox_type myclient;
	uint32_t	error_code;
	uint16_t	cnt;
	START_SEGMENT

	EXIT_IF(srvrhdlr->clientindx >= MAX_CLIENT_SP, error_code = -1)
	/*save client index for use*/
	myclient = srvrhdlr->clientlist[srvrhdlr->clientindx];
	/*response to server*/
	srvrhdlr->clientindx = MAX_CLIENT_SP;

	while(1)
	{
		if (myclient.status == CLIENTBOX_READY)
		{
			cnt = recv(myclient.con_id, myclient.rxbuff, MAX_BUFFER_SIZE, 0);
			if (cnt == -1)
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					/*system error*/
					LOG("tsap: recv() got error: %s\n", strerror(errno));
					break;
				}
				else
				{
					/* no any data yet, just wait*/
					sleep(1);
				}
			}
			else if (cnt == 0)
			{
				/*connection closed*/
				break;
			}
			else if (cnt > 0)
			{
				/*got data, call receive call back*/
				myclient->rxbuflen = cnt;
				srvrhdlr->receive_cb(&myclient.con_id, myclient->rxbuff, myclient->rxbuflen);
			}
		}
		else if (myclient.status == CLIENTBOX_TXREQ)
		{
			cnt = send(myclient.con_id, myclient->txbuff, myclient->txbuflen);
			if (cnt == -1)
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					/*system error*/
					LOG("tsap: send() got error: %s\n", strerror(errno));
					break;
				}
				else
				{
					/* send in process, just wait*/
					//sleep(1);
				}
			}
			else if (cnt >= 0)
			{
				/*done*/
				myclient.status = CLIENTBOX_READY;
			}

		}
		else if (myclient.status == CLIENTBOX_FREE)
		{
			break;
		}
	}
	END_SEGMENT

	myclient.status = CLIENTBOX_FREE;
	srvrhdlr->disconnected_cb(&myclient.status, error_code);
}

int16_t get_clientbox(clientbox_type* list)
{
	uint16_t i;
	for (i = 0; i<MAX_CLIENT_SP; i++)
	{
		if (list[i].status == CLIENTBOX_FREE)
		{
			return i;
		}
	}
	return -1;
}
