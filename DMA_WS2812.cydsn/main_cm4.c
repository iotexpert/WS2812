#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WS_ZOFFSET (1)
#define WS_ONE3  (0b110<<24)
#define WS_ZERO3 (0b100<<24)
#define WS_NUM_PIXELS (144)
#define WS_SPI_BIT_PER_BIT (3)
#define WS_COLOR_PER_PIXEL (3)
#define WS_BYTES_PER_PIXEL (WS_SPI_BIT_PER_BIT * WS_COLOR_PER_PIXEL)

static uint8_t WS_frameBuffer[WS_NUM_PIXELS*WS_BYTES_PER_PIXEL+WS_ZOFFSET];

// This is the interrupt handler for the WS DMA
// It doesnt do anything... and is just a stub
static void WS_DMAComplete(void)
{
    Cy_DMA_Channel_ClearInterrupt(WS_DMA_HW, WS_DMA_DW_CHANNEL);   
}

// Function WS_DMAConfiguration
// This function sets up the DMA and the descriptors

#define WS_NUM_DESCRIPTORS (sizeof(WS_frameBuffer) / 256 + 1)
static cy_stc_dma_descriptor_t WSDescriptors[WS_NUM_DESCRIPTORS];
static void WS_DMAConfigure(void)
{
    // I copies this structure from the PSoC Creator Component configuration 
    // in generated source
    const cy_stc_dma_descriptor_config_t WS_DMA_Descriptors_config =
    {
    .retrigger       = CY_DMA_RETRIG_IM,
    .interruptType   = CY_DMA_DESCR_CHAIN,
    .triggerOutType  = CY_DMA_1ELEMENT,
    .channelState    = CY_DMA_CHANNEL_ENABLED,
    .triggerInType   = CY_DMA_1ELEMENT,
    .dataSize        = CY_DMA_BYTE,
    .srcTransferSize = CY_DMA_TRANSFER_SIZE_DATA,
    .dstTransferSize = CY_DMA_TRANSFER_SIZE_WORD,
    .descriptorType  = CY_DMA_1D_TRANSFER,
    .srcAddress      = NULL,
    .dstAddress      = NULL,
    .srcXincrement   = 1L,
    .dstXincrement   = 0L,
    .xCount          = 256UL,
    .srcYincrement   = 0L,
    .dstYincrement   = 0L,
    .yCount          = 1UL,
    .nextDescriptor  = 0
    };

    for(unsigned int i=0;i<WS_NUM_DESCRIPTORS;i++)
    {
        Cy_DMA_Descriptor_Init(&WSDescriptors[i], &WS_DMA_Descriptors_config);
        Cy_DMA_Descriptor_SetSrcAddress(&WSDescriptors[i], (uint8_t *)&WS_frameBuffer[i*256]);
        Cy_DMA_Descriptor_SetDstAddress(&WSDescriptors[i], (void *)&WS_SPI_HW->TX_FIFO_WR);
        Cy_DMA_Descriptor_SetXloopDataCount(&WSDescriptors[i],256); // the last
        Cy_DMA_Descriptor_SetNextDescriptor(&WSDescriptors[i],&WSDescriptors[i+1]);
    }
    
    // The last one needs a bit of change
    Cy_DMA_Descriptor_SetXloopDataCount(&WSDescriptors[WS_NUM_DESCRIPTORS-1],sizeof(WS_frameBuffer)-256*(WS_NUM_DESCRIPTORS-1)); // the last
    Cy_DMA_Descriptor_SetNextDescriptor(&WSDescriptors[WS_NUM_DESCRIPTORS-1],0);
    Cy_DMA_Descriptor_SetChannelState(&WSDescriptors[WS_NUM_DESCRIPTORS-1],CY_DMA_CHANNEL_DISABLED);
 
    
     /* Initialize and enable the interrupt from WS_DMA */
    Cy_SysInt_Init(&WS_DMA_INT_cfg, &WS_DMAComplete);
    NVIC_EnableIRQ(WS_DMA_INT_cfg.intrSrc);
    Cy_DMA_Channel_SetInterruptMask(WS_DMA_HW, WS_DMA_DW_CHANNEL, WS_DMA_INTR_MASK);
    
    Cy_DMA_Enable(WS_DMA_HW);    
}

// Function: WS_DMATrigger
// This function sets up the channel... then enables it to dump the frameBuffer to pixels
void WS_DMATrigger()
{
    cy_stc_dma_channel_config_t channelConfig;	
    channelConfig.descriptor  = &WSDescriptors[0];
    channelConfig.preemptable = WS_DMA_PREEMPTABLE;
    channelConfig.priority    = WS_DMA_PRIORITY;
    channelConfig.enable      = false;
    Cy_DMA_Channel_Init(WS_DMA_HW, WS_DMA_DW_CHANNEL, &channelConfig);
    Cy_DMA_Channel_Enable(WS_DMA_HW,WS_DMA_DW_CHANNEL);
}

// Function: WS_SysTickHandler
// This function is called by the systick timer.  It counts up to 33 and then calls the update
// It also makes sure that the channel is inactive
void WS_SysTickHandler()
{
    static int count=0;
    if((Cy_DMA_Channel_GetStatus(WS_DMA_HW,WS_DMA_DW_CHANNEL) & CY_DMA_INTR_CAUSE_COMPLETION) && count++>32)
    {
        WS_DMATrigger();
        count = 0;
    }
}

// Function: convert3Code
// This function takes an 8-bit value representing a color
// and turns it into a WS2812 bit code... where 1=110 and 0=011
// 1 input byte turns into three output bytes of a uint32_t
uint32_t WS_convert3Code(uint8_t input)
{
    uint32_t rval=0;
    for(int i=0;i<8;i++)
    {
        if(input%2)
        {
            rval |= WS_ONE3;
        }
        else
        {
            rval |= WS_ZERO3;
        }
        rval = rval >> 3;
        
        input = input >> 1;
    }
    return rval;
}

// Function: WS_setRGB
// Takes a position and a three byte rgb value and updates the WS_frameBuffer with the correct 9-bytes
void WS_setRGB(int led,uint8_t red, uint8_t green, uint8_t blue)
{
    
    typedef union {
    uint8_t bytes[4];
    uint32_t word;
    } WS_colorUnion;
    
    WS_colorUnion color;
    color.word = WS_convert3Code(green);
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+WS_ZOFFSET] = color.bytes[2];
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+1+WS_ZOFFSET] = color.bytes[1];
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+2+WS_ZOFFSET] = color.bytes[0];
    
    color.word = WS_convert3Code(red);
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+3+WS_ZOFFSET] = color.bytes[2];
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+4+WS_ZOFFSET] = color.bytes[1];
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+5+WS_ZOFFSET] = color.bytes[0];

    color.word = WS_convert3Code(blue);
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+6+WS_ZOFFSET] = color.bytes[2];
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+7+WS_ZOFFSET] = color.bytes[1];
    WS_frameBuffer[led*WS_BYTES_PER_PIXEL+8+WS_ZOFFSET] = color.bytes[0];
}

// Function WS_setRange
// Sets all of the pixels from start to end with the red,green,blue value
void WS_setRange(int start, int end, uint8_t red,uint8_t green ,uint8_t blue)
{
    CY_ASSERT(start >= 0);
    CY_ASSERT(start < end);
    CY_ASSERT(end <= WS_NUM_PIXELS-1);
   
    WS_setRGB(start,red,green,blue);
    for(int i=1;i<=end-start;i++)
    {
        memcpy(&WS_frameBuffer[start*WS_BYTES_PER_PIXEL+i*WS_BYTES_PER_PIXEL+WS_ZOFFSET],
        &WS_frameBuffer[start*WS_BYTES_PER_PIXEL+WS_ZOFFSET],WS_BYTES_PER_PIXEL);
    }
}
// Function: WS_runTest
// This function just runs test-asserts against the pixel calculation functions
void WS_runTest()
{
    printf("Size of WS_frameBuffer = %d\n",sizeof(WS_frameBuffer));
 
    // Some unit tests for the convert3Code Function
    printf("Test 0x00 = %0X\n",(unsigned int)WS_convert3Code(0));
    printf("Test 0xFF = %0X\n",(unsigned int)WS_convert3Code(0xFF));
    printf("Test 0x80 = %0X\n",(unsigned int)WS_convert3Code(0x80));
    
    // Make sure that WS_convert3Code does the right thing
    CY_ASSERT(WS_convert3Code(0x00) == 0b00000000100100100100100100100100);
    CY_ASSERT(WS_convert3Code(0x80) == 0b00000000110100100100100100100100);
    CY_ASSERT(WS_convert3Code(0xFF) == 0b00000000110110110110110110110110);
    
 
    CY_ASSERT(WS_NUM_PIXELS>=3); // we are goign to test 3 locations
    // Test the WS_setRGB
    WS_setRGB(0,0x80,0,0xFF);
    
    // 0
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+0] == 0b10010010);
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+1] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+2] == 0b00100100);
    
    // 0x80
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+3] == 0b11010010); 
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+4] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+5] == 0b00100100);
    
    // 0xFF
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+6] == 0b11011011);
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+7] == 0b01101101);
    CY_ASSERT(WS_frameBuffer[0*9+WS_ZOFFSET+8] == 0b10110110);

    WS_setRGB(1,0,0xFF,0x80);
 
    // 0xFF
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+0] == 0b11011011);
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+1] == 0b01101101);
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+2] == 0b10110110);

    // 0
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+3] == 0b10010010);
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+4] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+5] == 0b00100100);
    
    // 0x80
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+6] == 0b11010010); 
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+7] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[1*9+WS_ZOFFSET+8] == 0b00100100);
    
    WS_setRGB(2,0xFF,0x80,0);
    
    // 0x80
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+0] == 0b11010010); 
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+1] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+2] == 0b00100100);
    
    // 0xFF
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+3] == 0b11011011);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+4] == 0b01101101);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+5] == 0b10110110);

    // 0
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+6] == 0b10010010);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+7] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+8] == 0b00100100);

    
    // change the values of the range
    WS_setRGB(2,0,0,0);  
    WS_setRGB(3,0,0,0);  
    WS_setRGB(4,0,0,0);  
    
    // Test the WS_setRange
    WS_setRange(2,4,0xFF,0x80,0);
    
    // 0x80
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+0] == 0b11010010); 
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+1] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+2] == 0b00100100);
    
    // 0xFF
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+3] == 0b11011011);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+4] == 0b01101101);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+5] == 0b10110110);

    // 0
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+6] == 0b10010010);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+7] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[2*9+WS_ZOFFSET+8] == 0b00100100);
    
    // 0x80
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+0] == 0b11010010); 
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+1] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+2] == 0b00100100);
    
    // 0xFF
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+3] == 0b11011011);
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+4] == 0b01101101);
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+5] == 0b10110110);

    // 0
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+6] == 0b10010010);
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+7] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[3*9+WS_ZOFFSET+8] == 0b00100100);

    // 0x80
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+0] == 0b11010010); 
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+1] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+2] == 0b00100100);
    
    // 0xFF
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+3] == 0b11011011);
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+4] == 0b01101101);
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+5] == 0b10110110);

    // 0
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+6] == 0b10010010);
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+7] == 0b01001001);
    CY_ASSERT(WS_frameBuffer[4*9+WS_ZOFFSET+8] == 0b00100100);

  
    for(int i=0;i<10;i++)
    {
        printf("%02X ",WS_frameBuffer[i]);
    }
    printf("\n");
    
}

// Initializes the RGB frame buffer to RGBRGBRGB...RGB
void WS_initMixColorRGB()
{
    for(int i=0;i<WS_NUM_PIXELS;i++)
    {
        switch(i%3)
        {
            case 0:
                WS_setRGB(i,0x80,0x00,0x00); // red
                break;
            case 1:
                WS_setRGB(i,0x00,0x80,0x00); // green
                break;
            case 2:
                WS_setRGB(i,0x00,0x00,0x80); // blue
                break;
        }
    }
}

// Function: WS_Start
// This function intiaizlies everything... 
void WS_Start()
{
    WS_runTest();
    WS_frameBuffer[0] = 0x00;
    WS_setRange(0,WS_NUM_PIXELS-1,0,0,0); // Initialize everything OFF
    Cy_SCB_SPI_Init(WS_SPI_HW, &WS_SPI_config, &WS_SPI_context);
    Cy_SCB_SPI_Enable(WS_SPI_HW);
    WS_DMAConfigure();
}

int main(void)
{
    __enable_irq(); /* Enable global interrupts. */

    UART_1_Start();
    setvbuf( stdin, NULL, _IONBF, 0 ); 
    printf("Started\n");   
    WS_Start();
    
    Cy_SysTick_Init(CY_SYSTICK_CLOCK_SOURCE_CLK_IMO,8000);
    Cy_SysTick_Enable();
  
    for(;;)
    {
        char c=getchar();
        switch(c)
        {        
            case 'u':
                printf("Enable auto DMA updating\n");
                WS_DMATrigger();
                Cy_SysTick_SetCallback(0,WS_SysTickHandler);
                
            break;
            case 'U':
                printf("Disable auto DMA updating\n");
                Cy_SysTick_SetCallback(0,0);
            break;
            case 't':
                printf("Trigger DMA\n");
                WS_DMATrigger();
            break;        
            case 'r':
                WS_setRGB(0,0xFF,0,0);
                printf("Set LED0 Red\n");
                break;
           case 'g':
                WS_setRGB(0,0,0xFF,0);
                printf("Set LED0 Green\n");
                break;            
            case 'O':
                WS_setRange(0,WS_NUM_PIXELS-1,0,0,0);
                printf("Turn off all LEDs\n");
                break;
            case 'o':
                WS_setRange(0,WS_NUM_PIXELS-1,0xFF,0xFF,0xFF);
                printf("Turn on all LEDs\n");
                break;
            case 'b':
                WS_setRGB(0,0,0,0xFF);
                printf("Set LED0 Blue\n");
                break;        
            case 'R':
                WS_setRange(0,WS_NUM_PIXELS-1,0x80,0,0);
                printf("Turn on all LEDs RED\n");
                break;
            case 'G':
                WS_setRange(0,WS_NUM_PIXELS-1,0,0x80,0);
                printf("Turn on all LEDs Green\n");
                break;
            case 'B':
                WS_setRange(0,WS_NUM_PIXELS-1,0,0,0x80);
                printf("Turn on all LEDs Blue\n");
                break;     
            case 'a':
                WS_initMixColorRGB();
                printf("Turn on all LEDs RGB Pattern\n");
                break;
            case '?':
                printf("u\tEnable Auto Update of LEDs\n");
                printf("U\tDisable Auto Update of LEDs\n");
                printf("t\tTrigger the DMA\n");
                printf("r\tSet the first pixel Red\n");
                printf("g\tSet the first pixel Green\n");
                printf("b\tSet the first pixel Blue\n");
                printf("O\tTurn off all of the pixels\n");
                printf("o\tSet the pixels to white full on\n");
                printf("R\tSet all of the pixels to Red\n");
                printf("G\tSet all of the pixels to Green\n");
                printf("B\tSet all of the pixels to Blue\n");
                printf("a\tSet pixels to repeating RGBRGB\n");
                printf("?\tHelp\n");
                break;
        }
    }
}
