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
#include <array>
#include <cstdint>
#include <vector>
#include <gtest/gtest.h>
#include "ZoneGauge.hpp"

// Zones
enum class Zone {
    UnderLow  = 1,
    UnderHigh = 2,
    Normal    = 3,
    OverLow   = 4,
    OverHigh  = 5
};

// Type aliases
using namespace zonegauge;
using TestValueType = int;
using TestTickType  = uint16_t;

// Tick provider function
static TestTickType currentTick = 0;

TestTickType tickProvider() {
    return currentTick;
}

// Value provider function
static TestValueType currentValue = 50;
static bool          valueValid   = true;

std::optional<TestValueType> valueProvider() {
    if (valueValid) {
        return currentValue;
    }
    return std::nullopt;
}

using TestZoneGauge = ZoneGauge<Zone, TestValueType, TestTickType, &tickProvider>;

// Test fixture
class ZoneGaugeTest : public ::testing::Test {
  protected:
    void SetUp() override {
        currentTick  = 0;
        currentValue = 50;
        valueValid   = true;
    }
    void processFor(TestZoneGauge& gauge, int ticks) {
        for (int i = 0; i < ticks; i++) {
            currentTick++;
            gauge.process();
        }
    }
};

TEST_F(ZoneGaugeTest, MultipleTransitionsRoundTrip) {
    constexpr std::array upRules{
        TestZoneGauge::TransitionRule{35, Zone::UnderHigh, 3},
        TestZoneGauge::TransitionRule{55, Zone::Normal, 3},
        TestZoneGauge::TransitionRule{75, Zone::OverLow, 3},
        TestZoneGauge::TransitionRule{95, Zone::OverHigh, 0},
    };

    constexpr std::array downRules{
        TestZoneGauge::TransitionRule{25, Zone::UnderLow, 3},
        TestZoneGauge::TransitionRule{45, Zone::UnderHigh, 3},
        TestZoneGauge::TransitionRule{65, Zone::Normal, 3},
        TestZoneGauge::TransitionRule{85, Zone::OverLow, 3},
    };

    currentValue = 20;
    const TestZoneGauge::Config config{.valueProvider = valueProvider,
                                       .transitionRules{.up = upRules, .down = downRules},
                                       .initial{.value = currentValue, .zone = Zone::UnderLow}};

    TestZoneGauge gauge(config);

    EXPECT_EQ(gauge.getZone(), Zone::UnderLow);

    // ===== UPWARD transitions =====
    currentValue = 36;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::UnderHigh);

    currentValue = 56;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);

    currentValue = 76;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::OverLow);

    currentValue = 96;
    processFor(gauge, 1);
    EXPECT_EQ(gauge.getZone(), Zone::OverHigh);

    // ===== DOWNWARD transitions =====
    currentValue = 84;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::OverLow);

    currentValue = 64;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);

    currentValue = 44;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::UnderHigh);

    currentValue = 24;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::UnderLow);
}

TEST_F(ZoneGaugeTest, NoTransitionsWithinHysteresisZone) {
    constexpr std::array upRules{
        TestZoneGauge::TransitionRule{70, Zone::OverLow, 5},
    };

    constexpr std::array downRules{
        TestZoneGauge::TransitionRule{40, Zone::UnderHigh, 5},
    };

    const TestZoneGauge::Config config{.valueProvider   = valueProvider,
                                       .transitionRules = {.up = upRules, .down = downRules},
                                       .initial         = {.value = 50, .zone = Zone::Normal}};

    TestZoneGauge gauge(config);

    // Increase the value at the upper threshold, but so it doesn't cross it
    currentValue = 70;
    processFor(gauge, 10);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);

    // Value bounces back, but not crossing downward threshold either
    currentValue = 40;
    processFor(gauge, 10);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);
}

TEST_F(ZoneGaugeTest, DebounceCancellationWithAdjacentZones) {
    constexpr std::array upRules{
        TestZoneGauge::TransitionRule{55, Zone::Normal, 5},
        TestZoneGauge::TransitionRule{75, Zone::OverLow, 5},
    };

    constexpr std::array downRules{
        TestZoneGauge::TransitionRule{45, Zone::UnderHigh, 5},
        TestZoneGauge::TransitionRule{65, Zone::Normal, 5},
    };

    const TestZoneGauge::Config config{.valueProvider   = valueProvider,
                                       .transitionRules = {.up = upRules, .down = downRules},
                                       .initial         = {.value = 60, .zone = Zone::Normal}};

    TestZoneGauge gauge(config);

    // ===== UPWARD transitions =====
    // Cross the upward threshold for zone Zone::OverLow
    currentValue = 76;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);

    // Value bounces back before debounce completes, debounce should be simply cancelled
    currentValue = 75;
    processFor(gauge, 1);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);

    // Even after more ticks, should still be in the same zone
    processFor(gauge, 10);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);

    // ===== DOWNWARD transitions =====
    // Cross the downward threshold for zone Zone::UnderHigh
    currentValue = 44;
    processFor(gauge, 4);

    // Value bounces back before debounce completes, debounce should be simply cancelled
    currentValue = 45;
    processFor(gauge, 1);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);

    // Even after more ticks, should still be in the same zone
    processFor(gauge, 10);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);
}

TEST_F(ZoneGaugeTest, DebounceCancellationWithNonAdjacentZones) {
    constexpr std::array upRules{
        TestZoneGauge::TransitionRule{35, Zone::UnderHigh, 5},
        TestZoneGauge::TransitionRule{55, Zone::Normal, 5},
        TestZoneGauge::TransitionRule{75, Zone::OverLow, 5},
        TestZoneGauge::TransitionRule{95, Zone::OverHigh, 5},
    };

    constexpr std::array downRules{
        TestZoneGauge::TransitionRule{25, Zone::UnderLow, 5},
        TestZoneGauge::TransitionRule{45, Zone::UnderHigh, 5},
        TestZoneGauge::TransitionRule{65, Zone::Normal, 5},
        TestZoneGauge::TransitionRule{85, Zone::OverLow, 5},
    };

    const TestZoneGauge::Config config{.valueProvider   = valueProvider,
                                       .transitionRules = {.up = upRules, .down = downRules},
                                       .initial         = {.value = 24, .zone = Zone::UnderLow}};

    TestZoneGauge gauge(config);

    // ===== UPWARD transitions =====
    // Cross the upward threshold for zone Zone::OverHigh
    currentValue = 96;
    processFor(gauge, 4);
    EXPECT_EQ(gauge.getZone(), Zone::UnderLow);

    // Value bounces back before debounce completes, but debounce should be retriggered with next threshold crossing
    currentValue = 95;
    processFor(gauge, 6);
    EXPECT_EQ(gauge.getZone(), Zone::OverLow);

    // Even after more ticks, should still be in the same zone
    processFor(gauge, 10);
    EXPECT_EQ(gauge.getZone(), Zone::OverLow);

    // ===== DOWNWARD transitions =====
    // Cross the downward threshold for zone Zone::UnderLow
    currentValue = 24;
    processFor(gauge, 4);

    // Value bounces back before debounce completes, but debounce should be retriggered with next threshold crossing
    currentValue = 25;
    processFor(gauge, 6);

    EXPECT_EQ(gauge.getZone(), Zone::UnderHigh);

    // Even after more ticks, should still be in the same zone
    processFor(gauge, 10);
    EXPECT_EQ(gauge.getZone(), Zone::UnderHigh);
}

TEST_F(ZoneGaugeTest, FreezeDebounceOnInvalidValue) {
    constexpr std::array upRules{
        TestZoneGauge::TransitionRule{70, Zone::OverLow, 5},
    };

    constexpr std::array downRules{
        TestZoneGauge::TransitionRule{30, Zone::UnderHigh, 5},
    };

    const TestZoneGauge::Config config{.valueProvider   = valueProvider,
                                       .transitionRules = {.up = upRules, .down = downRules},
                                       .initial         = {.value = 50, .zone = Zone::Normal}};

    TestZoneGauge gauge(config);

    currentValue = 50;
    valueValid   = true;
    processFor(gauge, 10);

    currentValue = 71;
    processFor(gauge, 4);

    valueValid = false; // Simulate invalid value

    // Process should not change zone when value is invalid
    processFor(gauge, 10);
    EXPECT_EQ(gauge.getZone(), Zone::Normal);

    // Restore valid value and ensure it can transition
    valueValid = true;
    processFor(gauge, 2);
    EXPECT_EQ(gauge.getZone(), Zone::OverLow);
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
