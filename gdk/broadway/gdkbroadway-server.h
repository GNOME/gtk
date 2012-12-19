#ifndef __GDK_BROADWAY_SERVER__
#define __GDK_BROADWAY_SERVER__

#include <gdk/gdktypes.h>

typedef struct _GdkBroadwayServer GdkBroadwayServer;
typedef struct _GdkBroadwayServerClass GdkBroadwayServerClass;

#define GDK_TYPE_BROADWAY_SERVER              (gdk_broadway_server_get_type())
#define GDK_BROADWAY_SERVER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_BROADWAY_SERVER, GdkBroadwayServer))
#define GDK_BROADWAY_SERVER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_BROADWAY_SERVER, GdkBroadwayServerClass))
#define GDK_IS_BROADWAY_SERVER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_BROADWAY_SERVER))
#define GDK_IS_BROADWAY_SERVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_BROADWAY_SERVER))
#define GDK_BROADWAY_SERVER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_BROADWAY_SERVER, GdkBroadwayServerClass))

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

GdkBroadwayServer *_gdk_broadway_server_new                      (int                 port,
								  GError            **error);
gboolean           _gdk_broadway_server_has_client               (GdkBroadwayServer  *server);
void               _gdk_broadway_server_flush                    (GdkBroadwayServer  *server);
void               _gdk_broadway_server_sync                     (GdkBroadwayServer  *server);
gulong             _gdk_broadway_server_get_next_serial          (GdkBroadwayServer  *server);
guint32            _gdk_broadway_server_get_last_seen_time       (GdkBroadwayServer  *server);
gboolean           _gdk_broadway_server_lookahead_event          (GdkBroadwayServer  *server,
								  const char         *types);
void               _gdk_broadway_server_query_mouse              (GdkBroadwayServer  *server,
								  gint               *toplevel,
								  gint               *root_x,
								  gint               *root_y,
								  guint32            *mask);
GdkGrabStatus      _gdk_broadway_server_grab_pointer             (GdkBroadwayServer  *server,
								  gint                id,
								  gboolean            owner_events,
								  guint32             event_mask,
								  guint32             time_);
guint32            _gdk_broadway_server_ungrab_pointer           (GdkBroadwayServer  *server,
								  guint32             time_);
gint32             _gdk_broadway_server_get_mouse_toplevel       (GdkBroadwayServer  *server);
guint32            _gdk_broadway_server_new_window               (GdkBroadwayServer  *server,
								  int                 x,
								  int                 y,
								  int                 width,
								  int                 height,
								  gboolean            is_temp);
void               _gdk_broadway_server_destroy_window           (GdkBroadwayServer  *server,
								  gint                id);
gboolean           _gdk_broadway_server_window_show              (GdkBroadwayServer  *server,
								  gint                id);
gboolean           _gdk_broadway_server_window_hide              (GdkBroadwayServer  *server,
								  gint                id);
void               _gdk_broadway_server_window_set_transient_for (GdkBroadwayServer  *server,
								  gint                id,
								  gint                parent);
gboolean           _gdk_broadway_server_window_translate         (GdkBroadwayServer  *server,
								  gint                id,
								  cairo_region_t     *area,
								  gint                dx,
								  gint                dy);
void               _gdk_broadway_server_window_update            (GdkBroadwayServer  *server,
								  gint                id,
								  cairo_surface_t    *surface);
gboolean           _gdk_broadway_server_window_move_resize       (GdkBroadwayServer  *server,
								  gint                id,
								  int                 x,
								  int                 y,
								  int                 width,
								  int                 height);

#endif /* __GDK_BROADWAY_SERVER__ */
