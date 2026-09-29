/*
 * CH32V003A4M6 dual-input RGB indicator controller
 *
 * Community-PIO-CH32V / WCH NoneOS SDK build.
 *
 * Wiring (SOP-16):
 *   PC1 / pin 1  -> LAN status input from Q1 (active high at MCU)
 *   PC2 / pin 2  -> HDD status input from Q1 (active high at MCU)
 *   PC3 / pin 3  -> Setup button to GND (internal pull-up)
 *   PC4 / pin 4  -> SK6812 LED 1 DIN (RGB1)
 *   PC6 / pin 5  -> SK6812 LED 2 DIN (RGB2)
 *   PD1 / pin 7  -> SWIO programming/debug (leave dedicated)
 *
 *   LED 1 and LED 2 are intentionally independent.
 *   Each LED DOUT is unused.
 *
 * Normal:
 *   LAN input  (PC1) -> logical LAN indicator.
 *   HDD input  (PC2) -> logical HDD indicator.
 *   Default mapping: RGB1/PC4 = LAN, RGB2/PC6 = HDD.
 *   Reverse setting swaps the two physical indicator positions.
 *   Double press toggles normal activity indication on/off.
 *   Activity indication always starts enabled after power-up.
 *
 * Defaults:
 *   LAN = Amber, HDD = Red, brightness = 100%, reverse = off.
 *
 * Setup:
 *   Hold button 3 s -> both LEDs blink red twice, then edit LAN color.
 *   Single press    -> cycle/toggle the current setting.
 *   Double press    -> LAN color -> HDD color -> brightness -> reverse -> LAN.
 *   Brightness      -> 10 levels: 10% through 100% in 10% steps.
 *   Reverse preview -> LAN position is green, HDD position is red.
 *   Hold button 3 s -> save all settings, blink both LEDs green twice,
 *                      then resume normal operation.
 *
 * Factory reset:
 *   Hold button 10 s -> restore defaults and save them to flash.
 *   Successful reset flashes both LEDs Green -> Orange -> Red.
 */

#include "debug.h"
#include <stdint.h>

/* ------------------------------------------------------------------------- */
/* GPIO                                                                      */
/* ------------------------------------------------------------------------- */

#define INPUT1_PIN      GPIO_Pin_1   /* PC1 */
#define INPUT2_PIN      GPIO_Pin_2   /* PC2 */
#define BUTTON_PIN      GPIO_Pin_3   /* PC3 */
#define LED1_PIN        GPIO_Pin_4   /* PC4 / RGB1 */
#define LED2_PIN        GPIO_Pin_6   /* PC6 / RGB2 */
#define LED_PIN_MASK    (LED1_PIN | LED2_PIN)

#define LOOP_MS             5U
#define DEBOUNCE_MS        20U
#define LONG_PRESS_MS    3000U
#define RESET_HOLD_MS    10000U
#define DOUBLE_PRESS_MS   350U

#define DEBOUNCE_TICKS      (DEBOUNCE_MS / LOOP_MS)
#define LONG_PRESS_TICKS    (LONG_PRESS_MS / LOOP_MS)
#define RESET_HOLD_TICKS    (RESET_HOLD_MS / LOOP_MS)
#define DOUBLE_PRESS_TICKS  (DOUBLE_PRESS_MS / LOOP_MS)

#define COLOR_COUNT        16U
#define BRIGHTNESS_LEVELS  10U

/* ------------------------------------------------------------------------- */
/* Colors                                                                    */
/* ------------------------------------------------------------------------- */

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

static const rgb_t kColors[COLOR_COUNT] =
{
    {255,   0,   0}, /*  0 Red        */
    {255,  64,   0}, /*  1 Orange     */
    {255, 128,   0}, /*  2 Amber      */
    {255, 255,   0}, /*  3 Yellow     */
    {128, 255,   0}, /*  4 Lime       */
    {  0, 255,   0}, /*  5 Green      */
    {  0, 255, 128}, /*  6 Mint       */
    {  0, 255, 255}, /*  7 Cyan       */
    {  0, 128, 255}, /*  8 Sky Blue   */
    {  0,   0, 255}, /*  9 Blue       */
    {128,   0, 255}, /* 10 Violet     */
    {192,   0, 255}, /* 11 Purple     */
    {255,   0, 255}, /* 12 Magenta    */
    {255,   0, 128}, /* 13 Pink       */
    {255, 255, 255}, /* 14 White      */
    {255, 128,  64}, /* 15 Warm White */
};

static const rgb_t kOff          = {0, 0, 0};
static const rgb_t kStatusRed    = {255, 0, 0};
static const rgb_t kStatusGreen  = {0, 255, 0};
static const rgb_t kStatusOrange = {255, 64, 0};

/*
 * Color indexes are logical functions, not physical LED positions:
 *   [0] = LAN
 *   [1] = HDD
 */
#define SETTING_LAN  0U
#define SETTING_HDD  1U

#define DEFAULT_LAN_COLOR      2U   /* Amber */
#define DEFAULT_HDD_COLOR      0U   /* Red */
#define DEFAULT_BRIGHTNESS    10U   /* 10 = 100% */
#define DEFAULT_REVERSE        0U   /* RGB1=LAN, RGB2=HDD */

static uint8_t g_saved_color[2] =
{
    DEFAULT_LAN_COLOR,
    DEFAULT_HDD_COLOR
};
static uint8_t g_edit_color[2];
static uint8_t g_saved_brightness = DEFAULT_BRIGHTNESS;
static uint8_t g_edit_brightness  = DEFAULT_BRIGHTNESS;
static uint8_t g_saved_reverse    = DEFAULT_REVERSE;
static uint8_t g_edit_reverse     = DEFAULT_REVERSE;

/* ------------------------------------------------------------------------- */
/* Flash config                                                              */
/* ------------------------------------------------------------------------- */

/*
 * CH32V003 has 16 KB flash from 0x08000000 through 0x08003FFF.
 * Fast erase/program page size is 64 bytes.
 * Reserve the final 64-byte page for:
 *
 *   LAN color index  : 0..15
 *   HDD color index  : 0..15
 *   Brightness       : 1..10  (10%..100%)
 *   Reverse mapping  : 0/1
 *
 * Keep the linked firmware below 0x08003FC0.
 */
#define CONFIG_FLASH_ADDR  ((uint32_t)0x08003FC0)
#define CONFIG_MAGIC       ((uint32_t)0x52474233) /* "RGB3" */
#define CONFIG_MARKER      ((uint8_t)0xA5)

static uint8_t config_checksum(uint8_t lan_color,
                               uint8_t hdd_color,
                               uint8_t brightness,
                               uint8_t reverse)
{
    return (uint8_t)(lan_color ^ hdd_color ^ brightness ^ reverse ^ 0x5AU);
}

static uint8_t config_load(void)
{
    volatile const uint32_t *flash =
        (volatile const uint32_t *)CONFIG_FLASH_ADDR;

    uint32_t packed;
    uint32_t meta;
    uint8_t lan_color;
    uint8_t hdd_color;
    uint8_t brightness;
    uint8_t reverse;
    uint8_t marker;
    uint8_t check;

    if (flash[0] != CONFIG_MAGIC)
        return 0U;

    packed = flash[1];
    meta   = flash[2];

    lan_color  = (uint8_t)( packed        & 0xFFU);
    hdd_color  = (uint8_t)((packed >>  8) & 0xFFU);
    brightness = (uint8_t)((packed >> 16) & 0xFFU);
    reverse    = (uint8_t)((packed >> 24) & 0xFFU);

    check  = (uint8_t)( meta       & 0xFFU);
    marker = (uint8_t)((meta >> 8) & 0xFFU);

    if (marker != CONFIG_MARKER)
        return 0U;

    if ((lan_color >= COLOR_COUNT) || (hdd_color >= COLOR_COUNT))
        return 0U;

    if ((brightness < 1U) || (brightness > BRIGHTNESS_LEVELS))
        return 0U;

    if (reverse > 1U)
        return 0U;

    if (check != config_checksum(lan_color,
                                 hdd_color,
                                 brightness,
                                 reverse))
        return 0U;

    g_saved_color[SETTING_LAN] = lan_color;
    g_saved_color[SETTING_HDD] = hdd_color;
    g_saved_brightness = brightness;
    g_saved_reverse = reverse;
    return 1U;
}

static uint8_t config_save(uint8_t lan_color,
                           uint8_t hdd_color,
                           uint8_t brightness,
                           uint8_t reverse)
{
    uint32_t page[16];
    volatile const uint32_t *verify =
        (volatile const uint32_t *)CONFIG_FLASH_ADDR;
    uint8_t i;

    for (i = 0U; i < 16U; ++i)
        page[i] = 0xFFFFFFFFU;

    page[0] = CONFIG_MAGIC;
    page[1] =
        ((uint32_t)reverse << 24) |
        ((uint32_t)brightness << 16) |
        ((uint32_t)hdd_color << 8) |
        (uint32_t)lan_color;
    page[2] =
        ((uint32_t)CONFIG_MARKER << 8) |
        (uint32_t)config_checksum(lan_color,
                                  hdd_color,
                                  brightness,
                                  reverse);

    __disable_irq();

    FLASH_Unlock_Fast();
    FLASH_ErasePage_Fast(CONFIG_FLASH_ADDR);

    FLASH_BufReset();
    for (i = 0U; i < 16U; ++i)
        FLASH_BufLoad(CONFIG_FLASH_ADDR + ((uint32_t)i * 4U), page[i]);

    FLASH_ProgramPage_Fast(CONFIG_FLASH_ADDR);
    FLASH_Lock_Fast();

    __enable_irq();

    if ((verify[0] != page[0]) ||
        (verify[1] != page[1]) ||
        (verify[2] != page[2]))
        return 0U;

    return 1U;
}

/* ------------------------------------------------------------------------- */
/* SK6812 independent dual-output driver                                     */
/* ------------------------------------------------------------------------- */

/*
 * SK6812SIDE-A-RVS data is 24-bit GRB, MSB first.
 *
 * PC4 drives LED1 DIN and PC6 drives LED2 DIN. The two LEDs are completely
 * independent; each DOUT is unused.
 *
 * Both data pins are on GPIOC, so each bit can start at the same instant.
 * A mask drops only outputs carrying a zero at the short-high point; both
 * outputs are dropped later at the long-high point.
 *
 * NOP counts target the CH32V003 running at 48 MHz. Production hardware
 * validation remains the final timing check.
 */

static void sk6812_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    gpio.GPIO_Pin = LED_PIN_MASK;
    gpio.GPIO_Speed = GPIO_Speed_30MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &gpio);

    GPIOC->BCR = LED_PIN_MASK;
    Delay_Us(250);
}

static inline __attribute__((always_inline))
void sk6812_send_dual_bit(uint32_t early_clear_mask)
{
    GPIOC->BSHR = LED_PIN_MASK;

    __asm__ volatile(
        ".rept 14\n\t"
        "nop\n\t"
        ".endr\n\t"
        ::: "memory"
    );

    GPIOC->BCR = early_clear_mask;

    __asm__ volatile(
        ".rept 26\n\t"
        "nop\n\t"
        ".endr\n\t"
        ::: "memory"
    );

    GPIOC->BCR = LED_PIN_MASK;

    __asm__ volatile(
        ".rept 12\n\t"
        "nop\n\t"
        ".endr\n\t"
        ::: "memory"
    );
}

static uint32_t sk6812_pack_grb(rgb_t c)
{
    return ((uint32_t)c.g << 16) |
           ((uint32_t)c.r << 8)  |
           ((uint32_t)c.b);
}

static void leds_write(rgb_t led1, rgb_t led2)
{
    uint32_t data1 = sk6812_pack_grb(led1);
    uint32_t data2 = sk6812_pack_grb(led2);
    uint32_t bit_mask = (1UL << 23);

    __disable_irq();

    while (bit_mask != 0U)
    {
        uint32_t early_clear = 0U;

        if ((data1 & bit_mask) == 0U)
            early_clear |= LED1_PIN;

        if ((data2 & bit_mask) == 0U)
            early_clear |= LED2_PIN;

        sk6812_send_dual_bit(early_clear);
        bit_mask >>= 1;
    }

    GPIOC->BCR = LED_PIN_MASK;
    __enable_irq();

    Delay_Us(250);
}

static rgb_t scale_color(rgb_t color, uint8_t brightness)
{
    rgb_t out;

    out.r = (uint8_t)(((uint16_t)color.r * brightness) / BRIGHTNESS_LEVELS);
    out.g = (uint8_t)(((uint16_t)color.g * brightness) / BRIGHTNESS_LEVELS);
    out.b = (uint8_t)(((uint16_t)color.b * brightness) / BRIGHTNESS_LEVELS);

    return out;
}

static void leds_write_logical(rgb_t lan, rgb_t hdd, uint8_t reverse)
{
    if (reverse)
        leds_write(hdd, lan);
    else
        leds_write(lan, hdd);
}

static void leds_off(void)
{
    leds_write(kOff, kOff);
}

static void blink_both(rgb_t color)
{
    uint8_t i;

    for (i = 0U; i < 2U; ++i)
    {
        leds_write(color, color);
        Delay_Ms(140);
        leds_off();
        Delay_Ms(100);
    }
}

/* ------------------------------------------------------------------------- */
/* GPIO / button                                                             */
/* ------------------------------------------------------------------------- */

typedef enum
{
    BUTTON_NONE = 0,
    BUTTON_SINGLE,
    BUTTON_DOUBLE,
    BUTTON_LONG,
    BUTTON_RESET
} button_event_t;

typedef struct
{
    uint8_t raw_pressed;
    uint8_t stable_pressed;
    uint8_t debounce_ticks;
    uint16_t hold_ticks;
    uint16_t click_wait_ticks;
    uint8_t click_count;
    uint8_t long_sent;
    uint8_t reset_sent;
} button_state_t;

static button_state_t g_button;

static void gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    /*
     * Q1 drives these nodes high when active. When Q1 is off its drain is
     * otherwise floating, so use the MCU internal pull-downs as the inactive
     * state without adding external resistors.
     */
    gpio.GPIO_Pin = INPUT1_PIN | INPUT2_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOC, &gpio);

    /* Setup button: active low using internal pull-up. */
    gpio.GPIO_Pin = BUTTON_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &gpio);
}

static uint8_t input1_active(void)
{
    return (GPIO_ReadInputDataBit(GPIOC, INPUT1_PIN) == Bit_SET) ? 1U : 0U;
}

static uint8_t input2_active(void)
{
    return (GPIO_ReadInputDataBit(GPIOC, INPUT2_PIN) == Bit_SET) ? 1U : 0U;
}

static uint8_t button_is_pressed(void)
{
    return (GPIO_ReadInputDataBit(GPIOC, BUTTON_PIN) == Bit_RESET) ? 1U : 0U;
}

static void button_init(void)
{
    uint8_t pressed = button_is_pressed();

    g_button.raw_pressed = pressed;
    g_button.stable_pressed = pressed;
    g_button.debounce_ticks = 0U;
    g_button.hold_ticks = 0U;
    g_button.click_wait_ticks = 0U;
    g_button.click_count = 0U;
    g_button.long_sent = 0U;
    g_button.reset_sent = 0U;
}

static button_event_t button_poll(void)
{
    uint8_t raw = button_is_pressed();

    if (raw != g_button.raw_pressed)
    {
        g_button.raw_pressed = raw;
        g_button.debounce_ticks = 0U;
    }
    else if (g_button.debounce_ticks < DEBOUNCE_TICKS)
    {
        ++g_button.debounce_ticks;
    }

    if ((g_button.debounce_ticks >= DEBOUNCE_TICKS) &&
        (raw != g_button.stable_pressed))
    {
        g_button.stable_pressed = raw;

        if (raw)
        {
            g_button.hold_ticks = 0U;
            g_button.long_sent = 0U;
            g_button.reset_sent = 0U;
        }
        else
        {
            g_button.hold_ticks = 0U;

            if (!g_button.long_sent && !g_button.reset_sent)
            {
                ++g_button.click_count;
                g_button.click_wait_ticks = 0U;

                if (g_button.click_count >= 2U)
                {
                    g_button.click_count = 0U;
                    return BUTTON_DOUBLE;
                }
            }
            else
            {
                /*
                 * A long/reset hold must never become a click when released.
                 */
                g_button.long_sent = 0U;
                g_button.reset_sent = 0U;
                g_button.click_count = 0U;
                g_button.click_wait_ticks = 0U;
            }
        }
    }

    if (g_button.stable_pressed)
    {
        if (g_button.hold_ticks < RESET_HOLD_TICKS)
            ++g_button.hold_ticks;

        /*
         * Long press still fires at 3 seconds so setup behavior remains
         * immediate. If the same press continues to 10 seconds, a factory
         * reset event is generated as a second, higher-priority event.
         */
        if (!g_button.long_sent &&
            (g_button.hold_ticks >= LONG_PRESS_TICKS))
        {
            g_button.long_sent = 1U;
            g_button.click_count = 0U;
            g_button.click_wait_ticks = 0U;
            return BUTTON_LONG;
        }

        if (!g_button.reset_sent &&
            (g_button.hold_ticks >= RESET_HOLD_TICKS))
        {
            g_button.reset_sent = 1U;
            g_button.click_count = 0U;
            g_button.click_wait_ticks = 0U;
            return BUTTON_RESET;
        }
    }
    else if (g_button.click_count == 1U)
    {
        if (g_button.click_wait_ticks < DOUBLE_PRESS_TICKS)
            ++g_button.click_wait_ticks;

        if (g_button.click_wait_ticks >= DOUBLE_PRESS_TICKS)
        {
            g_button.click_count = 0U;
            g_button.click_wait_ticks = 0U;
            return BUTTON_SINGLE;
        }
    }

    return BUTTON_NONE;
}

/* ------------------------------------------------------------------------- */
/* Application                                                               */
/* ------------------------------------------------------------------------- */

typedef enum
{
    MODE_NORMAL = 0,
    MODE_SETUP
} app_mode_t;

typedef enum
{
    SETUP_LAN_COLOR = 0,
    SETUP_HDD_COLOR,
    SETUP_BRIGHTNESS,
    SETUP_REVERSE
} setup_stage_t;

static app_mode_t g_mode = MODE_NORMAL;
static setup_stage_t g_setup_stage = SETUP_LAN_COLOR;

/*
 * Runtime-only activity display state.
 * This is intentionally not stored in flash: every power-up starts enabled.
 * Setup/save/reset feedback is always shown regardless of this flag.
 */
static uint8_t g_activity_enabled = 1U;

static void show_setup_preview(void)
{
    rgb_t lan = kOff;
    rgb_t hdd = kOff;

    if (g_setup_stage == SETUP_LAN_COLOR)
    {
        lan = scale_color(kColors[g_edit_color[SETTING_LAN]],
                          g_edit_brightness);
    }
    else if (g_setup_stage == SETUP_HDD_COLOR)
    {
        hdd = scale_color(kColors[g_edit_color[SETTING_HDD]],
                          g_edit_brightness);
    }
    else if (g_setup_stage == SETUP_BRIGHTNESS)
    {
        lan = scale_color(kColors[g_edit_color[SETTING_LAN]],
                          g_edit_brightness);
        hdd = scale_color(kColors[g_edit_color[SETTING_HDD]],
                          g_edit_brightness);
    }
    else
    {
        /*
         * Reverse-position preview:
         *   Green = LAN position
         *   Red   = HDD position
         *
         * Toggling reverse visibly swaps green/red between RGB1 and RGB2.
         */
        lan = kStatusGreen;
        hdd = kStatusRed;
    }

    leds_write_logical(lan, hdd, g_edit_reverse);
}

static void enter_setup(void)
{
    g_edit_color[SETTING_LAN] = g_saved_color[SETTING_LAN];
    g_edit_color[SETTING_HDD] = g_saved_color[SETTING_HDD];
    g_edit_brightness = g_saved_brightness;
    g_edit_reverse = g_saved_reverse;
    g_setup_stage = SETUP_LAN_COLOR;

    blink_both(kStatusRed);

    g_mode = MODE_SETUP;
    show_setup_preview();
}

static void show_save_error(void)
{
    uint8_t i;

    for (i = 0U; i < 3U; ++i)
    {
        leds_write(kStatusRed, kStatusRed);
        Delay_Ms(80);
        leds_off();
        Delay_Ms(80);
    }
}

static void save_and_exit_setup(void)
{
    if (!config_save(g_edit_color[SETTING_LAN],
                     g_edit_color[SETTING_HDD],
                     g_edit_brightness,
                     g_edit_reverse))
    {
        show_save_error();
        show_setup_preview();
        return;
    }

    g_saved_color[SETTING_LAN] = g_edit_color[SETTING_LAN];
    g_saved_color[SETTING_HDD] = g_edit_color[SETTING_HDD];
    g_saved_brightness = g_edit_brightness;
    g_saved_reverse = g_edit_reverse;

    blink_both(kStatusGreen);
    g_mode = MODE_NORMAL;
}

static void show_reset_success(void)
{
    leds_write(kStatusGreen, kStatusGreen);
    Delay_Ms(180);
    leds_off();
    Delay_Ms(90);

    leds_write(kStatusOrange, kStatusOrange);
    Delay_Ms(180);
    leds_off();
    Delay_Ms(90);

    leds_write(kStatusRed, kStatusRed);
    Delay_Ms(180);
    leds_off();
    Delay_Ms(90);
}

static uint8_t factory_reset(void)
{
    if (!config_save(DEFAULT_LAN_COLOR,
                     DEFAULT_HDD_COLOR,
                     DEFAULT_BRIGHTNESS,
                     DEFAULT_REVERSE))
    {
        show_save_error();

        if (g_mode == MODE_SETUP)
            show_setup_preview();

        return 0U;
    }

    g_saved_color[SETTING_LAN] = DEFAULT_LAN_COLOR;
    g_saved_color[SETTING_HDD] = DEFAULT_HDD_COLOR;
    g_saved_brightness = DEFAULT_BRIGHTNESS;
    g_saved_reverse = DEFAULT_REVERSE;

    g_edit_color[SETTING_LAN] = DEFAULT_LAN_COLOR;
    g_edit_color[SETTING_HDD] = DEFAULT_HDD_COLOR;
    g_edit_brightness = DEFAULT_BRIGHTNESS;
    g_edit_reverse = DEFAULT_REVERSE;

    g_mode = MODE_NORMAL;
    g_setup_stage = SETUP_LAN_COLOR;
    g_activity_enabled = 1U;

    show_reset_success();
    return 1U;
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void)
{
    uint8_t last_input_mask = 0xFFU;

    SystemCoreClockUpdate();
    Delay_Init();

    gpio_init();
    sk6812_gpio_init();

    (void)config_load();
    button_init();

    leds_off();

    while (1)
    {
        button_event_t event = button_poll();

        /*
         * Factory reset is valid from either normal operation or setup and
         * takes priority over the current mode.
         */
        if (event == BUTTON_RESET)
        {
            (void)factory_reset();
            last_input_mask = 0xFFU;
            Delay_Ms(LOOP_MS);
            continue;
        }

        if (g_mode == MODE_NORMAL)
        {
            if (event == BUTTON_LONG)
            {
                enter_setup();
                last_input_mask = 0xFFU;
            }
            else if (event == BUTTON_DOUBLE)
            {
                /*
                 * Runtime activity display toggle.
                 * This state is not written to flash.
                 */
                g_activity_enabled ^= 1U;

                if (!g_activity_enabled)
                {
                    leds_off();
                }

                /*
                 * Force an immediate refresh when re-enabled so the LEDs
                 * reflect the current LAN/HDD state without waiting for the
                 * next input transition.
                 */
                last_input_mask = 0xFFU;
            }
            else if (!g_activity_enabled)
            {
                /*
                 * Inputs continue to be sampled by hardware, but normal
                 * activity indication stays dark until double-clicked on.
                 */
                if (last_input_mask != 0U)
                {
                    leds_off();
                    last_input_mask = 0U;
                }
            }
            else
            {
                uint8_t input_mask = 0U;

                if (input1_active())
                    input_mask |= 0x01U; /* LAN */

                if (input2_active())
                    input_mask |= 0x02U; /* HDD */

                if (input_mask != last_input_mask)
                {
                    rgb_t lan =
                        (input_mask & 0x01U) ?
                        scale_color(kColors[g_saved_color[SETTING_LAN]],
                                    g_saved_brightness) : kOff;

                    rgb_t hdd =
                        (input_mask & 0x02U) ?
                        scale_color(kColors[g_saved_color[SETTING_HDD]],
                                    g_saved_brightness) : kOff;

                    leds_write_logical(lan, hdd, g_saved_reverse);
                    last_input_mask = input_mask;
                }
            }
        }
        else
        {
            if (event == BUTTON_SINGLE)
            {
                if (g_setup_stage == SETUP_LAN_COLOR)
                {
                    g_edit_color[SETTING_LAN] =
                        (uint8_t)((g_edit_color[SETTING_LAN] + 1U) %
                                  COLOR_COUNT);
                }
                else if (g_setup_stage == SETUP_HDD_COLOR)
                {
                    g_edit_color[SETTING_HDD] =
                        (uint8_t)((g_edit_color[SETTING_HDD] + 1U) %
                                  COLOR_COUNT);
                }
                else if (g_setup_stage == SETUP_BRIGHTNESS)
                {
                    ++g_edit_brightness;

                    if (g_edit_brightness > BRIGHTNESS_LEVELS)
                        g_edit_brightness = 1U;
                }
                else
                {
                    g_edit_reverse ^= 1U;
                }

                show_setup_preview();
            }
            else if (event == BUTTON_DOUBLE)
            {
                if (g_setup_stage == SETUP_LAN_COLOR)
                    g_setup_stage = SETUP_HDD_COLOR;
                else if (g_setup_stage == SETUP_HDD_COLOR)
                    g_setup_stage = SETUP_BRIGHTNESS;
                else if (g_setup_stage == SETUP_BRIGHTNESS)
                    g_setup_stage = SETUP_REVERSE;
                else
                    g_setup_stage = SETUP_LAN_COLOR;

                show_setup_preview();
            }
            else if (event == BUTTON_LONG)
            {
                save_and_exit_setup();
                last_input_mask = 0xFFU;
            }
        }

        Delay_Ms(LOOP_MS);
    }
}

/* Startup vector expects these handlers. */
void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while (1)
    {
    }
}
