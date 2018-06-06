#include "int_handler.h"
#include "tm4c1294ncpdt.h" // microcontroller components
#include "stdio.h"

void IntHandlerUART2 (void) {
    if (UART6_MIS_R & (1<<4)) {  // check whether UART2 Rx interrupt
        UART6_ICR_R |=(1<<4); // clear interrupt
        gucRxChar = UART6_DR_R; // read byte from UART2 data register
        printf("UART key received; %c\n", gucRxChar);
        gucNewData = 1;
    }
}