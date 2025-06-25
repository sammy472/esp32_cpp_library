

#pragma once
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <functional>
#include <vector>
#include <map>
#include "esp_err.h"
#include "esp_log.h"

namespace ESP32_ADC {
    class ADCManager {
    public:
        ADCManager();
        ~ADCManager();

        // One-shot mode
        esp_err_t configOneShot(
            adc_unit_t unit, 
            adc_channel_t channel, 
            adc_bitwidth_t bitwidth, 
            adc_atten_t atten
        );
        esp_err_t readOneShot(
            adc_unit_t unit, 
            adc_channel_t channel, 
            int* out_raw
        );
        esp_err_t readOneShotVoltage(
            adc_unit_t unit, 
            adc_channel_t channel, 
            int* out_voltage_mv
        );

        // Continuous mode
        esp_err_t configContinuous(
            adc_unit_t unit, 
            const std::vector<adc_channel_t>& channels,
            adc_bitwidth_t bitwidth, adc_atten_t atten,
            uint32_t sample_freq_hz = 1000,
            size_t sample_buffer_size = 1024
        );
        esp_err_t startContinuous();
        esp_err_t stopContinuous();
        void registerCallback(std::function<void(const adc_continuous_evt_data_t&)> cb);

    private:
        ADCManager(const ADCManager&) = delete;
        ADCManager& operator=(const ADCManager&) = delete;

        adc_oneshot_unit_handle_t one_shot_unit_handle;
        adc_continuous_handle_t continuous_handle;
        adc_cali_handle_t cali_handle;
        std::function<void(const adc_continuous_evt_data_t&)> user_callback;
        static void continuousCallback(
            adc_continuous_handle_t, 
            const adc_continuous_evt_data_t*, 
            void*
        );

        bool continuous_running = false;
        std::map<adc_channel_t, adc_atten_t> channel_atten_map;
    };
}