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

int fdListenerData;
int fdListenerCmd;
socklen_t* pAddrlenDataIn;
socklen_t* pAddrlenCmdIn;
struct sockaddr* pClientAddrData;
struct sockaddr* pClientAddrCmd;
socklen_t* pAddrlenDataOut;
socklen_t* pAddrlenCmdOu;

timespec diff(timespec start, timespec end)
{
	timespec temp;

	if( (end.tv_nsec - start.tv_nsec) < 0)
	{
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	}
	else
	{
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}

	return temp;
}

bool openConnections( 
                      
                      
                      int* pFdConnectionData,
                      int* pFdConnectionCmd )
{
    char addr_str[ INET6_ADDRSTRLEN ];
    CGPPTimer   timer;
    unsigned long millisecs;
    socklen_t   lenDataAddr;
    socklen_t   lenCmdAddr;
    bool    timedout;
    int     fddata;
    int     fdcmd;
    bool    retval;

    retval = false;

    fddata = -1;

    // Copying to local variables because the addresses of the variables passed to
    // accept() are written to; but this function is called repeatedly and must
    // always pass the same [in] values to accept(). accept() will change the
    // local variable, but the next call to this function will still receive
    // the right values to give to accept().
    memcpy( &lenCmdAddr, pAddrlenCmdIn, sizeof(socklen_t) );
    memcpy( &lenDataAddr, pAddrlenDataIn, sizeof(socklen_t) );

    // Get connection to CMD socket before continuing
    fdcmd = accept( fdListenerCmd, pClientAddrCmd, &lenCmdAddr );

    // If we got a valid file handle for the command socket
    if( fdcmd >= 0 )
    {
        memcpy( pAddrlenCmdOut, &lenCmdAddr, sizeof(socklen_t) );

        timedout = false;
        timer.StartTimer();
        while( fddata < 0 && !timedout )
        {
            fddata = accept( fdListenerData, pClientAddrData, &lenDataAddr );
            if( fddata < 0 )
            {
                if( isRetryableError( errno ) )
                {
                    if( !timer.ElapsedTimeMillisecs( &millisecs ) )
                    {
                        timedout = true;    // timer error, exit the loop
                    }
                    else if( millisecs > DATA_SOCKET_CONNECT_TIMEOUT_SECS*1000 )
                    {
                        timedout = true;
                    }
                    else
                    {
                        usleep( 500000 );
                    }
                }
                else
                {
                    // not really a timeout; but exit the loop
                    timedout = true;
                }
            }
            else
            {
                memcpy( pAddrlenDataOut, &lenDataAddr, sizeof(socklen_t) );
            }
        } // while( fddata == -1 && !timedout )
        if( fddata < 0 )
        {
            close( fdcmd );
        }
    }

    if( fddata >= 0 && fdcmd >= 0 )
    {
        *pFdConnectionData = fddata;
        *pFdConnectionCmd = fdcmd;

        // report the address of the connecting client(s)
        inet_ntop( ((struct sockaddr_in*)pClientAddrCmd)->sin_family,
                   (void *)&((struct sockaddr_in*)pClientAddrCmd)->sin_addr,
                   addr_str,
                   sizeof( addr_str ) );
        printf( "CMD Socket accepted - %X - from - %s\n",
                fdcmd, addr_str );

        inet_ntop( ((struct sockaddr_in*)pClientAddrData)->sin_family,
                   (void *)&((struct sockaddr_in*)pClientAddrData)->sin_addr,
                   addr_str,
                   sizeof( addr_str ) );
        printf( "DAT Socket accepted - %X - from - %s\n",
                fddata, addr_str );

        retval = true;
    }

    return retval;
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

	cout << diff(pre, post).tv_sec << ":" << (diff(pre, post).tv_nsec) * 1000000 << "ms" << endl;

	return 0;
}

