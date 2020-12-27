#ifndef __WIFI_H
#define __WIFI_H
#include <rtthread.h>
#include "ringblk_buf.h"

#define WIFI_SERVER_MAX_CONN        5

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
    rt_rbb_t recv_rbb;
    wifi_session_state state;
};

struct wifi_device
{
    rt_mutex_t mtx;
    rt_event_t evt;
    wifi_state_t wifi_state;
    struct wifi_session sessions[WIFI_SERVER_MAX_CONN];
    
    rt_uint16_t error_cnt;
    rt_tick_t wifi_timeout;

    char ip[16];
    char gateway[16];
    char netmask[16];
};

int wifi_session_send(struct wifi_session *session, const rt_uint8_t *buf, int buf_len);

#endif
