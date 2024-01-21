#pragma once

#include "LinkedListElement.h"
#include "CStreamer.h"
#include "platglue.h"

// supported command types
enum RTSP_CMD_TYPES
{
    RTSP_OPTIONS,
    RTSP_DESCRIBE,
    RTSP_SETUP,
    RTSP_PLAY,
    RTSP_TEARDOWN,
    RTSP_UNKNOWN
};

#define RTSP_BUFFER_SIZE       500  //10000 -> 500 MDAOOD  // for incoming requests, and outgoing responses
#define RTSP_PARAM_STRING_MAX  50   //200 -> 50 MDAOOD
#define MAX_HOSTNAME_LEN       56   //256 -> 56 MDAOOD

class CRtspSession : public LinkedListElement
{
public:
    CRtspSession( SOCKET aRtspClient, CStreamer * aStreamer );
    ~CRtspSession();

    RTSP_CMD_TYPES Handle_RtspRequest( char *aRequest, unsigned aRequestSize );
    int            GetStreamID();

    /**
       Read from our socket, parsing commands as possible.

       return false if the read timed out
     */
    bool handleRequests(uint32_t readTimeoutMs);

    bool m_streaming;
    bool m_stopped;

    void InitTransport(u_short aRtpPort, u_short aRtcpPort);

    bool isTcpTransport() { return m_TcpTransport; }
    SOCKET& getClient() { return m_RtspClient; }
    
    uint16_t getRtpClientPort() { return m_RtpClientPort; }

    bool debug; /// set to true to get a load of output
private:
    void newCommandInit();
    bool ParseRtspRequest( char * aRequest, unsigned aRequestSize );
    char const * DateHeader();

    // RTSP request command handlers
    void Handle_RtspOPTION();
    void Handle_RtspDESCRIBE();
    void Handle_RtspSETUP();
    void Handle_RtspPLAY();

    // global session state parameters
    int m_RtspSessionID;
    SOCKET m_Client;
    SOCKET m_RtspClient;                                      /// RTSP socket of that session
    int m_StreamID;                                           /// number of simulated stream of that session
    IPPORT m_ClientRTPPort;                                   /// client port for UDP based RTP transport
    IPPORT m_ClientRTCPPort;                                  /// client port for UDP based RTCP transport
    bool m_TcpTransport;                                      /// if Tcp based streaming was activated
    CStreamer    * m_Streamer;                                /// the UDP or TCP streamer of that session

    // parameters of the last received RTSP request
    RTSP_CMD_TYPES m_RtspCmdType;                             /// command type (if any) of the current request
    char m_CommandPresentationPart[RTSP_PARAM_STRING_MAX];        /// stream name pre suffix
    char m_CommandStreamPart[RTSP_PARAM_STRING_MAX];              /// stream name suffix
    char m_CommandHostPort[MAX_HOSTNAME_LEN];                     /// host:port part of the URL
    unsigned m_CSeq;                                          /// RTSP command sequence number
    unsigned m_ContentLength;                                 /// SDP string size

    uint16_t m_RtpClientPort;      // RTP receiver port on client (in host byte order!)
    uint16_t m_RtcpClientPort;     // RTCP receiver port on client (in host byte order!)
};
