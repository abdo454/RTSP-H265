#include "platglue.h"

#include "SimStreamer.h"
#include "CRtspSession.h"
#include "JPEGSamples.h"
#include <assert.h>
#include <sys/time.h>
#include "RTPEnc.h"
#include "Utils.h"

void printtime(int counter)
{
    struct timeval nowt;
    gettimeofday(&nowt, NULL); // crufty msecish timer
    uint32_t msect = nowt.tv_sec * 1000 + nowt.tv_usec / 1000;
    printf("time[%d] : %lu\r\n", counter, msect);
}
RTPMuxContext rtpMuxContext;
uint8_t *stream = NULL;
int stream_len = 0;
const char *fileName = "sample_960x540.hevc";

void workerThread(SOCKET s)
{
    SimStreamer streamer(true); // our streamer for UDP/TCP based RTP transport. true == use bigger resolution

    streamer.addSession(s)->debug = true; // our threads RTSP session and state
    printtime(1);
    while (streamer.anySessions())
    {

        uint32_t timeout = 10;
        if (!streamer.handleRequests(timeout))
        {

            struct timeval now;
            gettimeofday(&now, NULL); // crufty msecish timer
            uint32_t msec = now.tv_sec * 1000 + now.tv_usec / 1000;
            streamer.streamImage(msec);
            // control transmission speed
            usleep(1000000 / 10);
        }
        printtime(2);
    }
    printtime(4);
}

int main()
{
    initRTPMuxContext(&rtpMuxContext);
    int res = readFile(&stream, &stream_len, fileName);
    if (res)
    {
        printf("readFile error.\n");
        return -1;
    }

    SOCKET MasterSocket;    // our masterSocket(socket that listens for RTSP client connections)
    SOCKET ClientSocket;    // RTSP socket to handle an client
    sockaddr_in ServerAddr; // server address parameters
    sockaddr_in ClientAddr; // address parameters of a new RTSP client
    socklen_t ClientAddrLen = sizeof(ClientAddr);

    printf("running test RTSP server\n");

    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(8554); // listen on RTSP port 8554
    MasterSocket = socket(AF_INET, SOCK_STREAM, 0);

    int enable = 1;
    if (setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        printf("setsockopt(SO_REUSEADDR) failed");
        return 0;
    }

    // bind our master socket to the RTSP port and listen for a client connection
    if (bind(MasterSocket, (sockaddr *)&ServerAddr, sizeof(ServerAddr)) != 0)
    {
        printf("error can't bind port errno=%d\n", errno);

        return 0;
    }

    if (listen(MasterSocket, 5) != 0)
        return 0;

    while (true)
    { // loop forever to accept client connections
        ClientSocket = accept(MasterSocket, (struct sockaddr *)&ClientAddr, &ClientAddrLen);
        printf("Client connected. Client address: %s\r\n", inet_ntoa(ClientAddr.sin_addr));
        if (fork() == 0)
            workerThread(ClientSocket);
    }

    closesocket(MasterSocket);

    return 0;
}
