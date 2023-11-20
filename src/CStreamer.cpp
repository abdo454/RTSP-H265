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
    m_URIPresentation = "mjpeg";
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

int CStreamer::SendRtpPacket(unsigned const char *jpeg, int jpegLen, int fragmentOffset, BufPtr quant0tbl, BufPtr quant1tbl)
{
    // if ( debug ) printf("CStreamer::SendRtpPacket offset:%d - begin\n", fragmentOffset);
#define KRtpHeaderSize 12 // size of the RTP header
#define KJpegHeaderSize 8 // size of the special JPEG payload header

#define MAX_FRAGMENT_SIZE 1100 // FIXME, pick more carefully
    int fragmentLen = MAX_FRAGMENT_SIZE;
    if (fragmentLen + fragmentOffset > jpegLen) // Shrink last fragment if needed
        fragmentLen = jpegLen - fragmentOffset;

    bool isLastFragment = (fragmentOffset + fragmentLen) == jpegLen;

    if (!m_Clients.NotEmpty())
    {
        return isLastFragment ? 0 : fragmentOffset;
    }

    // Do we have custom quant tables? If so include them per RFC

    bool includeQuantTbl = quant0tbl && quant1tbl && fragmentOffset == 0;
    uint8_t q = includeQuantTbl ? 128 : 0x5e;

    static char RtpBuf[2048]; // Note: we assume single threaded, this large buf we keep off of the tiny stack
    int RtpPacketSize = fragmentLen + KRtpHeaderSize + KJpegHeaderSize + (includeQuantTbl ? (4 + 64 * 2) : 0);

    memset(RtpBuf, 0x00, sizeof(RtpBuf));
    // Prepare the first 4 byte of the packet. This is the Rtp over Rtsp header in case of TCP based transport
    RtpBuf[0] = '$'; // magic number
    RtpBuf[1] = 0;   // number of multiplexed subchannel on RTPS connection - here the RTP channel
    RtpBuf[2] = (RtpPacketSize & 0x0000FF00) >> 8;
    RtpBuf[3] = (RtpPacketSize & 0x000000FF);
    // Prepare the 12 byte RTP header
    RtpBuf[4] = 0x80;                                  // RTP version
    RtpBuf[5] = 0x1a | (isLastFragment ? 0x80 : 0x00); // JPEG payload (26) and marker bit
    RtpBuf[7] = m_SequenceNumber & 0x0FF;              // each packet is counted with a sequence counter
    RtpBuf[6] = m_SequenceNumber >> 8;
    RtpBuf[8] = (m_Timestamp & 0xFF000000) >> 24; // each image gets a timestamp
    RtpBuf[9] = (m_Timestamp & 0x00FF0000) >> 16;
    RtpBuf[10] = (m_Timestamp & 0x0000FF00) >> 8;
    RtpBuf[11] = (m_Timestamp & 0x000000FF);
    RtpBuf[12] = 0x13; // 4 byte SSRC (sychronization source identifier)
    RtpBuf[13] = 0xf9; // we just an arbitrary number here to keep it simple
    RtpBuf[14] = 0x7e;
    RtpBuf[15] = 0x67;

    // Prepare the 8 byte payload JPEG header
    RtpBuf[16] = 0x00;                                // type specific
    RtpBuf[17] = (fragmentOffset & 0x00FF0000) >> 16; // 3 byte fragmentation offset for fragmented images
    RtpBuf[18] = (fragmentOffset & 0x0000FF00) >> 8;
    RtpBuf[19] = (fragmentOffset & 0x000000FF);

    /*    These sampling factors indicate that the chrominance components of
       type 0 video is downsampled horizontally by 2 (often called 4:2:2)
       while the chrominance components of type 1 video are downsampled both
       horizontally and vertically by 2 (often called 4:2:0). */
    RtpBuf[20] = 0x00;         // type (fixme might be wrong for camera data) https://tools.ietf.org/html/rfc2435
    RtpBuf[21] = q;            // quality scale factor was 0x5e
    RtpBuf[22] = m_width / 8;  // width  / 8
    RtpBuf[23] = m_height / 8; // height / 8

    int headerLen = 24; // Inlcuding jpeg header but not qant table header
    if (includeQuantTbl)
    { // we need a quant header - but only in first packet of the frame
        // if ( debug ) printf("inserting quanttbl\n");
        RtpBuf[24] = 0; // MBZ
        RtpBuf[25] = 0; // 8 bit precision
        RtpBuf[26] = 0; // MSB of lentgh

        int numQantBytes = 64;         // Two 64 byte tables
        RtpBuf[27] = 2 * numQantBytes; // LSB of length

        headerLen += 4;

        memcpy(RtpBuf + headerLen, quant0tbl, numQantBytes);
        headerLen += numQantBytes;

        memcpy(RtpBuf + headerLen, quant1tbl, numQantBytes);
        headerLen += numQantBytes;
    }
    // if ( debug ) printf("Sending timestamp %d, seq %d, fragoff %d, fraglen %d, jpegLen %d\n", m_Timestamp, m_SequenceNumber, fragmentOffset, fragmentLen, jpegLen);

    // append the JPEG scan data to the RTP buffer
    memcpy(RtpBuf + headerLen, jpeg + fragmentOffset, fragmentLen);
    fragmentOffset += fragmentLen;

    m_SequenceNumber++; // prepare the packet counter for the next packet

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
                socketsend(session->getClient(), RtpBuf, RtpPacketSize + 4);
            else // UDP - we send just the buffer by skipping the 4 byte RTP over RTSP header
            {
                socketpeeraddr(session->getClient(), &otherip, &otherport);
                udpsocketsend(m_RtpSocket, &RtpBuf[4], RtpPacketSize, otherip, session->getRtpClientPort());
            }
        }
        element = element->m_Next;
    }
    // if ( debug ) printf("CStreamer::SendRtpPacket offset:%d - end\n", fragmentOffset);
    return isLastFragment ? 0 : fragmentOffset;
};

int CStreamer::rtpSendData(RTPMuxContext *ctx, const uint8_t *buf, int len, int mark)
{
    printf("rtpSendData\r\n");
    int res = 0;

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
    uint8_t *pos = ctx->cache;
    pos[0] = (RTP_VERSION << 6) & 0xff;                           // V P X CC
    pos[1] = (uint8_t)((RTP_H264 & 0x7f) | ((mark & 0x01) << 7)); // M PayloadType
    Load16(&pos[2], (uint16_t)ctx->seq);                          // Sequence number
    Load32(&pos[4], ctx->timestamp);
    Load32(&pos[8], ctx->ssrc);

    /* copy av data */
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
                printf("session->isTcpTransport()\r\n while(1)\r\n\n");
                while (1)
                {
                    /* code */
                }

                socketsend(session->getClient(), ctx->cache, (uint32_t)(len + 12));
            }
            else // UDP - we send just the buffer by skipping the 4 byte RTP over RTSP header
            {
                socketpeeraddr(session->getClient(), &otherip, &otherport);
                udpsocketsend(m_RtpSocket, ctx->cache, (uint32_t)(len + 12), otherip, session->getRtpClientPort());
            }
        }
        element = element->m_Next;
    }

    printf("rtpSendData cache [%d]: ", res);
    for (int i = 0; i < 20; ++i)
    {
        printf("%.2X ", ctx->cache[i]);
    }
    printf("\n\n");

    memset(ctx->cache, 0, RTP_PAYLOAD_MAX + 10);

    ctx->buf_ptr = ctx->buf; // restore buf_ptr

    ctx->seq = (ctx->seq + 1) & 0xffff;
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

void CStreamer::streamFrame(unsigned const char *data, uint32_t dataLen, uint32_t curMsec)
{
    if (m_prevMsec == 0) // first frame init our timestamp
        m_prevMsec = curMsec;

    // compute deltat (being careful to handle clock rollover with a little lie)
    uint32_t deltams = (curMsec >= m_prevMsec) ? curMsec - m_prevMsec : 100;
    m_prevMsec = curMsec;

    // locate quant tables if possible
    BufPtr qtable0, qtable1;

    if (!decodeJPEGfile(&data, &dataLen, &qtable0, &qtable1))
    {
        printf("can't decode jpeg data\n");
        return;
    }

    int offset = 0;
    do
    {
        offset = SendRtpPacket(data, dataLen, offset, qtable0, qtable1);
    } while (offset != 0);

    // Increment ONLY after a full frame
    uint32_t units = 90000;                  // Hz per RFC 2435
    m_Timestamp += (units * deltams / 1000); // fixed timestamp increment for a frame rate of 25fps

    m_SendIdx++;
    if (m_SendIdx > 1)
        m_SendIdx = 0;
};

#include <assert.h>

// search for a particular JPEG marker, moves *start to just after that marker
// This function fixes up the provided start ptr to point to the
// actual JPEG stream data and returns the number of bytes skipped
// APP0 e0
// DQT db
// DQT db
// DHT c4
// DHT c4
// DHT c4
// DHT c4
// SOF0 c0 baseline (not progressive) 3 color 0x01 Y, 0x21 2h1v, 0x00 tbl0
// - 0x02 Cb, 0x11 1h1v, 0x01 tbl1 - 0x03 Cr, 0x11 1h1v, 0x01 tbl1
// therefore 4:2:2, with two separate quant tables (0 and 1)
// SOS da
// EOI d9 (no need to strip data after this RFC says client will discard)
bool findJPEGheader(BufPtr *start, uint32_t *len, uint8_t marker)
{
    // per https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
    unsigned const char *bytes = *start;

    // kinda skanky, will break if unlucky and the headers inxlucde 0xffda
    // might fall off array if jpeg is invalid
    // FIXME - return false instead
    while (bytes - *start < *len)
    {
        uint8_t framing = *bytes++; // better be 0xff
        if (framing != 0xff)
        {
            printf("malformed jpeg, framing=%x\n", framing);
            return false;
        }
        uint8_t typecode = *bytes++;
        if (typecode == marker)
        {
            unsigned skipped = bytes - *start;
            // if ( debug ) printf("found marker 0x%x, skipped %d\n", marker, skipped);

            *start = bytes;

            // shrink len for the bytes we just skipped
            *len -= skipped;

            return true;
        }
        else
        {
            // not the section we were looking for, skip the entire section
            switch (typecode)
            {
            case 0xd8: // start of image
            {
                break; // no data to skip
            }
            case 0xe0: // app0
            case 0xdb: // dqt
            case 0xc4: // dht
            case 0xc0: // sof0
            case 0xda: // sos
            {
                // standard format section with 2 bytes for len.  skip that many bytes
                uint32_t len = bytes[0] * 256 + bytes[1];
                // if ( debug ) printf("skipping section 0x%x, %d bytes\n", typecode, len);
                bytes += len;
                break;
            }
            default:
                printf("unexpected jpeg typecode 0x%x\n", typecode);
                break;
            }
        }
    }

    printf("failed to find jpeg marker 0x%x", marker);
    return false;
}

// the scan data uses byte stuffing to guarantee anything that starts with 0xff
// followed by something not zero, is a new section.  Look for that marker and return the ptr
// pointing there
void skipScanBytes(BufPtr *start)
{
    BufPtr bytes = *start;

    while (true)
    { // FIXME, check against length
        while (*bytes++ != 0xff)
            ;
        if (*bytes++ != 0)
        {
            *start = bytes - 2; // back up to the 0xff marker we just found
            return;
        }
    }
}
void nextJpegBlock(BufPtr *bytes)
{
    uint32_t len = (*bytes)[0] * 256 + (*bytes)[1];
    // if ( debug ) printf("going to next jpeg block %d bytes\n", len);
    *bytes += len;
}

// When JPEG is stored as a file it is wrapped in a container
// This function fixes up the provided start ptr to point to the
// actual JPEG stream data and returns the number of bytes skipped
bool decodeJPEGfile(BufPtr *start, uint32_t *len, BufPtr *qtable0, BufPtr *qtable1)
{
    // per https://en.wikipedia.org/wiki/JPEG_File_Interchange_Format
    unsigned const char *bytes = *start;

    if (!findJPEGheader(&bytes, len, 0xd8)) // better at least look like a jpeg file
        return false;                       // FAILED!

    // Look for quant tables if they are present
    *qtable0 = NULL;
    *qtable1 = NULL;
    BufPtr quantstart = *start;
    uint32_t quantlen = *len;
    if (!findJPEGheader(&quantstart, &quantlen, 0xdb))
    {
        printf("error can't find quant table 0\n");
    }
    else
    {
        // if ( debug ) printf("found quant table %x\n", quantstart[2]);

        *qtable0 = quantstart + 3; // 3 bytes of header skipped
        nextJpegBlock(&quantstart);
        if (!findJPEGheader(&quantstart, &quantlen, 0xdb))
        {
            printf("error can't find quant table 1\n");
        }
        else
        {
            // if ( debug ) printf("found quant table %x\n", quantstart[2]);
        }
        *qtable1 = quantstart + 3;
        nextJpegBlock(&quantstart);
    }

    if (!findJPEGheader(start, len, 0xda))
        return false; // FAILED!

    // Skip the header bytes of the SOS marker FIXME why doesn't this work?
    uint32_t soslen = (*start)[0] * 256 + (*start)[1];
    *start += soslen;
    *len -= soslen;

    // start scanning the data portion of the scan to find the end marker
    BufPtr endmarkerptr = *start;
    uint32_t endlen = *len;

    skipScanBytes(&endmarkerptr);
    if (!findJPEGheader(&endmarkerptr, &endlen, 0xd9))
        return false; // FAILED!

    // endlen must now be the # of bytes between the start of our scan and
    // the end marker, tell the caller to ignore bytes afterwards
    *len = endmarkerptr - *start;

    return true;
}

void CStreamer::rtpSendNALH265(RTPMuxContext *ctx, const uint8_t *nal, int size, int last)
{

    printf("NALU len = %d M=%d\n", size, last);

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