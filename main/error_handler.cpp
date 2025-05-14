#include "../inc/error_handler.hpp"

extern "C" void error_handler(void){
    ESP_LOGE("ERROR_HANDLER", "An error occurred in the MQTT client.");
    while (1);
};