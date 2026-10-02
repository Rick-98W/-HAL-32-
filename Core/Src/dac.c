#include "dac.h"
DAC_HandleTypeDef hdac;
void MX_DAC1_Init(void)
{
  DAC_ChannelConfTypeDef config = {0};
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK) Error_Handler();
  config.DAC_Trigger = DAC_TRIGGER_NONE;
  config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &config, DAC_CHANNEL_1) != HAL_OK) Error_Handler();
}
void HAL_DAC_MspInit(DAC_HandleTypeDef *dacHandle)
{
  if (dacHandle->Instance == DAC) { __HAL_RCC_DAC_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE(); }
}
void HAL_DAC_MspDeInit(DAC_HandleTypeDef *dacHandle)
{
  if (dacHandle->Instance == DAC) __HAL_RCC_DAC_CLK_DISABLE();
}
