#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_OFFSCREEN_WINDOW_H__
#define __GTK_OFFSCREEN_WINDOW_H__

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_OFFSCREEN_WINDOW         (gtk_offscreen_window_get_type ())
#define GTK_OFFSCREEN_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_OFFSCREEN_WINDOW, GtkOffscreenWindow))
#define GTK_OFFSCREEN_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_OFFSCREEN_WINDOW, GtkOffscreenWindowClass))
#define GTK_IS_OFFSCREEN_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_OFFSCREEN_WINDOW))
#define GTK_IS_OFFSCREEN_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_OFFSCREEN_WINDOW))
#define GTK_OFFSCREEN_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_OFFSCREEN_WINDOW, GtkOffscreenWindowClass))

typedef struct _GtkOffscreenWindow      GtkOffscreenWindow;
typedef struct _GtkOffscreenWindowClass GtkOffscreenWindowClass;

struct _GtkOffscreenWindow
{
  GtkWindow parent_object;
};

struct _GtkOffscreenWindowClass
{
  GtkWindowClass parent_class;
};

GType   gtk_offscreen_window_get_type () G_GNUC_CONST;

G_END_DECLS

#endif /* __GTK_OFFSCREEN_WINDOW_H__ */
