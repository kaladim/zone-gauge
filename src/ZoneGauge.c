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
/*    Types                                                                   */
/******************************************************************************/
struct ZG_transition_t {
    const struct ZG_transitionRule_t* rule;     //< Transition rule
    ZG_value_t                        oldValue; //< Old value at the start of the transition
    bool                              upward;   //< Direction of the transition (true for upward, false for downward)
};

/** Runtime channel context */
struct ZG_context_t {
    const struct ZG_config_t* config;
    void*                     zone;

    struct {
        ZG_tick_t              ticks;
        ZG_value_t             value;
        struct ZG_transition_t transition;
    } last;

    bool      inUse;
    ZG_tick_t debounceStartTicks;
};

/******************************************************************************/
/*    Private operations prototypes                                           */
/******************************************************************************/
static void                   ZG_InitChannel(struct ZG_context_t*      ctx,
                                             const struct ZG_config_t* config);
static struct ZG_transition_t ZG_FindNearestThresholdCrossing(const struct ZG_context_t* ctx,
                                                              const ZG_value_t           actualValue,
                                                              const ZG_value_t           oldValue);
static bool                   ZG_ShouldCancelDebounce(const struct ZG_context_t* ctx,
                                                      const ZG_value_t           actualValue);
static bool                   ZG_ConfigValid(const struct ZG_config_t* config);
static bool                   ZG_TransitionRulesValid(const struct ZG_transitionRules_t* rules);

/******************************************************************************/
/*    Private global variables                                                */
/******************************************************************************/
static struct ZG_context_t ZG_contexts[ZG_CHANNEL_COUNT];
static ZG_tickProvider_t   ZG_tickProvider;

/******************************************************************************/
/*    Public operations                                                       */
/******************************************************************************/
void ZG_Init(ZG_tickProvider_t tickProvider) {
    assert((ZG_CHANNEL_COUNT > 0) && (ZG_CHANNEL_COUNT <= 256));
    assert(tickProvider != NULL);

    ZG_tickProvider = tickProvider;
}

ZG_handle_t ZG_Open(const struct ZG_config_t* config) {
    assert(ZG_ConfigValid(config));

    for (ZG_handle_t i = 0; i < ZG_CHANNEL_COUNT; i++) {
        if (!ZG_contexts[i].inUse) {
            ZG_contexts[i].inUse = true;
            ZG_InitChannel(&ZG_contexts[i], config);
            return i;
        }
    }

    assert(false); // All channels are already in use, increase ZG_CHANNEL_COUNT!
    return 0;
}

void ZG_Close(ZG_handle_t channel) {
    assert(channel < ZG_CHANNEL_COUNT);

    ZG_contexts[channel].inUse = false;
}

void ZG_Process(ZG_handle_t channel) {
    assert(channel < ZG_CHANNEL_COUNT);
    assert(ZG_contexts[channel].inUse);

    struct ZG_context_t* ctx          = &ZG_contexts[channel];
    ZG_value_t           actualValue  = {0};
    const ZG_tick_t      currentTicks = ZG_tickProvider();

    if (ctx->config->valueProvider(&actualValue)) {
        const struct ZG_transition_t newTransition = ZG_FindNearestThresholdCrossing(ctx, actualValue, ctx->last.value);
        ctx->last.value                            = actualValue;

        if (newTransition.rule == NULL) {
            /* No transition detected; check for debounce cancelation conditions */
            if (ZG_ShouldCancelDebounce(ctx, actualValue)) {
                ctx->last.transition = ZG_FindNearestThresholdCrossing(ctx, actualValue, ctx->last.transition.oldValue);
            }
        } else {
            /* New transition, start debounce */
            ctx->last.transition    = newTransition;
            ctx->debounceStartTicks = currentTicks;
        }

        /* Check debounce period expiration */
        if ((ctx->last.transition.rule != NULL) &&
            ((currentTicks - ctx->debounceStartTicks) >= ctx->last.transition.rule->debounceTicks)) {
            /* Debounce expired, confirm the zone */
            ctx->zone                 = ctx->last.transition.rule->targetZone;
            ctx->last.transition.rule = NULL;
        }
    } else {
        // Increment the debounce time due to invalid value
        ctx->debounceStartTicks += (currentTicks - ctx->last.ticks);
    }

    ctx->last.ticks = currentTicks;
}

void* ZG_GetZone(ZG_handle_t channel) {
    assert(channel < ZG_CHANNEL_COUNT);
    assert(ZG_contexts[channel].inUse);

    return ZG_contexts[channel].zone;
}

/******************************************************************************/
/*    Private operations                                                      */
/******************************************************************************/
/// \brief     Initializes a channel.
/// \param[in/out] ctx pointer to channel context
/// \param[in] config pointer to channel configuration
static void ZG_InitChannel(struct ZG_context_t* ctx, const struct ZG_config_t* config) {
    ctx->last.value           = config->initial.value;
    ctx->last.transition.rule = NULL;

    ctx->zone   = config->initial.zone;
    ctx->config = config;
}

/// \brief     Searches for nearest to 'actualValue' upward transition.
/// \param[in] ctx pointer to channel context
/// \param[in] actualValue actual value read from the provider
/// \param[in] oldValue previous value read from the provider
/// \return    Transition descriptor
static inline struct ZG_transition_t ZG_FindNearestUpwardThresholdCrossing(const struct ZG_context_t* ctx,
                                                                           const ZG_value_t           actualValue,
                                                                           const ZG_value_t           oldValue) {
    const struct ZG_transitionRules_t* rules = ctx->config->transitionRules.up;

    for (int16_t i = ((int16_t)rules->count - 1); i >= 0; i--) {
        if ((actualValue > rules->items[i].threshold) && (oldValue <= rules->items[i].threshold)) {
            return (struct ZG_transition_t){.rule = &rules->items[i], .oldValue = oldValue, .upward = true};
        }
    }

    return (struct ZG_transition_t){0};
}

/// \brief     Searches for nearest to 'actualValue' downward transition.
/// \param[in] ctx pointer to channel context
/// \param[in] actualValue actual value read from the provider
/// \param[in] oldValue previous value read from the provider
/// \return    Transition descriptor
static inline struct ZG_transition_t ZG_FindNearestDownwardThresholdCrossing(const struct ZG_context_t* ctx,
                                                                             const ZG_value_t           actualValue,
                                                                             const ZG_value_t           oldValue) {
    const struct ZG_transitionRules_t* rules = ctx->config->transitionRules.down;

    for (uint8_t i = 0U; i < rules->count; i++) {
        if ((actualValue < rules->items[i].threshold) && (oldValue >= rules->items[i].threshold)) {
            return (struct ZG_transition_t){.rule = &rules->items[i], .oldValue = oldValue, .upward = false};
        }
    }

    return (struct ZG_transition_t){0};
}

/// @brief Finds the nearest threshold crossing in a range, defined by actual and old values.
/// \param[in] ctx pointer to channel context
/// \param[in] actualValue latest value
/// \param[in] oldValue previous value
/// \return    Transition descriptor. It will have its .rule = NULL if a transition could not be found
static struct ZG_transition_t ZG_FindNearestThresholdCrossing(const struct ZG_context_t* ctx,
                                                              const ZG_value_t           actualValue,
                                                              const ZG_value_t           oldValue) {
    if (actualValue > oldValue) {
        return ZG_FindNearestUpwardThresholdCrossing(ctx, actualValue, oldValue);
    } else if (actualValue < oldValue) {
        return ZG_FindNearestDownwardThresholdCrossing(ctx, actualValue, oldValue);
    }
    return (struct ZG_transition_t){0}; /* No crossing found */
}

/// \brief     Predicate for debounce cancelation.
/// \param[in] ctx pointer to channel context
/// \param[in] actualValue latest value
/// \retval    false if debounce should not be cancelled
/// \retval    true if debounce should be cancelled
static bool ZG_ShouldCancelDebounce(const struct ZG_context_t* ctx, const ZG_value_t actualValue) {
    const struct ZG_transition_t* transition = &ctx->last.transition;

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
