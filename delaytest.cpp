#include <memory.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fstream>

using namespace std;

timespec diff(timespec start, timespec end)
{
	timespec temp;

	if( (end.tv_nsec - start.tv_nsec) < 0)
	{
		temp.tv_sec = end.tv_sec - start.tv_sec - 1
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	}
	else
	{
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}

	return temp;
}

int main()
{
	timespec pre, post;
	int temp;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &pre);

	for(int i = 0; i < 10000; i++)
	{
		temp+=temp;
	}

	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &post);

	cout << diff(pre, post).tv_sec << ":" << diff(pre, post).tv_nsec << endl;

	return 0;
}

