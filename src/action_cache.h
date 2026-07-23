/********************************************************************************
 * action_cache.h — semantic offline action cache (F5).
 *
 * When there is no parent (offline), gesture actions are cached *semantically*
 * rather than replayed verbatim, so a whole offline session flushes as a handful
 * of messages once the network returns:
 *   - On/Off : a single pending toggle intent.
 *   - Level  : one signed accumulator (level units) -> one Step on flush.
 *   - Colour temperature : one signed accumulator (mireds) -> one Step on flush.
 *   - Published gesture actions : queued in order (informational), oldest dropped
 *     past ACTION_CACHE_MAX.
 * This module only stores state; the flush (which sends) lives in zigbee_app.c
 * where the cluster senders are.
 ********************************************************************************/
#ifndef ACTION_CACHE_H
#define ACTION_CACHE_H

#include "tl_common.h"

void action_cache_toggle(void);          /* remember a pending On/Off toggle    */
void action_cache_level(s32 units);      /* accumulate signed level units        */
void action_cache_ct(s32 mireds);        /* accumulate signed colour-temp mireds */
void action_cache_push_action(u16 code); /* queue a published action code        */
void action_cache_clear(void);
bool action_cache_empty(void);

/* Accessors used by the flush in zigbee_app.c. */
bool action_cache_toggle_pending(void);
s32  action_cache_level_get(void);
s32  action_cache_ct_get(void);
u8   action_cache_action_count(void);
u16  action_cache_action_at(u8 i);

#endif /* ACTION_CACHE_H */
