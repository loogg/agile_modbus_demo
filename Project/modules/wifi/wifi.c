#include "wifi.h"
#include "drv_gpio.h"
#include "drv_usart.h"
#include "init_module.h"
#include "at.h"
#include "led.h"
#include <string.h>
#include <stdio.h>
#include <rthw.h>

#define DBG_ENABLE
#define DBG_COLOR
#define DBG_SECTION_NAME "wifi"
#define DBG_LEVEL DBG_INFO
#include <rtdbg.h>

#define WIFI_WAIT_CONNECT_TIME          6000
#define WIFI_NET_TIMEOUT                300
#define WIFI_SMART_TIMEOUT              180
#define WIFI_SEND_MAX_SIZE              2048
#define WIFI_CLIENT_RBB_BUFSZ           512
#define WIFI_CLIENT_RBB_BLKNUM          10

#define WIFI_EVENT_SEND_OK              (1UL << 0)
#define WIFI_EVENT_SEND_FAIL            (1UL << 1)

#define WIFI_SET_EVENT(socket, event)   ((1UL << (socket + 16)) | (event))

#define WIFI_AT_SEND_CMD(resp, resp_line, timeout, cmd)                                         \
        (resp) = at_resp_set_info((resp), 512, (resp_line), rt_tick_from_millisecond(timeout));    \
        if (at_exec_cmd((resp), (cmd)) < 0)                                          \
        {                                                                                          \
            break;                                                                                  \
        }

ALIGN(RT_ALIGN_SIZE)
static struct wifi_device wifi_dev = {0};
static int cur_socket = -1;

static int wifi_event_send(uint32_t event)
{
    return (int)rt_event_send(wifi_dev.evt, event);
}

static int wifi_event_recv(uint32_t event, rt_int32_t timeout, rt_uint8_t option)
{
    int result = 0;
    rt_uint32_t recved;

    result = rt_event_recv(wifi_dev.evt, event, option | RT_EVENT_FLAG_CLEAR, timeout, &recved);
    if (result != RT_EOK)
    {
        return -RT_ETIMEOUT;
    }

    return recved;
}

static struct wifi_session *wifi_session_get(int link_id)
{
    if(link_id < 0)
        return RT_NULL;

    struct wifi_session *session = RT_NULL;

    rt_base_t level = rt_hw_interrupt_disable();

    for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
    {
        if(wifi_dev.sessions[i].link_id == link_id)
        {
            session = &(wifi_dev.sessions[i]);
            break;
        }
    }

    rt_hw_interrupt_enable(level);

    return session;
}

static void wifi_sessions_clean(struct wifi_session *session)
{
    rt_base_t level = rt_hw_interrupt_disable();

    if(session == RT_NULL)
    {
        for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
        {
            wifi_dev.sessions[i].link_id = -1;
            wifi_dev.sessions[i].timeout = rt_tick_get();

            rt_rbb_blk_t block = RT_NULL;
            do
            {
                block = rt_rbb_blk_get(wifi_dev.sessions[i].recv_rbb);
                if (block == RT_NULL)
                    break;

                rt_rbb_blk_free(wifi_dev.sessions[i].recv_rbb, block);
            } while (block != RT_NULL);

            wifi_dev.sessions[i].state = WIFI_SESSION_STATE_CLOSED;
        }
    }
    else
    {
        session->link_id = -1;
        session->timeout = rt_tick_get();

        rt_rbb_blk_t block = RT_NULL;
        do
        {
            block = rt_rbb_blk_get(session->recv_rbb);
            if(block == RT_NULL)
                break;

            rt_rbb_blk_free(session->recv_rbb, block);
        }while(block != RT_NULL);

        session->state = WIFI_SESSION_STATE_CLOSED;
    }
    
    rt_hw_interrupt_enable(level);
}

static int wifi_para_init(void)
{
    int result = -RT_ERROR;
    rt_mutex_take(wifi_dev.mtx, RT_WAITING_FOREVER);

    at_response_t resp = at_create_resp(512, 0, 500);
    do
    {
        if(resp == RT_NULL)
        {
            LOG_E("No memory for response structure!");
            break;
        }

        WIFI_AT_SEND_CMD(resp, 0, 500, "ATE0");
        WIFI_AT_SEND_CMD(resp, 0, 500, "AT+GMR");
        for (int i = 0; i < resp->line_counts - 1; i++)
        {
            LOG_I("%s", at_resp_get_line(resp, i + 1));
        }
        rt_thread_mdelay(1000);

        WIFI_AT_SEND_CMD(resp, 0, 500, "AT+CWMODE_CUR=1");

        result = RT_EOK;
    }while(0);

    if(resp)
        at_delete_resp(resp);
    rt_mutex_release(wifi_dev.mtx);

    return result;
}

static int wifi_net_init(void)
{
    int result = -RT_ERROR;
    rt_mutex_take(wifi_dev.mtx, RT_WAITING_FOREVER);

    at_response_t resp = at_create_resp(512, 0, 500);
    do
    {
        if(resp == RT_NULL)
        {
            LOG_E("No memory for response structure!");
            break;
        }

        WIFI_AT_SEND_CMD(resp, 0, 500, "AT+CWDHCP_CUR=1,1");
        rt_thread_mdelay(100);

        at_resp_set_info(resp, 512, 0, 30000);
        if(at_exec_cmd(resp, "AT+CWJAP_CUR=\"%s\",\"%s\"", WIFI_STATION_SSID, WIFI_STATION_PASSWORD) < 0)
        {
            LOG_E("join ap failed.");
            break;
        }
        rt_thread_mdelay(100);

        WIFI_AT_SEND_CMD(resp, 0, 500, "AT+CIPSTA_CUR?");
        rt_thread_mdelay(1000);

        WIFI_AT_SEND_CMD(resp, 0, 500, "AT+CIPMODE=0");
        rt_thread_mdelay(100);

        WIFI_AT_SEND_CMD(resp, 0, 500, "AT+CIPDINFO=0");
        rt_thread_mdelay(100);

        WIFI_AT_SEND_CMD(resp, 0, 500, "AT+CIPMUX=1");
        rt_thread_mdelay(100);

        at_resp_set_info(resp, 512, 0, 5000);
        if(at_exec_cmd(resp, "AT+CIPSERVER=1,%d", WIFI_LISTEN_PORT) < 0)
        {
            LOG_E("listen port %d success.", WIFI_LISTEN_PORT);
            break;
        }
        rt_thread_mdelay(100);

        at_resp_set_info(resp, 512, 0, 500);
        if(at_exec_cmd(resp, "AT+CIPSTO=%d", WIFI_CLIENT_TIMEOUT * 2) < 0)
            break;

        result = RT_EOK;
    }while(0);

    if(resp)
        at_delete_resp(resp);
    rt_mutex_release(wifi_dev.mtx);

    return result;
}

static int wifi_get_ip(void)
{
    int result = -RT_ERROR;
    rt_mutex_take(wifi_dev.mtx, RT_WAITING_FOREVER);

    at_response_t resp = at_create_resp(512, 0, 500);
    do
    {
        if(resp == RT_NULL)
        {
            LOG_E("No memory for response structure!");
            break;
        }

        WIFI_AT_SEND_CMD(resp, 0, 500, "AT+CIPSTA_CUR?");

        result = RT_EOK;
    }while(0);

    if(resp)
        at_delete_resp(resp);
    rt_mutex_release(wifi_dev.mtx);

    return result;
}

static int wifi_socket_close(int socket)
{
    if(socket < 0)
        return -RT_ERROR;

    int result = -RT_ERROR;
    rt_mutex_take(wifi_dev.mtx, RT_WAITING_FOREVER);

    at_response_t resp = at_create_resp(512, 0, 3000);
    do
    {
        if(resp == RT_NULL)
        {
            LOG_E("No memory for response structure!");
            break;
        }

        int rc = at_exec_cmd(resp, "AT+CIPCLOSE=%d", socket);
        if(rc == -RT_ETIMEOUT)
            break;
        
        char query_kw[50];
        snprintf(query_kw, sizeof(query_kw), "+CIPSTATUS:%d", socket);

        WIFI_AT_SEND_CMD(resp, 0, 1000, "AT+CIPSTATUS");
        
        const char *query_result = at_resp_get_line_by_kw(resp, query_kw);
        if(query_result)
            break;
 
        result = RT_EOK;
    }while(0);

    if(result != RT_EOK)
        LOG_E("socket (%d) close failed.", socket);
    else
        LOG_D("socket (%d) close ok.", socket);

    if(resp)
        at_delete_resp(resp);
    rt_mutex_release(wifi_dev.mtx);

    return result;
}

static int wifi_socket_send(int socket, const uint8_t *buf, int buf_len)
{
    if(socket < 0)
        return -RT_ERROR;
    
    if(buf == RT_NULL)
        return -RT_ERROR;
    
    if(buf_len <= 0)
        return buf_len;
    
    int result = -RT_ERROR;
    rt_mutex_take(wifi_dev.mtx, RT_WAITING_FOREVER);

    at_response_t resp = at_create_resp(512, 2, 3000);
    cur_socket = -1;
    do
    {
        if(resp == RT_NULL)
        {
            LOG_E("No memory for response structure!");
            break;
        }

        int send_size = 0, cur_pkt_size = 0;

        int rc = RT_EOK;
        while(send_size < buf_len)
        {
            if(buf_len - send_size < WIFI_SEND_MAX_SIZE)
                cur_pkt_size = buf_len - send_size;
            else
                cur_pkt_size = WIFI_SEND_MAX_SIZE;
            
            wifi_event_recv(WIFI_SET_EVENT(socket, WIFI_EVENT_SEND_OK | WIFI_EVENT_SEND_FAIL), 0, RT_EVENT_FLAG_OR);
            cur_socket = socket;
            if(at_exec_cmd(resp, "AT+CIPSEND=%d,%d", socket, cur_pkt_size) < 0)
            {
                rc = -RT_ERROR;
                break;
            }

            if(at_client_send((const char *)(buf + send_size), cur_pkt_size) == 0)
            {
                rc = -RT_ERROR;
                break;
            }

            if(wifi_event_recv(WIFI_SET_EVENT(socket, 0), 5000, RT_EVENT_FLAG_OR) < 0)
            {
                rc = -RT_ERROR;
                break;
            }

            int event_result = 0;
            if((event_result = wifi_event_recv(WIFI_EVENT_SEND_OK | WIFI_EVENT_SEND_FAIL, 100, RT_EVENT_FLAG_OR)) < 0)
            {
                rc = -RT_ERROR;
                break;
            }

            if(event_result & WIFI_EVENT_SEND_FAIL)
            {
                rc = -RT_ERROR;
                break;
            }

            send_size += cur_pkt_size;
            cur_socket = -1;
        }

        if(rc != RT_EOK)
            break;

        result = RT_EOK;
    }while(0);

    if(result != RT_EOK)
        LOG_E("socket (%d) send failed.", socket);
    else
    {
        led_control(LED_CMD_WIFI_DATA);
        LOG_D("socket (%d) send ok.", socket);
    }

    rt_thread_mdelay(100);
    cur_socket = -1;
    if(resp)
        at_delete_resp(resp);
    rt_mutex_release(wifi_dev.mtx);

    return (result == RT_EOK) ? buf_len : result;
}

static int wifi_session_close(struct wifi_session *session)
{
    struct wifi_session *close_session = RT_NULL;

    rt_base_t level = rt_hw_interrupt_disable();

    do
    {
        if((session->link_id < 0) || (session->state == WIFI_SESSION_STATE_CLOSED))
            break;
        
        close_session = session;
        close_session->state = WIFI_SESSION_STATE_CLOSING;
    }while(0);

    if(close_session == RT_NULL)
    {
        rt_hw_interrupt_enable(level);
        return RT_EOK;
    }

    rt_hw_interrupt_enable(level);

    int result = wifi_socket_close(close_session->link_id);

    if(result == RT_EOK)
    {
        rt_base_t level = rt_hw_interrupt_disable();

        if(close_session->state == WIFI_SESSION_STATE_CLOSING)
            wifi_sessions_clean(close_session);

        rt_hw_interrupt_enable(level);
    }

    return result;
}

int wifi_session_send(struct wifi_session *session, const rt_uint8_t *buf, int buf_len)
{
    if(session->state != WIFI_SESSION_STATE_CONNECTED)
        return -RT_ERROR;
    
    return wifi_socket_send(session->link_id, buf, buf_len);
}

extern int wifi_session_process(struct wifi_session *session, rt_uint8_t *recv_buf, int recv_len);

static int wifi_net_process(void)
{
    for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
    {
        struct wifi_session *session = &(wifi_dev.sessions[i]);

        switch(session->state)
        {
            case WIFI_SESSION_STATE_CLOSED:
            break;

            case WIFI_SESSION_STATE_CLOSING:
            {
                if(wifi_session_close(session) != RT_EOK)
                    wifi_dev.error_cnt++;
            }
            break;

            case WIFI_SESSION_STATE_CONNECTED:
            {
                if((rt_tick_get() - session->timeout) < (RT_TICK_MAX / 2))
                {
                    if(wifi_session_close(session) != RT_EOK)
                        wifi_dev.error_cnt++;

                    break;
                }

                rt_rbb_blk_t block = rt_rbb_blk_get(session->recv_rbb);
                if(block == RT_NULL)
                    break;
                int rc = wifi_session_process(session, block->buf, block->size);
                rt_rbb_blk_free(session->recv_rbb, block);
                
                if(rc != RT_EOK)
                {
                    if(wifi_session_close(session) != RT_EOK)
                        wifi_dev.error_cnt++;
                }
            }
            break;

            default:
            break;
        }    
    }

    for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
    {
        if(wifi_dev.sessions[i].state == WIFI_SESSION_STATE_CONNECTED)
        {
            wifi_dev.wifi_timeout = rt_tick_get() + rt_tick_from_millisecond(WIFI_NET_TIMEOUT * 1000);
            break;
        }
    }

    if((rt_tick_get() - wifi_dev.wifi_timeout) < (RT_TICK_MAX / 2))
        wifi_dev.error_cnt = 99;

    if(wifi_dev.error_cnt > 5)
        return -RT_ERROR;
    
    return RT_EOK;
}

static void wifi_process_entry(void *parameter)
{
    rt_tick_t ip_tick_timeout = rt_tick_get() + rt_tick_from_millisecond(10 * 1000);

    while(1)
    {
        rt_thread_mdelay(10);

        rt_mutex_take(wifi_dev.mtx, RT_WAITING_FOREVER);

        if((rt_tick_get() - ip_tick_timeout) < (RT_TICK_MAX / 2))
        {
            if(wifi_dev.wifi_state == WIFI_STATE_NET_PROCESS)
                wifi_get_ip();

            ip_tick_timeout = rt_tick_get() + rt_tick_from_millisecond(10 * 1000);
        }

        switch(wifi_dev.wifi_state)
        {
            case WIFI_STATE_RESET:
            {
                LOG_W("Reset wifi.");
                led_control(LED_CMD_WIFI_RESET);
                rt_event_control(wifi_dev.evt, RT_IPC_CMD_RESET, RT_NULL);
                wifi_sessions_clean(RT_NULL);
                rt_memset(wifi_dev.ip, 0, sizeof(wifi_dev.ip));
                rt_memset(wifi_dev.gateway, 0, sizeof(wifi_dev.gateway));
                rt_memset(wifi_dev.netmask, 0, sizeof(wifi_dev.netmask));

                drv_pin_write(WIFI_POWER_PIN, PIN_HIGH);
                rt_thread_mdelay(1000);

                drv_pin_write(WIFI_RESET_PIN, PIN_LOW);
                rt_thread_mdelay(200);
                drv_pin_write(WIFI_RESET_PIN, PIN_HIGH);
                rt_thread_mdelay(2000);

                wifi_dev.wifi_state = WIFI_STATE_POWER_ON;
                wifi_dev.error_cnt = 0;
            }
            break;

            case WIFI_STATE_POWER_ON:
            {
                if(at_client_wait_connect(WIFI_WAIT_CONNECT_TIME) == RT_EOK)
                {
                    LOG_I("wifi power on.");
                    wifi_dev.wifi_state = WIFI_STATE_PARA_INIT;
                    led_control(LED_CMD_WIFI_DISCONNECT);
                }
                else
                {
                    LOG_W("wifi wait connect error, now reseting...");
                    wifi_dev.wifi_state = WIFI_STATE_RESET;
                }
            }
            break;

            case WIFI_STATE_PARA_INIT:
            {
                if(wifi_para_init() != RT_EOK)
                {
                    LOG_E("wifi para init failed.");
                    wifi_dev.wifi_state = WIFI_STATE_RESET;
                }
                else
                {
                    LOG_I("wifi para init success.");
                    wifi_dev.wifi_state = WIFI_STATE_NET_INIT;
                }
            }
            break;

            case WIFI_STATE_NET_INIT:
            {
                if(wifi_net_init() != RT_EOK)
                {
                    LOG_E("wifi net init failed.");
                    wifi_dev.wifi_state = WIFI_STATE_RESET;
                }
                else
                {
                    LOG_I("wifi net init success.");
                    wifi_dev.wifi_state = WIFI_STATE_NET_PROCESS;
                    wifi_dev.wifi_timeout = rt_tick_get() + rt_tick_from_millisecond(WIFI_NET_TIMEOUT * 1000);
                    led_control(LED_CMD_WIFI_CONNECTED);
                }
            }
            break;

            case WIFI_STATE_NET_PROCESS:
            {
                if(wifi_net_process() != RT_EOK)
                {
                    LOG_E("wifi net process failed.");
                    wifi_dev.wifi_state = WIFI_STATE_RESET;
                }
            }
            break;

            default:
                wifi_dev.wifi_state = WIFI_STATE_RESET;
            break;
        }

        rt_mutex_release(wifi_dev.mtx);
    }
}

static void urc_cipsta_cb(struct at_client *client, const char *data, rt_size_t size)
{
    const char *start_ptr = strchr(data, '\"');
    if(start_ptr == RT_NULL)
        return;
    start_ptr += 1;
    const char *end_ptr = strchr(start_ptr, '\"');
    if(end_ptr == RT_NULL)
        return;

    int len = end_ptr - start_ptr;
    if(len > 15)
        len = 15;

    if(strstr(data, "+CIPSTA_CUR:ip:"))
    {
        rt_memset(wifi_dev.ip, 0, sizeof(wifi_dev.ip));
        rt_memcpy(wifi_dev.ip, start_ptr, len);
    }
    else if(strstr(data, "+CIPSTA_CUR:gateway:"))
    {
        rt_memset(wifi_dev.gateway, 0, sizeof(wifi_dev.gateway));
        rt_memcpy(wifi_dev.gateway, start_ptr, len);
    }
    else if(strstr(data, "+CIPSTA_CUR:netmask:"))
    {
        rt_memset(wifi_dev.netmask, 0, sizeof(wifi_dev.netmask));
        rt_memcpy(wifi_dev.netmask, start_ptr, len);
    }
}

static void urc_closed_cb(struct at_client *client, const char *data, rt_size_t size)
{
    int socket = 0;
    if(sscanf(data, "%d,CLOSED", &socket) != 1)
        return;
    if(socket < 0)
        return;
    
    if(wifi_dev.wifi_state != WIFI_STATE_NET_PROCESS)
        return;

    rt_base_t level = rt_hw_interrupt_disable();

    do
    {
        struct wifi_session *session = RT_NULL;

        for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
        {
            if ((wifi_dev.sessions[i].link_id >= 0) &&
                (wifi_dev.sessions[i].link_id == socket) &&
                (wifi_dev.sessions[i].state > WIFI_SESSION_STATE_CLOSING))
            {
                session = &(wifi_dev.sessions[i]);
                break;
            }
        }

        if(session == RT_NULL)
            break;

        wifi_sessions_clean(session);
    }while(0);

    rt_hw_interrupt_enable(level);
}

static void urc_connect_cb(struct at_client *client, const char *data, rt_size_t size)
{
    int socket = 0;
    if(sscanf(data, "%d,CONNECT", &socket) != 1)
        return;
    if(socket < 0)
        return;
    
    if(wifi_dev.wifi_state != WIFI_STATE_NET_PROCESS)
        return;

    rt_base_t level = rt_hw_interrupt_disable();

    do
    {
        struct wifi_session *session = wifi_session_get(socket);
        if(session)
        {
            wifi_sessions_clean(session);
            session->link_id = socket;
            session->timeout = rt_tick_get() + rt_tick_from_millisecond(WIFI_CLIENT_TIMEOUT * 1000);
            session->state = WIFI_SESSION_STATE_CONNECTED;
            break;
        }

        session = RT_NULL;
        for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
        {
            if ((wifi_dev.sessions[i].link_id < 0) &&
                (wifi_dev.sessions[i].state == WIFI_SESSION_STATE_CLOSED))
            {
                session = &(wifi_dev.sessions[i]);
                break;
            }
        }

        if (session == RT_NULL)
            break;
        
        session->link_id = socket;
        session->timeout = rt_tick_get() + rt_tick_from_millisecond(WIFI_CLIENT_TIMEOUT * 1000);
        session->state = WIFI_SESSION_STATE_CONNECTED;
    }while(0);

    rt_hw_interrupt_enable(level);
}

static void urc_wifi_disconnect_cb(struct at_client *client, const char *data, rt_size_t size)
{
    if(wifi_dev.wifi_state != WIFI_STATE_NET_PROCESS)
        return;
    
    LOG_E("wifi disconnect.");
    wifi_dev.error_cnt = 99;
}

static void urc_ipd_cb(struct at_client *client, const char *data, rt_size_t size)
{
    if(wifi_dev.wifi_state != WIFI_STATE_NET_PROCESS)
        return;
    
    int socket = 0;
    int len = 0;
    if(sscanf(data, "+IPD,%d,%d:", &socket, &len) != 2)
        return;
    if(socket < 0)
        return;
    if(len <= 0)
        return;
    
    struct wifi_session *session = RT_NULL;

    rt_base_t level = rt_hw_interrupt_disable();

    do
    {
        session = wifi_session_get(socket);
        if(session)
            break;
        
        for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
        {
            if ((wifi_dev.sessions[i].link_id < 0) &&
                (wifi_dev.sessions[i].state == WIFI_SESSION_STATE_CLOSED))
            {
                session = &(wifi_dev.sessions[i]);
                break;
            }
        }

    }while(0);

    if(session == RT_NULL)
    {
        rt_hw_interrupt_enable(level);
        return;
    }

    if(session->state != WIFI_SESSION_STATE_CONNECTED)
    {
        wifi_sessions_clean(session);
        session->link_id = socket;
        session->timeout = rt_tick_get() + rt_tick_from_millisecond(WIFI_CLIENT_TIMEOUT * 1000);
        session->state = WIFI_SESSION_STATE_CONNECTED;
    }

    rt_hw_interrupt_enable(level);
    
    rt_rbb_blk_t block = rt_rbb_blk_alloc(session->recv_rbb, len);
    if(block == RT_NULL)
        return;
    rt_memset(block->buf, 0, block->size);

    if(at_client_recv((char *)(block->buf), block->size, 20) != block->size)
    {
        LOG_E("socket (%d) recv size (%d) data failed.", socket, len);
        rt_rbb_blk_free(session->recv_rbb, block);
        return;
    }

    rt_rbb_blk_put(block);
    session->timeout = rt_tick_get() + rt_tick_from_millisecond(WIFI_CLIENT_TIMEOUT * 1000);
    led_control(LED_CMD_WIFI_DATA);
}

static void urc_send_cb(struct at_client *client, const char *data, rt_size_t size)
{
    if(cur_socket < 0)
        return;
    
    struct wifi_session *session = wifi_session_get(cur_socket);
    if(session == RT_NULL)
        return;
    
    rt_base_t level = rt_hw_interrupt_disable();

    do
    {
        if(session->state != WIFI_SESSION_STATE_CONNECTED)
            break;
        
        if(strstr(data, "SEND OK"))
            wifi_event_send(WIFI_SET_EVENT(session->link_id, WIFI_EVENT_SEND_OK));
        else if(strstr(data, "SEND FAIL"))
            wifi_event_send(WIFI_SET_EVENT(session->link_id, WIFI_EVENT_SEND_FAIL));
    }while(0);

    rt_hw_interrupt_enable(level);
}

static struct at_urc urc_table[] = {
    {"+CIPSTA_CUR:", "\r\n", urc_cipsta_cb},
    {"", ",CLOSED\r\n", urc_closed_cb},
    {"", ",CONNECT\r\n", urc_connect_cb},
    {"", "WIFI DISCONNECT\r\n", urc_wifi_disconnect_cb},
    {"+IPD", ":", urc_ipd_cb},
    {"SEND OK", "\r\n", urc_send_cb},
    {"SEND FAIL", "\r\n", urc_send_cb},
};


static int wifi_init(void)
{
    wifi_dev.mtx = rt_mutex_create("wifi", RT_IPC_FLAG_FIFO);
    RT_ASSERT(wifi_dev.mtx != RT_NULL);
    wifi_dev.evt = rt_event_create("wifi", RT_IPC_FLAG_FIFO);
    RT_ASSERT(wifi_dev.evt != RT_NULL);
    wifi_dev.wifi_state = WIFI_STATE_RESET;
    for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
    {
        wifi_dev.sessions[i].link_id = -1;
        wifi_dev.sessions[i].timeout = rt_tick_get();
        wifi_dev.sessions[i].recv_rbb = rt_rbb_create(WIFI_CLIENT_RBB_BUFSZ, WIFI_CLIENT_RBB_BLKNUM);
        RT_ASSERT(wifi_dev.sessions[i].recv_rbb != RT_NULL);
        wifi_dev.sessions[i].state = WIFI_SESSION_STATE_CLOSED;
    }

    struct usr_device_usart *dev = (struct usr_device_usart *)usr_device_find(WIFI_CLIENT_DEVICE_NAME);
    struct usr_device_usart_parameter parameter = dev->parameter;
    parameter.baudrate = 115200;
    parameter.parity = UART_PARITY_NONE;
    parameter.wlen = UART_WORDLENGTH_8B;
    parameter.stblen = UART_STOPBITS_1;
    usr_device_control(&(dev->parent), USR_DEVICE_USART_CMD_SET_PARAMETER, &parameter);

    at_client_init(WIFI_CLIENT_DEVICE_NAME, WIFI_RECV_BUFF_LEN);
    /* register URC data execution function  */
    at_set_urc_table(urc_table, sizeof(urc_table) / sizeof(urc_table[0]));

    drv_pin_mode(WIFI_POWER_PIN, PIN_MODE_OUTPUT);
    drv_pin_mode(WIFI_RESET_PIN, PIN_MODE_OUTPUT);

    rt_thread_t tid = rt_thread_create("wifi_process", wifi_process_entry, RT_NULL, 2048, 2, 100);
    RT_ASSERT(tid != RT_NULL);
    rt_thread_startup(tid);

    return RT_EOK;
}

static struct init_module wifi_init_module;

static int wifi_init_module_register(void)
{
    wifi_init_module.init = wifi_init;
    init_module_app_register(&wifi_init_module);

    return RT_EOK;
}
INIT_PREV_EXPORT(wifi_init_module_register);

static int print_wifi_param(void)
{
    LOG_I("ip:%s, gateway:%s, netmask:%s", wifi_dev.ip, wifi_dev.gateway, wifi_dev.netmask);
    
    LOG_I("|link_id|state|");
    LOG_I("|-------|-----|");
    for (int i = 0; i < WIFI_SERVER_MAX_CONN; i++)
    {
        LOG_I("|  %d  |  %d  |", wifi_dev.sessions[i].link_id, wifi_dev.sessions[i].state);
    }

    return RT_EOK;
}
MSH_CMD_EXPORT(print_wifi_param, print wifi param);
