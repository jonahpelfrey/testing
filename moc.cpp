#include <memory.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <fstream>

#define READ_TIMEOUT_SECS 10
using namespace std;

/*******************************************************************************
 * TYPE DEFINITIONS
 ******************************************************************************/
typedef unsigned char BYTE;
typedef unsigned long ULONG;
typedef unsigned short USHORT;
typedef struct timespec TIMESPEC;

typedef struct _TOKEN
{
	sem_t semStart;
	int thread_id;
} TOKEN;
typedef TOKEN* PTOKEN;

/*******************************************************************************
 * GLOBAL DEFINITIONS
 ******************************************************************************/
static const ULONG MSEC = 1000000;

static BYTE perThreadData[1000];
static BYTE mainThreadData[1000];
static BYTE serverThreadData[1000];

static PTOKEN pMainToken;
static pthread_t MainThreadId;

static PTOKEN pPerToken;
static pthread_t PerThreadId;

static PTOKEN pServerToken;
static pthread_t ServerThreadId;

int                 listenfd_cmd = -1, connfd_cmd = -1;
struct sockaddr_in  servaddr_cmd, cliaddr_cmd;
socklen_t           clilen_cmd;
socklen_t           clilen_dat;

/*******************************************************************************
 * Thread acting as main thread from arm_main.cpp
 ******************************************************************************/
void* MainThreadProcess(void *pParam)
{
    bool runTest = true;

	PTOKEN pMainToken = (PTOKEN)pParam;
	TIMESPEC ts_wait;
	TIMESPEC ts_sleep = {0*MSEC};
	int rc = 0;

	clock_gettime(CLOCK_REALTIME, &ts_wait);
	ts_wait.tv_sec += 10;
	ts_wait.tv_nsec += (0*MSEC);

	rc = sem_timedwait( &(pMainToken->semStart), &ts_wait);

    while( runTest )
    {
        do
        {
            buff = messageQueue.front();
            sendReturn = sendto( connfd_cmd, buff, (size_t)buff[0], 0,
                                 (struct sockaddr *)&cliaddr_cmd, clilen_cmd );
        } while( sendReturn == -1 && isRetryableError( errno ) );

        if( sendReturn < 0 )
        {
            printf("MainThreadProcess: | FAILURE | socket returned -1 |");
        }
        else
        {
            // the send succeeded; take this message off the queue
            // loop back to send the next one
            free( buff );
            messageQueue.pop();
        }
    }
}


/*******************************************************************************
 * Thread acting as perthread from perthread.cpp
 ******************************************************************************/
void* PerThreadProcess(void *pParam)
{
    bool runTest = true;

	PTOKEN pPerToken = (PTOKEN)pParam;
	TIMESPEC ts_wait;
	TIMESPEC ts_sleep = {0*MSEC};
	int rc = 0;

	clock_gettime(CLOCK_REALTIME, &ts_wait);
	ts_wait.tv_sec += 10;
	ts_wait.tv_nsec += (0*MSEC);

	rc = sem_timedwait( &(pPerToken->semStart), &ts_wait);

    while( runTest )
    {
        do
        {
            buff = messageQueue.front();
            sendReturn = sendto( connfd_cmd, buff, (size_t)buff[0], 0,
                                 (struct sockaddr *)&cliaddr_cmd, clilen_cmd );
        } while( sendReturn == -1 && isRetryableError( errno ) );

        if( sendReturn < 0 )
        {
            printf("PerThreadProcess: | FAILURE | socket returned -1 |");
        }
        else
        {
            // the send succeeded; take this message off the queue
            // loop back to send the next one
            free( buff );
            messageQueue.pop();
        }
    }
}

/*******************************************************************************
 * 
 ******************************************************************************/
void* ServerThreadProcess(void *pParam)
{
    bool runTest = true;

    PTOKEN pPerToken = (PTOKEN)pParam;
    TIMESPEC ts_wait;
    TIMESPEC ts_sleep = {0*MSEC};
    int rc = 0;

    clock_gettime(CLOCK_REALTIME, &ts_wait);
    ts_wait.tv_sec += 10;
    ts_wait.tv_nsec += (0*MSEC);

    rc = sem_timedwait( &(pServerToken->semStart), &ts_wait);

    while( runTest )
    {

    }
}

/*******************************************************************************
 * 
 ******************************************************************************/
bool validateSocketSend()
{

}

/*******************************************************************************
 * 
 ******************************************************************************/
bool initializeSockets()
{

}

/*******************************************************************************
 * 
 ******************************************************************************/
bool initializeTestThreads()
{
    bool retVal = true;

	pMainToken = (PTOKEN)malloc(sizeof(TOKEN));
	pPerToken = (PTOKEN)malloc(sizeof(TOKEN));
    pServerToken = (PTOKEN)malloc(sizeof(TOKEN));

	sem_init(&pMainToken->semStrat, 0, 0);
	sem_init(&pPerToken->semStart, 0, 0);
    sem_init(&pServerToken->semStart, 0, 0);

	pthread_create(&MainThreadId, NULL, MainThreadProcess, (void*)pMainToken);
	pthread_create(&PerThreadId, NULL, PerThreadProcess, (void*)pPerToken);
    pthread_create(&ServerThreadId, NULL, ServerThreadProcess, (void*)pServerToken);

    return retVal;
}

/*******************************************************************************
 * 
 ******************************************************************************/
void startTestThreads()
{
    sem_post( &(pServerToken->semStart) );
    sleep(3);

	sem_post( &(pMainToken->semStart) );
	sem_post( &(pPerToken->semStart) );
}

/*******************************************************************************
 * 
 ******************************************************************************/
void stopTestThreads()
{

}

void populateArrays()
{
    for( int i=0; i<1000; i++ )
    {
        perThreadData[i] = i%256;
        mainThreadData[i] = i%256;
    }
}
