/**
 *
 * @copyright &copy; 2010 - 2017, Fraunhofer-Gesellschaft zur Foerderung der angewandten Forschung e.V. All rights reserved.
 *
 * BSD 3-Clause License
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 * 1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * We kindly request you to use one or more of the following phrases to refer to foxBMS in your hardware, software, documentation or advertising materials:
 *
 * &Prime;This product uses parts of foxBMS&reg;&Prime;
 *
 * &Prime;This product includes parts of foxBMS&reg;&Prime;
 *
 * &Prime;This product is derived from foxBMS&reg;&Prime;
 *
 */

/**
 * @file    can_cfg.c
 * @author  foxBMS Team
 * @date    12.07.2015 (date of creation)
 * @ingroup DRIVERS_CONF
 * @prefix  CAN
 *
 * @brief   Configuration for the CAN module
 *
 * The CAN bus settings and the received messages and their
 * reception handling are to be specified here.
 *
 */

/*================== Includes =============================================*/
/* recommended include order of header files:
 * 
 * 1.    include general.h
 * 2.    include module's own header
 * 3...  other headers
 *
 */
#include "general.h"
#include "can_cfg.h"
#include "rcc_cfg.h"
#include "mcu.h"

/*================== Macros and Definitions ===============================*/

/*================== Constant and Variable Definitions ====================*/

/*================== Function Prototypes ==================================*/

/*================== Function Implementations =============================*/

/* ***************************************
 *  Set CAN settings here
 ****************************************/

CAN_HandleTypeDef hcan0 = {
        .Instance = CAN2,
        .Lock = HAL_UNLOCKED,
        .State = HAL_CAN_STATE_RESET,
        .ErrorCode = HAL_CAN_ERROR_NONE,
#if (CAN_BAUDRATE == 1000000)
#if (RCC_APB1_CLOCK  ==  42000000)
        .Init.Prescaler = 3,        // CAN_CLOCK = APB1 = 42MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_6TQ,    // --> CAN = 42MHz/12/14 = 1.0MHz
        .Init.BS2 = CAN_BS2_7TQ,
#elif RCC_APB1_CLOCK  ==  32000000
        .Init.Prescaler = 4,        // CAN_CLOCK = APB1 = 32MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_5TQ,    // --> CAN = 32MHz/16/8 = 1.0MHz
        .Init.BS2 = CAN_BS2_2TQ,
#else
#error "Please configure CAN Baudrate according to your clock configuration "
#endif
#elif (CAN_BAUDRATE == 500000)
#if (RCC_APB1_CLOCK  ==  42000000)
        .Init.Prescaler = 6,        // CAN_CLOCK = APB1 = 42MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_6TQ,    // --> CAN = 42MHz/6/14 = 0.5MHz
        .Init.BS2 = CAN_BS2_7TQ,
#elif RCC_APB1_CLOCK  ==  32000000
        .Init.Prescaler = 8,        // CAN_CLOCK = APB1 = 32MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_5TQ,    // --> CAN = 32MHz/8/8 = 0.5MHz
        .Init.BS2 = CAN_BS2_2TQ,
#else
#error "Please configure CAN Baudrate according to your clock configuration "
#endif
#elif (CAN_BAUDRATE == 250000)
#if (RCC_APB1_CLOCK  ==  42000000)
        .Init.Prescaler = 12,        // CAN_CLOCK = APB1 = 42MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_6TQ,    // --> CAN = 42MHz/12/14 = 0.25MHz
        .Init.BS2 = CAN_BS2_7TQ,
#elif RCC_APB1_CLOCK  ==  32000000
        .Init.Prescaler = 16,        // CAN_CLOCK = APB1 = 32MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_5TQ,    // --> CAN = 32MHz/16/8 = 0.25MHz
        .Init.BS2 = CAN_BS2_2TQ,
#else
#error "Please configure CAN Baudrate according to your clock configuration "
#endif
#elif (CAN_BAUDRATE == 125000)
#if (RCC_APB1_CLOCK  ==  42000000)
        .Init.Prescaler = 24,        // CAN_CLOCK = APB1 = 42MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_6TQ,    // --> CAN = 42MHz/12/14 = 0.125MHz
        .Init.BS2 = CAN_BS2_7TQ,
#elif RCC_APB1_CLOCK  ==  32000000
        .Init.Prescaler = 32,        // CAN_CLOCK = APB1 = 32MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_5TQ,    // --> CAN = 32MHz/16/8 = 0.125MHz
        .Init.BS2 = CAN_BS2_2TQ,
#else
#error "Please configure CAN Baudrate according to your clock configuration "
#endif
#endif
        .Init.Mode = CAN_MODE_NORMAL,  // for test purpose, without connected can-bus use LOOPBACK mode
        .Init.SJW = CAN_SJW_1TQ,
        .Init.TTCM = DISABLE,   // time triggerd communication mode
                            // DISABLE: no influence
                            // ENABLE: saves timestamps for received and transmitted messages. See reference manual for more information.
        .Init.ABOM = ENABLE,    // automatic bus-off management
                            // DISABLE: Manually re-initialize CAN and wait for 128 * 11 recessive bits
                            // ENABLE: automatically leave bus-off mode after 128 * 11 recessive bits
        .Init.AWUM = ENABLE,    // automatic wake-up mode
                            // ENABLE: automatically leave sleep mode on message receiving
                            // DISABLE: SLEEP bit needs to be deleted by software
        .Init.NART = DISABLE,   // automatic retransition mode;
                            // DISABLE: retransmit the message until it has been successfully transmitted
                            // ENABLE: transmit only once, independently of transmission result
        .Init.RFLM = ENABLE,    // Receive FIFO locked against overrun.
                            // DISABLE: A new incoming message overwrites the last received message.
                            // ENABLE: Once a receive FIFO is full the next incoming message will be discarded.
        .Init.TXFP = DISABLE,   // Transmit FIFO priority
                            // DISABLE: driven by identifier of message. Lower identifier equals higher priority
                            // ENABLE: driven chronologically
};



CAN_HandleTypeDef hcan1 = {
        .Instance = CAN1,
        .Lock = HAL_UNLOCKED,
        .State = HAL_CAN_STATE_RESET,
        .ErrorCode = HAL_CAN_ERROR_NONE,
#if (CAN_BAUDRATE == 1000000)
#if (RCC_APB1_CLOCK  ==  42000000)
        .Init.Prescaler = 3,        // CAN_CLOCK = APB1 = 42MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_6TQ,    // --> CAN = 42MHz/3/14 = 1.0MHz
        .Init.BS2 = CAN_BS2_7TQ,
#elif RCC_APB1_CLOCK  ==  32000000
        .Init.Prescaler = 4,        // CAN_CLOCK = APB1 = 32MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_5TQ,    // --> CAN = 32MHz/4/8 = 1.0MHz
        .Init.BS2 = CAN_BS2_2TQ,
#else
#error "Please configure CAN Baudrate according to your clock configuration "
#endif
#elif (CAN_BAUDRATE == 500000)
#if (RCC_APB1_CLOCK  ==  42000000)
        .Init.Prescaler = 6,        // CAN_CLOCK = APB1 = 42MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_6TQ,    // --> CAN = 42MHz/6/14 = 0.5MHz
        .Init.BS2 = CAN_BS2_7TQ,
#elif RCC_APB1_CLOCK  ==  32000000
        .Init.Prescaler = 8,        // CAN_CLOCK = APB1 = 32MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_5TQ,    // --> CAN = 32MHz/8/8 = 0.5MHz
        .Init.BS2 = CAN_BS2_2TQ,
#else
#error "Please configure CAN Baudrate according to your clock configuration "
#endif
#elif (CAN_BAUDRATE == 250000)
#if (RCC_APB1_CLOCK  ==  42000000)
        .Init.Prescaler = 12,        // CAN_CLOCK = APB1 = 42MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_6TQ,    // --> CAN = 42MHz/12/14 = 0.25MHz
        .Init.BS2 = CAN_BS2_7TQ,
#elif RCC_APB1_CLOCK  ==  32000000
        .Init.Prescaler = 16,        // CAN_CLOCK = APB1 = 32MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_5TQ,    // --> CAN = 32MHz/16/8 = 0.25MHz
        .Init.BS2 = CAN_BS2_2TQ,
#else
#error "Please configure CAN Baudrate according to your clock configuration "
#endif
#elif (CAN_BAUDRATE == 125000)
#if (RCC_APB1_CLOCK  ==  42000000)
        .Init.Prescaler = 24,        // CAN_CLOCK = APB1 = 42MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_6TQ,    // --> CAN = 42MHz/12/14 = 0.125MHz
        .Init.BS2 = CAN_BS2_7TQ,
#elif RCC_APB1_CLOCK  ==  32000000
        .Init.Prescaler = 32,        // CAN_CLOCK = APB1 = 32MHz
                                    // resulting CAN speed: APB1/prescaler/sumOfTimequants
                                    // sum: 1tq for sync + BS1 + BS2
        .Init.BS1 = CAN_BS1_5TQ,    // --> CAN = 32MHz/16/8 = 0.125MHz
        .Init.BS2 = CAN_BS2_2TQ,
#else
#error "Please configure CAN Baudrate according to your clock configuration "
#endif
#endif
        .Init.Mode = CAN_MODE_NORMAL,    // for test purpose, without connected can-bus use LOOPBACK mode
        .Init.SJW = CAN_SJW_1TQ,
        .Init.TTCM = DISABLE,    // time triggerd communication mode
                            // DISABLE: no influence
                            // ENABLE: saves timestamps for received and transmitted messages. See reference manual for more information.
        .Init.ABOM = ENABLE,    // automatic bus-off management
                            // DISABLE: Manually re-initialize CAN and wait for 128 * 11 recessive bits
                            // ENABLE: automatically leave bus-off mode after 128 * 11 recessive bits
        .Init.AWUM = ENABLE,     // automatic wake-up mode
                            // ENABLE: automatically leave sleep mode on message receiving
                            // DISABLE: SLEEP bit needs to be deleted by software
        .Init.NART = DISABLE,     // automatic retransition mode;
                            // DISABLE: retransmit the message until it has been successfully transmitted
                            // ENABLE: transmit only once, independently of transmission result
        .Init.RFLM = ENABLE,    // Receive FIFO locked against overrun.
                            // DISABLE: A new incoming message overwrites the last received message.
                            // ENABLE: Once a receive FIFO is full the next incoming message will be discarded.
        .Init.TXFP = DISABLE,     // Transmit FIFO priority
                            // DISABLE: driven by identifier of message. Lower identifier equals higher priority
                            // ENABLE: driven chronologically
};



/* ***************************************
 *  Configure TX messages here
 ****************************************/
#if defined(ITRI_MOD_5)
#define CELL_REPETITION_TIME	400
#define CELL_REPETITION_OFFSET	10
#endif
const CAN_MSG_TX_TYPE_s can_CAN0_messages_tx[] = {

        { 0x110, 8, 100, 0, NULL_PTR },  //!< BMS system state 0
        { 0x111, 8, 100, 0, NULL_PTR },  //!< BMS system state 1
        { 0x112, 8, 100, 0, NULL_PTR },  //!< BMS system state 2

        { 0x115, 8, 100, 0, NULL_PTR },  //!< BMS slave state 0
        { 0x116, 8, 100, 0, NULL_PTR },  //!< BMS slave state 1

        { 0x130, 8, 100, 30, NULL_PTR },  //!< Maximum allowed current
        { 0x131, 8, 100, 30, NULL_PTR },  //!< SOP
        { 0x140, 8, 1000, 30, NULL_PTR },  //!< SOC
        { 0x150, 8, 5000, 30, NULL_PTR },  //!< SOH
        { 0x160, 8, 1000, 30, NULL_PTR },  //!< SOE
        { 0x170, 8, 100, 30, NULL_PTR },  //!< Cell voltages Min Max Average
        { 0x171, 8, 100, 30, NULL_PTR },  //!< SOV
        { 0x180, 8, 100, 30, NULL_PTR },  //!< Cell temperatures Min Max Average
        { 0x190, 8, 1000, 30, NULL_PTR },  //!< Tempering
        { 0x1A0, 8, 1000, 30, NULL_PTR },  //!< Insulation

        { 0x1D0, 8, 1000, 40, NULL_PTR },  //!< Running average power 0
        { 0x1D1, 8, 1000, 40, NULL_PTR },  //!< Running average power 1
        { 0x1D2, 8, 1000, 40, NULL_PTR },  //!< Running average power 2
        { 0x1E0, 8, 1000, 40, NULL_PTR },  //!< Running average current 0
        { 0x1E1, 8, 1000, 40, NULL_PTR },  //!< Running average current 1
        { 0x1E2, 8, 1000, 40, NULL_PTR },  //!< Running average current 2
#if defined(ITRI_MOD_5)
        { 0x200, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*0, NULL_PTR },  //!< Cell voltages module 0 cells 0 1 2
        { 0x201, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*0, NULL_PTR },  //!< Cell voltages module 0 cells 3 4 5
        { 0x202, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*0, NULL_PTR },  //!< Cell voltages module 0 cells 6 7 8
        { 0x203, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*0, NULL_PTR },  //!< Cell voltages module 0 cells 9 10 11

        { 0x210, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*0, NULL_PTR },  //!< Cell temperatures module 0 cells 0 1 2
        { 0x211, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*0, NULL_PTR },  //!< Cell temperatures module 0 cells 3 4 5
        { 0x212, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*0, NULL_PTR },  //!< Cell temperatures module 0 cells 6 7 8
        { 0x213, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*0, NULL_PTR },  //!< Cell temperatures module 0 cells 9 10 11

        { 0x220, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell voltages module 1 cells 0 1 2
        { 0x221, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell voltages module 1 cells 3 4 5
        { 0x222, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell voltages module 1 cells 6 7 8
        { 0x223, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell voltages module 1 cells 9 10 11

        { 0x230, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell temperatures module 1 cells 0 1 2
        { 0x231, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell temperatures module 1 cells 3 4 5
        { 0x232, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell temperatures module 1 cells 6 7 8
        { 0x233, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell temperatures module 1 cells 9 10 11

        { 0x240, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*2, NULL_PTR },  //!< Cell voltages module 2 cells 0 1 2
        { 0x241, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*2, NULL_PTR },  //!< Cell voltages module 2 cells 3 4 5
        { 0x242, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*2, NULL_PTR },  //!< Cell voltages module 2 cells 6 7 8
        { 0x243, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*2, NULL_PTR },  //!< Cell voltages module 2 cells 9 10 11

        { 0x250, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*2, NULL_PTR },  //!< Cell temperatures module 2 cells 0 1 2
        { 0x251, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*2, NULL_PTR },  //!< Cell temperatures module 2 cells 3 4 5
        { 0x252, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*2, NULL_PTR },  //!< Cell temperatures module 2 cells 6 7 8
        { 0x253, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*2, NULL_PTR },  //!< Cell temperatures module 2 cells 9 10 11

        { 0x260, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*3, NULL_PTR },  //!< Cell voltages module 3 cells 0 1 2
        { 0x261, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*3, NULL_PTR },  //!< Cell voltages module 3 cells 3 4 5
        { 0x262, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*3, NULL_PTR },  //!< Cell voltages module 3 cells 6 7 8
        { 0x263, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*3, NULL_PTR },  //!< Cell voltages module 3 cells 9 10 11

        { 0x270, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*3, NULL_PTR },  //!< Cell temperatures module 3 cells 0 1 2
        { 0x271, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*3, NULL_PTR },  //!< Cell temperatures module 3 cells 3 4 5
        { 0x272, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*3, NULL_PTR },  //!< Cell temperatures module 3 cells 6 7 8
        { 0x273, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*3, NULL_PTR },  //!< Cell temperatures module 3 cells 9 10 11

        { 0x280, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*4, NULL_PTR },  //!< Cell voltages module 4 cells 0 1 2
        { 0x281, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*4, NULL_PTR },  //!< Cell voltages module 4 cells 3 4 5
        { 0x282, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*4, NULL_PTR },  //!< Cell voltages module 4 cells 6 7 8
        { 0x283, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*4, NULL_PTR },  //!< Cell voltages module 4 cells 9 10 11

        { 0x290, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*4, NULL_PTR },  //!< Cell temperatures module 4 cells 0 1 2
        { 0x291, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*4, NULL_PTR },  //!< Cell temperatures module 4 cells 3 4 5
        { 0x292, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*4, NULL_PTR },  //!< Cell temperatures module 4 cells 6 7 8
        { 0x293, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*4, NULL_PTR },  //!< Cell temperatures module 4 cells 9 10 11

        { 0x2A0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*5, NULL_PTR },  //!< Cell voltages module 5 cells 0 1 2
        { 0x2A1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*5, NULL_PTR },  //!< Cell voltages module 5 cells 3 4 5
        { 0x2A2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*5, NULL_PTR },  //!< Cell voltages module 5 cells 6 7 8
        { 0x2A3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*5, NULL_PTR },  //!< Cell voltages module 5 cells 9 10 11

        { 0x2B0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*5, NULL_PTR },  //!< Cell temperatures module 5 cells 0 1 2
        { 0x2B1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*5, NULL_PTR },  //!< Cell temperatures module 5 cells 3 4 5
        { 0x2B2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*5, NULL_PTR },  //!< Cell temperatures module 5 cells 6 7 8
        { 0x2B3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*5, NULL_PTR },  //!< Cell temperatures module 5 cells 9 10 11

        { 0x2C0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*6, NULL_PTR },  //!< Cell voltages module 6 cells 0 1 2
        { 0x2C1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*6, NULL_PTR },  //!< Cell voltages module 6 cells 3 4 5
        { 0x2C2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*6, NULL_PTR },  //!< Cell voltages module 6 cells 6 7 8
        { 0x2C3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*6, NULL_PTR },  //!< Cell voltages module 6 cells 9 10 11

        { 0x2D0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*6, NULL_PTR },  //!< Cell temperatures module 6 cells 0 1 2
        { 0x2D1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*6, NULL_PTR },  //!< Cell temperatures module 6 cells 3 4 5
        { 0x2D2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*6, NULL_PTR },  //!< Cell temperatures module 6 cells 6 7 8
        { 0x2D3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*6, NULL_PTR },  //!< Cell temperatures module 6 cells 9 10 11

        { 0x2E0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*7, NULL_PTR },  //!< Cell voltages module 7 cells 0 1 2
        { 0x2E1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*7, NULL_PTR },  //!< Cell voltages module 7 cells 3 4 5
        { 0x2E2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*7, NULL_PTR },  //!< Cell voltages module 7 cells 6 7 8
        { 0x2E3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*7, NULL_PTR },  //!< Cell voltages module 7 cells 9 10 11

        { 0x2F0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*7, NULL_PTR },  //!< Cell temperatures module 7 cells 0 1 2
        { 0x2F1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*7, NULL_PTR },  //!< Cell temperatures module 7 cells 3 4 5
        { 0x2F2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*7, NULL_PTR },  //!< Cell temperatures module 7 cells 6 7 8
        { 0x2F3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*7, NULL_PTR },  //!< Cell temperatures module 7 cells 9 10 11
#else
        { 0x200, 8, 200, 20, NULL_PTR },  //!< Cell voltages module 0 cells 0 1 2
        { 0x201, 8, 200, 20, NULL_PTR },  //!< Cell voltages module 0 cells 3 4 5
        { 0x202, 8, 200, 20, NULL_PTR },  //!< Cell voltages module 0 cells 6 7 8
        { 0x203, 8, 200, 20, NULL_PTR },  //!< Cell voltages module 0 cells 9 10 11

        { 0x210, 8, 200, 30, NULL_PTR },  //!< Cell temperatures module 0 cells 0 1 2
        { 0x211, 8, 200, 30, NULL_PTR },  //!< Cell temperatures module 0 cells 3 4 5
        { 0x212, 8, 200, 30, NULL_PTR },  //!< Cell temperatures module 0 cells 6 7 8
        { 0x213, 8, 200, 30, NULL_PTR },  //!< Cell temperatures module 0 cells 9 10 11

        { 0x220, 8, 200, 40, NULL_PTR },  //!< Cell voltages module 1 cells 0 1 2
        { 0x221, 8, 200, 40, NULL_PTR },  //!< Cell voltages module 1 cells 3 4 5
        { 0x222, 8, 200, 40, NULL_PTR },  //!< Cell voltages module 1 cells 6 7 8
        { 0x223, 8, 200, 40, NULL_PTR },  //!< Cell voltages module 1 cells 9 10 11

        { 0x230, 8, 200, 50, NULL_PTR },  //!< Cell temperatures module 1 cells 0 1 2
        { 0x231, 8, 200, 50, NULL_PTR },  //!< Cell temperatures module 1 cells 3 4 5
        { 0x232, 8, 200, 50, NULL_PTR },  //!< Cell temperatures module 1 cells 6 7 8
        { 0x233, 8, 200, 50, NULL_PTR },  //!< Cell temperatures module 1 cells 9 10 11

        { 0x240, 8, 200, 60, NULL_PTR },  //!< Cell voltages module 2 cells 0 1 2
        { 0x241, 8, 200, 60, NULL_PTR },  //!< Cell voltages module 2 cells 3 4 5
        { 0x242, 8, 200, 60, NULL_PTR },  //!< Cell voltages module 2 cells 6 7 8
        { 0x243, 8, 200, 60, NULL_PTR },  //!< Cell voltages module 2 cells 9 10 11

        { 0x250, 8, 200, 70, NULL_PTR },  //!< Cell temperatures module 2 cells 0 1 2
        { 0x251, 8, 200, 70, NULL_PTR },  //!< Cell temperatures module 2 cells 3 4 5
        { 0x252, 8, 200, 70, NULL_PTR },  //!< Cell temperatures module 2 cells 6 7 8
        { 0x253, 8, 200, 70, NULL_PTR },  //!< Cell temperatures module 2 cells 9 10 11

        { 0x260, 8, 200, 80, NULL_PTR },  //!< Cell voltages module 3 cells 0 1 2
        { 0x261, 8, 200, 80, NULL_PTR },  //!< Cell voltages module 3 cells 3 4 5
        { 0x262, 8, 200, 80, NULL_PTR },  //!< Cell voltages module 3 cells 6 7 8
        { 0x263, 8, 200, 80, NULL_PTR },  //!< Cell voltages module 3 cells 9 10 11

        { 0x270, 8, 200, 90, NULL_PTR },  //!< Cell temperatures module 3 cells 0 1 2
        { 0x271, 8, 200, 90, NULL_PTR },  //!< Cell temperatures module 3 cells 3 4 5
        { 0x272, 8, 200, 90, NULL_PTR },  //!< Cell temperatures module 3 cells 6 7 8
        { 0x273, 8, 200, 90, NULL_PTR },  //!< Cell temperatures module 3 cells 9 10 11

        { 0x280, 8, 200, 100, NULL_PTR },  //!< Cell voltages module 4 cells 0 1 2
        { 0x281, 8, 200, 100, NULL_PTR },  //!< Cell voltages module 4 cells 3 4 5
        { 0x282, 8, 200, 100, NULL_PTR },  //!< Cell voltages module 4 cells 6 7 8
        { 0x283, 8, 200, 100, NULL_PTR },  //!< Cell voltages module 4 cells 9 10 11

        { 0x290, 8, 200, 110, NULL_PTR },  //!< Cell temperatures module 4 cells 0 1 2
        { 0x291, 8, 200, 110, NULL_PTR },  //!< Cell temperatures module 4 cells 3 4 5
        { 0x292, 8, 200, 110, NULL_PTR },  //!< Cell temperatures module 4 cells 6 7 8
        { 0x293, 8, 200, 110, NULL_PTR },  //!< Cell temperatures module 4 cells 9 10 11

        { 0x2A0, 8, 200, 120, NULL_PTR },  //!< Cell voltages module 5 cells 0 1 2
        { 0x2A1, 8, 200, 120, NULL_PTR },  //!< Cell voltages module 5 cells 3 4 5
        { 0x2A2, 8, 200, 120, NULL_PTR },  //!< Cell voltages module 5 cells 6 7 8
        { 0x2A3, 8, 200, 120, NULL_PTR },  //!< Cell voltages module 5 cells 9 10 11

        { 0x2B0, 8, 200, 130, NULL_PTR },  //!< Cell temperatures module 5 cells 0 1 2
        { 0x2B1, 8, 200, 130, NULL_PTR },  //!< Cell temperatures module 5 cells 3 4 5
        { 0x2B2, 8, 200, 130, NULL_PTR },  //!< Cell temperatures module 5 cells 6 7 8
        { 0x2B3, 8, 200, 130, NULL_PTR },  //!< Cell temperatures module 5 cells 9 10 11

        { 0x2C0, 8, 200, 140, NULL_PTR },  //!< Cell voltages module 6 cells 0 1 2
        { 0x2C1, 8, 200, 140, NULL_PTR },  //!< Cell voltages module 6 cells 3 4 5
        { 0x2C2, 8, 200, 140, NULL_PTR },  //!< Cell voltages module 6 cells 6 7 8
        { 0x2C3, 8, 200, 140, NULL_PTR },  //!< Cell voltages module 6 cells 9 10 11

        { 0x2D0, 8, 200, 150, NULL_PTR },  //!< Cell temperatures module 6 cells 0 1 2
        { 0x2D1, 8, 200, 150, NULL_PTR },  //!< Cell temperatures module 6 cells 3 4 5
        { 0x2D2, 8, 200, 150, NULL_PTR },  //!< Cell temperatures module 6 cells 6 7 8
        { 0x2D3, 8, 200, 150, NULL_PTR },  //!< Cell temperatures module 6 cells 9 10 11

        { 0x2E0, 8, 200, 160, NULL_PTR },  //!< Cell voltages module 7 cells 0 1 2
        { 0x2E1, 8, 200, 160, NULL_PTR },  //!< Cell voltages module 7 cells 3 4 5
        { 0x2E2, 8, 200, 160, NULL_PTR },  //!< Cell voltages module 7 cells 6 7 8
        { 0x2E3, 8, 200, 160, NULL_PTR },  //!< Cell voltages module 7 cells 9 10 11

        { 0x2F0, 8, 200, 170, NULL_PTR },  //!< Cell temperatures module 7 cells 0 1 2
        { 0x2F1, 8, 200, 170, NULL_PTR },  //!< Cell temperatures module 7 cells 3 4 5
        { 0x2F2, 8, 200, 170, NULL_PTR },  //!< Cell temperatures module 7 cells 6 7 8
        { 0x2F3, 8, 200, 170, NULL_PTR },  //!< Cell temperatures module 7 cells 9 10 11
#endif

#ifdef CAN_ISABELLENHUETTE_TRIGGERED
        , { 0x35B, 8, 100, 20, NULL_PTR }  //!< Current Sensor Trigger
#endif

#if defined(ITRI_MOD_5)
        { 0x400, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*8, NULL_PTR },  //!< Cell voltages module 8 cells 0 1 2
        { 0x401, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*8, NULL_PTR },
        { 0x402, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*8, NULL_PTR },
        { 0x403, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*8, NULL_PTR },

        { 0x410, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*8, NULL_PTR },  //!< Cell temperatures module 8 cells 0 1 2
        { 0x411, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*8, NULL_PTR },
        { 0x412, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*8, NULL_PTR },
        { 0x413, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*8, NULL_PTR },

		{ 0x420, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*9, NULL_PTR },  //!< Cell voltages module 9 cells 0 1 2
		{ 0x421, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*9, NULL_PTR },
		{ 0x422, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*9, NULL_PTR },
		{ 0x423, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*9, NULL_PTR },

		{ 0x430, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*9, NULL_PTR },  //!< Cell temperatures module 9 cells 0 1 2
		{ 0x431, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*9, NULL_PTR },
		{ 0x432, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*9, NULL_PTR },
		{ 0x433, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*9, NULL_PTR },

		{ 0x440, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*10, NULL_PTR },  //!< Cell voltages module 10 cells 0 1 2
		{ 0x441, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*10, NULL_PTR },
		{ 0x442, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*10, NULL_PTR },
		{ 0x443, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*10, NULL_PTR },

		{ 0x450, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*10, NULL_PTR },  //!< Cell temperatures module 10 cells 0 1 2
		{ 0x451, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*10, NULL_PTR },
		{ 0x452, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*10, NULL_PTR },
		{ 0x453, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*10, NULL_PTR },

		{ 0x460, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*11, NULL_PTR },  //!< Cell voltages module 11 cells 0 1 2
		{ 0x461, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*11, NULL_PTR },
		{ 0x462, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*11, NULL_PTR },
		{ 0x463, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*11, NULL_PTR },

		{ 0x470, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*11, NULL_PTR },  //!< Cell temperatures module 11 cells 0 1 2
		{ 0x471, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*11, NULL_PTR },
		{ 0x472, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*11, NULL_PTR },
		{ 0x473, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*11, NULL_PTR },

		{ 0x480, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*12, NULL_PTR },  //!< Cell voltages module 12 cells 0 1 2
		{ 0x481, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*12, NULL_PTR },
		{ 0x482, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*12, NULL_PTR },
		{ 0x483, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*12, NULL_PTR },

		{ 0x490, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell temperatures module 12 cells 0 1 2
		{ 0x491, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },
		{ 0x492, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },
		{ 0x493, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },

		{ 0x4A0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*13, NULL_PTR },  //!< Cell voltages module 13 cells 0 1 2
		{ 0x4A1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*13, NULL_PTR },
		{ 0x4A2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*13, NULL_PTR },
		{ 0x4A3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*13, NULL_PTR },

		{ 0x4B0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },  //!< Cell temperatures module 13 cells 0 1 2
		{ 0x4B1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },
		{ 0x4B2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },
		{ 0x4B3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*1, NULL_PTR },

		{ 0x4C0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*14, NULL_PTR },  //!< Cell voltages module 14 cells 0 1 2
		{ 0x4C1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*14, NULL_PTR },
		{ 0x4C2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*14, NULL_PTR },
		{ 0x4C3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*14, NULL_PTR },

		{ 0x4D0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*14, NULL_PTR },  //!< Cell temperatures module 14 cells 0 1 2
		{ 0x4D1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*14, NULL_PTR },
		{ 0x4D2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*14, NULL_PTR },
		{ 0x4D3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*14, NULL_PTR },

		{ 0x4E0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*15, NULL_PTR },  //!< Cell voltages module 15 cells 0 1 2
		{ 0x4E1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*15, NULL_PTR },
		{ 0x4E2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*15, NULL_PTR },
		{ 0x4E3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*15, NULL_PTR },

		{ 0x4F0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*15, NULL_PTR },  //!< Cell temperatures module 15 cells 0 1 2
		{ 0x4F1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*15, NULL_PTR },
		{ 0x4F2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*15, NULL_PTR },
		{ 0x4F3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*15, NULL_PTR },

		{ 0x500, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*16, NULL_PTR },  //!< Cell voltages module 16 cells 0 1 2
		{ 0x501, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*16, NULL_PTR },
		{ 0x502, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*16, NULL_PTR },
		{ 0x503, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*16, NULL_PTR },

		{ 0x510, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*16, NULL_PTR },  //!< Cell temperatures module 16 cells 0 1 2
		{ 0x511, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*16, NULL_PTR },
		{ 0x512, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*16, NULL_PTR },
		{ 0x513, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*16, NULL_PTR },

		{ 0x520, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*17, NULL_PTR },  //!< Cell voltages module 17 cells 0 1 2
		{ 0x521, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*17, NULL_PTR },
		{ 0x522, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*17, NULL_PTR },
		{ 0x523, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*17, NULL_PTR },

		{ 0x530, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*17, NULL_PTR },  //!< Cell temperatures module 17 cells 0 1 2
		{ 0x531, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*17, NULL_PTR },
		{ 0x532, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*17, NULL_PTR },
		{ 0x533, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*17, NULL_PTR },

		{ 0x540, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*18, NULL_PTR },  //!< Cell voltages module 18 cells 0 1 2
		{ 0x541, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*18, NULL_PTR },
		{ 0x542, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*18, NULL_PTR },
		{ 0x543, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*18, NULL_PTR },

		{ 0x550, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*18, NULL_PTR },  //!< Cell temperatures module 18 cells 0 1 2
		{ 0x551, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*18, NULL_PTR },
		{ 0x552, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*18, NULL_PTR },
		{ 0x553, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*18, NULL_PTR },

		{ 0x560, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*19, NULL_PTR },  //!< Cell voltages module 19 cells 0 1 2
		{ 0x561, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*19, NULL_PTR },
		{ 0x562, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*19, NULL_PTR },
		{ 0x563, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*19, NULL_PTR },

		{ 0x570, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*19, NULL_PTR },  //!< Cell temperatures module 19 cells 0 1 2
		{ 0x571, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*19, NULL_PTR },
		{ 0x572, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*19, NULL_PTR },
		{ 0x573, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*19, NULL_PTR },

		{ 0x580, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*20, NULL_PTR },  //!< Cell voltages module 20 cells 0 1 2
		{ 0x581, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*20, NULL_PTR },
		{ 0x582, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*20, NULL_PTR },
		{ 0x583, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*20, NULL_PTR },

		{ 0x590, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*20, NULL_PTR },  //!< Cell temperatures module 20 cells 0 1 2
		{ 0x591, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*20, NULL_PTR },
		{ 0x592, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*20, NULL_PTR },
		{ 0x593, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*20, NULL_PTR },

		{ 0x5A0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*21, NULL_PTR },  //!< Cell voltages module 21 cells 0 1 2
		{ 0x5A1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*21, NULL_PTR },
		{ 0x5A2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*21, NULL_PTR },
		{ 0x5A3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*21, NULL_PTR },

		{ 0x5B0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*21, NULL_PTR },  //!< Cell temperatures module 21 cells 0 1 2
		{ 0x5B1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*21, NULL_PTR },
		{ 0x5B2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*21, NULL_PTR },
		{ 0x5B3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*21, NULL_PTR },

		{ 0x5C0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*22, NULL_PTR },  //!< Cell voltages module 22 cells 0 1 2
		{ 0x5C1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*22, NULL_PTR },
		{ 0x5C2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*22, NULL_PTR },
		{ 0x5C3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*22, NULL_PTR },

		{ 0x5D0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*22, NULL_PTR },  //!< Cell temperatures module 22 cells 0 1 2
		{ 0x5D1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*22, NULL_PTR },
		{ 0x5D2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*22, NULL_PTR },
		{ 0x5D3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*22, NULL_PTR },

		{ 0x5E0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*23, NULL_PTR },  //!< Cell voltages module 23 cells 0 1 2
		{ 0x5E1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*23, NULL_PTR },
		{ 0x5E2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*23, NULL_PTR },
		{ 0x5E3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*23, NULL_PTR },

		{ 0x5F0, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*23, NULL_PTR },  //!< Cell temperatures module 23 cells 0 1 2
		{ 0x5F1, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*23, NULL_PTR },
		{ 0x5F2, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*23, NULL_PTR },
		{ 0x5F3, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*23, NULL_PTR },

		{ 0x600, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*24, NULL_PTR },  //!< Cell voltages module 24 cells 0 1 2
		{ 0x601, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*24, NULL_PTR },
		{ 0x602, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*24, NULL_PTR },
		{ 0x603, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*24, NULL_PTR },

		{ 0x610, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*24, NULL_PTR },  //!< Cell temperatures module 24 cells 0 1 2
		{ 0x611, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*24, NULL_PTR },
		{ 0x612, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*24, NULL_PTR },
		{ 0x613, 8, CELL_REPETITION_TIME, CELL_REPETITION_OFFSET*24, NULL_PTR },
#endif
};


const CAN_MSG_TX_TYPE_s can_CAN1_messages_tx[] = {

};

const uint8_t can_CAN0_tx_length = sizeof(can_CAN0_messages_tx)/sizeof(can_CAN0_messages_tx[0]);
const uint8_t can_CAN1_tx_length = sizeof(can_CAN1_messages_tx)/sizeof(can_CAN1_messages_tx[0]);

/* ***************************************
 *  Configure RX messages here
 ****************************************/

/* Bypassed messages are --- ALSO --- to be configured here. See further down for bypass ID setting!  */
CAN_MSG_RX_TYPE_s can0_RxMsgs[] = {
        { 0x120, 0xFFFF, 8, 0, CAN_FIFO0, NULL },   /*!< state request      */

        { CAN_ID_SOFTWARE_RESET_MSG, 0xFFFF, 8, 0, CAN_FIFO0, NULL },   /*!< software reset     */
        { CAN_ID_BOOTLOADER_MSG, 0x0000, 8, 0, CAN_FIFO0, NULL }, /*!< jump into BL and download new SW */
        { CAN_ID_NETWORK_NODE_ID, 0x0000, 8, 0, CAN_FIFO0, NULL }, /*!< write or read network node ID */

#ifdef CAN_ISABELLENHUETTE_TRIGGERED
        { 0x35C, 0xFFFF, 8, 0, CAN_FIFO0, NULL },   /*!< current sensor I   */
        { 0x35D, 0xFFFF, 8, 0, CAN_FIFO0, NULL },   /*!< current sensor U1  */
        { 0x35E, 0xFFFF, 8, 0, CAN_FIFO0, NULL },   /*!< current sensor U2  */
        { 0x35F, 0xFFFF, 8, 0, CAN_FIFO0, NULL },   /*!< current sensor U3  */
        { 0x525, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor T in cyclic mode  */
        { 0x526, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor Power in cyclic mode  */
        { 0x527, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor C-C in cyclic mode  */
        { 0x528, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor E-C in cyclic mode  */
#else
        { 0x521, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor I in cyclic mode   */
        { 0x522, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor U1 in cyclic mode  */
        { 0x523, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor U2 in cyclic mode  */
        { 0x524, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor U3 in cyclic mode  */
        { 0x525, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor T in cyclic mode  */
        { 0x526, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor Power in cyclic mode  */
        { 0x527, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor C-C in cyclic mode  */
        { 0x528, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< current sensor E-C in cyclic mode  */
#endif
        { 0x100, 0xFFFF, 8, 0, CAN_FIFO0, NULL },    /*!< debug message      */
};


CAN_MSG_RX_TYPE_s can1_RxMsgs[] = {

};


const uint8_t can_CAN0_rx_length = sizeof(can0_RxMsgs)/sizeof(can0_RxMsgs[0]);
const uint8_t can_CAN1_rx_length = sizeof(can1_RxMsgs)/sizeof(can1_RxMsgs[0]);

/* ***************************************
 *  Set bypass message IDs here
 ****************************************/

/* These IDs have to be included in the configuration for the filters in can_RxMsgs[]! */
uint32_t can0_bufferBypass_RxMsgs[CAN0_BUFFER_BYPASS_NUMBER_OF_IDs] = {CAN_ID_SOFTWARE_RESET_MSG,
                                                                       CAN_ID_NETWORK_NODE_ID,
                                                                       CAN_ID_BOOTLOADER_MSG};

uint32_t can1_bufferBypass_RxMsgs[CAN1_BUFFER_BYPASS_NUMBER_OF_IDs] = {CAN_ID_SOFTWARE_RESET_MSG,
                                                                       CAN_ID_NETWORK_NODE_ID,
                                                                       CAN_ID_BOOTLOADER_MSG};


STD_RETURN_TYPE_e CAN_CheckNodeID(uint8_t *dataptr) {
    STD_RETURN_TYPE_e ret_val;

    if (*(uint16_t*)(dataptr) == eepr_board_info.network_nodeID)
        ret_val = E_OK;
    else
        ret_val = E_NOT_OK;

    return ret_val;
}

STD_RETURN_TYPE_e CAN_CheckBroadcastID(uint8_t *dataptr) {
    STD_RETURN_TYPE_e ret_val;

    if (*(uint16_t*)(dataptr) == 0x0000)
        ret_val = E_OK;
    else
        ret_val = E_NOT_OK;

    return ret_val;
}

STD_RETURN_TYPE_e CAN_CheckUniqueDeviceID(uint8_t *dataptr) {
    STD_RETURN_TYPE_e ret_val;

    if (*(uint32_t*)(dataptr) == mcu_unique_deviceID.crc)
        ret_val = E_OK;
    else
        ret_val = E_NOT_OK;

    return ret_val;

}

