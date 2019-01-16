/*
 * tsap_srvr.c
 *
 *  Created on: Jan 14, 2019
 *      Author: cryviem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#include "tsap.h"
#include "tsap_srvr.h"
#include "commonthing.h"


void svrproc_checknewconnection(psrvrhdlr_t srvrhdlr);
void svrproc_communicate(psrvrhdlr_t srvrhdlr);

static int16_t get_clientbox(clientbox_type* list);
static void force_allclientstop(clientbox_type* list);
static int16_t get_serverhdlr(void);
static void push_serverhdlrtolist(int16_t indx, serverhandler_type* push);
static void destroy_serverhflr(int16_t indx);
static uint16_t get_s_indxfromhdlr(psrvrhdlr_t hdlr);

static serverbox_type serverlist[MAX_SERVER_SP];

int16_t server_init(uint16_t port, uint16_t maxclnt)
{
	int ret, fdflags, socketid, err = 0;
	struct sockaddr_in servaddr;
	psrvrhdlr_t	psrvrhdlr;
	uint16_t	srv_indx;


	START_SEGMENT

	srv_indx = get_serverhdlr();
	EXIT_IF(srv_indx == -1, err = -1)
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
	psrvrhdlr->status = SERVER_STS_RUNNING;
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
		thpool_pause(psrvrhdlr->thpoolinst);
		thpool_destroy(psrvrhdlr->thpoolinst);
		free(psrvrhdlr);
		//call disconnected_cb with con_id invalid
	}
	else
	{
		/*success*/
		push_serverhdlrtolist(srv_indx, psrvrhdlr);
		err = srv_indx;
	}
	return err;
}

void svrproc_checknewconnection(psrvrhdlr_t srvrhdlr)
{
	psrvrhdlr_t lp_srvrhdlr = srvrhdlr;
	threadpool l_thpool = lp_srvrhdlr->thpoolinst;
	int ret;
	int16_t clientindx;
	uint32_t	error_code = 0;
	struct sockaddr_in clientaddr;
	lp_srvrhdlr->clientindx = MAX_CLIENT_SP;
	while(1)
	{
		if (lp_srvrhdlr->status == SERVER_STS_STOPREQ)
		{
			force_allclientstop(lp_srvrhdlr->clientlist);
			lp_srvrhdlr->status = SERVER_STS_STOPPED;
		}
		else if (lp_srvrhdlr->status == SERVER_STS_STOPREQ)
		{
			/*finish this task*/
			break;
		}

		if (lp_srvrhdlr->clientindx < MAX_CLIENT_SP)
		{
			/*previous communication task hasn't response*/
			continue;
		}

		clientindx = get_clientbox(lp_srvrhdlr->clientlist);
		if (clientindx >= 0)
		{
			bzero(&clientaddr, sizeof(clientaddr));
			ret = accept(lp_srvrhdlr->socketid, (struct sockaddr *)&clientaddr, sizeof(clientaddr));
			if(ret >= 0)
			{
				/*new connection*/
				lp_srvrhdlr->clientindx = clientindx;
				lp_srvrhdlr->clientlist[clientindx].con_id = ret;
				lp_srvrhdlr->clientlist[clientindx].status = CLIENTBOX_READY;

				thpool_add_work(l_thpool, (void*)svrproc_communicate, lp_srvrhdlr);
				/*call connected*/
				lp_srvrhdlr->connected_cb(&ret, clientaddr.sin_addr.s_addr);
			}
			else
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					/*system error*/
					LOG("tsap: accept() got error: %s\n", strerror(errno));
					error_code = -1;
					lp_srvrhdlr->status = SERVER_STS_STOPREQ;
				}
				else
				{
					/* no any new connection, just wait*/
					sleep(1);
				}
			}
		}
	}
	/*close parent socket*/
	close(lp_srvrhdlr->socketid);

	lp_srvrhdlr->disconnected_cb(NULL, error_code);
	/*clean up all tsap server memory*/
	ret = get_s_indxfromhdlr(lp_srvrhdlr);
	if (ret >= 0)
	{
		destroy_serverhflr(ret);
	}
	/* destroy threadpool*/
	thpool_pause(l_thpool);
	thpool_destroy(l_thpool);
	/* good bye */
}

void svrproc_communicate(psrvrhdlr_t srvrhdlr)
{
	psrvrhdlr_t lp_srvrhdlr = srvrhdlr;
	clientbox_type* myclient;
	uint32_t	error_code;
	uint16_t	cnt, index;
	START_SEGMENT

	index = lp_srvrhdlr->clientindx;
	EXIT_IF(index >= MAX_CLIENT_SP, error_code = -1)
	/*save client index for use*/
	myclient = &lp_srvrhdlr->clientlist[index];
	/*response to server*/
	lp_srvrhdlr->clientindx = MAX_CLIENT_SP;

	while(1)
	{
		if (myclient->status == CLIENTBOX_READY)
		{
			cnt = recv(myclient->con_id, myclient->rxbuff, MAX_BUFFER_SIZE, 0);
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
				lp_srvrhdlr->receive_cb(&myclient->con_id, myclient->rxbuff, myclient->rxbuflen);
			}
		}
		else if (myclient->status == CLIENTBOX_TXREQ)
		{
			cnt = send(myclient->con_id, myclient->txbuff, myclient->txbuflen, 0);
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
				myclient->status = CLIENTBOX_READY;
			}

		}
		else if (myclient->status == CLIENTBOX_FREE)
		{
			break;
		}
	}
	END_SEGMENT

	close(myclient->con_id);
	myclient->status = CLIENTBOX_FREE;
	LOG("tsap: client with index %d was closed\n", index);
	lp_srvrhdlr->disconnected_cb(&myclient->con_id, error_code);
}

void server_send(uint16_t s_indx, void* con_id, void* buff, uint16_t len)
{
	uint16_t l_con_id = *((uint16_t*)con_id);
	psrvrhdlr_t		l_srvrhdlr;
	clientbox_type* clientbx;

	START_SEGMENT
	EXIT_IF(!len, LOG("tsap: len = 0!\n"))
	EXIT_IF(s_indx < 0 && s_indx > MAX_SERVER_SP, LOG("tsap: invalid server index!\n"))
	EXIT_IF(serverlist[s_indx].status == 0, LOG("tsap: server is not ready!\n"))

	l_srvrhdlr = serverlist[s_indx].serverpf;

	EXIT_IF(l_con_id < 0 && l_con_id > MAX_CLIENT_SP, LOG("tsap: invalid con id index!\n"))
	EXIT_IF(l_srvrhdlr->clientlist[l_con_id].status == CLIENTBOX_FREE, LOG("tsap: con id is not ready!\n"))
	clientbx = &l_srvrhdlr->clientlist[l_con_id];

	memcpy(clientbx->txbuff, buff, len);
	clientbx->txbuflen = len;
	clientbx->status = CLIENTBOX_TXREQ;
	END_SEGMENT
}


static int16_t get_clientbox(clientbox_type* list)
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

static void force_allclientstop(clientbox_type* list)
{
	uint16_t i;
	for (i = 0; i<MAX_CLIENT_SP; i++)
	{
		list[i].status = CLIENTBOX_FREE;
	}
}

static int16_t get_serverhdlr(void)
{
	uint16_t i;
	for (i = 0; i<MAX_SERVER_SP; i++)
	{
		if (serverlist[i].status == 0)
		{
			return i;
		}
	}
	return -1;
}

static void push_serverhdlrtolist(int16_t indx, serverhandler_type* push)
{
	if (indx > -1 && indx < MAX_SERVER_SP)
	{
		serverlist[indx].serverpf = push;
		serverlist[indx].status = 1;
	}
}

static void destroy_serverhflr(int16_t indx)
{
	if (indx > -1 && indx < MAX_SERVER_SP)
	{
		free(serverlist[indx].serverpf);
		serverlist[indx].status = 0;
	}

}

static uint16_t get_s_indxfromhdlr(psrvrhdlr_t hdlr)
{
	uint16_t i;
	for (i = 0; i<MAX_SERVER_SP; i++)
	{
		if ((serverlist[i].serverpf == hdlr) && (serverlist[i].status == 1))
		{
			return i;
		}
	}
	return -1;
}
