#include <asf.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>

#define IR_RECEIVER_PIN IOPORT_CREATE_PIN(PORTE, 4) // Digital pin 2. Replace with the actual pin number connected to VS1838B output
#define IR_LED_PIN     IOPORT_CREATE_PIN(PORTE, 5) // Pin 3 IR LED
#define EMITTER_LED_PIN IOPORT_CREATE_PIN(PORTG, 5) // Pin 4 emitter LED
#define UBRR_VALUE 51  // Example value for 9600 baud rate

volatile uint8_t irCodeReceived = 0;
volatile uint32_t lastIrCode = 0;
volatile bool isNECProtocol = false;

void my_delay_us(uint16_t microseconds);
void my_delay_ms(uint16_t miliseconds);
bool read_pin_level(port_pin_t pin);
void sendIRCode(uint32_t code);



void my_delay_us(uint16_t microseconds)
{
	delay_us(microseconds);
}

void my_delay_ms(uint16_t miliseconds)
{
	delay_ms(miliseconds);
}

bool read_pin_level(port_pin_t pin)
{
	return (PINF & (1 << pin)) != 0;
}

ISR(INT4_vect)
{
	static uint8_t dataBitCount = 0;
	static uint32_t receivedCode = 0;

	dataBitCount++;
	if (dataBitCount == 1)
	{
		if (!read_pin_level(IR_RECEIVER_PIN))
		{
			isNECProtocol = true;
			receivedCode = 0;
		}
		else
		{
			isNECProtocol = false;
			// Handle other protocols if needed
		}
	}
	else if (dataBitCount > 1 && dataBitCount <= 32)
	{
		receivedCode <<= 1;
		if (read_pin_level(IR_RECEIVER_PIN))
		receivedCode |= 1;
	}
	else if (dataBitCount > 32)
	{
		if (isNECProtocol)
		{
			lastIrCode = receivedCode;
			irCodeReceived = 1;
		}

		dataBitCount = 0;
	}
}

void sendIRCode(uint32_t code)
{
	// Adjust the following values to match the desired carrier frequency and delays
	const uint16_t carrierFrequency = 38000;      // Carrier frequency in Hz
	const uint16_t period = 1000000 / carrierFrequency;  // Carrier period in microseconds
	const uint16_t pulseDuration = period / 3;       // Pulse duration in microseconds
	const uint16_t delayAfterTransmit = 2000;        // Delay after transmission in microseconds

	// Transmission preamble
	ioport_set_pin_level(EMITTER_LED_PIN, true);
	my_delay_us(period);
	ioport_set_pin_level(EMITTER_LED_PIN, false);
	my_delay_us(period);

	// Transmission of code bits
	for (uint8_t i = 0; i < 32; i++)
	{
		bool bitValue = (code >> (31 - i)) & 0x01;
		ioport_set_pin_level(EMITTER_LED_PIN, true);
		my_delay_us(bitValue ? pulseDuration : pulseDuration * 2);
		ioport_set_pin_level(EMITTER_LED_PIN, false);
		my_delay_us(pulseDuration);
	}

	// Delay after transmission
	my_delay_us(delayAfterTransmit);
}

void init_usart()
{
	// Set baud rate
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)(UBRR_VALUE);

	// Enable receiver and transmitter
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	// Set frame format: 8 data bits, 1 stop bit, no parity
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}



FILE uart_output = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
FILE uart_input = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);
void uart_putchar(char c);
char uart_getchar(void);
int main(void)
{
	sysclk_init();
	board_init();
	delay_init();
	ioport_init();

	// Initialize the IR receiver pin
	ioport_set_pin_dir(IR_RECEIVER_PIN, IOPORT_DIR_INPUT);
	ioport_set_pin_mode(IR_RECEIVER_PIN, IOPORT_MODE_PULLUP);

	// Initialize the IR LED pin
	ioport_set_pin_dir(IR_LED_PIN, IOPORT_DIR_OUTPUT);

	// Initialize the emitter LED pin
	ioport_set_pin_dir(EMITTER_LED_PIN, IOPORT_DIR_OUTPUT);

	// Configure interrupt for IR receiver pin
	EICRB |= (1 << ISC41);  // Set interrupt to trigger on any logical change
	EIMSK |= (1 << INT4);   // Enable INT4 interrupt

	init_usart();           // Initialize USART communication

	stdout = &uart_output;  // Configure stdout to use USART
	stdin = &uart_input;    // Configure stdin to use USART

	sei();  // Enable global interrupts

	while (1)
	{
		// Wait for the IR code to be received
		while (!irCodeReceived)
		{
			// Do other tasks while waiting
		}

		// IR code received, blink the debug LED
		for (int i = 0; i < 2; i++)
		{
			ioport_toggle_pin_level(IR_LED_PIN);
			my_delay_ms(500);
		}

		// Send the received IR code immediately
		sendIRCode(lastIrCode);

		// Print the button pressed to the console
		printf("Button Pressed: %lu\n", lastIrCode);

		// Reset the flag
		irCodeReceived = 0;
	}
	
	int uart_putchar(char c, FILE *stream)
	{
		if (c == '\n')
		uart_putchar('\r', stream);

		// Wait for empty transmit buffer
		while (!(UCSR0A & (1 << UDRE0)))
		;

		// Put the data into the buffer and send
		UDR0 = c;
		return 0;
	}

	int uart_getchar(FILE *stream)
	{
		// Wait for data to be received
		while (!(UCSR0A & (1 << RXC0)))
		;

		// Get and return received data
		return UDR0;
	}

	return 0;
}
