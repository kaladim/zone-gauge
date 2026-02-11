# ZoneGauge

`C++` analog of a [physical zone gauge](https://duckduckgo.com/?q=zone+gauge+meter&iar=images&t=h_), with hysteresis and debounce. In embedded-friendly form.  
For `C` variants, checkout any of the `c`-prefixed branches.  

## Features

- **Header-only**
- **Type-safe zones**: Zones are your own `enum class`
- **Hysteresis**: Separate upward/downward thresholds prevent zone oscillation
- **Debouncing**: Configurable debounce periods ensure stable zone transitions
- **Dynamic configuration**: The count and content of transition rules can be changed at runtime
- **Modern C++20**: Uses type constraints and `std::span`.

See a detailed operation manual [here](./doc/Operation.md).

## Use Cases

- **Battery Management**: Monitor battery voltage with multiple charge level zones
- **Temperature Control**: Track temperature ranges with hysteresis to prevent rapid cycling
- **Sensor Monitoring**: Process analog sensor readings with noise immunity
- **Level Detection**: Monitor fluid levels, pressure, or any measurable parameter with discrete states

## Get & Setup the repo
```bash
git clone https://github.com/kaladim/zone-gauge.git
cd zone-gauge/
git submodule update --init
git checkout cpp
```

## Project Structure

```
/
├── doc/
│   └── Operation.md         # Detailed operation manual
├── external/
│   └── googletest/          # Google Test framework
├── src/
│   ├── ZoneGauge.hpp        # The library code
│   └── CMakeLists.txt       # Library build configuration
├── test/
│   ├── ZoneGaugeTest.cpp    # Unit tests
│   ├── ZG_Config.h          # Test configuration
│   └── CMakeLists.txt       # Test build configuration
├── CMakeLists.txt           # Root build configuration
├── LICENSE                  # MIT License
└── README.md                # This file
```

## Integration

### 1. Include [`ZoneGauge.hpp`](./src/ZoneGauge.hpp) in your application(s)

```cpp
#include <ZoneGauge.hpp>
```

### 2. Define Your Zones

Define an enumeration for your zones:

```cpp
enum class BatteryZone {
    CriticalLow,
    Low,
    Normal,
    High,
    CriticalHigh
};
```

### 3. Implement Provider Functions

Provide free functions for tick. It is  passed as template parameter, so it must have external linkage.  
Value provider can be either free function or class method.

```cpp
// Tick provider - must return a monotonically increasing value
uint32_t GetSystemTick() {
    return HAL_GetTick(); // Or your platform's tick source
}

// Value provider - returns std::nullopt when the reading is invalid
std::optional<uint16_t> GetBatteryVoltage() {
    uint16_t voltage = ADC_ReadBatteryVoltage();
    if (voltage == 0)
        return std::nullopt;
    return voltage;
}
```

### 4. Instantiate the Type and Set Up Transition Rules

The class template takes the zone type, value type, tick type, and the two provider functions:

```cpp
using namespace zonegauge;
using BattGauge = ZoneGauge<BatteryZone, uint16_t, uint32_t, &GetSystemTick>;

// Upward thresholds (ascending order, debounce in ticks)
const std::array upRules = {
    BattGauge::TransitionRule{3300, BatteryZone::Low,          10},  // > 3.3V → Low
    BattGauge::TransitionRule{3600, BatteryZone::Normal,       10},  // > 3.6V → Normal
    BattGauge::TransitionRule{4000, BatteryZone::High,         10},  // > 4.0V → High
    BattGauge::TransitionRule{4150, BatteryZone::CriticalHigh, 10},  // > 4.15V → Critical High
};

// Downward thresholds (ascending order, debounce in ticks)
const std::array downRules = {
    BattGauge::TransitionRule{3200, BatteryZone::CriticalLow, 10},  // < 3.2V → Critical Low
    BattGauge::TransitionRule{3500, BatteryZone::Low,         10},  // < 3.5V → Low
    BattGauge::TransitionRule{3900, BatteryZone::Normal,      10},  // < 3.9V → Normal
    BattGauge::TransitionRule{4100, BatteryZone::High,        10},  // < 4.1V → High
};
```

**Note**: Rules must be sorted in ascending order of their thresholds!

### 5. Initialize and Use

```cpp
// Create a configuration and construct the gauge instance
BattGauge::Config config {
    .valueProvider = GetBatteryVoltage,
    .transitionRules = { .up = upRules, .down = downRules },
    .initial         = { .value = 3700, .zone = BatteryZone::Normal },
};

BattGauge gauge(config);

// In your main loop, call process() periodically
while (true) {
    gauge.process();

    switch (gauge.getZone()) {
        case BatteryZone::CriticalLow:  ShutdownSystem();    break;
        case BatteryZone::Low:          EnablePowerSaving(); break;
        case BatteryZone::Normal:       NormalOperation();   break;
        // ... handle other zones
        default: break;
    }

    Delay(100);
}
```

## Building

The project uses `CMake` for building:

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

### Running Tests

```bash
cd build
ctest -C Debug --output-on-failure
```


## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
