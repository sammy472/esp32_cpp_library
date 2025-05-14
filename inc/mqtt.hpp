// @file MqttClient.hpp
// @brief MQTT client manager for ESP32 using C++ OOP approach
#pragma once

#include <string>
#include <vector>
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"

namespace ESP32_MQTT {

    /**
     * @brief MQTT connection status
     */
    enum class MqttStatus {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR
    };

    /**
     * @brief Callback for MQTT events
     * @param event MQTT event handle
     * @param user_data User provided data pointer
     */
    typedef void (*MqttEventCallback)(esp_mqtt_event_handle_t event, void* user_data);

    /**
     * @brief Class for managing MQTT connections on ESP32
     */
    class MqttClient {
    public:
        /**
         * @brief Get singleton instance
         */
        static MqttClient& getInstance();

        /**
         * @brief Initialize MQTT client library
         * @return true if init successful
         */
        bool init();

        /**
         * @brief Configure broker connection
         * @param uri Broker URI
         * @param clientId MQTT client identifier
         * @param username Optional username
         * @param password Optional password
         * @param keepalive Keepalive interval in seconds
         * @param cleanSession Clean session flag
         * @return true if configuration successful
         */
        bool configure(
            const std::string& uri,
            const std::string& clientId,
            const std::string& username = "",
            const std::string& password = "",
            int keepalive = 60,
            bool cleanSession = true
        );

        /**
         * @brief Set Last Will and Testament (LWT)
         * @param topic LWT topic
         * @param payload LWT payload
         * @param qos Quality of Service
         * @param retain Retain flag
         */
        void setWill(
            const std::string& topic,
            const std::string& payload,
            int qos = 0,
            bool retain = false
        );

        /**
         * @brief Connect to the MQTT broker
         * @return true if connection initiated
         */
        bool connect();

        /**
         * @brief Disconnect from the broker
         * @return true if disconnect initiated
         */
        bool disconnect();

        /**
         * @brief Publish a message
         * @param topic Topic string
         * @param payload Payload string
         * @param qos Quality of Service (0, 1, 2)
         * @param retain Retain flag
         * @return message id (>0) or negative on error
         */
        int publish(
            const std::string& topic,
            const std::string& payload,
            int qos = 0,
            bool retain = false
        );

        /**
         * @brief Publish binary data
         * @param topic Topic string
         * @param data Pointer to data
         * @param len Length of data
         * @param qos Quality of Service
         * @param retain Retain flag
         * @return message id or negative on error
         */
        int publish(
            const std::string& topic,
            const uint8_t* data,
            size_t len,
            int qos = 0,
            bool retain = false
        );

        /**
         * @brief Subscribe to a topic
         * @param topic Topic string
         * @param qos Desired QoS
         * @return message id or negative on error
         */
        int subscribe(const std::string& topic, int qos = 0);

        /**
         * @brief Unsubscribe from a topic
         * @param topic Topic string
         * @return message id or negative on error
         */
        int unsubscribe(const std::string& topic);

        /**
         * @brief Set event callback
         * @param callback Function to call on MQTT events
         * @param user_data User data pointer
         */
        void setEventCallback(MqttEventCallback callback, void* user_data = nullptr);

        /**
         * @brief Get current client status
         * @return MqttStatus enum value
         */
        MqttStatus getStatus() const;

        /**
         * @brief Get configured broker URI
         * @return Broker URI string
         */
        std::string getBrokerUri() const;

        /**
         * @brief Get client identifier
         * @return Client ID string
         */
        std::string getClientId() const;

    private:
        MqttClient();
        ~MqttClient();
        MqttClient(const MqttClient&) = delete;
        MqttClient& operator=(const MqttClient&) = delete;

        /**
         * @brief Internal MQTT event handler
         */
        static void eventHandlerCb(    
            void* arg, 
            esp_event_base_t eventBase, 
            int32_t eventId, 
            void* eventData
        );

        esp_mqtt_client_handle_t       _client = nullptr;
        esp_mqtt_client_config_t       _config = {};
        MqttStatus                     _status = MqttStatus::DISCONNECTED;
        MqttEventCallback              _userCallback = nullptr;
        void*                          _userData = nullptr;
        std::string                    _brokerUri;
        std::string                    _clientId;
        std::string                    _willTopic;
        std::string                    _willPayload;
        int                             _willQos = 0;
        bool                            _willRetain = false;
    };

    // C-compatible wrappers
    extern "C" {
        int mqtt_init();
        int mqtt_configure(
            const char* uri,
            const char* client_id,
            const char* username,
            const char* password,
            int keepalive,
            bool clean_session
        );

        int mqtt_set_will(
            const char* topic,
            const char* payload,
            int qos,
            bool retain
        );

        int mqtt_connect();
        int mqtt_disconnect();
        int mqtt_publish(
            const char* topic,
            const char* payload,
            int qos,
            bool retain
        );
        int mqtt_publish_binary(
            const char* topic,
            const uint8_t* data,
            size_t len,
            int qos,
            bool retain
        );
        int mqtt_subscribe(const char* topic, int qos);
        int mqtt_unsubscribe(const char* topic);
        int mqtt_get_status();
        const char* mqtt_get_broker_uri();
        const char* mqtt_get_client_id();
    }

} // namespace ESP32_MQTT
