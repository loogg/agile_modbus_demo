#ifndef __WIFI_H
#define __WIFI_H
#include <rtthread.h>
#include "ringblk_buf.h"
#include "usr_device.h"
#include "at.h"

#define WIFI_SERVER_MAX_CONN            5
#define WIFI_CLIENT_RBB_BUFSZ           512
#define WIFI_CLIENT_RBB_BLKNUM          10

#define USR_DEVICE_WIFI_CMD_SMART       0x01

typedef enum
{
    WIFI_STATE_RESET = 0,
    WIFI_STATE_POWER_ON,
    WIFI_STATE_PARA_INIT,
    WIFI_STATE_NET_INIT,
    WIFI_STATE_NET_PROCESS
}wifi_state_t;

typedef enum
{
    WIFI_SESSION_STATE_CLOSED = 0,
    WIFI_SESSION_STATE_CLOSING,
    WIFI_SESSION_STATE_CONNECTED,
}wifi_session_state;

struct wifi_session
{
    int link_id;
    rt_tick_t timeout;
    rt_uint8_t recv_rbb_buf[WIFI_CLIENT_RBB_BUFSZ];
    struct rt_rbb_blk recv_rbb_blk[WIFI_CLIENT_RBB_BLKNUM];
    struct rt_rbb recv_rbb;
    wifi_session_state state;
};

struct usr_device_wifi
{
    struct usr_device parent;

    rt_uint8_t init_ok;
    at_client_t client;
    rt_uint8_t smart_flag;
    struct rt_event evt;
    wifi_state_t wifi_state;
    struct wifi_session sessions[WIFI_SERVER_MAX_CONN];
    
    rt_uint16_t error_cnt;
    rt_tick_t wifi_timeout;

    char ssid[100];
    int rssi;
    char ip[16];
    char gateway[16];
    char netmask[16];
};

int wifi_session_send(struct wifi_session *session, const rt_uint8_t *buf, int buf_len);

#endif
