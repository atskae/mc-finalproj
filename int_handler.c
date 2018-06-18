#include "int_handler.h"
#include "tm4c1294ncpdt.h" // microcontroller components
#include "stdio.h"
#include "math.h"

object direction = s_right;
int loops = 0; // the number of times to swing left to right of the same display image before updating the image

void IntHandlerUART6(void) {
    if (UART6_MIS_R & (1<<4)) {  // check whether UART6 Rx interrupt
        UART6_ICR_R |=(1<<4); // clear interrupt

        gucRxChar = UART6_DR_R; // read byte from UART6 data register
        printf("UART key received; %c\n", gucRxChar);
        switch(gucRxChar) {
            case '8':
                direction = s_up;
                break;
            case '6':
            	direction = s_right;
                break;
            case '4':
            	direction = s_left;
                break;
            case '5':
            	direction = s_down;
                break;
        }
	    updateBoard(direction);
	    convertBoard();
        UART6_DR_R = 0;
    }
}

void timerWait(unsigned short usec) {
    unsigned short loadValue = (unsigned short) ceil((usec * 1.0e-6 * FCLK) / (PRESCALER+1)) - 1;
    TIMER0_TAILR_R = loadValue; // set interval load value
    TIMER0_CTL_R   |= 0x1; // start timer A
    while(!(TIMER0_RIS_R & (1<<0))); // wait for time-out, blocking
    TIMER0_ICR_R |= (1<<0); // clear interrupt flag
    TIMER0_CTL_R &= ~0x1; // disable timer
}

// left-right signal on pendulum
void IntHandlerRLSignal(void) {
    if(GPIO_PORTL_MIS_R & (1<<0)) {
        GPIO_PORTL_ICR_R |= (1<<0); // clear interrupt

        // draw the intro screen
        if(!displayed_intro) {
            if(GPIO_PORTL_DATA_R & 0x1) left_edge = 1;
            else left_edge = 0;

            return;
        }
        else {
        	// draw the board
            if(GPIO_PORTL_DATA_R & 0x1) {
            	int j;
                for(j = 0; j < NUMCOL_LED; j++) {
                    GPIO_PORTM_DATA_R = display[j];
                    timerWait(1000);
                }
                GPIO_PORTM_DATA_R = 0x00;
                loops++;
            } else {
            	if(loops == 3) {
            		loops = 0;
            		updateBoard(direction);
            		convertBoard();
            	}
            	//printf("right edge!\n");
            }
        } // draw the board ; end

    }
}
