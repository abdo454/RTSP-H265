/**
 * @file AVC.h
 * @author Abdo Daood (abdo.daood94@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef RTPSERVER_AVC_H
#define RTPSERVER_AVC_H

#include <stdint.h>

/* copied from FFmpeg libavformat/acv.c */
const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end);

#endif //RTPSERVER_AVC_H
