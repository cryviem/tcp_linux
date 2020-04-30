/*
 * app.c
 *
 *  Created on: Jan 8, 2019
 *      Author: cryviem
 */

#include "ss.h"
#include "commonthing.h"

#define APP_MAX_SIZE		1024
uint8_t app_sts = 0;
uint16_t	con_id = 0, buflen;
uint32_t client_ip;
uint8_t app_in_buff[APP_MAX_SIZE];
uint8_t app_out_buff[APP_MAX_SIZE];

static void app_reverse(uint8_t* out, uint8_t* in, uint16_t len);

void con_cb(void* con_id, uint32_t cl_ip)
{
	LOG("app: con_cb conid = %d\n", *(uint16_t*)con_id);
}

void rcv_cb(void* lcon_id, void* msg, uint16_t len)
{
	LOG("app: rx from %d - len: %d data: %s\n", *(uint16_t*)lcon_id, len, (char*)msg);

	if (app_sts == 1)
	{
		app_sts = 2;
		con_id = *(uint16_t*)lcon_id;
		memcpy(app_in_buff, msg, len);
		buflen = len;
	}

}

void dis_cb(void* con_id, int32_t error_code)
{
	if (con_id == NULL)
	{
		LOG("app: dis_cb conid = NULL\n");
		app_sts = 3;
	}
	else
	{
		LOG("app: dis_cb conid = %d\n", *(uint16_t*)con_id);
	}
}

void app_proc(void)
{
	int16_t ret;
	while (app_sts <3)
	{
		switch (app_sts)
		{
		case 0: /*init*/
			ret = server_init(8080, &con_cb, &dis_cb, &rcv_cb);
			if (ret)
			{
				/*fail*/
				return;
			}
			app_sts = 1;
			break;
		case 1: /*waiting for data*/

			break;
		case 2: /*handle rxdata*/
			app_reverse(app_out_buff, app_in_buff, buflen);
			server_send((void*)&con_id, (void*)app_out_buff, buflen);
			app_sts = 1;
			break;
		}
		sleep(1);
	}

	server_shutdown();

}

static void app_reverse(uint8_t* out, uint8_t* in, uint16_t len)
{
	uint16_t i;
	if (*(in + len - 1) == '\n')
	{
		for (i = 0; i < (len - 1); i++)
		{
			*(out + i) = *(in + len - 2 - i);
		}
		*(out + len - 1) = '\n';
	}
	else
	{
		for (i = 0; i < len; i++)
		{
			*(out + i) = *(in + len - 1 - i);
		}
	}

}
