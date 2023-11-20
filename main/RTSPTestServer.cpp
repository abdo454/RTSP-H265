#include "platglue.h"

#include "SimStreamer.h"
#include "CRtspSession.h"
#include <assert.h>
#include <sys/time.h>
#include "RTPEnc.h"
#include "Utils.h"
#include "Network.h"

UDPContext udpContext = {
    .dstIp = "127.0.0.1", // destination ip
    .dstPort = 1234       // destination port
};
void printtime(int counter)
{
    struct timeval nowt;
    gettimeofday(&nowt, NULL); // crufty msecish timer
    uint32_t msect = nowt.tv_sec * 1000 + nowt.tv_usec / 1000;
    printf("time[%d] : %d\r\n", counter, msect);
}
RTPMuxContext rtpMuxContext;
uint8_t *stream = NULL;
int stream_len = 0;
const char *fileName = "../sample_1280x720.hevc";

void workerThread(SOCKET s)
{

    SimStreamer streamer(true); // our streamer for UDP/TCP based RTP transport. 

    streamer.addSession(s)->debug = true; // our threads RTSP session and state
    printtime(1);
    while (streamer.anySessions())
    {
        printf("stream_len %d\r\n", stream_len);

        uint32_t timeout = 10;
        if (!streamer.handleRequests(timeout))
        {
            // struct timeval now;
            // gettimeofday(&now, NULL); // crufty msecish timer
            // uint32_t msec = now.tv_sec * 1000 + now.tv_usec / 1000;
            uint8_t *nal = nullptr;
            int nal_len = 0;
            streamer.SelectNextNal(stream, stream_len, nal, nal_len);
            printf("stream_len %d\r\n", stream_len);
            streamer.StreamNal(&rtpMuxContext, nal, nal_len);
            rtpMuxContext.timestamp += (90000.0 / 30);
            
            usleep(1000000 / 33);
            if (stream_len < 10)
            {
                printf("stream_len < 10\r\n");
                while (1)
                {
                    /* code */
                }
            }
        }
        //  printtime(2);
    }
    //  printtime(4);
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
        udpContext.socket = ClientSocket;
        printf("Client connected. Client address: %s\r\n", inet_ntoa(ClientAddr.sin_addr));
        if (fork() == 0)
            workerThread(ClientSocket);
        printf("Return to search for new clients\r\n");
    }

    closesocket(MasterSocket);

    return 0;
}
