# ZoneGauge

`C` analog of a [physical zone gauge](https://duckduckgo.com/?q=zone+gauge+meter&iar=images&t=h_), with hysteresis and debounce. In embedded-friendly form.   
For `C++` variant, checkout the `cpp` branch.  

## Features

- **Multiple Channels**: Monitor multiple values independently
- **Hysteresis**: Separate upward and downward thresholds prevent zone oscillation
- **Debouncing**: Configurable debounce periods ensure stable zone transitions
- **Dynamic configuration**: The count and content of transition rules can be changed at runtime
- **No Dynamic Allocation**: All memory allocated statically at compile time
- **Minimal Dependencies**: Pure C99 with no external dependencies

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
git checkout c-handle-based-api
```

## Project Structure

```
/
├── doc/
│   └── Operation.md         # Operation details
├── external/
│   └── googletest/          # Google Test framework
├── src/
│   ├── ZoneGauge.c          # Library implementation
│   ├── ZoneGauge.h          # Public API
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

### 1. Static configuration

Copy the [`ZG_Config.h`](./test/ZG_Config.h) to your application code base and modify it according to your needs.

### 2. Include [`ZoneGauge.h`](./src/ZoneGauge.h) in your application(s)

```c
#include "ZoneGauge.h"
```

### 3. Define Your Zones

Define an enumeration or constants for your zones:

```c
typedef enum {
    ZONE_CRITICAL_LOW,
    ZONE_LOW,
    ZONE_NORMAL,
    ZONE_HIGH,
    ZONE_CRITICAL_HIGH
} BatteryZone_t;
```

### 4. Set Up Transition Rules

Define zone transition rules for upward and downward movements:

```c
// Upward thresholds: a debounce is triggered when value rises above these levels
ZG_transitionRule_t upwardRules[] = {
    {3300, (void*)ZONE_LOW, 10},           // > 3.3V → Low (10 ticks debounce)
    {3600, (void*)ZONE_NORMAL, 10},        // > 3.6V → Normal
    {4000, (void*)ZONE_HIGH, 10},          // > 4.0V → High
    {4150, (void*)ZONE_CRITICAL_HIGH, 10}, // > 4.15V → Critical High
};

// Downward thresholds: a debounce is triggered when value falls below these levels
ZG_transitionRule_t downwardRules[] = {
    {3200, (void*)ZONE_CRITICAL_LOW, 10},  // < 3.2V → Critical Low
    {3500, (void*)ZONE_LOW, 10},           // < 3.5V → Low
    {3900, (void*)ZONE_NORMAL, 10},        // < 3.9V → Normal
    {4100, (void*)ZONE_HIGH, 10},          // < 4.1V → High
};

ZG_transitionRules_t upRules = {upwardRules, 4};
ZG_transitionRules_t downRules = {downwardRules, 4};
```

**Note**: Rules must be sorted in ascending order of their thresholds!

### 5. Implement Provider Functions

Provide tick and value provider functions:

```c
// Tick provider - must return monotonically increasing value
ZG_tick_t GetSystemTick(void)
{
    return HAL_GetTick(); // Or your system's tick function
}

// Value provider - returns current monitored value
bool GetBatteryVoltage(ZG_value_t* dest_ptr)
{
    uint16_t voltage = ADC_ReadBatteryVoltage();
    
    if (voltage == 0) // Invalid reading
        return false;
    
    *dest_ptr = voltage;
    return true;
}
```

### 6. Initialize and Use

```c
// Initialize the library (call once at startup)
ZG_Init(GetSystemTick);

// Create a channel configuration object
ZG_config_t batteryConfig = {
    .valueProvider = GetBatteryVoltage,
    .transitionRules = {
        .up = &upRules,
        .down = &downRules
    },
    .initial = {
        .value = 3700,
        .zone = (void*)ZONE_NORMAL
    }
};

// Open a channel
ZG_handle_t batteryChannel = ZG_Open(&batteryConfig);

// In your main loop, process the channel periodically
while (true) {
    ZG_Process(batteryChannel);
    
    // Get the current confirmed zone
    BatteryZone_t currentZone = (BatteryZone_t)ZG_GetZone(batteryChannel);
    
    // React to zone changes
    switch (currentZone) {
        case ZONE_CRITICAL_LOW:
            ShutdownSystem();
            break;
        case ZONE_LOW:
            EnablePowerSaving();
            break;
        case ZONE_NORMAL:
            NormalOperation();
            break;
        // ... handle other zones
    }
    
    Delay(100); // Adjust based on your needs
}

// Clean up when done
ZG_Close(batteryChannel);
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
