/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32c0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

extern ADC_HandleTypeDef hadc1;

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim17;

extern UART_HandleTypeDef huart1;


/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define BTN_BACK_Pin GPIO_PIN_9
#define BTN_BACK_GPIO_Port GPIOB
#define RDS_CLK_Pin GPIO_PIN_0
#define RDS_CLK_GPIO_Port GPIOA
#define FLASH_HOLD_Pin GPIO_PIN_1
#define FLASH_HOLD_GPIO_Port GPIOA
#define VFO_READING_Pin GPIO_PIN_2
#define VFO_READING_GPIO_Port GPIOA
#define BACKLIGHT_IN_Pin GPIO_PIN_3
#define BACKLIGHT_IN_GPIO_Port GPIOA
#define RDS_DAT_Pin GPIO_PIN_4
#define RDS_DAT_GPIO_Port GPIOA
#define DISP_D0_Pin GPIO_PIN_0
#define DISP_D0_GPIO_Port GPIOB
#define DISP_D1_Pin GPIO_PIN_1
#define DISP_D1_GPIO_Port GPIOB
#define DISP_D2_Pin GPIO_PIN_2
#define DISP_D2_GPIO_Port GPIOB
#define BACKLIGHT_OUT_Pin GPIO_PIN_6
#define BACKLIGHT_OUT_GPIO_Port GPIOC
#define BTN_CLEAR_Pin GPIO_PIN_10
#define BTN_CLEAR_GPIO_Port GPIOA
#define BTN_RIGHT_Pin GPIO_PIN_11
#define BTN_RIGHT_GPIO_Port GPIOA
#define BTN_LEFT_Pin GPIO_PIN_12
#define BTN_LEFT_GPIO_Port GPIOA
#define FLASH_CS_Pin GPIO_PIN_15
#define FLASH_CS_GPIO_Port GPIOA
#define DISP_D3_Pin GPIO_PIN_3
#define DISP_D3_GPIO_Port GPIOB
#define CS_SD_Pin GPIO_PIN_4
#define CS_SD_GPIO_Port GPIOB
#define DISP_E1_Pin GPIO_PIN_5
#define DISP_E1_GPIO_Port GPIOB
#define DISP_RS_Pin GPIO_PIN_6
#define DISP_RS_GPIO_Port GPIOB
#define DISP_E2_Pin GPIO_PIN_7
#define DISP_E2_GPIO_Port GPIOB
#define BTN_RESET_Pin GPIO_PIN_8
#define BTN_RESET_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
