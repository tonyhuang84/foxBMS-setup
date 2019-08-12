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
 * @file    ltc.c
 * @author  foxBMS Team
 * @date    01.09.2015 (date of creation)
 * @ingroup DRIVERS
 * @prefix  LTC
 *
 * @brief   Driver for the LTC monitoring chip.
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
#include "database.h"

#include "ltc.h"
#include "mcu.h"
#include "diag.h"
#include "os.h"
#include "ltc_pec.h"
#include "uart.h"

#if defined(ITRI_MOD_13)
	#include "batterycell_cfg.h"
#endif

/*================== Macros and Definitions ===============================*/
// LTC COMM definitions

#define LTC_ICOM_START              0x60
#define LTC_ICOM_STOP               0x10
#define LTC_ICOM_BLANK              0x00
#define LTC_ICOM_NO_TRANSMIT        0x70
#define LTC_FCOM_MASTER_ACK         0x00
#define LTC_FCOM_MASTER_NACK        0x08
#define LTC_FCOM_MASTER_NACK_STOP   0x09

/**
 * Saves the last state and the last substate
 */
#define LTC_SAVELASTSTATES()    ltc_state.laststate=ltc_state.state; \
                                ltc_state.lastsubstate = ltc_state.substate

/*================== Constant and Variable Definitions ====================*/
static uint32_t ltc_task_1ms_cnt = 0;
static uint8_t ltc_taskcycle = 0;

static DATA_BLOCK_CELLVOLTAGE_s ltc_cellvoltage;
static DATA_BLOCK_CELLTEMPERATURE_s ltc_celltemperature;
static DATA_BLOCK_MINMAX_s ltc_minmax;
static DATA_BLOCK_BALANCING_FEEDBACK_s ltc_balancing_feedback;
static DATA_BLOCK_USER_MUX_s ltc_user_mux;
static DATA_BLOCK_BALANCING_CONTROL_s ltc_balancing_control;
//static DATA_BLOCK_BALANCING_CONTROL_s ltc_balancing_control_backup;
static DATA_BLOCK_USER_IO_CONTROL_s ltc_user_io_control;


static LTC_ERRORTABLE_s LTC_ErrorTable[BS_NR_OF_MODULES];  // init in LTC_ResetErrorTable-function


static LTC_STATE_s ltc_state = {
    .timer                   = 0,
    .statereq                = LTC_STATE_NO_REQUEST,
    .state                   = LTC_STATEMACH_UNINITIALIZED,
    .substate                = 0,
    .laststate               = LTC_STATEMACH_UNINITIALIZED,
    .lastsubstate            = 0,
    .adcModereq              = LTC_ADCMODE_FAST_DCP0,
    .adcMode                 = LTC_ADCMODE_FAST_DCP0,
    .adcMeasChreq            = LTC_ADCMEAS_UNDEFINED,
    .adcMeasCh               = LTC_ADCMEAS_UNDEFINED,
    .numberOfMeasuredMux     = 32,
    .triggerentry            = 0,
    .ErrRetryCounter         = 0,
    .ErrRequestCounter       = 0,
    .VoltageSampleTime       = 0,
    .muxSampleTime           = 0,
#if defined(ITRI_MOD_6)
	.commandDataTransferTime = 3,
	.commandTransferTime     = 3,
#else
    .commandDataTransferTime = 3,
    .commandTransferTime     = 3,
#endif
    .gpioClocksTransferTime  = 3,
    .muxmeas_seqptr          = NULL_PTR,
    .muxmeas_seqendptr       = NULL_PTR,
    .muxmeas_nr_end          = 0,
    .first_measurement_made  = FALSE,
    .ltc_muxcycle_finished   = E_NOT_OK,
#if defined(ITRI_MOD_6_i)
	.check_spi_flag          = 0,
#endif
};

static const uint8_t ltc_cmdDummy[1]={0x00};
static const uint8_t ltc_cmdWRCFG[4]={0x00, 0x01, 0x3D, 0x6E};
static const uint8_t ltc_cmdWRCFG2[4]={0x00, 0x24, 0xB1, 0x9E};

static const uint8_t ltc_cmdRDCVA[4] = {0x00, 0x04, 0x07, 0xC2};
static const uint8_t ltc_cmdRDCVB[4] = {0x00, 0x06, 0x9A, 0x94};
static const uint8_t ltc_cmdRDCVC[4] = {0x00, 0x08, 0x5E, 0x52};
static const uint8_t ltc_cmdRDCVD[4] = {0x00, 0x0A, 0xC3, 0x04};
static const uint8_t ltc_cmdRDCVE[4] = {0x00, 0x09, 0xD5, 0x60};
static const uint8_t ltc_cmdRDCVF[4] = {0x00, 0x0B, 0x48, 0x36};
static const uint8_t ltc_cmdWRCOMM[4] = {0x07, 0x21, 0x24, 0xB2};
static const uint8_t ltc_cmdSTCOMM[4] = {0x07, 0x23, 0xB9, 0xE4};
static const uint8_t ltc_cmdRDCOMM[4] = {0x07, 0x22, 0x32, 0xD6};
static const uint8_t ltc_cmdRDAUXA[4] = {0x00, 0x0C, 0xEF, 0xCC};
static const uint8_t ltc_cmdRDAUXB[4] = {0x00, 0x0E, 0x72, 0x9A};
static const uint8_t ltc_cmdRDAUXC[4] = {0x00, 0x0D, 0x64, 0xFE};
static const uint8_t ltc_cmdRDAUXD[4] = {0x00, 0x0F, 0xF9, 0xA8};

//static const uint8_t ltc_cmdMUTE[4] = {0x00, 0x28, 0xE8, 0x0E};                    /*!< MUTE discharging via S pins */
//static const uint8_t ltc_cmdUNMUTE[4] = {0x00, 0x29, 0x63, 0x3C};                  /*!< UN-MUTE discharging via S pins */

/* LTC I2C commands */
//static const uint8_t ltc_I2CcmdDummy[6] = {0x7F, 0xF9, 0x7F, 0xF9, 0x7F, 0xF9};      /*!< dummy command (no transmit) */

static const uint8_t ltc_I2CcmdTempSens0[6] = {0x69, 0x08, 0x00, 0x09, 0x7F, 0xF9};  /*!< sets the internal data pointer of the temperature sensor (address 0x48) to 0x00 */
static const uint8_t ltc_I2CcmdTempSens1[6] = {0x69, 0x18, 0x0F, 0xF0, 0x0F, 0xF9};  /*!< reads two data bytes from the temperature sensor */

static const uint8_t ltc_I2CcmdEEPROM0[6] = {0x6A, 0x08, 0x0F, 0xA8, 0x6A, 0x18};    /*!< sets the internal data pointer of the EEPROM (address 0x50) to 0xFA and initiates a read command */
static const uint8_t ltc_I2CcmdEEPROM1[6] = {0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0};    /*!< reads three data bytes from the EEPROM, master sends ACK */
static const uint8_t ltc_I2CcmdEEPROM2[6] = {0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF9};    /*!< reads three data bytes from the EEPROM, master sends ACK, last byte NACK+STOP */

//static const uint8_t ltc_I2CcmdPortExpander0[6] = {0x64, 0x08, 0x0F, 0xF9, 0x7F, 0xF9};  /*!< writes one data byte (0xFF) to the port expander (address 0x20)*/
static const uint8_t ltc_I2CcmdPortExpander1[6] = {0x64, 0x18, 0x0F, 0xF9, 0x7F, 0xF9};  /*!< reads one data byte from the port expander */

/* Cells */
static const uint8_t ltc_cmdADCV_normal_DCP0[4] = {0x03, 0x60, 0xF4, 0x6C};        /*!< All cells, normal mode, discharge not permitted (DCP=0)    */
static const uint8_t ltc_cmdADCV_normal_DCP1[4] = {0x03, 0x70, 0xAF, 0x42};        /*!< All cells, normal mode, discharge permitted (DCP=1)        */
static const uint8_t ltc_cmdADCV_filtered_DCP0[4] = {0x03, 0xE0, 0xB0, 0x4A};      /*!< All cells, filtered mode, discharge not permitted (DCP=0)  */
static const uint8_t ltc_cmdADCV_filtered_DCP1[4] = {0x03, 0xF0, 0xEB, 0x64};      /*!< All cells, filtered mode, discharge permitted (DCP=1)      */
static const uint8_t ltc_cmdADCV_fast_DCP0[4] = {0x02, 0xE0, 0x38, 0x06};          /*!< All cells, fast mode, discharge not permitted (DCP=0)      */
static const uint8_t ltc_cmdADCV_fast_DCP1[4] = {0x02, 0xF0, 0x63, 0x28};          /*!< All cells, fast mode, discharge permitted (DCP=1)          */
static const uint8_t ltc_cmdADCV_fast_DCP0_twocells[4] = {0x02, 0xE1, 0xb3, 0x34}; /*!< Two cells (1 and 7), fast mode, discharge not permitted (DCP=0) */

/* GPIOs  */
static const uint8_t ltc_cmdADAX_normal_GPIO1[4] = {0x05, 0x61, 0x58, 0x92};      /*!< Single channel, GPIO 1, normal mode   */
static const uint8_t ltc_cmdADAX_filtered_GPIO1[4] = {0x05, 0xE1, 0x1C, 0xB4};    /*!< Single channel, GPIO 1, filtered mode */
static const uint8_t ltc_cmdADAX_fast_GPIO1[4] = {0x04, 0xE1, 0x94, 0xF8};        /*!< Single channel, GPIO 1, fast mode     */
static const uint8_t ltc_cmdADAX_normal_GPIO2[4] = {0x05, 0x62, 0x4E, 0xF6};      /*!< Single channel, GPIO 2, normal mode   */
static const uint8_t ltc_cmdADAX_filtered_GPIO2[4] = {0x05, 0xE2, 0x0A, 0xD0};    /*!< Single channel, GPIO 2, filtered mode */
static const uint8_t ltc_cmdADAX_fast_GPIO2[4] = {0x04, 0xE2, 0x82, 0x9C};        /*!< Single channel, GPIO 2, fast mode     */
static const uint8_t ltc_cmdADAX_normal_GPIO3[4] = {0x05, 0x63, 0xC5, 0xC4};      /*!< Single channel, GPIO 3, normal mode   */
static const uint8_t ltc_cmdADAX_filtered_GPIO3[4] = {0x05, 0xE3, 0x81, 0xE2};    /*!< Single channel, GPIO 3, filtered mode */
static const uint8_t ltc_cmdADAX_fast_GPIO3[4] = {0x04, 0xE3, 0x09, 0xAE};        /*!< Single channel, GPIO 3, fast mode     */
//static const uint8_t ltc_cmdADAX_normal_GPIO4[4] = {0x05, 0x64, 0x62, 0x3E};      /*!< Single channel, GPIO 4, normal mode   */
//static const uint8_t ltc_cmdADAX_filtered_GPIO4[4] = {0x05, 0xE4, 0x26, 0x18};    /*!< Single channel, GPIO 4, filtered mode */
//static const uint8_t ltc_cmdADAX_fast_GPIO4[4] = {0x04, 0xE4, 0xAE, 0x54};        /*!< Single channel, GPIO 4, fast mode     */
//static const uint8_t ltc_cmdADAX_normal_GPIO5[4] = {0x05, 0x65, 0xE9, 0x0C};      /*!< Single channel, GPIO 5, normal mode   */
//static const uint8_t ltc_cmdADAX_filtered_GPIO5[4] = {0x05, 0xE5, 0xAD, 0x2A};    /*!< Single channel, GPIO 5, filtered mode */
//static const uint8_t ltc_cmdADAX_fast_GPIO5[4] = {0x04, 0xE5, 0x25, 0x66};        /*!< Single channel, GPIO 5, fast mode     */
static const uint8_t ltc_cmdADAX_normal_ALLGPIOS[4] = {0x05, 0x60, 0xD3, 0xA0};   /*!< All channels, normal mode             */
static const uint8_t ltc_cmdADAX_filtered_ALLGPIOS[4] = {0x05, 0xE0, 0x97, 0x86}; /*!< All channels, filtered mode           */
static const uint8_t ltc_cmdADAX_fast_ALLGPIOS[4] = {0x04, 0xE0, 0x1F, 0xCA};     /*!< All channels, fast mode               */

static uint8_t ltc_tmpTXbuffer[6*LTC_N_LTC];

static uint8_t ltc_DataBufferSPI_TX_with_PEC_init[LTC_N_BYTES_FOR_DATA_TRANSMISSION];

static uint8_t ltc_tmpTXPECbuffer[LTC_N_BYTES_FOR_DATA_TRANSMISSION];
static uint8_t ltc_DataBufferSPI_RX_with_PEC_voltages[LTC_N_BYTES_FOR_DATA_TRANSMISSION];
static uint8_t LTC_CellVoltages[LTC_N_LTC*2*BS_NR_OF_BAT_CELLS_PER_MODULE];
//static uint8_t LTC_MultiplexerVoltages[LTC_N_LTC*2*4*8];
static uint8_t LTC_MultiplexerVoltages[LTC_N_LTC*2*LTC_N_MUX_CHANNELS_PER_LTC];

#if BS_NR_OF_BAT_CELLS_PER_MODULE > 12
static uint8_t LTC_GPIOVoltages[LTC_N_LTC*4*6];
#else
static uint8_t LTC_GPIOVoltages[LTC_N_LTC*2*6];
#endif

static uint32_t LTC_EEPROM_UIDs[LTC_N_LTC];
static uint8_t LTC_EXT_TEMP_SENS[LTC_N_LTC];    /* unit: �C */

#if BS_NR_OF_BAT_CELLS_PER_MODULE > 12
static uint16_t LTC_allGPIOVoltages[LTC_N_LTC*12];
#else
static uint16_t LTC_allGPIOVoltages[LTC_N_LTC*6];
#endif

static uint8_t ltc_DataBufferSPI_TX_temperatures[6*LTC_N_LTC];

static uint8_t ltc_DataBufferSPI_TX_user_io[6*LTC_N_LTC];
static uint8_t ltc_DataBufferSPI_TX_eeprom[6*LTC_N_LTC];
static uint8_t ltc_DataBufferSPI_TX_tempsens[6*LTC_N_LTC];

static uint8_t ltc_DataBufferSPI_TX_ClockCycles[4+9];
static uint8_t ltc_DataBufferSPI_TX_ClockCycles_with_PEC[4+9];

static uint8_t ltc_DataBufferSPI_TX_with_PEC_temperatures[LTC_N_BYTES_FOR_DATA_TRANSMISSION];
static uint8_t ltc_DataBufferSPI_RX_with_PEC_temperatures[LTC_N_BYTES_FOR_DATA_TRANSMISSION];

static uint8_t ltc_DataBufferSPI_TX_with_PEC_user_io[LTC_N_BYTES_FOR_DATA_TRANSMISSION];
static uint8_t ltc_DataBufferSPI_RX_with_PEC_user_io[LTC_N_BYTES_FOR_DATA_TRANSMISSION];

static uint8_t ltc_DataBufferSPI_TX_with_PEC_eeprom[LTC_N_BYTES_FOR_DATA_TRANSMISSION];
static uint8_t ltc_DataBufferSPI_RX_with_PEC_eeprom[LTC_N_BYTES_FOR_DATA_TRANSMISSION];

static uint8_t ltc_DataBufferSPI_TX_with_PEC_tempsens[LTC_N_BYTES_FOR_DATA_TRANSMISSION];
static uint8_t ltc_DataBufferSPI_RX_with_PEC_tempsens[LTC_N_BYTES_FOR_DATA_TRANSMISSION];

#if defined(ITRI_MOD_2)
/*
typedef enum {
	LTC_EBM_NONE,
	LTC_EBM_EB_CTRL,		// enable/bypass control
} LTC_EBM_CMD_s;
*/
LTC_EBM_CMD_s ltc_ebm_cmd = LTC_EBM_NONE;

typedef struct {
	uint8_t eb_state;		// 0:bypass, 1: enable, 2:disable(open)
} LTC_EBM_CONFIG_s;
LTC_EBM_CONFIG_s ltc_ebm_config[BS_NR_OF_MODULES];
#if defined(ITRI_MOD_9)
	LTC_EBM_CONFIG_s ltc_col_config[BS_NR_OF_COLUMNS];
#endif
#if defined(ITRI_MOD_12)
	LTC_EBM_CONFIG_s ltc_led_config[BS_NR_OF_LEDS];
#endif

#define LTC_EBM_MAX_CURR_CAL_CNT	(60)
#define LTC_EBM_CURR_ZERO_BASE		(25000)
#define LTC_EBM_BAT_CURR_IDX		(4)
#define LTC_EBM_MOD_CURR_IDX		(5)
typedef struct {
	uint8_t		isCali;				// 0: means calibrated
	int16_t		curBat_offset;
	int16_t		curMod_offset;
} LTC_EBM_CALI_s;
LTC_EBM_CALI_s ltc_ebm_cali[BS_NR_OF_MODULES];

static STD_RETURN_TYPE_e LTC_EBM_SetEBState(void);
#if defined(ITRI_MOD_9)
	static STD_RETURN_TYPE_e LTC_EBM_SetEBColState(uint8_t isStart);
	//static STD_RETURN_TYPE_e LTC_EBM_SetEBColState_end(void);
#endif
#endif

#define LTC_EBM_DUMP_NUM	(15)
static uint16_t ltc_ebm_dump_count = 0;

/*================== Function Prototypes ==================================*/
static float LTC_Convert_MuxVoltages_to_Temperatures(float v_adc);

static void LTC_Initialize_Database(void);
static void LTC_SaveBalancingFeedback(uint8_t *DataBufferSPI_RX);
static void LTC_Get_BalancingControlValues(void);

static STD_RETURN_TYPE_e LTC_BalanceControl(uint8_t registerSet);

static void LTC_ResetErrorTable(void);
static STD_RETURN_TYPE_e LTC_Init(void);

static STD_RETURN_TYPE_e LTC_StartVoltageMeasurement(LTC_ADCMODE_e adcMode, LTC_ADCMEAS_CHAN_e  adcMeasCh);
static STD_RETURN_TYPE_e LTC_StartGPIOMeasurement(LTC_ADCMODE_e adcMode, LTC_ADCMEAS_CHAN_e  adcMeasCh);
static uint16_t LTC_Get_MeasurementTCycle(LTC_ADCMODE_e adcMode, LTC_ADCMEAS_CHAN_e  adcMeasCh);

static STD_RETURN_TYPE_e LTC_RX_PECCheck(uint8_t *DataBufferSPI_RX_with_PEC);
static void LTC_SaveRXtoVoltagebuffer(uint8_t registerSet, uint8_t *rxBuffer);

static STD_RETURN_TYPE_e LTC_RX(uint8_t *Command, uint8_t *DataBufferSPI_RX_with_PEC);
static STD_RETURN_TYPE_e LTC_TX(uint8_t *Command, uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC);
static void LTC_SetMUXChCommand(uint8_t *DataBufferSPI_TX, uint8_t mux, uint8_t channel);
static uint8_t LTC_SetMuxChannel(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC, uint8_t mux, uint8_t channel);
static uint8_t LTC_SetPortExpander(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC);
static void LTC_PortExpanderSaveValues(uint8_t *rxBuffer);
//static uint8_t LTC_WriteEEPROMData(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC, uint8_t address, uint8_t data);
static void LTC_EEPROMSaveUID(uint8_t registerSet, uint8_t *rxBuffer);
static void LTC_TempSensSaveTemp(uint8_t *rxBuffer);

static STD_RETURN_TYPE_e LTC_I2CClock(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC);
static STD_RETURN_TYPE_e LTC_Send_I2C_Command(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC, uint8_t *cmd_data);

static uint8_t LTC_I2CCheckACK(uint8_t *DataBufferSPI_RX, int mux);

static void LTC_SaveMuxMeasurement(uint8_t *DataBufferSPI_RX, LTC_MUX_CH_CFG_s  *muxseqptr);
static void LTC_SaveGPIOMeasurement(uint8_t registerSet, uint8_t *rxBuffer);


static uint32_t LTC_GetSPIClock(void);
static void LTC_SetTransferTimes(void);

static LTC_RETURN_TYPE_e LTC_CheckStateRequest(LTC_STATE_REQUEST_e statereq);


/*================== Function Implementations =============================*/
#if defined(ITRI_MOD_9)
static void LTC_EBM_SetGPIO(uint8_t gpioNo, uint8_t val, uint8_t* txBuf) {
	if (val == 0) {
		*txBuf &= ~(0x01 << (gpioNo+2));
	} else {
		*txBuf |= (0x01 << (gpioNo+2));
	}
}

typedef struct {
	uint8_t no;
	uint8_t val;
} GPIO_SETTING;
typedef struct {
	GPIO_SETTING gpio1;
	GPIO_SETTING gpio2;
	GPIO_SETTING gpio4;
} EB_STATE_SETTING;
static EB_STATE_SETTING eb_state_setting[] = {
		// bypass setting
#if defined(ITRI_EBM_CHROMA_V2)
		{{1, 1}, {2, 0}, {4, 1},},
#else	// ver. 3+
		{{1, 1}, {2, 0}, {4, 0},},
#endif
		// enable setting
#if defined(ITRI_EBM_CHROMA_V2)
		{{1, 1}, {2, 0}, {4, 0},},
#else	// ver. 3+
		{{1, 1}, {2, 0}, {4, 1},},
#endif
		// disable setting
		{{1, 1}, {2, 1}, {4, 1},},
};

typedef enum {
	LTC_EBM_COL_STATE_NO_CHANGE	= 0,
	LTC_EBM_COL_STATE_RESET 	= 1,
	LTC_EBM_COL_STATE_DISABLE 	= 2,
	LTC_EBM_COL_STATE_NOT_READ 	= 9,
} LTC_EBM_COL_STATE_e;
typedef struct {
	uint16_t modNo;
	uint8_t  gpioNo;
	LTC_EBM_COL_STATE_e  gpioVal;
} COL_GPIO_SETTING;
typedef struct {
	// read state; gpioVal:9 -> not read, gpioVal:0 -> no change, gpioVal:1 -> reset(ON), gpioVal:2 -> disable(OFF)
	COL_GPIO_SETTING	state;
	// reset(ON)
	COL_GPIO_SETTING	reset;
	// disable(OFF)
	COL_GPIO_SETTING	disable;
} COL_STATE_SETTING;
static COL_STATE_SETTING col_state_setting[] = {
		// column 0
		{{0, 1, LTC_EBM_COL_STATE_NOT_READ},  {1, 1, LTC_EBM_COL_STATE_NO_CHANGE},  {2, 1, LTC_EBM_COL_STATE_NO_CHANGE},},
		// column 1
		{{5, 1, LTC_EBM_COL_STATE_NOT_READ},  {6, 1, LTC_EBM_COL_STATE_NO_CHANGE},  {7, 1, LTC_EBM_COL_STATE_NO_CHANGE},},
		// column 2
		{{10, 1, LTC_EBM_COL_STATE_NOT_READ}, {11, 1, LTC_EBM_COL_STATE_NO_CHANGE}, {12, 1, LTC_EBM_COL_STATE_NO_CHANGE},},
		// column 3
		{{19, 1, LTC_EBM_COL_STATE_NOT_READ}, {18, 1, LTC_EBM_COL_STATE_NO_CHANGE}, {17, 1, LTC_EBM_COL_STATE_NO_CHANGE},},
		// column 4
		{{24, 1, LTC_EBM_COL_STATE_NOT_READ}, {23, 1, LTC_EBM_COL_STATE_NO_CHANGE}, {22, 1, LTC_EBM_COL_STATE_NO_CHANGE},},
};

#if defined(ITRI_MOD_12)
typedef struct {
	uint16_t modNo;
	uint8_t  gpioNo;
} LED_STATE_SETTING;
static LED_STATE_SETTING led_state_setting[] = {
		// LED 0: power
		{3, 1},
		// LED 1:
		{4, 1},
		// LED 2:
		{8, 1},
		// LED 3:
		{9, 1},
		// LED 4:
		{13, 1},
		// LED 5:
		{14, 1},
};

uint32_t set_ebm_led_state(void* iParam1, void* iParam2, void* oParam1, void* oParam2);
static void LTC_EBM_SetLEDState(uint16_t modNo, uint8_t* txBuf) {
	uint8_t i;

	for (i=0; i < BS_NR_OF_LEDS; i++) {
		if (led_state_setting[i].modNo == modNo) {
			LTC_EBM_SetGPIO(led_state_setting[i].gpioNo,
							ltc_led_config[i].eb_state,
							txBuf);
			break;
		}
	}
}
#endif // ITRI_MOD_12

uint32_t set_ebm_eb_col_state(void* iParam1, void* iParam2, void* oParam1, void* oParam2);
static uint8_t ltc_ebm_force_update = 0;

static void LTC_EBM_SetColState(uint16_t modNo, uint8_t* txBuf, uint8_t isStart) {
	uint16_t colNo = modNo / BS_NR_OF_ROWS;
	uint8_t oldState = 0;

	// get col. old state and set new state
	if (col_state_setting[colNo].state.gpioVal == LTC_EBM_COL_STATE_NOT_READ) {
		uint16_t modNo = col_state_setting[colNo].state.modNo;
		uint16_t gpioNo = col_state_setting[colNo].state.gpioNo - 1;
		uint8_t oldState = *((uint16_t *)(&LTC_GPIOVoltages[modNo*6*2 + gpioNo*2])) > 40000 ? 1:0; // ref. #55; 1:Non-Protecting(=enable), 0:Protection(=bypass)
		if (ltc_col_config[colNo].eb_state == oldState) {
			col_state_setting[colNo].state.gpioVal = LTC_EBM_COL_STATE_NO_CHANGE;	// no change
		} else if (oldState == 1 && ltc_col_config[colNo].eb_state == 0) {
			col_state_setting[colNo].state.gpioVal = LTC_EBM_COL_STATE_DISABLE; // disable(turn OFF SPM)
		} else {
			col_state_setting[colNo].state.gpioVal = LTC_EBM_COL_STATE_RESET; // reset(turn ON SPM)
		}
		//DEBUG_PRINTF_EX("oldState:%u(%u, %u) newState:%u colState:%u colNo:%u modNo:%u\r\n",
		//		oldState, gpioNo, *((uint16_t *)(&LTC_GPIOVoltages[modNo*6*2 + gpioNo*2])),
		//		ltc_col_config[colNo].eb_state, col_state_setting[colNo].state.gpioVal, colNo, modNo);
	}

	// force update
	if (ltc_ebm_force_update == 1) {
		if (ltc_col_config[colNo].eb_state == 0) col_state_setting[colNo].state.gpioVal = LTC_EBM_COL_STATE_DISABLE;
		else 									 col_state_setting[colNo].state.gpioVal = LTC_EBM_COL_STATE_RESET;
	}

#define SPM_CONFIG_RESET_DISABLE(r, d) 									\
		if (col_state_setting[colNo].disable.modNo == modNo) { 			\
			LTC_EBM_SetGPIO(col_state_setting[colNo].disable.gpioNo,	\
							d,											\
							txBuf);										\
			/*DEBUG_PRINTF_EX("disable(%u) colNo:%u modNo:%u isStart:%u col.state:%u\r\n", d, colNo, modNo, isStart, col_state_setting[colNo].state.gpioVal );*/ \
		}																\
		if (col_state_setting[colNo].reset.modNo == modNo) {			\
			LTC_EBM_SetGPIO(col_state_setting[colNo].reset.gpioNo,		\
							r,											\
							txBuf);										\
			/*DEBUG_PRINTF_EX("reset(%u) colNo:%u modNo:%u isStart:%u col.state:%u\r\n", r, colNo, modNo, isStart, col_state_setting[colNo].state.gpioVal );*/ \
		}

	// reset
	if (col_state_setting[colNo].state.gpioVal == LTC_EBM_COL_STATE_RESET) {
		SPM_CONFIG_RESET_DISABLE(isStart, 0);
		/*
		if (col_state_setting[colNo].reset.modNo == modNo) {
			LTC_EBM_SetGPIO(col_state_setting[colNo].reset.gpioNo,
							isStart,
							txBuf);
		}
		if (col_state_setting[colNo].disable.modNo == modNo) {
			LTC_EBM_SetGPIO(col_state_setting[colNo].disable.gpioNo,
							0,
							txBuf);
		}
		*/
	}
	// disable
	if (col_state_setting[colNo].state.gpioVal == LTC_EBM_COL_STATE_DISABLE) {
		SPM_CONFIG_RESET_DISABLE(0, isStart);
		/*
		if (col_state_setting[colNo].disable.modNo == modNo) {
			LTC_EBM_SetGPIO(col_state_setting[colNo].disable.gpioNo,
							isStart,
							txBuf);
		}
		if (col_state_setting[colNo].reset.modNo == modNo) {
			LTC_EBM_SetGPIO(col_state_setting[colNo].reset.gpioNo,
							0,
							txBuf);
		}
		*/
	}
	// no change
	if (col_state_setting[colNo].state.gpioVal == LTC_EBM_COL_STATE_NO_CHANGE) {
		SPM_CONFIG_RESET_DISABLE(0, 0);
	}
	// reset col. gpioVal to "not read"
	if (isStart == 0 && (modNo % BS_NR_OF_ROWS) == (BS_NR_OF_ROWS - 1)) {
		col_state_setting[colNo].state.gpioVal = LTC_EBM_COL_STATE_NOT_READ;
	}
	//DEBUG_PRINTF_EX("col:%u mod:%u txBuf:0x%x\r\n", colNo, modNo, txBuf[0]);
}

static STD_RETURN_TYPE_e LTC_EBM_SetEBColState(uint8_t isStart) {
	STD_RETURN_TYPE_e retVal = E_OK;
	uint16_t i=0, j=0;
	uint8_t gpio1 = 1, gpio2 = 1, gpio4 = 1;

	for (j=0; j < BS_NR_OF_MODULES; j++) {
		i = BS_NR_OF_MODULES-j-1;
		ltc_tmpTXbuffer[0+(i)*6] = 0xFC;	// REFON = 1, GPIOs are all enabled

		/*
		LTC_EBM_SetGPIO(eb_state_setting[ltc_ebm_config[j].eb_state].gpio1.no,
						eb_state_setting[ltc_ebm_config[j].eb_state].gpio1.val,
						&ltc_tmpTXbuffer[0+(i)*6]);
		*/
		LTC_EBM_SetColState(j,
							&ltc_tmpTXbuffer[0+(i)*6],
							isStart);
		LTC_EBM_SetGPIO(eb_state_setting[ltc_ebm_config[j].eb_state].gpio2.no,
						eb_state_setting[ltc_ebm_config[j].eb_state].gpio2.val,
						&ltc_tmpTXbuffer[0+(i)*6]);
		LTC_EBM_SetGPIO(eb_state_setting[ltc_ebm_config[j].eb_state].gpio4.no,
						eb_state_setting[ltc_ebm_config[j].eb_state].gpio4.val,
						&ltc_tmpTXbuffer[0+(i)*6]);

#if defined(ITRI_MOD_12)
		LTC_EBM_SetLEDState(j, &ltc_tmpTXbuffer[0+(i)*6]);
#endif

		ltc_tmpTXbuffer[1+(i)*6] = 0x00;
		ltc_tmpTXbuffer[2+(i)*6] = 0x00;
		ltc_tmpTXbuffer[3+(i)*6] = 0x00;
		ltc_tmpTXbuffer[4+(i)*6] = 0x00;
		ltc_tmpTXbuffer[5+(i)*6] = 0x00;
	}
	//DEBUG_PRINTF_EX("[%u ms]isStart:%u 0x%x 0x%x 0x%x \r\n",
	//		MCU_GetTimeStamp(), isStart,
	//		ltc_tmpTXbuffer[2*6], ltc_tmpTXbuffer[1*6], ltc_tmpTXbuffer[0*6]);
	retVal = LTC_TX((uint8_t*)ltc_cmdWRCFG, ltc_tmpTXbuffer, ltc_DataBufferSPI_TX_with_PEC_init);

	return retVal;
}
/*
static STD_RETURN_TYPE_e LTC_EBM_SetEBColState_end(void) {
	STD_RETURN_TYPE_e retVal = E_OK;

	return retVal;
}
*/

#endif // ITRI_MOD_9
/*================== Public functions =====================================*/

/*================== Static functions =====================================*/
/**
 * @brief   in the database, initializes the fields related to the LTC drivers.
 *
 * This function loops through all the LTC-related data fields in the database
 * and sets them to 0. It should be called in the initialization or re-initialization
 * routine of the LTC driver.
 *
 * @return   void
 */
static void LTC_Initialize_Database(void) {

    uint16_t i = 0;

    ltc_cellvoltage.state = 0;
    ltc_cellvoltage.timestamp = 0;
    ltc_minmax.voltage_min = 0;
    ltc_minmax.voltage_max = 0;
    ltc_minmax.voltage_module_number_min = 0;
    ltc_minmax.voltage_module_number_max = 0;
    ltc_minmax.voltage_cell_number_min = 0;
    ltc_minmax.voltage_cell_number_max = 0;
    for (i=0; i < BS_NR_OF_BAT_CELLS; i++) {
        ltc_cellvoltage.voltage[i] = 0;
    }

    ltc_celltemperature.state = 0;
    ltc_celltemperature.timestamp = 0;
    ltc_minmax.temperature_min = 0;
    ltc_minmax.temperature_max = 0;
    ltc_minmax.temperature_module_number_min = 0;
    ltc_minmax.temperature_module_number_max = 0;
    ltc_minmax.temperature_sensor_number_min = 0;
    ltc_minmax.temperature_sensor_number_max = 0;
    for (i=0; i < BS_NR_OF_TEMP_SENSORS; i++) {
        ltc_celltemperature.temperature[i] = 0;
    }

    ltc_balancing_feedback.state = 0;
    ltc_balancing_feedback.timestamp = 0;
    ltc_balancing_control.state = 0;
    ltc_balancing_control.timestamp = 0;
    for (i=0; i < BS_NR_OF_BAT_CELLS; i++) {
        ltc_balancing_feedback.value[i] = 0;
        ltc_balancing_control.value[i] = 0;
    }

    ltc_user_io_control.state = 0;
    ltc_user_io_control.timestamp = 0;
    ltc_user_io_control.previous_timestamp = 0;
    for (i=0; i < BS_NR_OF_MODULES; i++) {
        ltc_user_io_control.value_in[i] = 0;
        ltc_user_io_control.value_out[i] = 0xFE;    // FIXME: swacker: start value for running light testing only
    }

    DATA_StoreDataBlock(&ltc_cellvoltage, DATA_BLOCK_ID_CELLVOLTAGE);
    DATA_StoreDataBlock(&ltc_celltemperature, DATA_BLOCK_ID_CELLTEMPERATURE);
    DATA_StoreDataBlock(&ltc_minmax, DATA_BLOCK_ID_MINMAX);
    DATA_StoreDataBlock(&ltc_balancing_feedback, DATA_BLOCK_ID_BALANCING_FEEDBACK_VALUES);
    DATA_StoreDataBlock(&ltc_balancing_control, DATA_BLOCK_ID_BALANCING_CONTROL_VALUES);

    for (i=0; i < (8*2*BS_NR_OF_MODULES); i++) {
        ltc_user_mux.value[i] = 0;
    }
    ltc_user_mux.previous_timestamp = 0;
    ltc_user_mux.timestamp = 0;
    ltc_user_mux.state = 0;

}

/**
 * @brief   stores the measured voltages of the 5 GPIOs of each LTC in daisy chain.
 *
 * Voltages currently stored in LTC_allGPIOVoltages[], but not in database yet.
 * This has to be added by user in case the use of these voltages is needed.
 *
 * The multiplication is used to get the result in mV . For details on conversion see LTC data sheet.
 *
 * @return  void
 *
 */
extern void LTC_SaveAllGPIOs(void) {

    #if BS_NR_OF_BAT_CELLS_PER_MODULE > 12
    for (uint16_t i=0; i < 12*LTC_N_LTC; i++) {
        LTC_allGPIOVoltages[i] = (uint16_t)(((float)(*((uint16_t *)(&LTC_GPIOVoltages[2*i]))))*100e-6*1000.0);
    }
    #else
    for (uint16_t i=0; i < 6*LTC_N_LTC; i++) {
        LTC_allGPIOVoltages[i] = (uint16_t)(((float)(*((uint16_t *)(&LTC_GPIOVoltages[2*i]))))*100e-6*1000.0);

#if defined(ITRI_MOD_6_g)
        {
        	uint16_t modNo = i / 6;
        	uint16_t gpioNo = i % 6;
        	//if (gpioNo == 2 && (LTC_allGPIOVoltages[i] < 900 || LTC_allGPIOVoltages[i] > 1600)) {
        	//	DEBUG_PRINTF_EX("[%d:ERR] M[%u] gpio[%u] vol:%u\r\n", __LINE__, modNo, gpioNo, LTC_allGPIOVoltages[i]);
        	//} else
        	if (gpioNo == 4 && (LTC_allGPIOVoltages[i] > 2500)) {
        		DEBUG_PRINTF_EX("[%d:ERR] M[%u] gpio[%u] vol:%u\r\n", __LINE__, modNo, gpioNo, LTC_allGPIOVoltages[i]);
        	} else if (gpioNo == 5 && (LTC_allGPIOVoltages[i] > 3100 || LTC_allGPIOVoltages[i] < 2900)) {
        		DEBUG_PRINTF_EX("[%d:ERR] M[%u] gpio[%u] vol:%u\r\n", __LINE__, modNo, gpioNo, LTC_allGPIOVoltages[i]);
        	}
        }
#endif
    }
    #endif

#if 0	//debug; dump gpio 1/2/4
	if (ltc_ebm_dump_count > 0) {
		uint8_t modNo = 0;
		DEBUG_PRINTF_EX("M[%d] gpio1/2/4: %u/%u/%u (mV)\r\n", modNo,
				LTC_allGPIOVoltages[modNo*6 + 0], LTC_allGPIOVoltages[modNo*6 + 1], LTC_allGPIOVoltages[modNo*6 + 3]);
		ltc_ebm_dump_count--;
	}
#endif
}

/**
 * @brief   stores the measured voltages in the database.
 *
 * This function loops through the data of all modules in the LTC daisy-chain that are
 * stored in the LTC_CellVoltages buffer and writes them in the database.
 * At each write iteration, the variable named "state" and related to voltages in the
 * database is incremented.
 *
 * @return  void
 *
 */
extern void LTC_SaveVoltages(void) {

    uint16_t i = 0;
    uint16_t j = 0;
    uint16_t val_ui = 0;
    float  val_fl = 0.0;
    uint16_t min = 0;
    uint16_t max = 0;
    uint32_t mean = 0;
    uint8_t module_number_min = 0;
    uint8_t module_number_max = 0;
    uint8_t cell_number_min = 0;
    uint8_t cell_number_max = 0;

    for (i=0; i < BS_NR_OF_MODULES; i++) {
#if defined(ITRI_MOD_2)
		if (ltc_ebm_cmd == LTC_EBM_CURR_CALI && ltc_ebm_cali[i].isCali > 0) {
			//DEBUG_PRINTF_EX("%u %u\r\n", *((uint16_t *)(&LTC_CellVoltages[2*LTC_EBM_BAT_CURR_IDX+i*LTC_NUMBER_OF_LTC_PER_MODULE*2*BS_NR_OF_BAT_CELLS_PER_MODULE])),
			//							 *((uint16_t *)(&LTC_CellVoltages[2*LTC_EBM_MOD_CURR_IDX+i*LTC_NUMBER_OF_LTC_PER_MODULE*2*BS_NR_OF_BAT_CELLS_PER_MODULE])));
			ltc_ebm_cali[i].curBat_offset += (*((int16_t *)(&LTC_CellVoltages[2*LTC_EBM_BAT_CURR_IDX+i*LTC_NUMBER_OF_LTC_PER_MODULE*2*BS_NR_OF_BAT_CELLS_PER_MODULE])) - LTC_EBM_CURR_ZERO_BASE);
			ltc_ebm_cali[i].curMod_offset += (*((int16_t *)(&LTC_CellVoltages[2*LTC_EBM_MOD_CURR_IDX+i*LTC_NUMBER_OF_LTC_PER_MODULE*2*BS_NR_OF_BAT_CELLS_PER_MODULE])) - LTC_EBM_CURR_ZERO_BASE);
			ltc_ebm_cali[i].isCali--;
			if (ltc_ebm_cali[i].isCali == 0) {
				ltc_ebm_cali[i].curBat_offset /= LTC_EBM_MAX_CURR_CAL_CNT;
				ltc_ebm_cali[i].curMod_offset /= LTC_EBM_MAX_CURR_CAL_CNT;
				if (i == (BS_NR_OF_MODULES-1)) {
					ltc_ebm_cmd = LTC_EBM_NONE;
					DEBUG_PRINTF_EX("current calibation done.\r\n");
				}
			}
		}
#endif

        for (j=0; j < BS_NR_OF_BAT_CELLS_PER_MODULE; j++) {
#if defined(ITRI_MOD_3)
        	val_ui = *((uint16_t *)(&LTC_CellVoltages[2*j+i*LTC_NUMBER_OF_LTC_PER_MODULE*BS_NR_OF_BAT_CELLS_PER_MODULE*2]));        // raw values
#else
            val_ui = *((uint16_t *)(&LTC_CellVoltages[2*j+i*LTC_NUMBER_OF_LTC_PER_MODULE*24]));        // raw values
#endif
#if defined(ITRI_MOD_2)
            if (ltc_ebm_cali[i].isCali == 0) {
				if (j == LTC_EBM_BAT_CURR_IDX) val_ui = (uint16_t)(val_ui - ltc_ebm_cali[i].curBat_offset);
            	/*
            	if (j == LTC_EBM_BAT_CURR_IDX) {
            		DEBUG_PRINTF_EX("%u %u %u\r\n", val_ui, LTC_EBM_CURR_ZERO_BASE, ltc_ebm_cali[i].curBat_offset);
            		val_ui = val_ui + LTC_EBM_CURR_ZERO_BASE - ltc_ebm_cali[i].curBat_offset;
            		DEBUG_PRINTF_EX("%u\r\n", val_ui);
            	}
            	*/
				if (j == LTC_EBM_MOD_CURR_IDX) val_ui = (uint16_t)(val_ui - ltc_ebm_cali[i].curMod_offset);
            }
#endif
            val_fl = ((float)(val_ui))*100e-6*1000.0;        // Unit V -> in mV
#if defined(ITRI_MOD_6_g)
            if (j < 4 && (val_fl >= 3600 || val_fl <= 2600)) {
            	DEBUG_PRINTF_EX("[%d:ERR]M[%d] C[%d] vol:%s\r\n", __LINE__, i, j, float_to_string(val_fl));
            } else if ((j == 4 || j == 5) && (val_fl > 2600 || val_fl < 2400)) {
            	DEBUG_PRINTF_EX("[%d:ERR]M[%d] C[%d] vol:%s\r\n", __LINE__, i, j, float_to_string(val_fl));
            }
#endif
            ltc_cellvoltage.voltage[i*(BS_NR_OF_BAT_CELLS_PER_MODULE)+j]=(uint16_t)(val_fl);
        }
    }

#if 0	//debug; dump cell vol.
	if (ltc_ebm_dump_count > 0) {
		uint8_t modNo = 0;
		uint8_t cellNo = 6;	// 0~3:cell vol., 4:IB, 5:IM, 6:VM
		DEBUG_PRINTF_EX("M[%d] cell[%d] %u\r\n", modNo, cellNo, ltc_cellvoltage.voltage[modNo*(BS_NR_OF_BAT_CELLS_PER_MODULE)+cellNo]);
		ltc_ebm_dump_count--;
	}
#endif

    max = min = ltc_cellvoltage.voltage[0];
    mean = 0;
    for (i=0; i < BS_NR_OF_MODULES; i++) {
#if defined(ITRI_MOD_13)
    	for (j=0; j < ITRI_NR_OF_BAT_CELLS_PER_MODULE; j++) {
#else
        for (j=0; j < BS_NR_OF_BAT_CELLS_PER_MODULE; j++) {
#endif
            mean += ltc_cellvoltage.voltage[i*(BS_NR_OF_BAT_CELLS_PER_MODULE)+j];
            if (ltc_cellvoltage.voltage[i*(BS_NR_OF_BAT_CELLS_PER_MODULE)+j] < min) {
                min = ltc_cellvoltage.voltage[i*(BS_NR_OF_BAT_CELLS_PER_MODULE)+j];
                module_number_min = i;
                cell_number_min = j;
            }
            if (ltc_cellvoltage.voltage[i*(BS_NR_OF_BAT_CELLS_PER_MODULE)+j] > max) {
                max = ltc_cellvoltage.voltage[i*(BS_NR_OF_BAT_CELLS_PER_MODULE)+j];
                module_number_max = i;
                cell_number_max = j;
            }
        }
    }
    mean /= (BS_NR_OF_BAT_CELLS);

    DATA_GetTable(&ltc_minmax, DATA_BLOCK_ID_MINMAX);
    ltc_cellvoltage.state++;
    ltc_minmax.state++;
    ltc_minmax.voltage_mean = mean;
    ltc_minmax.previous_voltage_min = ltc_minmax.voltage_min;
    ltc_minmax.voltage_min = min;
    ltc_minmax.voltage_module_number_min = module_number_min;
    ltc_minmax.voltage_cell_number_min = cell_number_min;
    ltc_minmax.previous_voltage_max = ltc_minmax.voltage_max;
    ltc_minmax.voltage_max = max;
    ltc_minmax.voltage_module_number_max = module_number_max;
    ltc_minmax.voltage_cell_number_max = cell_number_max;
    DATA_StoreDataBlock(&ltc_cellvoltage, DATA_BLOCK_ID_CELLVOLTAGE);
    DATA_StoreDataBlock(&ltc_minmax, DATA_BLOCK_ID_MINMAX);

}

/**
 * @brief   stores the measured temperatures and the measured multiplexer feedbacks in the database.
 *
 * This function loops through the temperature and multiplexer feedback data of all modules
 * in the LTC daisy-chain that are stored in the LTC_MultiplexerVoltages buffer and writes
 * them in the database.
 * At each write iteration, the variables named "state" and related to temperatures and multiplexer feedbacks
 * in the database are incremented.
 *
 * @return  void
 *
 */
extern void LTC_SaveTemperatures(void) {

    uint16_t i = 0;
    uint16_t j = 0;
    uint8_t sensor_idx = 0;
    uint8_t ch_idx = 0;
    uint16_t val_ui = 0;
    int16_t val_si = 0;
    float  val_fl = 0.0;

    int16_t min = 0;
    uint16_t max = 0;
    int32_t mean = 0;
    uint8_t module_number_min = 0;
    uint8_t module_number_max = 0;
    uint8_t sensor_number_min = 0;
    uint8_t sensor_number_max = 0;

    LTC_MUX_CH_CFG_s *muxseqptr;     // pointer to measurement Sequence of Mux- and Channel-Configurations (1,-1)...(3,-1),(0,1),...(0,7)
    LTC_MUX_CH_CFG_s *muxseqendptr;  // pointer to ending point of sequence

    muxseqptr = ltc_mux_seq.seqptr;
    muxseqendptr = ((LTC_MUX_CH_CFG_s *)ltc_mux_seq.seqptr)+ltc_mux_seq.nr_of_steps;  // last sequence + 1

    while (muxseqptr < muxseqendptr) {

        // last step of sequence not reached
        if (muxseqptr->muxCh != 0xff) {

            if (muxseqptr->muxID == 0) {    // muxID 0 // typically temperature multiplexer type
                for (i=0; i < BS_NR_OF_MODULES; i++) {

                    val_ui =*((uint16_t *)(&LTC_MultiplexerVoltages[2*((LTC_NUMBER_OF_LTC_PER_MODULE*i*LTC_N_MUX_CHANNELS_PER_LTC)+muxseqptr->muxID*LTC_N_MUX_CHANNELS_PER_MUX+muxseqptr->muxCh)]));  // raw values, all mux on all LTCs
                    val_fl = ((float)(val_ui))*100e-6;        // Unit -> in V

                    sensor_idx = ltc_muxsensortemperatur_cfg[muxseqptr->muxCh];

                    if (sensor_idx >= BS_NR_OF_TEMP_SENSORS_PER_MODULE)
                        return;

                    val_si = (int16_t)(LTC_Convert_MuxVoltages_to_Temperatures(val_fl));
                    ltc_celltemperature.temperature[i*(BS_NR_OF_TEMP_SENSORS_PER_MODULE)+sensor_idx] = val_si;

                }
            }

            if (muxseqptr->muxID >= 1 && muxseqptr->muxID <= 2) {   // muxID 1 or 2 // typically user multiplexer type
                for (i=0; i < BS_NR_OF_MODULES; i++) {
                    if (muxseqptr->muxID == 1)
                        ch_idx = 0 + muxseqptr->muxCh;    // channel index 0..7
                    else
                        ch_idx = 8 + muxseqptr->muxCh;    // channel index 8..15

                    if (ch_idx < 2*8) {
                        val_ui =*((uint16_t *)(&LTC_MultiplexerVoltages[2*(LTC_NUMBER_OF_LTC_PER_MODULE*i*LTC_N_MUX_CHANNELS_PER_LTC+muxseqptr->muxID*LTC_N_MUX_CHANNELS_PER_MUX+muxseqptr->muxCh)]));        // raw values, all mux on all LTCs
                        val_fl = ((float)(val_ui))*100e-6*1000.0;        // Unit -> in V -> in mV
                        ltc_user_mux.value[i*8*2+ch_idx] = (uint16_t)(val_fl);
                    }
                }
            }
        }  // end if! 0xff
        ++muxseqptr;
    }  // end while

    mean = 0;
    max = min = ltc_celltemperature.temperature[0];
    for (i=0; i < BS_NR_OF_MODULES; i++) {
        for (j=0; j < BS_NR_OF_TEMP_SENSORS_PER_MODULE; j++) {
            mean += ltc_celltemperature.temperature[i*(BS_NR_OF_TEMP_SENSORS_PER_MODULE)+j];
            if (ltc_celltemperature.temperature[i*(BS_NR_OF_TEMP_SENSORS_PER_MODULE)+j] < min) {
                min = ltc_celltemperature.temperature[i*(BS_NR_OF_TEMP_SENSORS_PER_MODULE)+j];
                module_number_min = i;
                sensor_number_min = j;
            }
            if (ltc_celltemperature.temperature[i*(BS_NR_OF_TEMP_SENSORS_PER_MODULE)+j] > max) {
                max = ltc_celltemperature.temperature[i*(BS_NR_OF_TEMP_SENSORS_PER_MODULE)+j];
                module_number_max = i;
                sensor_number_max = j;
            }
        }
    }
    mean /= (BS_NR_OF_TEMP_SENSORS);

    DATA_GetTable(&ltc_minmax, DATA_BLOCK_ID_MINMAX);
    ltc_celltemperature.previous_timestamp = ltc_celltemperature.timestamp;
    ltc_celltemperature.timestamp = MCU_GetTimeStamp();
    ltc_celltemperature.state++;
    ltc_minmax.state++;
    ltc_minmax.temperature_mean = mean;
    ltc_minmax.temperature_min = min;
    ltc_minmax.temperature_module_number_min = module_number_min;
    ltc_minmax.temperature_sensor_number_min = sensor_number_min;
    ltc_minmax.temperature_max = max;
    ltc_minmax.temperature_module_number_max = module_number_max;
    ltc_minmax.temperature_sensor_number_max = sensor_number_max;

#if defined(ITRI_MOD_2)
	for (i=0; i < BS_NR_OF_MODULES; i++) {
		for (j=0; j < 6; j++) {
			//(uint16_t)(((float)(*((uint16_t *)(&LTC_GPIOVoltages[2*i]))))*100e-6*1000.0)
			ltc_celltemperature.temperature[i*(BS_NR_OF_TEMP_SENSORS_PER_MODULE)+j+6] = *((int16_t *)(&LTC_GPIOVoltages[i*6*2 + j*2]));
			//ltc_celltemperature.temperature[i*(BS_NR_OF_TEMP_SENSORS_PER_MODULE)+j+6] = *((int16_t *)(&LTC_allGPIOVoltages[i*6 + j]));
		}
	}
	//DEBUG_PRINTF_EX("temp:%u %u %u %u %u %u\r\n",
	//	ltc_celltemperature.temperature[6], ltc_celltemperature.temperature[7], ltc_celltemperature.temperature[8],
	//	ltc_celltemperature.temperature[9], ltc_celltemperature.temperature[10], ltc_celltemperature.temperature[11]);
#endif

    DATA_StoreDataBlock(&ltc_celltemperature, DATA_BLOCK_ID_CELLTEMPERATURE);
    DATA_StoreDataBlock(&ltc_minmax, DATA_BLOCK_ID_MINMAX);
}

/**
 * @brief   stores the measured balancing feedback values in the database.
 *
 * This function stores the global balancing feedback value measured on GPIO3 of the LTC into the database
 *
 * @return  void
 *
 */
static void LTC_SaveBalancingFeedback(uint8_t *DataBufferSPI_RX) {

    uint16_t i = 0;
    uint16_t j = 0;
    uint16_t val_i = 0;

    for (i=0; i < LTC_N_LTC; i++) {

        val_i = DataBufferSPI_RX[8+1*i*8] | (DataBufferSPI_RX[8+1*i*8+1] << 8);    // raw value, GPIO3

        for (j=0; j < BS_NR_OF_BAT_CELLS_PER_MODULE; j++) {
            ltc_balancing_feedback.value[j+i*BS_NR_OF_BAT_CELLS_PER_MODULE] = val_i;
        }
    }

    ltc_balancing_feedback.previous_timestamp = ltc_balancing_feedback.timestamp;
    ltc_balancing_feedback.timestamp = MCU_GetTimeStamp();
    ltc_balancing_feedback.state++;
    DATA_StoreDataBlock(&ltc_balancing_feedback, DATA_BLOCK_ID_BALANCING_FEEDBACK_VALUES);

}


/**
 * @brief   gets the balancing orders from the database.
 *
 * This function gets the balancing control from the database. Balancing control
 * is set by the BMS. The LTC driver only executes the balancing orders.
 *
 * @return  void
 */
static void LTC_Get_BalancingControlValues(void) {
    DATA_GetTable(&ltc_balancing_control, DATA_BLOCK_ID_BALANCING_CONTROL_VALUES);
}


/**
 * @brief   converts a raw voltage from multiplexer to a temperature value in Celsius.
 *
 * The temperatures are read from NTC elements via voltage dividers.
 * This function implements the look-up table between voltage and temperature,
 * taking into account the NTC characteristics and the voltage divider.
 *
 * @param   Vout            voltage read from the multiplexer in V
 *
 * @return  temperature     temperature value in Celsius
 */
static float LTC_Convert_MuxVoltages_to_Temperatures(float v_adc) {

    float temperature = 0.0;
    //float v_adc2 = v_adc*v_adc;
    //float v_adc3 = v_adc2*v_adc;
    //float v_adc4 = v_adc3*v_adc;
    //float v_adc5 = v_adc4*v_adc;
    //float v_adc6 = v_adc5*v_adc;

    // Example: 5th grade polynomial for EPCOS B57861S0103F045 NTC-Thermistor, 10 kOhm, Series B57861S
    //temperature = -6.2765*v_adc5 + 49.0397*v_adc4 - 151.3602*v_adc3 + 233.2521*v_adc2 - 213.4588*v_adc + 130.5822;


    // Dummy function, must be adapted to the application
    temperature = 10 * v_adc;

    return temperature;
}

/**
 * @brief   re-entrance check of LTC state machine trigger function
 *
 * This function is not re-entrant and should only be called time- or event-triggered.
 * It increments the triggerentry counter from the state variable ltc_state.
 * It should never be called by two different processes, so if it is the case, triggerentry
 * should never be higher than 0 when this function is called.
 *
 *
 * @return  retval  0 if no further instance of the function is active, 0xff else
 *
 */
uint8_t LTC_CheckReEntrance(void) {
    uint8_t retval = 0;

    OS_TaskEnter_Critical();
    if (!ltc_state.triggerentry) {
        ltc_state.triggerentry++;
    } else {
        retval = 0xFF;    // multiple calls of function
    }
    OS_TaskExit_Critical();

    return (retval);
}

/**
 * @brief   gets the current state request.
 *
 * This function is used in the functioning of the LTC state machine.
 *
 * @return  retval  current state request, taken from LTC_STATE_REQUEST_e
 */
extern LTC_STATE_REQUEST_e LTC_GetStateRequest(void) {

    LTC_STATE_REQUEST_e retval = LTC_STATE_NO_REQUEST;

    OS_TaskEnter_Critical();
    retval    = ltc_state.statereq;
    OS_TaskExit_Critical();

    return (retval);
}

/**
 * @brief   gets the current state.
 *
 * This function is used in the functioning of the LTC state machine.
 *
 * @return  current state, taken from LTC_STATEMACH_e
 */
extern LTC_STATEMACH_e LTC_GetState(void) {
    return (ltc_state.state);
}

/**
 * @brief   transfers the current state request to the state machine.
 *
 * This function takes the current state request from ltc_state and transfers it to the state machine.
 * It resets the value from ltc_state to LTC_STATE_NO_REQUEST
 *
 * @param   *busIDptr       bus ID, main or backup (deprecated)
 * @param   *adcModeptr     LTC ADCmeasurement mode (fast, normal or filtered)
 * @param   *adcMeasChptr   number of channels measured for GPIOS (one at a time for multiplexers or all five GPIOs)
 *
 * @return  retVal          current state request, taken from LTC_STATE_REQUEST_e
 *
 */
LTC_STATE_REQUEST_e LTC_TransferStateRequest(uint8_t *busIDptr, LTC_ADCMODE_e *adcModeptr, LTC_ADCMEAS_CHAN_e *adcMeasChptr) {

    LTC_STATE_REQUEST_e retval = LTC_STATE_NO_REQUEST;

    OS_TaskEnter_Critical();
    retval    = ltc_state.statereq;
    *adcModeptr = ltc_state.adcModereq;
    *adcMeasChptr = ltc_state.adcMeasChreq;
    ltc_state.statereq = LTC_STATE_NO_REQUEST;
    OS_TaskExit_Critical();

    return (retval);
}

LTC_RETURN_TYPE_e LTC_SetStateRequest(LTC_STATE_REQUEST_e statereq, LTC_ADCMODE_e adcModereq, LTC_ADCMEAS_CHAN_e adcMeasChreq, uint8_t numberOfMeasuredMux) {

    LTC_RETURN_TYPE_e retVal = LTC_STATE_NO_REQUEST;

    OS_TaskEnter_Critical();
    retVal = LTC_CheckStateRequest(statereq);

    if (retVal == LTC_OK || retVal == LTC_BUSY_OK || retVal == LTC_OK_FROM_ERROR) {

		ltc_state.statereq   = statereq;
		ltc_state.adcModereq = adcModereq;            // measurement mode
		ltc_state.adcMeasChreq = adcMeasChreq;        // measurement channel
		ltc_state.numberOfMeasuredMux = numberOfMeasuredMux;        // measurement channel
	}
    OS_TaskExit_Critical();

    return (retVal);
}

void LTC_Trigger(void) {

    uint8_t mux_error = 0;
    static uint8_t user_io_taskcycle = 0;
    static uint8_t eeprom_taskcycle = 0;
    static uint8_t temp_sens_taskcycle = 0;

    STD_RETURN_TYPE_e retVal = E_OK;
    LTC_STATE_REQUEST_e statereq = LTC_STATE_NO_REQUEST;
    uint8_t tmpbusID = 0;
    LTC_ADCMODE_e tmpadcMode = LTC_ADCMODE_UNDEFINED;
    LTC_ADCMEAS_CHAN_e tmpadcMeasCh = LTC_ADCMEAS_UNDEFINED;

    // Check re-entrance of function
    if (LTC_CheckReEntrance())
        return;

    DIAG_SysMonNotify(DIAG_SYSMON_LTC_ID, 0);        // task is running, state = ok

#if defined(ITRI_MOD_6_i)
    if (ltc_state.check_spi_flag == 0) {
#endif

    if (ltc_state.timer) {
        if (--ltc_state.timer) {
            ltc_state.triggerentry--;
            return;    // handle state machine only if timer has elapsed
        }
    }
#if defined(ITRI_MOD_6_i)
	} else {
		if (SPI_IsTransmitOngoing() == TRUE) {
		   if (ltc_state.timer) {
			   if (--ltc_state.timer) {
				   ltc_state.triggerentry--;
				   return;    // handle state machine only if timer has elapsed
			   }
		   }
	   }
	   ltc_state.timer = 0;

	   if (ltc_state.check_spi_flag == 2) {
		   ltc_state.timer = ltc_state.commandTransferTime;	// should be 3ms(accuracy 2.3ms) wait time for ADC
	   }
#if defined(ITRI_MOD_9)
	   if (ltc_state.check_spi_flag == 3) {
		   ltc_state.timer = 70;	// 70ms for revised SPM
	   }
#endif
	   ltc_state.check_spi_flag = 0;	// reset flag
	   if (ltc_state.timer > 0) {
		   ltc_state.triggerentry--;
		   return;
	   }
   }
#endif


    switch (ltc_state.state) {
        /****************************UNINITIALIZED***********************************/
        case LTC_STATEMACH_UNINITIALIZED:
            // waiting for Initialization Request
            statereq = LTC_TransferStateRequest(&tmpbusID, &tmpadcMode, &tmpadcMeasCh);
            if (statereq == LTC_STATE_INIT_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_INITIALIZATION;
                ltc_state.substate = LTC_ENTRY_UNINITIALIZED;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = tmpadcMode;
                ltc_state.adcMeasCh = tmpadcMeasCh;
            } else if (statereq == LTC_STATE_NO_REQUEST) {

                // no actual request pending //
            } else {
                ltc_state.ErrRequestCounter++;   // illegal request pending
            }
            break;

        /****************************INITIALIZATION**********************************/
        case LTC_STATEMACH_INITIALIZATION:

            LTC_SetTransferTimes();
            ltc_state.muxmeas_seqptr = ltc_mux_seq.seqptr;
            ltc_state.muxmeas_nr_end = ltc_mux_seq.nr_of_steps;
            ltc_state.muxmeas_seqendptr = ((LTC_MUX_CH_CFG_s *)ltc_mux_seq.seqptr)+ltc_mux_seq.nr_of_steps;  // last sequence + 1

            if (ltc_state.substate == LTC_ENTRY_INITIALIZATION) {

                LTC_SAVELASTSTATES();
                retVal = LTC_SendWakeUp();        // Send dummy byte to wake up the daisy chain

                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer    = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    // LTC6804 datasheet page 41
                    // If LTC stack too long: a second wake up must be done
                    if (LTC_STATEMACH_DAISY_CHAIN_FIRST_INITIALIZATION_TIME > (LTC_TIDLE_US*1000)) {

                        ltc_state.substate = LTC_RE_ENTRY_INITIALIZATION;
                    } else {
                        // LTC stack not too long: second wake up not necessary
                        ltc_state.substate = LTC_START_INIT_INITIALIZATION;
                    }
                    ltc_state.timer = LTC_STATEMACH_DAISY_CHAIN_FIRST_INITIALIZATION_TIME;
                }
            } else if (ltc_state.substate == LTC_RE_ENTRY_INITIALIZATION) {

                LTC_SAVELASTSTATES();
                retVal = LTC_SendWakeUp();  // Send dummy byte again to wake up the daisy chain

                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_START_INIT_INITIALIZATION;
                    ltc_state.timer = LTC_STATEMACH_DAISY_CHAIN_SECOND_INITIALIZATION_TIME;
                }
            } else if (ltc_state.substate == LTC_START_INIT_INITIALIZATION) {

                retVal = LTC_Init();  // Initialize main LTC loop
                ltc_state.lastsubstate = ltc_state.substate;
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;        // wait
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_EXIT_INITIALIZATION;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                }
            } else if (ltc_state.substate == LTC_EXIT_INITIALIZATION) {
            // in daisy-chain mode, there is no confirmation of the initialization
                LTC_SAVELASTSTATES();
                LTC_Initialize_Database();
                LTC_ResetErrorTable();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_INITIALIZED;
                ltc_state.substate = LTC_ENTRY_INITIALIZATION;
            }
            break;

        /****************************INITIALIZED*************************************/
        case LTC_STATEMACH_INITIALIZED:
            LTC_IF_INITIALIZED_CALLBACK();
            LTC_SAVELASTSTATES();
            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
            ltc_state.state = LTC_STATEMACH_IDLE;
            ltc_state.substate = LTC_ENTRY_INITIALIZED;
            ltc_state.ErrRetryCounter = 0;
            break;

        /****************************IDLE********************************************/
        case LTC_STATEMACH_IDLE:
            statereq = LTC_TransferStateRequest(&tmpbusID, &tmpadcMode, &tmpadcMeasCh);
            if (statereq == LTC_STATE_VOLTAGEMEASUREMENT_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_STARTMEAS;
                ltc_state.substate = LTC_ENTRY;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = tmpadcMode;
                ltc_state.adcMeasCh = LTC_ADCMEAS_ALLCHANNEL;
            } else if (statereq == LTC_STATE_VOLTAGEMEASUREMENT_2CELLS_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_STARTMEAS;
                ltc_state.substate = LTC_ENTRY;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = tmpadcMode;
                ltc_state.adcMeasCh = LTC_ADCMEAS_SINGLECHANNEL_TWOCELLS;
            } else if (statereq == LTC_STATE_READVOLTAGE_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_READVOLTAGE;
                ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_A_RDCVA_READVOLTAGE;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = tmpadcMode;
                ltc_state.adcMeasCh = LTC_ADCMEAS_ALLCHANNEL;
            } else if (statereq == LTC_STATE_READVOLTAGE_2CELLS_REQUEST) {

                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_READVOLTAGE_2CELLS;  // TODO: implementation of LTC_STATEMACH_READVOLTAGE_2CELLS
                ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_A_RDCVA_READVOLTAGE;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = tmpadcMode;
                ltc_state.adcMeasCh = LTC_ADCMEAS_SINGLECHANNEL_TWOCELLS;
            } else if (statereq == LTC_STATE_MUXMEASUREMENT_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_MUXMEASUREMENT_CONFIG;
                ltc_state.substate = LTC_SET_MUX_CHANNEL_WRCOMM_MUXMEASUREMENT_CONFIG;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = tmpadcMode;
                ltc_state.adcMeasCh = LTC_ADCMEAS_SINGLECHANNEL_GPIO1;  // TODO: check this
            } else if (statereq == LTC_STATE_ALLGPIOMEASUREMENT_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_ALLGPIOMEASUREMENT;
                ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_A_RAUXA_READALLGPIO;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = tmpadcMode;
                ltc_state.adcMeasCh = LTC_ADCMEAS_ALLCHANNEL;
            } else if (statereq == LTC_STATE_USER_IO_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_USER_IO_CONTROL;
                ltc_state.substate = LTC_USER_IO_SET_OUTPUT_REGISTER;
                ltc_state.ErrRetryCounter = 0;
            } else if (statereq == LTC_STATE_EEPROM_READ_UID_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_EEPROM_READ_UID;
                ltc_state.substate = LTC_EEPROM_READ_DATA1;
                ltc_state.ErrRetryCounter = 0;
            } else if (statereq == LTC_STATE_TEMP_SENS_READ_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_TEMP_SENS_READ;
                ltc_state.substate = LTC_TEMP_SENS_SEND_DATA1;
                ltc_state.ErrRetryCounter = 0;
            } else if (statereq == LTC_STATE_BALANCECONTROL_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_BALANCECONTROL;
                ltc_state.substate = LTC_CONFIG_BALANCECONTROL;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = LTC_ADCMODE_NORMAL_DCP0;            // needed for balancing feedback
                ltc_state.adcMeasCh = LTC_ADCMEAS_SINGLECHANNEL_GPIO3;  // needed for balancing feedback
#if defined(ITRI_MOD_2)
			} else if (statereq == LTC_STATE_EBMCONTROL_REQUEST) {

				LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_EBMCONTROL;
                ltc_state.substate = LTC_START_EBMCONTROL;
                LTC_ResetErrorTable();
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = LTC_ADCMODE_UNDEFINED;    // not needed for this state
                ltc_state.adcMeasCh = LTC_ADCMEAS_UNDEFINED;  // not needed for this state
#endif
            } else if (statereq == LTC_STATE_REINIT_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_INITIALIZATION;
                ltc_state.substate = LTC_ENTRY_INITIALIZATION;
                LTC_ResetErrorTable();
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = LTC_ADCMODE_UNDEFINED;    // not needed for this state
                ltc_state.adcMeasCh = LTC_ADCMEAS_UNDEFINED;  // not needed for this state
            } else if (statereq == LTC_STATE_IDLE_REQUEST) {

                // we stay already in requested state, nothing to do
            } else if (statereq == LTC_STATE_NO_REQUEST) {

                // no actual request pending
            } else {

                ltc_state.ErrRequestCounter++;   // illegal request pending
            }

            break;

        /****************************START MEASUREMENT*******************************/
        case LTC_STATEMACH_STARTMEAS:
#if defined(ITRI_MOD_6_i)
        	ltc_state.check_spi_flag = 2;	// cmd transfer time(callback) + ADC time(3ms)
#endif

            retVal = LTC_StartVoltageMeasurement(ltc_state.adcMode, ltc_state.adcMeasCh);
            ltc_cellvoltage.previous_timestamp = ltc_cellvoltage.timestamp;
            ltc_cellvoltage.timestamp = MCU_GetTimeStamp();

            LTC_SAVELASTSTATES();
            if ((retVal != E_OK)) {

                ++ltc_state.ErrRetryCounter;
                ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;        // wait
                if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                    ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                    ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                    ltc_state.substate = LTC_ERROR_ENTRY;
                    break;
                }
            } else {

                ltc_state.ErrRetryCounter = 0;
#if defined(ITRI_MOD_6_a)
                ltc_state.timer = ltc_state.commandTransferTime;
#else
                ltc_state.timer = ltc_state.commandTransferTime + LTC_Get_MeasurementTCycle(ltc_state.adcMode, ltc_state.adcMeasCh);
#endif
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
                ltc_state.ErrPECCounter = 0;
            }
            break;

        /****************************READ VOLTAGE************************************/
        case LTC_STATEMACH_READVOLTAGE:
            if (ltc_state.substate == LTC_READ_VOLTAGE_REGISTER_A_RDCVA_READVOLTAGE) {

#if defined(ITRI_MOD_6_i)
            	ltc_state.check_spi_flag = 1;
            	SPI_SetTransmitOngoing();
#endif

                LTC_SAVELASTSTATES();
                retVal = LTC_RX((uint8_t*)(ltc_cmdRDCVA), ltc_DataBufferSPI_RX_with_PEC_voltages);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {

                    ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_B_RDCVB_READVOLTAGE;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
#if defined(ITRI_MOD_6_i)
                    ltc_state.timer += 20;
#endif
                }
            } else if (ltc_state.substate == LTC_READ_VOLTAGE_REGISTER_B_RDCVB_READVOLTAGE) {

                if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_A_RDCVA_READVOLTAGE) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err(cnt:%u)\r\n", __FILE__, __LINE__, ltc_state.ErrPECCounter);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {

                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_A_RDCVA_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(0, ltc_DataBufferSPI_RX_with_PEC_voltages);
                }

#if defined(ITRI_MOD_6_i)
                ltc_state.check_spi_flag = 1;
            	SPI_SetTransmitOngoing();
#endif
                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDCVB, ltc_DataBufferSPI_RX_with_PEC_voltages);
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {

                    ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_C_RDCVC_READVOLTAGE;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
#if defined(ITRI_MOD_6_i)
                    ltc_state.timer += 10;
#endif
                }
            } else if (ltc_state.substate == LTC_READ_VOLTAGE_REGISTER_C_RDCVC_READVOLTAGE) {

                if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_B_RDCVB_READVOLTAGE) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {
                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_B_RDCVB_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(1, ltc_DataBufferSPI_RX_with_PEC_voltages);
                }

#if defined(ITRI_MOD_6_i)
                ltc_state.check_spi_flag = 1;
            	SPI_SetTransmitOngoing();
#endif
                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDCVC, ltc_DataBufferSPI_RX_with_PEC_voltages);
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {

                    ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_D_RDCVD_READVOLTAGE;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
#if defined(ITRI_MOD_6_i)
                    ltc_state.timer += 10;
#endif
                }
            } else if (ltc_state.substate == LTC_READ_VOLTAGE_REGISTER_D_RDCVD_READVOLTAGE) {

                if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_C_RDCVC_READVOLTAGE) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {

                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_C_RDCVC_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(2, ltc_DataBufferSPI_RX_with_PEC_voltages);
                }

#if defined(ITRI_MOD_6_i)
                ltc_state.check_spi_flag = 1;
            	SPI_SetTransmitOngoing();
#endif
                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDCVD, ltc_DataBufferSPI_RX_with_PEC_voltages);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {

                    if (BS_NR_OF_BAT_CELLS_PER_MODULE > 12) {
                        /* 15 or 18 cell IC is used */

                        ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_E_RDCVE_READVOLTAGE;
                    } else {
                        ltc_state.substate = LTC_EXIT_READVOLTAGE;
                    }
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
#if defined(ITRI_MOD_6_i)
                    ltc_state.timer += 10;
#endif
                }
            } else if (ltc_state.substate == LTC_READ_VOLTAGE_REGISTER_E_RDCVE_READVOLTAGE) {

                if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_D_RDCVD_READVOLTAGE) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {

                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_D_RDCVD_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(3, ltc_DataBufferSPI_RX_with_PEC_voltages);
                }

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDCVE, ltc_DataBufferSPI_RX_with_PEC_voltages);
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    if (BS_NR_OF_BAT_CELLS_PER_MODULE > 15) {
                        /* 18 cell IC is used */

                        ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_F_RDCVF_READVOLTAGE;
                    } else {
                        ltc_state.substate = LTC_EXIT_READVOLTAGE;
                    }
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                }
            } else if (ltc_state.substate == LTC_READ_VOLTAGE_REGISTER_F_RDCVF_READVOLTAGE) {

                if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_E_RDCVE_READVOLTAGE) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {
                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_E_RDCVE_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(4, ltc_DataBufferSPI_RX_with_PEC_voltages);
                }

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDCVF, ltc_DataBufferSPI_RX_with_PEC_voltages);
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.substate = LTC_EXIT_READVOLTAGE;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                }
            } else if (ltc_state.substate == LTC_EXIT_READVOLTAGE) {

                // 12 Cells
                if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_D_RDCVD_READVOLTAGE) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {

                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_D_RDCVD_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(3, ltc_DataBufferSPI_RX_with_PEC_voltages);
                }
                // 15 Cells
                if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_E_RDCVE_READVOLTAGE) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {

                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_E_RDCVE_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(4, ltc_DataBufferSPI_RX_with_PEC_voltages);
                } else if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_F_RDCVF_READVOLTAGE) {
                    // 18 Cells
                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {

                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_F_RDCVF_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(5, ltc_DataBufferSPI_RX_with_PEC_voltages);
                }

                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
#if defined(ITRI_MOD_6_i)
                ltc_state.check_spi_flag = 0;
                //ltc_state.timer = 0;
#endif
            }
            break;


        /****************************READ VOLTAGE 2 CELLS***********************/
        case LTC_STATEMACH_READVOLTAGE_2CELLS:      // TODO: Implement state
            if (ltc_state.substate == LTC_READ_VOLTAGE_REGISTER_A_RDCVA_READVOLTAGE) {

                LTC_SAVELASTSTATES();
                retVal = LTC_RX((uint8_t*)(ltc_cmdRDCVA), ltc_DataBufferSPI_RX_with_PEC_voltages);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.substate = LTC_EXIT_READVOLTAGE;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                }
            } else if (ltc_state.substate == LTC_EXIT_READVOLTAGE) {

                if (ltc_state.lastsubstate == LTC_READ_VOLTAGE_REGISTER_A_RDCVA_READVOLTAGE) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_voltages) != E_OK) {
#if defined(ITRI_MOD_2)
                    	DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {

                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_VOLTAGE_REGISTER_A_RDCVA_READVOLTAGE;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveRXtoVoltagebuffer(0, ltc_DataBufferSPI_RX_with_PEC_voltages);
                }

                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
            }
            break;

#if defined(ITRI_MOD_2)
		/****************************EBM CONTROL*********************************/
		case LTC_STATEMACH_EBMCONTROL:
			if (ltc_state.substate == LTC_START_EBMCONTROL) {
				LTC_SAVELASTSTATES();

#if defined(ITRI_MOD_6_e)
				if (1) {
#else
				if (ltc_ebm_cmd == LTC_EBM_EB_CTRL) {
#endif
#if defined(ITRI_MOD_6_i)
					ltc_state.check_spi_flag = 1;
					SPI_SetTransmitOngoing();
#endif
					retVal = LTC_EBM_SetEBState();
					ltc_ebm_cmd = LTC_EBM_NONE;
				}
#if defined(ITRI_MOD_9)
				if (ltc_ebm_cmd == LTC_EBM_EB_COL_CTRL) {
#if defined(ITRI_MOD_6_i)
					ltc_state.check_spi_flag = 3;
					SPI_SetTransmitOngoing();
#endif
					retVal = LTC_EBM_SetEBColState(1);	// TODO:
					//ltc_ebm_cmd = LTC_EBM_NONE;
				}
#endif

				if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }

                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    if (ltc_ebm_cmd != LTC_EBM_NONE) {
                    	ltc_state.substate = LTC_PROC1_EBMCONTROL;
#if defined(ITRI_MOD_6_i)
                    	ltc_state.timer += 10;
#endif
                    } else {
                    	ltc_state.substate = LTC_EXIT_EBMCONTROL;
#if defined(ITRI_MOD_6_i)
                    	ltc_state.timer += 10;
#endif
                    }
                }
			} else if (ltc_state.substate == LTC_PROC1_EBMCONTROL) {
				LTC_SAVELASTSTATES();

#if defined(ITRI_MOD_9)
				if (ltc_ebm_cmd == LTC_EBM_EB_COL_CTRL) {
#if defined(ITRI_MOD_6_i)
					ltc_state.check_spi_flag = 1;
					SPI_SetTransmitOngoing();
#endif

					retVal =  LTC_EBM_SetEBColState(0);	// TODO:
					ltc_ebm_force_update = 0;	// reset it
					//ltc_ebm_cmd = LTC_EBM_NONE;
				}
#endif

				if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }

                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    if (ltc_ebm_cmd != LTC_EBM_NONE) {
                    	ltc_state.substate = LTC_PROC2_EBMCONTROL;
#if defined(ITRI_MOD_6_i)
                    	ltc_state.timer += 10;
#endif
                    } else {
#if defined(ITRI_MOD_6_i)
                    	ltc_state.timer += 10;
#endif
                    	ltc_state.substate = LTC_EXIT_EBMCONTROL;
                    }
                }
			} else if (ltc_state.substate == LTC_PROC2_EBMCONTROL) {
				LTC_SAVELASTSTATES();

#if defined(ITRI_MOD_9)
				if (ltc_ebm_cmd == LTC_EBM_EB_COL_CTRL) {
#if defined(ITRI_MOD_6_i)
					ltc_state.check_spi_flag = 1;
					SPI_SetTransmitOngoing();
#endif

					retVal =  LTC_EBM_SetEBColState(0);	// TODO:
					ltc_ebm_cmd = LTC_EBM_NONE;
				}
#endif

				if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }

                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
#if defined(ITRI_MOD_6_i)
                    ltc_state.timer += 10;
#endif
                    ltc_state.substate = LTC_EXIT_EBMCONTROL;
                }
			} else if (ltc_state.substate == LTC_EXIT_EBMCONTROL) {
				ltc_ebm_cmd = LTC_EBM_NONE;

				ltc_state.timer = LTC_STATEMACH_SHORTTIME;        // wait 1ms
                ltc_state.ErrRetryCounter = 0;
                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
#if defined(ITRI_MOD_6_i)
                //ltc_state.timer = 0;
#endif
			}
			break;
#endif

        /****************************BALANCE CONTROL*********************************/
        case LTC_STATEMACH_BALANCECONTROL:
            if (ltc_state.substate == LTC_CONFIG_BALANCECONTROL) {

                LTC_SAVELASTSTATES();
                retVal = LTC_BalanceControl(0);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }

                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    if (BS_NR_OF_BAT_CELLS_PER_MODULE > 12) {
                        ltc_state.substate = LTC_CONFIG2_BALANCECONTROL;
                    } else {
                        ltc_state.substate = LTC_REQUEST_FEEDBACK_BALANCECONTROL;
                    }
                }
            } else if (ltc_state.substate == LTC_CONFIG2_BALANCECONTROL) {

                LTC_SAVELASTSTATES();
                retVal = LTC_BalanceControl(1);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }

                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_REQUEST_FEEDBACK_BALANCECONTROL;
                }
            } else if (ltc_state.substate == LTC_REQUEST_FEEDBACK_BALANCECONTROL) {

                LTC_SAVELASTSTATES();
                retVal = LTC_StartGPIOMeasurement(ltc_state.adcMode, ltc_state.adcMeasCh);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_READ_FEEDBACK_BALANCECONTROL;
                }
            } else if (ltc_state.substate == LTC_READ_FEEDBACK_BALANCECONTROL) {

                LTC_SAVELASTSTATES();
                retVal = LTC_RX((uint8_t*)ltc_cmdRDAUXA, ltc_DataBufferSPI_RX_with_PEC_temperatures);  // read AUXA register
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_SAVE_FEEDBACK_BALANCECONTROL;
                }
            } else if (ltc_state.substate == LTC_SAVE_FEEDBACK_BALANCECONTROL) {

                LTC_SAVELASTSTATES();
                if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_temperatures) != E_OK) {
#if defined(ITRI_MOD_2)
                    DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                    if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                        if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                            ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                            ltc_state.substate = LTC_ERROR_ENTRY;
                            break;
                        }
                    }
                }
                LTC_ResetErrorTable();
                LTC_SaveBalancingFeedback(ltc_DataBufferSPI_RX_with_PEC_temperatures);
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.substate = LTC_EXIT_BALANCECONTROL;
            } else if (ltc_state.substate == LTC_EXIT_BALANCECONTROL) {

                ltc_state.timer = LTC_STATEMACH_SHORTTIME;        // wait 1ms
                ltc_state.ErrRetryCounter = 0;
                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
            }
            break;


        /****************************USER IO CONFIGRATION****************************/
        case LTC_STATEMACH_USER_IO_CONTROL:
            if (ltc_state.substate == LTC_USER_IO_SET_OUTPUT_REGISTER) {

                // running light for demo only //////////////////////// FIXME: remove this after testing
                static uint8_t cntr = 0;
                uint8_t tmp;
                if (cntr%2 == 0) {
                    if (cntr < 8*2) {
                        tmp = ~ltc_user_io_control.value_out[0];
                        if (cntr != 0) tmp <<= 1;
                        tmp = ~tmp;
                        ltc_user_io_control.value_out[0] = tmp;
                        ltc_user_io_control.value_out[1] = tmp;
                        ltc_user_io_control.value_out[2] = tmp;
                        ltc_user_io_control.value_out[3] = tmp;
                        ltc_user_io_control.value_out[4] = tmp;
                        ltc_user_io_control.value_out[5] = tmp;
                        ltc_user_io_control.value_out[6] = tmp;
                        ltc_user_io_control.value_out[7] = tmp;
                        cntr++;
                    } else if (cntr < 15*2) {
                        tmp = ~ltc_user_io_control.value_out[0];
                        tmp >>= 1;
                        tmp = ~tmp;
                        ltc_user_io_control.value_out[0] = tmp;
                        ltc_user_io_control.value_out[1] = tmp;
                        ltc_user_io_control.value_out[2] = tmp;
                        ltc_user_io_control.value_out[3] = tmp;
                        ltc_user_io_control.value_out[4] = tmp;
                        ltc_user_io_control.value_out[5] = tmp;
                        ltc_user_io_control.value_out[6] = tmp;
                        ltc_user_io_control.value_out[7] = tmp;
                        cntr++;
                    } else {
                        cntr = 0;
                    }
                } else {
                    cntr++;
                }
                /////////////////////////////////////////////////////

                user_io_taskcycle = 1;    // FIXME: swacker: remove repeated wake-up function
                if (user_io_taskcycle == 0) {
                    retVal = LTC_SendWakeUp();
                } else {
                    retVal = LTC_SetPortExpander(ltc_DataBufferSPI_TX_user_io, ltc_DataBufferSPI_TX_with_PEC_user_io);
                }
                ltc_state.lastsubstate = ltc_state.substate;
                ltc_state.laststate = ltc_state.state;
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    if (user_io_taskcycle == 0) {
                        ltc_state.substate = LTC_USER_IO_SET_OUTPUT_REGISTER;   // send data twice before STCOMM (workaround for I2C issue)
                        user_io_taskcycle++;
                    } else {
                        ltc_state.substate = LTC_USER_IO_SEND_CLOCK_STCOMM;
                    }
                }
            } else if (ltc_state.substate == LTC_USER_IO_READ_INPUT_REGISTER) {

                retVal = LTC_Send_I2C_Command(ltc_DataBufferSPI_TX_user_io, ltc_DataBufferSPI_TX_with_PEC_user_io, (uint8_t*)ltc_I2CcmdPortExpander1);
                ltc_state.lastsubstate = ltc_state.substate;
                ltc_state.laststate = ltc_state.state;
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_USER_IO_SEND_CLOCK_STCOMM;
                }
            } else if (ltc_state.substate == LTC_USER_IO_SEND_CLOCK_STCOMM) {

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_I2CClock(ltc_DataBufferSPI_TX_ClockCycles, ltc_DataBufferSPI_TX_ClockCycles_with_PEC);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.gpioClocksTransferTime;
                    user_io_taskcycle++;
                    switch (user_io_taskcycle) {
                        case 2: ltc_state.substate = LTC_USER_IO_READ_INPUT_REGISTER; break;
                        case 3: ltc_state.substate = LTC_USER_IO_READ_I2C_TRANSMISSION_RESULT_RDCOMM; break;
                        default: ltc_state.substate = LTC_USER_IO_FINISHED; break;
                    }
                }
            } else if (ltc_state.substate == LTC_USER_IO_READ_I2C_TRANSMISSION_RESULT_RDCOMM) {

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDCOMM, ltc_DataBufferSPI_RX_with_PEC_user_io);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;        // wait for next step
                    ltc_state.substate = LTC_USER_IO_SAVE_DATA;
                }
            } else if (ltc_state.substate == LTC_USER_IO_SAVE_DATA) {

                LTC_PortExpanderSaveValues(ltc_DataBufferSPI_RX_with_PEC_user_io);
                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_USER_IO_FINISHED;
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.ErrRetryCounter = 0;
            } else if (ltc_state.substate == LTC_USER_IO_FINISHED) {

                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.ErrRetryCounter = 0;
                user_io_taskcycle = 0;
            }
            break;

        /****************************EEPROM READ UID*********************************/
        case LTC_STATEMACH_EEPROM_READ_UID:
            if (ltc_state.substate == LTC_EEPROM_READ_DATA1) {

                eeprom_taskcycle = 1;    // FIXME: remove repeated wake-up function
                if (eeprom_taskcycle == 0) {
                    retVal = LTC_SendWakeUp();
                } else {
                    retVal = LTC_Send_I2C_Command(ltc_DataBufferSPI_TX_eeprom, ltc_DataBufferSPI_TX_with_PEC_eeprom, (uint8_t*)ltc_I2CcmdEEPROM0);
                }
                ltc_state.lastsubstate = ltc_state.substate;
                ltc_state.laststate = ltc_state.state;
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    if (eeprom_taskcycle == 0) {
                        ltc_state.substate = LTC_EEPROM_READ_DATA1;   // send data twice before STCOMM (workaround for I2C issue)
                        eeprom_taskcycle++;
                    } else {
                        ltc_state.substate = LTC_EEPROM_SEND_CLOCK_STCOMM;
                    }
                }
            } else if (ltc_state.substate == LTC_EEPROM_READ_DATA2) {

                retVal = LTC_Send_I2C_Command(ltc_DataBufferSPI_TX_eeprom, ltc_DataBufferSPI_TX_with_PEC_eeprom, (uint8_t*)ltc_I2CcmdEEPROM1);
                ltc_state.lastsubstate = ltc_state.substate;
                ltc_state.laststate = ltc_state.state;
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_EEPROM_SEND_CLOCK_STCOMM;
                }
            } else if (ltc_state.substate == LTC_EEPROM_READ_DATA3) {

                retVal = LTC_Send_I2C_Command(ltc_DataBufferSPI_TX_eeprom, ltc_DataBufferSPI_TX_with_PEC_eeprom, (uint8_t*)ltc_I2CcmdEEPROM2);
                ltc_state.lastsubstate = ltc_state.substate;
                ltc_state.laststate = ltc_state.state;
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_EEPROM_SEND_CLOCK_STCOMM;
                }
            } else if (ltc_state.substate == LTC_EEPROM_SEND_CLOCK_STCOMM) {

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_I2CClock(ltc_DataBufferSPI_TX_ClockCycles, ltc_DataBufferSPI_TX_ClockCycles_with_PEC);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.gpioClocksTransferTime;
                    // read transmission result
                    ltc_state.substate = LTC_EEPROM_READ_I2C_TRANSMISSION_RESULT_RDCOMM;
                }
            } else if (ltc_state.substate == LTC_EEPROM_READ_I2C_TRANSMISSION_RESULT_RDCOMM) {

                ltc_state.lastsubstate = ltc_state.substate;
                if (eeprom_taskcycle == 0) {
                    retVal = E_OK;
                } else {
                    retVal = LTC_RX((uint8_t*)ltc_cmdRDCOMM, ltc_DataBufferSPI_RX_with_PEC_eeprom);
                }
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;        // wait for next step
                    ltc_state.substate = LTC_EEPROM_SAVE_UID;
                }
            } else if (ltc_state.substate == LTC_EEPROM_SAVE_UID) {

                ltc_state.lastsubstate = ltc_state.substate;
                ltc_state.ErrRetryCounter = 0;
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                eeprom_taskcycle++;
                switch (eeprom_taskcycle) {
                    case 2: ltc_state.substate = LTC_EEPROM_READ_DATA2; break;
                    case 3: LTC_EEPROMSaveUID(0, ltc_DataBufferSPI_RX_with_PEC_eeprom);
                            ltc_state.substate = LTC_EEPROM_READ_DATA3; break;
                    case 4: LTC_EEPROMSaveUID(1, ltc_DataBufferSPI_RX_with_PEC_eeprom);
                            ltc_state.substate = LTC_EEPROM_FINISHED; eeprom_taskcycle = 0; break;
                    default: ltc_state.substate = LTC_EEPROM_FINISHED; break;
                }
            } else if (ltc_state.substate == LTC_EEPROM_FINISHED) {

                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.ErrRetryCounter = 0;
                eeprom_taskcycle = 0;
            }
            break;

            /****************************BOARD TEMPERATURE SENSOR*********************************/
            case LTC_STATEMACH_TEMP_SENS_READ:

                if (ltc_state.substate == LTC_TEMP_SENS_SEND_DATA1) {

                    temp_sens_taskcycle = 1;    // FIXME: remove repeated wake-up function
                    if (temp_sens_taskcycle == 0) {
                        retVal = LTC_SendWakeUp();
                    } else {
                        retVal = LTC_Send_I2C_Command(ltc_DataBufferSPI_TX_tempsens, ltc_DataBufferSPI_TX_with_PEC_tempsens, (uint8_t*)ltc_I2CcmdTempSens0);
                    }
                    ltc_state.lastsubstate = ltc_state.substate;
                    ltc_state.laststate = ltc_state.state;
                    if (retVal != E_OK) {

                        ++(ltc_state.ErrRetryCounter);
                        ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                        if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                            ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                            ltc_state.substate = LTC_ERROR_ENTRY;
                            break;
                        }
                    } else {
                        ltc_state.timer = ltc_state.commandDataTransferTime;
                        ltc_state.ErrRetryCounter = 0;
                        if (temp_sens_taskcycle == 0) {
                            ltc_state.substate = LTC_TEMP_SENS_SEND_DATA1;   // send data twice before STCOMM (workaround for I2C issue)
                            temp_sens_taskcycle++;
                        } else {
                            ltc_state.substate = LTC_TEMP_SENS_SEND_CLOCK_STCOMM;
                        }
                    }
                } else if (ltc_state.substate == LTC_TEMP_SENS_READ_DATA1) {

                    retVal = LTC_Send_I2C_Command(ltc_DataBufferSPI_TX_tempsens, ltc_DataBufferSPI_TX_with_PEC_tempsens, (uint8_t*)ltc_I2CcmdTempSens1);
                    ltc_state.lastsubstate = ltc_state.substate;
                    ltc_state.laststate = ltc_state.state;
                    if (retVal != E_OK) {

                        ++(ltc_state.ErrRetryCounter);
                        ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                        if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                            ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                            ltc_state.substate = LTC_ERROR_ENTRY;
                            break;
                        }
                    } else {
                        ltc_state.timer = ltc_state.commandDataTransferTime;
                        ltc_state.ErrRetryCounter = 0;
                        ltc_state.substate = LTC_TEMP_SENS_SEND_CLOCK_STCOMM;
                    }
                } else if (ltc_state.substate == LTC_TEMP_SENS_SEND_CLOCK_STCOMM) {

                    ltc_state.lastsubstate = ltc_state.substate;
                    retVal = LTC_I2CClock(ltc_DataBufferSPI_TX_ClockCycles, ltc_DataBufferSPI_TX_ClockCycles_with_PEC);
                    if (retVal != E_OK) {

                        ++ltc_state.ErrRetryCounter;
                        ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                        if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                            ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                            ltc_state.substate = LTC_ERROR_ENTRY;
                            break;
                        }
                    } else {
                        ltc_state.ErrRetryCounter = 0;
                        ltc_state.timer = ltc_state.gpioClocksTransferTime;
                        temp_sens_taskcycle++;
                        switch (temp_sens_taskcycle) {
                            case 2: ltc_state.substate = LTC_TEMP_SENS_READ_DATA1; break;
                            case 3: ltc_state.substate = LTC_TEMP_SENS_READ_I2C_TRANSMISSION_RESULT_RDCOMM; break;
                            default: ltc_state.substate = LTC_TEMP_SENS_FINISHED; break;
                        }
                    }
                } else if (ltc_state.substate == LTC_TEMP_SENS_READ_I2C_TRANSMISSION_RESULT_RDCOMM) {

                    ltc_state.lastsubstate = ltc_state.substate;
                    retVal = LTC_RX((uint8_t*)ltc_cmdRDCOMM, ltc_DataBufferSPI_RX_with_PEC_tempsens);
                    if (retVal != E_OK) {

                        ++ltc_state.ErrRetryCounter;
                        ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                        if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                            ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                            ltc_state.substate = LTC_ERROR_ENTRY;
                            break;
                        }
                    } else {
                        ltc_state.ErrRetryCounter = 0;
                        ltc_state.timer = ltc_state.commandDataTransferTime;        // wait for next step
                        ltc_state.substate = LTC_TEMP_SENS_SAVE_TEMP;
                    }
                } else if (ltc_state.substate == LTC_TEMP_SENS_SAVE_TEMP) {

                    ltc_state.lastsubstate = ltc_state.substate;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                    LTC_TempSensSaveTemp(ltc_DataBufferSPI_RX_with_PEC_tempsens);
                    ltc_state.substate = LTC_TEMP_SENS_FINISHED;
                } else if (ltc_state.substate == LTC_TEMP_SENS_FINISHED) {

                    LTC_SAVELASTSTATES();
                    ltc_state.substate = LTC_ENTRY;
                    ltc_state.state = LTC_STATEMACH_IDLE;
                    ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                    ltc_state.ErrRetryCounter = 0;
                    temp_sens_taskcycle = 0;
                }
                break;

        /****************************START MULTIPLEXED MEASUREMENT*******************/
        case LTC_STATEMACH_STARTMUXMEASUREMENT:

            // first mux sequence pointer initialization was made during the initialization state
            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
            LTC_SAVELASTSTATES();
            ltc_state.substate = LTC_SET_MUX_CHANNEL_WRCOMM_MUXMEASUREMENT_CONFIG;
            ltc_state.state = LTC_STATEMACH_MUXMEASUREMENT_CONFIG;
            ltc_state.ErrRetryCounter = 0;

            break;

        /****************************MULTIPLEXED MEASUREMENT CONFIGURATION***********/
        case LTC_STATEMACH_MUXMEASUREMENT_CONFIG:
            if (ltc_state.muxmeas_seqptr >= ltc_state.muxmeas_seqendptr) {
                // last step of sequence reached (or no sequence configured)
                // mux sequence re-initialized (if necessary) in sub-state LTC_SAVE_MUX_MEASUREMENT_MUXMEASUREMENT
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_MUXMEASUREMENT_FINISHED;
                ltc_state.ErrRetryCounter = 0;
                break;
            }

            if (ltc_state.substate == LTC_SET_MUX_CHANNEL_WRCOMM_MUXMEASUREMENT_CONFIG) {

                retVal = LTC_SetMuxChannel(ltc_DataBufferSPI_TX_temperatures, ltc_DataBufferSPI_TX_with_PEC_temperatures,
                                            ltc_state.muxmeas_seqptr->muxID,  /* mux */
                                            ltc_state.muxmeas_seqptr->muxCh  /* channel */);
                ltc_state.lastsubstate = ltc_state.substate;
                ltc_state.laststate = ltc_state.state;
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.substate = LTC_SEND_CLOCK_STCOMM_MUXMEASUREMENT_CONFIG;
                }
            } else if (ltc_state.substate == LTC_SEND_CLOCK_STCOMM_MUXMEASUREMENT_CONFIG) {

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_I2CClock(ltc_DataBufferSPI_TX_ClockCycles, ltc_DataBufferSPI_TX_ClockCycles_with_PEC);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.gpioClocksTransferTime;
                    if (LTC_READCOM == 1)
                        ltc_state.substate = LTC_READ_I2C_TRANSMISSION_RESULT_RDCOMM_MUXMEASUREMENT_CONFIG;
                    else
                        ltc_state.substate = LTC_START_GPIO_MEASUREMENT_MUXMEASUREMENT_CONFIG;
                }
            } else if (ltc_state.substate == LTC_READ_I2C_TRANSMISSION_RESULT_RDCOMM_MUXMEASUREMENT_CONFIG) {

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDCOMM, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;        // wait for next step
                    ltc_state.substate = LTC_START_GPIO_MEASUREMENT_MUXMEASUREMENT_CONFIG;
                }

            } else if (ltc_state.substate == LTC_START_GPIO_MEASUREMENT_MUXMEASUREMENT_CONFIG) {

                if (ltc_state.lastsubstate == LTC_READ_I2C_TRANSMISSION_RESULT_RDCOMM_MUXMEASUREMENT_CONFIG) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_temperatures) != E_OK) {
#if defined(ITRI_MOD_2)
                    	DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {
                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_I2C_TRANSMISSION_RESULT_RDCOMM_MUXMEASUREMENT_CONFIG;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrPECCounter = 0;
                    mux_error = LTC_I2CCheckACK(ltc_DataBufferSPI_RX_with_PEC_temperatures, ltc_state.muxmeas_seqptr->muxID);

                    if (mux_error != 0) {
                        if (LTC_DISCARD_MUX_CHECK == FALSE) {

                            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                            ltc_state.state = LTC_STATEMACH_ERROR_MUXFAILED;
                            ltc_state.substate = LTC_ERROR_ENTRY;
                            break;
                        }
                    }
                }

                ltc_state.lastsubstate = ltc_state.substate;

                if (ltc_state.muxmeas_seqptr->muxCh == 0xFF) {
                    // actual multiplexer is switched off, so do not make a measurement and follow up with next step (mux configuration)
                    ++ltc_state.muxmeas_seqptr;         // go further with next step of sequence
                                                        // ltc_state.numberOfMeasuredMux not decremented, this does not count as a measurement */
                    ltc_state.lastsubstate = ltc_state.substate;
                    ltc_state.substate = LTC_SET_MUX_CHANNEL_WRCOMM_MUXMEASUREMENT_CONFIG;
                    ltc_state.state = LTC_STATEMACH_MUXMEASUREMENT_CONFIG;
                    ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                    ltc_state.ErrRetryCounter = 0;
                    break;
                } else {
                    // user multiplexer type -> connected to GPIO2!
                    if (ltc_state.muxmeas_seqptr->muxID == 1 || ltc_state.muxmeas_seqptr->muxID == 2) {
                        retVal = LTC_StartGPIOMeasurement(ltc_state.adcMode, LTC_ADCMEAS_SINGLECHANNEL_GPIO2);
                    } else {
                        retVal = LTC_StartGPIOMeasurement(ltc_state.adcMode, LTC_ADCMEAS_SINGLECHANNEL_GPIO1);
                    }
                    if (retVal != E_OK) {

                        ++ltc_state.ErrRetryCounter;
                        ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                        if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                            ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                            ltc_state.substate = LTC_ERROR_ENTRY;
                            break;
                        }
                    } else {
                        LTC_SAVELASTSTATES();
                        ltc_state.state = LTC_STATEMACH_MUXMEASUREMENT;
                        ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_A_RAUXA_MUXMEASUREMENT;
                        ltc_state.timer = ltc_state.commandTransferTime + LTC_Get_MeasurementTCycle(ltc_state.adcMode, ltc_state.adcMeasCh);  // wait, ADAX-Command
                        ltc_state.ErrRetryCounter = 0;
                    }
                }
            } else {
                // state sequence error
                LTC_SAVELASTSTATES();
            }
            break;

        /****************************MULTIPLEXED MEASUREMENT*************************/
        case LTC_STATEMACH_MUXMEASUREMENT:
            if (ltc_state.substate == LTC_READ_AUXILIARY_REGISTER_A_RAUXA_MUXMEASUREMENT) {

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDAUXA, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                if (retVal != E_OK) {

                    ++ltc_state.ErrRetryCounter;
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                    ltc_state.substate = LTC_SAVE_MUX_MEASUREMENT_MUXMEASUREMENT;
                }
            } else if (ltc_state.substate == LTC_SAVE_MUX_MEASUREMENT_MUXMEASUREMENT) {

                ltc_state.lastsubstate = ltc_state.substate;
                if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_temperatures) != E_OK) {
#if defined(ITRI_MOD_2)
                    DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                    if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                        if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                            ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                            ltc_state.substate = LTC_ERROR_ENTRY;
                            break;
                        }
                    }
                }
                LTC_ResetErrorTable();
                LTC_SaveMuxMeasurement(ltc_DataBufferSPI_RX_with_PEC_temperatures, ltc_state.muxmeas_seqptr);

                ++(ltc_state.muxmeas_seqptr);        // go further with next step of sequence
                --(ltc_state.numberOfMeasuredMux);
                if (ltc_state.muxmeas_seqptr >= ltc_state.muxmeas_seqendptr) {
                    // last step of sequence reached
                    // The mux sequence starts again
                    ltc_state.ltc_muxcycle_finished = E_OK;

                    ltc_state.muxmeas_seqptr = ltc_mux_seq.seqptr;
                    ltc_state.muxmeas_nr_end = ltc_mux_seq.nr_of_steps;
                    ltc_state.muxmeas_seqendptr = ((LTC_MUX_CH_CFG_s *)ltc_mux_seq.seqptr)+ltc_mux_seq.nr_of_steps;  // last sequence + 1

                    ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                    LTC_SAVELASTSTATES();
                    ltc_state.substate = LTC_ENTRY;
                    ltc_state.state = LTC_STATEMACH_IDLE;
                } else if (ltc_state.numberOfMeasuredMux == 0) {

                    // number of multiplexers reached for this cycle, but multiplexer sequence not finished yet
                    ltc_state.ltc_muxcycle_finished = E_NOT_OK;
                    ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                    LTC_SAVELASTSTATES();
                    ltc_state.substate = LTC_ENTRY;
                    ltc_state.state = LTC_STATEMACH_IDLE;
                } else {
                    LTC_SAVELASTSTATES();
                    ltc_state.substate = LTC_SET_MUX_CHANNEL_WRCOMM_MUXMEASUREMENT_CONFIG;
                    ltc_state.state = LTC_STATEMACH_MUXMEASUREMENT_CONFIG;
                    ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                }
                ltc_state.ErrRetryCounter = 0;
                ltc_state.ErrPECCounter = 0;
                break;

            } else {
                // state sequence error
                LTC_SAVELASTSTATES();
            }
            break;

        /****************************MULTIPLEXER MEASUREMENT FINISHED****************/
        case LTC_STATEMACH_MUXMEASUREMENT_FINISHED:
                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.ErrRetryCounter = 0;
            break;

        /****************************MEASUREMENT OF ALL GPIOS************************/
        case LTC_STATEMACH_ALLGPIOMEASUREMENT:
        	//DEBUG_PRINTF_EX("[INFO]LTC_STATEMACH_ALLGPIOMEASUREMENT\r\n");
#if defined(ITRI_MOD_6_i)
			ltc_state.check_spi_flag = 2;
			SPI_SetTransmitOngoing();
#endif
            retVal = LTC_StartGPIOMeasurement(ltc_state.adcMode, ltc_state.adcMeasCh);
            LTC_SAVELASTSTATES();
            if ((retVal != E_OK)) {

                ++(ltc_state.ErrRetryCounter);
                ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;    // wait
                if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                    ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                    ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                    ltc_state.substate = LTC_ERROR_ENTRY;
                    break;
                }
            } else {
                ltc_state.ErrRetryCounter = 0;
#if defined(ITRI_MOD_6_a)
                ltc_state.timer = ltc_state.commandTransferTime;
#else
                ltc_state.timer = ltc_state.commandTransferTime + LTC_Get_MeasurementTCycle(ltc_state.adcMode, ltc_state.adcMeasCh);
#endif
#if defined(ITRI_MOD_6_i)
                ltc_state.timer += 10;
#endif
                ltc_state.state = LTC_STATEMACH_READALLGPIO;
                ltc_state.ErrPECCounter = 0;
            }
            break;

        /****************************MEASUREMENT OF ALL GPIOS************************/
        // used in case there are no multiplexers, measurement of all GPIOs
        case LTC_STATEMACH_READALLGPIO:
            if (ltc_state.substate == LTC_READ_AUXILIARY_REGISTER_A_RAUXA_READALLGPIO) {
            	//DEBUG_PRINTF_EX("[INFO]LTC_READ_AUXILIARY_REGISTER_A_RAUXA_READALLGPIO\r\n");
#if defined(ITRI_MOD_6_i)
				ltc_state.check_spi_flag = 1;
				SPI_SetTransmitOngoing();
#endif

                LTC_SAVELASTSTATES();
                retVal = LTC_RX((uint8_t*)ltc_cmdRDAUXA, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                if (retVal != E_OK) {

                	DEBUG_PRINTF_EX("[ERR]LTC_READ_AUXILIARY_REGISTER_A_RAUXA_READALLGPIO ret err!!!\r\n");
                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_B_RAUXB_READALLGPIO;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
#if defined(ITRI_MOD_6_i)
                    ltc_state.timer += 10;
#endif
                }
            } else if (ltc_state.substate == LTC_READ_AUXILIARY_REGISTER_B_RAUXB_READALLGPIO) {
            	//DEBUG_PRINTF_EX("[INFO]LTC_READ_AUXILIARY_REGISTER_B_RAUXB_READALLGPIO\r\n");

                if (ltc_state.lastsubstate == LTC_READ_AUXILIARY_REGISTER_A_RAUXA_READALLGPIO) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_temperatures) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {
                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_A_RAUXA_READALLGPIO;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveGPIOMeasurement(0, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                }

#if defined(ITRI_MOD_6_i)
				ltc_state.check_spi_flag = 1;
				SPI_SetTransmitOngoing();
#endif
                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDAUXB, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                if (retVal != E_OK) {
                	DEBUG_PRINTF_EX("[ERR]LTC_READ_AUXILIARY_REGISTER_B_RAUXB_READALLGPIO ret err!!!\r\n");

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    if ( BS_NR_OF_BAT_CELLS_PER_MODULE > 12 ) {
                        /* 15 or 18 cell IC is used */

                        ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_C_RAUXC_READALLGPIO;
                    } else {
                        ltc_state.substate = LTC_EXIT_READALLGPIO;
                    }
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
#if defined(ITRI_MOD_6_i)
                    ltc_state.timer += 10;
#endif
                }

            } else if (ltc_state.substate == LTC_READ_AUXILIARY_REGISTER_C_RAUXC_READALLGPIO) {
            	//DEBUG_PRINTF_EX("[INFO]LTC_READ_AUXILIARY_REGISTER_C_RAUXC_READALLGPIO\r\n");

                if (ltc_state.lastsubstate == LTC_READ_AUXILIARY_REGISTER_B_RAUXB_READALLGPIO) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_temperatures) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {
                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_B_RAUXB_READALLGPIO;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveGPIOMeasurement(1, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                }

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDAUXC, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_D_RAUXD_READALLGPIO;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                }

            } else if (ltc_state.substate == LTC_READ_AUXILIARY_REGISTER_D_RAUXD_READALLGPIO) {

                if (ltc_state.lastsubstate == LTC_READ_AUXILIARY_REGISTER_C_RAUXC_READALLGPIO) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_temperatures) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {
                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_C_RAUXC_READALLGPIO;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveGPIOMeasurement(2, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                }

                ltc_state.lastsubstate = ltc_state.substate;
                retVal = LTC_RX((uint8_t*)ltc_cmdRDAUXD, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                if (retVal != E_OK) {

                    ++(ltc_state.ErrRetryCounter);
                    ltc_state.timer = LTC_STATEMACH_SEQERRTTIME;
                    if (ltc_state.ErrRetryCounter > LTC_TRANSMIT_SPIERRLIMIT) {

                        ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                        ltc_state.state = LTC_STATEMACH_ERROR_SPIFAILED;
                        ltc_state.substate = LTC_ERROR_ENTRY;
                        break;
                    }
                } else {
                    ltc_state.substate = LTC_EXIT_READALLGPIO;
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.timer = ltc_state.commandDataTransferTime;
                }
            } else if (ltc_state.substate == LTC_EXIT_READALLGPIO) {
            	//DEBUG_PRINTF_EX("[INFO]LTC_EXIT_READALLGPIO\r\n");

                // 12-Cell
                if (ltc_state.lastsubstate == LTC_READ_AUXILIARY_REGISTER_B_RAUXB_READALLGPIO) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_temperatures) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {
                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_B_RAUXB_READALLGPIO;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveGPIOMeasurement(1, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                }
                // 15/18-Cell
                if (ltc_state.lastsubstate == LTC_READ_AUXILIARY_REGISTER_D_RAUXD_READALLGPIO) {

                    if (LTC_RX_PECCheck(ltc_DataBufferSPI_RX_with_PEC_temperatures) != E_OK) {
#if defined(ITRI_MOD_2)
                    	//DEBUG_PRINTF_EX("[%s:%d:WARN]PEC err\r\n", __FILE__, __LINE__);
#endif
                        if (++ltc_state.ErrPECCounter > LTC_TRANSMIT_PECERRLIMIT) {

                            if (LTC_GOTO_PEC_ERROR_STATE == TRUE) {

                                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                                ltc_state.state = LTC_STATEMACH_ERROR_PECFAILED;
                                ltc_state.substate = LTC_ERROR_ENTRY;
                                break;
                            }
                        } else {
                            ltc_state.lastsubstate = ltc_state.substate;
                            ltc_state.substate = LTC_READ_AUXILIARY_REGISTER_D_RAUXD_READALLGPIO;
                            ltc_state.timer = LTC_STATEMACH_PECERRTIME;
                            break;
                        }
                    }
                    LTC_ResetErrorTable();
                    ltc_state.ErrRetryCounter = 0;
                    ltc_state.ErrPECCounter = 0;
                    LTC_SaveGPIOMeasurement(3, ltc_DataBufferSPI_RX_with_PEC_temperatures);
                }

                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                LTC_SAVELASTSTATES();
                ltc_state.substate = LTC_ENTRY;
                ltc_state.state = LTC_STATEMACH_IDLE;
#if defined(ITRI_MOD_6_i)
                //ltc_state.timer = 0;
#endif

            }
            break;

        /****************************SPI ERROR***************************************/
        case LTC_STATEMACH_ERROR_SPIFAILED:
            if (ltc_state.substate == LTC_ERROR_ENTRY) {

                DIAG_Handler(DIAG_CH_LTC_SPI,DIAG_EVENT_NOK, 0, NULL_PTR);
                ltc_state.substate =  LTC_ERROR_PROCESSED;
            }

            statereq = LTC_TransferStateRequest(&tmpbusID, &tmpadcMode, &tmpadcMeasCh);
            if (statereq == LTC_STATE_REINIT_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_INITIALIZATION;
                ltc_state.substate = LTC_ENTRY_INITIALIZATION;
                LTC_ResetErrorTable();
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = LTC_ADCMODE_UNDEFINED;    // not needed for this state
                ltc_state.adcMeasCh = LTC_ADCMEAS_UNDEFINED;  // not needed for this state
            }
            ltc_state.timer = LTC_STATEMACH_SHORTTIME;

            break;

        /****************************PEC ERROR***************************************/
        case LTC_STATEMACH_ERROR_PECFAILED:
            if (ltc_state.substate == LTC_ERROR_ENTRY) {

                DIAG_Handler(DIAG_CH_LTC_PEC, DIAG_EVENT_NOK, 0, NULL_PTR);
                ltc_state.substate =  LTC_ERROR_PROCESSED;
            }

            statereq = LTC_TransferStateRequest(&tmpbusID, &tmpadcMode, &tmpadcMeasCh);
            if (statereq == LTC_STATE_REINIT_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_INITIALIZATION;
                ltc_state.substate = LTC_ENTRY_INITIALIZATION;
                LTC_ResetErrorTable();
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = LTC_ADCMODE_UNDEFINED;    // not needed for this state
                ltc_state.adcMeasCh = LTC_ADCMEAS_UNDEFINED;  // not needed for this state
            }
            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
            break;

        case LTC_STATEMACH_ERROR_MUXFAILED:
            if (ltc_state.substate == LTC_ERROR_ENTRY) {

                DIAG_Handler(DIAG_CH_LTC_MUX, DIAG_EVENT_NOK, 0, NULL_PTR);
                ltc_state.substate = LTC_ERROR_PROCESSED;
            }

            statereq = LTC_TransferStateRequest(&tmpbusID, &tmpadcMode, &tmpadcMeasCh);
            if (statereq == LTC_STATE_REINIT_REQUEST) {

                LTC_SAVELASTSTATES();
                ltc_state.timer = LTC_STATEMACH_SHORTTIME;
                ltc_state.state = LTC_STATEMACH_INITIALIZATION;
                ltc_state.substate = LTC_ENTRY_INITIALIZATION;
                LTC_ResetErrorTable();
                ltc_state.ErrRetryCounter = 0;
                ltc_state.adcMode = LTC_ADCMODE_UNDEFINED;    // not needed for this state
                ltc_state.adcMeasCh = LTC_ADCMEAS_UNDEFINED;  // not needed for this state
            }
            ltc_state.timer = LTC_STATEMACH_SHORTTIME;
            break;

        default:
            break;
    }

    ltc_state.triggerentry--;        // reentrance counter
}



/**
 * @brief   saves the multiplexer values read from the LTC daisy-chain.
 *
 * After a voltage measurement was initiated on GPIO 1 to read the currently selected
 * multiplexer voltage, the results is read via SPI from the daisy-chain.
 * This function is called to store the result from the transmission in a buffer.
 *
 * @param   *DataBufferSPI_RX   buffer containing the data obtained from the SPI transmission
 * @param   muxseqptr           pointer to the multiplexer sequence, which configures the currently selected multiplexer ID and channel
 *
 * @return  void
 */
static void LTC_SaveMuxMeasurement(uint8_t *DataBufferSPI_RX, LTC_MUX_CH_CFG_s  *muxseqptr) {   // pointer to measurement Sequence of Mux- and Channel-Configurations (1,0xFF)...(3,0xFF),(0,1),...(0,7))

    uint16_t i = 0;

    if (muxseqptr->muxCh == 0xFF)
        return; /* Channel 0xFF means that the multiplexer is deactivated, therefore no measurement will be made and saved*/

    // user multiplexer type -> connected to GPIO2!
    if (muxseqptr->muxID == 1 || muxseqptr->muxID == 2) {
        for (i=0; i < LTC_N_LTC; i++) {
            LTC_MultiplexerVoltages[2*(i*LTC_N_MUX_CHANNELS_PER_LTC+muxseqptr->muxID*LTC_N_MUX_CHANNELS_PER_MUX+muxseqptr->muxCh)] = DataBufferSPI_RX[6+1*i*8];      // raw values, all multiplexers on all LTCs
            LTC_MultiplexerVoltages[2*(i*LTC_N_MUX_CHANNELS_PER_LTC+muxseqptr->muxID*LTC_N_MUX_CHANNELS_PER_MUX+muxseqptr->muxCh)+1] = DataBufferSPI_RX[6+1*i*8+1];  // raw values, all multiplexers on all LTCs
        }
    } else {
        // temperature multiplexer type -> connected to GPIO1!
        for (i=0; i < LTC_N_LTC; i++) {
            LTC_MultiplexerVoltages[2*(i*LTC_N_MUX_CHANNELS_PER_LTC+muxseqptr->muxID*LTC_N_MUX_CHANNELS_PER_MUX+muxseqptr->muxCh)] = DataBufferSPI_RX[4+1*i*8];      // raw values, all multiplexers on all LTCs
            LTC_MultiplexerVoltages[2*(i*LTC_N_MUX_CHANNELS_PER_LTC+muxseqptr->muxID*LTC_N_MUX_CHANNELS_PER_MUX+muxseqptr->muxCh)+1] = DataBufferSPI_RX[4+1*i*8+1];  // raw values, all multiplexers on all LTCs
        }
    }
}


/**
 * @brief   saves the GPIO voltage values read from the LTC daisy-chain.
 *
 * After a voltage measurement was initiated to measure the voltages on all GPIOs,
 * the result is read via SPI from the daisy-chain. In order to read the result of all GPIO measurements,
 * it is necessary to read auxiliary register A and B.
 * Only one register can be read at a time.
 * This function is called to store the result from the transmission in a buffer.
 *
 * @param   registerSet    voltage register that was read (auxiliary register A or B)
 * @param   *rxBuffer      buffer containing the data obtained from the SPI transmission
 *
 * @return  void
 *
 */
static void LTC_SaveGPIOMeasurement(uint8_t registerSet, uint8_t *rxBuffer) {

    uint16_t i = 0;
    uint8_t i_offset = 0;

    if (registerSet == 0) {
    // RDAUXA command -> GPIO register group A
        i_offset = 0;
    } else if (registerSet == 1) {
    // RDAUXB command -> GPIO register group B
        i_offset = 6;
    } else if (registerSet == 2) {
    // RDAUXC command -> GPIO register group c
        i_offset = 12;
    } else if (registerSet == 3) {
    // RDAUXD command -> GPIO register group D
        i_offset = 18;
    } else {
        return;
    }

    /* Retrieve data without command and CRC*/

    #if BS_NR_OF_BAT_CELLS_PER_MODULE > 12
    for (i=0; i < LTC_N_LTC; i++) {
        LTC_GPIOVoltages[0+i_offset+24*i]= rxBuffer[4+i*8];
        LTC_GPIOVoltages[1+i_offset+24*i]= rxBuffer[5+i*8];
        LTC_GPIOVoltages[2+i_offset+24*i]= rxBuffer[6+i*8];
        LTC_GPIOVoltages[3+i_offset+24*i]= rxBuffer[7+i*8];
        LTC_GPIOVoltages[4+i_offset+24*i]= rxBuffer[8+i*8];
        LTC_GPIOVoltages[5+i_offset+24*i]= rxBuffer[9+i*8];

    }
    #else
    for (i=0; i < LTC_N_LTC; i++) {
        LTC_GPIOVoltages[0+i_offset+12*i]= rxBuffer[4+i*8];
        LTC_GPIOVoltages[1+i_offset+12*i]= rxBuffer[5+i*8];
        //DEBUG_PRINTF_EX("[TN]trace registerSet:%u [4]:0x%x [5]:0x%x\r\n", registerSet, rxBuffer[4+i*8], rxBuffer[5+i*8]);
        LTC_GPIOVoltages[2+i_offset+12*i]= rxBuffer[6+i*8];
        LTC_GPIOVoltages[3+i_offset+12*i]= rxBuffer[7+i*8];
        LTC_GPIOVoltages[4+i_offset+12*i]= rxBuffer[8+i*8];
        LTC_GPIOVoltages[5+i_offset+12*i]= rxBuffer[9+i*8];
    }
    #endif
}

/**
 * @brief   saves the voltage values read from the LTC daisy-chain.
 *
 * After a voltage measurement was initiated to measure the voltages of the cells,
 * the result is read via SPI from the daisy-chain.
 * There are 6 register to read _(A,B,C,D,E,F) to get all cell voltages.
 * Only one register can be read at a time.
 * This function is called to store the result from the transmission in a buffer.
 *
 * @param   registerSet    voltage register that was read (voltage register A,B,C,D,E or F)
 * @param   *rxBuffer      buffer containing the data obtained from the SPI transmission
 *
 * @return  void
 *
 */
static void LTC_SaveRXtoVoltagebuffer(uint8_t registerSet, uint8_t *rxBuffer) {

    uint16_t i = 0;
    uint8_t i_offset = 0;

#if defined(ITRI_MOD_3)
    uint8_t regSetNum = (uint8_t)((BS_NR_OF_BAT_CELLS_PER_MODULE+2) / 3);
    uint16_t j = 0, cellNum = 3;
    if (registerSet >= regSetNum)		return;
    if ((registerSet+1) == regSetNum)	cellNum = BS_NR_OF_BAT_CELLS_PER_MODULE % 3;
#endif

    if (registerSet == 0) {
    // RDCVA command -> voltage register group A
        i_offset = 0;
    } else if (registerSet == 1) {
    // RDCVB command -> voltage register group B
        i_offset = 6;
    } else if (registerSet == 2) {
    // RDCVC command -> voltage register group C
        i_offset = 12;
    } else if (registerSet == 3) {
    // RDCVD command -> voltage register group D
        i_offset = 18;
    } else if (registerSet == 4) {
    // RDCVD command -> voltage register group E (only for 15 and 18 cell version)
        i_offset = 24;
    } else if (registerSet == 5) {
    // RDCVD command -> voltage register group F (only for 18 cell version)
        i_offset = 30;
    } else {
        return;
    }

    /* Retrieve data without command and CRC*/
    for (i=0; i < LTC_N_LTC; i++) {
#if defined(ITRI_MOD_3)
    	for (j=0; j < cellNum; j++) {
    		LTC_CellVoltages[(0+2*j)+i_offset+2*BS_NR_OF_BAT_CELLS_PER_MODULE*i]= rxBuffer[(4+2*j)+i*8];
    		LTC_CellVoltages[(1+2*j)+i_offset+2*BS_NR_OF_BAT_CELLS_PER_MODULE*i]= rxBuffer[(5+2*j)+i*8];
    	}
#else
        LTC_CellVoltages[0+i_offset+2*BS_NR_OF_BAT_CELLS_PER_MODULE*i]= rxBuffer[4+i*8];
        LTC_CellVoltages[1+i_offset+2*BS_NR_OF_BAT_CELLS_PER_MODULE*i]= rxBuffer[5+i*8];
        LTC_CellVoltages[2+i_offset+2*BS_NR_OF_BAT_CELLS_PER_MODULE*i]= rxBuffer[6+i*8];
        LTC_CellVoltages[3+i_offset+2*BS_NR_OF_BAT_CELLS_PER_MODULE*i]= rxBuffer[7+i*8];
        LTC_CellVoltages[4+i_offset+2*BS_NR_OF_BAT_CELLS_PER_MODULE*i]= rxBuffer[8+i*8];
        LTC_CellVoltages[5+i_offset+2*BS_NR_OF_BAT_CELLS_PER_MODULE*i]= rxBuffer[9+i*8];
#endif
    }
}


/**
 * @brief   checks if the multiplexers acknowledged transmission.
 *
 * The RDCOMM command can be used to read the answer of the multiplexers to a
 * I2C transmission.
 * This function determines if the communication with the multiplexers was
 * successful or not.
 * The array LTC_ErrorTable is updated to locate the multiplexers that did not
 * acknowledge transmission.
 *
 * @param   *DataBufferSPI_RX    data obtained from the SPI transmission
 * @param   mux                  multiplexer to be addressed (multiplexer ID)
 *
 * @return  mux_error            0 is there was no error, 1 if there was errors
 */
static uint8_t LTC_I2CCheckACK(uint8_t *DataBufferSPI_RX, int mux) {
    uint8_t mux_error = 0;
    uint16_t i = 0;

    for (i=0; i < BS_NR_OF_MODULES; i++) {
        if (mux == 0) {
            if ((DataBufferSPI_RX[4+1+LTC_NUMBER_OF_LTC_PER_MODULE*i*8] & 0x0F) != 0x07) {    // ACK = 0xX7
                LTC_ErrorTable[i].mux0 = 1;
                mux_error = 1;
            }
        }
        if (mux == 1) {
            if ((DataBufferSPI_RX[4+1+LTC_NUMBER_OF_LTC_PER_MODULE*i*8] & 0x0F) != 0x07) {
                LTC_ErrorTable[i].mux1 = 1;
                mux_error = 1;
            }
        }
        if (mux == 2) {
            if ((DataBufferSPI_RX[4+1+LTC_NUMBER_OF_LTC_PER_MODULE*i*8] & 0x0F) != 0x47) {
                LTC_ErrorTable[i].mux2 = 1;
                mux_error = 1;
            }
        }
        if (mux == 3) {
            if ((DataBufferSPI_RX[4+1+LTC_NUMBER_OF_LTC_PER_MODULE*i*8] & 0x0F) != 0x07) {
                LTC_ErrorTable[i].mux3 = 1;
                mux_error = 1;
            }
        }
    }

    return mux_error;
}



/*
 * @brief   initialize the daisy-chain.
 *
 * To initialize the LTC6804 daisy-chain, a dummy byte (0x00) is sent.
 *
 * @return  retVal  E_OK if dummy byte was sent correctly by SPI, E_NOT_OK otherwise
 *
 */
static STD_RETURN_TYPE_e LTC_Init(void) {

    STD_RETURN_TYPE_e statusSPI = E_NOT_OK;
    STD_RETURN_TYPE_e retVal = E_OK;

    uint8_t PEC_Check[6];
    uint16_t PEC_result = 0;
    uint16_t i = 0;


#if defined(ITRI_MOD_2)
    for (i=0; i < BS_NR_OF_MODULES; i++) {
    	ltc_ebm_cali[i].isCali = LTC_EBM_MAX_CURR_CAL_CNT;
    	ltc_ebm_cali[i].curBat_offset = 0;
    	ltc_ebm_cali[i].curMod_offset = 0;
    }
#endif

    // The transfer-time is set according to the length of the daisy-chain
    // now set REFON bit to 1
    // data for the configuration
    for (i=0; i < LTC_N_LTC; i++) {
#if defined(ITRI_MOD_9)
        uint16_t j = BS_NR_OF_MODULES-i-1;

        // FC = disable all pull-downs, REFON = 1
        ltc_tmpTXbuffer[0+(1*j)*6] = 0xFC;
        ltc_tmpTXbuffer[1+(1*j)*6] = 0x00;
        ltc_tmpTXbuffer[2+(1*j)*6] = 0x00;
        ltc_tmpTXbuffer[3+(1*j)*6] = 0x00;
        ltc_tmpTXbuffer[4+(1*j)*6] = 0x00;
        ltc_tmpTXbuffer[5+(1*j)*6] = 0x00;

        // config gpio1 of SPM's reset and disable bit as 0
        /*
    	LTC_EBM_SetColState(i,
							&ltc_tmpTXbuffer[0+(j)*6],
							0);
		*/
        {
        	uint8_t ebm[BS_NR_OF_MODULES];
        	uint8_t spm[BS_NR_OF_COLUMNS];
        	for (uint8_t i=0; i < BS_NR_OF_MODULES; i++) ebm[i] = 2;	// disable
        	for (uint8_t i=0; i < BS_NR_OF_COLUMNS; i++) spm[i] = 0;	// disable
        	ltc_ebm_force_update = 1;
        	set_ebm_eb_col_state(ebm, spm, NULL, NULL);
        }
#else
        // FC = disable all pull-downs, REFON = 1
        ltc_tmpTXbuffer[0+(1*i)*6] = 0xFC;
        ltc_tmpTXbuffer[1+(1*i)*6] = 0x00;
        ltc_tmpTXbuffer[2+(1*i)*6] = 0x00;
        ltc_tmpTXbuffer[3+(1*i)*6] = 0x00;
        ltc_tmpTXbuffer[4+(1*i)*6] = 0x00;
        ltc_tmpTXbuffer[5+(1*i)*6] = 0x00;
#endif
    }

#if defined(ITRI_MOD_12)
    {
    	uint8_t led[BS_NR_OF_LEDS] = {1, 1, 1, 1, 1, 1};	// all LEDs are turn on
    	set_ebm_led_state(led, NULL, NULL, NULL);
    }
#endif

    // now construct the message to be sent: it contains the wanted data, PLUS the needed PECs
    ltc_DataBufferSPI_TX_with_PEC_init[0] = ltc_cmdWRCFG[0];
    ltc_DataBufferSPI_TX_with_PEC_init[1] = ltc_cmdWRCFG[1];
    ltc_DataBufferSPI_TX_with_PEC_init[2] = ltc_cmdWRCFG[2];
    ltc_DataBufferSPI_TX_with_PEC_init[3] = ltc_cmdWRCFG[3];

    for (i=0; i < LTC_N_LTC; i++) {

        PEC_Check[0] = ltc_DataBufferSPI_TX_with_PEC_init[4+i*8] = ltc_tmpTXbuffer[0+i*6];
        PEC_Check[1] = ltc_DataBufferSPI_TX_with_PEC_init[5+i*8] = ltc_tmpTXbuffer[1+i*6];
        PEC_Check[2] = ltc_DataBufferSPI_TX_with_PEC_init[6+i*8] = ltc_tmpTXbuffer[2+i*6];
        PEC_Check[3] = ltc_DataBufferSPI_TX_with_PEC_init[7+i*8] = ltc_tmpTXbuffer[3+i*6];
        PEC_Check[4] = ltc_DataBufferSPI_TX_with_PEC_init[8+i*8] = ltc_tmpTXbuffer[4+i*6];
        PEC_Check[5] = ltc_DataBufferSPI_TX_with_PEC_init[9+i*8] = ltc_tmpTXbuffer[5+i*6];

        PEC_result = LTC_pec15_calc(6, PEC_Check);
        ltc_DataBufferSPI_TX_with_PEC_init[10+i*8]=(uint8_t)((PEC_result>>8)&0xff);
        ltc_DataBufferSPI_TX_with_PEC_init[11+i*8]=(uint8_t)(PEC_result&0xff);
    }  // end for

    statusSPI = LTC_SendData(ltc_DataBufferSPI_TX_with_PEC_init);
    //DEBUG_PRINTF_EX("ltc_DataBufferSPI_TX_with_PEC_init:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \r\n",
    //		ltc_DataBufferSPI_TX_with_PEC_init[4 + 8*0],
	//		ltc_DataBufferSPI_TX_with_PEC_init[4 + 8*1],
	//		ltc_DataBufferSPI_TX_with_PEC_init[4 + 8*2],
	//		ltc_DataBufferSPI_TX_with_PEC_init[4 + 8*3],
	//		ltc_DataBufferSPI_TX_with_PEC_init[4 + 8*4],
	//		ltc_DataBufferSPI_TX_with_PEC_init[4 + 8*5],
	//		ltc_DataBufferSPI_TX_with_PEC_init[4 + 8*6],
	//		ltc_DataBufferSPI_TX_with_PEC_init[4 + 8*7]);

    if (statusSPI != E_OK) {
        retVal = E_NOT_OK;
    }

    retVal = statusSPI;

    return retVal;
}




/*
 * @brief   sets the balancing according to the control values read in the database.
 *
 * To set balancing for the cells, the corresponding bits have to be written in the configuration register.
 * The LTC driver only executes the balancing orders written by the BMS in the database.
 *
 * @param registerSet   Register Set, 0: cells 1 to 12 (WRCFG), 1: cells 13 to 15/18 (WRCFG2)
 *
 * @return              E_OK if dummy byte was sent correctly by SPI, E_NOT_OK otherwise
 *
 */
static STD_RETURN_TYPE_e LTC_BalanceControl(uint8_t registerSet) {

    STD_RETURN_TYPE_e retVal = E_OK;

    uint16_t i = 0;
    uint16_t j = 0;

    LTC_Get_BalancingControlValues();

    if (registerSet == 0) {  // cells 1 to 12, WRCFG

        for (j=0; j < BS_NR_OF_MODULES; j++) {

            i = BS_NR_OF_MODULES-j-1;

            // FC = disable all pull-downs, REFON = 1 (reference always on), DTEN off, ADCOPT = 0
            ltc_tmpTXbuffer[0+(i)*6] = 0xFC;
            ltc_tmpTXbuffer[1+(i)*6] = 0x00;
            ltc_tmpTXbuffer[2+(i)*6] = 0x00;
            ltc_tmpTXbuffer[3+(i)*6] = 0x00;
            ltc_tmpTXbuffer[4+(i)*6] = 0x00;
            ltc_tmpTXbuffer[5+(i)*6] = 0x00;

            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+0] == 1) {
                ltc_tmpTXbuffer[4+(i)*6]|=0x01;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+1] == 1) {
                ltc_tmpTXbuffer[4+(i)*6]|=0x02;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+2] == 1) {
                ltc_tmpTXbuffer[4+(i)*6]|=0x04;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+3] == 1) {
                ltc_tmpTXbuffer[4+(i)*6]|=0x08;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+4] == 1) {
                ltc_tmpTXbuffer[4+(i)*6]|=0x10;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+5] == 1) {
                ltc_tmpTXbuffer[4+(i)*6]|=0x20;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+6] == 1) {
                ltc_tmpTXbuffer[4+(i)*6]|=0x40;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+7] == 1) {
                ltc_tmpTXbuffer[4+(i)*6]|=0x80;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+8] == 1) {
                ltc_tmpTXbuffer[5+(i)*6]|=0x01;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+9] == 1) {
                ltc_tmpTXbuffer[5+(i)*6]|=0x02;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+10] == 1) {
                ltc_tmpTXbuffer[5+(i)*6]|=0x04;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+11] == 1) {
                ltc_tmpTXbuffer[5+(i)*6]|=0x08;
            }
        }
        retVal = LTC_TX((uint8_t*)ltc_cmdWRCFG, ltc_tmpTXbuffer, ltc_DataBufferSPI_TX_with_PEC_init);
    } else if (registerSet == 1) {  // cells 13 to 15/18 WRCFG2

        for (j=0; j < BS_NR_OF_MODULES; j++) {

            i = BS_NR_OF_MODULES-j-1;

            // 0x0F = disable pull-downs on GPIO6-9
            ltc_tmpTXbuffer[0+(i)*6] = 0x0F;
            ltc_tmpTXbuffer[1+(i)*6] = 0x00;
            ltc_tmpTXbuffer[2+(i)*6] = 0x00;
            ltc_tmpTXbuffer[3+(i)*6] = 0x00;
            ltc_tmpTXbuffer[4+(i)*6] = 0x00;
            ltc_tmpTXbuffer[5+(i)*6] = 0x00;

            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+12] == 1) {
                ltc_tmpTXbuffer[0+(i)*6] |= 0x10;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+13] == 1) {
                ltc_tmpTXbuffer[0+(i)*6] |= 0x20;
            }
            if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+14] == 1) {
                ltc_tmpTXbuffer[0+(i)*6] |= 0x40;
            }
            if (BS_NR_OF_BAT_CELLS_PER_MODULE > 15) {

                if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+15] == 1) {
                    ltc_tmpTXbuffer[0+(i)*6] |= 0x80;
                }
                if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+16] == 1) {
                    ltc_tmpTXbuffer[1+(i)*6] |= 0x01;
                }
                if (ltc_balancing_control.value[j*(BS_NR_OF_BAT_CELLS_PER_MODULE)+17] == 1) {
                    ltc_tmpTXbuffer[1+(i)*6] |= 0x02;
                }
            }
        }
        retVal = LTC_TX((uint8_t*)ltc_cmdWRCFG2, ltc_tmpTXbuffer, ltc_DataBufferSPI_TX_with_PEC_init);
    } else {
        return E_NOT_OK;
    }
    return retVal;
}

#if defined(ITRI_MOD_2)
static STD_RETURN_TYPE_e LTC_EBM_SetEBState(void) {
	STD_RETURN_TYPE_e retVal = E_OK;
	uint16_t i=0, j=0;

	for (j=0; j < BS_NR_OF_MODULES; j++) {
		i = BS_NR_OF_MODULES-j-1;

		if (ltc_ebm_config[j].eb_state == 0) {
#if defined(ITRI_EBM_CHROMA_V2)
			ltc_tmpTXbuffer[0+(i)*6] = 0xEC;	// bypass;	1110 1100
#else
			ltc_tmpTXbuffer[0+(i)*6] = 0xAC;	// bypass;	1010 1100
#endif
		} else if (ltc_ebm_config[j].eb_state == 1) {
#if defined(ITRI_EBM_CHROMA_V2)
			ltc_tmpTXbuffer[0+(i)*6] = 0xAC;	// enable;	1010 1100
#else
			ltc_tmpTXbuffer[0+(i)*6] = 0xEC;	// enable;	1110 1100
#endif
		} else if (ltc_ebm_config[j].eb_state == 2) {
#if defined(ITRI_EBM_CHROMA_V2)
			ltc_tmpTXbuffer[0+(i)*6] = 0xFC;	// disable; 1111 1100
#else
			ltc_tmpTXbuffer[0+(i)*6] = 0xFC;	// disable; 1111 1100
#endif
		} else {
			DEBUG_PRINTF_EX("[ERR]unknown ebm state(%u), module(%u)\r\n", ltc_ebm_config[j].eb_state, j);
			return E_NOT_OK;
		}

		ltc_tmpTXbuffer[1+(i)*6] = 0x00;
		ltc_tmpTXbuffer[2+(i)*6] = 0x00;
		ltc_tmpTXbuffer[3+(i)*6] = 0x00;
		ltc_tmpTXbuffer[4+(i)*6] = 0x00;
		ltc_tmpTXbuffer[5+(i)*6] = 0x00;
	}
	retVal = LTC_TX((uint8_t*)ltc_cmdWRCFG, ltc_tmpTXbuffer, ltc_DataBufferSPI_TX_with_PEC_init);

	return retVal;
}
#endif



/*
 * @brief   resets the error table.
 *
 * This function should be called during initialization or before starting a new measurement cycle
 *
 * @return  void
 *
 */
static void LTC_ResetErrorTable(void) {
    uint16_t i = 0;
    uint16_t j = 0;

    for (i=0; i < BS_NR_OF_MODULES; i++) {
        for (j=0; j < LTC_NUMBER_OF_LTC_PER_MODULE; j++) {
            LTC_ErrorTable[i].LTC[j] = 0;
        }
        LTC_ErrorTable[i].mux0 = 0;
        LTC_ErrorTable[i].mux1 = 0;
        LTC_ErrorTable[i].mux2 = 0;
        LTC_ErrorTable[i].mux3 = 0;
    }
}


/**
 * @brief   brief missing
 *
 * Gets the measurement time needed by the LTC chip, depending on the measurement mode and the number of channels.
 * For all cell voltages or all 5 GPIOS, the measurement time is the same.
 * For 2 cell voltages or 1 GPIO, the measurement time is the same.
 * As a consequence, this function is used for cell voltage and for GPIO measurement.
 *
 * @param   adcMode     LTC ADCmeasurement mode (fast, normal or filtered)
 * @param   adcMeasCh   number of channels measured for GPIOS (one at a time for multiplexers or all five GPIOs)
 *                      or number of cell voltage measured (2 cells or all cells)
 *
 * @return  retVal      measurement time in ms
 */
static uint16_t LTC_Get_MeasurementTCycle(LTC_ADCMODE_e adcMode, LTC_ADCMEAS_CHAN_e  adcMeasCh) {

    uint16_t retVal = LTC_STATEMACH_MEAS_ALL_NORMAL_TCYCLE;  // default

    if (adcMeasCh == LTC_ADCMEAS_ALLCHANNEL) {

        if (adcMode == LTC_ADCMODE_FAST_DCP0 || adcMode == LTC_ADCMODE_FAST_DCP1) {

            retVal = LTC_STATEMACH_MEAS_ALL_FAST_TCYCLE;
        } else if (adcMode == LTC_ADCMODE_NORMAL_DCP0 || adcMode == LTC_ADCMODE_NORMAL_DCP1) {

            retVal = LTC_STATEMACH_MEAS_ALL_NORMAL_TCYCLE;
        } else if (adcMode == LTC_ADCMODE_FILTERED_DCP0 || adcMode == LTC_ADCMODE_FILTERED_DCP1) {

            retVal = LTC_STATEMACH_MEAS_ALL_FILTERED_TCYCLE;
        }
    } else if (adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_GPIO1 || adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_GPIO2
            || adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_GPIO3 || adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_GPIO4
            || adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_GPIO5 || adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_TWOCELLS) {

        if (adcMode == LTC_ADCMODE_FAST_DCP0 || adcMode == LTC_ADCMODE_FAST_DCP1) {

            retVal = LTC_STATEMACH_MEAS_SINGLE_FAST_TCYCLE;
        } else if (adcMode == LTC_ADCMODE_NORMAL_DCP0 || adcMode == LTC_ADCMODE_NORMAL_DCP1) {

            retVal = LTC_STATEMACH_MEAS_SINGLE_NORMAL_TCYCLE;
        } else if (adcMode == LTC_ADCMODE_FILTERED_DCP0 || adcMode == LTC_ADCMODE_FILTERED_DCP1) {

            retVal = LTC_STATEMACH_MEAS_SINGLE_FILTERED_TCYCLE;
        }
    } else {

        retVal = LTC_STATEMACH_MEAS_ALL_NORMAL_TCYCLE;
    }

    return retVal;
}


/**
 * @brief   tells the LTC daisy-chain to start measuring the voltage on all cells.
 *
 * This function sends an instruction to the daisy-chain via SPI, in order to start voltage measurement for all cells.
 *
 * @param   adcMode     LTC ADCmeasurement mode (fast, normal or filtered)
 * @param   adcMeasCh   number of cell voltage measured (2 cells or all cells)
 *
 * @return  retVal      E_OK if dummy byte was sent correctly by SPI, E_NOT_OK otherwise
 *
 */
static STD_RETURN_TYPE_e LTC_StartVoltageMeasurement(LTC_ADCMODE_e adcMode, LTC_ADCMEAS_CHAN_e adcMeasCh) {

    STD_RETURN_TYPE_e retVal = E_OK;

    if (adcMeasCh == LTC_ADCMEAS_ALLCHANNEL) {
        if (adcMode == LTC_ADCMODE_FAST_DCP0) {

            retVal = LTC_SendCmd(ltc_cmdADCV_fast_DCP0);
        } else if (adcMode == LTC_ADCMODE_NORMAL_DCP0) {

            retVal = LTC_SendCmd(ltc_cmdADCV_normal_DCP0);
        } else if (adcMode == LTC_ADCMODE_FILTERED_DCP0) {

            retVal = LTC_SendCmd(ltc_cmdADCV_filtered_DCP0);
        } else if (adcMode == LTC_ADCMODE_FAST_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADCV_fast_DCP1);
        } else if (adcMode == LTC_ADCMODE_NORMAL_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADCV_normal_DCP1);
        } else if (adcMode == LTC_ADCMODE_FILTERED_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADCV_filtered_DCP1);
        } else {
            retVal = E_NOT_OK;
        }
    } else if (adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_TWOCELLS) {
        if (adcMode == LTC_ADCMODE_FAST_DCP0) {

            retVal = LTC_SendCmd(ltc_cmdADCV_fast_DCP0_twocells);
        } else {
            retVal = E_NOT_OK;
        }
    } else {
        retVal = E_NOT_OK;
    }
    return retVal;
}


/**
 * @brief   tells LTC daisy-chain to start measuring the voltage on GPIOS.
 *
 * This function sends an instruction to the daisy-chain via SPI to start the measurement.
 *
 * @param   adcMode     LTC ADCmeasurement mode (fast, normal or filtered)
 * @param   adcMeasCh   number of channels measured for GPIOS (one at a time, typically when multiplexers are used, or all five GPIOs)
 *
 * @return  retVal      E_OK if dummy byte was sent correctly by SPI, E_NOT_OK otherwise
 *
 */
static STD_RETURN_TYPE_e LTC_StartGPIOMeasurement(LTC_ADCMODE_e adcMode, LTC_ADCMEAS_CHAN_e  adcMeasCh) {

    STD_RETURN_TYPE_e retVal;

    if (adcMeasCh == LTC_ADCMEAS_ALLCHANNEL) {

        if (adcMode == LTC_ADCMODE_FAST_DCP0 || adcMode == LTC_ADCMODE_FAST_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADAX_fast_ALLGPIOS);
        } else if (adcMode == LTC_ADCMODE_FILTERED_DCP0 || adcMode == LTC_ADCMODE_FILTERED_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADAX_filtered_ALLGPIOS);
        } else {
            /*if(adcMode == LTC_ADCMODE_NORMAL_DCP0 || adcMode == LTC_ADCMODE_NORMAL_DCP1)*/
            retVal = LTC_SendCmd(ltc_cmdADAX_normal_ALLGPIOS);
        }
    } else if (adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_GPIO1) {
        // Single Channel
        if (adcMode == LTC_ADCMODE_FAST_DCP0 || adcMode == LTC_ADCMODE_FAST_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADAX_fast_GPIO1);
        } else if (adcMode == LTC_ADCMODE_FILTERED_DCP0 || adcMode == LTC_ADCMODE_FILTERED_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADAX_filtered_GPIO1);
        } else {
            /*if(adcMode == LTC_ADCMODE_NORMAL_DCP0 || adcMode == LTC_ADCMODE_NORMAL_DCP1)*/

            retVal = LTC_SendCmd(ltc_cmdADAX_normal_GPIO1);
        }
    } else if (adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_GPIO2) {
        // Single Channel
        if (adcMode == LTC_ADCMODE_FAST_DCP0 || adcMode == LTC_ADCMODE_FAST_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADAX_fast_GPIO2);
        } else if (adcMode == LTC_ADCMODE_FILTERED_DCP0 || adcMode == LTC_ADCMODE_FILTERED_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADAX_filtered_GPIO2);
        } else {
            /*if(adcMode == LTC_ADCMODE_NORMAL_DCP0 || adcMode == LTC_ADCMODE_NORMAL_DCP1)*/

            retVal = LTC_SendCmd(ltc_cmdADAX_normal_GPIO2);
        }
    } else if (adcMeasCh == LTC_ADCMEAS_SINGLECHANNEL_GPIO3) {
        // Single Channel
        if (adcMode == LTC_ADCMODE_FAST_DCP0 || adcMode == LTC_ADCMODE_FAST_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADAX_fast_GPIO3);
        } else if (adcMode == LTC_ADCMODE_FILTERED_DCP0 || adcMode == LTC_ADCMODE_FILTERED_DCP1) {

            retVal = LTC_SendCmd(ltc_cmdADAX_filtered_GPIO3);
        } else {
            /*if(adcMode == LTC_ADCMODE_NORMAL_DCP0 || adcMode == LTC_ADCMODE_NORMAL_DCP1)*/

            retVal = LTC_SendCmd(ltc_cmdADAX_normal_GPIO3);
        }
    } else {
        retVal = E_NOT_OK;
    }

    return retVal;
}


/**
 * @brief   checks if the data received from the daisy-chain is not corrupt.
 *
 * This function computes the PEC (CRC) from the data received by the daisy-chain.
 * It compares it with the PEC sent by the LTCs.
 * If there are errors, the array LTC_ErrorTable is updated to locate the LTCs in daisy-chain
 * that transmitted corrupt data.
 *
 * @param   *DataBufferSPI_RX_with_PEC   data obtained from the SPI transmission
 *
 * @return  retVal                       E_OK if PEC check is OK, E_NOT_OK otherwise
 *
 */
static STD_RETURN_TYPE_e LTC_RX_PECCheck(uint8_t *DataBufferSPI_RX_with_PEC) {

    uint16_t i = 0;
    uint16_t j = 0;
    STD_RETURN_TYPE_e retVal = E_OK;
    uint8_t PEC_TX[2];
    uint16_t PEC_result = 0;
    uint8_t PEC_Check[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // check all PECs and put data without command and PEC in DataBufferSPI_RX (easier to use)
    for (i=0; i < LTC_N_LTC; i++) {

        PEC_Check[0] = DataBufferSPI_RX_with_PEC[4+i*8];
        PEC_Check[1] = DataBufferSPI_RX_with_PEC[5+i*8];
        PEC_Check[2] = DataBufferSPI_RX_with_PEC[6+i*8];
        PEC_Check[3] = DataBufferSPI_RX_with_PEC[7+i*8];
        PEC_Check[4] = DataBufferSPI_RX_with_PEC[8+i*8];
        PEC_Check[5] = DataBufferSPI_RX_with_PEC[9+i*8];

        PEC_result = LTC_pec15_calc(6, PEC_Check);
        PEC_TX[0]=(uint8_t)((PEC_result>>8)&0xff);
        PEC_TX[1]=(uint8_t)(PEC_result&0xff);

        // if calculated PEC not equal to received PEC
        if ((PEC_TX[0]!=DataBufferSPI_RX_with_PEC[10+i*8]) || (PEC_TX[1]!=DataBufferSPI_RX_with_PEC[11+i*8])) {

            // update error table of the corresponding LTC
            j = i%LTC_NUMBER_OF_LTC_PER_MODULE;
            LTC_ErrorTable[i/LTC_NUMBER_OF_LTC_PER_MODULE].LTC[j] = 1;

            DIAG_Handler(DIAG_CH_LTC_PEC, DIAG_EVENT_NOK, 0, NULL_PTR);

#if defined(ITRI_MOD_2)
            //DEBUG_PRINTF_EX("[%d:WARN]LTC_RX_PECCheck ret E_NOT_OK (modNo:%u)\r\n", __LINE__, i);
#endif
            retVal = E_NOT_OK;

        } else {
            DIAG_Handler(DIAG_CH_LTC_PEC, DIAG_EVENT_OK, 0, NULL_PTR);
            retVal = E_OK;
        }
    }

    return (retVal);
}


/**
 * @brief   send command to the LTC daisy-chain and receives data from the LTC daisy-chain.
 *
 * This is the core function to receive data from the LTC6804 daisy-chain.
 * A 2 byte command is sent with the corresponding PEC. Example: read configuration register (RDCFG).
 * Only command has to be set, the function calculates the PEC automatically.
 * The data send is:
 * 2 bytes (COMMAND) 2 bytes (PEC)
 * The data received is:
 * 6 bytes (LTC1) 2 bytes (PEC) + 6 bytes (LTC2) 2 bytes (PEC) + 6 bytes (LTC3) 2 bytes (PEC) + ... + 6 bytes (LTC{LTC_N_LTC}) 2 bytes (PEC)
 *
 * The function does not check the PECs. This has to be done elsewhere.
 *
 * @param   *Command                    command sent to the daisy-chain
 * @param   *DataBufferSPI_RX_with_PEC  data to sent to the daisy-chain, i.e. data to be sent + PEC
 *
 * @return  statusSPI                   E_OK if SPI transmission is OK, E_NOT_OK otherwise
 *
 */
static STD_RETURN_TYPE_e LTC_RX(uint8_t *Command, uint8_t *DataBufferSPI_RX_with_PEC) {

    STD_RETURN_TYPE_e statusSPI = E_OK;
    uint16_t i = 0;

    // DataBufferSPI_RX_with_PEC contains the data to receive.
    // The transmission function checks the PECs.
    // It constructs DataBufferSPI_RX, which contains the received data without PEC (easier to use).

    for (i=0; i < LTC_N_BYTES_FOR_DATA_TRANSMISSION; i++) {
        ltc_tmpTXPECbuffer[i] = 0x00;
    }

    ltc_tmpTXPECbuffer[0] = Command[0];
    ltc_tmpTXPECbuffer[1] = Command[1];
    ltc_tmpTXPECbuffer[2] = Command[2];
    ltc_tmpTXPECbuffer[3] = Command[3];

    statusSPI = LTC_ReceiveData(ltc_tmpTXPECbuffer, DataBufferSPI_RX_with_PEC);

    if (statusSPI != E_OK) {

        return E_NOT_OK;
    } else {
        return E_OK;
    }
}



/**
 * @brief   sends command and data to the LTC daisy-chain.
 *
 * This is the core function to transmit data to the LTC6804 daisy-chain.
 * The data sent is:
 * COMMAND + 6 bytes (LTC1) + 6 bytes (LTC2) + 6 bytes (LTC3) + ... + 6 bytes (LTC{LTC_N_LTC})
 * A 2 byte command is sent with the corresponding PEC. Example: write configuration register (WRCFG).
 * THe command has to be set and then the function calculates the PEC automatically.
 * The function calculates the needed PEC to send the data to the daisy-chain. The sent data has the format:
 * 2 byte-COMMAND (2 bytes PEC) + 6 bytes (LTC1) (2 bytes PEC) + 6 bytes (LTC2) (2 bytes PEC) + 6 bytes (LTC3) (2 bytes PEC) + ... + 6 bytes (LTC{LTC_N_LTC}) (2 bytes PEC)
 *
 * The function returns 0. The only way to check if the transmission was successful is to read the results of the write operation.
 * (example: read configuration register after writing to it)
 *
 * @param   *Command                    command sent to the daisy-chain
 * @param   *DataBufferSPI_TX           data to be sent to the daisy-chain
 * @param   *DataBufferSPI_TX_with_PEC  data to sent to the daisy-chain, i.e. data to be sent + PEC (calculated by the function)
 *
 * @return                              E_OK if SPI transmission is OK, E_NOT_OK otherwise
 *
 */
static STD_RETURN_TYPE_e LTC_TX(uint8_t *Command, uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC) {

    uint16_t i = 0;
    STD_RETURN_TYPE_e statusSPI = E_NOT_OK;
    uint16_t PEC_result = 0;
    uint8_t PEC_Check[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    // DataBufferSPI_TX contains the data to send.
    // The transmission function calculates the needed PEC.
    // With it constructs DataBufferSPI_TX_with_PEC.
    // It corresponds to the data effectively received/sent.
    for (i=0; i < LTC_N_BYTES_FOR_DATA_TRANSMISSION; i++) {
        DataBufferSPI_TX_with_PEC[i] = 0x00;
    }

    DataBufferSPI_TX_with_PEC[0] = Command[0];
    DataBufferSPI_TX_with_PEC[1] = Command[1];
    DataBufferSPI_TX_with_PEC[2] = Command[2];
    DataBufferSPI_TX_with_PEC[3] = Command[3];

    // Calculate PEC of all data (1 PEC value for 6 bytes)
    for (i=0; i < LTC_N_LTC; i++) {

        PEC_Check[0] = DataBufferSPI_TX_with_PEC[4+i*8] = DataBufferSPI_TX[0+i*6];
        PEC_Check[1] = DataBufferSPI_TX_with_PEC[5+i*8] = DataBufferSPI_TX[1+i*6];
        PEC_Check[2] = DataBufferSPI_TX_with_PEC[6+i*8] = DataBufferSPI_TX[2+i*6];
        PEC_Check[3] = DataBufferSPI_TX_with_PEC[7+i*8] = DataBufferSPI_TX[3+i*6];
        PEC_Check[4] = DataBufferSPI_TX_with_PEC[8+i*8] = DataBufferSPI_TX[4+i*6];
        PEC_Check[5] = DataBufferSPI_TX_with_PEC[9+i*8] = DataBufferSPI_TX[5+i*6];

        PEC_result = LTC_pec15_calc(6, PEC_Check);
        DataBufferSPI_TX_with_PEC[10+i*8]=(uint8_t)((PEC_result>>8)&0xff);
        DataBufferSPI_TX_with_PEC[11+i*8]=(uint8_t)(PEC_result&0xff);
    }

    statusSPI = LTC_SendData(DataBufferSPI_TX_with_PEC);

    if (statusSPI != E_OK) {
        return E_NOT_OK;
    } else {
        return E_OK;
    }
}

/**
 * @brief   configures the data that will be sent to the LTC daisy-chain to configure multiplexer channels.
 *
 * This function does not sent the data to the multiplexer daisy-chain. This is done
 * by the function LTC_SetMuxChannel(), which calls LTC_SetMUXChCommand()..
 *
 * @param   *DataBufferSPI_TX      data to be sent to the daisy-chain to configure the multiplexer channels
 * @param   mux                    multiplexer ID to be configured (0,1,2 or 3)
 * @param   channel                multiplexer channel to be configured (0 to 7)
 *
 * @return  void
 *
 */
static void LTC_SetMUXChCommand(uint8_t *DataBufferSPI_TX, uint8_t mux, uint8_t channel) {

    uint16_t i = 0;

    for (i=0; i < LTC_N_LTC; i++) {

#if SLAVE_BOARD_VERSION == 2

        /* using ADG728 */
        uint8_t address = 0x4C | (mux % 3);
        uint8_t data = 1 << (channel % 8);
        if (channel == 0xFF) {  // no channel selected, output of multiplexer is high impedance
            data = 0x00;
        }

#else

        /* using LTC1380 */
        uint8_t address = 0x48 | (mux % 4);
        uint8_t data = 0x80 | (channel % 8);
        if (channel == 0xFF) {  // no channel selected, output of multiplexer is high impedance
            data = 0x00;
        }

#endif

        DataBufferSPI_TX[0 + i * 6] = LTC_ICOM_START | (address >> 3);        // 0x6 : LTC6804: ICOM START from Master
        DataBufferSPI_TX[1 + i * 6] = LTC_FCOM_MASTER_NACK | (address << 5);
        DataBufferSPI_TX[2 + i * 6] = LTC_ICOM_BLANK | (data >> 4);
        DataBufferSPI_TX[3 + i * 6] = LTC_FCOM_MASTER_NACK_STOP | (data << 4);
        DataBufferSPI_TX[4 + i * 6] = LTC_ICOM_NO_TRANSMIT;        // 0x1 : ICOM-STOP
        DataBufferSPI_TX[5 + i * 6] = 0x00;        // 0x0 : dummy (Dn)
                                                   // 9: MASTER NACK + STOP (FCOM)
    }
}



/**
 * @brief   sends data to the LTC daisy-chain to configure multiplexer channels.
 *
 * This function calls the function LTC_SetMUXChCommand() to set the data.
 *
 * @param        *DataBufferSPI_TX              data to be sent to the daisy-chain to configure the multiplexer channels
 * @param        *DataBufferSPI_TX_with_PEC     data to be sent to the daisy-chain to configure the multiplexer channels, with PEC (calculated by the function)
 * @param        mux                    multiplexer ID to be configured (0,1,2 or 3)
 * @param        channel                multiplexer channel to be configured (0 to 7)
 *
 * @return       E_OK if SPI transmission is OK, E_NOT_OK otherwise
 */
static uint8_t LTC_SetMuxChannel(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC, uint8_t mux, uint8_t channel ) {

    STD_RETURN_TYPE_e statusSPI = E_NOT_OK;

    // send WRCOMM to send I2C message to choose channel
    LTC_SetMUXChCommand(DataBufferSPI_TX, mux, channel);
    statusSPI = LTC_TX((uint8_t*)ltc_cmdWRCOMM, DataBufferSPI_TX, DataBufferSPI_TX_with_PEC);

    if (statusSPI != E_OK) {
        return E_NOT_OK;
    } else {
        return E_OK;
    }
}

/**
 * @brief   sends data to the LTC daisy-chain to save a data byte to the external EEPROM
 *
 * This function sends a control byte and a data byte to the external EEPROM
 *
 * @param        *DataBufferSPI_TX              data to be sent to the daisy-chain to configure the EEPROM
 * @param        *DataBufferSPI_TX_with_PEC     data to be sent to the daisy-chain to configure the EEPROM, with PEC (calculated by the function)
 * @param        address                        EEPROM address to write to
 * @param        data                           data byte to be written to EEPROM
 *
 * @return       E_OK if SPI transmission is OK, E_NOT_OK otherwise
 */
/*static uint8_t LTC_WriteEEPROMData(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC, uint8_t address, uint8_t data)
{
    STD_RETURN_TYPE_e statusSPI = E_NOT_OK;

    uint16_t i = 0;

    for(i=0; i<BS_NR_OF_MODULES; i++) {
        DataBufferSPI_TX[0+i*8] = 0x6A; // 6: ICOM0 start condition, A: upper nibble of EEPROM I2C address
        DataBufferSPI_TX[1+i*8] = 0x08; // 0: lower nibble of EEPROM I2C address + R/W bit, 8: FCOM0 master NACK

        DataBufferSPI_TX[2+i*8] = (address>>4); // 0: ICOM1 blank, x: upper nibble of address
        DataBufferSPI_TX[3+i*8] = (uint8_t)(address<<4)|0x08; // x: lower nibble of address, 9: FCOM1 master NACK

        DataBufferSPI_TX[4+i*8] = (data>>4); // 1: ICOM2 blank, x: data
        DataBufferSPI_TX[5+i*8] = (uint8_t)(data<<4)|0x09; // B: data, 7: FCOM2 master NACK + STOP
    }

    //send WRCOMM to send I2C message to choose channel
    statusSPI = LTC_TX((uint8_t*)ltc_cmdWRCOMM, DataBufferSPI_TX, DataBufferSPI_TX_with_PEC);

    if(statusSPI != E_OK) {

        return E_NOT_OK;
    }
    else
        return E_OK;
}*/

/**
 * @brief   sends data to the LTC daisy-chain to communicate via I2C
 *
 * This function initiates an I2C signal sent by the LTC6804 on the slave boards
 *
 * @param        *DataBufferSPI_TX              data to be sent to the daisy-chain to configure the EEPROM
 * @param        *DataBufferSPI_TX_with_PEC     data to be sent to the daisy-chain to configure the EEPROM, with PEC (calculated by the function)
 * @param        cmd_daa                        command data to be sent
 *
 * @return       E_OK if SPI transmission is OK, E_NOT_OK otherwise
 */
static STD_RETURN_TYPE_e LTC_Send_I2C_Command(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC, uint8_t *cmd_data) {

    STD_RETURN_TYPE_e statusSPI = E_NOT_OK;

    uint16_t i = 0;

    for (i=0; i < BS_NR_OF_MODULES; i++) {

        DataBufferSPI_TX[0+i*6] = cmd_data[0];
        DataBufferSPI_TX[1+i*6] = cmd_data[1];

        DataBufferSPI_TX[2+i*6] = cmd_data[2];
        DataBufferSPI_TX[3+i*6] = cmd_data[3];

        DataBufferSPI_TX[4+i*6] = cmd_data[4];
        DataBufferSPI_TX[5+i*6] = cmd_data[5];
    }

    // send WRCOMM to send I2C message to choose channel
    statusSPI = LTC_TX((uint8_t*)ltc_cmdWRCOMM, DataBufferSPI_TX, DataBufferSPI_TX_with_PEC);

    if (statusSPI != E_OK) {
        return E_NOT_OK;
    } else {
        return E_OK;
    }
}

/**
 * @brief   saves the UID values of the external EEPROMs read from the LTC daisy-chain.
 *
 * This function saves the unique IDs received from the external EEPROMs
 *
 * @param   registerSet    register set that was read from the EEPROM (0: address 0xFA-0xFC, 1: address 0xFD-0xFF)
 * @param   *rxBuffer      buffer containing the data obtained from the SPI transmission
 *
 * @return  void
 *
 */
static void LTC_EEPROMSaveUID(uint8_t registerSet, uint8_t *rxBuffer) {

    uint16_t i = 0;
    uint8_t eeprom_uid_tmp[4];

    // addresses 0xFA-0xFC were read from EEPROM
    if (registerSet == 0) {
        /* extract data */
        for (i=0; i < LTC_N_LTC; i++) {
            eeprom_uid_tmp[0] = (rxBuffer[8+i*8]<<4)|((rxBuffer[9+i*8]>>4));
            LTC_EEPROM_UIDs[i] &= ~(0xFF000000);
            LTC_EEPROM_UIDs[i] |= (eeprom_uid_tmp[0]<<24);
        }
    }

    // addresses 0xFD-0xFF were read from EEPROM
    else if (registerSet == 1) {
        /* extract data */
        for (i=0; i < LTC_N_LTC; i++) {
            eeprom_uid_tmp[1] = (rxBuffer[4+i*8]<<4)|((rxBuffer[5+i*8]>>4));
            eeprom_uid_tmp[2] = (rxBuffer[6+i*8]<<4)|((rxBuffer[7+i*8]>>4));
            eeprom_uid_tmp[3] = (rxBuffer[8+i*8]<<4)|((rxBuffer[9+i*8]>>4));
            LTC_EEPROM_UIDs[i] &= ~(0x00FFFFFF);
            LTC_EEPROM_UIDs[i] |= (eeprom_uid_tmp[1]<<16)|(eeprom_uid_tmp[2]<<8)|(eeprom_uid_tmp[3]);
        }
    } else {
        return;
    }
}

/**
 * @brief   saves the temperature value of the external temperature sensors read from the LTC daisy-chain.
 *
 * This function saves the temperature value received from the external temperature sensors
 *
 * @param   *rxBuffer      buffer containing the data obtained from the SPI transmission
 *
 * @return  void
 *
 */
static void LTC_TempSensSaveTemp(uint8_t *rxBuffer) {

    uint16_t i = 0;
    uint8_t temp_tmp[2];
    uint16_t val_i = 0;

    for (i=0; i < LTC_N_LTC; i++) {
        temp_tmp[0] = (rxBuffer[6+i*8]<<4)|((rxBuffer[7+i*8]>>4));
        temp_tmp[1] = (rxBuffer[8+i*8]<<4)|((rxBuffer[9+i*8]>>4));
        val_i = (temp_tmp[0]<<8)|(temp_tmp[1]);
        val_i = val_i>>8;
        LTC_EXT_TEMP_SENS[i] = val_i;
    }
}

/**
 * @brief   sends data to the LTC daisy-chain to control the user port expander
 *
 * This function sends a control byte to the register of the user port expander
 *
 * @param        *DataBufferSPI_TX              data to be sent to the daisy-chain to configure the multiplexer channels
 * @param        *DataBufferSPI_TX_with_PEC     data to be sent to the daisy-chain to configure the multiplexer channels, with PEC (calculated by the function)
 *
 * @return       E_OK if SPI transmission is OK, E_NOT_OK otherwise
 */
static uint8_t LTC_SetPortExpander(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC ) {

    STD_RETURN_TYPE_e statusSPI = E_NOT_OK;

    uint16_t i = 0;
    uint8_t output_data = 0;

    for (i=0; i < BS_NR_OF_MODULES; i++) {
        output_data = ltc_user_io_control.value_out[BS_NR_OF_MODULES-1-i];

        DataBufferSPI_TX[0+i*6] = LTC_ICOM_START | 0x04;     // 6: ICOM0 start condition, 4: upper nibble of PCA8574 address
        DataBufferSPI_TX[1+i*6] = 0 | LTC_FCOM_MASTER_NACK;  // 0: lower nibble of PCA8574 address + R/W bit, 8: FCOM0 master NACK

        DataBufferSPI_TX[2+i*6] = LTC_ICOM_BLANK | (output_data>>4);  // 0: ICOM1 blank, x: upper nibble of PCA8574 data register (0 == pin low)
        DataBufferSPI_TX[3+i*6] = (uint8_t)(output_data<<4) | LTC_FCOM_MASTER_NACK_STOP;  // x: lower nibble of PCA8574 data register, 9: FCOM1 master NACK + STOP

        DataBufferSPI_TX[4+i*6] = LTC_ICOM_NO_TRANSMIT;  // 7: no transmission, F: dummy data
        DataBufferSPI_TX[5+i*6] = 0;  // F: dummy data, 9: FCOM2 master NACK + STOP
    }

    // send WRCOMM to send I2C message to choose channel
    statusSPI = LTC_TX((uint8_t*)ltc_cmdWRCOMM, DataBufferSPI_TX, DataBufferSPI_TX_with_PEC);

    if (statusSPI != E_OK) {
        return E_NOT_OK;
    } else {
        return E_OK;
    }
}

/**
 * @brief   saves the received values of the external port expander read from the LTC daisy-chain.
 *
 * This function saves the received data byte from the external port expander
 *
 * @param   *rxBuffer      buffer containing the data obtained from the SPI transmission
 *
 * @return  void
 *
 */
static void LTC_PortExpanderSaveValues(uint8_t *rxBuffer) {
    uint16_t i = 0;
    uint8_t val_i;

    /* extract data */
    for (i=0; i < LTC_N_LTC; i++) {
        val_i = (rxBuffer[6+i*8]<<4)|((rxBuffer[7+i*8]>>4));
        ltc_user_io_control.value_in[i] = val_i;
    }
}

/**
 * @brief   sends 72 clock pulses to the LTC daisy-chain.
 *
 * This function is used for the communication with the multiplexers via I2C on the GPIOs.
 * It send the command STCOMM to the LTC daisy-chain.
 *
 * @param   *DataBufferSPI_TX           data to be sent to the daisy-chain, set to 0xFF
 * @param   *DataBufferSPI_TX_with_PEC  data to be sent to the daisy-chain  with PEC (calculated by the function)
 *
 * @return  statusSPI                   E_OK if clock pulses were sent correctly by SPI, E_NOT_OK otherwise
 *
 */
static STD_RETURN_TYPE_e LTC_I2CClock(uint8_t *DataBufferSPI_TX, uint8_t *DataBufferSPI_TX_with_PEC) {

    uint16_t i = 0;
    STD_RETURN_TYPE_e statusSPI = E_NOT_OK;

    for (i=0; i < 4+9; i++) {
        DataBufferSPI_TX_with_PEC[i] = 0xFF;
    }

    DataBufferSPI_TX_with_PEC[0] = ltc_cmdSTCOMM[0];
    DataBufferSPI_TX_with_PEC[1] = ltc_cmdSTCOMM[1];
    DataBufferSPI_TX_with_PEC[2] = ltc_cmdSTCOMM[2];
    DataBufferSPI_TX_with_PEC[3] = ltc_cmdSTCOMM[3];

    statusSPI = LTC_SendI2CCmd(DataBufferSPI_TX_with_PEC);

    return statusSPI;
}

LTC_RETURN_TYPE_e LTC_Ctrl(LTC_TASK_TYPE_e LTC_Todo) {

    LTC_STATEMACH_e ltcstate = LTC_STATEMACH_UNDEFINED;
    LTC_RETURN_TYPE_e retVal = LTC_REQUEST_PENDING;

    ltc_task_1ms_cnt++;

    if (LTC_GetStateRequest() == LTC_STATE_NO_REQUEST) {

        ltcstate = LTC_GetState();
        if (LTC_Todo == LTC_HAS_TO_MEASURE) {

            if (ltcstate == LTC_STATEMACH_UNINITIALIZED) {

                /* set initialization request*/
                retVal = LTC_SetStateRequest(LTC_STATE_INIT_REQUEST, LTC_ADCMODE_UNDEFINED, LTC_ADCMEAS_UNDEFINED, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                ltc_taskcycle = 1;
            } else if (ltcstate == LTC_STATEMACH_IDLE) {

                /* set state requests for starting measurement cycles if LTC is in IDLE state*/
                ++ltc_taskcycle;

                switch (ltc_taskcycle) {

                    case 2:
                        retVal = LTC_SetStateRequest(LTC_STATE_VOLTAGEMEASUREMENT_REQUEST, LTC_VOLTAGE_MEASUREMENT_MODE, LTC_ADCMEAS_ALLCHANNEL, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 3:
                        retVal = LTC_SetStateRequest(LTC_STATE_READVOLTAGE_REQUEST, LTC_VOLTAGE_MEASUREMENT_MODE, LTC_ADCMEAS_ALLCHANNEL, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 4:
                        LTC_SaveVoltages();
                        retVal = LTC_SetStateRequest(LTC_STATE_BALANCECONTROL_REQUEST, LTC_VOLTAGE_MEASUREMENT_MODE, LTC_ADCMEAS_ALLCHANNEL, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 5:
                        retVal = LTC_SetStateRequest(LTC_STATE_MUXMEASUREMENT_REQUEST, LTC_GPIO_MEASUREMENT_MODE, LTC_ADCMEAS_SINGLECHANNEL_GPIO1, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 6:
                        if (LTC_GetMuxSequenceState() == E_OK) {
                            LTC_SaveTemperatures();
                        }
                        retVal = LTC_SetStateRequest(LTC_STATE_USER_IO_REQUEST, LTC_ADCMODE_NORMAL_DCP0, LTC_ADCMEAS_ALLCHANNEL, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 7:
                        retVal = LTC_SetStateRequest(LTC_STATE_EEPROM_READ_UID_REQUEST, LTC_ADCMODE_NORMAL_DCP0, LTC_ADCMEAS_ALLCHANNEL, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 8:
                        retVal = LTC_SetStateRequest(LTC_STATE_TEMP_SENS_READ_REQUEST, LTC_ADCMODE_NORMAL_DCP0, LTC_ADCMEAS_ALLCHANNEL, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 9:
                        retVal = LTC_SetStateRequest(LTC_STATE_ALLGPIOMEASUREMENT_REQUEST, LTC_ADCMODE_NORMAL_DCP0, LTC_ADCMEAS_ALLCHANNEL, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 10:
                        LTC_SaveAllGPIOs();
                        ltc_taskcycle = 1;            // Restart measurement cycle
                        break;

                    default:
                        ltc_taskcycle = 1;
                        break;
                }
            }
        } else if (LTC_Todo == LTC_HAS_TO_REINIT) {

            if (ltcstate != LTC_STATEMACH_INITIALIZATION || ltcstate != LTC_STATEMACH_INITIALIZED) {

                ltc_taskcycle = 1;
                retVal = LTC_SetStateRequest(LTC_STATE_REINIT_REQUEST, LTC_ADCMODE_UNDEFINED, LTC_ADCMEAS_UNDEFINED, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
            }
        } else if (LTC_Todo == LTC_HAS_TO_MEASURE_2CELLS) {

            if (ltcstate == LTC_STATEMACH_UNINITIALIZED) {

                    /* set initialization request */
                    retVal = LTC_SetStateRequest(LTC_STATE_INIT_REQUEST, LTC_ADCMODE_FAST_DCP0, LTC_ADCMEAS_SINGLECHANNEL_TWOCELLS, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                    ltc_taskcycle = 1;
            } else if (ltcstate == LTC_STATEMACH_IDLE) {

                /* set state requests for starting measurement cycles if LTC is in IDLE state */
                ++ltc_taskcycle;

                switch (ltc_taskcycle) {

                    case 2:
                        retVal = LTC_SetStateRequest(LTC_STATE_VOLTAGEMEASUREMENT_2CELLS_REQUEST, LTC_ADCMODE_FAST_DCP0, LTC_ADCMEAS_SINGLECHANNEL_TWOCELLS, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 3:
                        retVal = LTC_SetStateRequest(LTC_STATE_READVOLTAGE_2CELLS_REQUEST, LTC_ADCMODE_FAST_DCP0, LTC_ADCMEAS_SINGLECHANNEL_TWOCELLS, LTC_NUMBER_OF_MUX_MEASUREMENTS_PER_CYCLE);
                        break;

                    case 4:
                        LTC_SaveVoltages();
                        ltc_taskcycle = 1;            // Restart measurement cycle
                        break;

                    default:
                        ltc_taskcycle = 1;
                        break;
                }
            }
        } else {
            retVal = LTC_ILLEGAL_TASK_TYPE;
        }
    }
    return retVal;
}



/**
 * @brief   gets the frequency of the SPI clock.
 *
 * This function reads the configuration from the SPI handle directly.
 *
 * @return    frequency of the SPI clock
 */
static uint32_t LTC_GetSPIClock(void) {

    uint32_t SPI_Clock = 0;

    if (LTC_SPI_INSTANCE == SPI2 || LTC_SPI_INSTANCE == SPI3) {
        // SPI2 and SPI3 are connected to APB1 (PCLK1)
        // The prescaler setup bits LTC_SPI_PRESCALER corresponds to the bits 5:3 in the SPI_CR1 register
        // Reference manual p.909
        // The shift by 3 puts the bits 5:3 to the first position
        // Division are made by powers of 2 which corresponds to shifting to the right
        // Then 0 corresponds to divide by 2, 1 corresponds to divide by 4... so 1 has to be added to the value of the configuration bits

        SPI_Clock = HAL_RCC_GetPCLK1Freq()>>( (LTC_SPI_PRESCALER>>3)+1);
    }

    if (LTC_SPI_INSTANCE == SPI1 || LTC_SPI_INSTANCE == SPI4 || LTC_SPI_INSTANCE == SPI5 || LTC_SPI_INSTANCE == SPI6) {
        // SPI1, SPI4, SPI5 and SPI6 are connected to APB2 (PCLK2)
        // The prescaler setup bits LTC_SPI_PRESCALER corresponds to the bits 5:3 in the SPI_CR1 register
        // Reference manual p.909
        // The shift by 3 puts the bits 5:3 to the first position
        // Division are made by powers of 2 which corresponds to shifting to the right
        // Then 0 corresponds to divide by 2, 1 corresponds to divide by 4... so 1 has to be added to the value of the configuration bits

        SPI_Clock = HAL_RCC_GetPCLK2Freq()>>( (LTC_SPI_PRESCALER>>3)+1);
    }

    return SPI_Clock;
}




/**
 * @brief   sets the transfer time needed to receive/send data with the LTC daisy-chain.
 *
 * This function gets the clock frequency and uses the number of LTCs in the daisy-chain.
 *
 * @return  void
 *
 */
static void LTC_SetTransferTimes(void) {

    uint32_t transferTime_us = 0;
    uint32_t SPI_Clock = 0;

    SPI_Clock = LTC_GetSPIClock();

    // Transmission of a command and data
    // Multiplication by 1000*1000 to get us
    transferTime_us = ((LTC_N_BYTES_FOR_DATA_TRANSMISSION)*8*1000*1000)/(SPI_Clock);
    transferTime_us = transferTime_us + SPI_WAKEUP_WAIT_TIME;
    ltc_state.commandDataTransferTime = (transferTime_us/1000)+1;

    // Transmission of a command
    // Multiplication by 1000*1000 to get us
    transferTime_us = ((4)*8*1000*1000)/(SPI_Clock);
    transferTime_us = transferTime_us + SPI_WAKEUP_WAIT_TIME;
    ltc_state.commandTransferTime = (transferTime_us/1000)+1;

    // Transmission of a command + 9 clocks
    // Multiplication by 1000*1000 to get us
    transferTime_us = ((4+9)*8*1000*1000)/(SPI_Clock);
    transferTime_us = transferTime_us + SPI_WAKEUP_WAIT_TIME;
    ltc_state.gpioClocksTransferTime = (transferTime_us/1000)+1;
}



/**
 * @brief   checks the state requests that are made.
 *
 * This function checks the validity of the state requests.
 * The resuls of the checked is returned immediately.
 *
 * @param   statereq    state request to be checked
 *
 * @return              result of the state request that was made, taken from LTC_RETURN_TYPE_e
 */
static LTC_RETURN_TYPE_e LTC_CheckStateRequest(LTC_STATE_REQUEST_e statereq) {

    if (ltc_state.statereq == LTC_STATE_NO_REQUEST) {

        // init only allowed from the uninitialized state
        if (statereq == LTC_STATE_INIT_REQUEST) {

            if (ltc_state.state == LTC_STATEMACH_UNINITIALIZED) {
                return LTC_OK;
            } else {
                return LTC_ALREADY_INITIALIZED;
            }
        }
        if (LTC_STATE_REINIT_REQUEST) {

            // re-init allowed from error states, too
            if (ltc_state.state == LTC_STATEMACH_IDLE) {

                 return LTC_OK;
            } else if ((ltc_state.state == LTC_STATEMACH_ERROR_SPIFAILED) ||
                      (ltc_state.state == LTC_STATEMACH_ERROR_PECFAILED) ||
                      (ltc_state.state == LTC_STATEMACH_ERROR_MUXFAILED)) {

                 return LTC_OK_FROM_ERROR;
            } else {
                 return LTC_BUSY_OK;
            }
        }
        // other state request than re-init not allowed if there is an error
        if ((statereq == LTC_STATE_VOLTAGEMEASUREMENT_REQUEST) ||
            (statereq == LTC_STATE_READVOLTAGE_REQUEST) ||
            (statereq == LTC_STATE_MUXMEASUREMENT_REQUEST) ||
            (statereq == LTC_STATE_BALANCECONTROL_REQUEST) ||
            (statereq == LTC_STATE_IDLE_REQUEST) ||
            (statereq == LTC_STATE_ALLGPIOMEASUREMENT_REQUEST)
          ) {

            if (ltc_state.state == LTC_STATEMACH_IDLE) {
                 return LTC_OK;
            } else if (ltc_state.state == LTC_STATEMACH_ERROR_SPIFAILED) {
                 return LTC_SPI_ERROR;
            } else if (ltc_state.state == LTC_STATEMACH_ERROR_PECFAILED) {
                 return LTC_PEC_ERROR;
            } else if (ltc_state.state == LTC_STATEMACH_ERROR_MUXFAILED) {
                 return LTC_MUX_ERROR;
            } else {
                 return LTC_BUSY_OK;
            }
        } else {
            return LTC_ILLEGAL_REQUEST;
        }
    } else {
        return LTC_REQUEST_PENDING;
    }
}



/**
 * @brief   gets the measurement initialization status.
 *
 * @return  retval  TRUE if a first measurement cycle was made, FALSE otherwise
 *
 */
extern uint8_t LTC_IsFirstMeasurementCycleFinished(void) {
    uint8_t retval = FALSE;

    OS_TaskEnter_Critical();
    retval    = ltc_state.first_measurement_made;
    OS_TaskExit_Critical();

    return (retval);
}

/**
 * @brief   sets the measurement initialization status.
 *
 * @return  none
 *
 */
extern void LTC_SetFirstMeasurementCycleFinished(void) {
    OS_TaskEnter_Critical();
    ltc_state.first_measurement_made = TRUE;
    OS_TaskExit_Critical();
}



/**
 * @brief   gets the measurement initialization status.
 *
 * @return  retval  TRUE if a first measurement cycle was made, FALSE otherwise
 *
 */
extern STD_RETURN_TYPE_e LTC_GetMuxSequenceState(void) {
    STD_RETURN_TYPE_e retval = FALSE;

    retval    = ltc_state.ltc_muxcycle_finished;

    return (retval);
}

#if defined(ITRI_MOD_2)
//#include "uart.h"
typedef uint32_t (*ltc_prop_funcPtr)(void* iParam1, void* iParam2, void* oParam1, void* oParam2);

uint32_t get_BS_NR_OF_MODULES(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	uint32_t* pOut = (uint32_t*)oParam1;
	*pOut = (uint32_t)BS_NR_OF_MODULES;
	return 0;
}

uint32_t get_BS_NR_OF_BAT_CELLS_PER_MODULE(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	uint32_t* pOut = (uint32_t*)oParam1;
	*pOut = (uint32_t)BS_NR_OF_BAT_CELLS_PER_MODULE;
	return 0;
}

uint32_t get_BS_NR_OF_TEMP_SENSORS_PER_MODULE(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	uint32_t* pOut = (uint32_t*)oParam1;
	*pOut = (uint32_t)BS_NR_OF_TEMP_SENSORS_PER_MODULE;
	return 0;
}

uint32_t get_LTC_allGPIOVoltages(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	uint32_t modIdx = *(uint32_t*)iParam1;
	char *com_out_buf = (char*)oParam1;

/*
	for (j=0; j < 6; j++) {
		sprintf(com_out_buf, "%s%u ", com_out_buf, *((uint16_t *)(&LTC_allGPIOVoltages[modIdx*6 + j])));
	}
*/
	sprintf(com_out_buf, "M[%u] %u %u %u %u %u %u", modIdx,
			 	 	 	 	 	 	 	 	 	 *((uint16_t *)(&LTC_allGPIOVoltages[modIdx*6 + 0])),
												 *((uint16_t *)(&LTC_allGPIOVoltages[modIdx*6 + 1])),
												 *((uint16_t *)(&LTC_allGPIOVoltages[modIdx*6 + 2])),
												 *((uint16_t *)(&LTC_allGPIOVoltages[modIdx*6 + 3])),
												 *((uint16_t *)(&LTC_allGPIOVoltages[modIdx*6 + 4])),
												 *((uint16_t *)(&LTC_allGPIOVoltages[modIdx*6 + 5])));

	return 0;
}

uint32_t set_ebm_eb_state(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	ltc_ebm_cmd = LTC_EBM_EB_CTRL;
	uint8_t* pEBState = (uint8_t*)iParam1;
	uint32_t i;

	for (i=0; i < BS_NR_OF_MODULES; i++) {
		ltc_ebm_config[i].eb_state = pEBState[i];
	}
	return 0;
}

#if defined(ITRI_MOD_9)
uint32_t set_ebm_eb_col_state(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	uint8_t* pEBState = (uint8_t*)iParam1;
	uint8_t* pColState = (uint8_t*)iParam2;
	uint32_t i;

	for (i=0; i < BS_NR_OF_MODULES; i++) {
		ltc_ebm_config[i].eb_state = pEBState[i];
	}
	for (i=0; i < BS_NR_OF_COLUMNS; i++) {
		ltc_col_config[i].eb_state = pColState != NULL ? pColState[i]:1;
	}
	//DEBUG_PRINTF_EX("[%d]col_cfg: %u\r\n", __LINE__, ltc_col_config[0].eb_state);
	//DEBUG_PRINTF_EX("[ltc.c:%d]done\r\n", __LINE__);
	ltc_ebm_cmd = LTC_EBM_EB_COL_CTRL;

	ltc_ebm_dump_count = LTC_EBM_DUMP_NUM;	//FIXME; debug
	return 0;
}
#endif
#if defined(ITRI_MOD_12)
uint32_t set_ebm_led_state(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	uint8_t* pLEDState = (uint8_t*)iParam1;
	uint8_t i;

	for (i=0; i < BS_NR_OF_LEDS; i++) {
		if (pLEDState[i] < 2) {
			ltc_led_config[i].eb_state = pLEDState[i] == 1 ? 0:1;	// for sw: 0->OFF, 1->ON; for hw: 0(LOW)->ON, 1(HIGH)-> OFF
		}
	}
	ltc_ebm_cmd = LTC_EBM_EB_COL_CTRL;

	//DEBUG_PRINTF_EX("[%d]LED: %d %d %d %d %d %d\r\n", __LINE__,
	//		pLEDState[0], pLEDState[1], pLEDState[2], pLEDState[3], pLEDState[4], pLEDState[5]);

	return 0;
}
#endif

uint32_t get_LTC_CellVoltages(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	uint32_t modIdx = *(uint32_t*)iParam1;
	char *com_out_buf = (char*)oParam1;
	//char volStr[85] = {0, };

#if 1 // there is a timing issue
	sprintf(com_out_buf, "M[%u] %u %u %u %u %u %u %u ", modIdx,
					ltc_cellvoltage.voltage[modIdx*(BS_NR_OF_BAT_CELLS_PER_MODULE)+0],
					ltc_cellvoltage.voltage[modIdx*(BS_NR_OF_BAT_CELLS_PER_MODULE)+1],
					ltc_cellvoltage.voltage[modIdx*(BS_NR_OF_BAT_CELLS_PER_MODULE)+2],
					ltc_cellvoltage.voltage[modIdx*(BS_NR_OF_BAT_CELLS_PER_MODULE)+3],
					ltc_cellvoltage.voltage[modIdx*(BS_NR_OF_BAT_CELLS_PER_MODULE)+4],
					ltc_cellvoltage.voltage[modIdx*(BS_NR_OF_BAT_CELLS_PER_MODULE)+5],
					ltc_cellvoltage.voltage[modIdx*(BS_NR_OF_BAT_CELLS_PER_MODULE)+6]);
	//DEBUG_PRINTF_EX("Module:%d Cell vol.(mV):%s\r\n", modIdx, volStr);
#else // debug
	DEBUG_PRINTF_EX("Module:%d Cell vol.(mV):%u %u \r\n", modIdx,
			*((uint16_t *)(&LTC_CellVoltages[2*4+modIdx*LTC_NUMBER_OF_LTC_PER_MODULE*24])),
			*((uint16_t *)(&LTC_CellVoltages[2*5+modIdx*LTC_NUMBER_OF_LTC_PER_MODULE*24])));
#endif
	return 0;
}

uint32_t set_curr_cali(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	if (ltc_ebm_cmd == LTC_EBM_NONE) {
		uint8_t i;
		for (i=0; i < BS_NR_OF_MODULES; i++) {
			ltc_ebm_cali[i].isCali = LTC_EBM_MAX_CURR_CAL_CNT;
			ltc_ebm_cali[i].curBat_offset = 0;
			ltc_ebm_cali[i].curMod_offset = 0;
		}
		ltc_ebm_cmd = LTC_EBM_CURR_CALI;
	}
	return 0;
}

uint32_t ltc_task_ALLGPIOMEASUREMEN(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	if (ltc_ebm_cmd == LTC_EBM_NONE) {
		ltc_ebm_cmd = LTC_IDLE_STATE_TASK_ALLGPIOMEASUREMENT;

		memset(LTC_GPIOVoltages, 0x00, sizeof(LTC_GPIOVoltages));
	}
	return 0;
}

uint32_t ltc_task_VOLTAGEMEASUREMENT(void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	if (ltc_ebm_cmd == LTC_EBM_NONE) {
		ltc_ebm_cmd = LTC_IDLE_STATE_TASK_VOLTAGEMEASUREMENT;

		memset(LTC_GPIOVoltages, 0x00, sizeof(LTC_GPIOVoltages));
	}
	return 0;
}

typedef struct {
	char prop[48];
	ltc_prop_funcPtr propFunc;
} LTC_PROP_s;

LTC_PROP_s ltc_props[] = {
	{"get_BS_NR_OF_MODULES", &get_BS_NR_OF_MODULES},
	{"get_BS_NR_OF_BAT_CELLS_PER_MODULE", &get_BS_NR_OF_BAT_CELLS_PER_MODULE},
	{"get_BS_NR_OF_TEMP_SENSORS_PER_MODULE", &get_BS_NR_OF_TEMP_SENSORS_PER_MODULE},
	{"get_LTC_allGPIOVoltages", &get_LTC_allGPIOVoltages},
	{"set_ebm_eb_state", &set_ebm_eb_state},
#if defined(ITRI_MOD_9)
	{"set_ebm_eb_col_state", &set_ebm_eb_col_state},
#endif
#if defined(ITRI_MOD_12)
	{"set_ebm_led_state", &set_ebm_led_state},
#endif
	{"get_LTC_CellVoltages", &get_LTC_CellVoltages},
	{"set_curr_cali", &set_curr_cali},
	{"ltc_task_ALLGPIOMEASUREMEN", &ltc_task_ALLGPIOMEASUREMEN},
	{"ltc_task_VOLTAGEMEASUREMENT", &ltc_task_VOLTAGEMEASUREMENT},
};

uint32_t LTC_Set_Get_Property(char* prop, void* iParam1, void* iParam2, void* oParam1, void* oParam2) {
	uint32_t i, ltc_props_len = sizeof(ltc_props) / sizeof(ltc_props[0]);

	if (prop == NULL) return 1;

	for (i=0; i < ltc_props_len; i++) {
		if (strcmp(prop, ltc_props[i].prop) == 0) {
			if (ltc_props[i].propFunc == NULL) break;
			return ltc_props[i].propFunc(iParam1, iParam2, oParam1, oParam2);
		}
	}
	DEBUG_PRINTF_EX("[E]unknown prop(%s)\r\n", prop);
	return 1;
}
#endif // ITRI_MOD_2
