
#include "SimStreamer.h"
#include "AVC.h"
#include "RTPEnc.h"


SimStreamer::SimStreamer(bool ) : CStreamer( 800 ,   480)
{
  
}

void SimStreamer::streamImage(uint32_t curMsec)
{
}
void SimStreamer::StreamNal(RTPMuxContext *ctx, uint8_t *nal, int nal_len)
{
    rtpSendNALH265(ctx, nal, nal_len, 0);
}
void SimStreamer::SelectNextNal(uint8_t *&buf, int &size, uint8_t *&r, int &r_len)
{
    const uint8_t *end = buf + size;
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
    const uint8_t *r1;
    while (!*(r++))
        ; // skip current startcode
    r1 = ff_avc_find_startcode(r, end); // find next startcode
    r_len = (int)(r1 - r);
    buf = (uint8_t *)r1;
    size = size - r_len - 4;
}
