#include "int_handler.h"
#include "tm4c1294ncpdt.h" // microcontroller components
#include "stdio.h"
#include "stdlib.h"
#include "math.h"

object direction = s_up;
int loops = 0; // the number of times to swing left to right of the same display image before updating the image

char initial_screen[STREAMSIZE] = {0x00, 0x00, 0xFF, 0xA0, 0xE0, 0x00, 0xFF, 0xA0,
                                0xF0, 0x0F, 0x00, 0xFF, 0x91, 0x91, 0x00, 0xF1, 0x91, 0x9F,
                                0x00, 0xF1, 0x91, 0x9F, 0x00, 0x00, 0xFF, 0x40, 0x00};

void IntHandlerUART6(void) {
    if (UART6_MIS_R & (1<<4)) {  // check whether UART6 Rx interrupt
        UART6_ICR_R |=(1<<4); // clear interrupt

        gucRxChar = UART6_DR_R; // read byte from UART6 data register
        //printf("UART key received; %c\n", gucRxChar);
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
        int i;

        // draw the intro screen
        if(!displayed_intro) {
            if(GPIO_PORTL_DATA_R & 0x1) { // if left edge
                GPIO_PORTM_DATA_R = 0x00;
                for(i = 0; i < STREAMSIZE; i++) {
                    GPIO_PORTM_DATA_R = initial_screen[i];
                    timerWait(1950);
                }
                GPIO_PORTM_DATA_R = 0x00;
            }
            return;
        }
        else {
            // draw the board
            if(GPIO_PORTL_DATA_R & 0x1) { // left edge
                for(i = 0; i < NUMCOL_LED; i++) {
                    GPIO_PORTM_DATA_R = display[i];
                    timerWait(1000);
                }
                GPIO_PORTM_DATA_R = 0x00;
                loops++;
            } else { // right edge
                if(loops == 2) {
                    loops = 0;
                    updateBoard(direction);
                    convertBoard();
                }
                //printf("right edge!\n");
            }
        } // draw the board ; end

        if(game_over) {
            for(i = 0; i < NUMCOL_LED; i++) {
                GPIO_PORTM_DATA_R = 0xFF;
                timerWait(1000);
            }
            exit(0);
        }

    }
}
