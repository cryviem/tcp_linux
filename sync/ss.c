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

#include "../common/commonthing.h"
#include "../thpool_lib/thpool.h"
#include "ss.h"

void svrproc_checknewconnection(void);
void svrproc_communicate(uint16_t* clindex);

static int16_t get_clientbox(void);
static void force_allclientstop(void);
static threadpool get_thpollinst(void);

serverhandler_type s_hdlr;

int16_t server_init(uint16_t port, 	connected_cb con_cb, disconnected_cb discon_cb, receive_cb rcv_cb)
{
	int ret, fdflags, socketid = -1, err = 0;
	struct sockaddr_in servaddr;

	START_SEGMENT

	/*create socket*/
	socketid = socket(AF_INET, SOCK_STREAM, 0);
	EXIT_IF(socketid == -1, err = -1; LOG("tsap: socket create fail!\n"))

	/*bind IP and PORT address*/
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	ret = bind(socketid, (struct sockaddr*)&servaddr, sizeof(servaddr));
	EXIT_IF(ret, err = -1; LOG("tsap: socket bind fail!\n"))

	/*turn socket file descriptor to non-blocking*/
	fdflags = fcntl(socketid, F_GETFL, 0);
	EXIT_IF(fdflags == -1, err = -1; LOG("tsap: fcntl get fail!\n"))

	ret = fcntl(socketid, F_SETFL, fdflags | O_NONBLOCK);
	EXIT_IF(ret == -1, err = -1; LOG("tsap: fcntl set fail!\n"))

	/*start listen for connection*/
	ret = listen(socketid, MAX_CLIENT_SP);
	EXIT_IF(ret == -1, err = -1; LOG("tsap: socket listen fail!\n"))

	/*initialize*/
	bzero(&s_hdlr, sizeof(serverhandler_type));
	/*Prepare handler data*/
	s_hdlr.socketid = socketid;
	s_hdlr.status = SERVER_STS_RUNNING;
	s_hdlr.connectedcbFun = con_cb;
	s_hdlr.disconnectedcbFun = discon_cb;
	s_hdlr.receivedcbFun = rcv_cb;
	/*create thread pool*/
	s_hdlr.thpoolinst = get_thpollinst();
	EXIT_IF(s_hdlr.thpoolinst == NULL, err = -1; LOG("tsap: thpool init fail!\n"))

	/*run check connection task*/
	ret = thpool_add_work(s_hdlr.thpoolinst, (void*)svrproc_checknewconnection, NULL);
	EXIT_IF(ret == -1, err = -1; LOG("tsap: thpool add work fail!\n"))

	END_SEGMENT

	if (err < 0)
	{
		/*got error, destroy section*/
		thpool_pause(s_hdlr.thpoolinst);
		thpool_destroy(s_hdlr.thpoolinst);
		if (socketid > -1)
		{
			close(socketid);
		}

		//call disconnected_cb with con_id invalid
	}
	else
	{
		/*success*/
		LOG("tsap: server init success");
	}
	return err;
}

void svrproc_checknewconnection(void)
{
	int ret;
	int16_t clientindx;
	uint32_t	error_code = 0;
	struct sockaddr_in clientaddr;
	int16_t clientaddr_len = sizeof(struct sockaddr_in);

	while(s_hdlr.status != SERVER_STS_STOPPED)
	{
		if (s_hdlr.status == SERVER_STS_REQSTOP)
		{
			force_allclientstop();
			s_hdlr.status = SERVER_STS_STOPPED;
			continue;
		}

		clientindx = get_clientbox();
		if (clientindx >= 0)
		{
			bzero(&clientaddr, sizeof(clientaddr));
			ret = accept(s_hdlr.socketid, (struct sockaddr *)&clientaddr, (socklen_t*)&clientaddr_len);
			if(ret >= 0)
			{
				/*new connection*/
				s_hdlr.clientlist[clientindx].con_id = ret;
				s_hdlr.clientlist[clientindx].status = CLIENT_STS_READY;

				thpool_add_work(s_hdlr.thpoolinst, (void*)svrproc_communicate, &clientindx);
				/*call connected*/
				s_hdlr.connectedcbFun(&clientindx, clientaddr.sin_addr.s_addr);
			}
			else
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					/*system error*/
					LOG("tsap: accept() got error: %s\n", strerror(errno));
					error_code = -1;
					s_hdlr.status = SERVER_STS_REQSTOP;
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

	s_hdlr.disconnectedcbFun(NULL, error_code);
	/*clean up all tsap server memory*/
	/* destroy threadpool*/
	thpool_pause(s_hdlr.thpoolinst);
	thpool_destroy(s_hdlr.thpoolinst);
	/* good bye */
}

void svrproc_communicate(uint16_t* clindex)
{
	clientbox_type* myclient;
	uint32_t	error_code;
	uint16_t	cnt, indx = (*clindex);
	START_SEGMENT

	EXIT_IF(indx >= MAX_CLIENT_SP, error_code = -1)
	/*save client index for use*/
	myclient = &s_hdlr.clientlist[indx];

	while(myclient->status != CLIENT_STS_FREE)
	{
		if (myclient->status == CLIENT_STS_READY)
		{
			cnt = recv(myclient->con_id, myclient->rxbuff, MAX_BUFFER_SIZE, 0);
			if (cnt == -1)
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					/*system error*/
					LOG("tsap: recv() got error: %s\n", strerror(errno));
					myclient->status = CLIENT_STS_FREE;
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
				myclient->status = CLIENT_STS_FREE;
			}
			else if (cnt > 0)
			{
				/*got data, call receive call back*/
				myclient->rxbuflen = cnt;
				s_hdlr.receivedcbFun(&myclient->con_id, myclient->rxbuff, myclient->rxbuflen);
			}
		}
		else if (myclient->status == CLIENT_STS_TXREQ)
		{
			cnt = send(myclient->con_id, myclient->txbuff, myclient->txbuflen, 0);
			if (cnt == -1)
			{
				if (errno != EWOULDBLOCK && errno != EAGAIN)
				{
					/*system error*/
					LOG("tsap: send() got error: %s\n", strerror(errno));
					myclient->status = CLIENT_STS_FREE;
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
				myclient->status = CLIENT_STS_READY;
			}

		}
	}
	END_SEGMENT

	close(myclient->con_id);
	LOG("tsap: client with index %d was closed\n", indx);
	s_hdlr.disconnectedcbFun(&indx, error_code);
}

void server_send(void* indx, void* buff, uint16_t len)
{
	uint16_t l_indx = *((uint16_t*)indx);
	clientbox_type* clientbx;

	START_SEGMENT
	EXIT_IF(!len, LOG("tsap: len = 0!\n"))

	EXIT_IF(l_indx < 0 && l_indx > MAX_CLIENT_SP, LOG("tsap: invalid con id index!\n"))
	EXIT_IF(s_hdlr.clientlist[l_indx].status == CLIENT_STS_FREE, LOG("tsap: con id is not ready!\n"))
	clientbx = &s_hdlr.clientlist[l_indx];

	memcpy(clientbx->txbuff, buff, len);
	clientbx->txbuflen = len;
	clientbx->status = CLIENT_STS_TXREQ;
	END_SEGMENT
}

void server_closeconn(void* indx)
{
	uint16_t l_indx = *((uint16_t*)indx);
	clientbox_type* clientbx;

	START_SEGMENT

	EXIT_IF(l_indx < 0 && l_indx > MAX_CLIENT_SP, LOG("tsap: invalid con id index!\n"))
	EXIT_IF(s_hdlr.clientlist[l_indx].status == CLIENT_STS_FREE, LOG("tsap: con id is not ready!\n"))
	clientbx = &s_hdlr.clientlist[l_indx];

	clientbx->status = CLIENT_STS_FREE;
	END_SEGMENT
}

void server_shutdown(void)
{
	s_hdlr.status = SERVER_STS_REQSTOP;
}

static int16_t get_clientbox(void)
{
	uint16_t i;
	for (i = 0; i<MAX_CLIENT_SP; i++)
	{
		if (s_hdlr.clientlist[i].status == CLIENT_STS_FREE)
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
		s_hdlr.clientlist[i].status = CLIENT_STS_FREE;
	}
}

static threadpool	get_thpollinst(void)
{
	static threadpool tmp = NULL;
	if (tmp == NULL)
	{
		tmp = thpool_init((MAX_CLIENT_SP+1));
	}

	return tmp;
}
