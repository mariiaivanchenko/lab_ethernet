#include "llmnr.h"
#include "socket.h"
#include "wizchip_conf.h"
#include <string.h>
#include <stdio.h>

#ifndef LLMNR_NAME
#define LLMNR_NAME "STM32"
#endif

#ifndef LLMNR_PORT
#define LLMNR_PORT 5355
#endif

//static uint8_t LLMNR_MCAST_IP[4] = {224, 0, 0, 252};


void LLMNR_Init(uint8_t sock)
{
    uint8_t m_ip[4]  = {224, 0, 0, 252};
    uint8_t m_mac[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0xFC};

    setSn_DHAR(sock, m_mac);

    setSn_DIPR(sock, m_ip);

    setSn_DPORT(sock, LLMNR_PORT);

    if (socket(sock,
               Sn_MR_UDP | Sn_MR_MULTI,
               LLMNR_PORT,
               SF_MULTI_ENABLE) != sock)
    {
        printf("LLMNR socket open failed\r\n");
        return;
    }

    printf("LLMNR: multicast join OK\r\n");
}



void LLMNR_Process(uint8_t sock)
{
	if (getSn_RX_RSR(sock) == 0) {
	        return;
	}
    uint8_t buf[256];
    uint8_t rip[4];
    uint16_t rport;

    int32_t len = recvfrom(sock, buf, sizeof(buf), rip, &rport);
    if (len <= 0) return;

    uint8_t id0 = buf[0];
    uint8_t id1 = buf[1];

    uint8_t *qname = &buf[12];

    if (qname[0] == 5 &&
        qname[1] == 'S' &&
        qname[2] == 'T' &&
        qname[3] == 'M' &&
        qname[4] == '3' &&
        qname[5] == '2')
    {
        LLMNR_SendResponse(sock, id0, id1, rip, rport);
    }
}



// Потрібен прототип функції ініціалізації, щоб перезапустити слухача
void LLMNR_Init(uint8_t sock);

void LLMNR_SendResponse(uint8_t sock, uint8_t id0, uint8_t id1, uint8_t *rip, uint16_t rport)
{
    // 1. Спочатку закриваємо основний сокет, щоб звільнити порт 5355
    close(sock);

    wiz_NetInfo info;
    wizchip_getnetinfo(&info);
    uint8_t ip[4] = {info.ip[0], info.ip[1], info.ip[2], info.ip[3]};
    uint8_t qname[] = { 0x05,'S','T','M','3','2', 0x00 };

    uint8_t packet[512];
    uint16_t p = 0;

    // --- FORM PACKET ---
    packet[p++] = id0;
    packet[p++] = id1;
    packet[p++] = 0x80; packet[p++] = 0x00; // Flags: Response + Authoritative
    packet[p++] = 0x00; packet[p++] = 0x01;
    packet[p++] = 0x00; packet[p++] = 0x01;
    packet[p++] = 0x00; packet[p++] = 0x00;
    packet[p++] = 0x00; packet[p++] = 0x00;

    // Question
    memcpy(&packet[p], qname, sizeof(qname));
    p += sizeof(qname);
    packet[p++] = 0x00; packet[p++] = 0x01;
    packet[p++] = 0x00; packet[p++] = 0x01;

    // Answer
    memcpy(&packet[p], qname, sizeof(qname));
    p += sizeof(qname);
    packet[p++] = 0x00; packet[p++] = 0x01;
    packet[p++] = 0x00; packet[p++] = 0x01;

    // TTL = 30
    packet[p++] = 0x00; packet[p++] = 0x00;
    packet[p++] = 0x00; packet[p++] = 0x1E;

    // Data
    packet[p++] = 0x00; packet[p++] = 0x04;
    memcpy(&packet[p], ip, 4);
    p += 4;

    if(socket(sock, Sn_MR_UDP, 5355, 0) == sock)
    {
        sendto(sock, packet, p, rip, rport);

    }

    close(sock);

    LLMNR_Init(sock);
}




