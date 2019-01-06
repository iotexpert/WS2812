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

static const uint8_t txBuffer[] = {'a','b','c','d'};

static void TxDmaComplete(void)
{
    /* Check tx DMA status */
    if ((CY_DMA_INTR_CAUSE_COMPLETION    != Cy_DMA_Channel_GetStatus(txDma_HW, txDma_DW_CHANNEL)) &&
        (CY_DMA_INTR_CAUSE_CURR_PTR_NULL != Cy_DMA_Channel_GetStatus(txDma_HW, txDma_DW_CHANNEL)))
    {
        /* DMA error occured while TX operations */
        //HandleError();
    }
    
    /* Clear tx DMA interrupt */
    Cy_DMA_Channel_ClearInterrupt(txDma_HW, txDma_DW_CHANNEL);   
    Cy_GPIO_Inv(red_PORT,red_NUM);
}

static void ConfigureTxDma(void)
{
    
    
    /* Initialize descriptor */
    Cy_DMA_Descriptor_Init(&txDma_Descriptor_1, &txDma_Descriptor_1_config);
     
    /* Set source and destination for descriptor 1 */
    Cy_DMA_Descriptor_SetSrcAddress(&txDma_Descriptor_1, (uint8_t *)txBuffer);
    Cy_DMA_Descriptor_SetDstAddress(&txDma_Descriptor_1, (void *)&SPI_1_HW->TX_FIFO_WR);
    
    Cy_DMA_Descriptor_SetXloopDataCount(&txDma_Descriptor_1,4);
    
     /* Initialize and enable the interrupt from TxDma */
    Cy_SysInt_Init(&intTxDma_cfg, &TxDmaComplete);
    NVIC_EnableIRQ(intTxDma_cfg.intrSrc);
    Cy_DMA_Channel_SetInterruptMask(txDma_HW, txDma_DW_CHANNEL, txDma_INTR_MASK);
    
    Cy_DMA_Enable(txDma_HW);    
    
}

void triggerTxDMA()
{
        /* Initialize the DMA channel */
    cy_stc_dma_channel_config_t channelConfig;	
    channelConfig.descriptor  = &txDma_Descriptor_1;
    channelConfig.preemptable = txDma_PREEMPTABLE;
    channelConfig.priority    = txDma_PRIORITY;
    channelConfig.enable      = false;
    Cy_DMA_Channel_Init(txDma_HW, txDma_DW_CHANNEL, &channelConfig);

    Cy_DMA_Channel_Enable(txDma_HW,txDma_DW_CHANNEL);
}

int main(void)
{
    __enable_irq(); /* Enable global interrupts. */


    UART_1_Start();
    setvbuf( stdin, NULL, _IONBF, 0 ); 
    printf("Started\n");    

    cy_en_scb_spi_status_t initStatus;
    cy_en_sysint_status_t sysSpistatus;
        
    /* Configure component */
    initStatus = Cy_SCB_SPI_Init(SPI_1_HW, &SPI_1_config, &SPI_1_context);

    Cy_SCB_SPI_Enable(SPI_1_HW);
    Cy_SCB_SPI_SetActiveSlaveSelect(SPI_1_HW, SPI_1_SPI_SLAVE_SELECT0);
    
    ConfigureTxDma();
            
    for(;;)
    {
        char c=getchar();
        switch(c)
        {
            case 'a':
                printf("Sending a\n");
                
                Cy_SCB_SPI_Write(SPI_1_HW,0xA0);
                break;
              
            case 't':
                printf("Trigger DMA\n");
                triggerTxDMA();
            break;    
            case '?':
                printf("?\tHelp\n");
                break;
                
        }
    }
}

/* [] END OF FILE */
