/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0
* Date               : 2020/08/06
* Description        : 骑行传感器主函数及任务系统初始化

*******************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "CONFIG.h"
#include "HAL.h"
#include "cyclingservice.h"
#include "cyclingsensor.h"

/*********************************************************************
 * GLOBAL TYPEDEFS
 */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */
int main(void)
{
    Delay_Init();
#ifdef DEBUG
    USART_Printf_Init( 115200 );
#endif
    PRINT("%s\n", VER_LIB);
    WCHBLE_Init();
    HAL_Init();
    GAPRole_PeripheralInit();
    CyclingSensor_Init();
    while(1)
    {
        TMOS_SystemProcess();
    }
}

/******************************** endfile @ main ******************************/
