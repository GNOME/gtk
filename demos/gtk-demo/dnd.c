/* Drag-and-Drop
 *
 * I can't believe its not glade!
 *
 * Try right-clicking in the window.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

typedef struct _GtkDemoWidget GtkDemoWidget;
struct _GtkDemoWidget
{
  GType type;
  union {
    char *text;
    gboolean active;
  };
};

static gpointer
copy_demo_widget (gpointer data)
{
  GtkDemoWidget *demo = g_memdup (data, sizeof (GtkDemoWidget));

  if (demo->type == GTK_TYPE_LABEL)
    demo->text = g_strdup (demo->text);

  return demo;
}

static void
free_demo_widget (gpointer data)
{
  GtkDemoWidget *demo = data;

  if (demo->type == GTK_TYPE_LABEL)
    g_free (demo->text);

  g_free (demo);
}

#define GTK_TYPE_DEMO_WIDGET (gtk_demo_widget_get_type ())
static GType gtk_demo_widget_get_type (void);
G_DEFINE_BOXED_TYPE (GtkDemoWidget, gtk_demo_widget, copy_demo_widget, free_demo_widget)

static GtkDemoWidget *
serialize_widget (GtkWidget *widget)
{
  GtkDemoWidget *demo;

  demo = g_new0 (GtkDemoWidget, 1);
  demo->type = G_OBJECT_TYPE (widget);

  if (GTK_IS_LABEL (widget))
    {
      demo->text = g_strdup (gtk_label_get_text (GTK_LABEL (widget)));
    }
  else if (GTK_IS_SPINNER (widget))
    {
      g_object_get (widget, "spinning", &demo->active, NULL);
    }
  else
    {
      g_print ("Type %s not supported\n", g_type_name (demo->type));
    }

  return demo;
}

static GtkWidget *
deserialize_widget (GtkDemoWidget *demo)
{
  GtkWidget *widget = NULL;

  if (demo->type == GTK_TYPE_LABEL)
    {
      widget = gtk_label_new (demo->text);
    }
  else if (demo->type == GTK_TYPE_SPINNER)
    {
      widget = g_object_new (demo->type, "spinning", demo->active, NULL);
      gtk_widget_add_css_class (widget, "demo");
    }
  else
    {
      g_print ("Type %s not supported\n", g_type_name (demo->type));
    }

  return widget;
}

static double pos_x, pos_y;

static void
new_label_cb (GtkWidget *button,
              gpointer     data)
{
  GtkFixed *fixed = data;
  GtkWidget *widget;

  widget = gtk_label_new ("Label");
  gtk_fixed_put (fixed, widget, pos_x, pos_y);

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
new_spinner_cb (GtkWidget *button,
                gpointer     data)
{
  GtkFixed *fixed = data;
  GtkWidget *widget;

  widget = gtk_spinner_new ();
  gtk_widget_add_css_class (widget, "demo");
  gtk_spinner_start (GTK_SPINNER (widget));
  gtk_fixed_put (fixed, widget, pos_x, pos_y);

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
copy_cb (GtkWidget *button, GtkWidget *child)
{
  GdkClipboard *clipboard;
  GtkDemoWidget *demo;

  demo = serialize_widget (child);

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  gdk_clipboard_set (clipboard, GTK_TYPE_DEMO_WIDGET, demo);

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
delete_cb (GtkWidget *button, GtkWidget *child)
{
  gtk_widget_destroy (child);

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
cut_cb (GtkWidget *button, GtkWidget *child)
{
  copy_cb (button, child);
  delete_cb (button, child);

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
value_read (GObject      *source,
            GAsyncResult *res,
            gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  GError *error = NULL;
  const GValue *value;
  GtkDemoWidget *demo;
  GtkWidget *widget = NULL;

  value = gdk_clipboard_read_value_finish (clipboard, res, &error);

  if (value == NULL)
    {
      g_print ("error: %s\n", error->message);
      g_error_free (error);
      return;
    }

  if (!G_VALUE_HOLDS (value, GTK_TYPE_DEMO_WIDGET))
    {
      g_print ("can't handle clipboard contents\n");
      return;
    }

  demo = g_value_get_boxed (value);
  widget = deserialize_widget (demo);

  gtk_fixed_put (GTK_FIXED (data), widget, pos_x, pos_y);
}

static void
paste_cb (GtkWidget *button, GtkWidget *fixed)
{
  GdkClipboard *clipboard;

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
  if (gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), GTK_TYPE_DEMO_WIDGET))
    {
      g_print ("Paste %s\n", g_type_name (GTK_TYPE_DEMO_WIDGET));
      gdk_clipboard_read_value_async (clipboard, GTK_TYPE_DEMO_WIDGET, 0, NULL, value_read, fixed);
    }
  else
    g_print ("Don't know how to handle clipboard contents\n");

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
edit_label_done (GtkWidget *entry, gpointer data)
{
  GtkWidget *fixed = gtk_widget_get_parent (entry);
  GtkWidget *label;
  int x, y;

  gtk_fixed_get_child_position (GTK_FIXED (fixed), entry, &x, &y);

  label = GTK_WIDGET (g_object_get_data (G_OBJECT (entry), "label"));
  gtk_label_set_text (GTK_LABEL (label), gtk_editable_get_text (GTK_EDITABLE (entry)));

  gtk_widget_destroy (entry);
}

static void
edit_cb (GtkWidget *button, GtkWidget *child)
{
  GtkWidget *fixed = gtk_widget_get_parent (child);
  int x, y;

  gtk_fixed_get_child_position (GTK_FIXED (fixed), child, &x, &y);

  if (GTK_IS_LABEL (child))
    {
      GtkWidget *entry = gtk_entry_new ();

      g_object_set_data (G_OBJECT (entry), "label", child);

      gtk_editable_set_text (GTK_EDITABLE (entry), gtk_label_get_text (GTK_LABEL (child)));
      gtk_editable_set_width_chars (GTK_EDITABLE (entry), 12);
      g_signal_connect (entry, "activate", G_CALLBACK (edit_label_done), NULL);
      gtk_fixed_put (GTK_FIXED (fixed), entry, x, y);
      gtk_widget_grab_focus (entry);
    }
  else if (GTK_IS_SPINNER (child))
    {
      gboolean active;

      g_object_get (child, "spinning", &active, NULL);
      g_object_set (child, "spinning", !active, NULL);
    }

  if (button)
    gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  GtkWidget *widget;
  GtkWidget *child;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = gtk_widget_pick (widget, x, y, GTK_PICK_DEFAULT);

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *menu;
      GtkWidget *box;
      GtkWidget *item;
      GdkClipboard *clipboard;

      pos_x = x;
      pos_y = y;

      menu = gtk_popover_new ();
      gtk_widget_set_parent (menu, widget);
      gtk_popover_set_has_arrow (GTK_POPOVER (menu), FALSE);
      gtk_popover_set_pointing_to (GTK_POPOVER (menu), &(GdkRectangle){ x, y, 1, 1});
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (menu), box);

      item = gtk_button_new_with_label ("New Label");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      g_signal_connect (item, "clicked", G_CALLBACK (new_label_cb), widget);
      gtk_container_add (GTK_CONTAINER (box), item);
      item = gtk_button_new_with_label ("New Spinner");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      g_signal_connect (item, "clicked", G_CALLBACK (new_spinner_cb), widget);
      gtk_container_add (GTK_CONTAINER (box), item);

      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_container_add (GTK_CONTAINER (box), item);

      item = gtk_button_new_with_label ("Edit");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (edit_cb), child);
      gtk_container_add (GTK_CONTAINER (box), item);

      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_container_add (GTK_CONTAINER (box), item);

      item = gtk_button_new_with_label ("Cut");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (cut_cb), child);
      gtk_container_add (GTK_CONTAINER (box), item);
      item = gtk_button_new_with_label ("Copy");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (copy_cb), child);
      gtk_container_add (GTK_CONTAINER (box), item);
      item = gtk_button_new_with_label ("Paste");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      clipboard = gdk_display_get_clipboard (gdk_display_get_default ());
      gtk_widget_set_sensitive (item,
                                gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), GTK_TYPE_DEMO_WIDGET));
      g_signal_connect (item, "clicked", G_CALLBACK (paste_cb), widget);
      gtk_container_add (GTK_CONTAINER (box), item);
      item = gtk_button_new_with_label ("Delete");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (delete_cb), child);
      gtk_container_add (GTK_CONTAINER (box), item);

      gtk_popover_popup (GTK_POPOVER (menu));
    }
}

static void
released_cb (GtkGesture *gesture,
             int         n_press,
             double      x,
             double      y,
             gpointer    data)
{
  GtkWidget *widget;
  GtkWidget *child;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = gtk_widget_pick (widget, x, y, 0);

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) == GDK_BUTTON_PRIMARY)
    {
      if (child != NULL && child != widget)
        edit_cb (NULL, child);
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

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Drag-and-drop");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      fixed = gtk_fixed_new ();
      gtk_container_add (GTK_CONTAINER (vbox), fixed);
      gtk_widget_set_hexpand (fixed, TRUE);
      gtk_widget_set_vexpand (fixed, TRUE);

      multipress = gtk_gesture_click_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (multipress), 0);
      g_signal_connect (multipress, "pressed", G_CALLBACK (pressed_cb), NULL);
      g_signal_connect (multipress, "released", G_CALLBACK (released_cb), NULL);
      gtk_widget_add_controller (fixed, GTK_EVENT_CONTROLLER (multipress));

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
