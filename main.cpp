#include <memory.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#define READ_TIMEOUT_SECS 10
using namespace std;

static unsigned char scratch[1000];
static bool doSleep = false;
static unsigned char array[1000];
static unsigned offsets[10];
static int readret[10];
static int offsetidx = 0;

int read_test( int fd, void* buffer, size_t bytes )
{
    if( fd == -1 )
    {
        return -1;
    }
    memcpy( buffer, &array[offsets[offsetidx]], bytes );

    if( doSleep )
    {
        usleep( 11000000 );
    }
    return readret[offsetidx++];
}

void validateReadBuff( unsigned char* buffer, size_t bytes, int testnum )
{
    for( size_t i=0; i<bytes; i++ )
    {
        if( buffer[i] != array[i] )
        {
            printf("buffer error test %d\n", testnum);
        }
    }

}

/*******************************************************************************
 *
 * bool exceededTimeout( struct timespec& timeout )
 *
 * Is the current time past the time given in 'timeout'?
 *
 * RETURNS  true if the current time is past the time given in 'timeout',
 *          false if current time <= timeout, or if cannot get the current time
 *
 ******************************************************************************/
bool exceededTimeout( struct timespec& timeout )
{
    struct timespec     currtime;
    int     timedOut;

    timedOut = false;
    if( clock_gettime( CLOCK_REALTIME, &currtime ) != -1 )
    {
        // see if we have timed out. if(currtime>readtime),
        // it timed out
        // normalize currtime
        if( currtime.tv_nsec >= (1000 * 1000 * 1000) )
        {
            currtime.tv_sec += (currtime.tv_nsec / (1000 * 1000 * 1000));
            currtime.tv_nsec %= (1000 * 1000 * 1000);
        }

        // is currtime > readtime?
        if( currtime.tv_sec >= timeout.tv_sec )
        {
            // using this logic to minimize jumps
            timedOut = true;
            if( currtime.tv_sec == timeout.tv_sec &&
                currtime.tv_nsec <= timeout.tv_nsec )
            {
                timedOut = false;
            }
        }
    }

    return timedOut;
}

/*******************************************************************************
 *
 * bool readSocketMessage( size_t numbytes,
 *                         int socketFD,
 *                         void* buffer,
 *                         size_t buffersize )
 *
 * This function is called immediately after a call to socket function recv()
 * which reads 1 byte, the number of bytes to be read in this function (plus 1).
 * The bytes being read were sent all at once across the socket, but a signal
 * can interrupt the read, in which fewer bytes would have been read that are
 * requested. This function will loop until the total number of bytes have been
 * read, or until an outright error occurs.
 *
 * If read has an error, but 'errno' is EINTR, the read will be reattempted
 * since EINTR means "the operation was interrupted by a signal and should be
 * retried."
 *
 * RETURNS: 1 if a message has been read, or 0 if:
 *              a socket error occurred
 *              the read timed out after READ_TIMEOUT_SECS seconds
 *              the number of bytes to read is 0 or other parameter error
 *
 *****************************************************************************/
bool readSocketMessage( size_t toRead,
                        int socketFD,
                        void* buffer,
                        size_t buffersize )
{
    struct timespec     readtime;
    bool    haveTime;
    int     errorFlag;
    int     readRet;
    size_t  bytesToRead;
    bool    timedOut;
    size_t  totalBytesRead;

    timedOut = false;

    if( toRead == 0 ||             // check the parameters
        socketFD == -1 ||
        buffer == NULL ||
        buffersize < toRead )
    {
        errorFlag = false;
    }
    else
    {
        if( clock_gettime( CLOCK_REALTIME, &readtime ) == -1 )
        {
            // this should never happen, it's just a safeguard to check for -1
            haveTime = false;
        }
        else
        {
            // calculate 10 seconds from now
            readtime.tv_sec += READ_TIMEOUT_SECS;
            if( readtime.tv_nsec >= (1000 * 1000 * 1000) )  // make sure it is normalized
            {
                readtime.tv_sec += (readtime.tv_nsec / (1000 * 1000 * 1000));
                readtime.tv_nsec %= (1000 * 1000 * 1000);
            }
            haveTime = true;
        }
        errorFlag = true;  // this variable will be returned, so preset the variable to
                           // "a message has been read"
        totalBytesRead = 0;
        while( totalBytesRead < toRead && errorFlag == true && !timedOut )
        {
            bytesToRead = toRead - totalBytesRead;
            readRet = read_test( socketFD, &((char*)buffer)[totalBytesRead], bytesToRead );
            if( readRet == -1 )     // if error
            {
                if( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK )
                {
                    // retry, but if the total elapsed time is more than READ_TIMEOUT_SECS
                    // seconds, give up because something must be seriously
                    // wrong. The caller will then loop around and create a new
                    // socket.
                    if( haveTime && exceededTimeout( readtime ) )
                    {
                        timedOut = true;
                    }
                    else if( errno != EINTR )
                    {
                        // if the read() would have blocked, it's something
                        // kind of weird because the first byte (size byte)
                        // came in OK before this function was called. But
                        // keep trying every 10 milliseconds until it succeeds
                        // or we time out
                        usleep( 10000 );
                    }
                }
                else
                {
                    errorFlag = false;  // error, need a new socket, exit loop
                }
            }
            else    // else if read() != -1 (no read error from socket)
            {
                totalBytesRead += readRet;
                if( (size_t)readRet < bytesToRead )
                {
                    if( haveTime && exceededTimeout( readtime ) )
                    {
                        timedOut = true;
                    }
                }
            }
        }
    }

    if( timedOut )
    {
        errorFlag = false;  // return an error
    }

    return errorFlag;
}

void test1()
{
    offsets[0] = 0;
    offsetidx = 0;
    readret[0] = 1;
    if( !readSocketMessage( 1, 2, scratch, sizeof(scratch) ) )
    {
        printf( "readSocketMessage error %d\n", 1 );
    }
    else
    {
        validateReadBuff( scratch, 1, 1 );
    }
}

void test2()
{
    offsets[0] = 0;
    offsetidx = 0;
    readret[0] = 2;
    if( !readSocketMessage( 2, 2, scratch, sizeof(scratch) ) )
    {
        printf( "readSocketMessage error %d\n", 2 );
    }
    else
    {
        validateReadBuff( scratch, 2, 2 );
    }

}

void test3()
{
    offsets[0] = 0;
    offsets[1] = 1;
    offsetidx = 0;
    readret[0] = 1;
    readret[1] = 1;

    if( !readSocketMessage( 2, 2, scratch, sizeof(scratch) ) )
    {
        printf( "readSocketMessage error %d\n", 2 );
    }
    else
    {
        validateReadBuff( scratch, 2, 3 );
    }
}

void test4()
{
    offsets[0] = 0;
    offsets[1] = 1;
    offsets[2] = 2;
    offsetidx = 0;
    readret[0] = 1;
    readret[1] = 1;
    readret[2] = 1;

    if( !readSocketMessage( 3, 2, scratch, sizeof(scratch) ) )
    {
        printf( "readSocketMessage error %d\n", 2 );
    }
    else
    {
        validateReadBuff( scratch, 3, 4 );
    }
}

void test5()
{
    offsets[0] = 0;
    offsets[1] = 1;
    offsets[2] = 2;
    offsetidx = 0;
    readret[0] = 1;
    readret[1] = 2;
    readret[2] = 1;

    if( !readSocketMessage( 3, 2, scratch, sizeof(scratch) ) )
    {
        printf( "readSocketMessage error %d\n", 2 );
    }
    else
    {
        validateReadBuff( scratch, 3, 4 );
    }
}

void test6()
{
    offsets[0] = 0;
    offsets[1] = 1;
    offsets[2] = 2;
    offsetidx = 0;
    readret[0] = 1;
    readret[1] = 2;
    readret[2] = 1;

    doSleep = true;

    if( readSocketMessage( 3, 2, scratch, sizeof(scratch) ) )
    {
        printf( "readSocketMessage error %d\n", 6 );
    }

    doSleep = false;
}

void test7()
{
    offsets[0] = 0;
    offsets[1] = 1;
    offsets[2] = 1;
    offsetidx = 0;
    readret[0] = 1;
    readret[1] = -1;
    readret[2] = 2;

    errno = EINTR;
    doSleep = false;

    if( !readSocketMessage( 3, 2, scratch, sizeof(scratch) ) )
    {
        printf( "readSocketMessage error %d\n", 7 );
    }
    else
    {
        validateReadBuff( scratch, 3, 4 );
    }
}

void test8()
{
    offsets[0] = 0;
    offsets[1] = 1;
    offsets[2] = 1;
    offsetidx = 0;
    readret[0] = 1;
    readret[1] = -1;
    readret[2] = 2;

    errno = ENOMEM;
    doSleep = false;

    if( readSocketMessage( 3, 2, scratch, sizeof(scratch) ) )
    {
        printf( "readSocketMessage error %d\n", 8 );
    }
}

int main(int argc, char *argv[])
{
    for( int i=0; i<1000; i++ )
    {
        array[i] = i%256;
    }
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();
    test8();

    return 0;
}
