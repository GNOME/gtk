#ifndef __WIDGETS_H__
#define __WIDGETS_H__

#include <gtk/gtk.h>


typedef enum
{
  SMALL,
  MEDIUM,
  LARGE,
  ASIS
} WidgetSize;

typedef struct WidgetInfo
{
  GtkWidget *window;
  char *name;
  gboolean no_focus;
  gboolean include_decorations;
  guint wait;
  WidgetSize size;
} WidgetInfo;

GList *get_all_widgets (void);


#endif /* __WIDGETS_H__ */
