// @file GpioManager.cpp

#include "../inc/gpio.hpp"
#include <cstring>

namespace ESP32_GPIO {

    GpioManager& GpioManager::getInstance() {
        static GpioManager instance;
        return instance;
    }

    GpioManager::GpioManager() : _initialized(false) {
        std::memset(_configs, 0, sizeof(_configs));
    }

    GpioManager::~GpioManager() {
        if (_initialized) {
            gpio_uninstall_isr_service();
        }
    }

    bool GpioManager::init() {
        if (!_initialized) {
            if (gpio_install_isr_service(0) == ESP_OK) {
                _initialized = true;
            }
        }
        return _initialized;
    }

    bool GpioManager::configurePin(int pin,
                                PinMode mode,
                                bool pull_up,
                                bool pull_down,
                                InterruptTrigger intr,
                                GpioCallback callback,
                                bool open_drain,
                                DriveStrength drive){
        if (!_initialized) this->init();

        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.mode = static_cast<gpio_mode_t>(mode);
        io_conf.pull_up_en = pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = pull_down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = static_cast<gpio_int_type_t>(intr);
        io_conf.mode = open_drain ? GPIO_MODE_INPUT_OUTPUT_OD : io_conf.mode;

        if (gpio_config(&io_conf) != ESP_OK) {
            ESP_LOGE("GPIO", "Failed to config pin %d", pin);
            return false;
        }

        gpio_set_drive_capability(static_cast<gpio_num_t>(pin), static_cast<gpio_drive_cap_t>(drive));

        if (intr != InterruptTrigger::NONE && callback) {
            _configs[pin].cb = callback;
            /*
            gpio_isr_t isrHandler = [](void* arg) {
            GpioManager::isrHandler(arg);
            };
            */
            gpio_isr_handler_add(static_cast<gpio_num_t>(pin), GpioManager::isrHandler, reinterpret_cast<void*>(static_cast<intptr_t>(pin)));
        }
        return true;
    }

    void GpioManager::setLevel(int pin, uint32_t level) {
        gpio_set_level(static_cast<gpio_num_t>(pin), level);
    }

    int GpioManager::getLevel(int pin) const {
        return gpio_get_level(static_cast<gpio_num_t>(pin));
    }

    void GpioManager::toggle(int pin) {
        int lvl = getLevel(pin);
        setLevel(pin, !lvl);
    }

    void GpioManager::resetPin(int pin) {
        gpio_reset_pin(static_cast<gpio_num_t>(pin));
    }

    void GpioManager::setDriveStrength(int pin, DriveStrength strength) {
        gpio_set_drive_capability(static_cast<gpio_num_t>(pin), static_cast<gpio_drive_cap_t>(strength));
    }

    /*
    void GpioManager::enableGlitchFilter(int pin) {
        gpio_glitch_filter_config_t filter_cfg = {
            .gpio_num = static_cast<gpio_num_t>(pin),
            .clk_src = GLITCH_FILTER_CLK_SRC_DEFAULT
        };
        gpio_new_glitch_filter(&filter_cfg, nullptr);
    }

    void GpioManager::disableGlitchFilter(int pin) {
        gpio_del_glitch_filter(static_cast<gpio_num_t>(pin));
    }
    */

    void GpioManager::isrHandler(void* arg) {
        int pin = static_cast<int>(reinterpret_cast<intptr_t>(arg));
        int level = gpio_get_level(static_cast<gpio_num_t>(pin));
        auto& cfg = getInstance()._configs[pin];
        if (cfg.cb) {
            cfg.cb(pin, level);
        }
    }

    esp_err_t GpioManager::set_isr_handler(void (*fn)(void*), void *arg, int intr_alloc_flags, gpio_isr_handle_t *handle) {
        if (!_initialized) {
            ESP_LOGE("GPIO", "GpioManager not initialized");
            return ESP_ERR_INVALID_STATE;
        }
        return gpio_isr_handler_add(static_cast<gpio_num_t>(reinterpret_cast<intptr_t>(arg)), fn, arg);
    }

    extern "C" {

    int gpio_mgr_init() {
        return ESP32_GPIO::GpioManager::getInstance().init() ? 1 : 0;
    }

    int gpio_mgr_configure(int pin,
        int mode,
        bool pull_up,
        bool pull_down,
        int intr_type
    ) {
        return ESP32_GPIO::GpioManager::getInstance().configurePin(
            pin,
            static_cast<ESP32_GPIO::PinMode>(mode),
            pull_up,
            pull_down,
            static_cast<ESP32_GPIO::InterruptTrigger>(intr_type),
            nullptr
            ) ? 1 : 0;
    }

    void gpio_mgr_set_level(int pin, int level) {
        ESP32_GPIO::GpioManager::getInstance().setLevel(pin, level);
    }

    int gpio_mgr_get_level(int pin) {
        return ESP32_GPIO::GpioManager::getInstance().getLevel(pin);
    }

    void gpio_mgr_toggle(int pin) {
        ESP32_GPIO::GpioManager::getInstance().toggle(pin);
    }

    void gpio_mgr_reset(int pin) {
        ESP32_GPIO::GpioManager::getInstance().resetPin(pin);
    }

    void gpio_mgr_set_drive_strength(int pin, int strength) {
        ESP32_GPIO::GpioManager::getInstance().setDriveStrength(pin, static_cast<ESP32_GPIO::DriveStrength>(strength));
    }

    void gpio_mgr_enable_glitch_filter(int pin) {
        ESP32_GPIO::GpioManager::getInstance().enableGlitchFilter(pin);
    }

    void gpio_mgr_disable_glitch_filter(int pin) {
        ESP32_GPIO::GpioManager::getInstance().disableGlitchFilter(pin);
    }

    } // extern "C"

} // namespace ESP32_GPIO
// End of GpioManager.cpp