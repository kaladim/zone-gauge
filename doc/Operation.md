# Operation

## Overview

`ZoneGauge` tracks values within user-defined zones/ranges. It provides independent monitoring instances, allowing multiple values to be tracked independently.

## Processing Cycle

On each call to processing function/method:

1. **Value Acquisition**: Calls the value provider function
2. **Trend Detection**: Compares new value to last value (rising, falling, or stable)
3. **Threshold Crossing Detection**: Finds if value crossed any threshold
4. **Debounce Management**: Starts, monitors, or cancels debounce period
5. **Zone Confirmation**: Confirms new zone when debounce expires

### Threshold Crossing Detection

A crossing is detected only when the value **moves across** a threshold between two consecutive reads. Being above or below a threshold alone is not enough — the previous value must have been on the other side.

**Upward transitions** (value is rising, `actualValue > last.value`):
- Searches `up` rules from **highest to lowest** threshold.
- Match condition: `actualValue > threshold AND last.value <= threshold`.
- Returns the first (highest) match.

**Downward transitions** (value is falling, `actualValue < last.value`):
- Searches `down` rules from **lowest to highest** threshold.
- Match condition: `actualValue < threshold AND last.value >= threshold`.
- Returns the first (lowest) match.

**No change** (`actualValue == last.value`): no crossing is detected.

Only **one transition can be pending** at a time. A new crossing replaces any pending transition.

### Hysteresis

Hysteresis is a natural result of using different thresholds for upward and downward transitions. It is not a separate configuration parameter.

```
  Value                                                  Value
    ↑                                                      ↓
    │                      Zone HIGH                       │
    │                                                      │
4000├─ ↑(→Zone HIGH)───────────────────────────────────────┤                             
    ├┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄(Zone NORMAL←)↓ ┄┤3900
    │                     Zone NORMAL                      │
    │                                                      │
3600├─ ↑(→Zone NORMAL)─────────────────────────────────────┤
    ├┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄(Zone LOW←)↓ ┄┤3500
    │                      Zone LOW                        │
    │                                                      │
    └──────────────────────────────────────────────────────┘

Legend:
─── = Upward threshold (rising value must exceed this)
┄┄┄ = Downward threshold (falling value must go below this)
↑ = Upward transition    ↓ = Downward transition
```

In the example above (battery voltage in mV):
- Enter NORMAL when rising above 3600 mV or when falling below 3900 mV.
- Exit NORMAL when falling below 3500 mV or when rising above 4000 mV.
- Hysteresis band: 100 mV. Value can fluctuate between 3500–3600 mV or 3900–4000 mV without triggering transitions.

### Debounce Cancellation

If no new crossing is detected, the pending transition is cancelled when the value crosses back over the threshold:
- Upward pending: cancelled if `actualValue <= threshold`
- Downward pending: cancelled if `actualValue >= threshold`

Upon cancellation, a **transition rescan** is performed: a search for a threshold crossing between `actualValue` and the `oldValue` recorded when the cancelled transition started. This is necessary because:
1. When the original transition was detected, only the outermost threshold crossing was captured. Lower thresholds (for upward transitions) or higher thresholds (for downward transitions) may have also been crossed at that time.
2. The regressed value may settle inside a hysteresis zone, triggering no new crossing on its own, yet one of those previously crossed thresholds still applies.

The rescan may establish a new pending transition (at a closer threshold crossed during the same original move), or find nothing if the value has settled in a region with no applicable crossing.