/********************************** (C) COPYRIGHT  *******************************
 * File Name          : iap.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2020/12/16
 * Description        : IAP
 *  Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/
#include "iap.h"
#include "string.h"
#include "flash.h"
#include "core_riscv.h"
#include "usb_istr.h"
#include "usb_lib.h"
/******************************************************************************/

#define FLASH_Base   0x08005000
#define USBD_DATA_SIZE               64
iapfun jump2app;
u32 Program_addr = 0;
u32 Verity_addr = 0;
u32 User_APP_Addr_offset = 0x5000;
u8 Verity_Star_flag = 0;
u8 Fast_Program_Buf[390];
u32 CodeLen = 0;
u8 End_Flag = 0;
u8 EP2_Rx_Buffer[USBD_DATA_SIZE];
#define  isp_cmd_t   ((isp_cmd  *)EP2_Rx_Buffer)

/*********************************************************************
 * @fn      RecData_Deal
 *
 * @brief   UART-USB deal date
 *
 * @return  ERR_ERROR - ERROR
 *          ERR_SCUESS - SCUESS
 *          ERR_End - End
 */

u8 RecData_Deal(void)
{
    u8 i, s, Lenth;
     u16 OffsetAdr;

     Lenth = isp_cmd_t->Len;
     OffsetAdr = BUILD_UINT16(isp_cmd_t->adr[0], isp_cmd_t->adr[1]);
     switch ( isp_cmd_t->Cmd) {
     case CMD_IAP_ERASE:
         FLASH_Unlock_Fast();
         s = ERR_SCUESS;
         break;

     case CMD_IAP_PROM:
         Program_addr = FLASH_Base + (u32) (OffsetAdr & 0xFF00);
         for (i = 0; i < Lenth; i++) {
             Fast_Program_Buf[CodeLen + i] = isp_cmd_t->data[i];
         }
         CodeLen += Lenth;
         if (CodeLen >= 256) {
             FLASH_Unlock_Fast();
             FLASH_ErasePage_Fast(Program_addr);
             CH32_IAP_Program(Program_addr, (u32*) Fast_Program_Buf);
             CodeLen -= 256;
             for (i = 0; i < CodeLen; i++) {
                 Fast_Program_Buf[i] = Fast_Program_Buf[256 + i];
             }

             Program_addr += 0x100;

         }
         s = ERR_SCUESS;
         break;

     case CMD_IAP_VERIFY:
         if (Verity_Star_flag == 0) {
             Verity_Star_flag = 1;

             for (i = 0; i < (256 - CodeLen); i++) {
                 Fast_Program_Buf[CodeLen + i] = 0xff;
             }

             FLASH_ErasePage_Fast(Program_addr);
             CH32_IAP_Program(Program_addr, (u32*) Fast_Program_Buf);
             CodeLen = 0;
         }

         Verity_addr = FLASH_Base + (u32)  (OffsetAdr );

         s = ERR_SCUESS;
         for (i = 0; i < Lenth; i++) {
             if (isp_cmd_t->data[i] != *(u8*) (Verity_addr + i)) {
                 s = ERR_ERROR;
                 break;
             }
         }

         break;

     case CMD_IAP_END:
         Verity_Star_flag = 0;
         End_Flag = 1;
         s = ERR_End;
         break;

     default:
         s = ERR_ERROR;
         break;
     }

     return s;
 }

/*********************************************************************
 * @fn      EP2_RecData_Deal
 *
 * @brief   EP2 out date deal
 *
 * @return  none
 */
void EP2_IN_Tx_Deal(void)
{
    UserToPMABufferCopy(EP2_Tx_Buffer, ENDP2_TXADDR, EP2_Tx_Cnt);
    SetEPTxCount(ENDP2, EP2_Tx_Cnt);
    SetEPTxValid(ENDP2);
}

/*********************************************************************
 * @fn      EP2_RecData_Deal
 *
 * @brief   EP2 out date deal
 *
 * @return  none
 */
void EP2_RecData_Deal(void)
{
    u8 s;

    if(EP2_OUT_Flag){
        EP2_OUT_Flag = 0;
        EP2_Rx_Cnt = USB_SIL_Read(EP2_OUT, EP2_Rx_Buffer);

        s = RecData_Deal();

        if(s!=ERR_End){
            EP2_Tx_Buffer[0] = 0x00;
            if(s==ERR_ERROR) EP2_Tx_Buffer[1] = 0x01;    //err
            else EP2_Tx_Buffer[1] = 0x00;
            EP2_Tx_Cnt = 2;
            EP2_IN_Tx_Deal();
        }

        SetEPRxStatus(ENDP2, EP_RX_VALID);
    }

}

/*********************************************************************
 * @fn      GPIO_Cfg_init
 *
 * @brief   GPIO init
 *
 * @return  none
 */
void GPIO_Cfg_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

}

/*********************************************************************
 * @fn      PA0_Check
 *
 * @brief   Check PA0 state
 *
 * @return  1 - IAP
 *          0 - APP
 */
u8 PA0_Check(void)
{
    u8 i, cnt=0;

    GPIO_Cfg_init();

    for(i=0; i<10; i++){
        if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)==0) cnt++;
        Delay_Ms(5);
    }

    if(cnt>6) return 0;
    else return 1;
}

/*********************************************************************
 * @fn      USART1_CFG
 *
 * @brief   baudrate:UART1 baudrate
 *
 * @return  none
 */
void USART1_CFG(u32 baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    //PA9
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    //PA10
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =
    USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}
/*********************************************************************
 * @fn      UART1_SendMultiyData
 *
 * @brief   Deal device Endpoint 1 OUT.
 *
 * @param   l: Data length.
 *
 * @return  none
 */
void UART1_SendMultiyData(u8* pbuf, u8 num)
{
    u8 i = 0;

    while(i<num)
    {
        while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
        USART_SendData(USART1, pbuf[i]);
        i++;
    }
}
/*********************************************************************
 * @fn      UART1_SendMultiyData
 *
 * @brief   USART1 send date
 *
 * @param   pbuf - Packet to be sent
 *          num - the number of date
 *
 * @return  none
 */
void UART1_SendData(u8 data)
{
    while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
    USART_SendData(USART1, data);

}

/*********************************************************************
 * @fn      Uart1_Rx
 *
 * @brief   Uart1 receive date
 *
 * @return  none
 */
u8 Uart1_Rx(void)
{
    while( USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
    return USART_ReceiveData( USART1);
}

/*********************************************************************
 * @fn      UART_Rx_Deal
 *
 * @brief   UART Rx data deal
 *
 * @return  none
 */
void UART_Rx_Deal(void)
{
    u8 i, s;
    u8 Data_add = 0;

    if (Uart1_Rx() == Uart_Sync_Head1)
    {
        if (Uart1_Rx() == Uart_Sync_Head2)
        {
            isp_cmd_t->Cmd = Uart1_Rx();
            Data_add += isp_cmd_t->Cmd;
            isp_cmd_t->Len = Uart1_Rx();
            Data_add += isp_cmd_t->Len;
            isp_cmd_t->adr[0] = Uart1_Rx();
            Data_add += isp_cmd_t->adr[0];
            isp_cmd_t->adr[1] = Uart1_Rx();
            Data_add += isp_cmd_t->adr[1];

            if ((isp_cmd_t->Cmd == CMD_IAP_PROM) || (isp_cmd_t->Cmd == CMD_IAP_VERIFY))
            {
                for (i = 0; i < isp_cmd_t->Len; i++) {
                    isp_cmd_t->data[i] = Uart1_Rx();
                    Data_add += isp_cmd_t->data[i];
                }
            }

            if (Uart1_Rx() == Data_add)
            {
                s = RecData_Deal();

                if (s != ERR_End)
                {
                    UART1_SendData(0x00);
                    if (s == ERR_ERROR)
                    {
                        UART1_SendData(0x01);
                    }
                    else
                    {
                        UART1_SendData(0x00);
                    }
                }
            }
        }
    }
}




