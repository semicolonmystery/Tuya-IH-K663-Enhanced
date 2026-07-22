/********************************************************************************
 * battery.c — internal-VBAT battery measurement + coin-cell curve (F7).
 * See battery.h. Uses the SDK ADC in VBAT mode (romasku's proven pattern for
 * this chip); drv_get_adc_data() returns millivolts.
 ********************************************************************************/
#include "tl_common.h"
#include "app_config.h"
#include "battery.h"

u8 g_batteryVoltage;      /* 0x0020, 100 mV units */
u8 g_batteryPercentage;   /* 0x0021, 0.5 % units  */

/* CR2032 discharge curve: flat middle, steep ends. mV -> percent, descending. */
static const struct { u16 mv; u8 pct; } s_curve[] = {
    { 3000, 100 }, { 2900, 95 }, { 2800, 90 }, { 2750, 80 },
    { 2700, 70 },  { 2650, 60 }, { 2600, 50 }, { 2550, 40 },
    { 2500, 30 },  { 2450, 20 }, { 2400, 10 }, { 2300, 0 },
};
#define CURVE_N   (sizeof(s_curve) / sizeof(s_curve[0]))

static u8 mv_to_pct(u16 mv)
{
    if (mv >= BATTERY_V_100) return 100;
    if (mv <= BATTERY_V_0)   return 0;
    for (u32 i = 0; i < CURVE_N - 1; i++) {
        u16 hi = s_curve[i].mv, lo = s_curve[i + 1].mv;
        if (mv <= hi && mv >= lo) {
            /* linear interpolation between the two bracketing points */
            u16 span = hi - lo;
            u8  phi = s_curve[i].pct, plo = s_curve[i + 1].pct;
            return (u8)(plo + (u32)(mv - lo) * (phi - plo) / (span ? span : 1));
        }
    }
    return 0;
}

static u16 read_mv(void)
{
#if BATTERY_USE_EXTERNAL_ADC
    drv_adc_mode_pin_set(DRV_ADC_BASE_MODE, (GPIO_PinTypeDef)BATTERY_ADC_PIN);
#else
    drv_adc_mode_pin_set(DRV_ADC_VBAT_MODE, (GPIO_PinTypeDef)BATTERY_ADC_PIN);
#endif
    drv_adc_enable(TRUE);
    sleep_us(100);
    u16 mv = drv_get_adc_data();
    drv_adc_enable(FALSE);
    return mv;
}

void battery_init(void)
{
    drv_adc_init();
    battery_update();
}

void battery_update(void)
{
    u16 mv = read_mv();
    u8  pct = mv_to_pct(mv);
    g_batteryVoltage    = (u8)(mv / 100);
    g_batteryPercentage = (u8)(pct * 2);
    printf("batt=%d pct=%d\n", mv, pct);
}
