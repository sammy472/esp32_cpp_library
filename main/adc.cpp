// @file ADCManager.cpp
#include "../inc/adc.hpp"
#include "esp_log.h"
#include <cstring>

#define TAG "ADCManager"

namespace ESP32_ADC {
    ADCManager::ADCManager():
        one_shot_unit_handle(nullptr), 
        continuous_handle(nullptr), 
        cali_handle(nullptr), 
        user_callback(nullptr), 
        continuous_running(false){}

    ADCManager::~ADCManager() {
        if (continuous_running) {
            this->stopContinuous();
        }
        if (continuous_handle) {
            adc_continuous_deinit(continuous_handle);
        }
        if (one_shot_unit_handle) {
            adc_oneshot_del_unit(one_shot_unit_handle);
        }
        if (cali_handle) {
            adc_cali_delete_scheme_line_fitting(cali_handle);
        }
    }

    // One-shot configuration
    esp_err_t ADCManager::configOneShot(
        adc_unit_t unit, 
        adc_channel_t channel, 
        adc_bitwidth_t bitwidth, 
        adc_atten_t atten
    ) {
        adc_oneshot_unit_init_cfg_t unit_cfg = {};
        unit_cfg.unit_id = unit;
        unit_cfg.ulp_mode = ADC_ULP_MODE_DISABLE;
        unit_cfg.clk_src = ADC_RTC_CLK_SRC_RC_FAST;

        esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &one_shot_unit_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create one-shot ADC unit");
            return ret;
        }

        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = atten,
            .bitwidth = bitwidth
        };

        ret = adc_oneshot_config_channel(one_shot_unit_handle, channel, &chan_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure one-shot channel");
            return ret;
        }

        // Setup calibration
        adc_cali_line_fitting_config_t cali_cfg = {};
        cali_cfg.unit_id = unit;
        cali_cfg.atten = atten;
        cali_cfg.bitwidth = bitwidth;

        ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Calibration not supported");
            cali_handle = nullptr;
        }
        ESP_LOGI(
            TAG, 
            "One-shot ADC configured: unit=%d, channel=%d, bitwidth=%d, atten=%d",
            unit, 
            channel, 
            bitwidth, 
            atten
        );

        return ESP_OK;
    }

    esp_err_t ADCManager::readOneShot(
        adc_unit_t unit, 
        adc_channel_t channel, 
        int* out_raw
    ) {
        if (!one_shot_unit_handle) return ESP_ERR_INVALID_STATE;
        return adc_oneshot_read(one_shot_unit_handle, channel, out_raw);
    }

    esp_err_t ADCManager::readOneShotVoltage(
        adc_unit_t unit, 
        adc_channel_t channel, 
        int* out_voltage_mv
    ) {
        int raw = 0;
        esp_err_t ret = readOneShot(unit, channel, &raw);
        if (ret != ESP_OK) return ret;

        if (cali_handle) {
            return adc_cali_raw_to_voltage(cali_handle, raw, out_voltage_mv);
        } else {
            *out_voltage_mv = raw; // No calibration
            return ESP_OK;
        }
    }

    // Continuous configuration
    esp_err_t ADCManager::configContinuous(
        adc_unit_t unit, 
        const std::vector<adc_channel_t>& channels,
        adc_bitwidth_t bitwidth, adc_atten_t atten,
        uint32_t sample_freq_hz, 
        size_t sample_buffer_size
    ) {

        adc_continuous_handle_cfg_t handle_cfg = {};
        handle_cfg.max_store_buf_size = sample_buffer_size;
        handle_cfg.conv_frame_size = sample_buffer_size / 2;
        handle_cfg.flags.flush_pool = true;

        esp_err_t ret = adc_continuous_new_handle(&handle_cfg, &continuous_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create continuous ADC handle");
            return ret;
        }

        std::vector<adc_channel_t> chs = channels;
        std::vector<adc_channel_t> adc_channels;

        for (auto ch : chs) {
            adc_channels.push_back(ch);
            this->channel_atten_map[ch] = atten;
        }

        adc_continuous_config_t adc_config = {};
        adc_config.sample_freq_hz = sample_freq_hz;
        adc_config.conv_mode = ADC_CONV_SINGLE_UNIT_1; 
        adc_config.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
        adc_config.adc_pattern = reinterpret_cast<adc_digi_pattern_config_t*>(adc_channels.data());
        adc_config.pattern_num = static_cast<uint32_t>(adc_channels.size());

        ret = adc_continuous_config(continuous_handle, &adc_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure continuous ADC");
            return ret;
        }

        adc_continuous_evt_cbs_t cbs = {};
        cbs.on_conv_done = ADCManager::continuousCallback;
        cbs.on_pool_ovf = nullptr;

        ret = adc_continuous_register_event_callbacks(continuous_handle, &cbs, this);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register callback");
        }

        return ESP_OK;
    }

    esp_err_t ADCManager::startContinuous() {
        if (!continuous_handle) return ESP_ERR_INVALID_STATE;
        esp_err_t ret = adc_continuous_start(continuous_handle);
        if (ret == ESP_OK) continuous_running = true;
        return ret;
    }

    esp_err_t ADCManager::stopContinuous() {
        if (!continuous_handle) return ESP_ERR_INVALID_STATE;
        esp_err_t ret = adc_continuous_stop(continuous_handle);
        if (ret == ESP_OK) continuous_running = false;
        return ret;
    }

    void ADCManager::registerCallback(std::function<void(const adc_continuous_evt_data_t&)> cb) {
        user_callback = std::move(cb);
    }

    bool  ADCManager::continuousCallback(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* event_data, void* user_data) {
        ADCManager* manager = reinterpret_cast<ADCManager*>(user_data);
        if (manager && manager->user_callback) {
            manager->user_callback(*event_data);
            return true;
        }
        return false;
    }
}