/**
 * Zigbee2MQTT external converter for a Tuya IH-K663 one-button Zigbee remote
 * (Telink TLSR8258) running CUSTOM replacement firmware.
 *
 * Stock device (for reference): modelID 'TS0041', manufacturerName '_TZ3000_fa9mlvja'.
 * This file targets ONLY the custom-firmware identity below, so it will never
 * shadow/override the stock TS0041 converter shipped with zigbee-herdsman-converters.
 *
 * Wire format implemented by the custom firmware (endpoint 1):
 *  - Cluster genMultistateInput (0x0012), attribute presentValue (0x0055, uint16):
 *      a "gesture code" is reported on EVERY gesture, even if the code repeats:
 *        1 = single_click        4 = single_hold_start   5 = single_hold_stop
 *        2 = double_click        6 = double_hold_start   7 = double_hold_stop
 *        3 = triple_click        8 = triple_hold_start   9 = triple_hold_stop
 *  - Cluster genMultistateInput (0x0012), manufacturer-specific attribute 0xF001
 *    (61441 decimal, uint32, manufacturer code 0x1141 / Telink): carries the
 *    duration (ms) of the hold gesture that just ended. It is reported
 *    IMMEDIATELY BEFORE the matching *_hold_stop presentValue report, so we
 *    stash it per-device and attach it as `action_duration` when the next
 *    presentValue report is a *_hold_stop.
 *  - Cluster genPowerCfg (0x0001):
 *      batteryPercentageRemaining (0x0021) is in half-percent units (divide by 2).
 *      batteryVoltage (0x0020) is in 100 mV units (multiply by 0.1 for volts).
 */

const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');

const e = exposes.presets;
const ea = exposes.access;

// Manufacturer-specific hold-duration attribute: 0xF001 == 61441 (decimal),
// manufacturer code 0x1141 (Telink). Since this attribute is not registered
// in herdsman's stock cluster definitions, unmodified herdsman-converters
// will surface it in msg.data keyed by its raw numeric attribute id - which
// JS objects always coerce to a string key. We defensively check the numeric
// key, its string form, and the 0xF001 hex literal (all equal, but this
// keeps the intent obvious and survives future herdsman changes that might
// register the attribute under a friendly name).
const HOLD_DURATION_ATTR_ID = 0xF001; // 61441
const HOLD_DURATION_MFR_CODE = 0x1141; // Telink

function extractHoldDuration(msg) {
    const data = msg && msg.data;
    if (!data) return undefined;

    // Optional sanity check: if herdsman surfaced the manufacturer code on
    // the message, make sure it matches Telink's. Don't hard-fail if it's
    // missing/different - some herdsman versions don't expose it per-attribute.
    if (msg.manufacturerCode !== undefined && msg.manufacturerCode !== HOLD_DURATION_MFR_CODE) {
        return undefined;
    }

    if (Object.prototype.hasOwnProperty.call(data, HOLD_DURATION_ATTR_ID)) {
        return Number(data[HOLD_DURATION_ATTR_ID]);
    }
    const stringKey = String(HOLD_DURATION_ATTR_ID); // '61441'
    if (Object.prototype.hasOwnProperty.call(data, stringKey)) {
        return Number(data[stringKey]);
    }
    // Fallback: some herdsman versions may name it if a custom cluster
    // definition is loaded elsewhere in the userdata directory.
    if (Object.prototype.hasOwnProperty.call(data, 'holdTime')) {
        return Number(data.holdTime);
    }
    return undefined;
}

// Gesture code -> action name, per the custom firmware's presentValue reports.
const ACTION_LOOKUP = {
    1: 'single_click',
    2: 'double_click',
    3: 'triple_click',
    4: 'single_hold_start',
    5: 'single_hold_stop',
    6: 'double_hold_start',
    7: 'double_hold_stop',
    8: 'triple_hold_start',
    9: 'triple_hold_stop',
};

// Per-device store for the hold duration seen on the 0xF001 report that
// precedes a *_hold_stop presentValue report. Keyed by ieeeAddr so multiple
// remotes on the same network don't clobber each other's pending value.
const pendingHoldDurations = new Map();

// Per-device store for the LAST hold duration, kept indefinitely. Z2M treats
// `action` (and, in practice, the related `action_*` fields) as momentary and
// clears them shortly after publishing, so `action_duration` would flash then
// go back to null. To make it persist we re-assert the last known duration on
// every action report. Keyed by ieeeAddr.
const lastHoldDurations = new Map();

const fzLocal = {
    ih_k663_enhanced_action: {
        cluster: 'genMultistateInput',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            const ieeeAddr = meta.device.ieeeAddr;

            // 1) Manufacturer-specific hold-duration attribute (0xF001).
            //    Just stash it - it arrives in its own report, immediately
            //    before the matching *_hold_stop presentValue report.
            const holdDuration = extractHoldDuration(msg);
            if (holdDuration !== undefined) {
                pendingHoldDurations.set(ieeeAddr, holdDuration);
            }

            // 2) The gesture code itself, via presentValue.
            if (Object.prototype.hasOwnProperty.call(msg.data, 'presentValue')) {
                const code = Number(msg.data.presentValue);
                const action = ACTION_LOOKUP[code];

                if (action) {
                    // The firmware sends a fresh report on every gesture, even
                    // when the code repeats (e.g. two single_click in a row),
                    // so every presentValue report is treated as a new event -
                    // no debouncing/deduping is applied here.
                    result.action = action;

                    if (action.endsWith('_hold_stop')) {
                        const duration = pendingHoldDurations.get(ieeeAddr);
                        if (duration !== undefined) {
                            lastHoldDurations.set(ieeeAddr, duration);
                            pendingHoldDurations.delete(ieeeAddr);
                        }
                    }

                    // Re-assert the last known hold duration on EVERY action so
                    // it persists in Z2M state instead of resetting to null.
                    const lastDuration = lastHoldDurations.get(ieeeAddr);
                    if (lastDuration !== undefined) {
                        result.action_duration = lastDuration;
                    }
                } else {
                    meta.logger && meta.logger.warn &&
                        meta.logger.warn(`ts0041-enhanced: unknown presentValue action code '${msg.data.presentValue}'`);
                }
            }

            return result;
        },
    },

    ih_k663_enhanced_battery: {
        cluster: 'genPowerCfg',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            const result = {};
            const data = msg.data;

            if (Object.prototype.hasOwnProperty.call(data, 'batteryPercentageRemaining')) {
                // Reported in half-percent units per the Zigbee spec (0-200 -> 0-100%).
                result.battery = Math.round(data.batteryPercentageRemaining / 2);
            }

            if (Object.prototype.hasOwnProperty.call(data, 'batteryVoltage')) {
                // Reported in 100 mV units -> multiply by 0.1 to get volts.
                result.voltage = Math.round(data.batteryVoltage * 10) / 100;
            }

            return result;
        },
    },
};

module.exports = {
    // Match ONLY the custom firmware identity - never the stock TS0041 image.
    fingerprint: [
        {
            modelID: 'TS0041-Enhanced',
            manufacturerName: '_TZ3000_fa9mlvja-Enhanced',
        },
    ],

    model: 'TS0041-Enhanced',
    vendor: 'Tuya',
    description: 'IH-K663 one-button Zigbee remote (custom Telink TLSR8258 firmware, stock was TS0041 / _TZ3000_fa9mlvja)',

    fromZigbee: [fzLocal.ih_k663_enhanced_action, fzLocal.ih_k663_enhanced_battery],
    toZigbee: [], // Pure sensor/remote - nothing on this device is settable from Z2M.

    exposes: [
        e.action([
            'single_click',
            'single_hold_start',
            'single_hold_stop',
            'double_click',
            'double_hold_start',
            'double_hold_stop',
            'triple_click',
            'triple_hold_start',
            'triple_hold_stop',
        ]),
        exposes.numeric('action_duration', ea.STATE)
            .withUnit('ms')
            .withDescription(
                'Duration of the last hold gesture, published alongside the corresponding *_hold_stop action',
            ),
        e.battery(),
        exposes.numeric('voltage', ea.STATE)
            .withUnit('V')
            .withDescription('Battery voltage'),
    ],

    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);

        // Bind only what this converter needs to read from the device:
        // battery reports and the multistate-input gesture/duration reports.
        //
        // NOTE: On/Off, Level Control and Color Control (i.e. light-control)
        // bindings are NOT created here. Those are set up by the USER from
        // within Z2M (bind this remote's endpoint to their target lights or
        // groups via the frontend/"Bind" tab) - this converter deliberately
        // stays out of that so it doesn't fight with user-managed bindings.
        await reporting.bind(endpoint, coordinatorEndpoint, ['genPowerCfg', 'genMultistateInput']);

        await reporting.batteryPercentageRemaining(endpoint, {min: 3600, max: 21600, change: 0});
        await reporting.batteryVoltage(endpoint, {min: 3600, max: 21600, change: 0});
    },

    ota: true,
};
