#include "gpio_tasks.h"
#include "driver/ledc.h"

static QueueHandle_t xRelayQueue = NULL;
static QueueHandle_t xOnPCBLedQueue = NULL; 

typedef struct {
    int relay_num;
    int state;
} RelayCommand;

void ledc_init() {
    // Безопасная инициализация через полное обнуление структуры ({0})
    ledc_timer_config_t ledc_timer = {}; 
    ledc_timer.speed_mode      = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num       = LEDC_TIMER_0;
    ledc_timer.duty_resolution = LEDC_TIMER_10_BIT;
    ledc_timer.freq_hz         = 5000;
    ledc_timer.clk_cfg         = LEDC_AUTO_CLK;

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel_1 = {};
    ledc_channel_1.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel_1.channel    = LEDC_CHANNEL_0;
    ledc_channel_1.timer_sel  = LEDC_TIMER_0;
    ledc_channel_1.intr_type  = LEDC_INTR_DISABLE;
    ledc_channel_1.gpio_num   = LED_PWM_GPIO;
    ledc_channel_1.duty       = 0;
    ledc_channel_1.hpoint     = 0;

    ledc_channel_config(&ledc_channel_1);

    ledc_fade_func_install(0);
}

void relay_send_command(int relay_num, int state) {
    RelayCommand command;
    command.relay_num = relay_num;
    command.state = state;
    if (xRelayQueue != NULL) {
        xQueueSend(xRelayQueue, &command, 0);
    }
}

void led_onpcb_send_command(int pwm_value) {
    if (xOnPCBLedQueue != NULL) {
        xQueueSend(xOnPCBLedQueue, &pwm_value, 0);
    }
}

void onpcb_led_task(void *pvParameter) {
    int pwm_value;
    while (1) {
        if (xQueueReceive(xOnPCBLedQueue, &pwm_value, portMAX_DELAY) == pdTRUE) {
           
            // 3. Запуск команды (целевое значение: 1023, время: 3000 мс)
            int current_duty = 1023 - pwm_value; // Преобразуем яркость в скважность


    ledc_set_fade_time_and_start(
        LEDC_LOW_SPEED_MODE, 
        LEDC_CHANNEL_0, 
        current_duty,                 // Целевая скважность (100% для 10 бит)
        3000,                 // Время перехода в миллисекундах (3 секунды)
        LEDC_FADE_NO_WAIT     // Важно: вернуть управление мгновенно!
        );
        }
    }
}

void relayTask(void *pvParameter){
    RelayCommand command;

    while(1){
        if(xQueueReceive(xRelayQueue, &command, portMAX_DELAY) == pdTRUE){
            gpio_set_level((gpio_num_t)command.relay_num, command.state);
        }
    }
}

void gpio_tasks_init(){
    ledc_init(); // Инициализация PWM для светодиода
    
    // Создаем очередь для реле здесь, чтобы она не была NULL
    xRelayQueue = xQueueGenericCreate(10, sizeof(RelayCommand), queueQUEUE_TYPE_BASE);
    xOnPCBLedQueue = xQueueGenericCreate(10, sizeof(int), queueQUEUE_TYPE_BASE);

    // Настройка GPIO для реле и светодиода
    gpio_config_t io_conf = {}; // Также лучше обнулить во избежание проблем
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << RELAY_1_GPIO) | (1ULL << RELAY_2_GPIO) | (1ULL << RELAY_3_GPIO) | (1ULL << RELAY_4_GPIO);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    // Создание задачи для управления реле
    xTaskCreate(relayTask, "relayTask", 2048, NULL, 5, NULL);
    // Создание задачи для управления яркостью светодиода
    xTaskCreate(onpcb_led_task, "onpcb_led_task", 2048, NULL, 5, NULL);
}