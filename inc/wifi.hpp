/**
 * @file WiFiManager.hpp
 * @brief WiFi connection manager for ESP32 using C++ OOP approach
 */
#pragma once

#include <string>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include <vector>

namespace ESP32_WIFI {

/**
 * @brief WiFi connection modes
 */
enum class WiFiMode {
    STATION,      // Connect to existing WiFi network
    ACCESS_POINT, // Create a WiFi access point
    BOTH          // Both station and access point modes
};

/**
 * @brief WiFi connection status
 */
enum class WiFiStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

/**
 * @brief Callback function type for WiFi events
 */
typedef void (*WiFiEventCallback)(WiFiStatus status, void* userData);

/**
 * @brief Class for managing WiFi connections on ESP32
 */
class WiFiManager {
public:
    /**
     * @brief Get singleton instance
     * @return Reference to the WiFiManager instance
     */
    static WiFiManager& getInstance();

    /**
     * @brief Initialize WiFi subsystem
     * @return true if initialization successful
     */
    bool init();

    /**
     * @brief Configure as WiFi station
     * @param ssid Network SSID
     * @param password Network password
     * @param hostname Optional hostname for the ESP32
     * @return true if configuration successful
     */
    bool configureStation(
        const std::string& ssid, 
        const std::string& password, 
        const std::string& hostname = ""
    );

    /**
     * @brief Configure as WiFi access point
     * @param ssid AP SSID
     * @param password AP password (empty for open network)
     * @param channel WiFi channel (1-13)
     * @param maxConnections Maximum number of connections
     * @return true if configuration successful
     */
    bool configureAP(
        const std::string& ssid, 
        const std::string& password = "", 
        uint8_t channel = 1, 
        uint8_t maxConnections = 4
    );

    /**
     * @brief Start WiFi with configured settings
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop WiFi
     * @return true if stopped successfully
     */
    bool stop();

    /**
     * @brief Get current connection status
     * @return WiFiStatus enum value
     */
    WiFiStatus getStatus() const;

    /**
     * @brief Get connected IP address (station mode)
     * @return IP address as string, empty if not connected
     */
    std::string getIPAddress() const;

    /**
     * @brief Get AP IP address (AP mode)
     * @return AP IP address as string
     */
    std::string getAPIPAddress() const;

    /**
     * @brief Set callback for WiFi events
     * @param callback Function to call on WiFi events
     * @param userData User data to pass to callback
     */
    void setEventCallback(
        WiFiEventCallback callback, 
        void* userData = nullptr
    );

    /**
     * @brief Wait for connection with timeout
     * @param timeoutMs Timeout in milliseconds
     * @return true if connected before timeout
     */
    bool waitForConnection(uint32_t timeoutMs = 30000);

    /**
     * @brief Scan for available WiFi networks
     * @param maxResults Maximum number of results to return
     * @return Vector of available networks
     */
    std::vector<wifi_ap_record_t> scanNetworks(uint16_t maxResults = 20);

private:
    // Singleton implementation
    WiFiManager();
    ~WiFiManager();
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;

    // Internal event handler
    static void wifiEventHandler(
        void* arg, 
        esp_event_base_t eventBase, 
        int32_t eventId, 
        void* eventData
    );

    // Internal state
    WiFiMode _mode;
    WiFiStatus _status;
    std::string _stationSSID;
    std::string _stationPassword;
    std::string _apSSID;
    std::string _apPassword;
    WiFiEventCallback _eventCallback;
    void* _userCallbackData;
    bool _initialized;
    
    // Event group for synchronization
    esp_netif_t* _staNetif;
    esp_netif_t* _apNetif;
};

// C-compatible wrapper functions for interfacing with C code
extern "C" {
    /**
     * @brief Initialize WiFi subsystem
     * @return 1 if successful, 0 otherwise
     */
    int wifi_init();
    
    /**
     * @brief Configure as WiFi station
     * @param ssid Network SSID
     * @param password Network password
     * @param hostname Optional hostname for the ESP32 (can be NULL)
     * @return 1 if successful, 0 otherwise
     */
    int wifi_configure_station(
        const char* ssid, 
        const char* password, 
        const char* hostname
    );
    
    /**
     * @brief Configure as WiFi access point
     * @param ssid AP SSID
     * @param password AP password (can be NULL for open network)
     * @param channel WiFi channel (1-13)
     * @param max_connections Maximum number of connections
     * @return 1 if successful, 0 otherwise
     */
    int wifi_configure_ap(
        const char* ssid, 
        const char* password, 
        uint8_t channel, 
        uint8_t max_connections
    );
    
    /**
     * @brief Start WiFi with configured settings
     * @return 1 if successful, 0 otherwise
     */
    int wifi_start();
    
    /**
     * @brief Stop WiFi
     * @return 1 if successful, 0 otherwise
     */
    int wifi_stop();
    
    /**
     * @brief Get current connection status
     * @return Status code (0=disconnected, 1=connecting, 2=connected, 3=failed)
     */
    int wifi_get_status();
    
    /**
     * @brief Get connected IP address (station mode)
     * @param ip_buffer Buffer to store IP address string
     * @param buffer_size Size of buffer
     * @return 1 if IP available, 0 otherwise
     */
    int wifi_get_ip_address(
        char* ip_buffer, 
        size_t buffer_size
    );
    
    /**
     * @brief Wait for connection with timeout
     * @param timeout_ms Timeout in milliseconds
     * @return 1 if connected before timeout, 0 otherwise
     */
    int wifi_wait_for_connection(uint32_t timeout_ms);
}

} // namespace ESP32_WIFI


