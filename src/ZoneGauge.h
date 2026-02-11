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

/// @brief Channel identifier
typedef uint8_t ZG_handle_t;

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

/// @brief  Channel configuration
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

/******************************************************************************/
/*    Provided operations                                                     */
/******************************************************************************/
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif

/// @brief Global initializer.
/// @pre Must be called once before opening any channels.
/// @param tickProvider Function providing a monotonic tick/timestamp value. Must not be NULL!
EXTERN_C void ZG_Init(ZG_tickProvider_t tickProvider);

/// @brief Opens a channel with the specified configuration.
/// @param config pointer to channel configuration. Must be non-null.
/// @pre The configuration object's lifetime must be longer than the channel's lifetime.
/// @return Handle to the opened channel.
EXTERN_C ZG_handle_t ZG_Open(const struct ZG_config_t* config);

/// @brief Closes a channel.
/// @param channel Handle to an opened channel.
EXTERN_C void ZG_Close(ZG_handle_t channel);

/// @brief Executes a channel's business logic.
/// @pre Should be called periodically, with a frequency, depending on the monitored value's dynamics and the debounce requirements.
/// @param channel Handle to an opened channel.
EXTERN_C void ZG_Process(ZG_handle_t channel);

/// @brief Gets the confirmed zone of a channel.
/// @param channel Handle to an opened channel.
/// @return Confirmed zone. The actual type should match the one used in the channel's configuration.
EXTERN_C void* ZG_GetZone(ZG_handle_t channel);

#endif
