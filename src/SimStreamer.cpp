
#include "SimStreamer.h"
#include "JPEGSamples.h"
#include "AVC.h"
#include "RTPEnc.h"
#ifdef INCLUDE_SIMDATA
SimStreamer::SimStreamer(bool showBig) : CStreamer(showBig ? 800 : 640, showBig ? 600 : 480)
{
    m_showBig = showBig;
}

void SimStreamer::streamImage(uint32_t curMsec)
{
    if (m_showBig)
    {
        BufPtr bytes = capture_jpg;
        uint32_t len = capture_jpg_len;

        streamFrame(bytes, len, curMsec);
    }
    else
    {
        BufPtr bytes = octo_jpg;
        uint32_t len = octo_jpg_len;

        streamFrame(bytes, len, curMsec);
    }
}
void SimStreamer::StreamNal(RTPMuxContext *ctx, uint8_t *nal, int nal_len)
{

    rtpSendNALH265(ctx, nal, nal_len, 0);
}
void SimStreamer::SelectNextNal(uint8_t *&buf, int &size, uint8_t *&r, int &r_len)
{
    const uint8_t *end = buf + size;
    // printf("SelectNextNal\n");

    if (NULL == buf || size <= 0)
    {
        printf("%s param error.\n", "rtpSendH265HEVC");
        return;
    }
    r = (uint8_t *)ff_avc_find_startcode(buf, end);

    /*
    startcode
    r[0]:  0x00
    r[1]:  0x00
    r[2]:  0x00
    r[3]:  0x01
    */
    // printf("r %lu\t", r);
    const uint8_t *r1;
    while (!*(r++))
        ; // skip current startcode
    // printf("rn %lu\t", r);
    r1 = ff_avc_find_startcode(r, end); // find next startcode
    // printf("r1 %lu\t", r1);
    r_len = (int)(r1 - r);
    // printf("r_len %lu\t", r_len);
    buf = (uint8_t *)r1;
    // printf("new_buf_start %lu\t", buf);
    size = size - r_len - 4;
    // printf("size %lu\n", size);
}

#endif
