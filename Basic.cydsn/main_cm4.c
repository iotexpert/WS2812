/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"
#include <stdio.h>

int main(void)
{
    __enable_irq(); /* Enable global interrupts. */

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */


    UART_1_Start();
    setvbuf( stdin, NULL, _IONBF, 0 ); 
    printf("Started\n");    

    cy_en_scb_spi_status_t initStatus;
    cy_en_sysint_status_t sysSpistatus;
        
    /* Configure component */
    initStatus = Cy_SCB_SPI_Init(SPI_1_HW, &SPI_1_config, &SPI_1_context);
    if(initStatus != CY_SCB_SPI_SUCCESS)
    {
        printf("SPI Start Failed\n");
        CY_ASSERT(0);
    }
    Cy_SCB_SPI_Enable(SPI_1_HW);
    
    for(;;)
    {
        char c=getchar();
        switch(c)
        {
            case 'a':
                printf("Sending a\n");
                Cy_SCB_SPI_SetActiveSlaveSelect(SPI_1_HW, SPI_1_SPI_SLAVE_SELECT0);
                Cy_SCB_SPI_Write(SPI_1_HW,0xA0);
                break;
                case 't':
                printf("Toggle Pin_1\n");
                Cy_GPIO_Inv(Pin_1_PORT,Pin_1_NUM);
                break;
            case '?':
                printf("?\tHelp\n");
                break;
                
        }
    }
}

/* [] END OF FILE */
