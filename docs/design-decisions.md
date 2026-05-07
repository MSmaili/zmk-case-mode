# Design Decisions

This document explains _why_ things are the way they are. Useful if you're coming back to this code after a while or learning how ZMK modules work.

## Why a ZMK module and not just keymap macros?

ZMK's keymap system lets you bind keys and define layers, but it can't intercept and rewrite key events globally. To transform space into underscore _only while a mode is active_, you need a behavior that hooks into the event system. That requires C code compiled into the firmware.

## Why devicetree (DT) for configuration?

ZMK uses Zephyr RTOS, which uses devicetree for all hardware and behavior configuration. It's not a choice - it's the framework. The flow is:

1. You write `.dtsi` files (like JSON for embedded systems)
2. The build system validates them against `.yaml` bindings (the schema)
3. It generates C macros that your code reads at compile time

There's no runtime config, no files to parse, no dynamic allocation. Everything is baked into the binary. That's why changes require reflashing.

## Why `#if DT_HAS_COMPAT_STATUS_OKAY`?

```c
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
// ... entire file ...
#endif
```

If nobody uses this behavior in their keymap, this guard makes the compiler skip the entire file. Zero flash usage, zero RAM. In embedded systems, every byte matters.

## Why a `devs[]` array and a loop?

A user might define multiple instances (snake, camel, kebab). The event listener doesn't know which one is active - it has to check all of them. The `devs[]` array is built at compile time with all instances, and the listener loops through them.

We `break` out of the loop once we find the active one, since `activate_case_mode()` guarantees only one is active at a time.

## Why separate `config` and `data` structs?

This is a Zephyr pattern:

- **config** = read-only, lives in flash (ROM). Set at compile time from your DT properties. Can't change at runtime. Costs zero RAM.
- **data** = read-write, lives in RAM. Changes as you type. This is your runtime state.

Separating them means the config (which is most of the struct) doesn't waste precious RAM.

## Why `ev->implicit_modifiers` instead of sending a new event?

We don't create new key events. We mutate the existing one:

```c
ev->keycode = config->delimiter_keycode;
ev->implicit_modifiers = config->delimiter_mods;
```

This is simpler and safer. The event system already handles press/release pairing, timing, and HID reports. By rewriting in-place, we piggyback on all of that. Creating new events would mean managing press/release pairs ourselves, which is error-prone.

## Why `ZMK_EV_EVENT_BUBBLE` vs `ZMK_EV_EVENT_HANDLED`?

These are return values from the listener:

- **BUBBLE** = "I'm done with this event, pass it along." The event continues through the system and eventually gets sent to the computer. We may have modified it, but it still gets sent.
- **HANDLED** = "I consumed this event. Nobody else sees it." The computer never knows this key was pressed. We use this for camelCase's space - the space disappears entirely.

## Why deactivate all instances on activate?

```c
static void activate_case_mode(const struct device *dev) {
    for (int i = 0; i < ARRAY_SIZE(devs); i++) {
        deactivate_case_mode(devs[i]);
    }
    // then activate this one
}
```

This ensures mutual exclusion. You can't be in snake_case and camelCase at the same time. Without this, pressing one case mode while another is active would leave both running, and the listener would try to process the event twice with conflicting rules.

## Why `is_mod()` check before everything?

```c
if (is_mod(ev->usage_page, ev->keycode)) {
    continue;
}
```

Modifier keys (Shift, Ctrl, Alt, Gui) generate their own key events. If you press Shift to type a capital letter manually, we don't want that Shift event to deactivate case mode. We just ignore modifier events entirely.

## Why the shortcut mods check?

```c
if (case_mode_has_shortcut_mods(ev->implicit_modifiers)) {
    continue;
}
```

If you press Ctrl+C or Cmd+S while in case mode, that's a shortcut, not text. We don't want to transform it or deactivate. We check if any "command" modifiers are held and skip the event entirely if so.

## Why `continue-list` uses three fields (page, id, modifiers)?

A keycode in ZMK isn't just one number. `UNDERSCORE` is actually:

- page = keyboard (0x07)
- id = minus key (0x2D)
- modifiers = shift

Without all three, you couldn't distinguish between `-` and `_` in the continue-list (they're the same physical key, just with/without shift).

## Why flexible array member for continuations?

```c
struct behavior_case_mode_config {
    ...
    uint8_t continuations_count;
    struct case_mode_continue_item continuations[];  // flexible array
};
```

Each instance might have a different number of items in its continue-list. A flexible array member lets the struct's size vary per instance. The macro section calculates the right size at compile time. No dynamic allocation needed.

## Why the macro section is so ugly

```c
#define CASE_MODE_INST(n) \
    static struct ... = { ... }; \
    BEHAVIOR_DT_INST_DEFINE(n, ...);

DT_INST_FOREACH_STATUS_OKAY(CASE_MODE_INST)
```

This is Zephyr's way of doing "for each instance in devicetree, generate code." There's no cleaner way in C. The preprocessor doesn't have real loops, so macros expand into macros into macros. It looks terrible but it's the standard pattern every ZMK behavior uses.

The key macros:

- `DT_INST_PROP(n, prop)` - read a property from instance `n`
- `ZMK_HID_USAGE_ID(x)` - extract keycode from a ZMK key value
- `SELECT_MODS(x)` - extract modifiers from a ZMK key value
- `COND_CODE_1(cond, if_true, if_false)` - compile-time if/else
- `LISTIFY(count, macro, sep)` - repeat a macro N times

## Why `capitalize_first` sets `shift_next` on activation?

```c
data->shift_next = config->capitalize_first;
```

For PascalCase, the very first letter needs to be capitalized. Instead of adding a separate "is this the first letter?" check in the listener, we just pre-set the `shift_next` flag. The listener already knows how to handle that flag - it shifts the next alpha key and clears it. Simple reuse.

## Why we handle both press and release for space?

The space branch doesn't check `ev->state` (which tells you press vs release):

```c
if (case_mode_is_space(ev->usage_page, ev->keycode)) {
    if (config->emit_delimiter) {
        ev->keycode = config->delimiter_keycode;  // rewrite both press AND release
        ...
    }
}
```

The computer needs matched press/release pairs. If we rewrite space-press to underscore-press but let space-release through unchanged, the computer sees "underscore pressed, space released" which is broken. We rewrite both events identically so the host sees a clean underscore press+release pair.

The `capitalize_words` flag is only set on press (`ev->state` check) because we only want to flag once, not twice.
