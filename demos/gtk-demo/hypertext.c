/* Text View/Hypertext
 *
 * Usually, tags modify the appearance of text in the view, e.g. making it
 * bold or colored or underlined. But tags are not restricted to appearance.
 * They can also affect the behavior of mouse and key presses, as this demo
 * shows.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

/* Inserts a piece of text into the buffer, giving it the usual
 * appearance of a hyperlink in a web browser: blue and underlined.
 * Additionally, attaches some data on the tag, to make it recognizable
 * as a link.
 */
static void
insert_link (GtkTextBuffer *buffer,
             GtkTextIter   *iter,
             gchar         *text,
             gint           page)
{
  GtkTextTag *tag;

  tag = gtk_text_buffer_create_tag (buffer, NULL,
                                    "foreground", "blue",
                                    "underline", PANGO_UNDERLINE_SINGLE,
                                    NULL);
  g_object_set_data (G_OBJECT (tag), "page", GINT_TO_POINTER (page));
  gtk_text_buffer_insert_with_tags (buffer, iter, text, -1, tag, NULL);
}

/* Fills the buffer with text and interspersed links. In any real
 * hypertext app, this method would parse a file to identify the links.
 */
static void
show_page (GtkTextBuffer *buffer,
           gint           page)
{
  GtkTextIter iter;

  gtk_text_buffer_set_text (buffer, "", 0);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  if (page == 1)
    {
      gtk_text_buffer_insert (buffer, &iter, "Some text to show that simple ", -1);
      insert_link (buffer, &iter, "hyper text", 3);
      gtk_text_buffer_insert (buffer, &iter, " can easily be realized with ", -1);
      insert_link (buffer, &iter, "tags", 2);
      gtk_text_buffer_insert (buffer, &iter, ".", -1);
    }
  else if (page == 2)
    {
      gtk_text_buffer_insert (buffer, &iter,
                              "A tag is an attribute that can be applied to some range of text. "
                              "For example, a tag might be called \"bold\" and make the text inside "
                              "the tag bold. However, the tag concept is more general than that; "
                              "tags don't have to affect appearance. They can instead affect the "
                              "behavior of mouse and key presses, \"lock\" a range of text so the "
                              "user can't edit it, or countless other things.\n", -1);
      insert_link (buffer, &iter, "Go back", 1);
    }
  else if (page == 3)
    {
      GtkTextTag *tag;

      tag = gtk_text_buffer_create_tag (buffer, NULL,
                                        "weight", PANGO_WEIGHT_BOLD,
                                        NULL);
      gtk_text_buffer_insert_with_tags (buffer, &iter, "hypertext:\n", -1, tag, NULL);
      gtk_text_buffer_insert (buffer, &iter,
                              "machine-readable text that is not sequential but is organized "
                              "so that related items of information are connected.\n", -1);
      insert_link (buffer, &iter, "Go back", 1);
    }
}

/* Looks at all tags covering the position of iter in the text view,
 * and if one of them is a link, follow it by showing the page identified
 * by the data attached to it.
 */
static void
follow_if_link (GtkWidget   *text_view,
                GtkTextIter *iter)
{
  GSList *tags = NULL, *tagp = NULL;

  tags = gtk_text_iter_get_tags (iter);
  for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
    {
      GtkTextTag *tag = tagp->data;
      gint page = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "page"));

      if (page != 0)
        {
          show_page (gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view)), page);
          break;
        }
    }

  if (tags)
    g_slist_free (tags);
}

/* Links can be activated by pressing Enter.
 */
static gboolean
key_pressed (GtkEventController *controller,
             guint               keyval,
             guint               keycode,
             GdkModifierType     modifiers,
             GtkWidget          *text_view)
{
  GtkTextIter iter;
  GtkTextBuffer *buffer;

  switch (keyval)
    {
      case GDK_KEY_Return:
      case GDK_KEY_KP_Enter:
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
        gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                          gtk_text_buffer_get_insert (buffer));
        follow_if_link (text_view, &iter);
        break;

      default:
        break;
    }

  return GDK_EVENT_PROPAGATE;
}

static void set_cursor_if_appropriate (GtkTextView *text_view,
                                       gint         x,
                                       gint         y);

static void
released_cb (GtkGestureClick *gesture,
             guint            n_press,
             gdouble          x,
             gdouble          y,
             GtkWidget       *text_view)
{
  GtkTextIter start, end, iter;
  GtkTextBuffer *buffer;
  int tx, ty;

  if (gtk_gesture_single_get_button (GTK_GESTURE_SINGLE (gesture)) > 1)
    return;

  gtk_text_view_widget_to_buffer_coords (GTK_TEXT_VIEW (text_view),
                                         x, y, &tx, &ty);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  /* we shouldn't follow a link if the user has selected something */
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  if (gtk_text_iter_get_offset (&start) != gtk_text_iter_get_offset (&end))
    return;

  if (gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (text_view), &iter, x, y))
    follow_if_link (text_view, &iter);
}

static void
motion_cb (GtkEventControllerMotion *controller,
           gdouble                   x,
           gdouble                   y,
           GtkTextView              *text_view)
{
  set_cursor_if_appropriate (text_view, x, y);
}

static gboolean hovering_over_link = FALSE;

/* Looks at all tags covering the position (x, y) in the text view,
 * and if one of them is a link, change the cursor to the "hands" cursor
 * typically used by web browsers.
 */
static void
set_cursor_if_appropriate (GtkTextView    *text_view,
                           gint            x,
                           gint            y)
{
  GSList *tags = NULL, *tagp = NULL;
  GtkTextIter iter;
  gboolean hovering = FALSE;

  if (gtk_text_view_get_iter_at_location (text_view, &iter, x, y))
    {
      tags = gtk_text_iter_get_tags (&iter);
      for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
        {
          GtkTextTag *tag = tagp->data;
          gint page = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (tag), "page"));

          if (page != 0)
            {
              hovering = TRUE;
              break;
            }
        }
    }

  if (hovering != hovering_over_link)
    {
      hovering_over_link = hovering;

      if (hovering_over_link)
        gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "pointer");
      else
        gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), "text");
    }

  if (tags)
    g_slist_free (tags);
}

GtkWidget *
do_hypertext (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *view;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      GtkEventController *controller;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Hypertext");
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 450, 450);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      view = gtk_text_view_new ();
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 20);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 20);
      controller = gtk_event_controller_key_new ();
      g_signal_connect (controller, "key-pressed", G_CALLBACK (key_pressed), view);
      gtk_widget_add_controller (view, controller);

      controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
      g_signal_connect (controller, "released",
                        G_CALLBACK (released_cb), view);
      gtk_widget_add_controller (view, controller);

      controller = gtk_event_controller_motion_new ();
      g_signal_connect (controller, "motion",
                        G_CALLBACK (motion_cb), view);
      gtk_widget_add_controller (view, controller);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (window), sw);
      gtk_container_add (GTK_CONTAINER (sw), view);

      show_page (buffer, 1);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
