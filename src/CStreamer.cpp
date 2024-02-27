#include "CStreamer.h"
#include "CRtspSession.h"
#include "Utils.h"
#include <stdio.h>

CStreamer::CStreamer(u_short width, u_short height) : m_Clients()
{
    printf("Creating TSP streamer\n");
    m_RtpServerPort = 0;
    m_RtcpServerPort = 0;

    m_SequenceNumber = 0;
    m_Timestamp = 0;
    m_SendIdx = 0;

    m_RtpSocket = NULLSOCKET;
    m_RtcpSocket = NULLSOCKET;

    m_width = width;
    m_height = height;
    m_prevMsec = 0;

    m_udpRefCount = 0;

    debug = false;

    m_URIHost = "127.0.0.1:554";
    m_URIPresentation = "live";
    m_URIStream = "1";
}

CStreamer::~CStreamer()
{
    LinkedListElement *element = m_Clients.m_Next;
    CRtspSession *session = NULL;
    while (element != &m_Clients)
    {
        session = static_cast<CRtspSession *>(element);
        element = element->m_Next;
        delete session;
    }
};

CRtspSession *CStreamer::addSession(SOCKET aClient)
{
    // if ( debug ) printf("CStreamer::addSession\n");
    CRtspSession *session = new CRtspSession(aClient, this); // our threads RTSP session and state
    // we have it stored in m_Clients
    session->debug = debug;
    return session;
}

void CStreamer::setURI(String hostport, String pres, String stream) // set URI parts for sessions to use.
{
    m_URIHost = hostport;
    m_URIPresentation = pres;
    m_URIStream = stream;
}
int CStreamer::rtpSendData(RTPMuxContext *ctx, const uint8_t *buf, int len, int mark)
{
    // printf("rtpSendData\r\n");
    // int res = 0;

    /* build the RTP header */
    /*
     *
     *    0                   1                   2                   3
     *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                           timestamp                           |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |           synchronization source (SSRC) identifier            |
     *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
     *   |            contributing source (CSRC) identifiers             |
     *   :                             ....                              :
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *
     **/
    // Prepare the first 4 byte of the packet. This is the Rtp over Rtsp header in case of TCP based transport
    ctx->cache[0] = '$'; // magic number
    ctx->cache[1] = 0;   // number of multiplexed subchannel on RTPS connection - here the RTP channel
    ctx->cache[2] = ((len + 12) & 0x0000FF00) >> 8;
    ctx->cache[3] = ((len + 12) & 0x000000FF);
    // Prepare the 12 byte RTP header
    uint8_t *pos = &ctx->cache[4];
    pos[0] = (RTP_VERSION << 6) & 0xff;                           // V P X CC
    pos[1] = (uint8_t)((RTP_H264 & 0x7f) | ((mark & 0x01) << 7)); // M PayloadType
    Load16(&pos[2], (uint16_t)ctx->seq);                          // Sequence number
    Load32(&pos[4], ctx->timestamp);
    Load32(&pos[8], ctx->ssrc);

    /* copy payload data */
    memcpy(&pos[12], buf, len);

    IPADDRESS otherip;
    IPPORT otherport;

    // RTP marker bit must be set on last fragment
    LinkedListElement *element = m_Clients.m_Next;
    CRtspSession *session = NULL;
    while (element != &m_Clients)
    {
        session = static_cast<CRtspSession *>(element);
        if (session->m_streaming && !session->m_stopped)
        {
            if (session->isTcpTransport()) // RTP over RTSP - we send the buffer + 4 byte additional header
            {
                socketsend(session->getClient(), ctx->cache, (uint32_t)(len + 12 + 4));
            }
            else // UDP - we send just the buffer by skipping the 4 byte RTP over RTSP header
            {
                socketpeeraddr(session->getClient(), &otherip, &otherport);
                udpsocketsend(m_RtpSocket, &ctx->cache[4], (uint32_t)(len + 12), otherip, session->getRtpClientPort());
            }
        }
        element = element->m_Next;
    }

    // printf("rtpSendData cache [%d]: ", res);
    // for (int i = 0; i < 20; ++i)
    // {
    //     printf("%.2X ", ctx->cache[i]);
    // }
    // printf("\n\n");

    memset(ctx->cache, 0, RTP_PAYLOAD_MAX + 10);

    ctx->buf_ptr = ctx->buf; // restore buf_ptr

    ctx->seq = (ctx->seq + 1) & 0xffff;
    return 0;
}
u_short CStreamer::GetRtpServerPort()
{
    return m_RtpServerPort;
};

u_short CStreamer::GetRtcpServerPort()
{
    return m_RtcpServerPort;
};

bool CStreamer::InitUdpTransport(void)
{
    if (m_udpRefCount != 0)
    {
        ++m_udpRefCount;
        return true;
    }

    for (u_short P = 6970; P < 0xFFFE; P += 2)
    {
        m_RtpSocket = udpsocketcreate(P);
        if (m_RtpSocket)
        { // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
            m_RtcpSocket = udpsocketcreate(P + 1);
            if (m_RtcpSocket)
            {
                m_RtpServerPort = P;
                m_RtcpServerPort = P + 1;
                break;
            }
            else
            {
                udpsocketclose(m_RtpSocket);
                udpsocketclose(m_RtcpSocket);
            };
        }
    };
    ++m_udpRefCount;
    return true;
}

void CStreamer::ReleaseUdpTransport(void)
{
    --m_udpRefCount;
    if (m_udpRefCount == 0)
    {
        m_RtpServerPort = 0;
        m_RtcpServerPort = 0;
        udpsocketclose(m_RtpSocket);
        udpsocketclose(m_RtcpSocket);

        m_RtpSocket = NULLSOCKET;
        m_RtcpSocket = NULLSOCKET;
    }
}

/**
   Call handleRequests on all sessions
 */
bool CStreamer::handleRequests(uint32_t readTimeoutMs)
{
    bool retVal = true;
    LinkedListElement *element = m_Clients.m_Next;
    while (element != &m_Clients)
    {
        CRtspSession *session = static_cast<CRtspSession *>(element);
        retVal &= session->handleRequests(readTimeoutMs);

        element = element->m_Next;

        if (session->m_stopped)
        {
            // remove session here, so we wont have to send to it
            delete session;
        }
    }

    return retVal;
}

#include <assert.h>

void CStreamer::rtpSendNALH265(RTPMuxContext *ctx, const uint8_t *nal, int size, int last)
{

    // printf("NALU len = %d M=%d\n", size, last);

    // Single NAL Packet or Aggregation Packets
    if (size <= RTP_PAYLOAD_MAX)
    {

        // Aggregation Packets
        // usually multiple small Nal Unit are encapsulated into an RTP package to reduce the overhead of the RTP package
        if (ctx->aggregation)
        {
            /*                       1                   2                   3
             *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *  |   PayloadHdr (Type=48)        |   NALU 1 Size                 |
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *  |           NALU 1 HDR          |   NALU 1 Data                 |
             *  |           ...                         ....                    |
             *                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *  |   ...         |      NALU 2 Size              | NALU 2 HDR    |
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *  |    NALU 2 HDR |   NALU 2 Data         ...                     |
             *  |           ...                         ...                     |
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             * */
            int buffered_size = (int)(ctx->buf_ptr - ctx->buf); // size of data in ctx->buf

            // The remaining space in ctx->buf is less than the required space
            // if (buffered_size + 2B for NALU_Size+  2B for NALU_Hdr + size > RTP_PAYLOAD_MAX)
            if (buffered_size + 2 + 2 + size > RTP_PAYLOAD_MAX)
            {
                rtpSendData(ctx, ctx->buf, buffered_size, 0);
                buffered_size = 0;
            }

            /* NRI(nal_ref_idc): Same as H264 encoding specification, the original code stream NRI value can be used directly here.  */
            uint8_t curNRI = (uint8_t)(nal[0] & 0x60); // NAL NRI

            if (buffered_size == 0)
            {
                *ctx->buf_ptr++ = (uint8_t)(24 | curNRI); // 0x18
            }
            else
            {
                uint8_t lastNRI = (uint8_t)(ctx->buf[0] & 0x60);
                if (curNRI > lastNRI)
                { // if curNRI > lastNRI, use new curNRI
                    ctx->buf[0] = (uint8_t)((ctx->buf[0] & 0x9F) | curNRI);
                }
            }

            // set STAP-A/AP NAL Header F = 1, if this NAL F is 1.
            ctx->buf[0] |= (nal[0] & 0x80);

            // NALU Size + NALU Header + NALU Data
            Load16(ctx->buf_ptr, (uint16_t)size); // NAL size
            ctx->buf_ptr += 2;
            memcpy(ctx->buf_ptr, nal, size); // NALU Header & Data
            ctx->buf_ptr += size;

            // meet last NAL, send all buf
            if (last == 1)
            {
                rtpSendData(ctx, ctx->buf, (int)(ctx->buf_ptr - ctx->buf), 1);
            }
        }
        // Single NAL Unit RTP Packet
        else
        {
            // the bits are directly used as loads
            /*
             *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 5 7 0 1 2 3 4 5 6 . . .
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *  |F|    Type   | LayerId   | TID | NAL unit payload data  ... |
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             * */
            rtpSendData(ctx, nal, size, last);
        }
    }
    else
    { // Fragmentation Unit
        // check if already there is some data
        if (ctx->buf_ptr > ctx->buf)
        {
            rtpSendData(ctx, ctx->buf, (int)(ctx->buf_ptr - ctx->buf), 0);
        }
        uint8_t header_Size = 0;
        uint8_t nalu_type = (nal[0] >> 1) & 0x3F;

        /*                       1                   2                   3
         *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |   PayloadHdr (Type=49)        |    FU header  |               |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |   ...         |         FU payload                            |
         *  |   ...                                 ...                     |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

        /*
         *   PayloadHdr (Type=49)
         *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 5
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |F|    Type   | LayerId   | TID |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *      F       = 0
         *      Type    = 49 (fragmention uint FU)
         *      LayerId = 0
         *      TID     = 1
         */

        ctx->buf[0] = (49 << 1);
        ctx->buf[1] = 1;

        /* Create the FU header
         *
         *   0 1 2 3 4 5 6 7
         *  +-+-+-+-+-+-+-+-+
         *  |S|E|   FuType  |
         *  +-+-+-+-+-+-+-+-+
         * S start : Variable
         * E End   : Variable
         * FuType  :  nalu_type
         */
        ctx->buf[2] = nalu_type;
        /* set the S bit : mark as start fragment */
        ctx->buf[2] |= 1 << 7; // S=1 , E=0
        nal += 2;              // pass the orginal Nal Header , we already captures them
        size -= 2;
        header_Size = 3; //    sizeof(PayloadHdr)+sizeof(FU header)
        while (size + header_Size > RTP_PAYLOAD_MAX)
        {
            memcpy(&ctx->buf[header_Size], nal, (size_t)(RTP_PAYLOAD_MAX - header_Size));
            rtpSendData(ctx, ctx->buf, RTP_PAYLOAD_MAX, 0);
            nal += RTP_PAYLOAD_MAX - header_Size;
            size -= RTP_PAYLOAD_MAX - header_Size;
            ctx->buf[2] &= 0x7f; // S=0 , E=0
        }
        ctx->buf[2] |= 0x40; // S=0 , E=1
        memcpy(&ctx->buf[header_Size], nal, size);
        rtpSendData(ctx, ctx->buf, size + header_Size, last);
    }
}