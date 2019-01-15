/*
 * tsap.c
 *
 *  Created on: Jan 8, 2019
 *      Author: cryviem
 */

#include "tsap/tsap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>




int socketid;
struct sockaddr_in servaddr;

clienthandler_type client_buf[MAX_CLIENT_SP];

static void tsap_initclienthanderbuff(void);
static clienthandler_type* tsap_getclienthandler(void);
static void tsap_freeclienthandler(clienthandler_type* pin);

void tsap_init(void)
{
	int ret, fdflags;

	/*create socket*/
	socketid = socket(AF_INET, SOCK_STREAM, 0);
	if (socketid == -1)
	{
		printf("tsap: create socket fail!\n");
		exit(1);
	}
	else
	{
		printf("tsap: create socket successful!\n");
	}

	/*bind IP and PORT address*/
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    ret = bind(socketid, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (ret != 0)
    {
    	printf("tsap: bind socket fail!\n");
    	exit(1);
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
    	exit(1);
    }
    else
    {
    	printf("tsap: fcntl get flag successful!\n");
    }

    ret = fcntl(socketid, F_SETFL, fdflags | O_NONBLOCK);
    if (ret < 0)
    {
    	printf("tsap: fcntl set non-blocking fail!\n");
    	exit(1);
    }
    else
    {
    	printf("tsap: fcntl set non-blocking successful!\n");
    }

    /*start listen for connection*/
    ret = listen(socketid, MAX_CLIENT_SP);
    if (ret < 0)
    {
    	printf("tsap: start listen fail!\n");
    	exit(1);
    }
    else
    {
    	printf("tsap: listening...\n");
    }
}

void tsap_check4newclienttask(void)
{
	static int ret;
	clienthandler_type* l_pclienthr;
	struct sockaddr_in l_clientadr;

	ret = accept(socketid, (struct sockaddr*)&l_clientadr, sizeof(struct sockaddr));
	if (ret < 0)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
		{
			/* No connection yet*/
			return;
		}
		else
		{
			printf("tsap: ERROR: %s\n", strerror(errno));
			exit(1);
		}
	}

	/*got connection --> prepare client handler*/
	l_pclienthr = tsap_getclienthandler();
	if (l_pclienthr == NULL)
	{
		printf("tsap: Unexpected case, no more client handler free\n");
		return;
	}

	l_pclienthr->client_adr = l_clientadr;
	l_pclienthr->client_id = ret;

}

void tsap_receivetask(void)
{

}

void tsap_transmittask(void)
{

}

static void tsap_initclienthanderbuff(void)
{
	bzero(client_buf, sizeof(client_buf));
}

static clienthandler_type* tsap_getclienthandler(void)
{
	int i;
	for (i = 0; i < MAX_CLIENT_SP; i++)
	{
		if (client_buf[i].status == 0)
		{
			client_buf[i].status == 1;
			return &client_buf[i];
		}
	}
	return NULL;
}

static void tsap_freeclienthandler(clienthandler_type* pin)
{
	bzero(pin, sizeof(clienthandler_type));
}
