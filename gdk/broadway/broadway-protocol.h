#ifndef __BROADWAY_PROTOCOL_H__
#define __BROADWAY_PROTOCOL_H__

#include <glib.h>

typedef struct  {
    gint32 x, y;
    gint32 width, height;
} BroadwayRect;

typedef enum {
  BROADWAY_EVENT_ENTER = 'e',
  BROADWAY_EVENT_LEAVE = 'l',
  BROADWAY_EVENT_POINTER_MOVE = 'm',
  BROADWAY_EVENT_BUTTON_PRESS = 'b',
  BROADWAY_EVENT_BUTTON_RELEASE = 'B',
  BROADWAY_EVENT_TOUCH = 't',
  BROADWAY_EVENT_SCROLL = 's',
  BROADWAY_EVENT_KEY_PRESS = 'k',
  BROADWAY_EVENT_KEY_RELEASE = 'K',
  BROADWAY_EVENT_GRAB_NOTIFY = 'g',
  BROADWAY_EVENT_UNGRAB_NOTIFY = 'u',
  BROADWAY_EVENT_CONFIGURE_NOTIFY = 'w',
  BROADWAY_EVENT_DELETE_NOTIFY = 'W',
  BROADWAY_EVENT_SCREEN_SIZE_CHANGED = 'd',
  BROADWAY_EVENT_FOCUS = 'f'
} BroadwayEventType;

typedef enum {
  BROADWAY_OP_GRAB_POINTER = 'g',
  BROADWAY_OP_UNGRAB_POINTER = 'u',
  BROADWAY_OP_NEW_SURFACE = 's',
  BROADWAY_OP_SHOW_SURFACE = 'S',
  BROADWAY_OP_HIDE_SURFACE = 'H',
  BROADWAY_OP_RAISE_SURFACE = 'r',
  BROADWAY_OP_LOWER_SURFACE = 'R',
  BROADWAY_OP_DESTROY_SURFACE = 'd',
  BROADWAY_OP_MOVE_RESIZE = 'm',
  BROADWAY_OP_SET_TRANSIENT_FOR = 'p',
  BROADWAY_OP_PUT_RGB = 'i',
  BROADWAY_OP_REQUEST_AUTH = 'l',
  BROADWAY_OP_AUTH_OK = 'L',
  BROADWAY_OP_DISCONNECTED = 'D',
  BROADWAY_OP_PUT_BUFFER = 'b',
  BROADWAY_OP_SET_SHOW_KEYBOARD = 'k',
} BroadwayOpType;

typedef struct {
  guint32 type;
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
  guint32 touch_type;
  guint32 event_window_id;
  guint32 sequence_id;
  guint32 is_emulated;
  gint32 root_x;
  gint32 root_y;
  gint32 win_x;
  gint32 win_y;
  guint32 state;
} BroadwayInputTouchMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 window_id;
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
  guint32 width;
  guint32 height;
} BroadwayInputScreenResizeNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 id;
} BroadwayInputDeleteNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  gint32 new_id;
  gint32 old_id;
} BroadwayInputFocusMsg;

typedef union {
  BroadwayInputBaseMsg base;
  BroadwayInputPointerMsg pointer;
  BroadwayInputCrossingMsg crossing;
  BroadwayInputButtonMsg button;
  BroadwayInputScrollMsg scroll;
  BroadwayInputTouchMsg touch;
  BroadwayInputKeyMsg key;
  BroadwayInputGrabReply grab_reply;
  BroadwayInputConfigureNotify configure_notify;
  BroadwayInputDeleteNotify delete_notify;
  BroadwayInputScreenResizeNotify screen_resize_notify;
  BroadwayInputFocusMsg focus;
} BroadwayInputMsg;

typedef enum {
  BROADWAY_REQUEST_NEW_WINDOW,
  BROADWAY_REQUEST_FLUSH,
  BROADWAY_REQUEST_SYNC,
  BROADWAY_REQUEST_QUERY_MOUSE,
  BROADWAY_REQUEST_DESTROY_WINDOW,
  BROADWAY_REQUEST_SHOW_WINDOW,
  BROADWAY_REQUEST_HIDE_WINDOW,
  BROADWAY_REQUEST_SET_TRANSIENT_FOR,
  BROADWAY_REQUEST_UPDATE,
  BROADWAY_REQUEST_MOVE_RESIZE,
  BROADWAY_REQUEST_GRAB_POINTER,
  BROADWAY_REQUEST_UNGRAB_POINTER,
  BROADWAY_REQUEST_FOCUS_WINDOW,
  BROADWAY_REQUEST_SET_SHOW_KEYBOARD
} BroadwayRequestType;

typedef struct {
  guint32 size;
  guint32 serial;
  guint32 type;
} BroadwayRequestBase, BroadwayRequestFlush, BroadwayRequestSync, BroadwayRequestQueryMouse;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
} BroadwayRequestDestroyWindow, BroadwayRequestShowWindow, BroadwayRequestHideWindow, BroadwayRequestFocusWindow;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 parent;
} BroadwayRequestSetTransientFor;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  gint32 dx;
  gint32 dy;
  guint32 n_rects;
  BroadwayRect rects[1];
} BroadwayRequestTranslate;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  char name[36];
  guint32 width;
  guint32 height;
} BroadwayRequestUpdate;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 owner_events;
  guint32 event_mask;
  guint32 time_;
} BroadwayRequestGrabPointer;

typedef struct {
  BroadwayRequestBase base;
  guint32 time_;
} BroadwayRequestUngrabPointer;

typedef struct {
  BroadwayRequestBase base;
  gint32 x;
  gint32 y;
  guint32 width;
  guint32 height;
  guint32 is_temp;
} BroadwayRequestNewWindow;

typedef struct {
  BroadwayRequestBase base;
  guint32 id;
  guint32 with_move;
  gint32 x;
  gint32 y;
  guint32 width;
  guint32 height;
} BroadwayRequestMoveResize;

typedef struct {
  BroadwayRequestBase base;
  guint32 show_keyboard;
} BroadwayRequestSetShowKeyboard;

typedef union {
  BroadwayRequestBase base;
  BroadwayRequestNewWindow new_window;
  BroadwayRequestFlush flush;
  BroadwayRequestSync sync;
  BroadwayRequestQueryMouse query_mouse;
  BroadwayRequestDestroyWindow destroy_window;
  BroadwayRequestShowWindow show_window;
  BroadwayRequestHideWindow hide_window;
  BroadwayRequestSetTransientFor set_transient_for;
  BroadwayRequestUpdate update;
  BroadwayRequestMoveResize move_resize;
  BroadwayRequestGrabPointer grab_pointer;
  BroadwayRequestUngrabPointer ungrab_pointer;
  BroadwayRequestTranslate translate;
  BroadwayRequestFocusWindow focus_window;
  BroadwayRequestSetShowKeyboard set_show_keyboard;
} BroadwayRequest;

typedef enum {
  BROADWAY_REPLY_EVENT,
  BROADWAY_REPLY_SYNC,
  BROADWAY_REPLY_QUERY_MOUSE,
  BROADWAY_REPLY_NEW_WINDOW,
  BROADWAY_REPLY_GRAB_POINTER,
  BROADWAY_REPLY_UNGRAB_POINTER
} BroadwayReplyType;

typedef struct {
  guint32 size;
  guint32 in_reply_to;
  guint32 type;
} BroadwayReplyBase, BroadwayReplySync;

typedef struct {
  BroadwayReplyBase base;
  guint32 id;
} BroadwayReplyNewWindow;

typedef struct {
  BroadwayReplyBase base;
  guint32 status;
} BroadwayReplyGrabPointer, BroadwayReplyUngrabPointer;

typedef struct {
  BroadwayReplyBase base;
  guint32 toplevel;
  gint32 root_x;
  gint32 root_y;
  guint32 mask;
} BroadwayReplyQueryMouse;

typedef struct {
  BroadwayReplyBase base;
  BroadwayInputMsg msg;
} BroadwayReplyEvent;

typedef union {
  BroadwayReplyBase base;
  BroadwayReplyEvent event;
  BroadwayReplyQueryMouse query_mouse;
  BroadwayReplyNewWindow new_window;
  BroadwayReplyGrabPointer grab_pointer;
  BroadwayReplyUngrabPointer ungrab_pointer;
} BroadwayReply;

#endif /* __BROADWAY_PROTOCOL_H__ */
