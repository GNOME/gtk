/* Benchmark/Transformation
 * #Keywords: GtkLayoutManager, GskTransform
 *
 * This demo shows how to use transforms in a nontrivial
 * way with a custom layout manager. The layout manager places
 * content on a rotating sphere. The direction of the rotation
 * can be changed using arrow keys.
 */

#include <gtk/gtk.h>

#include "demo2widget.h"
#include "demo2layout.h"
#include "demochild.h"

static GtkWidget *
get_child (unsigned int i)
{
  const char *name[] = {
    "action-unavailable-symbolic",
    "address-book-new-symbolic",
    "application-exit-symbolic",
    "appointment-new-symbolic",
    "bookmark-new-symbolic",
    "call-start-symbolic",
    "call-stop-symbolic",
    "camera-switch-symbolic",
    "chat-message-new-symbolic",
    "color-select-symbolic",
    "contact-new-symbolic",
    "document-edit-symbolic",
    "document-new-symbolic",
    "document-open-recent-symbolic",
    "document-open-symbolic",
    "document-page-setup-symbolic",
    "document-print-preview-symbolic",
    "document-print-symbolic",
    "document-properties-symbolic",
    "document-revert-symbolic-rtl",
    "document-revert-symbolic",
    "document-save-as-symbolic",
    "document-save-symbolic",
    "document-send-symbolic",
    "edit-clear-all-symbolic",
    "edit-clear-symbolic-rtl",
    "edit-clear-symbolic",
    "edit-copy-symbolic",
    "edit-cut-symbolic",
    "edit-delete-symbolic",
    "edit-find-replace-symbolic",
    "edit-find-symbolic",
    "edit-paste-symbolic",
    "edit-redo-symbolic-rtl",
    "edit-redo-symbolic",
    "edit-select-all-symbolic",
    "edit-select-symbolic",
    "edit-undo-symbolic-rtl",
    "edit-undo-symbolic",
    "error-correct-symbolic",
    "find-location-symbolic",
    "folder-new-symbolic",
    "font-select-symbolic",
    "format-indent-less-symbolic-rtl",
    "format-indent-less-symbolic",
    "format-indent-more-symbolic-rtl",
    "format-indent-more-symbolic",
    "format-justify-center-symbolic",
    "format-justify-fill-symbolic",
    "format-justify-left-symbolic",
    "format-justify-right-symbolic",
    "format-text-bold-symbolic",
    "format-text-direction-symbolic-rtl",
    "format-text-direction-symbolic",
    "format-text-italic-symbolic",
    "format-text-strikethrough-symbolic",
    "format-text-underline-symbolic",
    "go-bottom-symbolic",
    "go-down-symbolic",
    "go-first-symbolic-rtl",
    "go-first-symbolic",
    "go-home-symbolic",
    "go-jump-symbolic-rtl",
    "go-jump-symbolic",
    "go-last-symbolic-rtl",
    "go-last-symbolic",
    "go-next-symbolic-rtl",
    "go-next-symbolic",
    "go-previous-symbolic-rtl",
    "go-previous-symbolic",
    "go-top-symbolic",
    "go-up-symbolic",
    "help-about-symbolic",
    "insert-image-symbolic",
    "insert-link-symbolic",
    "insert-object-symbolic",
    "insert-text-symbolic",
    "list-add-symbolic",
    "list-remove-all-symbolic",
    "list-remove-symbolic",
    "mail-forward-symbolic",
    "mail-mark-important-symbolic",
    "mail-mark-junk-symbolic",
    "mail-mark-notjunk-symbolic",
    "mail-message-new-symbolic",
    "mail-reply-all-symbolic",
    "mail-reply-sender-symbolic",
    "mail-send-receive-symbolic",
    "mail-send-symbolic",
    "mark-location-symbolic",
    "media-eject-symbolic",
    "media-playback-pause-symbolic",
    "media-playback-start-symbolic",
    "media-playback-stop-symbolic",
    "media-record-symbolic",
    "media-seek-backward-symbolic",
    "media-seek-forward-symbolic",
    "media-skip-backward-symbolic",
    "media-skip-forward-symbolic",
    "media-view-subtitles-symbolic",
    "object-flip-horizontal-symbolic",
    "object-flip-vertical-symbolic",
    "object-rotate-left-symbolic",
    "object-rotate-right-symbolic",
    "object-select-symbolic",
    "open-menu-symbolic",
    "process-stop-symbolic",
    "send-to-symbolic",
    "sidebar-hide-symbolic",
    "sidebar-show-symbolic",
    "star-new-symbolic",
    "system-log-out-symbolic",
    "system-reboot-symbolic",
    "system-run-symbolic",
    "system-search-symbolic",
    "system-shutdown-symbolic",
    "system-switch-user-symbolic",
    "tab-new-symbolic",
    "tools-check-spelling-symbolic",
    "value-decrease-symbolic",
    "value-increase-symbolic",
    "view-app-grid-symbolic",
    "view-conceal-symbolic",
    "view-continuous-symbolic",
    "view-dual-symbolic",
    "view-fullscreen-symbolic",
    "view-grid-symbolic",
    "view-list-bullet-symbolic",
    "view-list-ordered-symbolic",
    "view-list-symbolic",
    "view-mirror-symbolic",
    "view-more-horizontal-symbolic",
    "view-more-symbolic",
    "view-paged-symbolic",
    "view-pin-symbolic",
    "view-refresh-symbolic",
    "view-restore-symbolic",
    "view-reveal-symbolic",
    "view-sort-ascending-symbolic",
    "view-sort-descending-symbolic",
    "zoom-fit-best-symbolic",
    "zoom-in-symbolic",
    "zoom-original-symbolic",
    "zoom-out-symbolic",
  };
  GtkWidget *child;

  child = gtk_image_new_from_icon_name (name[i % G_N_ELEMENTS (name)]);

  gtk_widget_set_margin_start (child, 4);
  gtk_widget_set_margin_end (child, 4);
  gtk_widget_set_margin_top (child, 4);
  gtk_widget_set_margin_bottom (child, 4);

  return child;
}

static GtkWidget *
get_label_child (unsigned int num)
{
  gunichar ch;
  char buf[12] = { 0, };
  int written = 0;
  GtkWidget *label, *frame;

  for (unsigned int i = 0; i < 2; i++)
    {
      ch = g_random_int_range (30, 128);
      written = g_unichar_to_utf8 (ch, &buf[written]);
    }

  label = gtk_label_new (buf);
  frame = gtk_aspect_frame_new (0.5, 0.5, 1, FALSE);
  gtk_aspect_frame_set_child (GTK_ASPECT_FRAME (frame), label);

  return frame;
}

static void
repopulate (GtkWindow    *window,
            unsigned int  kind)
{
  GtkWidget *widget;
  GtkWidget *child;
  GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL;
  int direction = 1;

  widget = gtk_window_get_child (window);
  if (widget)
    {
      orientation = demo2_widget_get_orientation (DEMO2_WIDGET (widget));
      direction = demo2_widget_get_direction (DEMO2_WIDGET (widget));
    }

  widget = demo2_widget_new ();

  for (unsigned i = 0; i < 18 * 36; i++)
    {
      if (kind == 0)
        child = get_child (i);
      else
        child = get_label_child (i);

      demo2_widget_add_child (DEMO2_WIDGET (widget), child);
    }

  if (kind == 0)
    gtk_window_set_title (GTK_WINDOW (window), "Transformed icons");
  else
    gtk_window_set_title (GTK_WINDOW (window), "Transformed text");

  gtk_window_set_child (GTK_WINDOW (window), widget);

  demo2_widget_set_orientation (DEMO2_WIDGET (widget), orientation);
  demo2_widget_set_direction (DEMO2_WIDGET (widget), direction);
}

#define N_KINDS 2
static unsigned int kind = 0;

static void
prev_cb (GtkWindow *window)
{
  kind = (kind + N_KINDS - 1) % N_KINDS;
  repopulate (window, kind);
}

static void
next_cb (GtkWindow *window)
{
  kind = (kind + 1) % N_KINDS;
  repopulate (window, kind);
}

static gboolean
update_fps (gpointer data)
{
  GtkWidget *label = data;
  GdkFrameClock *frame_clock;
  double fps;
  char *str;

  frame_clock = gtk_widget_get_frame_clock (label);

  fps = gdk_frame_clock_get_fps (frame_clock);
  str = g_strdup_printf ("%.2f fps", fps);
  gtk_label_set_label (GTK_LABEL (label), str);
  g_free (str);

  return G_SOURCE_CONTINUE;
}

static void
remove_timeout (gpointer data)
{
  g_source_remove (GPOINTER_TO_UINT (data));
}

GtkWidget *
do_layoutmanager2 (GtkWidget *parent)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *headerbar, *box, *button, *label;
      PangoAttrList *attrs;
      guint id;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Layout Manager — Transformation");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 620);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      headerbar = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_add_css_class (box, "linked");
      gtk_header_bar_pack_start (GTK_HEADER_BAR (headerbar), box);
      button = gtk_button_new_from_icon_name ("go-previous-symbolic");
      gtk_widget_set_focus_on_click (button, FALSE);
      g_signal_connect_swapped (button, "clicked", G_CALLBACK (prev_cb), window);
      gtk_box_append (GTK_BOX (box), button);
      button = gtk_button_new_from_icon_name ("go-next-symbolic");
      gtk_widget_set_focus_on_click (button, FALSE);
      g_signal_connect_swapped (button, "clicked", G_CALLBACK (next_cb), window);
      gtk_box_append (GTK_BOX (box), button);
      label = gtk_label_new ("");
      attrs = pango_attr_list_from_string ("0 -1 font-features \"tnum=1\"");
      gtk_label_set_attributes (GTK_LABEL (label), attrs);
      pango_attr_list_unref (attrs);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (headerbar), label);
      id = g_timeout_add_full (G_PRIORITY_HIGH, 500, update_fps, label, NULL);
      g_object_set_data_full (G_OBJECT (label), "timeout",
                              GUINT_TO_POINTER (id), remove_timeout);

      repopulate (GTK_WINDOW (window), 0);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;

}
      GtkWidget *child;
