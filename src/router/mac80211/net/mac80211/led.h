/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2006, Johannes Berg <johannes@sipsolutions.net>
 */
#ifndef LED_H
#define LED_H

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/leds.h>
#include "ieee80211_i.h"

#define MAC80211_BLINK_DELAY 50 /* ms */

static inline void ieee80211_led_rx(struct ieee80211_local *local)
{
#ifdef CPTCFG_MAC80211_LEDS
	unsigned long led_delay __maybe_unused = MAC80211_BLINK_DELAY;

	if (!atomic_read(&local->rx_led_active))
		return;
#if LINUX_VERSION_IS_GEQ(6,6,0)
	led_trigger_blink_oneshot(&local->rx_led, led_delay, led_delay, 0);
#elif LINUX_VERSION_IS_GEQ(3,6,0)
	led_trigger_blink_oneshot(&local->rx_led, &led_delay, &led_delay, 0);
#else
	if (local->rx_led_counter++ % 2 == 0)
		led_trigger_event(&local->rx_led, LED_OFF);
	else
		led_trigger_event(&local->rx_led, LED_FULL);
#endif
#endif
}

static inline void ieee80211_led_tx(struct ieee80211_local *local)
{
#ifdef CPTCFG_MAC80211_LEDS
	unsigned long led_delay __maybe_unused = MAC80211_BLINK_DELAY;

	if (!atomic_read(&local->tx_led_active))
		return;
#if LINUX_VERSION_IS_GEQ(6,6,0)
	led_trigger_blink_oneshot(&local->tx_led, led_delay, led_delay, 0);
#elif LINUX_VERSION_IS_GEQ(3,6,0)
	led_trigger_blink_oneshot(&local->tx_led, &led_delay, &led_delay, 0);
#else
	if (local->tx_led_counter++ % 2 == 0)
		led_trigger_event(&local->tx_led, LED_OFF);
	else
		led_trigger_event(&local->tx_led, LED_FULL);
#endif
#endif
}

#ifdef CPTCFG_MAC80211_LEDS
void ieee80211_led_assoc(struct ieee80211_local *local,
			 bool associated);
void ieee80211_led_radio(struct ieee80211_local *local,
			 bool enabled);
static void ieee80211_alloc_led_names(struct ieee80211_local *local);
static void ieee80211_free_led_names(struct ieee80211_local *local);
void ieee80211_led_init(struct ieee80211_local *local);
void ieee80211_led_exit(struct ieee80211_local *local);
static void ieee80211_mod_tpt_led_trig(struct ieee80211_local *local,
				unsigned int types_on, unsigned int types_off);
#else
static inline void ieee80211_led_assoc(struct ieee80211_local *local,
				       bool associated)
{
}
static inline void ieee80211_led_radio(struct ieee80211_local *local,
				       bool enabled)
{
}
static inline void ieee80211_alloc_led_names(struct ieee80211_local *local)
{
}
static inline void ieee80211_free_led_names(struct ieee80211_local *local)
{
}
static inline void ieee80211_led_init(struct ieee80211_local *local)
{
}
static inline void ieee80211_led_exit(struct ieee80211_local *local)
{
}
static inline void ieee80211_mod_tpt_led_trig(struct ieee80211_local *local,
					      unsigned int types_on,
					      unsigned int types_off)
{
}
#endif

static inline void
ieee80211_tpt_led_trig_tx(struct ieee80211_local *local, int bytes)
{
#ifdef CPTCFG_MAC80211_LEDS
	if (atomic_read(&local->tpt_led_active))
		local->tpt_led_trigger->tx_bytes += bytes;
#endif
}

static inline void
ieee80211_tpt_led_trig_rx(struct ieee80211_local *local, int bytes)
{
#ifdef CPTCFG_MAC80211_LEDS
	if (atomic_read(&local->tpt_led_active))
		local->tpt_led_trigger->rx_bytes += bytes;
#endif
}
#endif