#ifndef __GDK_DND_H__
#define __GDK_DND_H__

#include <gdk/gdktypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GdkDragContext        GdkDragContext;

typedef enum
{
  GDK_ACTION_DEFAULT = 1 << 0,
  GDK_ACTION_COPY    = 1 << 1,
  GDK_ACTION_MOVE    = 1 << 2,
  GDK_ACTION_LINK    = 1 << 3,
  GDK_ACTION_PRIVATE = 1 << 4,
  GDK_ACTION_ASK     = 1 << 5
} GdkDragAction;

typedef enum
{
  GDK_DRAG_PROTO_MOTIF,
  GDK_DRAG_PROTO_XDND,
  GDK_DRAG_PROTO_ROOTWIN,	  /* A root window with nobody claiming
				   * drags */
  GDK_DRAG_PROTO_NONE,		  /* Not a valid drag window */
  GDK_DRAG_PROTO_WIN32_DROPFILES, /* The simple WM_DROPFILES dnd */
  GDK_DRAG_PROTO_OLE2,		  /* The complex OLE2 dnd (not implemented) */
  GDK_DRAG_PROTO_LOCAL            /* Intra-app */
} GdkDragProtocol;

/* Object that holds information about a drag in progress.
 * this is used on both source and destination sides.
 */

typedef struct _GdkDragContextClass GdkDragContextClass;

#define GDK_TYPE_DRAG_CONTEXT              (gdk_drag_context_get_type ())
#define GDK_DRAG_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAG_CONTEXT, GdkDragContext))
#define GDK_DRAG_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAG_CONTEXT, GdkDragContextClass))
#define GDK_IS_DRAG_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAG_CONTEXT))
#define GDK_IS_DRAG_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAG_CONTEXT))
#define GDK_DRAG_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAG_CONTEXT, GdkDragContextClass))

struct _GdkDragContext {
  GObject parent_instance;

  /*< public >*/
  
  GdkDragProtocol protocol;
  
  gboolean is_source;
  
  GdkWindow *source_window;
  GdkWindow *dest_window;

  GList *targets;
  GdkDragAction actions;
  GdkDragAction suggested_action;
  GdkDragAction action; 

  guint32 start_time;

  /*< private >*/
  
  gpointer windowing_data;
};

struct _GdkDragContextClass {
  GObjectClass parent_class;

  
};

/* Drag and Drop */

GType            gdk_drag_context_get_type   (void) G_GNUC_CONST;
GdkDragContext * gdk_drag_context_new        (void);

#ifndef GDK_DISABLE_DEPRECATED
void             gdk_drag_context_ref        (GdkDragContext *context);
void             gdk_drag_context_unref      (GdkDragContext *context);
#endif

/* Destination side */

void             gdk_drag_status        (GdkDragContext   *context,
				         GdkDragAction     action,
					 guint32           time_);
void             gdk_drop_reply         (GdkDragContext   *context,
					 gboolean          ok,
					 guint32           time_);
void             gdk_drop_finish        (GdkDragContext   *context,
					 gboolean          success,
					 guint32           time_);
GdkAtom          gdk_drag_get_selection (GdkDragContext   *context);

/* Source side */

GdkDragContext * gdk_drag_begin      (GdkWindow      *window,
				      GList          *targets);

guint32 gdk_drag_get_protocol_for_display (GdkDisplay       *display,
					   guint32           xid,
					   GdkDragProtocol  *protocol);
void    gdk_drag_find_window_for_screen   (GdkDragContext   *context,
					   GdkWindow        *drag_window,
					   GdkScreen        *screen,
					   gint              x_root,
					   gint              y_root,
					   GdkWindow       **dest_window,
					   GdkDragProtocol  *protocol);

#ifndef GDK_MULTIHEAD_SAFE
guint32 gdk_drag_get_protocol (guint32           xid,
			       GdkDragProtocol  *protocol);
void    gdk_drag_find_window  (GdkDragContext   *context,
			       GdkWindow        *drag_window,
			       gint              x_root,
			       gint              y_root,
			       GdkWindow       **dest_window,
			       GdkDragProtocol  *protocol);
#endif /* GDK_MULTIHEAD_SAFE */

gboolean        gdk_drag_motion      (GdkDragContext *context,
				      GdkWindow      *dest_window,
				      GdkDragProtocol protocol,
				      gint            x_root, 
				      gint            y_root,
				      GdkDragAction   suggested_action,
				      GdkDragAction   possible_actions,
				      guint32         time_);
void            gdk_drag_drop        (GdkDragContext *context,
				      guint32         time_);
void            gdk_drag_abort       (GdkDragContext *context,
				      guint32         time_);
gboolean        gdk_drag_drop_succeeded (GdkDragContext *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_DND_H__ */
