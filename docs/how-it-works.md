# How Case Mode Works

A high-level explanation of the mental model — no C knowledge required.

## The Core Idea

You press a key to enter a "mode." While in that mode, your spacebar does something different. When you're done, the mode turns off.

```mermaid
stateDiagram-v2
    [*] --> Inactive: keyboard boots
    Inactive --> Active: press case-mode key
    Active --> Inactive: press case-mode key again
    Active --> Inactive: type a non-continuation key
    Active --> Active: type alpha / space / continue-list key
```

## What Happens to Each Key

While case mode is active:

```mermaid
flowchart LR
    A[You press a key] --> B{What key?}
    B -->|A-Z| C[Passes through<br/>maybe shifted]
    B -->|Space| D[Becomes delimiter<br/>or gets swallowed]
    B -->|In continue-list| E[Passes through normally]
    B -->|Anything else| F[Passes through<br/>AND deactivates mode]
    B -->|Ctrl/Alt/Gui + key| G[Ignored entirely<br/>shortcuts work normally]
```

## Example: Typing `my_variable` in Snake Case

```mermaid
sequenceDiagram
    participant You
    participant Keyboard as Keyboard (firmware)
    participant Computer

    Note over Keyboard: case_snake is ACTIVE

    You->>Keyboard: press M
    Keyboard->>Computer: send 'm'

    You->>Keyboard: press Y
    Keyboard->>Computer: send 'y'

    You->>Keyboard: press SPACE
    Note over Keyboard: rewrite space → shift+minus = '_'
    Keyboard->>Computer: send '_'

    You->>Keyboard: press V
    Keyboard->>Computer: send 'v'

    You->>Keyboard: press A, R, I, A, B, L, E
    Keyboard->>Computer: send 'a', 'r', 'i', 'a', 'b', 'l', 'e'

    You->>Keyboard: press ENTER
    Note over Keyboard: ENTER not alpha, not in continue-list → deactivate
    Keyboard->>Computer: send 'enter'

    Note over Keyboard: case_snake is INACTIVE
```

Result: `my_variable` + enter, and you're back to normal typing.

## Example: Typing `myVariable` in Camel Case

```mermaid
sequenceDiagram
    participant You
    participant Keyboard as Keyboard (firmware)
    participant Computer

    Note over Keyboard: case_camel is ACTIVE

    You->>Keyboard: press M, Y
    Keyboard->>Computer: send 'm', 'y'

    You->>Keyboard: press SPACE
    Note over Keyboard: swallow space, flag "shift next letter"
    Keyboard->>Computer: (nothing sent)

    You->>Keyboard: press V
    Note over Keyboard: shift_next is true → add shift
    Keyboard->>Computer: send 'V'

    You->>Keyboard: press A, R, I, A, B, L, E
    Keyboard->>Computer: send 'a', 'r', 'i', 'a', 'b', 'l', 'e'

    You->>Keyboard: press ENTER
    Keyboard->>Computer: send 'enter' (+ deactivate)
```

Result: `myVariable` + enter.

## Example: Typing `MyVariable` in Pascal Case

Same as camelCase, but the very first letter is also shifted:

```mermaid
sequenceDiagram
    participant You
    participant Keyboard as Keyboard (firmware)
    participant Computer

    Note over Keyboard: case_pascal ACTIVE, shift_next = true (from capitalize_first)

    You->>Keyboard: press M
    Note over Keyboard: shift_next is true → add shift
    Keyboard->>Computer: send 'M'

    You->>Keyboard: press Y
    Keyboard->>Computer: send 'y'

    You->>Keyboard: press SPACE
    Note over Keyboard: swallow, flag shift_next
    Keyboard->>Computer: (nothing)

    You->>Keyboard: press V
    Keyboard->>Computer: send 'V'

    You->>Keyboard: press A, R
    Keyboard->>Computer: send 'a', 'r'
```

Result: `MyVar...`

## The Properties — What They Control

```mermaid
flowchart TD
    SPACE[Space pressed] --> EMIT{emit-delimiter?}

    EMIT -->|true| DELIM[Send delimiter keycode<br/>e.g. underscore, hyphen, dot]
    EMIT -->|false| SWALLOW[Swallow space<br/>send nothing]

    DELIM --> CAP1{capitalize-words?}
    SWALLOW --> CAP2{capitalize-words?}

    CAP1 -->|yes| FLAG1[Flag: shift next alpha]
    CAP1 -->|no| DONE1[Done]
    CAP2 -->|yes| FLAG2[Flag: shift next alpha]
    CAP2 -->|no| DONE2[Done]

    ALPHA[Alpha key pressed] --> SHIFT{capitalize-all?}
    SHIFT -->|yes| UPPER[Always send uppercase]
    SHIFT -->|no| NEXT{shift_next flag set?}
    NEXT -->|yes| UPPER2[Send uppercase, clear flag]
    NEXT -->|no| LOWER[Send lowercase]
```

## Exiting Case Mode

Three ways out:

| Method                    | What happens                                                                 |
| ------------------------- | ---------------------------------------------------------------------------- |
| **Toggle key**            | Press the same binding again. Clean, explicit.                               |
| **Non-continuation key**  | Type anything not in alpha/continue-list. It passes through AND deactivates. |
| **Activate another mode** | Pressing a different case-mode binding deactivates the current one.          |

The toggle key is the recommended "I'm done" action. The non-continuation exit is a safety net so you don't get stuck.

## What Stays Active (Continue-List)

By default, only A-Z keeps the mode alive. Everything else exits.

If you want numbers, backspace, or other keys to NOT exit the mode, add them to `continue-list`:

```
continue-list = <N1 N2 N3 N4 N5 N6 N7 N8 N9 N0 BACKSPACE DELETE>;
```

The pre-defined behaviors (`&case_snake`, etc.) already include numbers.
