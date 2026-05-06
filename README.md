# ZMK Case Mode

`zmk-behavior-case-mode` is a small ZMK module that makes it easier to type names like `snake_case`, `camelCase`, and `kebab-case` on compact keyboards.

When a case mode is active, pressing `space` changes how the next word boundary is typed.

- `snake_case`
- `camelCase`
- `kebab-case`

## Features

- Rewrites `space` into `_` for snake case
- Rewrites `space` into `-` for kebab case
- Suppresses `space` and shifts the next letter key for camel case
- Lets you keep the mode active for selected extra keys via `continue-list`
- Ensures only one case mode instance is active at a time

## Install

Add the module to your ZMK config manifest, usually in `config/west.yml` or your top-level `west.yml`.

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

Define one or more behavior instances in your own config, for example in `config/case_modes.dtsi`.

Then include that file from your keymap or shared include file.

```dts
#define CASE_MODE_SNAKE 0
#define CASE_MODE_CAMEL 1
#define CASE_MODE_KEBAB 2

/ {
    behaviors {
        case_snake: case_snake {
            compatible = "zmk,behavior-case-mode";
            #binding-cells = <0>;
            mode = <CASE_MODE_SNAKE>;
            continue-list = <BACKSPACE DELETE>;
        };

        case_camel: case_camel {
            compatible = "zmk,behavior-case-mode";
            #binding-cells = <0>;
            mode = <CASE_MODE_CAMEL>;
            continue-list = <BACKSPACE DELETE>;
        };

        case_kebab: case_kebab {
            compatible = "zmk,behavior-case-mode";
            #binding-cells = <0>;
            mode = <CASE_MODE_KEBAB>;
            continue-list = <BACKSPACE DELETE>;
        };
    };
};
```

Example include:

```dts
#include "case_modes.dtsi"
```

Then bind them however you want, for example through combos:

```dts
combo_snake_case {
    timeout-ms = <50>;
    bindings = <&case_snake>;
    key-positions = <1 2>;
    layers = <0>;
};
```

## Properties

- `mode`: required integer enum (`0` = snake, `1` = camel, `2` = kebab)
- `continue-list`: optional list of extra keycodes that should not deactivate the mode

Alphanumeric keys are always allowed to continue the mode.

## Behavior Notes

- Pressing an active case mode again toggles it off.
- Activating one case mode deactivates any other active case mode instance.
- In camel case mode, only the next alphabetic key after `space` is shifted.

## Development

This module follows the ZMK module layout documented here:

- https://zmk.dev/docs/development/module-creation
