#ifndef EVENT_H
#define EVENT_H

#include "types.h"

#include "constants/event.h"

#define CLEAR_EVENT(event) EventFunction(EVENT_ACTION_CLEAR, event)
#define SET_EVENT(event) EventFunction(EVENT_ACTION_SET, event)
#define TOGGLE_EVENT(event) EventFunction(EVENT_ACTION_TOGGLE, event)
#define CHECK_EVENT(event) EventFunction(EVENT_ACTION_CHECK, event)

#ifdef PLATFORM_PC // linker.ld places it in EWRAM
#include "gba/memory.h"
#define gEventsTriggered (*(u32 (*)[8])(EWRAM_BASE + 0x37e00))
#else
extern u32 gEventsTriggered[8];
#endif
u32 EventFunction(EventAction action, Event event);

#endif /* EVENT_H */
