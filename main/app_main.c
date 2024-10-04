#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

static const char *TAG = "mqtt_example";

#define BUTTON_GPIO GPIO_NUM_23
#define MQTT_TOPIC "KMITL/SIET/65030034/topic/switch"

static esp_mqtt_client_handle_t client;

static void init_button(void) {
    esp_rom_gpio_pad_select_gpio(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    // (ไม่มีการเปลี่ยนแปลงในโค้ดนี้)
}

static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "MQTT client started."); // Log to check if MQTT client starts successfully
}

void button_task(void *arg) {
    static int button_state = 0; // เริ่มต้นสถานะปุ่มเป็น 0

    while (1) {
        int current_state = gpio_get_level(BUTTON_GPIO);
        ESP_LOGI(TAG, "Current button state: %d", current_state); // Log current button state

        // ตรวจสอบการกดปุ่ม
        if (current_state == 1) {
            // รอจนกว่าปุ่มจะถูกปล่อยเพื่อหลีกเลี่ยงการอ่านค่าหลายครั้ง
            vTaskDelay(50 / portTICK_PERIOD_MS); // หน่วงเวลาเพื่อรอให้ปุ่มถูกปล่อย
            if (gpio_get_level(BUTTON_GPIO) == 1) { // ถ้าปุ่มยังถูกกด
                // สลับสถานะปุ่ม
                button_state = !button_state; // เปลี่ยนค่า 1 เป็น 0 หรือ 0 เป็น 1
                const char *payload = button_state ? "1" : "0"; // ตั้งค่า payload ตามสถานะปุ่ม
                esp_mqtt_client_publish(client, MQTT_TOPIC, payload, 0, 1, 0);
                ESP_LOGI(TAG, "Button state changed, published: %s", payload);
            }
            // รอจนกว่าปุ่มจะถูกปล่อย
            while (gpio_get_level(BUTTON_GPIO) == 1) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS); // Delay for debouncing
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    init_button();
    mqtt_app_start();

    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    ESP_ERROR_CHECK(example_connect());
}
