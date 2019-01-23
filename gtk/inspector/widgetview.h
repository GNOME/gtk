
#ifndef __GTK_WIDGET_VIEW_H__
#define __GTK_WIDGET_VIEW_H__

#include "gtkwidget.h"
#include "gtkwidgetpaintable.h"

#define GTK_TYPE_WIDGET_VIEW                 (gtk_widget_view_get_type ())
#define GTK_WIDGET_VIEW(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_WIDGET_VIEW, GtkWidgetView))
#define GTK_WIDGET_VIEW_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_WIDGET_VIEW, GtkWidgetViewClass))
#define GTK_IS_WIDGET_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_WIDGET_VIEW))
#define GTK_IS_WIDGET_VIEW_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WIDGET_VIEW))
#define GTK_WIDGET_VIEW_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WIDGET_VIEW, GtkWidgetViewClass))

typedef struct _GtkWidgetView             GtkWidgetView;
typedef struct _GtkWidgetViewClass        GtkWidgetViewClass;

struct _GtkWidgetView
{
  GtkWidget parent_instance;

  GtkWidget *box;
  GtkWidget *typename_label;
  GtkWidget *paintable_picture;

  GtkWidget *inspected;
};

struct _GtkWidgetViewClass
{
  GtkWidgetClass parent_class;
};

GType        gtk_widget_view_get_type             (void) G_GNUC_CONST;
GtkWidget *  gtk_widget_view_new                  (void);
void         gtk_widget_view_set_inspected_widget (GtkWidgetView *self,
                                                   GtkWidget     *inspected);

#endif
