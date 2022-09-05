#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "ssd1306.h"
#include "stdio.h"

#include "math.h"

// Reverses a string 'str' of length 'len'
void reverse(char* str, int len)
{
    int i = 0, j = len - 1, temp;
    while (i < j) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }
}

// Converts a given integer x to string str[].
// d is the number of digits required in the output.
// If d is more than the number of digits in x,
// then 0s are added at the beginning.
int intToStr(int x, char str[], int d)
{
    int i = 0;
    while (x) {
        str[i++] = (x % 10) + '0';
        x = x / 10;
    }

    // If number of digits required is more, then
    // add 0s at the beginning
    while (i < d)
        str[i++] = '0';

    reverse(str, i);
    str[i] = '\0';
    return i;
}

// Converts a floating-point/double number to a string.
void ftoa(float n, char* res, int afterpoint)
{
    // Extract integer part
    int ipart = (int)n;

    // Extract floating part
    float fpart = n - (float)ipart;

    // convert integer part to string
    int i = intToStr(ipart, res, 0);

    // check for display option after point
    if (afterpoint != 0) {
        res[i] = '.'; // add dot

        // Get the value of fraction part upto given no.
        // of points after dot. The third parameter
        // is needed to handle cases like 233.007
        fpart = fpart * pow(10, afterpoint);

        intToStr((int)fpart, res + i + 1, afterpoint);
    }
}

// Driver program to test above function

#define BUFF_SIZE   20
char buff[BUFF_SIZE];

#define LINE_TRIGGER                PAL_LINE(GPIOB, 10U)
#define LINE_ECHO                   PAL_LINE(GPIOA, 8U)
/* Enable if your terminal supports ANSI ESCAPE CODE */
#define ANSI_ESCAPE_CODE_ALLOWED    TRUE

static BaseSequentialStream * chp = (BaseSequentialStream*) &SD2;

static const I2CConfig i2ccfg = {
  OPMODE_I2C,
  400000,
  FAST_DUTY_CYCLE_2,
};

static const SSD1306Config ssd1306cfg = {
  &I2CD1,
  &i2ccfg,
  SSD1306_SAD_0X78,
};

static SSD1306Driver SSD1306D1;


/*===========================================================================*/
/* ICU related code                                                          */
/*===========================================================================*/

#define ICU_TIM_FREQ                1000000
#define M_TO_CM                     100.0f
#define SPEED_OF_SOUND              343.2f

static float lastdistance = 0.0;

static void icuwidthcb(ICUDriver *icup) {

  icucnt_t width = icuGetWidthX(icup);
  lastdistance = (SPEED_OF_SOUND * width * M_TO_CM) / (ICU_TIM_FREQ * 2);

}

static ICUConfig icucfg = {
  ICU_INPUT_ACTIVE_HIGH,
  ICU_TIM_FREQ,                                /* 1MHz ICU clock frequency.   */
  icuwidthcb,
  NULL,
  NULL,
  ICU_CHANNEL_1,
  0,
  0xFFFFFFFFU
};

/*===========================================================================*/
/* Generic code                                                              */
/*===========================================================================*/

/*
 * Red LED blinker thread, times are in milliseconds.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg) {

  (void)arg;
  chRegSetThreadName("blinker");
  while (true) {
    palClearPad(GPIOA, GPIOA_LED_GREEN);
    chThdSleepMilliseconds(500);
    palSetPad(GPIOA, GPIOA_LED_GREEN);
    chThdSleepMilliseconds(500);
  }
}

static THD_WORKING_AREA(waOledDisplay, 512);
static THD_FUNCTION(OledDisplay, arg) {
  (void)arg;

  chRegSetThreadName("OledDisplay");

  /*
   * Initialize SSD1306 Display Driver Object.
   */
  ssd1306ObjectInit(&SSD1306D1);

  /*
   * Start the SSD1306 Display Driver Object with
   * configuration.
   */
  ssd1306Start(&SSD1306D1, &ssd1306cfg);

  ssd1306FillScreen(&SSD1306D1, 0x00);

  //char string[10];

  while (true) {

    ssd1306GotoXy(&SSD1306D1, 0, 1);
    chsnprintf(buff, BUFF_SIZE, "DISTANZA:\n\r");
    ssd1306Puts(&SSD1306D1, buff, &ssd1306_font_11x18, SSD1306_COLOR_WHITE);

    ssd1306GotoXy(&SSD1306D1, 0, 36);
    ftoa(lastdistance, buff, 2);
          ssd1306Puts(&SSD1306D1, buff, &ssd1306_font_7x10, SSD1306_COLOR_BLACK);
          ssd1306UpdateScreen(&SSD1306D1);

    /*ssd1306GotoXy(&SSD1306D1, 0, 20);
    chsnprintf(buff, BUFF_SIZE, string);
    ssd1306Puts(&SSD1306D1, buff, &ssd1306_font_7x10, SSD1306_COLOR_BLACK);*/

    ssd1306UpdateScreen(&SSD1306D1);
    chThdSleepMilliseconds(500);
  }

}


/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Activates the serial driver 2 using the driver default configuration.
   */
  sdStart(&SD2, NULL);

  /* Configuring I2C related PINs */
     palSetLineMode(LINE_ARD_D15, PAL_MODE_ALTERNATE(4) |
                    PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST |
                    PAL_STM32_PUPDR_PULLUP);
     palSetLineMode(LINE_ARD_D14, PAL_MODE_ALTERNATE(4) |
                    PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST |
                    PAL_STM32_PUPDR_PULLUP);

     /*
        * Creates the OLED thread.
        */
     chThdCreateStatic(waOledDisplay, sizeof(waOledDisplay), NORMALPRIO, OledDisplay, NULL);

  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /* Initializes the ICU driver 1. The ICU input i PA8. */
  palSetLineMode(LINE_ECHO, PAL_MODE_ALTERNATE(1));
  icuStart(&ICUD1, &icucfg);
  icuStartCapture(&ICUD1);
  icuEnableNotifications(&ICUD1);

  palSetLineMode(LINE_TRIGGER, PAL_MODE_OUTPUT_PUSHPULL);

  while (true) {
    /* Triggering */
    palWriteLine(LINE_TRIGGER, PAL_HIGH);
    chThdSleepMicroseconds(10);
    palWriteLine(LINE_TRIGGER, PAL_LOW);
#if ANSI_ESCAPE_CODE_ALLOWED
    chprintf(chp, "\033[2J\033[1;1H");
#endif
    chprintf(chp, "Distance: %.2f cm\n\r", lastdistance);

    chThdSleepMilliseconds(1000);
  }
  icuStopCapture(&ICUD1);
  icuStop(&ICUD1);
}
