#pragma once

#include "JPEGSamples.h"
#include "CStreamer.h"

#ifdef INCLUDE_SIMDATA
class SimStreamer : public CStreamer
{
    bool m_showBig;
   

public:
    SimStreamer(bool showBig);
    void SelectNextNal(uint8_t *&buf, int &size, uint8_t *&r, int &r_len);
    void StreamNal(RTPMuxContext *ctx, uint8_t *nal, int nal_len);

    virtual void streamImage(uint32_t curMsec);
};
#endif
