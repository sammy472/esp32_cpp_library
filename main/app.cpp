#include <iostream>
#include<string>
#include <cstring>
#include <fstream>
#include "../inc/app.hpp" 
#include "../inc/mqtt.hpp"
#include "../inc/wifi.hpp"
#include "../inc/error_handler.hpp"


using namespace std;
using namespace ESP32_WIFI;
using namespace ESP32_MQTT;

//For MQTT Client
#define TAG_MQTT "MQTT_APP"
//Define the broker URL
#define BROKER_URI "ws://pf-co68wy8c3fz386csm6gr.cedalo.cloud/mqtt"

#define TAG_WIFI "WIFI_APP"

//For WIFI Station mode
// WiFi credentials - replace with your own
#define WIFI_SSID "Wifi_Perso_2.4Ghz"
#define WIFI_PASS "Jepasse5@"
#define ESP_HOSTNAME "esp32-device"

// For AP mode
#define AP_SSID "ESP32_AP"
#define AP_PASS "password123"
#define AP_CHANNEL 1
#define AP_MAX_CONNECTIONS 4

// Function prototypes
void app_main(void);
void wifi_station_mode_example(void);
void wifi_ap_mode_example(void);
void mqtt_configure(void);
void mqtt_run(void);
//char* get_file_text(string filename);


static void esp_app_main(void){
    ESP_LOGI(TAG_WIFI, "ESP32 WiFi Example Starting...");
    
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Choose which example to run
    wifi_station_mode_example();
    wifi_ap_mode_example();
    mqtt_configure();
    mqtt_run();
    
    ESP_LOGI(TAG_WIFI, "Example complete, entering infinite loop");

    int i = 0;
    char payload[16];

    while (1) {
        i++;
        // Format integer into a C-string
        int len = snprintf(payload, sizeof(payload), "%d", i);
        if (len < 0 || len >= sizeof(payload)) {
            ESP_LOGE(TAG_MQTT, "snprintf error or output truncated");
            len = 0;  // fallback to empty payload
        }

        // Publish using explicit length to skip strlen()
        int msg_id = mqtt_publish(
            "test/topic",
            payload,
            1,
            true
        );
        ESP_LOGI(TAG_MQTT, "Published msg_id=%d payload='%s'", msg_id, payload);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

extern "C" void application_main(void){
        esp_app_main();
}


/**
 * @brief Example of using WiFi in station mode
 */
void wifi_station_mode_example(void) {
    char ip_address[16] = {0};
    
    // Initialize WiFi
    ESP_LOGI(TAG_WIFI, "Initializing WiFi...");
    if (!wifi_init()) {
        ESP_LOGE(TAG_WIFI, "Failed to initialize WiFi");
        return;
    }
    
    // Configure as station
    ESP_LOGI(TAG_WIFI, "Configuring as station for SSID: %s", WIFI_SSID);
    if (!wifi_configure_station(WIFI_SSID, WIFI_PASS, ESP_HOSTNAME)) {
        ESP_LOGE(TAG_WIFI, "Failed to configure WiFi station");
        return;
    }
    
    // Start WiFi
    ESP_LOGI(TAG_WIFI, "Starting WiFi...");
    if (!wifi_start()) {
        ESP_LOGE(TAG_WIFI, "Failed to start WiFi");
        return;
    }
    
    // Wait for connection
    ESP_LOGI(TAG_WIFI, "Waiting for connection...");
    if (wifi_wait_for_connection(30000)) {
        ESP_LOGI(TAG_WIFI, "Connected to WiFi!");
        
        // Get IP address
        if (wifi_get_ip_address(ip_address, sizeof(ip_address))) {
            ESP_LOGI(TAG_WIFI, "IP Address: %s", ip_address);
        } else {
            ESP_LOGE(TAG_WIFI, "Failed to get IP address");
        }
    } else {
        ESP_LOGE(TAG_WIFI, "Failed to connect to WiFi");
    }
}

/**
 * @brief Example of using WiFi in AP mode
 */
void wifi_ap_mode_example(void) {
    //char ip_address[16] = {0};
    
    // Initialize WiFi
    ESP_LOGI(TAG_WIFI, "Initializing WiFi...");
    if (!wifi_init()) {
        ESP_LOGE(TAG_WIFI, "Failed to initialize WiFi");
        return;
    }
    
    // Configure as access point
    ESP_LOGI(TAG_WIFI, "Configuring as access point: %s", AP_SSID);
    if (!wifi_configure_ap(AP_SSID, AP_PASS, AP_CHANNEL, AP_MAX_CONNECTIONS)) {
        ESP_LOGE(TAG_WIFI, "Failed to configure WiFi AP");
        return;
    }
    
    ESP_LOGI(TAG_WIFI, "WiFi access point started");
    ESP_LOGI(TAG_WIFI, "SSID: %s", AP_SSID);
    ESP_LOGI(TAG_WIFI, "Password: %s", AP_PASS);
}
/**
 * @brief Configure MQTT client
 */
void mqtt_configure(void) {
    // Configure MQTT client
    ESP_LOGI(TAG_MQTT, "Configuring MQTT client...");
    if (!mqtt_init()) {
        ESP_LOGE(TAG_MQTT, "Failed to initialize MQTT client");
        return;
    }else{
        ESP_LOGI(TAG_MQTT, "MQTT client initialized successfully");
    }
    
    // Set broker URI and client ID
    if (!mqtt_configure(BROKER_URI, "ESP32_Client", "Abena", "Newtonian472", 60, true)) {
        ESP_LOGE(TAG_MQTT, "Failed to configure MQTT client");
        return;
    }else{
        ESP_LOGI(TAG_MQTT, "MQTT client configured successfully");
    }
    
    // Set Last Will and Testament (LWT)
    mqtt_set_will("lwt/topic", "Device disconnected", 1, true);
    ESP_LOGI(TAG_MQTT, "LWT set to topic: lwt/topic");
    ESP_LOGI(TAG_MQTT, "LWT payload: Device disconnected");
    ESP_LOGI(TAG_MQTT, "LWT QoS: 1");
    ESP_LOGI(TAG_MQTT, "LWT Retain:true");
    // Connect to MQTT broker
    if (!mqtt_connect()) {
        ESP_LOGE(TAG_MQTT, "Failed to connect to MQTT broker");
        return;
    }
    ESP_LOGI(TAG_MQTT, "Connected to MQTT broker");
}

/**
 * @brief Run MQTT client
 */
void mqtt_run(void) {
    // Publish a message
    const char* topic = "test/topic";
    const char* payload = "Hello from ESP32!";
    int qos = 1;
    bool retain = false;
    
    ESP_LOGI(TAG_MQTT, "Publishing message to topic: %s", topic);
    if (mqtt_publish(topic, payload, qos, retain) < 0) {
        ESP_LOGE(TAG_MQTT, "Failed to publish message");
        return;
    }
    
    ESP_LOGI(TAG_MQTT, "Message published successfully");
    ESP_LOGI(TAG_MQTT, "Topic: %s", topic);
    ESP_LOGI(TAG_MQTT, "Payload: %s", payload);
    ESP_LOGI(TAG_MQTT, "QoS: %d", qos);
    ESP_LOGI(TAG_MQTT, "Retain: %s", retain ? "true" : "false");
    // Subscribe to a topic
    const char* subscribe_topic = "test/subscribe";
    int subscribe_qos = 1;
    ESP_LOGI(TAG_MQTT, "Subscribing to topic: %s", subscribe_topic);
    if (mqtt_subscribe(subscribe_topic, subscribe_qos) < 0) {
        ESP_LOGE(TAG_MQTT, "Failed to subscribe to topic");
        return;
    }
    ESP_LOGI(TAG_MQTT, "Subscribed to topic successfully");
    ESP_LOGI(TAG_MQTT, "Topic: %s", subscribe_topic);
    ESP_LOGI(TAG_MQTT, "QoS: %d", subscribe_qos);
}

/*
char* get_file_text(string filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        ESP_LOGE(TAG_MQTT, "Failed to open file: %s", filename.c_str());
        return nullptr;
    }
    
    string text;
    getline(file, text, '\0'); // Read entire file
    file.close();
    
    char* result = new char[text.size() + 1];
    strcpy(result, text.c_str());
    
    return result;
}
*/