/********************************************************************************
 * action_cache.c — semantic offline action cache state (F5). See action_cache.h.
 ********************************************************************************/
#include "tl_common.h"
#include "app_config.h"
#include "action_cache.h"

#define LEVEL_CLAMP   254            /* Step size is one byte / ±254 levels     */
#define CT_CLAMP      (CT_MAX_MIREDS - CT_MIN_MIREDS)

static u8  s_toggle;
static s32 s_level;
static s32 s_ct;
static u16 s_actions[ACTION_CACHE_MAX];
static u8  s_head;                   /* index of the oldest queued action        */
static u8  s_count;

static s32 clamp(s32 v, s32 lim)
{
    if (v >  lim) return  lim;
    if (v < -lim) return -lim;
    return v;
}

void action_cache_toggle(void)          { s_toggle = 1; }
void action_cache_level(s32 units)      { s_level = clamp(s_level + units, LEVEL_CLAMP); }
void action_cache_ct(s32 mireds)        { s_ct = clamp(s_ct + mireds, CT_CLAMP); }

void action_cache_push_action(u16 code)
{
    if (s_count < ACTION_CACHE_MAX) {
        s_actions[(s_head + s_count) % ACTION_CACHE_MAX] = code;
        s_count++;
    } else {
        /* Full: drop the oldest. */
        s_actions[s_head] = code;
        s_head = (s_head + 1) % ACTION_CACHE_MAX;
    }
}

void action_cache_clear(void)
{
    s_toggle = 0;
    s_level = 0;
    s_ct = 0;
    s_head = 0;
    s_count = 0;
}

bool action_cache_empty(void)
{
    return !s_toggle && s_level == 0 && s_ct == 0 && s_count == 0;
}

bool action_cache_toggle_pending(void) { return s_toggle != 0; }
s32  action_cache_level_get(void)      { return s_level; }
s32  action_cache_ct_get(void)         { return s_ct; }
u8   action_cache_action_count(void)   { return s_count; }
u16  action_cache_action_at(u8 i)      { return s_actions[(s_head + i) % ACTION_CACHE_MAX]; }
