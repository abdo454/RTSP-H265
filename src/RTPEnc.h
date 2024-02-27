/**
 * @file RTPEnc.h
 * @author Abdo Daood (abdo.daood94@gmail.com)
 * @brief
 * @version 0.1
 * @date 2023-11-19
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef RTPSERVER_RTPENC_H
#define RTPSERVER_RTPENC_H

#include "Network.h"

#define RTP_PAYLOAD_MAX 1400
#if !defined(RTP_VERSION)
#define RTP_VERSION 2
#endif // MACRO
#if !defined(RTP_H264)
#define RTP_H264 96
#endif // MACRO
typedef struct
{
    uint8_t cache[RTP_PAYLOAD_MAX + 12 + 4]; // RTP packet = TCP Header (4) + RTP header(12) + buf ()
    uint8_t buf[RTP_PAYLOAD_MAX];        // NAL header + NAL
    uint8_t *buf_ptr;

    int aggregation;  // 0: Single Unit, 1: Aggregation Unit
    int payload_type; // 0, H.264/AVC; 1, HEVC/H.265
    uint32_t ssrc;
    uint32_t seq;
    uint32_t timestamp;
} RTPMuxContext;

int initRTPMuxContext(RTPMuxContext *ctx);

// /* send a H.264/AVC video stream */
// void rtpSendH264AVC(RTPMuxContext *ctx, UDPContext *udp, const uint8_t *buf, int size);

// /* send a H.265/HEVC video stream */
// void rtpSendH265HEVC(RTPMuxContext *ctx, UDPContext *udp, const uint8_t *buf, int size);
// void rtpSendH265HEVCTemp(RTPMuxContext *, UDPContext *, unsigned char const *, int *);
#endif // RTPSERVER_RTPENC_H
