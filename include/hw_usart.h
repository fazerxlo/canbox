#ifndef HW_USART_H
#define HW_USART_H

#include <inttypes.h>

struct usart_t;
struct usart_t * hw_usart_get_can(void); // For SLCAN
struct usart_t * hw_usart_get_debug(void); // For Debug Output

void hw_usart_setup(struct usart_t * usart, uint32_t speed, uint8_t * txbuf, uint32_t txbuflen, uint8_t * rxbuf, uint32_t rxbuflen);
void hw_usart_disable(struct usart_t *);

int hw_usart_write(struct usart_t * usart, const uint8_t * ptr, int len);
void hw_usart_wait_transfer(struct usart_t *); // You might not need this
uint8_t hw_usart_read_ch(struct usart_t * usart, uint8_t *ch);

uint32_t hw_usart_get_rx_overflow(struct usart_t * usart);
uint32_t hw_usart_get_tx_overflow(struct usart_t * usart);
uint32_t hw_usart_get_rx(struct usart_t * usart);
uint32_t hw_usart_get_tx(struct usart_t * usart);

#endif