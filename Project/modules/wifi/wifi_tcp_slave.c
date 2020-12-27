#include "wifi.h"
#include "agile_modbus.h"

#define SLAVE_ADDR  1

static rt_uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
static rt_uint8_t ctx_read_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
static rt_uint16_t hold_registers[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

static int _modbus_slave_process(agile_modbus_t *ctx, int msg_length)
{
    if(agile_modbus_receive_judge(ctx, msg_length) < 0)
        return -RT_ERROR;
    
    int offset;
    int slave;
    int function;
    uint16_t address;
    int rsp_length = 0;
    agile_modbus_sft_t sft;

    offset = ctx->backend->header_length;
    slave = ctx->read_buf[offset - 1];
    function = ctx->read_buf[offset];
    address = (ctx->read_buf[offset + 1] << 8) + ctx->read_buf[offset + 2];

    sft.slave = slave;
    sft.function = function;
    sft.t_id = ctx->backend->prepare_response_tid(ctx->read_buf, &msg_length);

    if(slave != SLAVE_ADDR)
        return 0;
    
    switch(function)
    {
        case AGILE_MODBUS_FC_READ_HOLDING_REGISTERS:
        case AGILE_MODBUS_FC_READ_INPUT_REGISTERS:
        {
            int nb = (ctx->read_buf[offset + 3] << 8) + ctx->read_buf[offset + 4];
            if (nb < 1 || AGILE_MODBUS_MAX_READ_REGISTERS < nb)
                return -1;
            if ((address + nb) > 0x10000)
                return -1;
            
            rsp_length = ctx->backend->build_response_basis(&sft, ctx->send_buf);
            ctx->send_buf[rsp_length++] = nb << 1;

            for (int i = address; i < address + nb; i++)
            {
                uint16_t data = 0;

                if(i < 10)
                    data = hold_registers[i];
                
                ctx->send_buf[rsp_length++] = data >> 8;
                ctx->send_buf[rsp_length++] = data & 0xFF;
            }

            rsp_length = ctx->backend->send_msg_pre(ctx->send_buf, rsp_length);
        }
        break;

        default:
        break;
    }

    return rsp_length;
}

int wifi_session_process(struct wifi_session *session, rt_uint8_t *recv_buf, int recv_len)
{
    if((recv_len <= 0) || (recv_len > sizeof(ctx_read_buf)))
        return -RT_ERROR;
    
    agile_modbus_tcp_t ctx_tcp;
    agile_modbus_tcp_init(&ctx_tcp, ctx_send_buf, sizeof(ctx_send_buf), ctx_read_buf, sizeof(ctx_read_buf));
    rt_memcpy(ctx_tcp._ctx.read_buf, recv_buf, recv_len);

    int rsp_len = _modbus_slave_process(&(ctx_tcp._ctx), recv_len);
    if(rsp_len < 0)
        return -RT_ERROR;
    
    if(rsp_len > 0)
    {
        int send_len = wifi_session_send(session, ctx_tcp._ctx.send_buf, rsp_len);

        if(send_len != rsp_len)
            return -RT_ERROR;
    }

    return RT_EOK;
}
