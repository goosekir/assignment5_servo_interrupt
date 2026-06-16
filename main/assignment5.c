#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"

#define SERVO_GPIO GPIO_NUM_18
#define BOOT_GPIO GPIO_NUM_0          // BOOT / IO0 button
#define POT_CHANNEL ADC_CHANNEL_6     // GPIO34 = ADC1 Channel 6

#define SERVO_MIN_US 500
#define SERVO_MAX_US 2500
#define SERVO_PERIOD_US 20000         // 50Hz servo period = 20ms

static SemaphoreHandle_t boot_semaphore = NULL;
static SemaphoreHandle_t servo_mutex = NULL;

static int map_value(int x, int in_min, int in_max, int out_min, int out_max)
{
    if (x < in_min) x = in_min;
    if (x > in_max) x = in_max;

    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void set_servo_pulse_us(int pulse_width_us)
{
    uint32_t max_duty = (1UL << LEDC_TIMER_16_BIT) - 1;
    uint32_t duty = (pulse_width_us * max_duty) / SERVO_PERIOD_US;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// BOOT button interrupt handler
static void IRAM_ATTR boot_button_isr_handler(void *arg)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    xSemaphoreGiveFromISR(boot_semaphore, &higher_priority_task_woken);

    if (higher_priority_task_woken)
    {
        portYIELD_FROM_ISR();
    }
}

void app_main(void)
{
    // ---------- Mutex / semaphore setup ----------
    boot_semaphore = xSemaphoreCreateBinary();
    servo_mutex = xSemaphoreCreateMutex();

    // ---------- BOOT button interrupt setup ----------
    gpio_reset_pin(BOOT_GPIO);
    gpio_set_direction(BOOT_GPIO, GPIO_MODE_INPUT);
    gpio_pullup_en(BOOT_GPIO);

    // BOOT button is active LOW, so falling edge = pressed
    gpio_set_intr_type(BOOT_GPIO, GPIO_INTR_NEGEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOOT_GPIO, boot_button_isr_handler, NULL);

    // ---------- ADC setup for potentiometer ----------
    adc_oneshot_unit_handle_t adc_handle;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };

    adc_oneshot_new_unit(&init_config, &adc_handle);

    adc_oneshot_chan_cfg_t adc_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    adc_oneshot_config_channel(adc_handle, POT_CHANNEL, &adc_config);

    // ---------- PWM setup for servo ----------
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_16_BIT,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config(&ledc_channel);

    while (1)
    {
        int pot_value = 0;
        adc_oneshot_read(adc_handle, POT_CHANNEL, &pot_value);

        int pot_pulse = map_value(pot_value, 0, 4095, SERVO_MIN_US, SERVO_MAX_US);
        int pot_angle = map_value(pot_value, 0, 4095, 0, 180);

        // If BOOT interrupt happened, swing servo to limit for 1 second
        if (xSemaphoreTake(boot_semaphore, 0) == pdTRUE)
        {
            printf("BOOT interrupt detected. Swinging servo to limit for 1 second.\n");

            if (xSemaphoreTake(servo_mutex, portMAX_DELAY) == pdTRUE)
            {
                set_servo_pulse_us(SERVO_MAX_US);
                xSemaphoreGive(servo_mutex);
            }

            vTaskDelay(pdMS_TO_TICKS(1000));

            // After 1 second, return to current potentiometer position
            adc_oneshot_read(adc_handle, POT_CHANNEL, &pot_value);
            pot_pulse = map_value(pot_value, 0, 4095, SERVO_MIN_US, SERVO_MAX_US);
            pot_angle = map_value(pot_value, 0, 4095, 0, 180);

            if (xSemaphoreTake(servo_mutex, portMAX_DELAY) == pdTRUE)
            {
                set_servo_pulse_us(pot_pulse);
                xSemaphoreGive(servo_mutex);
            }

            printf("Returned to potentiometer position | Pot: %d | Angle: %d | Pulse: %d us\n",
                   pot_value,
                   pot_angle,
                   pot_pulse);
        }
        else
        {
            // Normal behavior: servo follows potentiometer
            if (xSemaphoreTake(servo_mutex, portMAX_DELAY) == pdTRUE)
            {
                set_servo_pulse_us(pot_pulse);
                xSemaphoreGive(servo_mutex);
            }

            printf("Pot: %d | Servo angle: %d | Pulse: %d us\n",
                   pot_value,
                   pot_angle,
                   pot_pulse);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}