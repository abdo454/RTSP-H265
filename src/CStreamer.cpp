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
/**
 * General Rulle :
 * Remove "00 00 00 01" from the original code NAL stream, add a tcp (12 Byte + 2Bytes(length)) or udp (12bits) header to the header, and then send it out.
 */

void CStreamer::rtpSendNALH265(RTPMuxContext *ctx, const uint8_t *nal, int size, int last)
{

    // Single NAL Packet or Aggregation Packets
    if (size <= RTP_PAYLOAD_MAX)
    {

        // Handle Aggregation Packets
        // Multiple small NAL units are encapsulated into a single RTP packet to reduce RTP overhead
        // Adding 4 bytes for Payload Header (2 bytes) + NAL Unit size (2 bytes)
        if (ctx->aggregation && size + 4 <= RTP_PAYLOAD_MAX)
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

            int buffered_size = static_cast<int>(ctx->buf_ptr - ctx->buf); // Calculate current buffer size

            // If remaining buffer space is insufficient, send existing data
            if (buffered_size && buffered_size + 2 + size > RTP_PAYLOAD_MAX)
            {
                rtpSendData(ctx, ctx->buf, buffered_size);
                buffered_size = 0;
            }
            /*   PayloadHdr (Type=48)
             *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 5
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *  |F|    Type   | LayerId   | TID |
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *      F       = 0
             *      Type    = 48 (Aggregation Uint AU)
             *      LayerId = 0
             *      TID     = 1
             */
            // First entry in the aggregation packet
            if (!buffered_size)
            {
                ctx->buf[0] = (48 << 1);
                ctx->buf[1] = 1;
                ctx->buf_ptr += 2;
            }

            // Add NALU Size, NALU Header, and NALU Data
            Load16(ctx->buf_ptr, static_cast<uint16_t>(size)); // Load NAL size
            ctx->buf_ptr += 2;
            memcpy(ctx->buf_ptr, nal, size); // Copy NALU Header & Data
            ctx->buf_ptr += size;

            // If this is the last NAL, send the buffer
            if (last == 1)
            {
                rtpSendData(ctx, ctx->buf, static_cast<int>(ctx->buf_ptr - ctx->buf));
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
            rtpSendData(ctx, nal, size);
        }
    }
    else // Fragmentation Unit
    {
        // If buffer has existing data, send it
        if (ctx->buf_ptr > ctx->buf)
        {
            rtpSendData(ctx, ctx->buf, (int)(ctx->buf_ptr - ctx->buf));
        }

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
        // S = 1 (start fragment), E = 0 (not end), FuType = nalu_type
        ctx->buf[2] = nalu_type;
        ctx->buf[2] |= 1 << 7; // Set S=1 , E=0
        nal += 2;              // Skip the original NAL header
        size -= 2;
        const uint8_t header_Size = 3; // sizeof(PayloadHdr) + sizeof(FU header)

        // Fragment the NAL unit and send in multiple RTP packets if necessary
        while (size + header_Size > RTP_PAYLOAD_MAX)
        {
            memcpy(&ctx->buf[header_Size], nal, static_cast<size_t>(RTP_PAYLOAD_MAX - header_Size));
            rtpSendData(ctx, ctx->buf, RTP_PAYLOAD_MAX);
            nal += RTP_PAYLOAD_MAX - header_Size;
            size -= RTP_PAYLOAD_MAX - header_Size;
            ctx->buf[2] &= 0x7f; // Clear S and E bits
        }
        // Final fragment, set E bit to 1
        ctx->buf[2] |= 0x40; 
        memcpy(&ctx->buf[header_Size], nal, size);
        rtpSendData(ctx, ctx->buf, size + header_Size);
    }
}