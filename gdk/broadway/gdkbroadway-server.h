#ifndef __GDK_BROADWAY_SERVER__
#define __GDK_BROADWAY_SERVER__

#include <gdk/gdktypes.h>
#include "broadway-protocol.h"

typedef struct _GdkBroadwayServer GdkBroadwayServer;
typedef struct _GdkBroadwayServerClass GdkBroadwayServerClass;

#define GDK_TYPE_BROADWAY_SERVER              (gdk_broadway_server_get_type())
#define GDK_BROADWAY_SERVER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_BROADWAY_SERVER, GdkBroadwayServer))
#define GDK_BROADWAY_SERVER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_BROADWAY_SERVER, GdkBroadwayServerClass))
#define GDK_IS_BROADWAY_SERVER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_BROADWAY_SERVER))
#define GDK_IS_BROADWAY_SERVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_BROADWAY_SERVER))
#define GDK_BROADWAY_SERVER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_BROADWAY_SERVER, GdkBroadwayServerClass))

GdkBroadwayServer *_gdk_broadway_server_new                      (const char         *display,
								  GError            **error);
void               _gdk_broadway_server_flush                    (GdkBroadwayServer  *server);
void               _gdk_broadway_server_sync                     (GdkBroadwayServer  *server);
gulong             _gdk_broadway_server_get_next_serial          (GdkBroadwayServer  *server);
guint32            _gdk_broadway_server_get_last_seen_time       (GdkBroadwayServer  *server);
gboolean           _gdk_broadway_server_lookahead_event          (GdkBroadwayServer  *server,
								  const char         *types);
void               _gdk_broadway_server_query_mouse              (GdkBroadwayServer  *server,
								  guint32            *toplevel,
								  gint32             *root_x,
								  gint32             *root_y,
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
cairo_surface_t   *_gdk_broadway_server_create_surface           (int                 width,
								  int                 height);
void               _gdk_broadway_server_window_update            (GdkBroadwayServer  *server,
								  gint                id,
								  cairo_surface_t    *surface);
gboolean           _gdk_broadway_server_window_move_resize       (GdkBroadwayServer  *server,
								  gint                id,
								  gboolean            with_move,
								  int                 x,
								  int                 y,
								  int                 width,
								  int                 height);

#endif /* __GDK_BROADWAY_SERVER__ */
