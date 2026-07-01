#ifndef PICO_HID_UPS_H
#define PICO_HID_UPS_H

/**
 * @file pico_hid_ups.h
 * @brief USB HID Power Device (UPS) library for RP2040.
 *
 * Single-header public API. Implements a USB HID UPS using TinyUSB on the
 * Pico SDK. HID is always interface 0 (required by some versions of NUT, e.g., those used in older Synology NAS devices).
 * CDC serial is always available on interfaces 1+2.
 *
 * @note The consuming application must NOT use pico_enable_stdio_usb.
 *       This library owns all USB descriptor callbacks.
 */

#include <cstdint>

/**
 * @brief Battery capacity measurement mode.
 *
 * HID report 22 (CapacityMode), Battery System page (0x85), usage 0x2C.
 * Determines the unit used by capacity-related reports.
 */
enum class CapacityMode : uint8_t {
    mAh     = 0, ///< Milliamp-hours
    mWh     = 1, ///< Milliwatt-hours
    Percent = 2, ///< Percentage (0–100)
    Boolean = 3, ///< Boolean (0 = not present, 1 = present)
};

/**
 * @brief Audible alarm control mode.
 *
 * HID report 20 (AudibleAlarmControl), Power Device page (0x84), usage 0x5A.
 */
enum class AlarmControl : uint8_t {
    Disabled = 1, ///< Alarm disabled
    Enabled  = 2, ///< Alarm enabled
    Muted    = 3, ///< Alarm muted (temporarily silenced)
};

/**
 * @brief UPS present status bitfield.
 *
 * HID report 7 (PresentStatus), 2 bytes / 14 status bits + 2 padding.
 * Nested Logical collection inside the Power Device Application collection.
 *
 * Bits 0–7 (byte 0):
 * - 0: Charging (Battery System 0x85, usage 0x44)
 * - 1: Discharging (0x85, usage 0x45)
 * - 2: ACPresent (0x85, usage 0xD0)
 * - 3: BatteryPresent (0x85, usage 0xD1)
 * - 4: BelowRemainingCapacityLimit (0x85, usage 0x42)
 * - 5: RemainingTimeLimitExpired (0x85, usage 0x43)
 * - 6: NeedReplacement (0x85, usage 0x4B)
 * - 7: VoltageNotRegulated (0x85, usage 0xDB)
 *
 * Bits 8–13 (byte 1):
 * - 8: FullyCharged (0x85, usage 0x46)
 * - 9: FullyDischarged (0x85, usage 0x47)
 * - 10: ShutdownRequested (Power Device 0x84, usage 0x68)
 * - 11: ShutdownImminent (0x84, usage 0x69)
 * - 12: CommunicationLost (0x84, usage 0x73)
 * - 13: Overload (0x84, usage 0x65)
 * - 14–15: padding
 */
struct PresentStatus {
    uint8_t charging : 1;
    uint8_t discharging : 1;
    uint8_t ac_present : 1;
    uint8_t battery_present : 1;
    uint8_t below_remaining_capacity_limit : 1;
    uint8_t remaining_time_limit_expired : 1;
    uint8_t need_replacement : 1;
    uint8_t voltage_not_regulated : 1;

    uint8_t fully_charged : 1;
    uint8_t fully_discharged : 1;
    uint8_t shutdown_requested : 1;
    uint8_t shutdown_imminent : 1;
    uint8_t communication_lost : 1;
    uint8_t overload : 1;
    uint8_t _pad : 2;

    /** @brief Convert to uint16_t for report transmission. */
    operator uint16_t() const { return *reinterpret_cast<const uint16_t*>(this); }
};
static_assert(sizeof(PresentStatus) == 2);

/**
 * @brief Encode a date for HID ManufactureDate report.
 *
 * Uses the USB Battery System date encoding: (year-1980)*512 + month*32 + day.
 *
 * @param year  Calendar year (e.g. 2024).
 * @param month Month 1–12.
 * @param day   Day 1–31.
 * @return Encoded 16-bit date value for report 9.
 */
constexpr uint16_t encode_manufacture_date(uint16_t year, uint8_t month, uint8_t day) {
    return static_cast<uint16_t>((year - 1980) * 512 + month * 32 + day);
}

/**
 * @brief USB HID Power Device (UPS) for RP2040.
 *
 * Provides a complete USB UPS implementation using TinyUSB. The library owns
 * all USB descriptor callbacks and device state callbacks. The application
 * sets values via named setters and calls send() periodically.
 *
 * Typical usage:
 * @code
 * #include "pico_hid_ups.h"
 * #include "tusb.h"
 *
 * int main() {
 *     hid_ups.begin();
 *     tusb_init();
 *     while (true) {
 *         tud_task();
 *         // update hid_ups.status and values...
 *         hid_ups.send();
 *     }
 * }
 * @endcode
 */
class PicoHidUps {
public:
    PicoHidUps();

    /**
     * @brief Initialize with sane defaults. Call before tusb_init().
     *
     * Sets all report values to safe defaults: AC present, battery present,
     * fully charged, 100% capacity, 3600s runtime, 12V, etc.
     */
    void begin();

    /**
     * @brief Send all INPUT reports via the interrupt endpoint.
     *
     * Sends the first INPUT report (RemainingCapacity, report 12). The
     * remaining INPUT reports are chained via tud_hid_report_complete_cb():
     * 12 -> 13 -> 7 -> 11 -> 21 -> 27 -> 28.
     *
     * No-op if USB is not mounted or HID endpoint is not ready.
     */
    void send();

    /**
     * @brief Check if USB device is mounted (configured by host).
     * @return true if the host has configured the device.
     */
    bool mounted() const;

    /**
     * @brief Check if USB device is suspended by host.
     * @return true if the host has suspended the device.
     */
    bool suspended() const;

    /** @brief PresentStatus bitfield — write bits directly. */
    PresentStatus status;

    /**
     * @brief Battery capacity as a percentage.
     * @param percent Battery charge level, clamped to 0–100.
     *
     * Maps to HID report 12 (RemainingCapacity), Battery System page (0x85),
     * usage 0x66. Sent as INPUT (interrupt) and returned via GET_REPORT (FEATURE).
     */
    void set_remaining_capacity(uint8_t percent);

    /**
     * @brief Estimated time until battery is empty.
     * @param seconds Time in seconds (0–65534).
     *
     * Maps to HID report 13 (RunTimeToEmpty), Battery System page (0x85),
     * usage 0x68. Unit: seconds. Sent as INPUT and FEATURE.
     */
    void set_runtime_to_empty(uint16_t seconds);

    /**
     * @brief Current battery/output voltage.
     * @param centivolts Voltage in units of 10mV (e.g. 1200 = 12.00V).
     *
     * Maps to HID report 11 (Voltage), Power Device page (0x84),
     * usage 0x30. Unit: centivolts (10^5 V). Sent as INPUT and FEATURE.
     */
    void set_voltage(uint16_t centivolts);

    /**
     * @brief Average time until battery is empty.
     * @param seconds Time in seconds (0–65534).
     *
     * Maps to HID report 28 (AverageTimeToEmpty), Battery System page (0x85),
     * usage 0x69. Unit: seconds. Sent as INPUT and FEATURE.
     */
    void set_average_time_to_empty(uint16_t seconds);

    /**
     * @brief Average time until battery is fully charged.
     * @param seconds Time in seconds (0–65534).
     *
     * Maps to HID report 26 (AverageTimeToFull), Battery System page (0x85),
     * usage 0x6A. Unit: seconds. FEATURE-only.
     */
    void set_average_time_to_full(uint16_t seconds);

    /**
     * @brief Instantaneous battery current.
     * @param milliamps Current in milliamps. Positive = charging, negative = discharging.
     *
     * Maps to HID report 21 (Current), Power Device page (0x84),
     * usage 0x31. Unit: milliamps (10^-3 A). Sent as INPUT and FEATURE.
     */
    void set_current(int16_t milliamps);

    /**
     * @brief One-minute rolling average battery current.
     * @param milliamps Current in milliamps. Positive = charging, negative = discharging.
     *
     * Maps to HID report 27 (AverageCurrent), Battery System page (0x85),
     * usage 0x62. Unit: milliamps (10^-3 A). Sent as INPUT and FEATURE.
     */
    void set_average_current(int16_t milliamps);

    /**
     * @brief Nominal/configured voltage.
     * @param centivolts Voltage in units of 10mV.
     *
     * Maps to HID report 10 (ConfigVoltage), Power Device page (0x84),
     * usage 0x40. Unit: centivolts. FEATURE-only, read-only.
     */
    void set_config_voltage(uint16_t centivolts);

    /**
     * @brief Minimum remaining time before low-battery warning.
     * @param seconds Time in seconds (120–1380).
     *
     * Maps to HID report 8 (RemainingTimeLimit), Battery System page (0x85),
     * usage 0x2A. Unit: seconds. FEATURE-only, writable by host.
     */
    void set_remaining_time_limit(uint16_t seconds);

    /**
     * @brief Battery manufacture date.
     * @param year  Calendar year (e.g. 2024).
     * @param month Month 1–12.
     * @param day   Day 1–31.
     *
     * Maps to HID report 9 (ManufacturerDate), Battery System page (0x85),
     * usage 0x85. Encoded as (year-1980)*512 + month*32 + day. FEATURE-only.
     */
    void set_manufacture_date(uint16_t year, uint8_t month, uint8_t day);

    /**
     * @brief Battery design capacity.
     * @param percent Design capacity, clamped to 0–100.
     *
     * Maps to HID report 23 (DesignCapacity), Battery System page (0x85),
     * usage 0x83. FEATURE-only.
     */
    void set_design_capacity(uint8_t percent);

    /**
     * @brief Last full charge capacity.
     * @param percent Full charge capacity, clamped to 0–100.
     *
     * Maps to HID report 14 (FullChargeCapacity), Battery System page (0x85),
     * usage 0x67. FEATURE-only.
     */
    void set_full_charge_capacity(uint8_t percent);

    /**
     * @brief Warning capacity threshold.
     * @param percent Capacity below which a warning is issued, clamped to 0–100.
     *
     * Maps to HID report 15 (WarningCapacityLimit), Battery System page (0x85),
     * usage 0x8C. FEATURE-only, writable by host.
     */
    void set_warning_capacity_limit(uint8_t percent);

    /**
     * @brief Low-battery capacity threshold.
     * @param percent Capacity below which battery is critically low, clamped to 0–100.
     *
     * Maps to HID report 17 (RemainingCapacityLimit), Battery System page (0x85),
     * usage 0x29. FEATURE-only, writable by host.
     */
    void set_remaining_capacity_limit(uint8_t percent);

    /**
     * @brief Capacity reporting granularity below WarningCapacityLimit.
     * @param percent Granularity step size, clamped to 0–100.
     *
     * Maps to HID report 16 (CapacityGranularity1), Battery System page (0x85),
     * usage 0x8D. FEATURE-only, writable by host (nonvolatile).
     */
    void set_capacity_granularity1(uint8_t percent);

    /**
     * @brief Capacity reporting granularity above WarningCapacityLimit.
     * @param percent Granularity step size, clamped to 0–100.
     *
     * Maps to HID report 24 (CapacityGranularity2), Battery System page (0x85),
     * usage 0x8E. FEATURE-only.
     */
    void set_capacity_granularity2(uint8_t percent);

    /**
     * @brief Audible alarm control setting.
     * @param mode AlarmControl::Disabled (1), Enabled (2), or Muted (3).
     *
     * Maps to HID report 20 (AudibleAlarmControl), Power Device page (0x84),
     * usage 0x5A. FEATURE-only, writable by host.
     */
    void set_audible_alarm_control(AlarmControl mode);

    /**
     * @brief Whether the battery is rechargeable.
     * @param v true if rechargeable.
     *
     * Maps to HID report 6 (Rechargeable), Battery System page (0x85),
     * usage 0x8B. FEATURE-only.
     */
    void set_rechargeable(bool v);

    /**
     * @brief Capacity measurement mode.
     * @param mode One of CapacityMode values.
     *
     * Maps to HID report 22 (CapacityMode), Battery System page (0x85),
     * usage 0x2C. FEATURE-only.
     */
    void set_capacity_mode(CapacityMode mode);

    /**
     * @brief Host-requested shutdown delay.
     * @return Delay in seconds. -1 = no shutdown requested. 0 = immediate.
     *
     * Written by host via SET_REPORT on report 18 (DelayBeforeShutdown),
     * Power Device page (0x84), usage 0x57.
     */
    int16_t delay_before_shutdown() const;

    /**
     * @brief Host-requested reboot delay.
     * @return Delay in seconds. -1 = no reboot requested. 0 = immediate.
     *
     * Written by host via SET_REPORT on report 19 (DelayBeforeReboot),
     * Power Device page (0x84), usage 0x55.
     */
    int16_t delay_before_reboot() const;

    /**
     * @brief Current audible alarm control setting.
     * @return AlarmControl::Disabled (1), Enabled (2), or Muted (3).
     *
     * Reads the value of HID report 20 (AudibleAlarmControl).
     */
    AlarmControl audible_alarm_control() const;

    /**
     * @brief Register a callback for host SET_REPORT commands.
     *
     * Called from within tud_task() context whenever the host writes to a
     * writable report (7, 8, 15, 16, 17, 18, 19, 20). The callback receives
     * the report ID that was written.
     *
     * @param cb Function pointer, or nullptr to clear.
     */
    using set_report_cb_t = void (*)(uint8_t report_id);
    void on_set_report(set_report_cb_t cb);

    /// @cond INTERNAL
    // Internal state — accessed by TinyUSB callbacks in pico_hid_ups.cpp.
    // Use the public API instead of touching these directly.

    bool mounted_ = false;
    bool suspended_ = false;

    uint8_t remaining_capacity_ = 100;
    uint16_t runtime_to_empty_ = 3600;
    uint16_t voltage_ = 1200;
    uint16_t average_time_to_empty_ = 0;
    uint16_t average_time_to_full_ = 0;
    int16_t current_ = 0;
    int16_t average_current_ = 0;
    uint16_t config_voltage_ = 1200;
    uint16_t remaining_time_limit_ = 120;
    uint16_t manufacture_date_ = 0;
    uint8_t design_capacity_ = 100;
    uint8_t full_charge_capacity_ = 100;
    uint8_t warning_capacity_limit_ = 10;
    uint8_t remaining_capacity_limit_ = 5;
    uint8_t capacity_granularity1_ = 1;
    uint8_t capacity_granularity2_ = 1;
    AlarmControl audible_alarm_control_ = AlarmControl::Enabled;
    uint8_t rechargeable_ = 1;
    CapacityMode capacity_mode_ = CapacityMode::Percent;

    int16_t delay_before_shutdown_ = -1;
    int16_t delay_before_reboot_ = -1;

    uint8_t iproduct_ = 2;
    uint8_t iserial_ = 3;
    uint8_t imanufacturer_ = 1;
    uint8_t idevicechemistry_ = 4;
    uint8_t ioeminformation_ = 5;

    set_report_cb_t set_report_cb_ = nullptr;

    void send_input_report(uint8_t report_id);
    static uint8_t clamp100(uint8_t v) { return v > 100 ? 100 : v; }
    /// @endcond
};

/** @brief Global UPS instance. */
extern PicoHidUps hid_ups;

#endif
