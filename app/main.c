/*
 * main.c
 *
 *  Created on: Jan 14, 2019
 *      Author: cryviem
 */

#include <stdio.h>
#include <unistd.h>
#include "../thpool_lib/thpool.h"

void task1(void)
{
	int i;
	for (i = 0; i<50; i++)
	{
		printf("task 1 running %d\n", i);
		sleep(1);
	}
}

void task2(void)
{
	int i;
	for (i = 0; i<20; i++)
	{
		printf("task 2 running %d\n", i);
		sleep(1);
	}
}

int main(void)
{
	int cnt;

	threadpool ch_thrdpl = thpool_init(4);
	thpool_add_work(ch_thrdpl, (void*)task1, NULL);
	thpool_add_work(ch_thrdpl, (void*)task2, NULL);

	sleep(5);

	cnt = thpool_num_threads_working(ch_thrdpl);
	printf("num of thread %d\n", cnt);

	thpool_wait(ch_thrdpl);
	printf("all thread finished\n");
	thpool_destroy(ch_thrdpl);

}
