
#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "private.h"
#include "utils.h"

#define is_power_of_2(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))


typedef struct 
{
	uint8_t 		*buffer;	 
	uint32_t		size;	 
	uint32_t		in; 		
	uint32_t		out;	  
}btfifo_t;

typedef struct
{
	int (*init)(uint8_t fid,uint8_t *buffer, uint32_t size);
	uint32_t (*len)(uint8_t fid);
	uint32_t (*get)(uint8_t fid, uint8_t * buffer, uint32_t size);
	uint32_t (*peek)(uint8_t fid, uint8_t * buffer, uint32_t size); 
	uint32_t (*put)(uint8_t fid, uint8_t *buffer, uint32_t size);
}fifo_inst_t;

extern const fifo_inst_t fifo_inst;
extern btfifo_t  fifo_ctrl_block[];


#endif
