/*!
 * \file    ZoneGauge.h
 * \brief   Interface of the ZoneGauge library.
 * \author  Kaloyan Dimitrov
 * \copyright Copyright (c) 2026 Kaloyan Dimitrov
 *            https://github.com/kaladim/zone-gauge
 *            SPDX-License-Identifier: MIT
 */
#ifndef ZONEGAUGE_H
#define ZONEGAUGE_H

/******************************************************************************/
/*    Dependencies                                                            */
/******************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <ZG_Config.h>

/******************************************************************************/
/*    Provided types                                                          */
/******************************************************************************/
/// @brief      Function providing a monotonic tick/timestamp value
/// @retval     Current tick count.
typedef ZG_tick_t (*ZG_tickProvider_t)(void);

/// @brief      Function, providing a monitored value
/// @param[out] dest pointer to a value.
/// @retval     true if the value is valid
/// @retval     false if the value is invalid/outdated/not available
typedef bool (*ZG_valueProvider_t)(ZG_value_t* const dest);

/// @brief  Rule for zone change.
struct ZG_transitionRule_t {
    ZG_value_t threshold;     //< Threshold for starting a debounce. Strict comparison is used (< or >)
    void*      targetZone;    //< Target zone after threshold is crossed and debounce completes.
    ZG_tick_t  debounceTicks; //< Debounce ticks for confirming a stable 'targetZone'.
};

/// @brief  Transition rule set
struct ZG_transitionRules_t {
    const struct ZG_transitionRule_t* items; //< Array of rules. Must be sorted by thresholds, in ascending order!
    uint8_t                           count; //< Count of rules
};

/// @brief  Gauge configuration
struct ZG_config_t {
    ZG_valueProvider_t valueProvider;

    struct {
        const struct ZG_transitionRules_t* up;
        const struct ZG_transitionRules_t* down;
    } transitionRules;

    struct {
        ZG_value_t value;
        void*      zone;
    } initial;
};

/// @brief  Pending zone transition state. Implementation detail — do not access fields directly.
struct ZG_transition_t {
    const struct ZG_transitionRule_t* rule;     //< Active rule; NULL when no transition is pending
    ZG_value_t                        oldValue; //< Value at the start of the transition
    bool                              upward;   //< Direction: true = upward, false = downward
};

/// @brief  Gauge instance layout
/// @note   Fields are implementation details — do not access them directly.
struct ZG_instance_t {
    const struct ZG_config_t* config; //< Pointer to configuration struct
    void*                     zone;   //< Confirmed zone

    struct {
        ZG_tick_t              ticks;      //< Tick value at last execution
        ZG_value_t             value;      //< Monitored value at last execution
        struct ZG_transition_t transition; //< Pending transition
    } last;

    ZG_tick_t debounceStartTicks; //< Tick value at start of debounce
};

/******************************************************************************/
/*    Provided operations                                                     */
/******************************************************************************/
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif

/// @brief Global initializer.
/// @pre Must be called once before initializing any gauges.
/// @param tickProvider Function providing a monotonic tick/timestamp value. Must not be NULL!
EXTERN_C void ZG_InitGlobal(ZG_tickProvider_t tickProvider);

/// @brief Initializes a gauge with the specified configuration.
/// @param gauge  Pointer to a caller-allocated instance. Must not be NULL.
/// @param config Pointer to gauge configuration. Must not be NULL.
/// @pre The configuration object's lifetime must exceed the context's lifetime.
EXTERN_C void ZG_Init(struct ZG_instance_t* gauge, const struct ZG_config_t* config);

/// @brief Executes a gauge's business logic.
/// @pre Should be called periodically, with a frequency depending on the monitored value's dynamics and the debounce requirements.
/// @param gauge Pointer to a caller-allocated instance
EXTERN_C void ZG_Process(struct ZG_instance_t* gauge);

/// @brief Gets the confirmed zone of a gauge.
/// @param gauge Pointer to a caller-allocated instance
/// @return Confirmed zone. The actual type should match the one used in the gauge's configuration.
EXTERN_C void* ZG_GetZone(struct ZG_instance_t* gauge);

#endif
