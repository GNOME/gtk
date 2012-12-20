#ifndef __BROADWAY_PROTOCOL_H__
#define __BROADWAY_PROTOCOL_H__

#include <glib.h>

typedef enum {
  BROADWAY_EVENT_ENTER = 'e',
  BROADWAY_EVENT_LEAVE = 'l',
  BROADWAY_EVENT_POINTER_MOVE = 'm',
  BROADWAY_EVENT_BUTTON_PRESS = 'b',
  BROADWAY_EVENT_BUTTON_RELEASE = 'B',
  BROADWAY_EVENT_SCROLL = 's',
  BROADWAY_EVENT_KEY_PRESS = 'k',
  BROADWAY_EVENT_KEY_RELEASE = 'K',
  BROADWAY_EVENT_GRAB_NOTIFY = 'g',
  BROADWAY_EVENT_UNGRAB_NOTIFY = 'u',
  BROADWAY_EVENT_CONFIGURE_NOTIFY = 'w',
  BROADWAY_EVENT_DELETE_NOTIFY = 'W',
  BROADWAY_EVENT_SCREEN_SIZE_CHANGED = 'd'
} BroadwayEventType;

typedef struct {
  guint8 type;
  guint32 serial;
  guint64 time;
} BroadwayInputBaseMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 mouse_window_id; /* The real window, not taking grabs into account */
  guint32 event_window_id;
  gint32 root_x;
  gint32 root_y;
  gint32 win_x;
  gint32 win_y;
  guint32 state;
} BroadwayInputPointerMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  guint32 mode;
} BroadwayInputCrossingMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  guint32 button;
} BroadwayInputButtonMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  gint32 dir;
} BroadwayInputScrollMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 mouse_window_id; /* The real window, not taking grabs into account */
  guint32 state;
  gint32 key;
} BroadwayInputKeyMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 res;
} BroadwayInputGrabReply;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 id;
  gint32 x;
  gint32 y;
  gint32 width;
  gint32 height;
} BroadwayInputConfigureNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 width;
  gint32 height;
} BroadwayInputScreenResizeNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 id;
} BroadwayInputDeleteNotify;

typedef union {
  BroadwayInputBaseMsg base;
  BroadwayInputPointerMsg pointer;
  BroadwayInputCrossingMsg crossing;
  BroadwayInputButtonMsg button;
  BroadwayInputScrollMsg scroll;
  BroadwayInputKeyMsg key;
  BroadwayInputGrabReply grab_reply;
  BroadwayInputConfigureNotify configure_notify;
  BroadwayInputDeleteNotify delete_notify;
  BroadwayInputScreenResizeNotify screen_resize_notify;
} BroadwayInputMsg;

#endif /* __BROADWAY_PROTOCOL_H__ */
