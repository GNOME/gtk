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

static double pos_x, pos_y;

static void
new_label_cb (GtkMenuItem *item,
              gpointer     data)
{
  GtkFixed *fixed = data;
  GtkWidget *widget;

  widget = gtk_label_new ("Label");
  gtk_fixed_put (fixed, widget, pos_x, pos_y);
}

static void
new_spinner_cb (GtkMenuItem *item,
                gpointer     data)
{
  GtkFixed *fixed = data;
  GtkWidget *widget;

  widget = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (widget));
  gtk_fixed_put (fixed, widget, pos_x, pos_y);
}

static void
new_demo_cb (GtkMenuItem *item,
             gpointer     data)
{
  GtkFixed *fixed = data;
  GtkWidget *widget;

  widget = gtk_demo_widget_new ();
  gtk_fixed_put (fixed, widget, pos_x, pos_y);
}

static void
copy_cb (GtkMenuItem *item,
         gpointer     data)
{
  GtkWidget *child = data;
  GdkClipboard *clipboard;
  GValue value = { 0, };

  g_print ("Copy %s!\n", g_type_name_from_instance ((GTypeInstance *)child));

  g_value_init (&value, G_OBJECT_TYPE (child));
  g_value_set_object (&value, child);

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  gdk_clipboard_set_value (clipboard, &value);
}

static void
value_read (GObject      *source,
            GAsyncResult *res,
            gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  GError *error = NULL;
  const GValue *value;
  GtkWidget *widget;
  GtkWidget *new_widget;

  value = gdk_clipboard_read_value_finish (clipboard, res, &error);

  if (value == NULL)
    {
      g_print ("error: %s\n", error->message);
      g_error_free (error);
      return;
    }

  widget = g_value_get_object (value);
  if (GTK_IS_LABEL (widget))
    new_widget = gtk_label_new (gtk_label_get_text (GTK_LABEL (widget)));
  else if (GTK_IS_SPINNER (widget))
    {
      new_widget = gtk_spinner_new ();
      gtk_spinner_start (GTK_SPINNER (new_widget));
    }
  else
    {
      return;
    }

  gtk_fixed_put (GTK_FIXED (data), new_widget, pos_x, pos_y);
}
static void
paste_cb (GtkMenuItem *item,
          gpointer     data)
{
  GdkClipboard *clipboard;

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  if (gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), GTK_TYPE_LABEL))
    gdk_clipboard_read_value_async (clipboard, GTK_TYPE_LABEL, 0, NULL, value_read, data);
  else if (gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), GTK_TYPE_SPINNER))
    gdk_clipboard_read_value_async (clipboard, GTK_TYPE_SPINNER, 0, NULL, value_read, data);
  else
    g_print ("Don't know how to handle clipboard contents\n");
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *widget;
      GdkRectangle rect;
      GtkWidget *menu;
      GtkWidget *item;
      GtkWidget *child;
      GdkClipboard *clipboard;

      widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
      child = gtk_widget_pick (widget, x, y);

      pos_x = x;
      pos_y = y;

      menu = gtk_menu_new ();
      item = gtk_menu_item_new_with_label ("New Label");
      g_signal_connect (item, "activate", G_CALLBACK (new_label_cb), widget);
      gtk_container_add (GTK_CONTAINER (menu), item);
      item = gtk_menu_item_new_with_label ("New Spinner");
      g_signal_connect (item, "activate", G_CALLBACK (new_spinner_cb), widget);
      gtk_container_add (GTK_CONTAINER (menu), item);
      item = gtk_menu_item_new_with_label ("New Custom");
      g_signal_connect (item, "activate", G_CALLBACK (new_demo_cb), widget);
      gtk_container_add (GTK_CONTAINER (menu), item);
      item = gtk_separator_menu_item_new ();
      gtk_container_add (GTK_CONTAINER (menu), item);
      item = gtk_menu_item_new_with_label ("Copy");
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "activate", G_CALLBACK (copy_cb), child);
      gtk_container_add (GTK_CONTAINER (menu), item);
      item = gtk_menu_item_new_with_label ("Paste");

      clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
      if (gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), GTK_TYPE_LABEL) ||
          gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), GTK_TYPE_SPINNER))
        gtk_widget_set_sensitive (item, TRUE);
      else
        gtk_widget_set_sensitive (item, FALSE);
      g_signal_connect (item, "activate", G_CALLBACK (paste_cb), child);
      gtk_container_add (GTK_CONTAINER (menu), item);

      rect.x = x;
      rect.y = y;
      rect.width = 0;
      rect.height = 0;

      gtk_menu_popup_at_rect (GTK_MENU (menu),
                              gtk_widget_get_window (widget),
                              &rect,
                              GDK_GRAVITY_NORTH_WEST,
                              GDK_GRAVITY_NORTH_WEST,
                              NULL);

      return;
    }
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
