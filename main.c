/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : LCD Driver - Display "DAVIDE" on 16x2 LCD
  *                   STM32C031C6 - 4-bit mode via LL library
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "stm32c0xx_ll_gpio.h"
#include "stm32c0xx_ll_utils.h"
/* USER CODE END Includes */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* USER CODE BEGIN PFP */

/* ---------- Macro per semplificare accesso GPIO (come da slide) ---------- */
#define RESET(c)    LL_GPIO_ResetOutputPin(c##_GPIO_Port, c##_Pin)
#define SET(c)      LL_GPIO_SetOutputPin(c##_GPIO_Port, c##_Pin)
#define READ(c)     LL_GPIO_IsInputPinSet(c##_GPIO_Port, c##_Pin)
#define DIR_OUT(c)  LL_GPIO_SetPinMode(c##_GPIO_Port, c##_Pin, LL_GPIO_MODE_OUTPUT)
#define DIR_IN(c)   LL_GPIO_SetPinMode(c##_GPIO_Port, c##_Pin, LL_GPIO_MODE_INPUT)

/* ---------- Prototipi funzioni LCD ---------- */
void Delay_uS(int uSTim);
void setLcdDataPort(uint8_t portDato);
int  lcdCheckBusy(void);
void lcdSendCmd(char cmd);
void lcdSendChar(char data);
void lcdInit(void);
void lcdPrint(const char *str);
void lcdSetCursor(uint8_t col, uint8_t row);

/* USER CODE END PFP */


/* ============================================================
   DELAY microsecondi
   Usa HAL_Delay per semplicità (1 ms = 1000 uS).
   Per delay più precisi si userebbe un timer hardware.
   ============================================================ */
void Delay_uS(int uSTim)
{
    /* Busy-wait loop: ~8 MHz HSE => ~125ns per ciclo
       Ogni iterazione ~4 istruzioni => ~500ns
       Per 1 uS => ~2 iterazioni */
    volatile int cnt = (uSTim * 2);
    while (cnt-- > 0)
    {
        __NOP();
        __NOP();
        __NOP();
        __NOP();
    }
}


/* ============================================================
   SET LCD DATA PORT (nibble DB4-DB7)
   portDato: bit0 -> DB4, bit1 -> DB5, bit2 -> DB6, bit3 -> DB7
   ============================================================ */
void setLcdDataPort(uint8_t portDato)
{
    if (portDato & 0x1) { SET(DISPDB4); } else { RESET(DISPDB4); }
    if (portDato & 0x2) { SET(DISPDB5); } else { RESET(DISPDB5); }
    if (portDato & 0x4) { SET(DISPDB6); } else { RESET(DISPDB6); }
    if (portDato & 0x8) { SET(DISPDB7); } else { RESET(DISPDB7); }
}


/* ============================================================
   CHECK BUSY FLAG
   Legge DB7 in modalità lettura finché il display non è pronto
   ============================================================ */
int lcdCheckBusy(void)
{
    uint8_t tmpLcdData;
    int delay2 = 0;

    /* Imposta DB4-DB7 come input */
    DIR_IN(DISPDB4); DIR_IN(DISPDB5);
    DIR_IN(DISPDB6); DIR_IN(DISPDB7);

    RESET(DISPRS);   /* RS = 0 -> comando */
    SET(DISPRW);     /* RW = 1 -> lettura */

    do {
        Delay_uS(1);
        SET(DISPEN);
        Delay_uS(1);
        tmpLcdData = READ(DISPDB7);  /* legge busy flag (bit7) */
        RESET(DISPEN);
        Delay_uS(1);
        /* Secondo nibble (necessario in 4-bit mode) */
        SET(DISPEN);
        Delay_uS(1);
        RESET(DISPEN);

        if (tmpLcdData == 0)
            break;

    } while (delay2++ < 200);

    RESET(DISPRW);   /* RW = 0 -> scrittura */

    /* Ripristina DB4-DB7 come output */
    DIR_OUT(DISPDB4); DIR_OUT(DISPDB5);
    DIR_OUT(DISPDB6); DIR_OUT(DISPDB7);

    return (int)tmpLcdData;
}


/* ============================================================
   INVIA NIBBLE con impulso Enable
   ============================================================ */
static void lcdSendNibble(uint8_t nibble)
{
    setLcdDataPort(nibble & 0x0F);
    Delay_uS(1);
    SET(DISPEN);
    Delay_uS(1);
    RESET(DISPEN);
    Delay_uS(1);
}


/* ============================================================
   INVIA COMANDO AL DISPLAY (RS=0)
   ============================================================ */
void lcdSendCmd(char cmd)
{
    lcdCheckBusy();

    RESET(DISPRS);   /* RS = 0 -> registro comandi */
    RESET(DISPRW);   /* RW = 0 -> scrittura        */

    /* Nibble alto prima */
    lcdSendNibble((cmd >> 4) & 0x0F);
    /* Nibble basso poi */
    lcdSendNibble(cmd & 0x0F);
}


/* ============================================================
   INVIA CARATTERE AL DISPLAY (RS=1)
   ============================================================ */
void lcdSendChar(char data)
{
    lcdCheckBusy();

    SET(DISPRS);     /* RS = 1 -> registro dati */
    RESET(DISPRW);   /* RW = 0 -> scrittura     */

    /* Nibble alto prima */
    lcdSendNibble((data >> 4) & 0x0F);
    /* Nibble basso poi */
    lcdSendNibble(data & 0x0F);
}


/* ============================================================
   INIZIALIZZAZIONE LCD (sequenza standard 4-bit mode)
   Segue il diagramma di flusso dal datasheet ST7066U
   ============================================================ */
void lcdInit(void)
{
    /* Reset segnali di controllo */
    RESET(DISPEN);
    RESET(DISPRS);
    RESET(DISPRW);

    /* Attesa >15ms dopo power-on */
    HAL_Delay(50);

    /* --- Passo 1: Function Set (8-bit, prima chiamata) --- */
    setLcdDataPort(0x3);
    Delay_uS(1);
    SET(DISPEN);
    Delay_uS(1);
    RESET(DISPEN);
    HAL_Delay(5);   /* attesa >4.1ms */

    /* --- Passo 2: Function Set (8-bit, seconda chiamata) --- */
    setLcdDataPort(0x3);
    Delay_uS(1);
    SET(DISPEN);
    Delay_uS(1);
    RESET(DISPEN);
    Delay_uS(200);  /* attesa >100us */

    /* --- Passo 3: Function Set (8-bit, terza chiamata) --- */
    setLcdDataPort(0x3);
    Delay_uS(1);
    SET(DISPEN);
    Delay_uS(1);
    RESET(DISPEN);
    Delay_uS(200);

    /* --- Passo 4: Passa a 4-bit mode --- */
    setLcdDataPort(0x2);
    Delay_uS(1);
    SET(DISPEN);
    Delay_uS(1);
    RESET(DISPEN);
    Delay_uS(200);

    /* --- Da qui in poi usa lcdSendCmd (4-bit completo) --- */

    /* Function Set: 4-bit, 2 righe, font 5x8 -> 0x28 */
    lcdSendCmd(0x28);

    /* Display OFF -> 0x08 */
    lcdSendCmd(0x08);

    /* Display Clear -> 0x01 */
    lcdSendCmd(0x01);
    HAL_Delay(2);   /* attesa >1.52ms */

    /* Entry Mode Set: incremento cursore, no shift -> 0x06 */
    lcdSendCmd(0x06);

    /* Display ON, cursore OFF, blink OFF -> 0x0C */
    lcdSendCmd(0x0C);
}


/* ============================================================
   STAMPA STRINGA
   ============================================================ */
void lcdPrint(const char *str)
{
    while (*str)
    {
        lcdSendChar(*str++);
    }
}


/* ============================================================
   POSIZIONA CURSORE
   col: 0-15, row: 0 (riga 1) o 1 (riga 2)
   ============================================================ */
void lcdSetCursor(uint8_t col, uint8_t row)
{
    uint8_t addr;
    if (row == 0)
        addr = 0x00 + col;   /* Riga 1: DDRAM 0x00..0x0F */
    else
        addr = 0x40 + col;   /* Riga 2: DDRAM 0x40..0x4F */

    lcdSendCmd(0x80 | addr); /* Set DDRAM Address */
}


/* ============================================================
   MAIN
   ============================================================ */
int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    HAL_Init();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    SystemClock_Config();

    /* USER CODE BEGIN SysInit */
    /* USER CODE END SysInit */

    MX_GPIO_Init();

    /* USER CODE BEGIN 2 */

    /* Inizializza il display LCD */
    lcdInit();

    /* --- Riga 1: "DAVIDE" centrato (posizione 5 su 16 char) --- */
    lcdSetCursor(5, 0);
    lcdPrint("DAVIDE!");

    /* --- Riga 2: messaggio aggiuntivo --- */
    lcdSetCursor(4, 1);
    lcdPrint("FEDERICO!");

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* Il display mantiene il contenuto autonomamente */
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}


/* ============================================================
   SYSTEM CLOCK CONFIG (generato da CubeMX - HSE 8MHz)
   ============================================================ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSE;
    RCC_ClkInitStruct.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ============================================================
   GPIO INIT (invariato rispetto al progetto originale)
   ============================================================ */
static void MX_GPIO_Init(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOC);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOF);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

    /* Reset tutti i pin LCD */
    LL_GPIO_ResetOutputPin(DISPDB4_GPIO_Port, DISPDB4_Pin);
    LL_GPIO_ResetOutputPin(DISPEN_GPIO_Port,  DISPEN_Pin);
    LL_GPIO_ResetOutputPin(DISPDB7_GPIO_Port, DISPDB7_Pin);
    LL_GPIO_ResetOutputPin(DISPDB5_GPIO_Port, DISPDB5_Pin);
    LL_GPIO_ResetOutputPin(DISPDB6_GPIO_Port, DISPDB6_Pin);
    LL_GPIO_ResetOutputPin(DISPRW_GPIO_Port,  DISPRW_Pin);
    LL_GPIO_ResetOutputPin(DISPRS_GPIO_Port,  DISPRS_Pin);

    /* USART2 TX */
    GPIO_InitStruct.Pin        = VCP_USART2_TX_Pin;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_NO;
    GPIO_InitStruct.Alternate  = LL_GPIO_AF_1;
    LL_GPIO_Init(VCP_USART2_TX_GPIO_Port, &GPIO_InitStruct);

    /* USART2 RX */
    GPIO_InitStruct.Pin        = VCP_USART2_RX_Pin;
    LL_GPIO_Init(VCP_USART2_RX_GPIO_Port, &GPIO_InitStruct);

    /* Pin LCD come output push-pull */
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_NO;

    GPIO_InitStruct.Pin = DISPDB4_Pin;
    LL_GPIO_Init(DISPDB4_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DISPEN_Pin;
    LL_GPIO_Init(DISPEN_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DISPDB7_Pin;
    LL_GPIO_Init(DISPDB7_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DISPDB5_Pin;
    LL_GPIO_Init(DISPDB5_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DISPDB6_Pin;
    LL_GPIO_Init(DISPDB6_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DISPRW_Pin;
    LL_GPIO_Init(DISPRW_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DISPRS_Pin;
    LL_GPIO_Init(DISPRS_GPIO_Port, &GPIO_InitStruct);
}


/* ============================================================
   ERROR HANDLER
   ============================================================ */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* Implementazione opzionale */
}
#endif

