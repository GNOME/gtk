#ifndef __WIDGETS_H__
#define __WIDGETS_H__

#include <gtk/gtk.h>

typedef struct WidgetInfo
{
  GtkWidget *window;
  gchar *name;
  gboolean no_focus;
  gboolean include_decorations;
} WidgetInfo;

GList *get_all_widgets (void);


#endif /* __WIDGETS_H__ */
