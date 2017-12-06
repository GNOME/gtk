/* Drag-and-Drop
 *
 * I can't believe its not glade!
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define GTK_TYPE_DEMO_WIDGET (gtk_demo_widget_get_type ())

G_DECLARE_FINAL_TYPE (GtkDemoWidget, gtk_demo_widget, GTK, DEMO_WIDGET, GtkWidget)

static GtkWidget *gtk_demo_widget_new (void);

struct _GtkDemoWidget
{
  GtkWidget parent_instance;
};

G_DEFINE_TYPE (GtkDemoWidget, gtk_demo_widget, GTK_TYPE_WIDGET)

static void
gtk_demo_widget_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_demo_widget_parent_class)->finalize (object);
}

static void
gtk_demo_widget_init (GtkDemoWidget *demo)
{
  gtk_widget_set_has_window (GTK_WIDGET (demo), FALSE);

}

static void
gtk_demo_widget_class_init (GtkDemoWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_demo_widget_finalize;

  gtk_widget_class_set_css_name (widget_class, "demo");
}

static GtkWidget *
gtk_demo_widget_new (void)
{
  return g_object_new (GTK_TYPE_DEMO_WIDGET, NULL);
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  GtkWidget *widget;

  if (n_press < 2)
    return;

  widget = gtk_demo_widget_new ();

  gtk_fixed_put (GTK_FIXED (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture))), widget, x, y);
}

static GtkWidget *window = NULL;

GtkWidget *
do_dnd (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *vbox, *fixed;
      GtkGesture *multipress;
      GtkCssProvider *provider;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Drag-and-drop");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      fixed = gtk_fixed_new ();
      gtk_box_pack_start (GTK_BOX (vbox), fixed);
      gtk_widget_set_hexpand (fixed, TRUE);
      gtk_widget_set_vexpand (fixed, TRUE);

      multipress = gtk_gesture_multi_press_new (fixed);
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (multipress), 0);
      g_signal_connect (multipress, "pressed", G_CALLBACK (pressed_cb), NULL);

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/dnd/dnd.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
