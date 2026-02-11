/******************************************************************************/
/*
 * MIT License
 *
 * Copyright (c) 2026 Kaloyan Dimitrov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
/******************************************************************************/
#include <cstdint>
#include <gtest/gtest.h>
#include "ZoneGauge.h"

// Zone enumeration for test clarity
enum class Zone {
    UnderLow  = 1,
    UnderHigh = 2,
    Normal    = 3,
    OverLow   = 4,
    OverHigh  = 5
};

// Helper to convert Zone to void* for API calls
inline void* ToVoidPtr(Zone zone) {
    return reinterpret_cast<void*>(static_cast<intptr_t>(zone));
}

inline bool operator==(void* lhs, Zone rhs) {
    return (reinterpret_cast<intptr_t>(lhs) == static_cast<intptr_t>(rhs));
}

// Parameterized test fixture
class ZoneGaugeTest : public ::testing::Test {
    protected:
    static uint16_t   currentTick;
    static ZG_value_t currentValue;
    static bool       valueProviderReturnValue;

    static ZG_tick_t TickProvider() {
        return currentTick;
    }

    static bool ValueProvider(ZG_value_t* const pValue) {
        *pValue = currentValue;
        return valueProviderReturnValue;
    }

    void processFor(ZG_handle_t channel, int ticks) {
        for (int i = 0; i < ticks; i++) {
            currentTick++;
            ZG_Process(channel);
        }
    }

    void SetUp() override {
        currentTick              = 0;
        currentValue             = 50;
        valueProviderReturnValue = true;

        ZG_Init(TickProvider);
    }
};

uint16_t   ZoneGaugeTest::currentTick;
ZG_value_t ZoneGaugeTest::currentValue;
bool       ZoneGaugeTest::valueProviderReturnValue;

TEST_F(ZoneGaugeTest, MultipleTransitionsRoundTrip) {
    const ZG_transitionRule_t transitionsRulesUp[] = {
        {35, ToVoidPtr(Zone::UnderHigh), 3},
        {55, ToVoidPtr(Zone::Normal), 3},
        {75, ToVoidPtr(Zone::OverLow), 3},
        {95, ToVoidPtr(Zone::OverHigh), 0},
    };

    const ZG_transitionRule_t transitionsRulesDown[] = {
        {25, ToVoidPtr(Zone::UnderLow), 3},
        {45, ToVoidPtr(Zone::UnderHigh), 3},
        {65, ToVoidPtr(Zone::Normal), 3},
        {85, ToVoidPtr(Zone::OverLow), 3},
    };

    const ZG_transitionRules_t upRules   = {transitionsRulesUp, 4};
    const ZG_transitionRules_t downRules = {transitionsRulesDown, 4};

    currentValue             = 20;
    const ZG_config_t config = {
        .valueProvider   = ValueProvider,
        .transitionRules = {.up = &upRules, .down = &downRules},
        .initial         = {.value = currentValue, .zone = ToVoidPtr(Zone::UnderLow)},
    };

    ZG_handle_t handle = ZG_Open(&config);
    EXPECT_EQ(ZG_GetZone(handle), Zone::UnderLow);

    // ===== UPWARD transitions =====
    currentValue = 36;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::UnderHigh);

    currentValue = 56;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    currentValue = 76;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::OverLow);

    currentValue = 96;
    processFor(handle, 1);
    EXPECT_EQ(ZG_GetZone(handle), Zone::OverHigh);

    // ===== DOWNWARD transitions =====
    currentValue = 84;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::OverLow);

    currentValue = 64;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    currentValue = 44;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::UnderHigh);

    currentValue = 24;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::UnderLow);

    ZG_Close(handle);
}

TEST_F(ZoneGaugeTest, NoTransitionsWithinHysteresisZone) {
    const ZG_transitionRule_t transitionsRulesUp[] = {
        {70, ToVoidPtr(Zone::OverLow), 5},
    };

    const ZG_transitionRule_t transitionsRulesDown[] = {
        {40, ToVoidPtr(Zone::UnderHigh), 5},
    };

    const ZG_transitionRules_t upRules   = {transitionsRulesUp, 1};
    const ZG_transitionRules_t downRules = {transitionsRulesDown, 1};

    const ZG_config_t config = {
        .valueProvider   = ValueProvider,
        .transitionRules = {.up = &upRules, .down = &downRules},
        .initial         = {.value = 50, .zone = ToVoidPtr(Zone::Normal)},
    };

    ZG_handle_t handle = ZG_Open(&config);

    // Increase the value at the upper threshold, but so it doesn't cross it
    currentValue = 70;
    processFor(handle, 10);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    // Value bounces back, but not crossing downward threshold either
    currentValue = 40;
    processFor(handle, 10);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    ZG_Close(handle);
}

TEST_F(ZoneGaugeTest, DebounceCancellationWithAdjacentZones) {
    const ZG_transitionRule_t transitionsRulesUp[] = {
        {55, ToVoidPtr(Zone::Normal), 5},
        {75, ToVoidPtr(Zone::OverLow), 5},
    };

    const ZG_transitionRule_t transitionsRulesDown[] = {
        {45, ToVoidPtr(Zone::UnderHigh), 5},
        {65, ToVoidPtr(Zone::Normal), 5},
    };

    const ZG_transitionRules_t upRules   = {transitionsRulesUp, 2};
    const ZG_transitionRules_t downRules = {transitionsRulesDown, 2};

    const ZG_config_t config = {
        .valueProvider   = ValueProvider,
        .transitionRules = {.up = &upRules, .down = &downRules},
        .initial         = {.value = 60, .zone = ToVoidPtr(Zone::Normal)},
    };

    ZG_handle_t handle = ZG_Open(&config);

    // ===== UPWARD transitions =====
    // Cross the upward threshold for zone Zone::OverLow
    currentValue = 76;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    // Value bounces back before debounce completes, debounce should be simply cancelled
    currentValue = 75;
    processFor(handle, 1);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    // Even after more ticks, should still be in the same zone
    processFor(handle, 10);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    // ===== DOWNWARD transitions =====
    // Cross the downward threshold for zone Zone::UnderHigh
    currentValue = 44;
    processFor(handle, 4);

    // Value bounces back before debounce completes, debounce should be simply cancelled
    currentValue = 45;
    processFor(handle, 1);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    // Even after more ticks, should still be in the same zone
    processFor(handle, 10);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);
}

TEST_F(ZoneGaugeTest, DebounceCancellationWithNonAdjacentZones) {
    const ZG_transitionRule_t transitionsRulesUp[] = {
        {35, ToVoidPtr(Zone::UnderHigh), 5},
        {55, ToVoidPtr(Zone::Normal), 5},
        {75, ToVoidPtr(Zone::OverLow), 5},
        {95, ToVoidPtr(Zone::OverHigh), 5},
    };

    const ZG_transitionRule_t transitionsRulesDown[] = {
        {25, ToVoidPtr(Zone::UnderLow), 5},
        {45, ToVoidPtr(Zone::UnderHigh), 5},
        {65, ToVoidPtr(Zone::Normal), 5},
        {85, ToVoidPtr(Zone::OverLow), 5},
    };

    const ZG_transitionRules_t upRules   = {transitionsRulesUp, 4};
    const ZG_transitionRules_t downRules = {transitionsRulesDown, 4};

    const ZG_config_t config = {
        .valueProvider   = ValueProvider,
        .transitionRules = {.up = &upRules, .down = &downRules},
        .initial         = {.value = 24, .zone = ToVoidPtr(Zone::UnderLow)},
    };

    ZG_handle_t handle = ZG_Open(&config);

    // ===== UPWARD transitions =====
    // Cross the upward threshold for zone Zone::OverHigh
    currentValue = 96;
    processFor(handle, 4);
    EXPECT_EQ(ZG_GetZone(handle), Zone::UnderLow);

    // Value bounces back before debounce completes, but debounce should be retriggered with next threshold crossing
    currentValue = 95;
    processFor(handle, 6);
    EXPECT_EQ(ZG_GetZone(handle), Zone::OverLow);

    // Even after more ticks, should still be in the same zone
    processFor(handle, 10);
    EXPECT_EQ(ZG_GetZone(handle), Zone::OverLow);

    // ===== DOWNWARD transitions =====
    // Cross the downward threshold for zone Zone::UnderLow
    currentValue = 24;
    processFor(handle, 4);

    // Value bounces back before debounce completes, but debounce should be retriggered with next threshold crossing
    currentValue = 25;
    processFor(handle, 6);
    EXPECT_EQ(ZG_GetZone(handle), Zone::UnderHigh);

    // Even after more ticks, should still be in the same zone
    processFor(handle, 10);
    EXPECT_EQ(ZG_GetZone(handle), Zone::UnderHigh);
}

TEST_F(ZoneGaugeTest, FreezeDebounceOnInvalidValue) {
    const ZG_transitionRule_t transitionsRulesUp[] = {
        {70, ToVoidPtr(Zone::OverLow), 5},
    };

    const ZG_transitionRule_t transitionsRulesDown[] = {
        {30, ToVoidPtr(Zone::Normal), 5},
    };

    const ZG_transitionRules_t upRules   = {transitionsRulesUp, 1};
    const ZG_transitionRules_t downRules = {transitionsRulesDown, 1};

    const ZG_config_t config = {
        .valueProvider   = ValueProvider,
        .transitionRules = {.up = &upRules, .down = &downRules},
        .initial         = {.value = 50, .zone = ToVoidPtr(Zone::Normal)},
    };

    ZG_handle_t handle = ZG_Open(&config);

    currentValue             = 50;
    valueProviderReturnValue = true;
    processFor(handle, 10);

    currentValue = 71;
    processFor(handle, 4);

    valueProviderReturnValue = false; // Simulate invalid value

    // Process should not change zone when value is invalid
    processFor(handle, 10);
    EXPECT_EQ(ZG_GetZone(handle), Zone::Normal);

    // Restore valid value and ensure it can transition
    valueProviderReturnValue = true;
    processFor(handle, 2);
    EXPECT_EQ(ZG_GetZone(handle), Zone::OverLow);

    ZG_Close(handle);
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
