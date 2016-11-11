/**
 * @author Jaroslaw Surmacz <jsurmacz@gmail.com>
 **/

#include <inttypes.h>
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "lcd.h"
#include <string.h>

#define USART_BAUDRATE 9600 //Baudrate definition
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1) //magic number for baudrate setup :)
volatile unsigned char flag;// we need this one for interrupt service routine

void uart_str_P(const char * str);

struct token {
    char value[7];
    char buffer[12];
    char process;
};

#define COMMAND_COUNT 3
struct token tokens[COMMAND_COUNT];
int8_t j=0;

int8_t items_to_go = COMMAND_COUNT;

//initialising USART; static inline! doing it only once!
static inline void usart_init(void)
{
	UBRRL = BAUD_PRESCALE;	//kload 8 lower bits of baud prescaler
	UBRRH = (BAUD_PRESCALE >> 8);	//shift for 8 places right
	UCSRC = 0b00000110;	//asynchroneous comm,no parity, 1 stop bit, 8 bit character size, clock polarity 0
	UCSRB = 0b00011000;	//no interrupts 000, RX enable 1, TX enable 1, no ninth bit 000
}

void usart_putchar(char c)	//writes one char to USART
{
	while (!(UCSRA & (1 << UDRE))); //check if char is sent, wait UDRE to be set
	UDR = c;							//char goes out
	return;
}

void str_push_c (char * framebuff, int buff_len, char addition) {
	int8_t str_len = strlen(framebuff);
	int8_t i = 0;
    // framebuffer not-overloaded
    if (str_len < buff_len) {
        framebuff[str_len] = addition;
        framebuff[str_len+1] = '\0';
        return;
    }

    // framebuffer overloaded
    while (i < buff_len) {
        framebuff[i] = framebuff[i+1];
        i++;
    }

    framebuff[buff_len-1] = addition;
    framebuff[buff_len] = '\0';
    return;

}

void process(int struct_index, char c, char *key) {
    str_push_c(tokens[struct_index].buffer, strlen(key), c);
    // keyword found
    if (!strcmp(tokens[struct_index].buffer, key)) {
        // item found - enable processor
        tokens[struct_index].process = 1;
    }

    if (tokens[struct_index].process == 1 && (c == ',' || c == '}')) {
        // set index to 0 and disable further processing
    	items_to_go--;
        tokens[struct_index].process = 0;
        j=0;
    }


    if (tokens[struct_index].process == 1 && c != ':' && c != ',' && c != '\"') {
            tokens[struct_index].value[j++] = c;
    }

}

void usart_fillbuffer(void)	//load buffer until enter pressed
{
	unsigned char c = 0;
	while (items_to_go)			//repeat until all commands be processed
	{
		//wait for char from USART
		while (!(UCSRA & (1 << RXC)));

		c = UDR;
		process(0, c, "\"temp\"");
		process(1, c, "\"pressure\"");
		process(2, c, "\"humidity\"");

	}
	return;
}

const char at_1[] PROGMEM = "AT+CIPMUX=1\r\n";
const char at_2[] PROGMEM = "AT+CIPSTART=4,\"TCP\",\"api.openweathermap.org\",80\r\n";

// eg. Tychy, Poland
const char at_3[] PROGMEM = "AT+CIPSEND=4,419\r\n";
const char at_4[] PROGMEM = "GET /data/2.5/weather?q=Tychy,pl&APPID=2631d99144dff74a9ee01e4069702a74&units=metric&lang=pl HTTP/1.1\nHost: api.openweathermap.org\nConnection: keep-alive\nCache-Control: max-age=0\nUpgrade-Insecure-Requests: 1\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.71 Safari/537.36\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\n\n";

//// eg. Warsaw, Poland
//const char at_3[] PROGMEM = "AT+CIPSEND=4,420\r\n";
//const char at_4[] PROGMEM = "GET /data/2.5/weather?q=Warsaw,pl&APPID=2631d99144dff74a9ee01e4069702a74&units=metric&lang=pl HTTP/1.1\nHost: api.openweathermap.org\nConnection: keep-alive\nCache-Control: max-age=0\nUpgrade-Insecure-Requests: 1\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.71 Safari/537.36\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\n\n";

//// eg. London, UK
//const char at_3[] PROGMEM = "AT+CIPSEND=4,420\r\n";
//const char at_4[] PROGMEM = "GET /data/2.5/weather?q=London,uk&APPID=2631d99144dff74a9ee01e4069702a74&units=metric&lang=pl HTTP/1.1\nHost: api.openweathermap.org\nConnection: keep-alive\nCache-Control: max-age=0\nUpgrade-Insecure-Requests: 1\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.71 Safari/537.36\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\n\n";

const char at_5[] PROGMEM = "AT+CIPCLOSE=4\r\n";

void uart_str_P(const char * str) {
	char znak;
	while ((znak = pgm_read_byte(str++)))
		usart_putchar(((znak >= 0x80) && (znak <= 0x87)) ? (znak & 0x07) : znak);
}

int main(void) {

	//init USART
	usart_init();
	lcd_init(LCD_DISP_ON);
	lcd_clrscr();

	// wait some time for ESP8266 to be initialized
	_delay_ms(2000);

	// some ascii-art on lcd during http negotiation
	lcd_puts_p(PSTR("- |"));
	uart_str_P(at_1);
	_delay_ms(500);
	lcd_clrscr();
	lcd_puts_p(PSTR("\\ ||"));
	uart_str_P(at_2);
	_delay_ms(1000);
	lcd_clrscr();
	lcd_puts_p(PSTR("| |||"));
	uart_str_P(at_3);
	_delay_ms(500);
	lcd_clrscr();
	lcd_puts_p(PSTR("/ ||||"));
	uart_str_P(at_4);

	// wait & parse response
	lcd_clrscr();
	usart_fillbuffer();

	// temperature
	lcd_puts_p(PSTR("T:"));
	lcd_puts(tokens[0].value);
	lcd_puts_p(PSTR("C\n"));

	// pressure
	lcd_puts_p(PSTR("P:"));
	lcd_puts(tokens[1].value);
	lcd_puts_p(PSTR("hPa "));

	// humidity
	lcd_puts_p(PSTR("H:"));
	lcd_puts(tokens[2].value);
	lcd_puts_p(PSTR("%"));

	// close connection
	_delay_ms(1000);
	uart_str_P(at_5);

	return 0;

}


