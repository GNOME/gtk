
#ifndef __GTK_CENTER_BOX_H__
#define __GTK_CENTER_BOX_H__

#include "gtkwidget.h"

#define GTK_TYPE_CENTER_BOX                 (gtk_center_box_get_type ())
#define GTK_CENTER_BOX(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CENTER_BOX, GtkCenterBox))
#define GTK_CENTER_BOX_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CENTER_BOX, GtkCenterBoxClass))
#define GTK_IS_CENTER_BOX(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CENTER_BOX))
#define GTK_IS_CENTER_BOX_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CENTER_BOX))
#define GTK_CENTER_BOX_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CENTER_BOX, GtkCenterBoxClass))

typedef struct _GtkCenterBox             GtkCenterBox;
typedef struct _GtkCenterBoxClass        GtkCenterBoxClass;

struct _GtkCenterBox
{
  GtkWidget parent_instance;

  GtkWidget *start_widget;
  GtkWidget *center_widget;
  GtkWidget *end_widget;
};

struct _GtkCenterBoxClass
{
  GtkWidgetClass parent_class;
};

GType      gtk_center_box_get_type (void) G_GNUC_CONST;

GtkWidget *gtk_center_box_new (void);
void       gtk_center_box_set_start_widget  (GtkCenterBox *self, GtkWidget *child);
void       gtk_center_box_set_center_widget (GtkCenterBox *self, GtkWidget *child);
void       gtk_center_box_set_end_widget    (GtkCenterBox *self, GtkWidget *child);

GtkWidget * gtk_center_box_get_start_widget  (GtkCenterBox *self);
GtkWidget * gtk_center_box_get_center_widget (GtkCenterBox *self);
GtkWidget * gtk_center_box_get_end_widget    (GtkCenterBox *self);


#endif
