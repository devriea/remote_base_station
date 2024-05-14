/*
 * subghz.c
 *
 *  Created on: Mar 23, 2024
 *      Author: adevries
 */

#include "subghz.h"

#include "stm32wlxx_ll_gpio.h"
#include "stm32wlxx_ll_bus.h"
#include "stm32wlxx_ll_rcc.h"
#include "stm32wlxx_ll_utils.h"

#include "mprintf.h"

#include <stdint.h>

#define RF_SW_CTRL3_Pin LL_GPIO_PIN_3
#define RF_SW_CTRL3_GPIO_Port GPIOC
#define RF_SW_CTRL2_Pin LL_GPIO_PIN_5
#define RF_SW_CTRL2_GPIO_Port GPIOC
#define RF_SW_CTRL1_Pin LL_GPIO_PIN_4
#define RF_SW_CTRL1_GPIO_Port GPIOC


#define RF_FREQ						868000000
#define BIT_RATE					50000
#define FREQ_DEVIATION				25000
#define XTAL_FREQ					32000000

#define SYNCWORD_BASEADDRESS		0x06C0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define RADIO_MODE_STANDBY_RC        0x02
#define RADIO_MODE_STANDBY_HSE32	 0x03
#define	RADIO_MODE_FS				 0x04
#define	RADIO_MODE_RX				 0x05
#define RADIO_MODE_TX                0x06

#define	RADIO_COMMAND_RX_DONE		 0x02
#define	RADIO_COMMAND_TIMEOUT		 0x03
#define RADIO_COMMAND_TX_DONE        0x06

#define RADIO_MODE_BITFIELD          0x70
#define RADIO_STATUS_BITFIELD        0x0E

#define SX_FREQ_TO_CHANNEL( channel, freq )                                  \
do                                                                           \
{                                                                            \
  channel = (uint32_t) ((((uint64_t) freq)<<25)/(XTAL_FREQ) );               \
}while( 0 )

struct __attribute__((__packed__)) sRadioParams {
	uint16_t PbLength;
	uint8_t PbDetLength;
	uint8_t SyncWordLength;
	uint8_t AddrComp;
	uint8_t PktType;
	uint8_t PayloadLength;
	uint8_t CrcType;
	uint8_t Whitening;
};

static SUBGHZ_HandleTypeDef hsubghz;

struct sRadioParams params = {
	.PbLength = 32,
	.PbDetLength = 0x07,
	.SyncWordLength = 32,
	.AddrComp = 0,
	.PktType = 0,
	.PayloadLength = 1,
	.CrcType = 1,
	.Whitening = 0
};


HAL_StatusTypeDef SetRfFrequency(SUBGHZ_HandleTypeDef *hsubghz, uint32_t frequency);
HAL_StatusTypeDef SetModulationParams(SUBGHZ_HandleTypeDef *hsubghz);

void HAL_SUBGHZ_MspInit(SUBGHZ_HandleTypeDef* hsubghz);
void HAL_SUBGHZ_MspDeInit(SUBGHZ_HandleTypeDef* hsubghz);

static void MX_NVIC_Init(void);

/**
  * @brief SUBGHZ Initialization Function
  * @param None
  * @retval None
  */
void MX_SUBGHZ_Init(void)
{
  hsubghz.Init.BaudratePrescaler = SUBGHZSPI_BAUDRATEPRESCALER_8;
  if (HAL_SUBGHZ_Init(&hsubghz) != HAL_OK)
  {
    // Error_Handler();
  }
  if(subghz_init(&hsubghz) != HAL_OK)
  {
    // Error_Handler();
  }

  MX_NVIC_Init();

}


HAL_StatusTypeDef subghz_init(SUBGHZ_HandleTypeDef *hsubghz)
{
	uint8_t RadioResult = 0x00;
	uint8_t RadioParam  = 0x00;
	uint8_t RadioMode   = 0x00;
//	uint8_t RadioStatus = 0x00;

	const uint8_t syncword[] = {0x00, 0x00, 0x00, 0x00, 0x48, 0xDF, 0x70, 0x72};

	HAL_StatusTypeDef result;

	/* Set Sleep Mode */
	result = HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_SET_SLEEP, &RadioParam, 1);
	if(result != HAL_OK){
		return result;
	}

	/* Set Standby Mode */
	result = HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_SET_STANDBY, &RadioParam, 1);
	if(result != HAL_OK){
		return result;
	}

	const uint8_t buf_addr[2] = {0x80, 0x00};

	result = HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_SET_BUFFERBASEADDRESS, buf_addr, 2);
	if(result != HAL_OK){
		return result;
	}

	result = HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_SET_PACKETTYPE, &RadioParam, 1);
	if(result != HAL_OK){
		return result;
	}

	result = HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_SET_PACKETPARAMS, (uint8_t*)&params, 9);
	if(result != HAL_OK){
		return result;
	}


	result = HAL_SUBGHZ_WriteRegisters(hsubghz, SYNCWORD_BASEADDRESS, syncword, sizeof(syncword));
	if(result != HAL_OK){
		return result;
	}

	result = SetRfFrequency(hsubghz, RF_FREQ);
	if(result != HAL_OK){
		return result;
	}

	result = SetModulationParams(hsubghz);
	if(result != HAL_OK){
		return result;
	}

//	uint16_t buf[4] = {0};
//	//  buf[0] = 0x024E;
//	//  buf[1] = 0x024E;
//	buf[0] = 0x03FF;
//	buf[1] = 0x03FF;

	const uint8_t buf[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};

	result = HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_CFG_DIOIRQ, buf, 8);
	if(result != HAL_OK){
		return result;
	}

	/* Retrieve Status from SUBGHZ Radio */
	result = HAL_SUBGHZ_ExecGetCmd(hsubghz, RADIO_GET_STATUS, &RadioResult, 1);
	if(result != HAL_OK){
		return result;
	}

	/* Format Mode and Status receive from SUBGHZ Radio */
	RadioMode = ((RadioResult & RADIO_MODE_BITFIELD) >> 4);

	/* Check if SUBGHZ Radio is in RADIO_MODE_STANDBY_RC mode */
	if(RadioMode != RADIO_MODE_STANDBY_RC)
	{
		return HAL_ERROR;
	}

	return HAL_OK;
}

void HAL_SUBGHZ_MspInit(SUBGHZ_HandleTypeDef* hsubghz)
{
    LL_APB3_GRP1_EnableClock(LL_APB3_GRP1_PERIPH_SUBGHZSPI);
    LL_RCC_HSE_EnableTcxo();
    LL_RCC_HSE_Enable();

    while (LL_RCC_HSE_IsReady() == 0)
    {}
}

void HAL_SUBGHZ_MspDeInit(SUBGHZ_HandleTypeDef* hsubghz)
{
    LL_APB3_GRP1_DisableClock(LL_APB3_GRP1_PERIPH_SUBGHZSPI);
    NVIC_DisableIRQ(SUBGHZ_Radio_IRQn);
}

void subghz_radio_getstatus(void)
{
	uint8_t RadioResult = 0x00;
	HAL_SUBGHZ_ExecGetCmd(&hsubghz, RADIO_GET_STATUS, &RadioResult, 1);
  	printf_("RR: 0x%02x\n", RadioResult);
}

HAL_StatusTypeDef single_rx_block(SUBGHZ_HandleTypeDef *hsubghz)
{
	uint8_t RadioCmd[3] = {0xFF, 0xFF, 0xFE};
	return(HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_SET_RX, RadioCmd, 3));

	uint8_t RadioMode = 0;
	uint8_t RadioResult = 0;

	do
	{
	  /* Retrieve Status from SUBGHZ Radio */
	  uint8_t result = HAL_SUBGHZ_ExecGetCmd(hsubghz, RADIO_GET_STATUS, &RadioResult, 1);
	  if (result != HAL_OK)
	  {
		printf_("error: 0x%02x\n", result);
	  }
	  printf_("RR: 0x%02x\n", RadioResult);

	  printf_("delay\n");
	  LL_mDelay(500);

	  /* Format Mode and Status receive from SUBGHZ Radio */
	  RadioMode = ((RadioResult & RADIO_MODE_BITFIELD) >> 4);
	}
	while (RadioMode != RADIO_MODE_STANDBY_RC);
}

HAL_StatusTypeDef continuous_rx(void)
{
	uint8_t RadioCmd[3] = {0xFF, 0xFF, 0xFF};
	return(HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, RadioCmd, 3));
}

/**
  * @brief  Configure Radio Switch.
  * @param  Config: Specifies the Radio RF switch path to be set.
  *         This parameter can be one of following parameters:
  *           @arg RADIO_SWITCH_OFF
  *           @arg RADIO_SWITCH_RX
  *           @arg RADIO_SWITCH_RFO_LP
  *           @arg RADIO_SWITCH_RFO_HP
  * @retval BSP status
  */
int32_t ConfigRFSwitch(BSP_RADIO_Switch_TypeDef Config)
{
  switch (Config)
  {
    case RADIO_SWITCH_OFF:
    {
      /* Turn off switch */
	  LL_GPIO_ResetOutputPin(GPIOC, RF_SW_CTRL1_Pin | RF_SW_CTRL2_Pin | RF_SW_CTRL3_Pin);
      break;
    }
    case RADIO_SWITCH_RX:
    {
      /*Turns On in Rx Mode the RF Switch */
	  LL_GPIO_SetOutputPin(GPIOC, RF_SW_CTRL1_Pin | RF_SW_CTRL3_Pin);
	  LL_GPIO_ResetOutputPin(GPIOC, RF_SW_CTRL2_Pin);
      break;
    }
    case RADIO_SWITCH_RFO_LP:
    {
      /*Turns On in Tx Low Power the RF Switch */
	  LL_GPIO_SetOutputPin(GPIOC, RF_SW_CTRL1_Pin | RF_SW_CTRL2_Pin | RF_SW_CTRL3_Pin);
      break;
    }
    case RADIO_SWITCH_RFO_HP:
    {
      /*Turns On in Tx High Power the RF Switch */
	  LL_GPIO_SetOutputPin(GPIOC, RF_SW_CTRL3_Pin);
	  LL_GPIO_ResetOutputPin(GPIOC, RF_SW_CTRL1_Pin);
	  LL_GPIO_SetOutputPin(GPIOC, RF_SW_CTRL2_Pin);
      break;
    }
    default:
      break;
  }

  return 0;
}


HAL_StatusTypeDef SetRfFrequency(SUBGHZ_HandleTypeDef *hsubghz, uint32_t frequency)
{
    uint8_t buf[4];
    uint32_t chan = 0;


    SX_FREQ_TO_CHANNEL(chan, frequency);
    buf[0] = ( uint8_t )( ( chan >> 24 ) & 0xFF );
    buf[1] = ( uint8_t )( ( chan >> 16 ) & 0xFF );
    buf[2] = ( uint8_t )( ( chan >> 8 ) & 0xFF );
    buf[3] = ( uint8_t )( chan & 0xFF );
    return(HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_SET_RFFREQUENCY, buf, 4));
}

HAL_StatusTypeDef SetModulationParams(SUBGHZ_HandleTypeDef *hsubghz)
{
    uint32_t tempVal = 0;
    uint8_t buf[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	tempVal = ( uint32_t )(( 32 * XTAL_FREQ ) / BIT_RATE );
	buf[0] = ( tempVal >> 16 ) & 0xFF;		// BR
	buf[1] = ( tempVal >> 8 ) & 0xFF;		// BR
	buf[2] = tempVal & 0xFF;				// BR
	buf[3] = 0;					// modulation
	buf[4] = 0x13;				// RX bandwidth
	//calculate Fdev
	SX_FREQ_TO_CHANNEL(tempVal, FREQ_DEVIATION);
	buf[5] = ( tempVal >> 16 ) & 0xFF;		// FDev
	buf[6] = ( tempVal >> 8 ) & 0xFF;		// FDev
	buf[7] = ( tempVal& 0xFF );				// FDev

	return(HAL_SUBGHZ_ExecSetCmd(hsubghz, RADIO_SET_MODULATIONPARAMS, buf, 8));
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* SUBGHZ_Radio_IRQn interrupt configuration */
  NVIC_SetPriority(SUBGHZ_Radio_IRQn, 0);
  NVIC_EnableIRQ(SUBGHZ_Radio_IRQn);
}

void SUBGHZ_Radio_IRQHandler(void)
{
  HAL_SUBGHZ_IRQHandler(&hsubghz);
}