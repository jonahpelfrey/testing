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
static int MainProcessWrites = 0;

static PTOKEN pPerToken;
static pthread_t PerThreadId;
static int PerProcessWrites = 0;

static PTOKEN pServerToken;
static pthread_t ServerThreadId;
static int ServerProcessReads = 0;
static char ServerBuffer[2048];
static int serverBufIndex = 0;

static int portNumber = 5150;
static int client_fd = -1;

static bool runServer = true;
static bool runClient = true;

static const char msgA[17] = "|11111111111111|";
static const char msgB[17] = "|00000000000000|";

static const int MSG_LEN = 16;

/*******************************************************************************
 * 
 ******************************************************************************/
 bool validateBuffer()
 {
    bool retVal = true;

    int totalWrites = MainProcessWrites + PerProcessWrites;
    printf("Process Writes: %d\n", MainProcessWrites);
    printf("Server Reads: %d\n", ServerProcessReads);

    //Check for missed reads or writes
    if(totalWrites != ServerProcessReads)
    {
        retVal = false;
        printf("ERROR: Total Writes = %d | Total Reads = %d\n", totalWrites, ServerProcessReads);
    }

    //Check validity of server buffer
    // char dest[16];
    // for(int i = 0; i < 2048; i+=16)
    // {
    //     memcpy(dest, &ServerBuffer[i], 16);
    //     printf("Dest: %s\n", dest);
    // }

    return retVal;
 }

/*******************************************************************************
 * 
 ******************************************************************************/
bool openClientSocket()
{
    bool retVal = true;

    int portno;
    struct sockaddr_in serv_addr;
    struct hostent* server;

    //Grab the port number for the client to test
    portno = portNumber;

    //Attempt to open the socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_fd < 0)
    {
        printf("Error opening socket\n");
        retVal = false;
    }

    //Check to see if the server exists
    server = gethostbyname("localhost");
    if( NULL == server )
    {
        printf("Error, no such host\n");
        retVal = false;
    }

    //Zero out the server address structure
    bzero( (char*)&serv_addr, sizeof(serv_addr) );

    //Populate server address structure
    serv_addr.sin_family = AF_INET;
    bcopy( (char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    //Attempt to connect to the server
    printf("Attempting to connect...");
    if( connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 )
    {
        printf("Error connecting to server\n");
        retVal = false;
    }

    printf("Connected to server\n");

    return retVal;
}
 /*******************************************************************************
 * Thread acting as main thread from arm_main.cpp
 ******************************************************************************/
void* MainThreadProcess(void *pParam)
{
    int n;

	PTOKEN pMainToken = (PTOKEN)pParam;
	TIMESPEC ts_wait;
	TIMESPEC ts_sleep = {0*MSEC};
	int rc = 0;

	clock_gettime(CLOCK_REALTIME, &ts_wait);
	ts_wait.tv_sec += 10;
	ts_wait.tv_nsec += (0*MSEC);

	rc = sem_timedwait( &(pMainToken->semStart), &ts_wait);

    if( openClientSocket() )
    {
        while(runClient)
        {
            n = write(client_fd, msgA, strlen(msgA));

            if(n < 0)
            {
                printf("Error writing to socket\n");
                exit(1);
            }
            MainProcessWrites++;
            usleep(40000);
        }
    }

    
    printf("Ending client thread\n");

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

}

/*******************************************************************************
 * 
 ******************************************************************************/
void* ServerThreadProcess(void *pParam)
{
    //Thread Params
    PTOKEN pPerToken = (PTOKEN)pParam;
    TIMESPEC ts_wait;
    TIMESPEC ts_sleep = {0*MSEC};
    int rc = 0;

    //Socket Params
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    //Establish port number
    portno = portNumber;

    //Attempt to open socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        printf("Error opening socket\n");
        exit(1);
    }

    //Zero out the server address structure
    bzero( (char*)&serv_addr, sizeof(serv_addr) );

    //Populate the address structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    //Attempt to bind socket
    if( bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Error binding socket\n");
        exit(1);
    }

    //Listen for connections, allow up to 5 backlogged connections
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    //Time Structs
    clock_gettime(CLOCK_REALTIME, &ts_wait);
    ts_wait.tv_sec += 10;
    ts_wait.tv_nsec += (0*MSEC);

    //Wait for start semaphore to be posted
    rc = sem_timedwait( &(pServerToken->semStart), &ts_wait);

    //New FD is waiting for a client connection
    printf("Waiting to accept...\n");
    newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

    if(newsockfd < 0)
    {
        printf("Error on accept\n");
        exit(1);
    }

    printf("Accepted client connection\n");

    while( runServer )
    {
        int n;
        char buffer[256];

        n = read(newsockfd, buffer, 16);


        if(n < 0)
        {
            printf("Error reading from socket\n");
        } 

        printf("Here is the message: %s\n", buffer);

        if(serverBufIndex != 2048)
        {
            memcpy(&ServerBuffer[serverBufIndex], buffer, strlen(buffer));
            serverBufIndex+=16;
        }


        printf("Server Buffer: %s\n", ServerBuffer);

        ServerProcessReads++;
        usleep(40000);
    }
    printf("Ending server thread\n");
    printf("Starting validation...\n");
    validateBuffer();

    close(sockfd);
}

/*******************************************************************************
 * 
 ******************************************************************************/
void initializeTestThreads()
{
	pMainToken = (PTOKEN)malloc(sizeof(TOKEN));
    pServerToken = (PTOKEN)malloc(sizeof(TOKEN));

	sem_init(&pMainToken->semStart, 0, 0);
    sem_init(&pServerToken->semStart, 0, 0);

    pthread_create(&ServerThreadId, NULL, ServerThreadProcess, (void*)pServerToken);
    pthread_create(&MainThreadId, NULL, MainThreadProcess, (void*)pMainToken);
}

/*******************************************************************************
 * 
 ******************************************************************************/
void startTestThreads()
{
    sem_post( &(pServerToken->semStart) );

    sleep(3);

    sem_post( &(pMainToken->semStart) );
}

/*******************************************************************************
 * 
 ******************************************************************************/
void stopTestThreads()
{

}


int
main()
{
    initializeTestThreads();

    sleep(1);

    startTestThreads();

    while(1)
    {
        if(0x0a == getchar() )
        {
            runServer = false;
            runClient = false;
            break;
        }
    }
    sleep(2);
    printf("End of main thread\n");

    return 0;



}
