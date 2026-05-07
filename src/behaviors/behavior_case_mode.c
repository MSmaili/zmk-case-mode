/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_case_mode

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct case_mode_continue_item {
    uint16_t page;
    uint32_t id;
    uint8_t implicit_modifiers;
};

struct behavior_case_mode_config {
    uint8_t delimiter_keycode;
    uint8_t delimiter_mods;
    bool emit_delimiter;
    bool capitalize_words;
    bool capitalize_first;
    bool capitalize_all;
    uint8_t continuations_count;
    struct case_mode_continue_item continuations[];
};

struct behavior_case_mode_data {
    bool active;
    bool shift_next;
};

#define GET_DEV(inst) DEVICE_DT_INST_GET(inst),
static const struct device *devs[] = {DT_INST_FOREACH_STATUS_OKAY(GET_DEV)};

static void deactivate_case_mode(const struct device *dev) {
    struct behavior_case_mode_data *data = dev->data;

    data->active = false;
    data->shift_next = false;
}

static void activate_case_mode(const struct device *dev) {
    const struct behavior_case_mode_config *config = dev->config;

    for (int i = 0; i < ARRAY_SIZE(devs); i++) {
        deactivate_case_mode(devs[i]);
    }

    struct behavior_case_mode_data *data = dev->data;

    data->active = true;
    data->shift_next = config->capitalize_first;
}

static int on_case_mode_binding_pressed(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_case_mode_data *data = dev->data;

    if (data->active) {
        deactivate_case_mode(dev);
    } else {
        activate_case_mode(dev);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_case_mode_binding_released(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_case_mode_driver_api = {
    .binding_pressed = on_case_mode_binding_pressed,
    .binding_released = on_case_mode_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

static int case_mode_keycode_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_case_mode, case_mode_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_case_mode, zmk_keycode_state_changed);

static bool case_mode_is_alpha(uint16_t usage_page, uint8_t usage_id) {
    return usage_page == HID_USAGE_KEY && usage_id >= HID_USAGE_KEY_KEYBOARD_A &&
           usage_id <= HID_USAGE_KEY_KEYBOARD_Z;
}

static bool case_mode_is_space(uint16_t usage_page, uint8_t keycode) {
    return usage_page == HID_USAGE_KEY && keycode == HID_USAGE_KEY_KEYBOARD_SPACEBAR;
}

static bool case_mode_has_shortcut_mods(uint8_t implicit_modifiers) {
    uint8_t mods = implicit_modifiers | zmk_hid_get_explicit_mods();

    return (mods & (MOD_LCTL | MOD_RCTL | MOD_LALT | MOD_RALT | MOD_LGUI | MOD_RGUI)) != 0;
}

static bool case_mode_is_in_continue_list(const struct behavior_case_mode_config *config,
                                          uint16_t usage_page, uint8_t usage_id,
                                          uint8_t implicit_modifiers) {
    for (int i = 0; i < config->continuations_count; i++) {
        const struct case_mode_continue_item *item = &config->continuations[i];

        if (item->page == usage_page && item->id == usage_id &&
            (item->implicit_modifiers &
             (implicit_modifiers | zmk_hid_get_explicit_mods())) == item->implicit_modifiers) {
            return true;
        }
    }

    return false;
}

static int case_mode_keycode_state_changed_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (int i = 0; i < ARRAY_SIZE(devs); i++) {
        const struct device *dev = devs[i];
        struct behavior_case_mode_data *data = dev->data;

        if (!data->active) {
            continue;
        }

        const struct behavior_case_mode_config *config = dev->config;

        if (is_mod(ev->usage_page, ev->keycode)) {
            continue;
        }

        if (case_mode_has_shortcut_mods(ev->implicit_modifiers)) {
            continue;
        }

        // Space: delimiter handling
        if (case_mode_is_space(ev->usage_page, ev->keycode)) {
            if (config->emit_delimiter) {
                ev->keycode = config->delimiter_keycode;
                ev->implicit_modifiers = config->delimiter_mods;
                if (ev->state && config->capitalize_words) {
                    data->shift_next = true;
                }
                return ZMK_EV_EVENT_BUBBLE;
            }

            // No delimiter emitted (e.g. camelCase) - just flag next letter
            if (ev->state && config->capitalize_words) {
                data->shift_next = true;
            }
            return ZMK_EV_EVENT_HANDLED;
        }

        if (!ev->state) {
            continue;
        }

        // Alpha keys: apply shift if needed
        if (case_mode_is_alpha(ev->usage_page, ev->keycode)) {
            if (config->capitalize_all || data->shift_next) {
                ev->implicit_modifiers |= MOD_LSFT;
                data->shift_next = false;
            }
            break;
        }

        // Non-alpha press: clear pending shift
        data->shift_next = false;

        // Deactivate if not in continue list
        if (!case_mode_is_in_continue_list(config, ev->usage_page, ev->keycode,
                                           ev->implicit_modifiers)) {
            LOG_DBG("Deactivating case_mode for 0x%02X - 0x%02X", ev->usage_page, ev->keycode);
            deactivate_case_mode(dev);
        }
        break;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

#define PARSE_CONTINUE(i)                                                                          \
    {.page = ZMK_HID_USAGE_PAGE(i), .id = ZMK_HID_USAGE_ID(i), .implicit_modifiers = SELECT_MODS(i)}

#define CONTINUE_ITEM(i, n) PARSE_CONTINUE(DT_INST_PROP_BY_IDX(n, continue_list, i))

#define HAS_CONTINUE_LIST(n) DT_INST_NODE_HAS_PROP(n, continue_list)

#define CASE_MODE_INST(n)                                                                          \
    static struct behavior_case_mode_data behavior_case_mode_data_##n = {                          \
        .active = false, .shift_next = false};                                                     \
    static const struct behavior_case_mode_config behavior_case_mode_config_##n = {                \
        .delimiter_keycode = ZMK_HID_USAGE_ID(DT_INST_PROP(n, delimiter)),                         \
        .delimiter_mods = SELECT_MODS(DT_INST_PROP(n, delimiter)),                                 \
        .emit_delimiter = DT_INST_PROP(n, emit_delimiter),                                         \
        .capitalize_words = DT_INST_PROP(n, capitalize_words),                                     \
        .capitalize_first = DT_INST_PROP(n, capitalize_first),                                     \
        .capitalize_all = DT_INST_PROP(n, capitalize_all),                                         \
        .continuations_count = COND_CODE_1(HAS_CONTINUE_LIST(n),                                   \
                                           (DT_INST_PROP_LEN(n, continue_list)), (0)),             \
        .continuations = {COND_CODE_1(HAS_CONTINUE_LIST(n),                                        \
                                      (LISTIFY(DT_INST_PROP_LEN(n, continue_list),                 \
                                               CONTINUE_ITEM, (, ), n)),                           \
                                      ())},                                                        \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_case_mode_data_##n,                           \
                            &behavior_case_mode_config_##n, POST_KERNEL,                           \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_case_mode_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CASE_MODE_INST)

#endif
