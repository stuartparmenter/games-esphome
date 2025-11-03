# LVGL Game Runner for ESPHome

A game runner component for ESPHome that renders games on LVGL Canvas widgets.

## Features

- **Component-based Architecture**: Games are independent ESPHome components
- **Multiple Game Support**: Easy addition of new games as separate components
- **Flexible Input System**: Supports buttons, rotary encoders, Bluetooth gamepads, and future touchscreen input
- **Bluetooth Gamepad Support**: Optional Bluepad32 integration for wireless controllers (Xbox, PlayStation, Switch, etc.)
- **FPS Control**: Configurable frame rate (1-240 FPS)
- **Canvas-based Rendering**: Uses LVGL canvas for efficient drawing
- **Performance Metrics**: Optional compile-time metrics tracking
- **Pause/Resume**: Low-power pause mode when games aren't active
- **ESPHome Integration**: Full automation action support

## Currently Implemented Games

### Snake
- Classic snake game with grid-based gameplay
- Configurable walls (collision vs wraparound)
- Autoplay mode for testing
- Supports both directional and rotary encoder input

### Breakout
- Classic brick-breaking arcade game
- 8 special brick types with unique powerups (shield, extra ball, wider paddle, extra life, wonky bricks, shooter, powerup shuffle)
- Progressive difficulty with increasing levels and speed
- Multi-ball support (up to 10 balls)
- Projectile shooting system
- Lives system with visual heart indicators
- Autoplay AI mode

### Pong
- Classic Pong with AI vs AI gameplay
- Sophisticated human-like AI featuring:
  - Reaction delays (different for each paddle)
  - Smooth acceleration and deceleration
  - Speed caps with panic boost when ball is close
  - Random jitter and aim bias for realistic imperfection
  - Occasional intentional misses (6% chance)
- Ball spin based on paddle movement
- Serve angle variation
- Edge-based scoring
- xorshift32 PRNG for deterministic randomness

## Architecture

Component-based architecture inspired by ESPHome's light effects pattern:

- **GameBase**: Base class for all games (similar to FxBase)
- **GameRegistry**: Registry pattern for game instance management
- **LvglGameRunner**: Main component managing timing, input, and lifecycle
- **InputHandler**: Thread-safe input queue with semaphore protection
- **Separate Game Components**: Each game is an independent ESPHome component (e.g., `game_snake`, `game_breakout`)
- **Decorator Pattern**: Games use `@register_game()` decorator (similar to `@register_addressable_effect()`)

## Directory Structure

```
esphome/components/
├── lvgl_game_runner/               # Core game runner component
│   ├── __init__.py                 # ESPHome integration & register_game() decorator
│   ├── lvgl_game_runner.h / .cpp   # Main component
│   ├── game_base.h                 # Base class interface
│   ├── game_registry.h             # Registry pattern
│   ├── input_types.h               # Input event definitions
│   ├── input_handler.h / .cpp      # Input abstraction
│   └── game_state.h                # Score/lives utilities
│
├── game_snake/                     # Snake game component
│   ├── __init__.py                 # Component registration using @register_game()
│   ├── game_snake.h / .cpp         # Snake game implementation
│   └── (future: custom config)
│
├── game_breakout/                  # Breakout game component
│   ├── __init__.py                 # Component registration using @register_game()
│   ├── game_breakout.h / .cpp      # Breakout game implementation
│   └── (future: custom config)
│
└── game_pong/                      # Pong game component
    ├── __init__.py                 # Component registration using @register_game()
    ├── game_pong.h / .cpp          # Pong game implementation
    └── (future: custom config)
```

## Installation

1. Copy the component directories to your ESPHome configuration:
   - `esphome/components/lvgl_game_runner/` (core runner)
   - `esphome/components/game_snake/` (Snake game)
   - `esphome/components/game_breakout/` (Breakout game)
   - `esphome/components/game_pong/` (Pong game)

2. Add as external components in your YAML:

```yaml
external_components:
  - source:
      type: local
      path: esphome/components
    components: [lvgl_game_runner, game_snake, game_breakout, game_pong]
```

## Basic Usage

```yaml
# 1. Declare game components first
game_snake:
  id: snake

# 2. Configure the game runner
lvgl_game_runner:
  id: my_game
  game: snake        # References the registered game name
  canvas: game_canvas
  fps: 30
  start_paused: false
  bluepad32: false   # Optional: Enable Bluetooth gamepad support
```

## Input Configuration

### Button Controls

```yaml
binary_sensor:
  - platform: gpio
    id: button_up
    pin: GPIO0
    on_press:
      - lvgl_game_runner.send_input:
          id: my_game
          input: "UP"
```

### Rotary Encoder

```yaml
sensor:
  - platform: rotary_encoder
    id: game_knob
    pin_a: GPIO32
    pin_b: GPIO33
    on_clockwise:
      - lvgl_game_runner.send_input:
          id: my_game
          input: "ROTATE_CW"
```

### Bluetooth Gamepads (Bluepad32)

Enable wireless gamepad support with Bluepad32:

```yaml
esp32:
  framework:
    type: esp-idf  # Required for Bluepad32

lvgl_game_runner:
  id: my_game
  game: snake
  canvas: game_canvas
  fps: 30
  bluepad32: true  # Enable Bluetooth gamepad support
```

**Supported Controllers:**
- Xbox One/Series controllers
- PlayStation DualShock 4 / DualSense (PS4/PS5)
- Nintendo Switch Pro / Joy-Cons
- 8BitDo controllers
- Generic HID gamepads

**Features:**
- Automatic controller pairing (no manual setup required)
- D-pad and analog stick support (with deadzone)
- Button mapping (A, B buttons + directional input)
- Runs on dedicated CPU core (CPU0) for optimal performance
- Thread-safe integration with existing GPIO inputs

**Note:** Bluepad32 requires exclusive Bluetooth access and cannot be used with other ESPHome BLE components (`esp32_ble_tracker`, `bluetooth_proxy`, etc.). DualShock 3 (PS3) controllers are not supported without patches.

## Actions

- `lvgl_game_runner.start` - Start/restart game
- `lvgl_game_runner.pause` - Pause game
- `lvgl_game_runner.resume` - Resume game
- `lvgl_game_runner.toggle` - Toggle pause state
- `lvgl_game_runner.set_game` - Switch games
- `lvgl_game_runner.send_input` - Send input event
- `lvgl_game_runner.set_fps` - Adjust frame rate

## Input Types

Supported input types:
- `UP`, `DOWN`, `LEFT`, `RIGHT` - Directional
- `A`, `B` - Action buttons
- `SELECT`, `START` - Menu buttons
- `ROTATE_CW`, `ROTATE_CCW` - Rotary encoder
- `TOUCH` - Future touchscreen support

## Adding New Games

Create a new game as an independent ESPHome component:

### 1. Create Component Directory

```
esphome/components/game_yourname/
├── __init__.py
├── game_yourname.h
└── game_yourname.cpp
```

### 2. Implement Game Class (`game_yourname.h`)

```cpp
#pragma once
#include "esphome/components/lvgl_game_runner/game_base.h"

namespace esphome::game_yourname {

using lvgl_game_runner::GameBase;

class GameYourName : public GameBase {
 public:
  void step(float dt) override;     // Game logic & rendering
  void reset() override;             // Initialize game state
  void on_input(const InputEvent &event) override;  // Handle input
};

}  // namespace esphome::game_yourname
```

### 3. Register with Component (`__init__.py`)

```python
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import lvgl_game_runner

DEPENDENCIES = ["lvgl_game_runner"]

game_yourname_ns = cg.esphome_ns.namespace("game_yourname")
GameYourName = game_yourname_ns.class_("GameYourName", lvgl_game_runner.GameBase)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GameYourName),
})

@lvgl_game_runner.register_game("yourname")
async def to_code(config, var):
    """Game initialization - add custom config here if needed."""
    pass
```

### 4. Use in YAML

```yaml
external_components:
  - source:
      type: local
      path: esphome/components
    components: [lvgl_game_runner, game_yourname]

game_yourname:
  id: my_custom_game

lvgl_game_runner:
  id: runner
  game: yourname
  canvas: game_canvas
```

See [example.yaml](example.yaml) and existing games for complete examples.

## Performance

Target performance on ESP32:
- Snake @ 30 FPS: ~20-30% CPU
- Metrics tracking adds ~1-2% overhead
- Paused games: ~0% CPU (loop disabled)

## Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `game` | string | required | Game name ("snake", "breakout", "pong") |
| `canvas` | id | required | LVGL canvas widget ID |
| `fps` | float | 30.0 | Target frame rate (1-240) |
| `x`, `y` | int | 0 | Sub-region offset |
| `width`, `height` | int | 0 | Sub-region size (0=full canvas) |
| `start_paused` | bool | false | Start in paused state |
| `bluepad32` | bool | false | Enable Bluetooth gamepad support (requires ESP-IDF) |

## Examples

See [example.yaml](example.yaml) for a complete working configuration.

## Credits

Architecture inspired by [lvgl-canvas-fx](https://github.com/stuartparmenter/lvgl-canvas-fx).

Games ported from [esphome-clock-os](https://github.com/richrd/esphome-clock-os):
- Snake game
- Breakout game

Games ported from [hub75-studio](https://github.com/stuartparmenter/hub75-studio):
- Pong
