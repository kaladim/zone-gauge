/*!
 * \file    ZG_Config.h
 * \brief   Static configuration of the ZoneGauge library.
 * \author  Kaloyan Dimitrov
 * \copyright Copyright (c) 2026 Kaloyan Dimitrov
 *            https://github.com/kaladim/zone-gauge
 *            SPDX-License-Identifier: MIT
 */
#ifndef ZG_CONFIG_H
#define ZG_CONFIG_H

#include <stdint.h>

/// @brief Monitored value type. Can be any integer or a floating-point type.
/// @note Applies to all channels.
typedef uint16_t ZG_value_t;

/// @brief Tick/timestamp type. Can be unsigned integer of any length.
typedef uint16_t ZG_tick_t;

#endif // ZG_CONFIG_H
