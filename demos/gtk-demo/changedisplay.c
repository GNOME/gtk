/* Change Display
 *
 * Demonstrates migrating a window between different displays.
 * A display is a mouse and keyboard with some number of
 * associated monitors. The neat thing about having multiple
 * displays is that they can be on a completely separate
 * computers, as long as there is a network connection to the
 * computer where the application is running.
 *
 * Only some of the windowing systems where GTK runs have the
 * concept of multiple displays. (The X Window System is the
 * main example.) Other windowing systems can only handle one
 * keyboard and mouse, and combine all monitors into
 * a single display.
 *
 * This is a moderately complex example, and demonstrates:
 *
 *  - Tracking the currently open displays
 *
 *  - Changing the display for a window
 *
 *  - Letting the user choose a window by clicking on it
 *
 *  - Using GtkListStore and GtkTreeView
 *
 *  - Using GtkDialog
 */
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

/* The ChangeDisplayInfo structure corresponds to a toplevel window and
 * holds pointers to widgets inside the toplevel window along with other
 * information about the contents of the window.
 * This is a common organizational structure in real applications.
 */
typedef struct _ChangeDisplayInfo ChangeDisplayInfo;

struct _ChangeDisplayInfo
{
  GtkWidget *window;
  GtkSizeGroup *size_group;

  GtkTreeModel *display_model;

  GdkDisplay *current_display;
};

/* These enumerations provide symbolic names for the columns
 * in the two GtkListStore models.
 */
enum
{
  DISPLAY_COLUMN_NAME,
  DISPLAY_COLUMN_DISPLAY,
  DISPLAY_NUM_COLUMNS
};

enum
{
  SCREEN_COLUMN_NUMBER,
  SCREEN_COLUMN_SCREEN,
  SCREEN_NUM_COLUMNS
};

/* Finds the toplevel window under the mouse pointer, if any.
 */
static GtkWidget *
find_toplevel_at_pointer (GdkDisplay *display)
{
  GdkSurface *pointer_window;
  GtkWidget *widget = NULL;

  pointer_window = gdk_device_get_surface_at_position (gtk_get_current_event_device (), NULL, NULL);

  if (pointer_window)
    widget = GTK_WIDGET (gtk_native_get_for_surface (pointer_window));

  return widget;
}

static void
released_cb (GtkGestureClick *gesture,
             guint            n_press,
             gdouble          x,
             gdouble          y,
             gboolean        *clicked)
{
  *clicked = TRUE;
}

/* Asks the user to click on a window, then waits for them click
 * the mouse. When the mouse is released, returns the toplevel
 * window under the pointer, or NULL, if there is none.
 */
static GtkWidget *
query_for_toplevel (GdkDisplay *display,
                    const char *prompt)
{
  GtkWidget *popup, *label, *frame;
  GdkCursor *cursor;
  GtkWidget *toplevel = NULL;
  GdkDevice *device;

  popup = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_display (GTK_WINDOW (popup), display);
  gtk_window_set_modal (GTK_WINDOW (popup), TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (popup), frame);

  label = gtk_label_new (prompt);
  g_object_set (label, "margin", 10, NULL);
  gtk_container_add (GTK_CONTAINER (frame), label);

  gtk_widget_show (popup);
  cursor = gdk_cursor_new_from_name ("crosshair", NULL);
  device = gtk_get_current_event_device ();

  if (gdk_seat_grab (gdk_device_get_seat (device),
                     gtk_native_get_surface (GTK_NATIVE (popup)),
                     GDK_SEAT_CAPABILITY_ALL_POINTING,
                     FALSE, cursor, NULL, NULL, NULL) == GDK_GRAB_SUCCESS)
    {
      GtkGesture *gesture = gtk_gesture_click_new ();
      gboolean clicked = FALSE;

      g_signal_connect (gesture, "released",
                        G_CALLBACK (released_cb), &clicked);
      gtk_widget_add_controller (popup, GTK_EVENT_CONTROLLER (gesture));

      /* Process events until clicked is set by our button release event handler.
       * We pass in may_block=TRUE since we want to wait if there
       * are no events currently.
       */
      while (!clicked)
        g_main_context_iteration (NULL, TRUE);

      gdk_seat_ungrab (gdk_device_get_seat (device));

      toplevel = find_toplevel_at_pointer (display);
      if (toplevel == popup)
        toplevel = NULL;
    }

  g_object_unref (cursor);
  gtk_widget_destroy (popup);

  return toplevel;
}

/* Prompts the user for a toplevel window to move, and then moves
 * that window to the currently selected display
 */
static void
query_change_display (ChangeDisplayInfo *info)
{
  GdkDisplay *display = gtk_widget_get_display (info->window);
  GtkWidget *toplevel;

  toplevel = query_for_toplevel (display,
                                 "Please select the toplevel\n"
                                 "to move to the new display");

  if (toplevel)
    gtk_window_set_display (GTK_WINDOW (toplevel), info->current_display);
  else
    gdk_display_beep (display);
}

/* Called when the user clicks on a button in our dialog or
 * closes the dialog through the window manager. Unless the
 * "Change" button was clicked, we destroy the dialog.
 */
static void
response_cb (GtkDialog         *dialog,
             gint               response_id,
             ChangeDisplayInfo *info)
{
  if (response_id == GTK_RESPONSE_OK)
    query_change_display (info);
  else
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

/* Called when the user clicks on "Open..." in the display
 * frame. Prompts for a new display, and then opens a connection
 * to that display.
 */
static void
open_display_cb (GtkWidget         *button,
                 ChangeDisplayInfo *info)
{
  GtkWidget *content_area;
  GtkWidget *dialog;
  GtkWidget *display_entry;
  GtkWidget *dialog_label;
  gchar *new_screen_name = NULL;
  GdkDisplay *result = NULL;

  dialog = gtk_dialog_new_with_buttons ("Open Display",
                                        GTK_WINDOW (info->window),
                                        GTK_DIALOG_MODAL,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_OK"), GTK_RESPONSE_OK,
                                        NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  display_entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (display_entry), TRUE);
  dialog_label =
    gtk_label_new ("Please enter the name of\nthe new display\n");

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  gtk_container_add (GTK_CONTAINER (content_area), dialog_label);
  gtk_container_add (GTK_CONTAINER (content_area), display_entry);

  gtk_widget_grab_focus (display_entry);

  while (!result)
    {
      gint response_id = gtk_dialog_run (GTK_DIALOG (dialog));
      if (response_id != GTK_RESPONSE_OK)
        break;

      new_screen_name = gtk_editable_get_chars (GTK_EDITABLE (display_entry),
                                                0, -1);

      if (strcmp (new_screen_name, "") != 0)
        {
          result = gdk_display_open (new_screen_name);
          if (!result)
            {
              gchar *error_msg =
                g_strdup_printf  ("Can't open display:\n\t%s\nplease try another one\n",
                                  new_screen_name);
              gtk_label_set_text (GTK_LABEL (dialog_label), error_msg);
              g_free (error_msg);
            }

          g_free (new_screen_name);
        }
    }

  gtk_widget_destroy (dialog);
}

/* Called when the user clicks on the "Close" button in the
 * "Display" frame. Closes the selected display.
 */
static void
close_display_cb (GtkWidget         *button,
                  ChangeDisplayInfo *info)
{
  if (info->current_display)
    gdk_display_close (info->current_display);
}

/* Called when the selected row in the display list changes.
 * Updates info->current_display, then refills the list of
 * screens.
 */
static void
display_changed_cb (GtkTreeSelection  *selection,
                    ChangeDisplayInfo *info)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  if (info->current_display)
    g_object_unref (info->current_display);
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter,
                        DISPLAY_COLUMN_DISPLAY, &info->current_display,
                        -1);
  else
    info->current_display = NULL;
}

/* This function is used both for creating the "Display" and
 * "Screen" frames, since they have a similar structure. The
 * caller hooks up the right context for the value returned
 * in tree_view, and packs any relevant buttons into button_vbox.
 */
static void
create_frame (ChangeDisplayInfo *info,
              const char        *title,
              GtkWidget        **frame,
              GtkWidget        **tree_view,
              GtkWidget        **button_vbox)
{
  GtkTreeSelection *selection;
  GtkWidget *scrollwin;
  GtkWidget *hbox;

  *frame = gtk_frame_new (title);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  g_object_set (hbox, "margin", 8, NULL);
  gtk_container_add (GTK_CONTAINER (*frame), hbox);

  scrollwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollwin),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrollwin),
                                       GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (hbox), scrollwin);

  *tree_view = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (*tree_view), FALSE);
  gtk_container_add (GTK_CONTAINER (scrollwin), *tree_view);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (*tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

  *button_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (hbox), *button_vbox);

  if (!info->size_group)
    info->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_size_group_add_widget (GTK_SIZE_GROUP (info->size_group), *button_vbox);
}

/* If we have a stack of buttons, it often looks better if their contents
 * are left-aligned, rather than centered. This function creates a button
 * and left-aligns it contents.
 */
GtkWidget *
left_align_button_new (const char *label)
{
  GtkWidget *button = gtk_button_new_with_mnemonic (label);
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (button));

  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);

  return button;
}

/* Creates the "Display" frame in the main window.
 */
GtkWidget *
create_display_frame (ChangeDisplayInfo *info)
{
  GtkWidget *frame;
  GtkWidget *tree_view;
  GtkWidget *button_vbox;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  GtkWidget *button;

  create_frame (info, "Display", &frame, &tree_view, &button_vbox);

  button = left_align_button_new ("_Open...");
  g_signal_connect (button, "clicked",  G_CALLBACK (open_display_cb), info);
  gtk_container_add (GTK_CONTAINER (button_vbox), button);

  button = left_align_button_new ("_Close");
  g_signal_connect (button, "clicked",  G_CALLBACK (close_display_cb), info);
  gtk_container_add (GTK_CONTAINER (button_vbox), button);

  info->display_model = (GtkTreeModel *)gtk_list_store_new (DISPLAY_NUM_COLUMNS,
                                                            G_TYPE_STRING,
                                                            GDK_TYPE_DISPLAY);

  gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), info->display_model);

  column = gtk_tree_view_column_new_with_attributes ("Name",
                                                     gtk_cell_renderer_text_new (),
                                                     "text", DISPLAY_COLUMN_NAME,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (display_changed_cb), info);

  return frame;
}

/* Called when one of the currently open displays is closed.
 * Remove it from our list of displays.
 */
static void
display_closed_cb (GdkDisplay        *display,
                   gboolean           is_error,
                   ChangeDisplayInfo *info)
{
  GtkTreeIter iter;
  gboolean valid;

  for (valid = gtk_tree_model_get_iter_first (info->display_model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (info->display_model, &iter))
    {
      GdkDisplay *tmp_display;

      gtk_tree_model_get (info->display_model, &iter,
                          DISPLAY_COLUMN_DISPLAY, &tmp_display,
                          -1);
      if (tmp_display == display)
        {
          gtk_list_store_remove (GTK_LIST_STORE (info->display_model), &iter);
          break;
        }
    }
}

/* Adds a new display to our list of displays, and connects
 * to the "closed" signal so that we can remove it from the
 * list of displays again.
 */
static void
add_display (ChangeDisplayInfo *info,
             GdkDisplay        *display)
{
  const gchar *name = gdk_display_get_name (display);
  GtkTreeIter iter;

  gtk_list_store_append (GTK_LIST_STORE (info->display_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (info->display_model), &iter,
                      DISPLAY_COLUMN_NAME, name,
                      DISPLAY_COLUMN_DISPLAY, display,
                      -1);

  g_signal_connect (display, "closed",
                    G_CALLBACK (display_closed_cb), info);
}

/* Called when a new display is opened
 */
static void
display_opened_cb (GdkDisplayManager *manager,
                   GdkDisplay        *display,
                   ChangeDisplayInfo *info)
{
  add_display (info, display);
}

/* Adds all currently open displays to our list of displays,
 * and set up a signal connection so that we'll be notified
 * when displays are opened in the future as well.
 */
static void
initialize_displays (ChangeDisplayInfo *info)
{
  GdkDisplayManager *manager = gdk_display_manager_get ();
  GSList *displays = gdk_display_manager_list_displays (manager);
  GSList *tmp_list;

  for (tmp_list = displays; tmp_list; tmp_list = tmp_list->next)
    add_display (info, tmp_list->data);

  g_slist_free (tmp_list);

  g_signal_connect (manager, "display-opened",
                    G_CALLBACK (display_opened_cb), info);
}

/* Cleans up when the toplevel is destroyed; we remove the
 * connections we use to track currently open displays, then
 * free the ChangeDisplayInfo structure.
 */
static void
destroy_info (ChangeDisplayInfo *info)
{
  GdkDisplayManager *manager = gdk_display_manager_get ();
  GSList *displays = gdk_display_manager_list_displays (manager);
  GSList *tmp_list;

  g_signal_handlers_disconnect_by_func (manager,
                                        display_opened_cb,
                                        info);

  for (tmp_list = displays; tmp_list; tmp_list = tmp_list->next)
    g_signal_handlers_disconnect_by_func (tmp_list->data,
                                          display_closed_cb,
                                          info);

  g_slist_free (tmp_list);

  g_object_unref (info->size_group);
  g_object_unref (info->display_model);

  if (info->current_display)
    g_object_unref (info->current_display);

  g_free (info);
}

static void
destroy_cb (GObject            *object,
            ChangeDisplayInfo **info)
{
  destroy_info (*info);
  *info = NULL;
}

/* Main entry point. If the dialog for this demo doesn't yet exist, creates
 * it. Otherwise, destroys it.
 */
GtkWidget *
do_changedisplay (GtkWidget *do_widget)
{
  static ChangeDisplayInfo *info = NULL;

  if (!info)
    {
      GtkWidget *content_area;
      GtkWidget *vbox;
      GtkWidget *frame;

      info = g_new0 (ChangeDisplayInfo, 1);

      info->window = gtk_dialog_new_with_buttons ("Change Display",
                                                  GTK_WINDOW (do_widget),
                                                  0,
                                                  "Close", GTK_RESPONSE_CLOSE,
                                                  "Change", GTK_RESPONSE_OK,
                                                  NULL);

      gtk_window_set_default_size (GTK_WINDOW (info->window), 300, 400);

      g_signal_connect (info->window, "response",
                        G_CALLBACK (response_cb), info);
      g_signal_connect (info->window, "destroy",
                        G_CALLBACK (destroy_cb), &info);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (info->window));

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      g_object_set (vbox, "margin", 8, NULL);
      gtk_container_add (GTK_CONTAINER (content_area), vbox);

      frame = create_display_frame (info);
      gtk_container_add (GTK_CONTAINER (vbox), frame);

      initialize_displays (info);

      gtk_widget_show (info->window);
      return info->window;
    }
  else
    {
      gtk_widget_destroy (info->window);
      return NULL;
    }
}
