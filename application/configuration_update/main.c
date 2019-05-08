
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "debug.h"
#include "private.h"
#include "common.h"
#include "at_engine.h"

#include "hal_uart.h"
#include "hal.h"



#define SPLICE_TO_WRITE(params_str)		("="params_str)
#define APP_BT_NAME						"FeasycomSDK"
#define APP_BT_PIN						"1234"
#define APP_BT_BAUD						115200


static uint32_t app_ticks_now;
static uint32_t app_ticks_last;

theApp_t 	theApp;
#ifdef HAVE_LED
theLED_t	theLED;
#endif


uint8_t DATA_TRANSFER_BUFF_T[256];
uint8_t DATA_TRANSFER_BUFF[DATA_TRANSFER_BUFF_SIZE];

#ifdef HAVE_LED
static void hal_set_led(uint16_t on_ticks,uint16_t off_ticks)
{
	theLED.on = on_ticks;
	theLED.off = off_ticks;
	hal_pio_set(BT_PORT_B,BT_PIN_2);
}
static void hal_reset_led(uint16_t on_ticks,uint16_t off_ticks)
{
	theLED.on = on_ticks;	
	theLED.off = off_ticks;
	hal_pio_reset(BT_PORT_B,BT_PIN_2);
}

void bt_led_update(void)
{
	if(!theLED.on && !theLED.off )
		return ;	
	if(theLED.on)
	{
		theLED.on--;
		if(!theLED.on)
			hal_reset_led(0, _TO_TICKS(LED_OFF_TIMEOUT));
	}
	else if(theLED.off)
	{
		theLED.off--;
		if(!theLED.off)
			hal_set_led(_TO_TICKS(LED_ON_TIMEOUT),0);
	}
}
#endif

void get_bt_mac(char *addr, int size)
{
	size = size;
	
	str_to_addr(theApp.bt_addr,(uint8_t*)&addr[1]);
	DEBUG(("BT MAC: %s",addr_to_str(theApp.bt_addr)));

	app_terminate_task(TASK_MASK_ADDR_BIT);
	app_start_task(TASK_MASK_VER_BIT);
}

void get_bt_ver(char *ver, int size)
{	
	memcpy(theApp.bt_version,(uint8_t*)&ver[1],size-1);
	DEBUG(("BT Version: %s",theApp.bt_version));

	app_terminate_task(TASK_MASK_VER_BIT);
	app_start_task(TASK_MASK_PLIST_BIT);
}

void get_bt_plist(char *plist, int size)
{
	if(size < 12)
	{
		if(plist[1] == '{')
		{
			theApp.bt_plist_num = 0;
			app_terminate_task(TASK_MASK_PLIST_BIT);
		}
		else if(plist[1] == '}')
		{
			app_start_task(TASK_MASK_NAME_BIT);
		}
		return ;
	}
	
	if(theApp.bt_plist_num < MAX_PAIRED_DEVICE_NUM)
	{
		str_to_addr(theApp.bt_paired_device[theApp.bt_plist_num],(uint8_t*)&plist[3]);
		DEBUG(("BT Paired Device#%d: %s",theApp.bt_plist_num,addr_to_str(theApp.bt_paired_device[theApp.bt_plist_num])));
		theApp.bt_plist_num++;
	}
}

void app_excute_task(void)
{
	uint16_t tasks = theApp.state_mask;
	
	if(tasks & TASK_MASK_ADDR_BIT)
	{
		at_cmd_send(FSC_BT_ADDR,NULL,0);
	}
	
	if(tasks & TASK_MASK_VER_BIT)
	{
		at_cmd_send(FSC_BT_VER,NULL,0);
	}

	if(tasks & TASK_MASK_PLIST_BIT)
	{
		at_cmd_send(FSC_BT_PLIST,NULL,0);
	}
	
	if(tasks & TASK_MASK_NAME_BIT)
	{
		at_cmd_send(FSC_BT_NAME,SPLICE_TO_WRITE(APP_BT_NAME),strlen(SPLICE_TO_WRITE(APP_BT_NAME)));
	}

	if(tasks & TASK_MASK_PIN_BIT)
	{
		at_cmd_send(FSC_BT_PIN,SPLICE_TO_WRITE(APP_BT_PIN),strlen(SPLICE_TO_WRITE(APP_BT_PIN)));
	}
}

void app_start_task(uint16_t mask)
{
	uint16_t changed_bits = (theApp.state_mask | mask) ^ theApp.state_mask;
	
	if(!changed_bits) return;

	theApp.state_mask |= mask;
}

void app_terminate_task(uint16_t mask)
{
	uint16_t changed_bits = (theApp.state_mask & ~mask) ^ theApp.state_mask;
	
	if(!changed_bits) return;
	
	theApp.state_mask &= ~mask;	
}

uint16_t app_get_task(uint16_t bits)
{
	return theApp.state_mask & bits;
} 

void event_set_tickout(uint32_t *evt_tickout, uint32_t ticks)
{
	*evt_tickout = ticks;
}

void event_tickout_process(uint32_t *evt_tickout, void (*tickout_handler)(void))
{
	volatile uint32_t *tickout = evt_tickout;
	if(*tickout)
	{
		(*tickout)--;
		if(!*tickout)
		{
			tickout_handler();
		}
	}
}

void bt_message_dispatcher(const at_pattern_t *pattern, uint8_t *packet, int size)
{
	//DEBUG(("pattern_index=%d",pattern->index));

	if(FSC_RESP_OK == pattern->index)
	{
		int cmd_index = size;
		switch(cmd_index)
		{
			case FSC_BT_NAME:
				app_terminate_task(TASK_MASK_NAME_BIT);
				app_start_task(TASK_MASK_PIN_BIT);
				DEBUG(("BT Device Name Updated"));
				break;
			case FSC_BT_PIN:
				app_terminate_task(TASK_MASK_PIN_BIT);
				DEBUG(("BT PIN Updated"));
				break;
			default:
				break;
		}
	}
	else
	{
		//@TODO: handle SCAN results here
		switch(pattern->index)
		{
			case FSC_BT_ADDR:
				get_bt_mac((char*)packet,size);
				break;
			case FSC_BT_VER:
				get_bt_ver((char*)packet,size);
				break;
			case FSC_BT_PLIST:
				get_bt_plist((char*)packet,size);
				//DEBUG(("FSC_BT_PLIST"));
				break;
			default:
				break;
		}
	}
}

static void try_start_at_sequence(void)
{
	if(!bd_addr_exist(theApp.bt_addr) && !app_get_task(TASK_MASK_ADDR_BIT))
		app_start_task(TASK_MASK_ADDR_BIT);
}

void app_execute_once(void)
{
	try_start_at_sequence();

	app_excute_task();
	
	at_engine_run();

	if(app_ticks_last != app_ticks_now)
	{
		app_ticks_last = app_ticks_now;

#if defined(HAVE_SPP_MASTER)
		event_tickout_process(&theApp.conn_tickout,power_on_create_connection_cancel);
#endif
    } 
}

static void app_tick_handler(void)
{
	app_ticks_now++;
#ifdef HAVE_LED
	bt_led_update();
#endif
}

uint32_t app_get_ticks(void)
{
    return app_ticks_now;
}

uint32_t app_get_time(void)
{
    return app_ticks_now * TICK_PERIOD_IN_MS;
}

void app_execute(void)
{	
    app_ticks_now = 0;
	hal_tick_set_handler(&app_tick_handler);

    while (1)
	{
		app_execute_once();
	}
}

static void app_init(void)
{
	uint8_t i;
	memset(theApp.bt_addr,0,6);
	memset(theApp.bt_version,0,32);
	for(i = 0; i < MAX_PAIRED_DEVICE_NUM; i++)
	{
		memset(theApp.bt_paired_device[i],0,6);
	}
	theApp.bt_plist_num = 0;
	theApp.state_mask = 0;
}

static void fifo_init(void)
{
	memset((uint8_t*)fifo_ctrl_block,0,sizeof(btfifo_t)*FIFO_MAX_NUM/sizeof(uint8_t));	
	fifo_inst.init(FIFO_IDX_BUART_RX,DATA_TRANSFER_BUFF,DATA_TRANSFER_BUFF_SIZE);
}

int main(void)
{
	hal_init();
		
	app_init();
			
	fifo_init();

	DEBUG(("Feasycom SDK"));
#ifdef HAVE_LED
	hal_set_led(_TO_TICKS(LED_ON_TIMEOUT), _TO_TICKS(LED_OFF_TIMEOUT));
#endif
	at_engine_init();

	at_engine_register_message_dispatcher(bt_message_dispatcher);
	
	app_execute();

	return 0;
}

