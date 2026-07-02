#include "tusb.h"
#include "pico/unique_id.h"
#include <cstring>

// Report IDs
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

// String descriptor indices
enum {
    STRID_LANGID           = 0,
    STRID_MANUFACTURER     = 1,
    STRID_PRODUCT          = 2,
    STRID_SERIAL           = 3,
    STRID_DEVICECHEMISTRY  = 4,
    STRID_OEMINFORMATION   = 5,
};

// HID report descriptor — 27 reports
// Based on HIDPowerDevice with corrections:
//   - Report 8 (RemainingTimeLimit): FEATURE-only (removed INPUT)
//   - Report 20 (AudibleAlarmControl): FEATURE-only (removed INPUT)
// Added reports beyond HIDPowerDevice:
//   - Report 21 (Current): page 0x84, usage 0x31, 16-bit signed, INPUT+FEATURE
//   - Report 27 (AverageCurrent): page 0x85, usage 0x62, 16-bit signed, INPUT+FEATURE
static uint8_t const desc_hid_report[] = {
    0x05, 0x84,       // USAGE_PAGE (Power Device)
    0x09, 0x04,       // USAGE (UPS)
    0xA1, 0x01,       // COLLECTION (Application)
    0x09, 0x24,       //   USAGE (Sink)
    0xA1, 0x02,       //   COLLECTION (Logical)

    // -- Initial state: 8-bit, count 1, 0-255 --
    0x75, 0x08,       //     REPORT_SIZE (8)
    0x95, 0x01,       //     REPORT_COUNT (1)
    0x15, 0x00,       //     LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00, //     LOGICAL_MAXIMUM (255)

    // -- String index reports (FEATURE-only, page 0x84) --
    // Report 1: iProduct
    0x85, RID_IPRODUCT,
    0x09, 0xFE,       //     USAGE (iProduct)
    0x79, STRID_PRODUCT,
    0xB1, 0x23,       //     FEATURE (Const, Var, Abs, NonVol)

    // Report 2: iSerialNumber
    0x85, RID_ISERIAL,
    0x09, 0xFF,       //     USAGE (iSerialNumber)
    0x79, STRID_SERIAL,
    0xB1, 0x23,

    // Report 3: iManufacturer
    0x85, RID_IMANUFACTURER,
    0x09, 0xFD,       //     USAGE (iManufacturer)
    0x79, STRID_MANUFACTURER,
    0xB1, 0x23,

    // -- Switch to Battery System page (0x85) --
    0x05, 0x85,

    // Report 6: Rechargeable
    0x85, RID_RECHARGEABLE,
    0x09, 0x8B,       //     USAGE (Rechargeable)
    0xB1, 0x23,

    // Report 31: iDeviceChemistry
    0x85, RID_IDEVICECHEMISTRY,
    0x09, 0x89,       //     USAGE (iDeviceChemistry)
    0x79, STRID_DEVICECHEMISTRY,
    0xB1, 0x23,

    // Report 32: iOEMInformation
    0x85, RID_IOEMINFORMATION,
    0x09, 0x8F,       //     USAGE (iOEMInformation)
    0x79, STRID_OEMINFORMATION,
    0xB1, 0x23,

    // Report 22: CapacityMode
    0x85, RID_CAPACITYMODE,
    0x09, 0x2C,       //     USAGE (CapacityMode)
    0xB1, 0x23,

    // Report 16: CapacityGranularity1 (max 100)
    0x85, RID_CPCTYGRANULARITY1,
    0x09, 0x8D,       //     USAGE (CapacityGranularity1)
    0x26, 0x64, 0x00, //     LOGICAL_MAXIMUM (100)
    0xB1, 0x22,       //     FEATURE (Data, Var, Abs, NonVol)

    // Report 24: CapacityGranularity2
    0x85, RID_CPCTYGRANULARITY2,
    0x09, 0x8E,       //     USAGE (CapacityGranularity2)
    0xB1, 0x23,

    // Report 14: FullChargeCapacity
    0x85, RID_FULLCHRGECAPACITY,
    0x09, 0x67,       //     USAGE (FullChargeCapacity)
    0xB1, 0x83,       //     FEATURE (Const, Var, Abs, Vol)

    // Report 23: DesignCapacity
    0x85, RID_DESIGNCAPACITY,
    0x09, 0x83,       //     USAGE (DesignCapacity)
    0xB1, 0x83,

    // Report 12: RemainingCapacity — INPUT + FEATURE
    0x85, RID_REMAININGCAPACITY,
    0x09, 0x66,       //     USAGE (RemainingCapacity)
    0x81, 0xA3,       //     INPUT (Const, Var, Abs)
    0x09, 0x66,
    0xB1, 0xA3,       //     FEATURE (Const, Var, Abs, Vol)

    // Report 15: WarningCapacityLimit
    0x85, RID_WARNCAPACITYLIMIT,
    0x09, 0x8C,       //     USAGE (WarningCapacityLimit)
    0xB1, 0xA2,       //     FEATURE (Data, Var, Abs, Vol)

    // Report 17: RemainingCapacityLimit
    0x85, RID_REMNCAPACITYLIMIT,
    0x09, 0x29,       //     USAGE (RemainingCapacityLimit)
    0xB1, 0xA2,

    // -- 16-bit reports --
    // Report 9: ManufactureDate
    0x85, RID_MANUFACTUREDATE,
    0x09, 0x85,       //     USAGE (ManufacturerDate)
    0x75, 0x10,       //     REPORT_SIZE (16)
    0x27, 0xFF, 0xFF, 0x00, 0x00, // LOGICAL_MAXIMUM (65535)
    0xB1, 0xA3,       //     FEATURE (Const, Var, Abs, Vol)

    // Report 26: AverageTimeToFull (unit: seconds)
    0x85, RID_AVERAGETIME2FULL,
    0x09, 0x6A,       //     USAGE (AverageTimeToFull)
    0x66, 0x01, 0x10, //     UNIT (Seconds)
    0x55, 0x00,       //     UNIT_EXPONENT (0)
    0xB1, 0xA3,

    // Report 28: AverageTimeToEmpty — INPUT + FEATURE
    0x85, RID_AVERAGETIME2EMPTY,
    0x09, 0x69,       //     USAGE (AverageTimeToEmpty)
    0x81, 0xA3,       //     INPUT
    0x09, 0x69,
    0xB1, 0xA3,       //     FEATURE

    // Report 13: RunTimeToEmpty — INPUT + FEATURE
    0x85, RID_RUNTIMETOEMPTY,
    0x09, 0x68,       //     USAGE (RunTimeToEmpty)
    0x81, 0xA3,       //     INPUT
    0x09, 0x68,
    0xB1, 0xA3,       //     FEATURE

    // Report 8: RemainingTimeLimit — FEATURE-only (corrected from HIDPowerDevice)
    0x85, RID_REMAINTIMELIMIT,
    0x09, 0x2A,       //     USAGE (RemainingTimeLimit)
    0x27, 0x64, 0x05, 0x00, 0x00, // LOGICAL_MAXIMUM (1380)
    0x16, 0x78, 0x00, //     LOGICAL_MINIMUM (120)
    0xB1, 0xA2,       //     FEATURE (Data, Var, Abs, Vol)

    // -- Switch to Power Device page (0x84) --
    0x05, 0x84,

    // Report 18: DelayBeforeShutdown — signed 16-bit
    0x85, RID_DELAYBE4SHUTDOWN,
    0x09, 0x57,       //     USAGE (DelayBeforeShutdown)
    0x16, 0x00, 0x80, //     LOGICAL_MINIMUM (-32768)
    0x27, 0xFF, 0x7F, 0x00, 0x00, // LOGICAL_MAXIMUM (32767)
    0xB1, 0xA2,

    // Report 19: DelayBeforeReboot — signed 16-bit
    0x85, RID_DELAYBE4REBOOT,
    0x09, 0x55,       //     USAGE (DelayBeforeReboot)
    0xB1, 0xA2,

    // Report 10: ConfigVoltage — unsigned, centivolts
    0x85, RID_CONFIGVOLTAGE,
    0x09, 0x40,       //     USAGE (ConfigVoltage)
    0x15, 0x00,       //     LOGICAL_MINIMUM (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, // LOGICAL_MAXIMUM (65535)
    0x67, 0x21, 0xD1, 0xF0, 0x00, // UNIT (Centivolts)
    0x55, 0x05,       //     UNIT_EXPONENT (5)
    0xB1, 0x23,       //     FEATURE (Const, Var, Abs, NonVol)

    // Report 11: Voltage — INPUT + FEATURE, centivolts
    0x85, RID_VOLTAGE,
    0x09, 0x30,       //     USAGE (Voltage)
    0x81, 0xA3,       //     INPUT
    0x09, 0x30,
    0xB1, 0xA3,       //     FEATURE

    // Report 21: Current — INPUT + FEATURE, signed, milliamps
    // Power Device page (0x84), usage 0x31
    0x85, RID_CURRENT,
    0x09, 0x31,       //     USAGE (Current)
    0x16, 0x00, 0x80, //     LOGICAL_MINIMUM (-32768)
    0x27, 0xFF, 0x7F, 0x00, 0x00, // LOGICAL_MAXIMUM (32767)
    0x67, 0x01, 0x00, 0x10, 0x00, // UNIT (Amps)
    0x55, 0x0D,       //     UNIT_EXPONENT (-3, milliamps)
    0x81, 0xA3,       //     INPUT
    0x09, 0x31,
    0xB1, 0xA3,       //     FEATURE

    // -- Switch to Battery System page (0x85) --
    0x05, 0x85,

    // Report 27: AverageCurrent — INPUT + FEATURE, signed, milliamps
    // Battery System page (0x85), usage 0x62
    0x85, RID_AVERAGECURRENT,
    0x09, 0x62,       //     USAGE (AverageCurrent)
    0x81, 0xA3,       //     INPUT
    0x09, 0x62,
    0xB1, 0xA3,       //     FEATURE

    // Report 20: AudibleAlarmControl — FEATURE-only (corrected from HIDPowerDevice)
    0x85, RID_AUDIBLEALARMCTRL,
    0x05, 0x84,       //     USAGE_PAGE (Power Device)
    0x09, 0x5A,       //     USAGE (AudibleAlarmControl)
    0x75, 0x08,       //     REPORT_SIZE (8)
    0x15, 0x01,       //     LOGICAL_MINIMUM (1)
    0x25, 0x03,       //     LOGICAL_MAXIMUM (3)
    0x65, 0x00,       //     UNIT (None)
    0x55, 0x00,       //     UNIT_EXPONENT (0)
    0xB1, 0xA2,       //     FEATURE (Data, Var, Abs, Vol)

    // -- PresentStatus (report 7) — nested Logical collection --
    0x09, 0x02,       //     USAGE (PresentStatus)
    0xA1, 0x02,       //     COLLECTION (Logical)
    0x85, RID_PRESENTSTATUS,
    0x05, 0x85,       //       USAGE_PAGE (Battery System)
    0x09, 0x44,       //       USAGE (Charging)
    0x75, 0x01,       //       REPORT_SIZE (1)
    0x15, 0x00,       //       LOGICAL_MINIMUM (0)
    0x25, 0x01,       //       LOGICAL_MAXIMUM (1)
    0x81, 0xA3,       //       INPUT (Const, Var, Abs)
    0x09, 0x44,
    0xB1, 0xA3,       //       FEATURE
    0x09, 0x45,       //       USAGE (Discharging)
    0x81, 0xA3,
    0x09, 0x45,
    0xB1, 0xA3,
    0x09, 0xD0,       //       USAGE (ACPresent)
    0x81, 0xA3,
    0x09, 0xD0,
    0xB1, 0xA3,
    0x09, 0xD1,       //       USAGE (BatteryPresent)
    0x81, 0xA3,
    0x09, 0xD1,
    0xB1, 0xA3,
    0x09, 0x42,       //       USAGE (BelowRemainingCapacityLimit)
    0x81, 0xA3,
    0x09, 0x42,
    0xB1, 0xA3,
    0x09, 0x43,       //       USAGE (RemainingTimeLimitExpired)
    0x81, 0xA2,       //       INPUT (Data, Var, Abs) — writable
    0x09, 0x43,
    0xB1, 0xA2,       //       FEATURE (Data, Var, Abs, Vol)
    0x09, 0x4B,       //       USAGE (NeedReplacement)
    0x81, 0xA3,
    0x09, 0x4B,
    0xB1, 0xA3,
    0x09, 0xDB,       //       USAGE (VoltageNotRegulated)
    0x81, 0xA3,
    0x09, 0xDB,
    0xB1, 0xA3,
    0x09, 0x46,       //       USAGE (FullyCharged)
    0x81, 0xA3,
    0x09, 0x46,
    0xB1, 0xA3,
    0x09, 0x47,       //       USAGE (FullyDischarged)
    0x81, 0xA3,
    0x09, 0x47,
    0xB1, 0xA3,
    0x05, 0x84,       //       USAGE_PAGE (Power Device)
    0x09, 0x68,       //       USAGE (ShutdownRequested)
    0x81, 0xA2,       //       INPUT (Data, Var, Abs) — writable
    0x09, 0x68,
    0xB1, 0xA2,
    0x09, 0x69,       //       USAGE (ShutdownImminent)
    0x81, 0xA3,
    0x09, 0x69,
    0xB1, 0xA3,
    0x09, 0x73,       //       USAGE (CommunicationLost)
    0x81, 0xA3,
    0x09, 0x73,
    0xB1, 0xA3,
    0x09, 0x65,       //       USAGE (Overload)
    0x81, 0xA3,
    0x09, 0x65,
    0xB1, 0xA3,
    // 2 padding bits
    0x95, 0x02,       //       REPORT_COUNT (2)
    0x81, 0x01,       //       INPUT (Const, Array, Abs)
    0xB1, 0x01,       //       FEATURE (Const, Array, Abs)
    0xC0,             //     END_COLLECTION (PresentStatus)

    0xC0,             //   END_COLLECTION (Logical/Sink)
    0xC0,             // END_COLLECTION (Application/UPS)
};

// --- Device descriptor ---

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x2E8A,
    .idProduct          = 0x0005,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1,
};

// --- Configuration descriptor ---

enum {
    ITF_NUM_HID      = 0,
    ITF_NUM_CDC      = 1,
    ITF_NUM_CDC_DATA = 2,
    ITF_NUM_TOTAL    = 3,
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                        sizeof(desc_hid_report), 0x81, 16, 10),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 0, 0x82, 8, 0x03, 0x83, 64),
};

// --- String descriptors ---

static char serial_str[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];

static char const *string_desc_arr[] = {
    nullptr,              // 0: language (handled separately)
    "pico-hid-ups",       // 1: manufacturer
    "Pico UPS",           // 2: product
    serial_str,           // 3: serial (filled at runtime)
    "LiP",                // 4: device chemistry
    "",                   // 5: OEM information
};

static uint16_t desc_str_buf[32 + 1];

static void init_serial_string() {
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        static const char hex[] = "0123456789ABCDEF";
        serial_str[2 * i]     = hex[(id.id[i] >> 4) & 0xF];
        serial_str[2 * i + 1] = hex[id.id[i] & 0xF];
    }
    serial_str[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES] = '\0';
}

// --- TinyUSB descriptor callbacks ---

extern "C" {

uint8_t const *tud_descriptor_device_cb(void) {
    return reinterpret_cast<uint8_t const *>(&desc_device);
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return desc_hid_report;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    if (index == 0) {
        desc_str_buf[0] = (uint16_t)(TUSB_DESC_STRING << 8 | 4);
        desc_str_buf[1] = 0x0409;
        return desc_str_buf;
    }

    if (index == STRID_SERIAL && serial_str[0] == '\0') {
        init_serial_string();
    }

    size_t count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]);
    if (index >= count) return nullptr;

    const char *str = string_desc_arr[index];
    uint8_t len = 0;
    for (; str[len] && len < 31; len++) {
        desc_str_buf[1 + len] = str[len];
    }

    desc_str_buf[0] = (uint16_t)(TUSB_DESC_STRING << 8 | (2 + 2 * len));
    return desc_str_buf;
}

} // extern "C"
