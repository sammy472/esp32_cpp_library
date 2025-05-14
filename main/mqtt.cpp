// @file MqttClient.cpp
// @brief Implementation of MQTT client manager for ESP32 using C++ OOP approach

#include "../inc/mqtt.hpp"
#include <cstring>

namespace ESP32_MQTT {

MqttClient& MqttClient::getInstance() {
    static MqttClient instance;
    return instance;
}

MqttClient::MqttClient()
    :_client(nullptr),
    _config({}),
    _status(MqttStatus::DISCONNECTED),
    _userCallback(nullptr),
    _userData(nullptr),
    _brokerUri(""),
    _clientId(""),
    _willTopic(""),
    _willPayload(""),
    _willQos(0),
    _willRetain(false)
{
    ESP_LOGI("MQTT", "MqttClient instance created");
}

MqttClient::~MqttClient() {
    if (_client) {
        esp_mqtt_client_stop(_client);
        esp_mqtt_client_destroy(_client);
        _client = nullptr;
    }
}

bool MqttClient::init() {
    // Nothing to init beyond configuration
    return true;
}

bool MqttClient::configure(
    const std::string& uri,
    const std::string& clientId,
    const std::string& username,
    const std::string& password,
    int keepalive,
    bool cleanSession
) {
    _brokerUri = uri;
    _clientId = clientId;

    memset(&_config, 0, sizeof(_config));
    _config.broker.address.uri = _brokerUri.c_str();
    _config.credentials.client_id = _clientId.c_str();
    if (!username.empty()) _config.credentials.username = username.c_str();
    if (!password.empty()) {
        _config.credentials.authentication.password = password.c_str();
    };
    _config.session.keepalive = keepalive;
    _config.session.disable_clean_session = cleanSession;
    // LWT fields if setWill called earlier
    if (!_willTopic.empty()) {
        _config.session.last_will.topic = _willTopic.c_str();
        _config.session.last_will.msg = _willPayload.c_str();
        _config.session.last_will.qos = _willQos;
        _config.session.last_will.retain = _willRetain;
    }

    _client = esp_mqtt_client_init(&_config);
    if (!_client) {
        _status = MqttStatus::ERROR;
        return false;
    }

    esp_mqtt_client_register_event(
        _client, 
        MQTT_EVENT_ANY, 
        &MqttClient::eventHandlerCb, 
        this
    );
    _status = MqttStatus::DISCONNECTED;
    return true;
}

void MqttClient::setWill(const std::string& topic,
                         const std::string& payload,
                         int qos,
                         bool retain) {
    _willTopic = topic;
    _willPayload = payload;
    _willQos = qos;
    _willRetain = retain;
}

bool MqttClient::connect() {
    if (!_client) return false;
    esp_err_t err = esp_mqtt_client_start(_client);
    if (err == ESP_OK) {
        _status = MqttStatus::CONNECTING;
        return true;
    }
    _status = MqttStatus::ERROR;
    return false;
}

bool MqttClient::disconnect() {
    if (!_client) return false;
    esp_err_t err = esp_mqtt_client_stop(_client);
    if (err == ESP_OK) {
        _status = MqttStatus::DISCONNECTED;
        return true;
    }
    return false;
}

int MqttClient::publish(
    const std::string& topic,
    const std::string& payload,
    int qos,
    bool retain
) {
    if (!_client) return -1;
    return esp_mqtt_client_publish(_client, topic.c_str(), payload.c_str(), 0, qos, retain);
}

int MqttClient::publish(
    const std::string& topic,
    const uint8_t* data,
    size_t len,
    int qos,
    bool retain
) {
    if (!_client) return -1;
    return esp_mqtt_client_publish(_client, topic.c_str(), reinterpret_cast<const char*>(data), len, qos, retain);
}

int MqttClient::subscribe(const std::string& topic, int qos) {
    if (!_client) return -1;
    return esp_mqtt_client_subscribe_single(_client, topic.c_str(), qos);
}

int MqttClient::unsubscribe(const std::string& topic) {
    if (!_client) return -1;
    return esp_mqtt_client_unsubscribe(_client, topic.c_str());
}

void MqttClient::setEventCallback(MqttEventCallback callback, void* user_data) {
    _userCallback = callback;
    _userData = user_data;
}

MqttStatus MqttClient::getStatus() const {
    return _status;
}

std::string MqttClient::getBrokerUri() const {
    return _brokerUri;
}

std::string MqttClient::getClientId() const {
    return _clientId;
}

// Internal MQTT event handler
void MqttClient::eventHandlerCb(        
    void* arg, 
    esp_event_base_t eventBase, 
    int32_t eventId, 
    void* eventData
) {
    MqttClient* self = static_cast<MqttClient*>(arg);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(eventData);
    switch (eventId) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI("MQTT", "Before Connect");
            self->_status = MqttStatus::CONNECTING;
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("MQTT", "Connected");
            self->_status = MqttStatus::CONNECTED;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI("MQTT", "Disconnected");
            self->_status = MqttStatus::DISCONNECTED;
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI("MQTT", "Subscribed, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI("MQTT", "Unsubscribed, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI("MQTT", "Published, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI("MQTT", "Data received on topic %.*s: %.*s", 
                     event->topic_len, event->topic, 
                     event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE("MQTT", "Error");
            self->_status = MqttStatus::ERROR;
            break;
        default:
            ESP_LOGI("MQTT", "Other event id %d", event->event_id);
            break;
    }
    if (self->_userCallback) {
        self->_userCallback(event, self->_userData);
    }
}

// C-compatible wrappers
extern "C" {

int mqtt_init() {
    return ESP32_MQTT::MqttClient::getInstance().init() ? 1 : 0;
}

int mqtt_configure(const char* uri,
                   const char* client_id,
                   const char* username,
                   const char* password,
                   int keepalive,
                   bool clean_session) {
    return ESP32_MQTT::MqttClient::getInstance()
        .configure(uri, client_id, username ? username : "", password ? password : "", keepalive, clean_session) ? 1 : 0;
}

int mqtt_set_will(const char* topic,
                  const char* payload,
                  int qos,
                  bool retain) {
    ESP32_MQTT::MqttClient::getInstance().setWill(topic, payload, qos, retain);
    return 1;
}

int mqtt_connect() {
    return ESP32_MQTT::MqttClient::getInstance().connect() ? 1 : 0;
}

int mqtt_disconnect() {
    return ESP32_MQTT::MqttClient::getInstance().disconnect() ? 1 : 0;
}

int mqtt_publish(const char* topic,
                 const char* payload,
                 int qos,
                 bool retain) {
    return ESP32_MQTT::MqttClient::getInstance().publish(topic, payload, qos, retain);
}

int mqtt_publish_binary(const char* topic,
                         const uint8_t* data,
                         size_t len,
                         int qos,
                         bool retain) {
    return ESP32_MQTT::MqttClient::getInstance().publish(topic, data, len, qos, retain);
}

int mqtt_subscribe(const char* topic, int qos) {
    return ESP32_MQTT::MqttClient::getInstance().subscribe(topic, qos);
}

int mqtt_unsubscribe(const char* topic) {
    return ESP32_MQTT::MqttClient::getInstance().unsubscribe(topic);
}

int mqtt_get_status() {
    return static_cast<int>(ESP32_MQTT::MqttClient::getInstance().getStatus());
}

const char* mqtt_get_broker_uri() {
    return ESP32_MQTT::MqttClient::getInstance().getBrokerUri().c_str();
}

const char* mqtt_get_client_id() {
    return ESP32_MQTT::MqttClient::getInstance().getClientId().c_str();
}

} // extern "C"

} // namespace ESP32_MQTT 
 