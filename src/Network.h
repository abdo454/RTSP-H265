/**
 * @file Network.h
 * @author Abdo Daood (abdo.daood94@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef RTPSERVER_NETWORK_H
#define RTPSERVER_NETWORK_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct{
    const char *dstIp;
    int dstPort;
    struct sockaddr_in servAddr;
    int socket;
}UDPContext;

/* create UDP socket */
int udpInit(UDPContext *udp);

/* send UDP packet */
int udpSend(const UDPContext *udp, const uint8_t *data, uint32_t len);

#endif //RTPSERVER_NETWORK_H
