#include "platform_config.h"

#ifdef CFG_PROTOCOL_USART1
#include "stm32f10x_usart.h"
#endif // CFG_PROTOCOL_USART1

#ifdef CFG_PROTOCOL_USBCDC
#include "usb_cdc.h"
#endif // CFG_PROTOCOL_USBCDC

#include "protocol.h"
#include "timer.h"
#include "led.h"
#include <string.h>

typedef enum
{
    PROTOCOL_DECODER_STATE_SYNC_0 = 0,
    PROTOCOL_DECODER_STATE_SYNC_1,
    PROTOCOL_DECODER_STATE_CLASS_ID,
    PROTOCOL_DECODER_STATE_MSG_ID,
    PROTOCOL_DECODER_STATE_LENGTH_0,
    PROTOCOL_DECODER_STATE_LENGTH_1,
    PROTOCOL_DECODER_STATE_PAYLOAD,
    PROTOCOL_DECODER_STATE_CHECKSUM_0,
    PROTOCOL_DECODER_STATE_CHECKSUM_1
} protocolDecoderState_t;


#ifdef CFG_PROTOCOL_USART1
/* RX Ring Buffer */
#define USART1_RX_BUFFER_SIZE 1024
uint8_t  USART1_Rx_Buffer [USART1_RX_BUFFER_SIZE];
uint32_t USART1_Rx_In_Ptr = 0;
uint32_t USART1_Rx_Out_Ptr = 0;
uint32_t USART1_Rx_Length = 0;

void protocolInit(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO |
                           RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;

    /* TX Pin */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* RX Pin */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);


    USART_InitTypeDef USART_InitStructure;

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);

    USART_Cmd(USART1, ENABLE);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    NVIC_EnableIRQ(USART1_IRQn);
}

void protocolSendChar(uint8_t c)
{
    USART_SendData(USART1, c);

    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
    }
}

uint32_t protocolReadChar(uint8_t *c)
{
    uint32_t available = USART1_Rx_Length;
    if(USART1_Rx_Length > 0)
    {
        *c = USART1_Rx_Buffer[USART1_Rx_Out_Ptr];
        USART1_Rx_Out_Ptr++;
        USART1_Rx_Length--;

        if(USART1_Rx_Out_Ptr >= USART1_RX_BUFFER_SIZE)
        {
            USART1_Rx_Out_Ptr = 0;
        }
    }
    return available;
}

void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t c = USART_ReceiveData(USART1);
        USART1_Rx_Buffer[USART1_Rx_In_Ptr] = c;
        USART1_Rx_In_Ptr++;
        USART1_Rx_Length++;

        if(USART1_Rx_In_Ptr == USART1_RX_BUFFER_SIZE)
        {
            USART1_Rx_In_Ptr = 0;
        }
        if(USART1_Rx_Length == USART1_RX_BUFFER_SIZE)
        {
            // error discard rx
        }
    }
}
#endif // CFG_PROTOCOL_USART1

#ifdef CFG_PROTOCOL_USBCDC
void protocolInit(void)
{
    USB_CDC_Init();
}

void protocolSendChar(uint8_t c)
{
    USB_CDC_Send(c);
}

uint32_t protocolReadChar(uint8_t *c)
{
    return USB_CDC_Read(c);
}
#endif // CFG_PROTOCOL_USBCDC


void protocolPoll(void)
{
    static protocolPacket_t packet;

    if(protocolGetPacket(&packet))
    {
        if(protocolCheckPacket(&packet))
        {
            if(protocolEvaluatePacket(&packet))
            {
                // finish
            }
            else
            {
                // received not implemented packet
                // spit out some error
            	led_usr_on();
				timer_sleep(40000);
				led_usr_off();
            }
        }
        else
        {
            // spit out some error
        	led_usr_on();
        	timer_sleep(10000);
        	led_usr_off();
        }
    }
}

bool protocolGetPacket(protocolPacket_t *packet)
{
    static protocolDecoderState_t state;
    static uint16_t length;
    static timer_ticks_t t;
    bool isWholePacket = false;

    if (timer_delayCount - t > 10000)
    {
        length = 0;
        state = PROTOCOL_DECODER_STATE_SYNC_0;
    }

    uint8_t c;
    while(protocolReadChar(&c) > 0)
    {
    	led_sys_on();
    	timer_sleep(200);
    	led_sys_off();

        switch(state)
        {
        case PROTOCOL_DECODER_STATE_SYNC_0:
            state = (c == PROTOCOL_SYNC_0) ? PROTOCOL_DECODER_STATE_SYNC_1 : PROTOCOL_DECODER_STATE_SYNC_0;
            t = timer_delayCount;
            break;

        case PROTOCOL_DECODER_STATE_SYNC_1:
            state = (c == PROTOCOL_SYNC_1) ? PROTOCOL_DECODER_STATE_CLASS_ID : PROTOCOL_DECODER_STATE_SYNC_0;
            break;

        case PROTOCOL_DECODER_STATE_CLASS_ID:
            packet->msgId = c;
            state = PROTOCOL_DECODER_STATE_MSG_ID;
            break;

        case PROTOCOL_DECODER_STATE_MSG_ID:
            packet->msgId += (c << 8);
            state = PROTOCOL_DECODER_STATE_LENGTH_0;
            break;

        case PROTOCOL_DECODER_STATE_LENGTH_0:
            length = c;
            state = PROTOCOL_DECODER_STATE_LENGTH_1;
            break;

        case PROTOCOL_DECODER_STATE_LENGTH_1:
            length += (c << 8);
            packet->payloadLength = length;
            if(length > 0)
            {
                state = PROTOCOL_DECODER_STATE_PAYLOAD;
            }
            else
            {
                state = PROTOCOL_DECODER_STATE_CHECKSUM_0;
            }
            break;

        case PROTOCOL_DECODER_STATE_PAYLOAD:
            packet->payload[packet->payloadLength - length--] = c;
            if(length == 0)
            {
                state = PROTOCOL_DECODER_STATE_CHECKSUM_0;
            }
            break;

        case PROTOCOL_DECODER_STATE_CHECKSUM_0:
            packet->checksum = c;
            state = PROTOCOL_DECODER_STATE_CHECKSUM_1;
            break;

        case PROTOCOL_DECODER_STATE_CHECKSUM_1:
            packet->checksum += (c << 8);
            state = PROTOCOL_DECODER_STATE_SYNC_0;
            isWholePacket = true;
            break;
        }

        return isWholePacket;
    }
    return isWholePacket;
}

void protocolReplyPacket(uint16_t msgId)
{
    protocolPacket_t packet;
    packet.sync[0] = PROTOCOL_SYNC_0;
    packet.sync[1] = PROTOCOL_SYNC_1;
    packet.msgId = msgId;
    packet.payloadLength = 0;
    packet.checksum = protocolCalculateChecksum(&packet);
    protocolSendPacket(&packet);
}

void protocolSendPacket(protocolPacket_t *packet)
{
    protocolSendChar(packet->sync[0]);
    protocolSendChar(packet->sync[1]);
    protocolSendChar(packet->msgId & 0xFF);
    protocolSendChar(packet->msgId >> 8);
    protocolSendChar(packet->payloadLength & 0xFF);
    protocolSendChar(packet->payloadLength >> 8);

    for(uint16_t i = 0; i < packet->payloadLength; ++i)
    {
        protocolSendChar(packet->payload[i]);
    }

    protocolSendChar(packet->checksum & 0xFF);
    protocolSendChar(packet->checksum >> 8);
}

bool protocolCheckPacket(const protocolPacket_t *packet)
{
    uint16_t cs = protocolCalculateChecksum(packet);
    return (packet->checksum == cs);
}

uint16_t protocolCalculateChecksum(const protocolPacket_t *packet)
{
    uint8_t a = 0;
    uint8_t b = 0;

    a = a + (packet->msgId & 0xFF);
    b = b + a;

    a = a + (packet->msgId >> 8);
    b = b + a;

    a = a + (packet->payloadLength & 0xFF);
    b = b + a;

    a = a + (packet->payloadLength >> 8);
    b = b + a;

    for(uint16_t i = 0; i < packet->payloadLength; ++i)
    {
        a = a + packet->payload[i];
        b = b + a;
    }

    return (b << 8) + a;
}
