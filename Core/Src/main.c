/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : LED Kontrol ve Flash Depolama Ödevi
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* Kullanıcı Tanımlamaları ---------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FLASH_VERI_ADRESI  0x0800FC00  // Verinin saklanacağı Flash adresi
/* USER CODE END PD */

/* Değişkenler ---------------------------------------------------------------*/
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
uint32_t led_tekrar_sayisi = 4;        // Kaç kez yanıp söneceği (4-7 arası)
volatile uint32_t zaman_sayaci = 0;    // Timer kesmelerini sayan değişken
volatile uint8_t calisma_durumu = 1;   // 1: Yanıp sönme modu, 0: 5sn bekleme modu
volatile uint32_t anlik_blink = 0;     // O anki döngüde kaçıncı kez yandığı
uint32_t son_buton_zamani = 0;         // Debounce (ark söndürme) için
/* USER CODE END PV */

/* Fonksiyon Prototipleri ----------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void Flash_Veri_Yaz(uint32_t veri);

/* USER CODE BEGIN 0 */
// Flash belleğe veri yazma fonksiyonu
static void Flash_Veri_Yaz(uint32_t veri) {
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_struct = {0};
    uint32_t sayfa_hatasi;
    
    erase_struct.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase_struct.PageAddress = FLASH_VERI_ADRESI;
    erase_struct.NbPages     = 1;
    
    HAL_FLASHEx_Erase(&erase_struct, &sayfa_hatasi);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_VERI_ADRESI, veri);
    HAL_FLASH_Lock();
}
/* USER CODE END 0 */

int main(void) {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM2_Init();

  /* --- f Şıkkı: Flash Kontrolü --- */
  uint32_t okunan_deger = *(__IO uint32_t *)FLASH_VERI_ADRESI;

  if (okunan_deger < 4 || okunan_deger > 7) {
      led_tekrar_sayisi = 4;
      Flash_Veri_Yaz(4);
  } else {
      led_tekrar_sayisi = okunan_deger;
  }

  /* --- g Şıkkı: Fabrika Ayarları (3sn Basılı Tutma) --- */
  // Başlangıçta butona 3sn basılırsa flash 4 yapılır
  uint32_t baslangic_ani = HAL_GetTick();
  while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
      if (HAL_GetTick() - baslangic_ani >= 3000) {
          led_tekrar_sayisi = 4;
          Flash_Veri_Yaz(4);
          // Buton bırakılana kadar bekle
          while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET);
          break;
      }
  }

  // Timer Başlat
  HAL_TIM_Base_Start_IT(&htim2);

  while (1) {
    /* --- d Şıkkı: Buton Kontrolü --- */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
        if (HAL_GetTick() - son_buton_zamani > 300) { // Debounce
            son_buton_zamani = HAL_GetTick();

            // Butonun bırakılmasını bekle (Mekanik gürültüyü önler)
            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET);

            // Sayı artırma mantığı
            if (led_tekrar_sayisi >= 7) led_tekrar_sayisi = 4;
            else led_tekrar_sayisi++;

            // Değişimi kaydet (e şıkkı)
            Flash_Veri_Yaz(led_tekrar_sayisi);

            // Döngüyü baştan başlatmak için parametreleri sıfırla
            zaman_sayaci = 0;
            anlik_blink = 0;
            calisma_durumu = 1;
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        }
    }
  }
}

/* --- a, b ve c Şıkları: Timer Kesme Fonksiyonu --- */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        if (calisma_durumu) { // LED Yanıp Sönme Periyodu
            zaman_sayaci++;
            
            // Tek sayılarda LED YANAR (PC13 ters mantıktır: RESET=ON)
            if (zaman_sayaci % 2 != 0) {
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            } else {
                // Çift sayılarda LED SÖNER (SET=OFF)
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
                anlik_blink++;
            }

            // Belirlenen sayıya ulaşıldıysa
            if (anlik_blink >= led_tekrar_sayisi) {
                calisma_durumu = 0; // 5 saniyelik bekleme moduna geç
                zaman_sayaci = 0;
                anlik_blink = 0;
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); // Sönük kal
            }
        } else { // 5 Saniyelik Bekleme Periyodu
            zaman_sayaci++;
            if (zaman_sayaci >= 5) {
                calisma_durumu = 1; // Tekrar yanmaya başla
                zaman_sayaci = 0;
                anlik_blink = 0;
            }
        }
    }
}

/* --- h Şıkkı: GPIO Yapılandırması --- */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  // PC13 LED (Lojik 1 ile başlar - Kapalı)
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  // PA1 Pinini Yapılandır (Lojik 0 ve sabit kalacak)
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // PA0 Buton (Pull-up)
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // PC13 LED
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* Timer Ayarları: 72MHz için 1sn periyot */
static void MX_TIM2_Init(void) {
  htim2.Instance = TIM2;
  // 72.000.000 / (7199+1) = 10.000 Hz
  htim2.Init.Prescaler = 7199;
  // 10.000 / (9999+1) = 1 Hz (1 Saniye)
  htim2.Init.Period = 9999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  HAL_TIM_Base_Init(&htim2);
  
  HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

void SystemClock_Config(void) {
  // Standart 72MHz HSE yapılandırması buraya gelecek (CubeMX oluşturur)
}

void Error_Handler(void) {
  __disable_irq();
  while (1);
}