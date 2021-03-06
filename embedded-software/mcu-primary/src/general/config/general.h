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
 * @file    general.h
 * @author  foxBMS Team
 * @date    25.02.2015 (date of creation)
 * @ingroup GENERAL_CONF
 * @prefix  none
 *
 * @brief   Settings for the system
 *
 * e.g., enable modules, set software version
 *
 */

#ifndef GENERAL_H_
#define GENERAL_H_

/*================== Includes =============================================*/
#include <stdint.h>
#include <std_types.h>

/*================== Macros and Definitions ===============================*/
#define FOXBMS_BOARD                      1

#define ITRI_MOD	// ITRI modification
#if defined(ITRI_MOD)
	#define ITRI_DBG_PRINT

	#define ITRI_MOD_1			// COM command
	#define ITRI_MOD_2			// LTC extend
	#define ITRI_MOD_3			// fixed foxBMS 1.0.1 bug

	#define ITRI_MOD_5			// expand module number to 25
	//#define ITRI_MOD_6		// modify LTC cmd/data transfer time
	//#define ITRI_MOD_6_a		// change cmdTransferTime of ADCV and ADAX  (6ms -> 3ms)
	//#define ITRI_MOD_6_b		// SPI CRC
	//#define ITRI_MOD_6_c		// ADC mode FAST
	//#define ITRI_MOD_6_d		// add dummy cmd for wake up
	//#define ITRI_MOD_6_e		// continuous to do ebm control
	#define ITRI_MOD_6_f		// refine LTC_SaveVoltages() timing
	//#define ITRI_MOD_6_g		// verify cell and GPIO voltages
	//#define ITRI_MOD_6_h
	#define ITRI_MOD_6_i		// LTC driver uses interrupt
	#define ITRI_MOD_8			// bypass reading temperature and UID
	#define	ITRI_MOD_9			// supporting SPM on/off
	//#define ITRI_MOD_9_a		// supporting async. EBM control cmd
	//#define ITRI_MOD_10			// change CAN baud rate 0.5M -> 1Mbps; NOTE: remember update RPi CAN bitrate simultaneously
	#define ITRI_MOD_11			// support heart beat
	#define ITRI_MOD_12			// support LED control
	#define ITRI_MOD_13			// monitor cell voltage safe operating limit (BC_VOLTMAX)

	//#define ITRI_EBM_CHROMA_V2	// disable:H(GPIO2)/H(GPIO4), bypass:L/H, enable:L/L
	#define ITRI_EBM_CHROMA_V3	// disable:H(GPIO2)/H(GPIO4), bypass:L/L, enable:L/H

//	#define ITRI_MOD_90			// [WARNING] LTC enables command mode
#endif

/**
 * @ingroup CONFIG_GENERAL
 * enables software safety features
 * \par Type:
 * select(2)
 * \par Default:
 * 1
*/
//  #define BUILD_MODULE_ENABLE_SAFETY_FEATURES 1
#define BUILD_MODULE_ENABLE_SAFETY_FEATURES 0


/**
 * @ingroup CONFIG_GENERAL
 * enables checking of flash checksum at startup.
 * \par Type:
 * select(2)
 * \par Default:
 * 0
*/
#define BUILD_MODULE_ENABLE_FLASHCHECKSUM           1
//  #define BUILD_MODULE_ENABLE_FLASHCHECKSUM           0


/**
 * @ingroup CONFIG_GENERAL
 * enables bootloader (wrapper) startup.
 * \par Type:
 * select(2)
 * \par Default:
 * 0
*/
#define BUILD_MODULE_ENABLE_BOOTLOADER              1
//  #define BUILD_MODULE_ENABLE_BOOTLOADER              0

/**
 * @ingroup CONFIG_GENERAL
 * enables Mini-USB: USART3 peripheral (serial interface)
 * \par Type:
 * select(2)
 * \par Default:
 * 0
*/
#define BUILD_MODULE_ENABLE_UART          1
//  #define BUILD_MODULE_ENABLE_UART          0

/**
 * @ingroup CONFIG_GENERAL
 * enables RS485: USART2 peripheral (serial interface)
 * \par Type:
 * select(2)
 * \par Default:
 * 0
*/
#define BUILD_MODULE_ENABLE_RS485           1
//  #define BUILD_MODULE_ENABLE_RS485          0

/**
 * @ingroup CONFIG_GENERAL
 * enables NVRAM
 * \par Type:
 * select(2)
 * \par Default:
 * 0
*/
#define BUILD_MODULE_ENABLE_NVRAM           1
//  #define BUILD_MODULE_ENABLE_NVRAM          0

/**
 * @ingroup CONFIG_GENERAL
 * enables COM module.
 * \par Type:
 * select(2)
 * \par Default:
 * 0
*/
#define BUILD_MODULE_ENABLE_COM           1
//  #define BUILD_MODULE_ENABLE_COM           0

/**
 * @ingroup CONFIG_GENERAL
 * enables printf debugging with serial interface
 * \par Type:
 * select(2)
 * \par Default:
 * 0
*/
#define BUILD_MODULE_DEBUGPRINTF          1
//  #define BUILD_MODULE_DEBUGPRINTF          0

/**
 * @ingroup CONFIG_GENERAL
 * enables RTC peripheral (Real Time Clock)
 * \par Type:
 * select(2)
 * \par Default:
 * 0
*/
#define BUILD_MODULE_ENABLE_RTC           1
//  #define BUILD_MODULE_ENABLE_RTC           0


/**
 * @ingroup CONFIG_GENERAL
 * enables MCU Watchdog
 * \par Type:
 * select(2)
 * \par Default:
 * 1
*/
#define BUILD_MODULE_ENABLE_WATCHDOG        1
//  #define BUILD_MODULE_ENABLE_WATCHDOG      0


//#define BUILD_MODULE_IMPORT_CELL_DATASHEET  1
#define BUILD_MODULE_IMPORT_CELL_DATASHEET  0

#define STR(TESTMACRO) #TESTMACRO
#define XSTR(TESTMACRO) STR(TESTMACRO)

#ifndef BUILD_VERSION
#define BUILD_VERSION        "    0.5"                /*strlen: 16 (15 + '/0') */
#endif

#ifndef BUILD_APPNAME
#define BUILD_APPNAME        "foxbms "                /*strlen: 16 (15 + '/0') */
#endif

#ifndef BUILD_VENDOR
#define BUILD_VENDOR         "Fraunhofer IISB"        /*strlen: 16 (15 + '/0') */
#endif

#ifndef BUILD_BL_MAJOR
#define BUILD_BL_MAJOR          0
#endif

#ifndef BUILD_BL_MINOR
#define BUILD_BL_MINOR          2
#endif

#ifndef BUILD_BL_BUGFIX
#define BUILD_BL_BUGFIX         0
#endif
//  #pragma message XSTR(BUILD_VERSION)
//  #pragma message XSTR(BUILD_APPNAME)
//  #pragma message XSTR(BUILD_VENDOR)

/**
 * A variable defined as ``(type) MEM_BKP_SRAM (name)`` will be stored in the
 * RAM which is backuped by a button cell. Therefore as long as the power
 * supply is not disconnected, the variable value will be stored during
 * e.g. restarts.
 */
#define MEM_BKP_SRAM    __attribute__((section (".BKP_RAMSection")))


/*================== Constant and Variable Definitions ====================*/


/*================== Function Prototypes ==================================*/


/*================== Function Implementations =============================*/


#endif /* GENERAL_H_ */
