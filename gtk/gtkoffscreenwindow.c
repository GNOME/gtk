#include "gtkoffscreenwindow.h"

G_DEFINE_TYPE (GtkOffscreenWindow, gtk_offscreen_window, GTK_TYPE_WINDOW);

static void
gtk_offscreen_window_size_request (GtkWidget *widget,
                                   GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);
  gint border_width;
  gint default_width, default_height;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  requisition->width = border_width * 2;
  requisition->height = border_width * 2;

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkRequisition child_req;

      gtk_widget_size_request (bin->child, &child_req);

      requisition->width += child_req.width;
      requisition->height += child_req.height;
    }

  gtk_window_get_default_size (GTK_WINDOW (widget),
                               &default_width, &default_height);
  if (default_width > 0)
    requisition->width = default_width;

  if (default_height > 0)
    requisition->height = default_height;
}

static void
gtk_offscreen_window_size_allocate (GtkWidget *widget,
                                    GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN (widget);
  gint border_width;

  widget->allocation = *allocation;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  if (bin->child && GTK_WIDGET_VISIBLE (bin->child))
    {
      GtkAllocation  child_alloc;

      child_alloc.x = border_width;
      child_alloc.y = border_width;
      child_alloc.width = allocation->width - 2 * border_width;
      child_alloc.height = allocation->height - 2 * border_width;

      gtk_widget_size_allocate (bin->child, &child_alloc);
    }

  gtk_widget_queue_draw (widget);
}

static void
gtk_offscreen_window_realize (GtkWidget *widget)
{
  GtkBin *bin;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;

  bin = GTK_BIN (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_OFFSCREEN;
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.wclass = GDK_INPUT_OUTPUT;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  if (bin->child)
    gtk_widget_set_parent_window (bin->child, widget->window);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
gtk_offscreen_window_resize (GtkWidget *widget)
{
  GtkAllocation allocation = { 0, 0 };
  GtkRequisition requisition;

  gtk_widget_size_request (widget, &requisition);

  allocation.width  = requisition.width;
  allocation.height = requisition.height;
  gtk_widget_size_allocate (widget, &allocation);
}

static void
move_focus (GtkWidget       *widget,
            GtkDirectionType dir)
{
  gtk_widget_child_focus (widget, dir);

  if (!GTK_CONTAINER (widget)->focus_child)
    gtk_window_set_focus (GTK_WINDOW (widget), NULL);
}

static void
gtk_offscreen_window_show (GtkWidget *widget)
{
  gboolean need_resize;
  GtkContainer *container;

  GTK_WIDGET_SET_FLAGS (widget, GTK_VISIBLE);

  container = GTK_CONTAINER (widget);
  need_resize = container->need_resize || !GTK_WIDGET_REALIZED (widget);
  container->need_resize = FALSE;

  if (need_resize)
    gtk_offscreen_window_resize (widget);

  gtk_widget_map (widget);

  /* Try to make sure that we have some focused widget */
  if (!gtk_window_get_focus (GTK_WINDOW (widget)))
    move_focus (widget, GTK_DIR_TAB_FORWARD);
}

static void
gtk_offscreen_window_hide (GtkWidget *widget)
{
  GTK_WIDGET_UNSET_FLAGS (widget, GTK_VISIBLE);
  gtk_widget_unmap (widget);
}

static void
gtk_offscreen_window_check_resize (GtkContainer *container)
{
  GtkWidget *widget = GTK_WIDGET (container);

  if (GTK_WIDGET_VISIBLE (widget))
    gtk_offscreen_window_resize (widget);
}

static void
gtk_offscreen_window_class_init (GtkOffscreenWindowClass *class)
{
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  widget_class = GTK_WIDGET_CLASS (class);
  container_class = GTK_CONTAINER_CLASS (class);

  widget_class->realize = gtk_offscreen_window_realize;
  widget_class->show = gtk_offscreen_window_show;
  widget_class->hide = gtk_offscreen_window_hide;
  widget_class->size_request = gtk_offscreen_window_size_request;
  widget_class->size_allocate = gtk_offscreen_window_size_allocate;

  container_class->check_resize = gtk_offscreen_window_check_resize;
}

static void
gtk_offscreen_window_init (GtkOffscreenWindow *bin)
{
}

GtkWidget *
gtk_offscreen_window_new (void)
{
  return g_object_new (gtk_offscreen_window_get_type (), NULL);
}


#define __GTK_OFFSCREEN_WINDOW_C__
#include "gtkaliasdef.c"
