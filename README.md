# ZMK Case Mode

A ZMK module that lets you type identifiers like `snake_case`, `camelCase`, `PascalCase`, `kebab-case`, and more; just by typing normally with spaces.

Activate a case mode, type words separated by space, and the module transforms each space into the appropriate delimiter or capitalizes the next letter. Inspired by ZMK's built-in [caps-word](https://zmk.dev/docs/keymaps/behaviors/caps-word) - same idea of a smart mode that auto-deactivates, but for case styles.

## Install

Add the module to your ZMK config manifest (`config/west.yml`):

```yaml
manifest:
  remotes:
    - name: msmaili
      url-base: https://github.com/MSmaili
  projects:
    - name: zmk-case-mode
      remote: msmaili
      revision: main
```

## Usage

Include the pre-defined behaviors and bind them in your keymap:

```dts
#include <behaviors/case_mode.dtsi>

/ {
    keymap {
        default_layer {
            bindings = <... &case_snake ...>;
        };
    };
};
```

Or via combos:

```dts
/ {
    combos {
        combo_snake {
            timeout-ms = <50>;
            bindings = <&case_snake>;
            key-positions = <1 2>;
        };
    };
};
```

## Pre-defined Behaviors

Ready to use after including the dtsi. All pre-defined behaviors include numbers (`N0`-`N9`) and `BACKSPACE` in their `continue-list`.
Create your own behavior if you want to remove it or add more keys like `DELETE`.

| Binding           | Produces        | Example       |
| ----------------- | --------------- | ------------- |
| `&case_snake`     | snake_case      | `my_variable` |
| `&case_camel`     | camelCase       | `myVariable`  |
| `&case_pascal`    | PascalCase      | `MyVariable`  |
| `&case_kebab`     | kebab-case      | `my-variable` |
| `&case_screaming` | SCREAMING_SNAKE | `MY_VARIABLE` |

## Custom Behaviors

Need dot.case, Train-Case, or something else? Define your own:

```dts
/ {
    behaviors {
        case_dot: case_dot {
            compatible = "zmk,behavior-case-mode";
            #binding-cells = <0>;
            delimiter = <DOT>;
            emit-delimiter;
            continue-list = <N1 N2 N3 N4 N5 N6 N7 N8 N9 N0 BACKSPACE>;
        };
    };
};
```

Path-style delimiters also work, for example:

```dts
/ {
    behaviors {
        case_path: case_path {
            compatible = "zmk,behavior-case-mode";
            #binding-cells = <0>;
            delimiter = <SLASH>;
            emit-delimiter;
        };
    };
};
```

## Properties

| Property           | Type    | Default | Description                                                    |
| ------------------ | ------- | ------- | -------------------------------------------------------------- |
| `delimiter`        | int     | `0`     | Keyboard keycode emitted on space, such as `UNDERSCORE`, `MINUS`, `DOT`, or `SLASH`. Set this when `emit-delimiter` is enabled. |
| `emit-delimiter`   | boolean | false   | Whether to send the delimiter. When false, space is swallowed. |
| `capitalize-words` | boolean | false   | Capitalize the first letter of each word (after space).        |
| `capitalize-first` | boolean | false   | Capitalize the very first letter after activation.             |
| `capitalize-all`   | boolean | false   | Capitalize every letter (for SCREAMING styles).                |
| `continue-list`    | array   | none    | Extra keycodes that don't deactivate case mode.                |

Boolean properties in devicetree are enabled by writing the property name alone, and disabled by omitting it.

Alpha keys (A-Z) always continue the mode. Everything else deactivates unless it's in `continue-list`.

### How properties combine

| Style           | delimiter    | emit-delimiter | capitalize-words | capitalize-first | capitalize-all |
| --------------- | ------------ | -------------- | ---------------- | ---------------- | -------------- |
| snake_case      | `UNDERSCORE` | ✓              |                  |                  |                |
| camelCase       |              |                | ✓                |                  |                |
| PascalCase      |              |                | ✓                | ✓                |                |
| kebab-case      | `MINUS`      | ✓              |                  |                  |                |
| SCREAMING_SNAKE | `UNDERSCORE` | ✓              |                  |                  | ✓              |
| dot.case        | `DOT`        | ✓              |                  |                  |                |
| Train-Case      | `MINUS`      | ✓              | ✓                | ✓                |                |

## Behavior Notes

- Press the case mode binding again to toggle it off.
- Activating one case mode deactivates any other active instance.
- Modifier chords (Ctrl/Alt/Gui + key) pass through without affecting case mode.
- Any key not in alpha (A-Z) or `continue-list` deactivates the mode and passes through normally.

## Development

This module follows the ZMK module layout:
https://zmk.dev/docs/development/module-creation
