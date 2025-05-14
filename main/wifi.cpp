/**
 * @file WiFiManager.cpp
 * @brief Implementation of WiFi connection manager for ESP32
 */
#include "../inc/wifi.hpp"
#include <vector>
#include "freertos/FreeRTOS.h"  // Used only for delay functions, not tasks
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <string.h>

#define TAG "WiFiManager"

namespace ESP32_WIFI {

    // Event group bits
    static const int WIFI_CONNECTED_BIT = BIT0;
    static const int WIFI_FAIL_BIT = BIT1;
    static EventGroupHandle_t s_wifi_event_group = nullptr;

    WiFiManager::WiFiManager() 
        : _mode(WiFiMode::STATION),
        _status(WiFiStatus::DISCONNECTED),
        _eventCallback(nullptr),
        _userCallbackData(nullptr),
        _initialized(false),
        _staNetif(nullptr),
        _apNetif(nullptr)
    {
    }

    WiFiManager::~WiFiManager() {
        stop();
        
        if (_staNetif) {
            esp_netif_destroy(_staNetif);
            _staNetif = nullptr;
        }
        
        if (_apNetif) {
            esp_netif_destroy(_apNetif);
            _apNetif = nullptr;
        }
        
        if (s_wifi_event_group) {
            vEventGroupDelete(s_wifi_event_group);
            s_wifi_event_group = nullptr;
        }
    }

    WiFiManager& WiFiManager::getInstance() {
        static WiFiManager instance;
        return instance;
    }

    bool WiFiManager::init() {
        if (_initialized) {
            return true;
        }
        
        // Initialize NVS
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        
        // Initialize event loop
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        
        // Initialize TCP/IP stack
        ESP_ERROR_CHECK(esp_netif_init());
        
        // Create event group
        s_wifi_event_group = xEventGroupCreate();
        if (!s_wifi_event_group) {
            ESP_LOGE(TAG, "Failed to create event group");
            return false;
        }
        
        // Create default STA and AP netif instances
        _staNetif = esp_netif_create_default_wifi_sta();
        _apNetif = esp_netif_create_default_wifi_ap();
        
        if (!_staNetif || !_apNetif) {
            ESP_LOGE(TAG, "Failed to create netif instances");
            return false;
        }
        
        // Initialize WiFi with default config
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        
        // Register event handlers
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, 
                                                ESP_EVENT_ANY_ID, 
                                                &WiFiManager::wifiEventHandler, 
                                                this));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, 
                                                IP_EVENT_STA_GOT_IP, 
                                                &WiFiManager::wifiEventHandler, 
                                                this));
        
        _initialized = true;
        return true;
    }

    bool WiFiManager::configureStation(const std::string& ssid, const std::string& password, 
                                    const std::string& hostname) {
        if (!_initialized && !init()) {
            return false;
        }
        
        _stationSSID = ssid;
        _stationPassword = password;
        
        // Set hostname if provided
        if (!hostname.empty()) {
            esp_netif_set_hostname(_staNetif, hostname.c_str());
        }
        
        // Update mode to include station
        if (_mode == WiFiMode::ACCESS_POINT) {
            _mode = WiFiMode::BOTH;
        } else {
            _mode = WiFiMode::STATION;
        }
        
        return true;
    }

    bool WiFiManager::configureAP(const std::string& ssid, const std::string& password, 
                                uint8_t channel, uint8_t maxConnections) {
        if (!_initialized && !init()) {
            return false;
        }
        
        _apSSID = ssid;
        _apPassword = password;
        
        // Update mode to include AP
        if (_mode == WiFiMode::STATION) {
            _mode = WiFiMode::BOTH;
        } else {
            _mode = WiFiMode::ACCESS_POINT;
        }
        
        // Configure AP
        wifi_config_t wifi_config = {};
        strncpy((char*)wifi_config.ap.ssid, ssid.c_str(), sizeof(wifi_config.ap.ssid) - 1);
        wifi_config.ap.ssid_len = ssid.length();
        
        if (password.empty()) {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        } else {
            strncpy((char*)wifi_config.ap.password, password.c_str(), sizeof(wifi_config.ap.password) - 1);
            wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        }
        
        wifi_config.ap.max_connection = maxConnections;
        wifi_config.ap.channel = channel;
        
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        
        return true;
    }

    bool WiFiManager::start() {
        if (!_initialized && !init()) {
            return false;
        }
        
        // Set WiFi mode
        wifi_mode_t mode = WIFI_MODE_NULL;
        switch (_mode) {
            case WiFiMode::STATION:
                mode = WIFI_MODE_STA;
                break;
            case WiFiMode::ACCESS_POINT:
                mode = WIFI_MODE_AP;
                break;
            case WiFiMode::BOTH:
                mode = WIFI_MODE_APSTA;
                break;
        }
        ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
        
        // Configure station if needed
        if (_mode == WiFiMode::STATION || _mode == WiFiMode::BOTH) {
            wifi_config_t wifi_config = {};
            strncpy((char*)wifi_config.sta.ssid, _stationSSID.c_str(), sizeof(wifi_config.sta.ssid) - 1);
            strncpy((char*)wifi_config.sta.password, _stationPassword.c_str(), sizeof(wifi_config.sta.password) - 1);
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        }
        
        // Start WiFi
        ESP_ERROR_CHECK(esp_wifi_start());
        
        // Connect if in station mode
        if (_mode == WiFiMode::STATION || _mode == WiFiMode::BOTH) {
            ESP_ERROR_CHECK(esp_wifi_connect());
            _status = WiFiStatus::CONNECTING;
        }
        
        ESP_LOGI(TAG, "WiFi started in mode %d", (int)_mode);
        return true;
    }

    bool WiFiManager::stop() {
        if (!_initialized) {
            return true;
        }
        
        esp_err_t err = esp_wifi_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(err));
            return false;
        }
        
        _status = WiFiStatus::DISCONNECTED;
        return true;
    }

    WiFiStatus WiFiManager::getStatus() const {
        return _status;
    }

    std::string WiFiManager::getIPAddress() const {
        if (_status != WiFiStatus::CONNECTED) {
            return "";
        }
        
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(_staNetif, &ip_info);
        
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        
        return std::string(ip_str);
    }

    std::string WiFiManager::getAPIPAddress() const {
        if (_mode != WiFiMode::ACCESS_POINT && _mode != WiFiMode::BOTH) {
            return "";
        }
        
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(_apNetif, &ip_info);
        
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        
        return std::string(ip_str);
    }

    void WiFiManager::setEventCallback(WiFiEventCallback callback, void* userData) {
        _eventCallback = callback;
        _userCallbackData = userData;
    }

    bool WiFiManager::waitForConnection(uint32_t timeoutMs) {
        if (_mode != WiFiMode::STATION && _mode != WiFiMode::BOTH) {
            return false;
        }
        
        // Wait for connection using event bits
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE,
                                            pdFALSE,
                                            timeoutMs / portTICK_PERIOD_MS);
        
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to AP SSID: %s", _stationSSID.c_str());
            return true;
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Failed to connect to SSID: %s", _stationSSID.c_str());
            return false;
        } else {
            ESP_LOGI(TAG, "Connection timeout");
            return false;
        }
    }

    std::vector<wifi_ap_record_t> WiFiManager::scanNetworks(uint16_t maxResults) {
        std::vector<wifi_ap_record_t> results;
        
        // Check if WiFi is initialized
        if (!_initialized && !init()) {
            return results;
        }
        
        // Set to station mode temporarily for scanning
        wifi_mode_t currentMode;
        esp_wifi_get_mode(&currentMode);
        
        if (currentMode == WIFI_MODE_NULL) {
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_start();
        }
        
        // Allocate buffer for scan results
        uint16_t number = maxResults;
        wifi_ap_record_t* list = new wifi_ap_record_t[number];
        
        if (list == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate memory for scan results");
            return results;
        }
        
        // Perform scan
        esp_err_t err = esp_wifi_scan_start(NULL, true);
        if (err == ESP_OK) {
            err = esp_wifi_scan_get_ap_records(&number, list);
            if (err == ESP_OK) {
                for (int i = 0; i < number; i++) {
                    results.push_back(list[i]);
                }
            }
        }
        
        // Clean up
        delete []list;
        
        // Restore original state if we changed it
        if (currentMode == WIFI_MODE_NULL) {
            esp_wifi_stop();
        }
        
        return results;
    }

    void WiFiManager::wifiEventHandler(
        void* arg, 
        esp_event_base_t eventBase, 
        int32_t eventId, 
        void* eventData
    ){
        WiFiManager* self = static_cast<WiFiManager*>(arg);
        
        if (eventBase == WIFI_EVENT) {
            if (eventId == WIFI_EVENT_STA_START) {
                esp_wifi_connect();
                self->_status = WiFiStatus::CONNECTING;
            } else if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
                self->_status = WiFiStatus::DISCONNECTED;
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGI(TAG, "Retry connecting to AP");
                
                // Notify callback if registered
                if (self->_eventCallback) {
                    self->_eventCallback(WiFiStatus::DISCONNECTED, self->_userCallbackData);
                }
            } else if (eventId == WIFI_EVENT_AP_STACONNECTED) {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)eventData;
                ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d", 
                        MAC2STR(event->mac), event->aid);
            } else if (eventId == WIFI_EVENT_AP_STADISCONNECTED) {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)eventData;
                ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d", 
                        MAC2STR(event->mac), event->aid);
            }
        } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)eventData;
            ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
            self->_status = WiFiStatus::CONNECTED;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            
            // Notify callback if registered
            if (self->_eventCallback) {
                self->_eventCallback(WiFiStatus::CONNECTED, self->_userCallbackData);
            }
        }
    }

    // C-compatible wrapper function implementations
    extern "C" {

        int wifi_init() {
            return WiFiManager::getInstance().init() ? 1 : 0;
        }

        int wifi_configure_station(const char* ssid, const char* password, const char* hostname) {
            std::string hostnameStr = hostname ? hostname : "";
            return WiFiManager::getInstance().configureStation(ssid, password, hostnameStr) ? 1 : 0;
        }

        int wifi_configure_ap(const char* ssid, const char* password, uint8_t channel, uint8_t max_connections) {
            std::string passwordStr = password ? password : "";
            return WiFiManager::getInstance().configureAP(ssid, passwordStr, channel, max_connections) ? 1 : 0;
        }

        int wifi_start() {
            return WiFiManager::getInstance().start() ? 1 : 0;
        }

        int wifi_stop() {
            return WiFiManager::getInstance().stop() ? 1 : 0;
        }

        int wifi_get_status() {
            return static_cast<int>(WiFiManager::getInstance().getStatus());
        }

        int wifi_get_ip_address(char* ip_buffer, size_t buffer_size) {
            std::string ip = WiFiManager::getInstance().getIPAddress();
            if (ip.empty()) {
                return 0;
            }
            
            strncpy(ip_buffer, ip.c_str(), buffer_size - 1);
            ip_buffer[buffer_size - 1] = '\0';
            return 1;
        }

        int wifi_wait_for_connection(uint32_t timeout_ms) {
            return WiFiManager::getInstance().waitForConnection(timeout_ms) ? 1 : 0;
        }

    } // extern "C"

} // namespace ESP32_WIFI