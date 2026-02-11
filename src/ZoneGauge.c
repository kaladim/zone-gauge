/*!
 * \file    ZoneGauge.c
 * \brief   Implementation of the ZoneGauge.
 * \author  Kaloyan Dimitrov
 * \copyright Copyright (c) 2026 Kaloyan Dimitrov
 *            https://github.com/kaladim/zone-gauge
 *            SPDX-License-Identifier: MIT
 */
/******************************************************************************/
/*    Dependencies                                                            */
/******************************************************************************/
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "ZoneGauge.h"

/******************************************************************************/
/*    Private operations prototypes                                           */
/******************************************************************************/
static struct ZG_transition_t ZG_FindNearestThresholdCrossing(const struct ZG_instance_t* gauge,
                                                              const ZG_value_t            actualValue,
                                                              const ZG_value_t            oldValue);
static bool                   ZG_ShouldCancelDebounce(const struct ZG_instance_t* gauge,
                                                      const ZG_value_t            actualValue);
static bool                   ZG_ConfigValid(const struct ZG_config_t* config);
static bool                   ZG_TransitionRulesValid(const struct ZG_transitionRules_t* rules);

/******************************************************************************/
/*    Private global variables                                                */
/******************************************************************************/
static ZG_tickProvider_t ZG_tickProvider;

/******************************************************************************/
/*    Public operations                                                       */
/******************************************************************************/
void ZG_InitGlobal(ZG_tickProvider_t tickProvider) {
    assert(tickProvider != NULL);
    ZG_tickProvider = tickProvider;
}

void ZG_Init(struct ZG_instance_t* gauge, const struct ZG_config_t* config) {
    assert(gauge != NULL);
    assert(ZG_ConfigValid(config));

    gauge->last.value           = config->initial.value;
    gauge->last.transition.rule = NULL;
    gauge->zone                 = config->initial.zone;
    gauge->config               = config;
}

void ZG_Process(struct ZG_instance_t* gauge) {
    assert(gauge != NULL);

    ZG_value_t      actualValue  = {0};
    const ZG_tick_t currentTicks = ZG_tickProvider();

    if (gauge->config->valueProvider(&actualValue)) {
        const struct ZG_transition_t newTransition = ZG_FindNearestThresholdCrossing(gauge, actualValue, gauge->last.value);
        gauge->last.value                          = actualValue;

        if (newTransition.rule == NULL) {
            /* No transition detected; check for debounce cancelation conditions */
            if (ZG_ShouldCancelDebounce(gauge, actualValue)) {
                gauge->last.transition = ZG_FindNearestThresholdCrossing(gauge, actualValue, gauge->last.transition.oldValue);
            }
        } else {
            /* New transition, start debounce */
            gauge->last.transition    = newTransition;
            gauge->debounceStartTicks = currentTicks;
        }

        /* Check debounce period expiration */
        if ((gauge->last.transition.rule != NULL) &&
            ((currentTicks - gauge->debounceStartTicks) >= gauge->last.transition.rule->debounceTicks)) {
            /* Debounce expired, confirm the zone */
            gauge->zone                 = gauge->last.transition.rule->targetZone;
            gauge->last.transition.rule = NULL;
        }
    } else {
        // Increment the debounce time due to invalid value
        gauge->debounceStartTicks += (currentTicks - gauge->last.ticks);
    }

    gauge->last.ticks = currentTicks;
}

void* ZG_GetZone(struct ZG_instance_t* gauge) {
    assert(gauge != NULL);
    return gauge->zone;
}

/******************************************************************************/
/*    Private operations                                                      */
/******************************************************************************/
/// \brief     Searches for nearest to 'actualValue' upward transition.
/// \param[in] gauge pointer to channel instance
/// \param[in] actualValue actual value read from the provider
/// \param[in] oldValue previous value read from the provider
/// \return    Transition descriptor
static inline struct ZG_transition_t ZG_FindNearestUpwardThresholdCrossing(const struct ZG_instance_t* gauge,
                                                                           const ZG_value_t            actualValue,
                                                                           const ZG_value_t            oldValue) {
    const struct ZG_transitionRules_t* rules = gauge->config->transitionRules.up;

    for (int16_t i = ((int16_t)rules->count - 1); i >= 0; i--) {
        if ((actualValue > rules->items[i].threshold) && (oldValue <= rules->items[i].threshold)) {
            return (struct ZG_transition_t){.rule = &rules->items[i], .oldValue = oldValue, .upward = true};
        }
    }

    return (struct ZG_transition_t){0};
}

/// \brief     Searches for nearest to 'actualValue' downward transition.
/// \param[in] gauge pointer to channel instance
/// \param[in] actualValue actual value read from the provider
/// \param[in] oldValue previous value read from the provider
/// \return    Transition descriptor
static inline struct ZG_transition_t ZG_FindNearestDownwardThresholdCrossing(const struct ZG_instance_t* gauge,
                                                                             const ZG_value_t            actualValue,
                                                                             const ZG_value_t            oldValue) {
    const struct ZG_transitionRules_t* rules = gauge->config->transitionRules.down;

    for (uint8_t i = 0U; i < rules->count; i++) {
        if ((actualValue < rules->items[i].threshold) && (oldValue >= rules->items[i].threshold)) {
            return (struct ZG_transition_t){.rule = &rules->items[i], .oldValue = oldValue, .upward = false};
        }
    }

    return (struct ZG_transition_t){0};
}

/// @brief Finds the nearest threshold crossing in a range, defined by actual and old values.
/// \param[in] gauge pointer to channel instance
/// \param[in] actualValue latest value
/// \param[in] oldValue previous value
/// \return    Transition descriptor. It will have its .rule = NULL if a transition could not be found
static struct ZG_transition_t ZG_FindNearestThresholdCrossing(const struct ZG_instance_t* gauge,
                                                              const ZG_value_t            actualValue,
                                                              const ZG_value_t            oldValue) {
    if (actualValue > oldValue) {
        return ZG_FindNearestUpwardThresholdCrossing(gauge, actualValue, oldValue);
    } else if (actualValue < oldValue) {
        return ZG_FindNearestDownwardThresholdCrossing(gauge, actualValue, oldValue);
    }
    return (struct ZG_transition_t){0}; /* No crossing found */
}

/// \brief     Predicate for debounce cancelation.
/// \param[in] gauge pointer to channel instance
/// \param[in] actualValue latest value
/// \retval    false if debounce should not be cancelled
/// \retval    true if debounce should be cancelled
static bool ZG_ShouldCancelDebounce(const struct ZG_instance_t* gauge, const ZG_value_t actualValue) {
    const struct ZG_transition_t* transition = &gauge->last.transition;

    return ((transition->rule != NULL) && ((transition->upward && (actualValue <= transition->rule->threshold)) ||
                                           (!transition->upward && (actualValue >= transition->rule->threshold))));
}

/// @brief  Configuration validator
/// @param[in] config pointer to configuration object
/// @retval true if valid
/// @retval false if invalid
static bool ZG_ConfigValid(const struct ZG_config_t* config) {
    if (config == NULL)
        return false;

    if (config->valueProvider == NULL)
        return false;

    if (!ZG_TransitionRulesValid(config->transitionRules.up))
        return false;

    if (!ZG_TransitionRulesValid(config->transitionRules.down))
        return false;

    return true;
}

/// @brief  Transition rule set validator
/// @param[in] rules pointer to rules table
/// @retval true if valid
/// @retval false if invalid
static bool ZG_TransitionRulesValid(const struct ZG_transitionRules_t* rules) {
    if (rules == NULL)
        return false;

    if (rules->items == NULL)
        return false;

    if (rules->count == 0)
        return false;

    if (rules->count > 1) {
        for (uint8_t i = 0U; i < (rules->count - 1U); i++) {
            if (rules->items[i].threshold >= rules->items[i + 1].threshold) {
                return false; // Thresholds must be in ascending order!
            }
        }
    }

    return true;
}
