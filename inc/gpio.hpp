// @file GpioManager.hpp
// @brief GPIO manager for ESP32 using C++ OOP approach
#pragma once

#include <cstdint>
#include <functional>
#include "driver/gpio.h"
#include "esp_log.h"

#define GPIO_NUM_MAX 40 // Maximum GPIO number for ESP32

namespace ESP32_GPIO {

    /**
     * @brief GPIO pin direction and pull-up/pull-down options
     */
    enum class PinMode {
        DISABLED       = GPIO_MODE_DISABLE,
        INPUT          = GPIO_MODE_INPUT,
        OUTPUT         = GPIO_MODE_OUTPUT,
        INPUT_PULLUP   = GPIO_MODE_INPUT,
        INPUT_PULLDOWN = GPIO_MODE_INPUT
    };

    /**
     * @brief GPIO interrupt trigger types
     */
    enum class InterruptTrigger {
        NONE    = GPIO_INTR_DISABLE,
        RISING  = GPIO_INTR_POSEDGE,
        FALLING = GPIO_INTR_NEGEDGE,
        BOTH    = GPIO_INTR_ANYEDGE
    };

    /**
     * @brief GPIO drive strength levels
     */
    enum class DriveStrength {
        DEFAULT = GPIO_DRIVE_CAP_DEFAULT,
        _0      = GPIO_DRIVE_CAP_0,
        _1      = GPIO_DRIVE_CAP_1,
        _2      = GPIO_DRIVE_CAP_2,
        _3      = GPIO_DRIVE_CAP_3
    };

    /**
     * @brief Callback signature for GPIO interrupts
     * @param gpio_num The GPIO pin number generating the interrupt
     * @param level    The logic level of the pin at interrupt
     */
    using GpioCallback = std::function<void(int gpio_num, int level)>;

    /**
     * @brief Singleton class to manage GPIO configuration and interrupts
     */
    class GpioManager {
    public:
        /**
         * @brief Access the single instance
         */
        static GpioManager& getInstance();

        /**
         * @brief Initialize ISR service
         * @return true if successful
         */
        bool init();

        /**
         * @brief Configure a GPIO pin
         * @param pin         GPIO number
         * @param mode        PinMode
         * @param pull_up     Enable pull-up (only valid if mode is input)
         * @param pull_down   Enable pull-down (only valid if mode is input)
         * @param intr        InterruptTrigger
         * @param callback    Callback invoked on interrupt (optional)
         * @param open_drain  Enable open-drain mode
         * @param drive       Drive strength
         */
        bool configurePin(int pin,
            PinMode mode,
            bool pull_up = false,
            bool pull_down = false,
            InterruptTrigger intr = InterruptTrigger::NONE,
            GpioCallback callback = nullptr,
            bool open_drain = false,
            DriveStrength drive = DriveStrength::DEFAULT
        );

        /**
         * @brief Write output level
         * @param pin   GPIO number
         * @param level 0 or 1
         */
        void setLevel(int pin, uint32_t level);

        /**
         * @brief Read input level
         * @param pin GPIO number
         * @return 0 or 1
         */
        int getLevel(int pin) const;

        /**
         * @brief Toggle output state
         * @param pin GPIO number
         */
        void toggle(int pin);

        /**
         * @brief Reset GPIO pin to default state
         * @param pin GPIO number
         */
        void resetPin(int pin);

        /**
         * @brief Set drive strength for a GPIO pin
         * @param pin     GPIO number
         * @param strength DriveStrength level
         */
        void setDriveStrength(int pin, DriveStrength strength);

        /**
         * @brief Enable glitch filter for a GPIO pin
         * @param pin GPIO number
         */
        void enableGlitchFilter(int pin);

        /**
         * @brief Disable glitch filter for a GPIO pin
         * @param pin GPIO number
         */
        void disableGlitchFilter(int pin);

        /**
         * @brief Register an ISR handler for a GPIO pin
         * @param fn            Callback function to invoke on interrupt
         * @param arg           Argument to pass to the callback
         * @param intr_alloc_flags Flags for interrupt allocation
         * @param handle        Output handle for the ISR
         * @return ESP_OK on success, error code otherwise
         */
        esp_err_t set_isr_handler(void (*fn)(void*), void *arg, int intr_alloc_flags, gpio_isr_handle_t *handle);

    private:
        GpioManager();
        ~GpioManager();
        GpioManager(const GpioManager&) = delete;
        GpioManager& operator=(const GpioManager&) = delete;

        static void isrHandler(void* arg);

        struct PinConfig {
            GpioCallback cb;
        };

        /**
         * Array to store callbacks per pin number
         * GPIO_NUM_MAX is at least 40
         */
        PinConfig _configs[GPIO_NUM_MAX];
        bool _initialized;
    };

    // C wrappers for C compatibility or interfacing with C code
    // These functions provide a C interface to the GpioManager class
    extern "C" {
        int gpio_mgr_init();
        int gpio_mgr_configure(
            int pin,
            int mode,
            bool pull_up,
            bool pull_down,
            int intr_type
        );
        void gpio_mgr_set_level(
            int pin, 
            int level
        );
        int  gpio_mgr_get_level(int pin);
        void gpio_mgr_toggle(int pin);
        void gpio_mgr_reset(int pin);
        void gpio_mgr_set_drive_strength(
            int pin, 
            int strength
        );
        void gpio_mgr_enable_glitch_filter(int pin);
        void gpio_mgr_disable_glitch_filter(int pin);
        bool gpio_mgr_isr_register(
            void (*fn)(void*), 
            void *arg, 
            int intr_alloc_flags, 
            gpio_isr_handle_t *handle
        );
    }

} // namespace ESP32_GPIO
