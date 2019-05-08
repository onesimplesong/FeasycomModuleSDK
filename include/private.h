#ifndef __PRIVATE_H_
#define __PRIVATE_H_

#include <stdbool.h>

#include "config.h"
#include "utils.h"

#include "hal.h"
#include "hal_uart.h"


#define TICK_PERIOD_IN_MS 							(5)
#define _TO_TICKS(tm)								((tm) / TICK_PERIOD_IN_MS)
#define _TO_MSEC(tk)								((tk) * TICK_PERIOD_IN_MS)


#define LED_ON_TIMEOUT								(500)
#define LED_OFF_TIMEOUT								(500)

#define DATA_TRANSFER_BUFF_SIZE						(2048)


#define FIFO_IDX_BASE								(0)
#define FIFO_IDX_BUART_RX							(FIFO_IDX_BASE)
#define FIFO_IDX_END								(FIFO_IDX_BUART_RX) 
#define FIFO_MAX_NUM 								(FIFO_IDX_END+1)


typedef struct
{
	uint16_t on;
	uint16_t off;
}theLED_t;

typedef enum
{
	SYSTEM_RST_NORMAL = 0,
	SYSTEM_RST_BT_HW_ERROR,
	SYSTEM_RST_MCU_HW_ERROR,
	SYSTEM_RST_RESTORE_TO_FACTORY,
	SYSTEM_RST_UNKNOWN_ERROR,
}system_reset_t;


typedef struct
{
	bd_addr_t					bt_addr;
	char						bt_version[32];
	bd_addr_t					bt_paired_device[MAX_PAIRED_DEVICE_NUM];
	uint8_t						bt_plist_num;
	uint16_t					state_mask; 
	const uart_inst_t *			huart;
	const uart_inst_t *			buart;
}theApp_t;


void bt_led_update(void); 
void system_reset(system_reset_t rst_type);
uint16_t app_get_task(uint16_t mask);
void app_start_task(uint16_t mask);
void app_terminate_task(uint16_t bits);
uint32_t app_get_ticks(void);
uint32_t app_get_time(void);
void event_set_tickout(uint32_t * evt_tickout,uint32_t ticks);

extern theLED_t theLED;
extern theApp_t theApp;
extern uint8_t DATA_TRANSFER_BUFF_T[];
extern uint8_t DATA_TRANSFER_BUFF[];


#endif // __PRIVATE_H_

