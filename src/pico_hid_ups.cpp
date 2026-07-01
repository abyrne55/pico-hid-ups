#include "pico_hid_ups.h"
#include "tusb.h"

PicoHidUps hid_ups;

// Report IDs (must match usb_descriptors.cpp)
enum {
    RID_IPRODUCT           = 1,
    RID_ISERIAL            = 2,
    RID_IMANUFACTURER      = 3,
    RID_RECHARGEABLE       = 6,
    RID_PRESENTSTATUS      = 7,
    RID_REMAINTIMELIMIT    = 8,
    RID_MANUFACTUREDATE    = 9,
    RID_CONFIGVOLTAGE      = 10,
    RID_VOLTAGE            = 11,
    RID_REMAININGCAPACITY  = 12,
    RID_RUNTIMETOEMPTY     = 13,
    RID_FULLCHRGECAPACITY  = 14,
    RID_WARNCAPACITYLIMIT  = 15,
    RID_CPCTYGRANULARITY1  = 16,
    RID_REMNCAPACITYLIMIT  = 17,
    RID_DELAYBE4SHUTDOWN   = 18,
    RID_DELAYBE4REBOOT     = 19,
    RID_AUDIBLEALARMCTRL   = 20,
    RID_CURRENT            = 21,
    RID_CAPACITYMODE       = 22,
    RID_DESIGNCAPACITY     = 23,
    RID_CPCTYGRANULARITY2  = 24,
    RID_AVERAGETIME2FULL   = 26,
    RID_AVERAGECURRENT     = 27,
    RID_AVERAGETIME2EMPTY  = 28,
    RID_IDEVICECHEMISTRY   = 31,
    RID_IOEMINFORMATION    = 32,
};

// INPUT report chain order: 12 -> 13 -> 7 -> 11 -> 21 -> 27 -> 28
static constexpr uint8_t input_chain[] = {
    RID_REMAININGCAPACITY,
    RID_RUNTIMETOEMPTY,
    RID_PRESENTSTATUS,
    RID_VOLTAGE,
    RID_CURRENT,
    RID_AVERAGECURRENT,
    RID_AVERAGETIME2EMPTY,
};
static constexpr size_t INPUT_CHAIN_LEN = sizeof(input_chain) / sizeof(input_chain[0]);

PicoHidUps::PicoHidUps() {}

void PicoHidUps::begin() {
    status = {};
    status.ac_present = 1;
    status.battery_present = 1;
    status.fully_charged = 1;

    remaining_capacity_ = 100;
    runtime_to_empty_ = 3600;
    voltage_ = 1200;
    config_voltage_ = 1200;
    rechargeable_ = 1;
    full_charge_capacity_ = 100;
    design_capacity_ = 100;
    capacity_mode_ = CapacityMode::Percent;
    warning_capacity_limit_ = 10;
    remaining_capacity_limit_ = 5;
    capacity_granularity1_ = 1;
    capacity_granularity2_ = 1;
    audible_alarm_control_ = AlarmControl::Enabled;
    remaining_time_limit_ = 120;
    delay_before_shutdown_ = -1;
    delay_before_reboot_ = -1;
    manufacture_date_ = 0;
    average_time_to_full_ = 0;
    average_time_to_empty_ = 0;
    current_ = 0;
    average_current_ = 0;
}

void PicoHidUps::send() {
    if (!mounted_ || !tud_hid_ready()) return;
    send_input_report(input_chain[0]);
}

bool PicoHidUps::mounted() const { return mounted_; }
bool PicoHidUps::suspended() const { return suspended_; }

void PicoHidUps::set_remaining_capacity(uint8_t percent) {
    remaining_capacity_ = clamp100(percent);
}

void PicoHidUps::set_runtime_to_empty(uint16_t seconds) {
    runtime_to_empty_ = seconds;
}

void PicoHidUps::set_voltage(uint16_t centivolts) {
    voltage_ = centivolts;
}

void PicoHidUps::set_average_time_to_empty(uint16_t seconds) {
    average_time_to_empty_ = seconds;
}

void PicoHidUps::set_average_time_to_full(uint16_t seconds) {
    average_time_to_full_ = seconds;
}

void PicoHidUps::set_current(int16_t milliamps) {
    current_ = milliamps;
}

void PicoHidUps::set_average_current(int16_t milliamps) {
    average_current_ = milliamps;
}

void PicoHidUps::set_config_voltage(uint16_t centivolts) {
    config_voltage_ = centivolts;
}

void PicoHidUps::set_remaining_time_limit(uint16_t seconds) {
    remaining_time_limit_ = seconds;
}

void PicoHidUps::set_manufacture_date(uint16_t year, uint8_t month, uint8_t day) {
    manufacture_date_ = encode_manufacture_date(year, month, day);
}

void PicoHidUps::set_design_capacity(uint8_t percent) {
    design_capacity_ = clamp100(percent);
}

void PicoHidUps::set_full_charge_capacity(uint8_t percent) {
    full_charge_capacity_ = clamp100(percent);
}

void PicoHidUps::set_warning_capacity_limit(uint8_t percent) {
    warning_capacity_limit_ = clamp100(percent);
}

void PicoHidUps::set_remaining_capacity_limit(uint8_t percent) {
    remaining_capacity_limit_ = clamp100(percent);
}

void PicoHidUps::set_capacity_granularity1(uint8_t percent) {
    capacity_granularity1_ = clamp100(percent);
}

void PicoHidUps::set_capacity_granularity2(uint8_t percent) {
    capacity_granularity2_ = clamp100(percent);
}

void PicoHidUps::set_audible_alarm_control(AlarmControl mode) {
    audible_alarm_control_ = mode;
}

void PicoHidUps::set_rechargeable(bool v) {
    rechargeable_ = v ? 1 : 0;
}

void PicoHidUps::set_capacity_mode(CapacityMode mode) {
    capacity_mode_ = mode;
}

int16_t PicoHidUps::delay_before_shutdown() const {
    return delay_before_shutdown_;
}

int16_t PicoHidUps::delay_before_reboot() const {
    return delay_before_reboot_;
}

void PicoHidUps::on_set_report(set_report_cb_t cb) {
    set_report_cb_ = cb;
}

void PicoHidUps::send_input_report(uint8_t report_id) {
    switch (report_id) {
        case RID_REMAININGCAPACITY:
            tud_hid_report(RID_REMAININGCAPACITY, &remaining_capacity_, 1);
            break;
        case RID_RUNTIMETOEMPTY:
            tud_hid_report(RID_RUNTIMETOEMPTY, &runtime_to_empty_, 2);
            break;
        case RID_PRESENTSTATUS: {
            uint16_t val = status;
            tud_hid_report(RID_PRESENTSTATUS, &val, 2);
            break;
        }
        case RID_VOLTAGE:
            tud_hid_report(RID_VOLTAGE, &voltage_, 2);
            break;
        case RID_CURRENT:
            tud_hid_report(RID_CURRENT, &current_, 2);
            break;
        case RID_AVERAGECURRENT:
            tud_hid_report(RID_AVERAGECURRENT, &average_current_, 2);
            break;
        case RID_AVERAGETIME2EMPTY:
            tud_hid_report(RID_AVERAGETIME2EMPTY, &average_time_to_empty_, 2);
            break;
        default:
            break;
    }
}

// --- TinyUSB callbacks ---

extern "C" {

void tud_mount_cb(void) {
    hid_ups.mounted_ = true;
    hid_ups.suspended_ = false;
}

void tud_umount_cb(void) {
    hid_ups.mounted_ = false;
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    hid_ups.suspended_ = true;
}

void tud_resume_cb(void) {
    hid_ups.suspended_ = false;
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
    (void)instance;
    (void)len;

    uint8_t sent_id = report[0];

    for (size_t i = 0; i < INPUT_CHAIN_LEN - 1; i++) {
        if (input_chain[i] == sent_id) {
            hid_ups.send_input_report(input_chain[i + 1]);
            return;
        }
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_type;
    (void)reqlen;

    switch (report_id) {
        case RID_IPRODUCT:
            buffer[0] = hid_ups.iproduct_;
            return 1;
        case RID_ISERIAL:
            buffer[0] = hid_ups.iserial_;
            return 1;
        case RID_IMANUFACTURER:
            buffer[0] = hid_ups.imanufacturer_;
            return 1;
        case RID_RECHARGEABLE:
            buffer[0] = hid_ups.rechargeable_;
            return 1;
        case RID_PRESENTSTATUS: {
            uint16_t val = hid_ups.status;
            memcpy(buffer, &val, 2);
            return 2;
        }
        case RID_REMAINTIMELIMIT:
            memcpy(buffer, &hid_ups.remaining_time_limit_, 2);
            return 2;
        case RID_MANUFACTUREDATE:
            memcpy(buffer, &hid_ups.manufacture_date_, 2);
            return 2;
        case RID_CONFIGVOLTAGE:
            memcpy(buffer, &hid_ups.config_voltage_, 2);
            return 2;
        case RID_VOLTAGE:
            memcpy(buffer, &hid_ups.voltage_, 2);
            return 2;
        case RID_REMAININGCAPACITY:
            buffer[0] = hid_ups.remaining_capacity_;
            return 1;
        case RID_RUNTIMETOEMPTY:
            memcpy(buffer, &hid_ups.runtime_to_empty_, 2);
            return 2;
        case RID_FULLCHRGECAPACITY:
            buffer[0] = hid_ups.full_charge_capacity_;
            return 1;
        case RID_WARNCAPACITYLIMIT:
            buffer[0] = hid_ups.warning_capacity_limit_;
            return 1;
        case RID_CPCTYGRANULARITY1:
            buffer[0] = hid_ups.capacity_granularity1_;
            return 1;
        case RID_REMNCAPACITYLIMIT:
            buffer[0] = hid_ups.remaining_capacity_limit_;
            return 1;
        case RID_DELAYBE4SHUTDOWN:
            memcpy(buffer, &hid_ups.delay_before_shutdown_, 2);
            return 2;
        case RID_DELAYBE4REBOOT:
            memcpy(buffer, &hid_ups.delay_before_reboot_, 2);
            return 2;
        case RID_AUDIBLEALARMCTRL:
            buffer[0] = static_cast<uint8_t>(hid_ups.audible_alarm_control_);
            return 1;
        case RID_CURRENT:
            memcpy(buffer, &hid_ups.current_, 2);
            return 2;
        case RID_CAPACITYMODE:
            buffer[0] = static_cast<uint8_t>(hid_ups.capacity_mode_);
            return 1;
        case RID_DESIGNCAPACITY:
            buffer[0] = hid_ups.design_capacity_;
            return 1;
        case RID_CPCTYGRANULARITY2:
            buffer[0] = hid_ups.capacity_granularity2_;
            return 1;
        case RID_AVERAGETIME2FULL:
            memcpy(buffer, &hid_ups.average_time_to_full_, 2);
            return 2;
        case RID_AVERAGECURRENT:
            memcpy(buffer, &hid_ups.average_current_, 2);
            return 2;
        case RID_AVERAGETIME2EMPTY:
            memcpy(buffer, &hid_ups.average_time_to_empty_, 2);
            return 2;
        case RID_IDEVICECHEMISTRY:
            buffer[0] = hid_ups.idevicechemistry_;
            return 1;
        case RID_IOEMINFORMATION:
            buffer[0] = hid_ups.ioeminformation_;
            return 1;
        default:
            return 0;
    }
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_type;

    switch (report_id) {
        case RID_PRESENTSTATUS:
            if (bufsize >= 2) {
                uint16_t val;
                memcpy(&val, buffer, 2);
                // Only ShutdownRequested (bit 10) and RemainingTimeLimitExpired (bit 5) are writable
                hid_ups.status.shutdown_requested = (val >> 10) & 1;
                hid_ups.status.remaining_time_limit_expired = (val >> 5) & 1;
            }
            break;
        case RID_REMAINTIMELIMIT:
            if (bufsize >= 2)
                memcpy(&hid_ups.remaining_time_limit_, buffer, 2);
            break;
        case RID_WARNCAPACITYLIMIT:
            if (bufsize >= 1)
                hid_ups.warning_capacity_limit_ = buffer[0];
            break;
        case RID_CPCTYGRANULARITY1:
            if (bufsize >= 1)
                hid_ups.capacity_granularity1_ = buffer[0];
            break;
        case RID_REMNCAPACITYLIMIT:
            if (bufsize >= 1)
                hid_ups.remaining_capacity_limit_ = buffer[0];
            break;
        case RID_DELAYBE4SHUTDOWN:
            if (bufsize >= 2)
                memcpy(&hid_ups.delay_before_shutdown_, buffer, 2);
            break;
        case RID_DELAYBE4REBOOT:
            if (bufsize >= 2)
                memcpy(&hid_ups.delay_before_reboot_, buffer, 2);
            break;
        case RID_AUDIBLEALARMCTRL:
            if (bufsize >= 1)
                hid_ups.audible_alarm_control_ = static_cast<AlarmControl>(buffer[0]);
            break;
        default:
            return;
    }

    if (hid_ups.set_report_cb_)
        hid_ups.set_report_cb_(report_id);
}

} // extern "C"
