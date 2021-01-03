/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-03-30     chenyong     first version
 * 2018-04-12     chenyong     add client implement
 * 2018-08-17     chenyong     multiple client support
 */

#include <at.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG              "at.clnt"
#include <at_log.h>

#ifdef AT_USING_CLIENT

#define AT_RESP_END_OK                 "OK"
#define AT_RESP_END_ERROR              "ERROR"
#define AT_RESP_END_FAIL               "FAIL"
#define AT_END_CR_LF                   "\r\n"

extern rt_size_t at_vprintfln(usr_device_t dev, const char *format, va_list args);
extern void at_print_raw_cmd(const char *type, const char *cmd, rt_size_t size);
extern const char *at_get_last_cmd(rt_size_t *cmd_size);

ALIGN(RT_ALIGN_SIZE)
static struct at_client at_client_table[AT_CLIENT_NUM_MAX] = { 0 };

int at_resp_init(at_response_t resp, char *buf, rt_size_t buf_size, rt_size_t line_num, rt_int32_t timeout)
{
    RT_ASSERT(buf);
    RT_ASSERT(buf_size > 0);

    resp->buf = buf;
    resp->buf_size = buf_size;
    resp->line_num = line_num;
    resp->line_counts = 0;
    resp->timeout = timeout;

    return RT_EOK;
}


int at_resp_set_info(at_response_t resp, rt_size_t line_num, rt_int32_t timeout)
{
    RT_ASSERT(resp);

    resp->line_num = line_num;
    resp->timeout = timeout;

    return RT_EOK;
}

/**
 * Get one line AT response buffer by line number.
 *
 * @param resp response object
 * @param resp_line line number, start from '1'
 *
 * @return != RT_NULL: response line buffer
 *          = RT_NULL: input response line error
 */
const char *at_resp_get_line(at_response_t resp, rt_size_t resp_line)
{
    char *resp_buf = resp->buf;
    char *resp_line_buf = RT_NULL;
    rt_size_t line_num = 1;

    RT_ASSERT(resp);

    if (resp_line > resp->line_counts || resp_line <= 0)
    {
        LOG_E("AT response get line failed! Input response line(%d) error!", resp_line);
        return RT_NULL;
    }

    for (line_num = 1; line_num <= resp->line_counts; line_num++)
    {
        if (resp_line == line_num)
        {
            resp_line_buf = resp_buf;

            return resp_line_buf;
        }

        resp_buf += strlen(resp_buf) + 1;
    }

    return RT_NULL;
}

/**
 * Get one line AT response buffer by keyword
 *
 * @param resp response object
 * @param keyword query keyword
 *
 * @return != RT_NULL: response line buffer
 *          = RT_NULL: no matching data
 */
const char *at_resp_get_line_by_kw(at_response_t resp, const char *keyword)
{
    char *resp_buf = resp->buf;
    char *resp_line_buf = RT_NULL;
    rt_size_t line_num = 1;

    RT_ASSERT(resp);
    RT_ASSERT(keyword);

    for (line_num = 1; line_num <= resp->line_counts; line_num++)
    {
        if (strstr(resp_buf, keyword))
        {
            resp_line_buf = resp_buf;

            return resp_line_buf;
        }

        resp_buf += strlen(resp_buf) + 1;
    }

    return RT_NULL;
}

/**
 * Get and parse AT response buffer arguments by line number.
 *
 * @param resp response object
 * @param resp_line line number, start from '1'
 * @param resp_expr response buffer expression
 *
 * @return -1 : input response line number error or get line buffer error
 *          0 : parsed without match
 *         >0 : the number of arguments successfully parsed
 */
int at_resp_parse_line_args(at_response_t resp, rt_size_t resp_line, const char *resp_expr, ...)
{
    va_list args;
    int resp_args_num = 0;
    const char *resp_line_buf = RT_NULL;

    RT_ASSERT(resp);
    RT_ASSERT(resp_expr);

    if ((resp_line_buf = at_resp_get_line(resp, resp_line)) == RT_NULL)
    {
        return -1;
    }

    va_start(args, resp_expr);

    resp_args_num = vsscanf(resp_line_buf, resp_expr, args);

    va_end(args);

    return resp_args_num;
}

/**
 * Get and parse AT response buffer arguments by keyword.
 *
 * @param resp response object
 * @param keyword query keyword
 * @param resp_expr response buffer expression
 *
 * @return -1 : input keyword error or get line buffer error
 *          0 : parsed without match
 *         >0 : the number of arguments successfully parsed
 */
int at_resp_parse_line_args_by_kw(at_response_t resp, const char *keyword, const char *resp_expr, ...)
{
    va_list args;
    int resp_args_num = 0;
    const char *resp_line_buf = RT_NULL;

    RT_ASSERT(resp);
    RT_ASSERT(resp_expr);

    if ((resp_line_buf = at_resp_get_line_by_kw(resp, keyword)) == RT_NULL)
    {
        return -1;
    }

    va_start(args, resp_expr);

    resp_args_num = vsscanf(resp_line_buf, resp_expr, args);

    va_end(args);

    return resp_args_num;
}

/**
 * Send commands to AT server and wait response.
 *
 * @param client current AT client object
 * @param resp AT response object, using RT_NULL when you don't care response
 * @param cmd_expr AT commands expression
 *
 * @return 0 : success
 *        -1 : response status error
 *        -2 : wait timeout
 *        -7 : enter AT CLI mode
 */
int at_obj_exec_cmd(at_client_t client, at_response_t resp, const char *cmd_expr, ...)
{
    va_list args;
    rt_size_t cmd_size = 0;
    rt_err_t result = RT_EOK;
    const char *cmd = RT_NULL;

    RT_ASSERT(cmd_expr);

    if (client == RT_NULL)
    {
        LOG_E("input AT Client object is NULL, please create or get AT Client object!");
        return -RT_ERROR;
    }

    /* check AT CLI mode */
    if (client->status == AT_STATUS_CLI && resp)
    {
        return -RT_EBUSY;
    }

    client->resp_status = AT_RESP_OK;
    client->resp = resp;

    if (resp != RT_NULL)
    {
        rt_sem_control(&(client->resp_notice), RT_IPC_CMD_RESET, RT_NULL);
        resp->buf_len = 0;
        resp->line_counts = 0;
    }

    va_start(args, cmd_expr);
    at_vprintfln(client->dev, cmd_expr, args);
    va_end(args);

    if (resp != RT_NULL)
    {
        if (rt_sem_take(&(client->resp_notice), resp->timeout) != RT_EOK)
        {
            cmd = at_get_last_cmd(&cmd_size);
            LOG_D("execute command (%.*s) timeout (%d ticks)!", cmd_size, cmd, resp->timeout);
            client->resp_status = AT_RESP_TIMEOUT;
            result = -RT_ETIMEOUT;
            goto __exit;
        }
        if (client->resp_status != AT_RESP_OK)
        {
            cmd = at_get_last_cmd(&cmd_size);
            LOG_E("execute command (%.*s) failed!", cmd_size, cmd);
            result = -RT_ERROR;
            goto __exit;
        }
    }

__exit:
    client->resp = RT_NULL;

    return result;
}

int at_client_obj_wait_connect(at_client_t client, at_response_t resp, rt_uint32_t timeout)
{
    rt_err_t result = RT_EOK;
    rt_tick_t start_time = 0;
    char *client_name = client->dev->name;

    if (client == RT_NULL)
    {
        LOG_E("input AT client object is NULL, please create or get AT Client object!");
        return -RT_ERROR;
    }

    at_resp_set_info(resp, 0, rt_tick_from_millisecond(300));

    rt_sem_control(&(client->resp_notice), RT_IPC_CMD_RESET, RT_NULL);
    client->resp = resp;
    
    start_time = rt_tick_get();

    while (1)
    {
        /* Check whether it is timeout */
        if (rt_tick_get() - start_time > rt_tick_from_millisecond(timeout))
        {
            LOG_E("wait AT client(%s) connect timeout(%d tick).", client_name, timeout);
            result = -RT_ETIMEOUT;
            break;
        }

        /* Check whether it is already connected */
        resp->buf_len = 0;
        resp->line_counts = 0;
        usr_device_write(client->dev, 0, "AT\r\n", 4);

        if (rt_sem_take(&(client->resp_notice), resp->timeout) != RT_EOK)
            continue;
        else
            break;
    }

    client->resp = RT_NULL;

    return result;
}

/**
 * Send data to AT server, send data don't have end sign(eg: \r\n).
 *
 * @param client current AT client object
 * @param buf   send data buffer
 * @param size  send fixed data size
 *
 * @return >0: send data size
 *         =0: send failed
 */
rt_size_t at_client_obj_send(at_client_t client, const char *buf, rt_size_t size)
{
    RT_ASSERT(buf);

    if (client == RT_NULL)
    {
        LOG_E("input AT Client object is NULL, please create or get AT Client object!");
        return 0;
    }

#ifdef AT_PRINT_RAW_CMD
    at_print_raw_cmd("sendline", buf, size);
#endif

    return usr_device_write(client->dev, 0, buf, size);
}

static rt_err_t at_client_getchar(at_client_t client, char *ch, rt_int32_t timeout)
{
    rt_err_t result = RT_EOK;

    while (usr_device_read(client->dev, 0, ch, 1) == 0)
    {
        rt_sem_control(&(client->rx_notice), RT_IPC_CMD_RESET, RT_NULL);

        result = rt_sem_take(&(client->rx_notice), rt_tick_from_millisecond(timeout));
        if (result != RT_EOK)
        {
            return result;
        }
    }

    return RT_EOK;
}

/**
 * AT client receive fixed-length data.
 *
 * @param client current AT client object
 * @param buf   receive data buffer
 * @param size  receive fixed data size
 * @param timeout  receive data timeout (ms)
 *
 * @note this function can only be used in execution function of URC data
 *
 * @return >0: receive data size
 *         =0: receive failed
 */
rt_size_t at_client_obj_recv(at_client_t client, char *buf, rt_size_t size, rt_int32_t timeout)
{
    rt_size_t read_idx = 0;
    rt_err_t result = RT_EOK;
    char ch;

    RT_ASSERT(buf);

    if (client == RT_NULL)
    {
        LOG_E("input AT Client object is NULL, please create or get AT Client object!");
        return 0;
    }

    while (1)
    {
        if (read_idx < size)
        {
            result = at_client_getchar(client, &ch, timeout);
            if (result != RT_EOK)
            {
                LOG_E("AT Client receive failed, uart device get data error(%d)", result);
                return 0;
            }

            buf[read_idx++] = ch;
        }
        else
        {
            break;
        }
    }

#ifdef AT_PRINT_RAW_CMD
    at_print_raw_cmd("urc_recv", buf, size);
#endif

    return read_idx;
}

/**
 *  AT client set end sign.
 *
 * @param client current AT client object
 * @param ch the end sign, can not be used when it is '\0'
 */
void at_obj_set_end_sign(at_client_t client, char ch)
{
    if (client == RT_NULL)
    {
        LOG_E("input AT Client object is NULL, please create or get AT Client object!");
        return;
    }

    client->end_sign = ch;
}

/**
 * set URC(Unsolicited Result Code) table
 *
 * @param client current AT client object
 * @param table URC table
 * @param size table size
 */
int at_obj_set_urc_table(at_client_t client, const struct at_urc *urc_table, rt_size_t table_sz)
{
    rt_size_t idx;

    if (client == RT_NULL)
    {
        LOG_E("input AT Client object is NULL, please create or get AT Client object!");
        return -RT_ERROR;
    }

    for (idx = 0; idx < table_sz; idx++)
    {
        RT_ASSERT(urc_table[idx].cmd_prefix);
        RT_ASSERT(urc_table[idx].cmd_suffix);
    }
    
    client->urc_table.urc = urc_table;
    client->urc_table.urc_size = table_sz;

    return RT_EOK;
}

/**
 * get AT client object by AT device name.
 *
 * @dev_name AT client device name
 *
 * @return AT client object
 */
at_client_t at_client_get(const char *dev_name)
{
    int idx = 0;

    RT_ASSERT(dev_name);

    for (idx = 0; idx < AT_CLIENT_NUM_MAX; idx++)
    {
        if (rt_strcmp(at_client_table[idx].dev->name, dev_name) == 0)
        {
            return &at_client_table[idx];
        }
    }

    return RT_NULL;
}

/**
 * get first AT client object in the table.
 *
 * @return AT client object
 */
at_client_t at_client_get_first(void)
{
    if (at_client_table[0].dev == RT_NULL)
    {
        return RT_NULL;
    }

    return &at_client_table[0];
}

static const struct at_urc *get_urc_obj(at_client_t client)
{
    rt_size_t i, prefix_len, suffix_len;
    rt_size_t bufsz;
    char *buffer = RT_NULL;
    const struct at_urc *urc = RT_NULL;

    if ((client->urc_table.urc == RT_NULL) || (client->urc_table.urc_size <= 0))
    {
        return RT_NULL;
    }

    buffer = client->recv_line_buf;
    bufsz = client->recv_line_len;

    for (i = 0; i < client->urc_table.urc_size; i++)
    {
        urc = client->urc_table.urc + i;

        prefix_len = rt_strlen(urc->cmd_prefix);
        suffix_len = rt_strlen(urc->cmd_suffix);
        if (bufsz < prefix_len + suffix_len)
        {
            continue;
        }
        if ((prefix_len ? !rt_strncmp(buffer, urc->cmd_prefix, prefix_len) : 1) && (suffix_len ? !rt_strncmp(buffer + bufsz - suffix_len, urc->cmd_suffix, suffix_len) : 1))
        {
            return urc;
        }
    }

    return RT_NULL;
}

static int at_recv_readline(at_client_t client)
{
    rt_size_t read_len = 0;
    char ch = 0, last_ch = 0;
    rt_bool_t is_full = RT_FALSE;

    rt_memset(client->recv_line_buf, 0x00, client->recv_bufsz);
    client->recv_line_len = 0;

    while (1)
    {
        at_client_getchar(client, &ch, RT_WAITING_FOREVER);

        if (read_len < client->recv_bufsz)
        {
            client->recv_line_buf[read_len++] = ch;
            client->recv_line_len = read_len;
        }
        else
        {
            is_full = RT_TRUE;
        }

        /* is newline or URC data */
        if ((ch == '\n' && last_ch == '\r') || (client->end_sign != 0 && ch == client->end_sign)
                || get_urc_obj(client))
        {
            if (is_full)
            {
                LOG_E("read line failed. The line data length is out of buffer size(%d)!", client->recv_bufsz);
                rt_memset(client->recv_line_buf, 0x00, client->recv_bufsz);
                client->recv_line_len = 0;
                return -RT_EFULL;
            }
            break;
        }
        last_ch = ch;
    }

#ifdef AT_PRINT_RAW_CMD
    at_print_raw_cmd("recvline", client->recv_line_buf, read_len);
#endif

    return read_len;
}

void client_parser(at_client_t client)
{
    const struct at_urc *urc;

    while(1)
    {
        if (at_recv_readline(client) > 0)
        {
            if ((urc = get_urc_obj(client)) != RT_NULL)
            {
                /* current receive is request, try to execute related operations */
                if (urc->func != RT_NULL)
                {
                    urc->func(client, client->recv_line_buf, client->recv_line_len);
                }
            }
            else if (client->resp != RT_NULL)
            {
                at_response_t resp = client->resp;

                /* current receive is response */
                client->recv_line_buf[client->recv_line_len - 1] = '\0';
                if (resp->buf_len + client->recv_line_len < resp->buf_size)
                {
                    /* copy response lines, separated by '\0' */
                    rt_memcpy(resp->buf + resp->buf_len, client->recv_line_buf, client->recv_line_len);

                    /* update the current response information */
                    resp->buf_len += client->recv_line_len;
                    resp->line_counts++;
                }
                else
                {
                    client->resp_status = AT_RESP_BUFF_FULL;
                    LOG_E("Read response buffer failed. The Response buffer size is out of buffer size(%d)!", resp->buf_size);
                }
                /* check response result */
                if (rt_memcmp(client->recv_line_buf, AT_RESP_END_OK, rt_strlen(AT_RESP_END_OK)) == 0
                        && resp->line_num == 0)
                {
                    /* get the end data by response result, return response state END_OK. */
                    client->resp_status = AT_RESP_OK;
                }
                else if (rt_strstr(client->recv_line_buf, AT_RESP_END_ERROR)
                        || (rt_memcmp(client->recv_line_buf, AT_RESP_END_FAIL, rt_strlen(AT_RESP_END_FAIL)) == 0))
                {
                    client->resp_status = AT_RESP_ERROR;
                }
                else if (resp->line_counts == resp->line_num && resp->line_num)
                {
                    /* get the end data by response line, return response state END_OK.*/
                    client->resp_status = AT_RESP_OK;
                }
                else
                {
                    continue;
                }

                client->resp = RT_NULL;
                rt_sem_release(&(client->resp_notice));
            }
            else
            {
//                log_d("unrecognized line: %.*s", client->recv_line_len, client->recv_line_buf);
            }
        }
    }
}

static rt_err_t at_client_rx_ind(usr_device_t dev, rt_size_t size)
{
    int idx = 0;

    for (idx = 0; idx < AT_CLIENT_NUM_MAX; idx++)
    {
        if (at_client_table[idx].dev == dev && size > 0)
        {
            rt_sem_release(&(at_client_table[idx].rx_notice));
        }
    }

    return RT_EOK;
}

/* initialize the client object parameters */
static int at_client_para_init(at_client_t client, usr_device_t dev, char *recv_line_buf, rt_size_t recv_bufsz)
{
#define AT_CLIENT_SEM_NAME             "at_cs"
#define AT_CLIENT_RESP_NAME            "at_cr"

    static int at_client_num = 0;
    char name[RT_NAME_MAX];

    client->dev = dev;
    client->status = AT_STATUS_UNINITIALIZED;
    client->recv_line_buf = recv_line_buf;
    client->recv_line_len = 0;
    client->recv_bufsz = recv_bufsz;

    rt_snprintf(name, RT_NAME_MAX, "%s%d", AT_CLIENT_SEM_NAME, at_client_num);
    rt_sem_init(&(client->rx_notice), name, 0, RT_IPC_FLAG_FIFO);

    rt_snprintf(name, RT_NAME_MAX, "%s%d", AT_CLIENT_RESP_NAME, at_client_num);
    rt_sem_init(&(client->resp_notice), name, 0, RT_IPC_FLAG_FIFO);

    usr_device_set_rx_indicate(client->dev, at_client_rx_ind);

    client->status = AT_STATUS_INITIALIZED;

    at_client_num++;

    return RT_EOK;
}

at_client_t at_client_init(usr_device_t dev, char *recv_line_buf, rt_size_t recv_bufsz)
{
    RT_ASSERT(dev);
    RT_ASSERT(recv_line_buf);
    RT_ASSERT(recv_bufsz > 0);

    if (at_client_get(dev->name) != RT_NULL)
        return RT_NULL;

    int idx = 0;
    for (idx = 0; idx < AT_CLIENT_NUM_MAX && at_client_table[idx].dev; idx++);

    if (idx >= AT_CLIENT_NUM_MAX)
    {
        LOG_E("AT client initialize failed! Check the maximum number(%d) of AT client.", AT_CLIENT_NUM_MAX);
        return RT_NULL;
    }

    at_client_t client = &at_client_table[idx];
    rt_memset(client, 0x00, sizeof(struct at_client));

    at_client_para_init(client, dev, recv_line_buf, recv_bufsz);

    LOG_I("AT client(V%s) on device %s initialize success.", AT_SW_VERSION, dev->name);

    return client;
}
#endif /* AT_USING_CLIENT */
