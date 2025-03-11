#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/f1/nvic.h>

#include "ring.h"
#include "hw_gpio.h"
#include "hw_usart.h"

struct usart_t
{
	uint32_t baddr;
	uint32_t rcc;
	uint32_t irq;
	struct gpio_t tx;
	struct gpio_t rx;
	struct ring_t tx_ring;
	struct ring_t rx_ring;
	uint16_t baudrate;
	uint32_t rx_cnt;
	uint32_t tx_cnt;
};

// USART1 for SLCAN (CAN communication)
static struct usart_t usart1 =
{
	.baddr = USART1,
	.rcc = RCC_USART1,
	.tx = GPIO_INIT(A, 9),
	.rx = GPIO_INIT(A, 10),
	.irq = NVIC_USART1_IRQ,
	.baudrate = 0,
	.rx_cnt = 0,
	.tx_cnt = 0,
};

// USART2 for Debug Output
static struct usart_t usart2 =  // Use USART2 for debug
{
    .baddr = USART2,
    .rcc = RCC_USART2,
    .tx = GPIO_INIT(A, 2), // Different pins for USART2
    .rx = GPIO_INIT(A, 3),
    .irq = NVIC_USART2_IRQ, // Different interrupt
    .baudrate = 0,        // Initialize baudrate
    .rx_cnt = 0,
    .tx_cnt = 0,
};

// Buffers for USART1 (SLCAN)
static uint8_t usart1_tx_ring_buffer[512];
static uint8_t usart1_rx_ring_buffer[32];

// Buffers for USART2 (Debug) - Make these large enough for your debug messages
static uint8_t usart2_tx_ring_buffer[512];  // Adjust size as needed
static uint8_t usart2_rx_ring_buffer[32];   // Adjust size as needed

struct usart_t * hw_usart_get_can(void)
{
	return &usart1;
}

struct usart_t * hw_usart_get_debug(void)
{
    return &usart2; // Return the debug USART instance
}

void hw_usart_setup(struct usart_t * usart, uint32_t speed, uint8_t * txbuf, uint32_t txbuflen, uint8_t * rxbuf, uint32_t rxbuflen)
{
	ring_init(&usart->tx_ring, txbuf, txbuflen);
	ring_init(&usart->rx_ring, rxbuf, rxbuflen);

	usart_disable(usart->baddr);

	/* Enable clocks for USART. */
	rcc_periph_clock_enable(usart->rcc);
	rcc_periph_clock_enable(RCC_GPIOA); // Assuming both USART1 and USART2 use GPIOA


	/* Enable the USART interrupt. */
	nvic_enable_irq(usart->irq);

	gpio_set_mode(usart->tx.port, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, usart->tx.pin);
	gpio_set_mode(usart->rx.port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, usart->rx.pin);

	/* Setup UART parameters. */
	usart_set_baudrate(usart->baddr, speed);
	usart_set_databits(usart->baddr, 8);
	usart_set_stopbits(usart->baddr, USART_STOPBITS_1);
	usart_set_parity(usart->baddr, USART_PARITY_NONE);
	usart_set_flow_control(usart->baddr, USART_FLOWCONTROL_NONE);
	usart_set_mode(usart->baddr, USART_MODE_TX_RX);

	/* Enable USART Receive interrupt. */
	USART_CR1(usart->baddr) |= USART_CR1_RXNEIE;

	/* Finally enable the USART. */
	usart_enable(usart->baddr);
}

uint32_t usart_isr_cnt = 0; // You might want separate counters for each USART

void usart_isr(struct usart_t * usart)
{
	usart_isr_cnt++;

	/* Check if we were called because of RXNE. */
	if (((USART_CR1(usart->baddr) & USART_CR1_RXNEIE) != 0) &&
	    ((USART_SR(usart->baddr) & USART_SR_RXNE) != 0)) {

		usart->rx_cnt++; // Increment the specific USART's counter

		/* Retrieve the data from the peripheral. */
		ring_write_ch(&usart->rx_ring, usart_recv(usart->baddr));
	}

	/* Check if we were called because of TXE. */
	if (((USART_CR1(usart->baddr) & USART_CR1_TXEIE) != 0) &&
	    ((USART_SR(usart->baddr) & USART_SR_TXE) != 0)) {

		uint8_t ch;
		if (!ring_read_ch(&usart->tx_ring, &ch)) {

			/* Disable the TXE interrupt, it's no longer needed. */
			USART_CR1(usart->baddr) &= ~USART_CR1_TXEIE;
		} else {

			usart->tx_cnt++;  //Increment the specific USART's counter

			/* Put data into the transmit register. */
			usart_send(usart->baddr, ch);
		}
	}
}

// ISRs for *both* USARTs
void usart1_isr(void)
{
	usart_isr(&usart1);
}

void usart2_isr(void)
{
    usart_isr(&usart2);
}


int hw_usart_write(struct usart_t * usart, const uint8_t * ptr, int len)
{
	int ret = ring_write(&usart->tx_ring, (uint8_t *)ptr, len);
    // Start transmitting.
	USART_CR1(usart->baddr) |= USART_CR1_TXEIE;

	return ret;
}

void hw_usart_disable(struct usart_t * usart)
{
    // Disable and clear interrupts.
    USART_CR1(usart->baddr) &= ~(USART_CR1_TXEIE | USART_CR1_RXNEIE);
	usart_disable(usart->baddr);
	hw_gpio_set_float(&usart->rx);
	hw_gpio_set_float(&usart->tx);
}

uint8_t hw_usart_read_ch(struct usart_t * usart, uint8_t *ch)
{
	return ring_read_ch(&usart->rx_ring, ch);
}

uint32_t hw_usart_get_rx_overflow(struct usart_t * usart)
{
	return ring_get_overflow(&usart->rx_ring);
}

uint32_t hw_usart_get_tx_overflow(struct usart_t * usart)
{
	return ring_get_overflow(&usart->tx_ring);
}

uint32_t hw_usart_get_tx(struct usart_t * usart)
{
	return usart->tx_cnt;
}

uint32_t hw_usart_get_rx(struct usart_t * usart)
{
	return usart->rx_cnt;
}