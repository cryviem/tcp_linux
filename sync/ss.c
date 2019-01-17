/*
 * ss.c
 *
 *  Created on: Jan 17, 2019
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

#include "commonthing.h"

void svrproc_checknewconnection(void);
void svrproc_communicate(uint16* clindex);

static int16_t get_clientbox(clientbox_type* list);
static void force_allclientstop(clientbox_type* list);

serverhandler_type s_hdlr;

int16_t server_init(uint16_t port)
{
	int ret, fdflags, socketid, err = 0;
	struct sockaddr_in servaddr;

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
	ret = listen(socketid, MAX_CLIENT_SP);
	EXIT_IF(ret == -1, err = -5)

	/*initialize*/
	bzero(&s_hdlr, sizeof(serverhandler_type));
	/*Prepare handler data*/
	s_hdlr.socketid = socketid;
	s_hdlr.status = SERVER_STS_RUNNING;
	/*create thread pool*/
	s_hdlr.thpoolinst = thpool_init((MAX_CLIENT_SP+1));
	EXIT_IF(s_hdlr.thpoolinst == NULL, err = -7)

	/*run check connection task*/
	ret = thpool_add_work(s_hdlr.thpoolinst, (void*)svrproc_checknewconnection, NULL);
	EXIT_IF(ret == -1, err = -8)

	END_SEGMENT

	if (err < 0)
	{
		/*got error, destroy section*/
		thpool_pause(s_hdl.thpoolinst);
		thpool_destroy(s_hdlr.thpoolinst);
		//call disconnected_cb with con_id invalid
	}
	else
	{
		/*success*/
	}
	return err;
}

void svrproc_checknewconnection(void)
{
	int ret;
	int16_t clientindx;
	uint32_t	error_code = 0;
	struct sockaddr_in clientaddr;

	while(s_hdlr.status != SERVER_STS_STOPPED)
	{
		if (s_hdlr.status == SERVER_STS_STOPREQ)
		{
			force_allclientstop(s_hdlr.clientlist);
			s_hdlr.status = SERVER_STS_STOPPED;
			continue;
		}

		clientindx = get_clientbox();
		if (clientindx >= 0)
		{
			bzero(&clientaddr, sizeof(clientaddr));
			ret = accept(lp_srvrhdlr->socketid, (struct sockaddr *)&clientaddr, sizeof(clientaddr));
			if(ret >= 0)
			{
				/*new connection*/
				s_hdlr.clientlist[clientindx].con_id = ret;
				s_hdlr.clientlist[clientindx].status = CLIENTBOX_READY;

				thpool_add_work(s_hdlr.thpoolinst, (void*)svrproc_communicate, &clientindx);
				/*call connected*/
				s_hdlr.connected_cb(&ret, clientaddr.sin_addr.s_addr);
			}
			else
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					/*system error*/
					LOG("tsap: accept() got error: %s\n", strerror(errno));
					error_code = -1;
					s_hdlr.status = SERVER_STS_STOPREQ;
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
	close(s_hdlr.socketid);

	s_hdlr.disconnected_cb(NULL, error_code);
	/*clean up all tsap server memory*/
	/* destroy threadpool*/
	thpool_pause(s_hdlr.thpoolinst);
	thpool_destroy(s_hdlr.thpoolinst);
	/* good bye */
}

void svrproc_communicate(uint16* clindex)
{
	psrvrhdlr_t lp_srvrhdlr = srvrhdlr;
	clientbox_type* myclient;
	uint32_t	error_code;
	uint16_t	cnt;
	START_SEGMENT

	EXIT_IF(clindex >= MAX_CLIENT_SP, error_code = -1)
	/*save client index for use*/
	myclient = &s_hdlr.clientlist[clindex];

	while(myclient->status != CLIENTBOX_FREE)
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
					myclient->status = CLIENTBOX_FREE;
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
				myclient->status = CLIENTBOX_FREE;
			}
			else if (cnt > 0)
			{
				/*got data, call receive call back*/
				myclient->rxbuflen = cnt;
				s_hdlr.receive_cb(&myclient->con_id, myclient->rxbuff, myclient->rxbuflen);
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
					myclient->status = CLIENTBOX_FREE;
				}
				else
				{
					/* send in process, just wait*/
					sleep(1);
				}
			}
			else if (cnt >= 0)
			{
				/*done*/
				myclient->status = CLIENTBOX_READY;
			}

		}
	}
	END_SEGMENT

	close(myclient->con_id);
	LOG("tsap: client with index %d was closed\n", clindex);
	s_hdlr.disconnected_cb(&myclient->con_id, error_code);
}

void server_send(void* con_id, void* buff, uint16_t len)
{
	uint16_t l_con_id = *((uint16_t*)con_id);
	clientbox_type* clientbx;

	START_SEGMENT
	EXIT_IF(!len, LOG("tsap: len = 0!\n"))

	EXIT_IF(l_con_id < 0 && l_con_id > MAX_CLIENT_SP, LOG("tsap: invalid con id index!\n"))
	EXIT_IF(s_hdlr.clientlist[l_con_id].status == CLIENTBOX_FREE, LOG("tsap: con id is not ready!\n"))
	clientbx = &s_hdlr.clientlist[l_con_id];

	memcpy(clientbx->txbuff, buff, len);
	clientbx->txbuflen = len;
	clientbx->status = CLIENTBOX_TXREQ;
	END_SEGMENT
}


static int16_t get_clientbox(void)
{
	uint16_t i;
	for (i = 0; i<MAX_CLIENT_SP; i++)
	{
		if (s_hdlr.clientlist[i].status == CLIENTBOX_FREE)
		{
			return i;
		}
	}
	return -1;
}

static void force_allclientstop(void)
{
	uint16_t i;
	for (i = 0; i<MAX_CLIENT_SP; i++)
	{
		s_hdlr.clientlist[i].status = CLIENTBOX_FREE;
	}
}

