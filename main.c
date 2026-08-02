/* This is the main.c file of cubeide configured using cubemx for ultra low power wearable project 
   Updated: 02/08/2026
   Github: https://github.com/praveen-chilamakuri/Ultra-Low-Power-Wearable-Project
*/


/* USER CODE BEGIN Header */

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
	STATE_SHT31_POWERUP,
	STATE_SHT31_CONVERTING,
	STATE_AD8232_WARMING,
	STATE_ECG_SAMPLING,
	STATE_ESP32_WAKEUP,
	STATE_ENTERING_STOP
} LP_State_t;

typedef enum {
    ECG_STATUS_OK,
    ECG_STATUS_LEADS_OFF
} ECG_Status_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TEMP_ALERT_LIMIT    38   // temperature threshold
#define ECG_FS              125
#define ECG_BUFFER_SIZE     250
#define ECG_MIN_RR_MS       300
#define ECG_MAX_RR_MS       1500
#define ECG_REFRACTORY_MS   250

#define FP_SCALE            1024
#define ALPHA_BASELINE_SHIFT 7
#define ALPHA_PEAK_SHIFT     4

#define K_NUM               3
#define K_DEN               2

#define BPM_ALERT_THRESHOLD     105    // heart rate threshold
#define BPM_ALERT_COUNT_LIMIT   5

#define MS_PER_SECOND             1000U
#define MS_PER_MINUTE             60000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t sht31_raw[3];        // 3-byte only temperature array for [MSB, LSB, CRC]

volatile LP_State_t current_state;
volatile uint8_t one_shot_timer_flag = 0;
volatile uint16_t ecgBuffer[ECG_BUFFER_SIZE];


static int32_t baseline_fp   = 0;
static int32_t threshold_fp  = 0;
static int32_t prev1_fp      = 0;
static int32_t prev2_fp      = 0;

static uint32_t sampleCounter     = 0;
static uint32_t lastRPeakTimeMs   = 0;
static uint32_t rrHistory[5]      = {0};
static uint8_t  rrCount           = 0;
static uint8_t  rrIndex           = 0;

static uint8_t  bpmAlertCounter = 0;

volatile uint32_t bpm_int         = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Restore_System_Clocks(void);
void ProcessECGBuffer(void);
void Prepare_For_Sleep(void);
void Restore_Peripherals(void);
static ECG_Status_t RunECGSamplingAndProcessing(uint8_t *alert_detected2, uint8_t *alert_detected3);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
       Restore_System_Clocks();     // restore clocks after stop wakeup
       HAL_ResumeTick();          // Resume SysTick timer
       Restore_Peripherals();     // Re-initialize I2C, ADC, UART, and GPIOs

      printf("\r\nExited Stop Mode \r\n");

      // power on SHT31 & AD8232

      HAL_GPIO_WritePin(SHT31_PWR_GPIO_Port, SHT31_PWR_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(AD8232_SDN_GPIO_Port, AD8232_SDN_Pin, GPIO_PIN_SET);

        HAL_Delay(2); // for SHT31 internal micro-boot

      uint8_t cmd[2] = {0x24, 0x0B};   // no stretch, medium repeatability sht31 command
      HAL_I2C_Master_Transmit(&hi2c1, (0x44 << 1), cmd, 2, 10);

      // sleep until SHT31 internal measurement completes

        current_state = STATE_SHT31_CONVERTING;
        one_shot_timer_flag = 0;

      __HAL_TIM_SET_AUTORELOAD(&htim2, 4999);
      __HAL_TIM_SET_COUNTER(&htim2, 0);
      __HAL_RCC_TIM2_CLK_ENABLE();
      HAL_TIM_Base_Start_IT(&htim2);

      HAL_SuspendTick();
      while (!one_shot_timer_flag)
      {
          HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
      }
      HAL_ResumeTick();

      HAL_TIM_Base_Stop_IT(&htim2);
      __HAL_RCC_TIM2_CLK_DISABLE();


      // Read the raw temperature only data from SHT31

      HAL_I2C_Master_Receive(&hi2c1, (0x44 << 1), sht31_raw, 3, 10);
      HAL_GPIO_WritePin(SHT31_PWR_GPIO_Port, SHT31_PWR_Pin, GPIO_PIN_RESET);   // Power off SHT31


      uint16_t raw_temp = (sht31_raw[0] << 8) | sht31_raw[1];
      int32_t current_temperature = -45 + ((175 * (uint32_t)raw_temp) / 65535);

      printf("Temperature: %ld °C\r\n", current_temperature);

      uint8_t alert_detected1 = 0;
      if (current_temperature > TEMP_ALERT_LIMIT)
      {
          printf("Temperature Alert \r\n");
          alert_detected1 = 1;
      }

      // Sleep until AD8232 fully stabilize internally

      current_state = STATE_AD8232_WARMING;
      one_shot_timer_flag = 0;

      __HAL_TIM_SET_AUTORELOAD(&htim2, 142999);
      __HAL_TIM_SET_COUNTER(&htim2, 0);
      __HAL_RCC_TIM2_CLK_ENABLE();
      HAL_TIM_Base_Start_IT(&htim2);

      HAL_SuspendTick();
      while (!one_shot_timer_flag)
      {
    	  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

      }
      HAL_ResumeTick();

      HAL_TIM_Base_Stop_IT(&htim2);
      __HAL_RCC_TIM2_CLK_DISABLE();


      // Check leads off status before setting up ADC to avoid unnecessary sampling that results in garbage values out

      uint8_t alert_detected2 = 0;
      uint8_t alert_detected3 = 0;

      RunECGSamplingAndProcessing(&alert_detected2,&alert_detected3);


      if (alert_detected1 || alert_detected2 || alert_detected3)    // temperature threshold crossed or leads off detected or heartrate threshold crossed
      {
          printf("Alert, waking up ESP32 \r\n");
          HAL_GPIO_WritePin(ESP32_WKUP_GPIO_Port, ESP32_WKUP_Pin, GPIO_PIN_SET);

          current_state = STATE_ESP32_WAKEUP;
          one_shot_timer_flag = 0;

                      __HAL_TIM_SET_AUTORELOAD(&htim2, 499);
                      __HAL_TIM_SET_COUNTER(&htim2, 0);
                      __HAL_RCC_TIM2_CLK_ENABLE();
                      HAL_TIM_Base_Start_IT(&htim2);

                      HAL_SuspendTick();
                      while (!one_shot_timer_flag)
                      {
                          HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
                      }
                      HAL_ResumeTick();

                      HAL_TIM_Base_Stop_IT(&htim2);
                      __HAL_RCC_TIM2_CLK_DISABLE();

          HAL_GPIO_WritePin(ESP32_WKUP_GPIO_Port, ESP32_WKUP_Pin, GPIO_PIN_RESET);
      }
      else
      {
          printf("No Alerts, entering STOP mode \r\n");
      }

      // Ensure all external sensors and controls are driven low
            HAL_GPIO_WritePin(SHT31_PWR_GPIO_Port, SHT31_PWR_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(AD8232_SDN_GPIO_Port, AD8232_SDN_Pin, GPIO_PIN_RESET);

            // De-initialize active peripherals and switch pins to Analog mode
            Prepare_For_Sleep();

            // Suspend SysTick to prevent periodic wakeup interrupts
            HAL_SuspendTick();

            // Enable Flash Power Down during STOP mode for maximum savings
            HAL_PWREx_EnableFlashPowerDown();

     // Enter STOP mode

     current_state = STATE_ENTERING_STOP;
     HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
     HAL_PWREx_DisableFlashPowerDown();
  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void Restore_System_Clocks(void)
{
	__HAL_RCC_HSI_ENABLE();
	while(__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == RESET);
	__HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI);
	while(__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_SYSCLKSOURCE_STATUS_HSI)
	{
	}

}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM2)
	{
		HAL_TIM_Base_Stop_IT(&htim2);
		one_shot_timer_flag = 1;
	}
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADC1)
	{
		HAL_TIM_Base_Stop(&htim3);
		HAL_ADC_Stop_DMA(&hadc1);
		one_shot_timer_flag = 1;
	}
}
void ProcessECGBuffer(void)
    {
        for (uint16_t i = 0; i < ECG_BUFFER_SIZE; i++)
        {
            int32_t sample_fp = ((int32_t)ecgBuffer[i]) * FP_SCALE;

            baseline_fp += ((sample_fp - baseline_fp) >> ALPHA_BASELINE_SHIFT);
            int32_t hp_fp = sample_fp - baseline_fp;

            int32_t smooth_fp = (hp_fp + prev1_fp + prev2_fp) / 3;
            prev2_fp = prev1_fp;
            prev1_fp = hp_fp;

            int32_t rect_fp = (smooth_fp >= 0) ? smooth_fp : -smooth_fp;

            threshold_fp += ((rect_fp - threshold_fp) >> ALPHA_PEAK_SHIFT);

            uint32_t sampleTimeMs = sampleCounter * MS_PER_SECOND / ECG_FS;
            sampleCounter++;

            uint8_t inRefractory = 0;
            if (lastRPeakTimeMs != 0)
            {
                uint32_t dt = sampleTimeMs - lastRPeakTimeMs;
                if (dt < ECG_REFRACTORY_MS)
                    inRefractory = 1;
            }

            if (!inRefractory &&
                (rect_fp * K_DEN > threshold_fp * K_NUM))
            {
                if (lastRPeakTimeMs != 0)
                {
                    uint32_t rrMs = sampleTimeMs - lastRPeakTimeMs;

                    if (rrMs >= ECG_MIN_RR_MS && rrMs <= ECG_MAX_RR_MS)
                    {
                        rrHistory[rrIndex] = rrMs;
                        rrIndex = (rrIndex + 1) % 5;
                        if (rrCount < 5) rrCount++;

                        uint32_t sumRR = 0;
                        for (uint8_t k = 0; k < rrCount; k++)
                            sumRR += rrHistory[k];

                        uint32_t avgRR = sumRR / rrCount;

                        bpm_int = MS_PER_MINUTE / avgRR;
                    }
                }

                lastRPeakTimeMs = sampleTimeMs;
            }
        }

        if (bpm_int > BPM_ALERT_THRESHOLD)
        {
            if (bpmAlertCounter < BPM_ALERT_COUNT_LIMIT)
                bpmAlertCounter++;
        }
        else
        {
            bpmAlertCounter = 0;
        }

    }

void Prepare_For_Sleep(void)
{
    //  De-initialize peripherals
    HAL_I2C_DeInit(&hi2c1);
    HAL_ADC_DeInit(&hadc1);
    HAL_UART_DeInit(&huart2);

    //  Disable peripheral clocks
    __HAL_RCC_I2C1_CLK_DISABLE();
    __HAL_RCC_ADC1_CLK_DISABLE();
    __HAL_RCC_USART2_CLK_DISABLE();

    //  Reconfigure Peripheral Pins & Unused Pins to Analog
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    // I2C1 Pins
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // ADC1 Pin
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // USART2 Pins
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Leads-Off Detection Pins
    GPIO_InitStruct.Pin = LO_PLUS_Pin | LO_MINUS_Pin;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

void Restore_Peripherals(void)
{
    //  Re-enable Peripheral Clocks
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    //  Re-initialize GPIOs & Peripherals
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
}

static ECG_Status_t RunECGSamplingAndProcessing(uint8_t *alert_detected2, uint8_t *alert_detected3)
     {
         *alert_detected2 = 0;
         *alert_detected3 = 0;

            bpm_int          = 0;
            baseline_fp      = 0;
            threshold_fp     = 0;
            prev1_fp         = 0;
            prev2_fp         = 0;
            lastRPeakTimeMs  = 0;
            rrCount          = 0;
            rrIndex          = 0;
            sampleCounter    = 0;


         // Check leads-off first
         if (HAL_GPIO_ReadPin(LO_PLUS_GPIO_Port, LO_PLUS_Pin) == GPIO_PIN_SET ||
             HAL_GPIO_ReadPin(LO_MINUS_GPIO_Port, LO_MINUS_Pin) == GPIO_PIN_SET)
         {
             printf("Leads OFF \r\n");

             // Reset ECG state variables
             bpm_int        = 0;
             baseline_fp    = 0;
             threshold_fp   = 0;
             prev1_fp       = 0;
             prev2_fp       = 0;
             lastRPeakTimeMs = 0;
             rrCount        = 0;
             rrIndex        = 0;
             bpmAlertCounter = 0;
             *alert_detected2 = 1;


             // Power down AD8232 immediately
             HAL_GPIO_WritePin(AD8232_SDN_GPIO_Port, AD8232_SDN_Pin, GPIO_PIN_RESET);

             return ECG_STATUS_LEADS_OFF;
         }


        // Normal ECG sampling path
         current_state       = STATE_ECG_SAMPLING;
         one_shot_timer_flag = 0;

         __HAL_TIM_SET_AUTORELOAD(&htim3, 7999);  // 8 ms period at current clock
         __HAL_TIM_SET_COUNTER(&htim3, 0);

         __HAL_RCC_TIM3_CLK_ENABLE();
         __HAL_RCC_ADC1_CLK_ENABLE();

         HAL_TIM_Base_Start(&htim3);
         HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ecgBuffer, ECG_BUFFER_SIZE);

         HAL_SuspendTick();
         while (!one_shot_timer_flag)
         {
             HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
         }
         HAL_ResumeTick();

         HAL_TIM_Base_Stop(&htim3);
         HAL_ADC_Stop_DMA(&hadc1);

         __HAL_RCC_TIM3_CLK_DISABLE();
         __HAL_RCC_ADC1_CLK_DISABLE();

         // Process buffer
         one_shot_timer_flag = 0;
         ProcessECGBuffer();
         printf("BPM = %lu\r\n", bpm_int);


         if (bpmAlertCounter >= BPM_ALERT_COUNT_LIMIT)
         {
             printf("BPM Alert %lu\r\n", bpm_int);
             *alert_detected3 = 1;
         }

         // Power down AD8232 after sampling
         HAL_GPIO_WritePin(AD8232_SDN_GPIO_Port, AD8232_SDN_Pin, GPIO_PIN_RESET);

         return ECG_STATUS_OK;
     }

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
