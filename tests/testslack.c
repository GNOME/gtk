#include <gtk/gtk.h>

#define GTK_TYPE_SLACK (gtk_slack_get_type ())
G_DECLARE_FINAL_TYPE (GtkSlack, gtk_slack, GTK, SLACK, GtkWidget)

struct _GtkSlack {
  GtkWidget parent;

  GtkWidget *child;

  gboolean has_width;
  gboolean has_height;
  int width;
  int height;
  int hslack;
  int vslack;
};

struct _GtkSlackClass {
  GtkWidgetClass parent_class;
};

enum {
  PROP_HSLACK = 1,
  PROP_VSLACK,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];

G_DEFINE_TYPE (GtkSlack, gtk_slack, GTK_TYPE_WIDGET)

static void
gtk_slack_init (GtkSlack *slack)
{
  slack->has_width = FALSE;
  slack->has_height = FALSE;
  slack->hslack = 0;
  slack->vslack = 0;
}

static void
gtk_slack_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   gint            for_size,
                   gint            *minimum,
                   gint            *natural,
                   gint            *minimum_baseline,
                   gint            *natural_baseline)
{
  GtkSlack *slack = GTK_SLACK (widget);
  int child_min, child_nat;

  if (slack->child && gtk_widget_get_visible (slack->child))
    {
      gtk_widget_measure (slack->child,
                          orientation, for_size,
                          &child_min, &child_nat,
                          NULL, NULL);
    }
  else
    {
      child_min = 0;
      child_nat = 0;
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (!slack->has_width)
        {
          slack->width = child_nat + slack->hslack;
          slack->has_width = TRUE;
        }
      else if (abs (slack->width - child_nat) > slack->hslack)
        {
          slack->width = child_nat + slack->hslack;
        }
      else
        {
          slack->width = MAX (slack->width, child_min);
        }

      *minimum = child_min;
      *natural = slack->width;
    }
  else
    {
      if (!slack->has_height)
        {
          slack->height = child_nat + slack->vslack;
          slack->has_height = TRUE;
        }
      else if(abs (slack->height - child_nat) > slack->vslack)
        {
          slack->height = child_nat + slack->vslack;
        }
      else
        {
          slack->height = MAX (slack->height, child_min);
        }

      *minimum = child_min;
      *natural = slack->height;
    }
}

static void
gtk_slack_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkSlack *slack = GTK_SLACK (widget);
  GtkAllocation allocation;

  allocation.x = 0;
  allocation.y = 0;
  allocation.width = width;
  allocation.height = height;

  gtk_widget_size_allocate (slack->child, &allocation, -1);
}

static void
gtk_slack_set_property (GObject         *object,
                        guint            prop_id,
                        const GValue    *value,
                        GParamSpec      *pspec)
{
  GtkSlack *slack = GTK_SLACK (object);

  switch (prop_id)
    {
    case PROP_HSLACK:
      if (slack->hslack != g_value_get_int (value))
        {
          slack->hslack = g_value_get_int (value);
          slack->has_width = FALSE;
          gtk_widget_queue_resize (GTK_WIDGET (slack));
        }
      break;
    case PROP_VSLACK:
      if (slack->vslack != g_value_get_int (value))
        {
          slack->vslack = g_value_get_int (value);
          slack->has_height = FALSE;
          gtk_widget_queue_resize (GTK_WIDGET (slack));
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_slack_get_property (GObject         *object,
                        guint            prop_id,
                        GValue          *value,
                        GParamSpec      *pspec)
{
  GtkSlack *slack = GTK_SLACK (object);

  switch (prop_id)
    {
    case PROP_HSLACK:
      g_value_set_int (value, slack->hslack);
      break;
    case PROP_VSLACK:
      g_value_set_int (value, slack->vslack);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_slack_class_init (GtkSlackClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->get_property = gtk_slack_get_property;
  gobject_class->set_property = gtk_slack_set_property;

  widget_class->measure = gtk_slack_measure;
  widget_class->size_allocate = gtk_slack_size_allocate;

  props[PROP_HSLACK] =
    g_param_spec_int ("hslack", "Horizontal Slack", "Horizontal Slack",
                      0, 100, 0,
                      G_PARAM_READWRITE);
  props[PROP_VSLACK] =
    g_param_spec_int ("vslack", "Vertical Slack", "Vertical Slack",
                      0, 100, 0,
                      G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, "slack");
}

static GtkWidget *
gtk_slack_new (void)
{
  return g_object_new (GTK_TYPE_SLACK, NULL);
}

static void
gtk_slack_set_child (GtkSlack  *slack,
                     GtkWidget *widget)
{
  slack->child = widget;
  gtk_widget_set_parent (widget, GTK_WIDGET (slack));
}

static gboolean
close_cb (GtkWindow *window, gpointer data)
{
  *((gboolean *)data) = TRUE;

  g_main_context_wakeup (NULL);

  return TRUE;
}

static const char css[] =
"window {"
"  background: blue; "
"}"
".label {"
"  background: yellow;"
"}";

int
main (int argc, char *argv[])
{
  gboolean done = FALSE;
  GtkWidget *window;
  GtkWidget *slack;
  GtkWidget *label;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              800);

  window = gtk_window_new ();
  g_signal_connect (window, "close-request", G_CALLBACK (close_cb), &done);

  slack = gtk_slack_new ();
  gtk_widget_set_halign (slack, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (slack, GTK_ALIGN_CENTER);
  gtk_window_set_child (GTK_WINDOW (window), slack);

  label = gtk_label_new ("Test");
  gtk_widget_add_css_class (label, "label");
  gtk_slack_set_child (GTK_SLACK (slack), label);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));
}
