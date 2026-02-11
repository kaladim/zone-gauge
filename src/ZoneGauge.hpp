/*!
 * \file    ZoneGauge.hpp
 * \brief   ZoneGauge header-only library.
 * \author  Kaloyan Dimitrov
 * \copyright Copyright (c) 2026 Kaloyan Dimitrov
 *            https://github.com/kaladim/zone-gauge
 *            SPDX-License-Identifier: MIT
 */
#ifndef ZONEGAUGE_HPP
#define ZONEGAUGE_HPP

/******************************************************************************/
/*    Dependencies                                                            */
/******************************************************************************/
#include <cassert>
#include <functional>
#include <optional>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>

namespace zonegauge {

/// @brief ZoneGauge class template.
/// @tparam ZoneType Type representing the zone.
/// @tparam ValueType Type representing the measured value.
/// @tparam TickType Type representing the tick count.
/// @tparam TickProvider Pointer to a function that provides the current tick count.
template <typename ZoneType, typename ValueType, typename TickType, TickType (*TickProvider)()>
    requires std::is_arithmetic_v<ValueType> and std::is_integral_v<TickType> and (not std::is_same_v<TickType, bool>) and
             (TickProvider != nullptr)
class ZoneGauge {
  public:
    /// @brief  Rule for zone change.
    struct TransitionRule {
        ValueType threshold;     /**< Threshold for starting a debounce. Strict comparison is used (< or >) */
        ZoneType  targetZone;    /**< Target zone after threshold is crossed and debounce completes. */
        TickType  debounceTicks; /**< Debounce ticks for confirming a stable 'targetZone'. */
    };

    /// @brief  Configuration
    struct Config {
        std::function<std::optional<ValueType>()> valueProvider;

        struct {
            std::span<const TransitionRule> up;
            std::span<const TransitionRule> down;
        } transitionRules;

        struct {
            ValueType value;
            ZoneType  zone;
        } initial;
    };

    /// @brief  Constructor
    /// @param config instance configuration. Must remain valid for the entire lifetime of the instance.
    explicit ZoneGauge(const Config& config)
        : _config{config},
          _zone{config.initial.zone},
          _debounceStartTicks{} {

        assert(configValid(config));

        _last.value      = config.initial.value;
        _last.transition = std::nullopt;
        _last.ticks      = 0;
    }

    /// @brief Default destructor
    ~ZoneGauge() = default;

    /// @brief Executes the business logic.
    /// @pre Should be called periodically.
    void process() {
        const auto     actualValueOpt = _config.valueProvider();
        const TickType currentTicks   = TickProvider();

        if (actualValueOpt.has_value()) {
            const ValueType actualValue   = actualValueOpt.value();
            const auto      newTransition = findNearestThresholdCrossing(actualValue, _last.value);
            _last.value                   = actualValue;

            if (newTransition.has_value()) {
                /* New transition, start debounce */
                _last.transition    = newTransition.value();
                _debounceStartTicks = currentTicks;
            }
            else {
                /* No transition detected; check for ongoing debounce cancellation conditions */
                if (shouldCancelDebounce(actualValue)) {
                    _last.transition = findNearestThresholdCrossing(actualValue, _last.transition.value().oldValue);
                }
            }

            /* Check debounce period expiration */
            if (_last.transition.has_value()) {
                if ((currentTicks - _debounceStartTicks) >= _last.transition.value().rule->debounceTicks) {
                    /* Debounce expired, confirm the zone */
                    _zone = _last.transition.value().rule->targetZone;
                    _last.transition.reset();
                }
            }
        }
        else {
            // Increment the debounce time due to invalid value
            _debounceStartTicks += (currentTicks - _last.ticks);
        }

        _last.ticks = currentTicks;
    }

    /// @brief Gets the confirmed zone
    /// @return Confirmed zone.
    ZoneType getZone() const {
        return _zone;
    }

  private:
    struct Transition {
        const TransitionRule* rule;     //< Transition rule
        ValueType             oldValue; //< Old value at the start of the transition
        bool                  upward;   //< Direction of the transition (true for upward, false for downward)
    };

    /// @brief Debounce cancellation predicate.
    /// @param actualValue actual value read from the provider.
    /// @return true if the current debounce should be cancelled, false otherwise.
    bool shouldCancelDebounce(const ValueType actualValue) const {
        if (not _last.transition.has_value()) {
            return false; /* No active debounce, nothing to cancel */
        }

        const Transition& transition = _last.transition.value();

        return ((transition.upward and (actualValue <= transition.rule->threshold)) or
                (not transition.upward and (actualValue >= transition.rule->threshold)));
    }

    /// @brief Finds the nearest threshold crossing in a range, defined by actual and old values.
    /// @param actualValue latest value
    /// @param oldValue previous value
    /// @return Transition details if a crossing is detected, std::nullopt otherwise.
    std::optional<Transition> findNearestThresholdCrossing(const ValueType actualValue, const ValueType oldValue) const {
        if (actualValue > oldValue) {
            return findNearestUpwardThresholdCrossing(actualValue, oldValue);
        }
        else if (actualValue < oldValue) {
            return findNearestDownwardThresholdCrossing(actualValue, oldValue);
        }

        return std::nullopt; /* No crossing found */
    }

    inline std::optional<Transition> findNearestUpwardThresholdCrossing(const ValueType actualValue, const ValueType oldValue) const {
        for (const auto& rule : _config.transitionRules.up | std::views::reverse) {
            if ((actualValue > rule.threshold) and (oldValue <= rule.threshold)) {
                return Transition{&rule, oldValue, true};
            }
        }

        return std::nullopt;
    }

    inline std::optional<Transition> findNearestDownwardThresholdCrossing(const ValueType actualValue, const ValueType oldValue) const {
        for (const auto& rule : _config.transitionRules.down) {
            if ((actualValue < rule.threshold) and (oldValue >= rule.threshold)) {
                return Transition{&rule, oldValue, false};
            }
        }

        return std::nullopt;
    }

    /// @brief Configuration validator.
    /// @param config reference to the configuration object.
    /// @return true if the configuration is valid, false otherwise.
    static bool configValid(const Config& config) {
        if (config.valueProvider == nullptr)
            return false;

        if (not transitionRulesValid(config.transitionRules.up))
            return false;

        if (not transitionRulesValid(config.transitionRules.down))
            return false;

        return true;
    }

    /// @brief Transition rules validator.
    /// @param rules reference to the transitions collection.
    /// @return true if the transition rules are valid, false otherwise.
    static bool transitionRulesValid(const std::span<const TransitionRule>& rules) {
        return std::ranges::adjacent_find(rules, std::greater_equal{}, &TransitionRule::threshold) == rules.end();
    }

    const Config& _config;
    ZoneType      _zone;
    TickType      _debounceStartTicks;

    struct {
        TickType                  ticks;
        ValueType                 value;
        std::optional<Transition> transition;
    } _last;
};

} // namespace zonegauge
#endif
