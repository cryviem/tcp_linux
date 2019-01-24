/*
 * commonthing.h
 *
 *  Created on: Jan 15, 2019
 *      Author: cryviem
 */

#ifndef SRC_COMMON_COMMONTHING_H_
#define SRC_COMMON_COMMONTHING_H_

#define START_SEGMENT		do{
#define END_SEGMENT		}while(0);

#define EXIT_IF(con, action_on_exit)			\
if(con)											\
{												\
	action_on_exit;								\
	break;										\
}

#define do_nothing								;
#define PRINTLOG

#ifdef PRINTLOG
#define LOG			printf
#else
#define LOG
#endif

#endif /* SRC_COMMON_COMMONTHING_H_ */
