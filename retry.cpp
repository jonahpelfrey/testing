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

//GLOBALS
int commandSocket = 0;
int dataSocket = 0;
bool mainIsRunning = false;
bool socketError = false;
bool socketMutexIsLocked = false;
bool socketHandlesValid = false;
bool didOpenConnections = false;
bool gotSocketsAtLeastOnce = false;
bool readMessageSuccess = false;
bool readMessageLength = false;
bool socketHandlesWereSet = false;

/*******************************************************************************
 *
 * bool openConnections( int fdListenerData,
 *                       int fdListenerCmd,
 *                       struct sockaddr* pClientAddrData,
 *                       struct sockaddr* pClientAddrCmd,
 *                       socklen_t* pAddrlenData,
 *                       socklen_t* pAddrlenCmd,
 *                       int* pFdConnectionData,
 *                       int* pFdConnectionCmd )
 *
 * The listening sockets are opened with the non-blocking flag set in the call
 * to the function socket(). That means the calls to accept() will fail with
 * the error EAGAIN or EWOULDBLOCK if there is no pending connection. After a
 * failure, main() loops around and calls this function again. The GPP starts
 * when the ARM board's linux is booted up. At some later point, the GUI
 * will be started and attempt to connect. Thus many failures are expected
 * before a connection finally succeeds. Once the first connection succeeds,
 * the connection to the other socket should also succeed immediately.
 *
 * The code always connects to the "command" socket first, then tries to connect
 * to the "data" socket.
 *
 * If the command socket cannot connect, this function exits immediately; the
 * caller will wait an amount of time till the next try. If the command socket
 * does connect, this function tries to connect the data socket immediately. If the
 * data socket connects immediately, this function returns true. If the data
 * socket has a retryable error, it waits up to DATA_SOCKET_CONNECT_TIMEOUT_SECS
 * seconds, retrying every 0.5 seconds until it connects or times out. If the
 * retries also fail, the function returns false. If the first attempt to
 * connect to the data socket fails with a non-retryable error, the function
 * immediately returns false and the caller will loop around and try again after
 * some period of time.
 *
 * It doesn't matter if the external application tries to connect to the 'cmd'
 * or 'dat' socket or port first, but it needs to make the second connection
 * request immediately after the first connection succeeds. The GPP looks for
 * a connection to the 'cmd' socket first. Once the 'cmd' socket is connected,
 * it allows only a few seconds - DATA_SOCKET_CONNECT_TIMEOUT_SECS seconds -
 * to receive the second connect request to the data socket.
 *
 ******************************************************************************/
bool openConnections()
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
    // fdcmd = accept( fdListenerCmd, pClientAddrCmd, &lenCmdAddr );

    int commandSocket = 0;
    int dataSocket = 0;

    // If we got a valid file handle for the command socket
    if( commandSocket >= 0 )
    {
        // memcpy( pAddrlenCmdOut, &lenCmdAddr, sizeof(socklen_t) );

        timedout = false;
        timer.StartTimer();
        while( dataSocket < 0 && !timedout )
        {
            // fddata = accept( fdListenerData, pClientAddrData, &lenDataAddr );

            if( dataSocket < 0 )
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
                // memcpy( pAddrlenDataOut, &lenDataAddr, sizeof(socklen_t) );
            }
        } // while( fddata == -1 && !timedout )
        if( dataSocket < 0 )
        {
            // close( fdcmd );
        }
    }

    if( dataSocket >= 0 && commandSocket >= 0 )
    {
        *pFdConnectionData = fddata;
        *pFdConnectionCmd = fdcmd;

        retval = true;
    }

    return retval;
}

void runMain()
{

    while( mainIsRunning )
    {
        // open the command and data sockets.
        if( !socketError )
        {
            if( !didOpenConnections ) 
            {
                mainIsRunning = false;
                continue;
            }
            else
            {
                if( socketMutexIsLocked )
                {
                    dataSocket = 1;
                    commandSocket = 1;
                    socketHandlesWereSet = true;
                    gotSocketsAtLeastOnce = true;
                    socketHandlesValid = true;
                }
            }
        }

        while( mainIsRunning && !socketError )
        {
            // readMessageLength() tries to read one byte from the socket. If
            // a byte can be read, it is the first byte of a message and the
            // number of following bytes to read is returned in inLength.
            // readMessageLength() will return 'false' with socketError unchanged
            // if there is a retryable error. Then the loop just retries the
            // read. It will return 'false' with socketError=true if there is
            // non-retryable error; then the inner loop exits, the socket handles
            // are closed, and the outer loop reopens the socket.
            if( readMessageLength ) // output, non-retryable err
            {
                //Potential change on value of socket error?


                // read the rest of the message into netCmdInput[]
                if( !readMessageSuccess )
                {
                    // Since we did read the first byte of a message, the 'length'
                    // byte, if we don't get the rest of the message pretty quickly
                    // after that without a non-retryable error there is some error
                    // condition occurring. readSocketMessage() will retry after a
                    // retryable error, but times out after READWRITE_TIMEOUT_SECS
                    // seconds. The rest of the bytes have to be successfully read
                    // within that period of time or we assume there is an error
                    // and we get a new socket. Chances are the code will never
                    // execute this branch, but we are handling whatever error might
                    // occur.
                    socketError = true;
                }
                else
                {
                    // we got the whole message - process it
                    
                }

                mainIsRunning = false;
            }
            else 
            {
                mainIsRunning = false;
            }

        } // while( main_running && !socketError )

        // We always have to close the socket handles if we get here.
        // Could be in the middle of a send from cmdSocketSend() or
        // datSocketSend(), though
        if( socketMutexIsLocked )
        {
            socketHandlesValid = false;
        }

        // Close current sockets
        if( commandSocket >= 0 )
        {
            //close command socket
            commandSocket = -1;
        }
        if( dataSocket >= 0 )
        {
            //close data socket
            dataSocket = -1;
        }

        socketError = false;
        mainIsRunning = false;

    } // while( main_running )
}

void resetGlobals()
{
    commandSocket = 0;
    dataSocket = 0;
    mainIsRunning = false;
    socketError = false;
    socketMutexIsLocked = false;
    socketHandlesValid = false;
    didOpenConnections = false;
    gotSocketsAtLeastOnce = false;
    readMessageSuccess = false;
    readMessageLength = false;
    socketHandlesWereSet = false;
}

bool test1()
{
    mainIsRunning = true;
    socketError = true;
    socketMutexIsLocked = true;

    runMain();

    if(socketError)             { return false; }
    if(!socketMutexIsLocked)    { return false; }
    if(dataSocket != -1)        { return false; }
    if(commandSocket != -1)     { return false; }

    return true;
}

bool test2()
{
    mainIsRunning = true;
    socketError = false;
    openConnections = false;
    socketHandlesValid = true;

    runMain();

    if(dataSocket != 0)         { return false; }
    if(commandSocket != 0)      { return false; }
    if(!socketHandlesValid)     { return false; }

    return true;
}

bool test3()
{
    mainIsRunning = true;
    socketError = false;
    openConnections = true;
    socketMutexIsLocked = true;
    socketHandlesValid = false;
    readMessageLength = false;

    runMain();

    if(dataSocket != -1)        { return false; }
    if(commandSocket != -1)     { return false; }
    if(socketError)             { return false; }
    if(socketHandlesValid)      { return false; }
    if(!socketHandlesWereSet)   { return false; }

    return true;
}

void test4()
{
    mainIsRunning = true;
    socketError = false;
    openConnections = true;
    socketMutexIsLocked = true;
    socketHandlesValid = false;
    readMessageLength = true;
    readMessageSuccess = true;

    runMain();

    if(dataSocket != -1)        { return false; }
    if(commandSocket != -1)     { return false; }
    if(socketError)             { return false; }
    if(socketHandlesValid)      { return false; }
    if(!socketHandlesWereSet)   { return false; }


}

void test5()
{
    mainIsRunning = true;
    socketError = false;
    openConnections = true;
    socketMutexIsLocked = true;
    socketHandlesValid = false;
    readMessageLength = true;
    readMessageSuccess = false;

    runMain();

    if(dataSocket != -1)        { return false; }
    if(commandSocket != -1)     { return false; }
    if(!socketError)            { return false; }
    if(socketHandlesValid)      { return false; }
    if(!socketHandlesWereSet)   { return false; }
}

int
main()
{
    if( test1() ) { printf("Test #1: PASSED\n"); }
    else { printf("Test #1: FAILED\n"); }

    resetGlobals();

    if( test2() ) { printf("Test #2: PASSED\n"); }
    else { printf("Test #2: FAILED\n"); }

    resetGlobals();

    if( test3() ) { printf("Test #3: PASSED\n"); }
    else { printf("Test #3: FAILED\n"); }

    resetGlobals();

    if( test4() ) { printf("Test #4: PASSED\n"); }
    else { printf("Test #4: FAILED\n"); }

    resetGlobals();

    if( test5() ) { printf("Test #5: PASSED\n"); }
    else { printf("Test #5: FAILED"); }

    return 0;
}