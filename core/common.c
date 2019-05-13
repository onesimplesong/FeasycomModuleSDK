

#include <string.h>
#include <stdarg.h>

#include "debug.h"
#include "at_engine.h"

#include "common.h"


btfifo_t  fifo_ctrl_block[FIFO_MAX_NUM];


#ifdef HAVE_HUART
static char btprintf_buffer[128];
void dbg_printf( const char* Format, ... )
{
	va_list	 parms;
	uint16_t length = 0x00;
	va_start( parms, Format );
	length += vsprintf( &btprintf_buffer[0], Format, parms );
	va_end(parms);

	memcpy(btprintf_buffer+length,"\r\n",2);
	length += 2;
	theApp.huart->send((uint8_t*)btprintf_buffer, length);
}
#endif

int  __btfifo_init(uint8_t fid,uint8_t *buffer, uint32_t size)
{
	if (!is_power_of_2(size)) return -1;
	
	memset(&fifo_ctrl_block[fid], 0, sizeof(btfifo_t));
	fifo_ctrl_block[fid].buffer = buffer;
	fifo_ctrl_block[fid].size = size;
	fifo_ctrl_block[fid].in = 0;
	fifo_ctrl_block[fid].out = 0;
	
	return 0;
}

uint32_t __btfifo_len(uint8_t fid)
{
    return (fifo_ctrl_block[fid].in - fifo_ctrl_block[fid].out);
}

uint32_t __btfifo_get(uint8_t fid, uint8_t * buffer, uint32_t size)
{
	uint32_t len = 0;
	btfifo_t * fifo = &fifo_ctrl_block[fid];
	uint32_t in = fifo->in; //prefetch counter 'in' for better reentrant operation
	
	size  = the_smaller(size, in - fifo->out);
	/* first get the data from fifo->out until the end of the buffer */
	len = the_smaller(size, fifo->size - (fifo->out & (fifo->size - 1)));	
	if(buffer)
	{
		memcpy(buffer, fifo->buffer + (fifo->out & (fifo->size - 1)), len);
		/* then get the rest (if any) from the beginning of the buffer */
		memcpy(buffer + len, fifo->buffer, size - len);
	}
	fifo->out += size;
	
	return size;
}

uint32_t __btfifo_peek(uint8_t fid, uint8_t * buffer, uint32_t size)
{
	uint32_t len = 0;
	btfifo_t * fifo = &fifo_ctrl_block[fid];
	uint32_t in = fifo->in; //prefetch counter 'in' for better reentrant operation
	
	size  = the_smaller(size, in - fifo->out);
	/* first get the data from fifo->out until the end of the buffer */
	len = the_smaller(size, fifo->size - (fifo->out & (fifo->size - 1)));	
	memcpy(buffer, fifo->buffer + (fifo->out & (fifo->size - 1)), len);
	/* then get the rest (if any) from the beginning of the buffer */
	memcpy(buffer + len, fifo->buffer, size - len);
	
	return size;	
}

uint32_t __btfifo_put(uint8_t fid, uint8_t *buffer, uint32_t size)
{
	uint32_t len = 0;
	btfifo_t * fifo = &fifo_ctrl_block[fid];
	
	size = the_smaller(size, fifo->size - (fifo->in - fifo->out));
	len  = the_smaller(size, fifo->size - (fifo->in & (fifo->size - 1)));
	if(buffer)
	{
		memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buffer, len);
		/* then put the rest (if any) at the beginning of the buffer */
		memcpy(fifo->buffer, buffer + len, size - len);
	}
	fifo->in += size;
	
	return size;
}

const fifo_inst_t fifo_inst = 
{
	&__btfifo_init,
	&__btfifo_len,
	&__btfifo_get,
	&__btfifo_peek,
	&__btfifo_put,
};
	
