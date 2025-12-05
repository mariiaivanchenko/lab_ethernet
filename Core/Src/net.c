#include "net.h"
#include "ILI9341.h"


uint8_t mac_addr[6] = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x33};

uint8_t dhcp_buffer[1024] = {0};


void Callback_IPConflict(void) {
    printf("ERROR: IP Conflict detected!\r\n");
}


void Net_Init(void) {

	setSHAR(mac_addr);

    DHCP_init(0, dhcp_buffer);

    reg_dhcp_cbfunc(Callback_IPConflict, Callback_IPConflict, Callback_IPConflict);

    printf("Connecting to DHCP...\r\n");

    uint32_t start_time = HAL_GetTick();
    uint8_t dhcp_status = DHCP_run();

    while (dhcp_status == DHCP_RUNNING) {

    	static uint32_t sec_timer = 0;
        if (HAL_GetTick() - sec_timer >= 1000) {
            sec_timer = HAL_GetTick();
            DHCP_time_handler();
            printf(".");
        }

        dhcp_status = DHCP_run();

        if (HAL_GetTick() - start_time > 10000) {
            dhcp_status = DHCP_FAILED;
            break;
        }
    }
    printf("\r\n");

    wiz_NetInfo net_info;

    if (dhcp_status == DHCP_IP_LEASED) {
        printf("DHCP Success!\r\n");
        getIPfromDHCP(net_info.ip);
        getGWfromDHCP(net_info.gw);
        getSNfromDHCP(net_info.sn);
        getDNSfromDHCP(net_info.dns);

        for(int i=0; i<6; i++) net_info.mac[i] = mac_addr[i];
        net_info.dhcp = NETINFO_DHCP;
    }
    else {
        printf("DHCP Failed. Using Static IP.\r\n");

        uint8_t static_ip[4] = {192, 168, 4, 50};
        uint8_t static_gw[4] = {192, 168, 4, 1};
        uint8_t static_sn[4] = {255, 255, 255, 0};
        uint8_t static_dns[4] = {8, 8, 8, 8};

        for(int i=0; i<4; i++) {
            net_info.ip[i] = static_ip[i];
            net_info.gw[i] = static_gw[i];
            net_info.sn[i] = static_sn[i];
            net_info.dns[i] = static_dns[i];
        }
        for(int i=0; i<6; i++) net_info.mac[i] = mac_addr[i];
        net_info.dhcp = NETINFO_STATIC;
    }

    wizchip_setnetinfo(&net_info);
    wizchip_getnetinfo(&net_info);

    char buf[64];  // buffer for formatted text
	  // MAC
	  snprintf(buf, sizeof(buf), "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
			   net_info.mac[0], net_info.mac[1], net_info.mac[2],
			   net_info.mac[3], net_info.mac[4], net_info.mac[5]);
	  ILI9341_draw_text(&hspi2, &ILI9341_Screen, 220, 10, buf, 0xF800);

	  // IP
	  snprintf(buf, sizeof(buf), "IP:  %d.%d.%d.%d",
			   net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3]);
	  ILI9341_draw_text(&hspi2, &ILI9341_Screen, 220, 30, buf, 0x07E0);

	  // Gateway
	  snprintf(buf, sizeof(buf), "GW:  %d.%d.%d.%d",
			   net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3]);
	  ILI9341_draw_text(&hspi2, &ILI9341_Screen, 220, 50, buf, 0x001F);

	  // Subnet mask
	  snprintf(buf, sizeof(buf), "MSK: %d.%d.%d.%d",
			   net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
	  ILI9341_draw_text(&hspi2, &ILI9341_Screen, 220, 70, buf, 0xFFE0);


}
