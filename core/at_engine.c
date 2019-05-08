
#include <string.h>

#include "debug.h"
#include "common.h"
#include "module.h"

#include "hal.h"

#include "at_engine.h"


#define AT_ERR_UNINITIALIZED				(-1)
#define AT_ERR_BUART_BUSY					(-2)
#define AT_ERR_UNKNOWN_CMD					(-3)
#define AT_ERR_READ_BUSY					(-4)

#define CHAR_CR								'\r'
#define CHAR_LF								'\n'


typedef enum
{
	AT_ENGINE_UNINITIALZED,
	AT_ENGINE_INITIALZED
}at_engine_state_t;

typedef enum
{
	TX_IDLE,
	TX_PACKET_SENT,
	TX_LOCKED,
}at_engine_tx_state_t;

typedef enum
{
	MSG_HEAD_CR, //'\r'
	MSG_HEAD_LF, //'\n'
	MSG_TAIL_CR, //'\r'
	MSG_TAIL_LF, //'\n'
	MSG_W4_PROCESS,
}message_parse_state_t;

typedef struct
{
	at_engine_state_t		state;
	message_parse_state_t	msg_parse_sm;
	uint32_t				boot_time_last;
	const module_t			*module;
	char					cmd_buf[256];
	uint8_t					cmd_buf_len;
	at_pattern_index_t		command_sent;
	message_dispatcher_t	message_dispatcher;
	at_engine_tx_state_t	tx_state;
	uint32_t				tx_lock_ticks_last;
}at_engine_t;


static at_engine_t ate;


static void incoming_message_handler(const at_pattern_t *pattern, char *packet, int size)
{
	uint16_t payload_offset = strlen(pattern->pattern);
	
	//DEBUG(("pattern=%s,size=%d",pattern->pattern,size));

	if(ate.message_dispatcher)
	{
		if(FSC_RESP_OK <= pattern->index && pattern->index < END_OF_ALL_PATTERNS)
		{
			if(FSC_RESP_OK == pattern->index)
				ate.tx_state = TX_IDLE;
			ate.message_dispatcher(pattern,NULL,ate.command_sent);
		}
		else
			ate.message_dispatcher(pattern,(uint8_t*)packet+payload_offset,size);
	}
}

static int get_message_size(char *packet,int size)
{
	int num = 0;
	message_parse_state_t msg_parse_sm = MSG_HEAD_CR;

	while(num < size)
	{
		switch(msg_parse_sm)
		{
			case MSG_HEAD_CR:
				if(packet[num] == CHAR_CR)
					msg_parse_sm = MSG_HEAD_LF;
				break;
			case MSG_HEAD_LF:
				if(packet[num] == CHAR_LF)
					msg_parse_sm = MSG_TAIL_CR;
				else
					msg_parse_sm = MSG_HEAD_CR;
				break;
			case MSG_TAIL_CR:
				if(packet[num] == CHAR_CR)
					msg_parse_sm = MSG_TAIL_LF;
				break;
			case MSG_TAIL_LF:
				if(packet[num] == CHAR_LF)
					msg_parse_sm = MSG_W4_PROCESS;
				else
					msg_parse_sm = MSG_TAIL_CR;
				break;
			default:
				break;
		}
		
		num++;
		
		if(msg_parse_sm == MSG_W4_PROCESS)
			return num;
	}

	return 0;
}

static uint16_t incoming_message_parser(char *packet,int size)
{
	char *datain = packet;
	int msg_size = get_message_size(packet,size);
	int num = msg_size;
	const at_pattern_t	*p_table = &ate.module->at_pattern_table[0];
	uint16_t i;

	if(!msg_size)
		return 0;

	datain += 2; // skip CR and LF of the head
	num -= 4; // decrease the size due to the CR and LF of both head and tail

	// traversing AT pattern table to find the exact pattern of the packet
	for(i = 0; p_table[i].index < END_OF_ALL_PATTERNS; i++)
	{
		uint8_t pattern_len = strlen((p_table[i].pattern));
		
		if(!pattern_len || (pattern_len > num))
			continue;
			
		if(memcmp((uint8_t*)(p_table[i].pattern),(uint8_t*)datain,pattern_len))
			continue;
			
		incoming_message_handler(&p_table[i],datain,num - pattern_len);
		break;
	}

	return msg_size;
}

static void buart_tx_callback(void)
{
	if(ate.tx_state == TX_PACKET_SENT)
	{
		ate.tx_state = TX_LOCKED;
		ate.tx_lock_ticks_last = app_get_ticks();
	}
}

static void buart_rx_callback(uint8_t value)
{
	fifo_inst.put(FIFO_IDX_BUART_RX,&value,1);
}

static int create_at_cmd(at_pattern_index_t cmd_idx, const uint8_t *params, uint16_t plen)
{
	uint8_t i;
	const char *pattern = NULL;

	memcpy(ate.cmd_buf,"AT",2);
	ate.cmd_buf_len = 2;

	if(cmd_idx < FSC_BT_AT || cmd_idx >= END_OF_COMMANDS)
		return 0;

	for(i = 0; ate.module->at_pattern_table[i].index != END_OF_COMMANDS; i++)
	{
		if(ate.module->at_pattern_table[i].index == cmd_idx)
		{
			pattern = ate.module->at_pattern_table[i].pattern;
			break;
		}
	}
	memcpy(ate.cmd_buf+ate.cmd_buf_len,pattern,strlen(pattern));
	ate.cmd_buf_len += strlen(pattern);

	if(plen)
	{
		memcpy(ate.cmd_buf+ate.cmd_buf_len,params,plen);
		ate.cmd_buf_len += plen;
	}

	memcpy(ate.cmd_buf+ate.cmd_buf_len,"\r\n",2);
	ate.cmd_buf_len += 2;

	return 1;
}

int at_cmd_send(at_pattern_index_t cmd_idx, const uint8_t *params, uint16_t plen)
{
	if(ate.state != AT_ENGINE_INITIALZED)
		return AT_ERR_UNINITIALIZED;

	if(ate.tx_state != TX_IDLE)
		return AT_ERR_BUART_BUSY;
	
	if(!create_at_cmd(cmd_idx,params,plen))
		return AT_ERR_UNKNOWN_CMD;

	ate.command_sent = cmd_idx;
	ate.tx_state = TX_PACKET_SENT;
	theApp.buart->send((uint8_t*)ate.cmd_buf,ate.cmd_buf_len);

	return 0;
}

void at_engine_register_message_dispatcher(message_dispatcher_t message_dispatcher)
{
	ate.message_dispatcher = message_dispatcher;
}

static void buart_rx_parse_once(void)
{
	uint8_t *buf = DATA_TRANSFER_BUFF_T;
	uint16_t fifo_len = fifo_inst.len(FIFO_IDX_BUART_RX);
	
	if(!fifo_len)
		return;

	if(hal_bt_connected())
	{
		int length = fifo_inst.get(FIFO_IDX_BUART_RX,buf,the_smaller(fifo_len,256));
		theApp.huart->send(buf,length);
	}
	else
	{
		int length = fifo_inst.peek(FIFO_IDX_BUART_RX,buf,the_smaller(fifo_len,256));
		if(length)
		{
			int can_discard_data = 0;
			if(length >= 4)
			{
				if(buf[length-2] == '\r' && buf[length-1] == '\n') 
				{
					length = incoming_message_parser((char*)buf,length);
					can_discard_data = 1;
				}
			}
			if(length > 100)
				can_discard_data = 1;
			if(can_discard_data)
				fifo_inst.get(FIFO_IDX_BUART_RX,NULL,length);
		}
	}
}

void at_engine_run(void)
{
	if(ate.state != AT_ENGINE_INITIALZED)
	{
		if(app_get_time() - ate.boot_time_last >= ate.module->init_time)
		{
			ate.state = AT_ENGINE_INITIALZED;
		}
	}

	if(ate.tx_state == TX_LOCKED)
	{
		uint32_t dedicated_at_response_tickout = _TO_TICKS(ate.module->dedicated_at_response_timeout);
		if(app_get_ticks() - ate.tx_lock_ticks_last > dedicated_at_response_tickout)
		{
			ate.tx_state = TX_IDLE;
		}
	}

	buart_rx_parse_once();
}

void at_engine_init(void)
{
	ate.state = AT_ENGINE_UNINITIALZED;
	ate.msg_parse_sm = MSG_HEAD_CR;
	ate.message_dispatcher = NULL;
	ate.tx_state = TX_IDLE;
	ate.tx_lock_ticks_last = 0;
	theApp.buart->set_callback(buart_tx_callback,buart_rx_callback,NULL,NULL);	
	ate.boot_time_last = app_get_time();
	ate.module = get_module();
}

