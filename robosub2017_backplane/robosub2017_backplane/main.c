/*****************************************************************************
	main.c

	Description:
		Backplane for SDSU Mechatronics 2017 Robosub Perseverance
		

	Created:	06/13/2017
	Author:		Ryan Young
	Email:		RyanaYoung81@gmail.com
******************************************************************************/

/*=============================================================================
				TODO
				
	-adc setup
	-measure/store current/voltage sense
	-setup response frame to tegra request
	-setup over/under current critical interrupt
		-setup error lights accordingly
	
	-possibly add acceptance of all can messages intended for tegra
		and relay over serial & trigger error leds
	-setup backup serial2can interface if tegra can board doesn't work.
=============================================================================*/

#define F_CPU 16000000UL // 16MHz clock from the debug processor
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include <util/atomic.h>


/******************************************************************************
	CANBUS ID definition|
		
******************************************************************************/
//RxID is your device ID that you allow messages to receive
uint16_t RxID = 0x001;  

//TxID is the target ID you're transmitting to
uint8_t TxID = 0x020;	//M
//uint8_t TxID = 0x10;	//S
/******************************************************************************/


#include "headers/global.h"			//general define header pulled from net
#include "headers/defines.h"		//Pin name definitions
#include "headers/functions.h"		//general functions
#include "headers/spi_ry.h"			//SPI protocol implementation
#include "headers/usart_ry.h"		//serial communication with PC
#include "headers/mcp2515_ry_def.h"	//MCP2515 register and bit definitions
#include "headers/mcp2515_ry.h"		//MCP2515 functions

tCAN usart_char;	//transmit package
tCAN spi_char;		//receive package

volatile uint8_t rx_flag = 0;

uint16_t voltage_sense[4];
uint8_t adc_select = 0;

/******************************************************************************
	start of main()|
******************************************************************************/
int main(void)
{
  	//initialization functions
	GPIO_init();
	INTERRUPT_init();
	USART_Init(103);//103 sets baud rate at 9600
	SPI_masterInit();
	
	//MCP2515 initialization
	if(mcp2515_init(CANSPEED_500))
	{
		USART_Transmit_TX("Can Init SUCCESS!");
	}else
	{
		USART_Transmit_TX("Can Init FAILURE!");
	}
	USART_Transmit(10);//New Line
	USART_Transmit(13);//Carriage return
	
	//setup the transmit frame
	usart_char.id = TxID;			//set target device ID
	usart_char.header.rtr = 0;		//no remote transmit(i.e. request info)
	usart_char.header.length = 1;	//single byte(could be up to 8)
	
	while(1)
    {
		if(!(UCSR0A & (1<<RXC0)))//if data in serial buffer
		{
			//get serial data
			usart_char.data[0] = USART_Receive();
			
			//transmit usart_char over canbus
			mcp2515_send_message(&usart_char);
		}
		
		if(rx_flag){
			ATOMIC_BLOCK(ATOMIC_FORCEON){
				USART_Transmit(spi_char.id >> 8); //CanID_High

				USART_Transmit(spi_char.id); //CandID_Low

				USART_Transmit(spi_char.header.rtr); //rtr

				USART_Transmit(spi_char.header.length); //length

				
				//read back all data received.
				if(!spi_char.header.rtr){
					for (uint8_t t = 0; t < spi_char.header.length;t++) {
						USART_Transmit(spi_char.data[t]); //data
					}
				}
				rx_flag = 0;
			}
		}
		
		if(adc_select == 4){
			USART_Transmit()
			
		}
		
		TOGGLE(LED1);
		_delay_ms(200);
		TOGGLE(LED2);
		_delay_ms(200);
		TOGGLE(LED3);
		_delay_ms(200);
		TOGGLE(LED4);
		_delay_ms(200);
		
    }
}

/******************************************************************************
	RECEIVE interrupt on pin PD2|
******************************************************************************/
ISR(INT0_vect)
{
	mcp2515_get_message(&spi_char);//get canbus message
	rx_flag = 1;  //set flag
}

/******************************************************************************* 
	 ADC conversion complete ISR|
*******************************************************************************/ 
ISR(ADC_vect) 
{ 
	Read_Request_Backplane_Current.data[adc_select] = map(ADC, 0, 1023, 0, 50);
	
	adc_select++;
	if(adc_select > 3)
		adc_select = 0;
		
	
	
	switch(adc_select){
		case 0 : ADMUX &= 0b11110000; //set ADC0
			break;	
		case 1 : ADMUX &= 0b11110001; //set ADC1
		break;
		case 2 : ADMUX &= 0b11110110; //set ADC6
		break;
		case 3 : ADMUX &= 0b11110111; //set ADC7
		break;
		default : ADMUX &= 0b11110000; //set ADC0
		break;
	}
	ADCSRA |= (1<<ADSC); //start adc sample
}
