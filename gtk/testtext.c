#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

typedef struct _Buffer Buffer;
typedef struct _View View;

static gint untitled_serial = 1;

GSList *active_window_stack = NULL;

struct _Buffer
{
  gint refcount;
  GtkTextBuffer *buffer;
  char *filename;
  gint untitled_serial;
  GtkTextTag *invisible_tag;
  GtkTextTag *not_editable_tag;
  GtkTextTag *found_text_tag;
  GtkTextTag *custom_tabs_tag;
  GSList *color_tags;
  guint color_cycle_timeout;
  gdouble start_hue;
};

struct _View
{
  GtkWidget *window;
  GtkWidget *text_view;
  GtkAccelGroup *accel_group;
  GtkItemFactory *item_factory;
  Buffer *buffer;
};

static void push_active_window (GtkWindow *window);
static void pop_active_window (void);
static GtkWindow *get_active_window (void);

static Buffer * create_buffer      (void);
static gboolean check_buffer_saved (Buffer *buffer);
static gboolean save_buffer        (Buffer *buffer);
static gboolean save_as_buffer     (Buffer *buffer);
static char *   buffer_pretty_name (Buffer *buffer);
static void     buffer_filename_set (Buffer *buffer);
static void     buffer_search_forward (Buffer *buffer,
                                       const char *str,
                                       View *view);
static void     buffer_search_backward (Buffer *buffer,
                                       const char *str,
                                       View *view);
static void     buffer_set_colors      (Buffer  *buffer,
                                        gboolean enabled);
static void     buffer_cycle_colors    (Buffer  *buffer);

static View *view_from_widget (GtkWidget *widget);

static View *create_view      (Buffer *buffer);
static void  check_close_view (View   *view);
static void  close_view       (View   *view);
static void  view_set_title   (View   *view);
static void  view_init_menus  (View   *view);
static void  view_add_example_widgets (View *view);

GSList *buffers = NULL;
GSList *views = NULL;

static void
push_active_window (GtkWindow *window)
{
  g_object_ref (G_OBJECT (window));
  active_window_stack = g_slist_prepend (active_window_stack, window);
}

static void
pop_active_window (void)
{
  gtk_object_unref (active_window_stack->data);
  active_window_stack = g_slist_delete_link (active_window_stack, active_window_stack);
}

static GtkWindow *
get_active_window (void)
{
  if (active_window_stack)
    return active_window_stack->data;
  else
    return NULL;
}

/*
 * Filesel utility function
 */

typedef gboolean (*FileselOKFunc) (const char *filename, gpointer data);

static void
filesel_ok_cb (GtkWidget *button, GtkWidget *filesel)
{
  FileselOKFunc ok_func = g_object_get_data (G_OBJECT (filesel), "ok-func");
  gpointer data = g_object_get_data (G_OBJECT (filesel), "ok-data");
  gint *result = g_object_get_data (G_OBJECT (filesel), "ok-result");
  
  gtk_widget_hide (filesel);
  
  if ((*ok_func) (gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel)), data))
    {
      gtk_widget_destroy (filesel);
      *result = TRUE;
    }
  else
    gtk_widget_show (filesel);
}

gboolean
filesel_run (GtkWindow    *parent, 
	     const char   *title,
	     const char   *start_file,
	     FileselOKFunc func,
	     gpointer      data)
{
  GtkWidget *filesel = gtk_file_selection_new (title);
  gboolean result = FALSE;

  if (!parent)
    parent = get_active_window ();
  
  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (filesel), parent);

  if (start_file)
    gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel), start_file);

  
  g_object_set_data (G_OBJECT (filesel), "ok-func", func);
  g_object_set_data (G_OBJECT (filesel), "ok-data", data);
  g_object_set_data (G_OBJECT (filesel), "ok-result", &result);

  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button),
		      "clicked",
		      GTK_SIGNAL_FUNC (filesel_ok_cb), filesel);
  gtk_signal_connect_object (GTK_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button),
			     "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy), GTK_OBJECT (filesel));

  gtk_signal_connect (GTK_OBJECT (filesel), "destroy",
		      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
  gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);

  gtk_widget_show (filesel);
  gtk_main ();

  return result;
}

/*
 * MsgBox utility functions
 */

static void
msgbox_yes_cb (GtkWidget *widget, gboolean *result)
{
  *result = 0;
  gtk_object_destroy (GTK_OBJECT (gtk_widget_get_toplevel (widget)));
}

static void
msgbox_no_cb (GtkWidget *widget, gboolean *result)
{
  *result = 1;
  gtk_object_destroy (GTK_OBJECT (gtk_widget_get_toplevel (widget)));
}

static gboolean
msgbox_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
  if (event->keyval == GDK_Escape)
    {
      gtk_signal_emit_stop_by_name (GTK_OBJECT (widget), "key_press_event");
      gtk_object_destroy (GTK_OBJECT (widget));
      return TRUE;
    }

  return FALSE;
}

gint
msgbox_run (GtkWindow  *parent,
	    const char *message,
	    const char *yes_button,
	    const char *no_button,
	    const char *cancel_button,
	    gint default_index)
{
  gboolean result = -1;
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *vbox;
  GtkWidget *button_box;
  GtkWidget *separator;

  g_return_val_if_fail (message != NULL, FALSE);
  g_return_val_if_fail (default_index >= 0 && default_index <= 1, FALSE);

  if (!parent)
    parent = get_active_window ();
  
  /* Create a dialog
   */
  dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);

  /* Quit our recursive main loop when the dialog is destroyed.
   */
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
		      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  /* Catch Escape key presses and have them destroy the dialog
   */
  gtk_signal_connect (GTK_OBJECT (dialog), "key_press_event",
		      GTK_SIGNAL_FUNC (msgbox_key_press_cb), NULL);

  /* Fill in the contents of the widget
   */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (dialog), vbox);
  
  label = gtk_label_new (message);
  gtk_misc_set_padding (GTK_MISC (label), 12, 12);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

  separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, FALSE, 0);

  button_box = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (button_box), 8);
  

  /* When Yes is clicked, call the msgbox_yes_cb
   * This sets the result variable and destroys the dialog
   */
  if (yes_button)
    {
      button = gtk_button_new_with_label (yes_button);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_container_add (GTK_CONTAINER (button_box), button);

      if (default_index == 0)
	gtk_widget_grab_default (button);
      
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (msgbox_yes_cb), &result);
    }

  /* When No is clicked, call the msgbox_no_cb
   * This sets the result variable and destroys the dialog
   */
  if (no_button)
    {
      button = gtk_button_new_with_label (no_button);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_container_add (GTK_CONTAINER (button_box), button);

      if (default_index == 0)
	gtk_widget_grab_default (button);
      
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
			  GTK_SIGNAL_FUNC (msgbox_no_cb), &result);
    }

  /* When Cancel is clicked, destroy the dialog
   */
  if (cancel_button)
    {
      button = gtk_button_new_with_label (cancel_button);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_container_add (GTK_CONTAINER (button_box), button);
      
      if (default_index == 1)
	gtk_widget_grab_default (button);
      
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 GTK_SIGNAL_FUNC (gtk_object_destroy), GTK_OBJECT (dialog));
    }

  gtk_widget_show_all (dialog);

  /* Run a recursive main loop until a button is clicked
   * or the user destroys the dialog through the window mananger */
  gtk_main ();

  return result;
}

/*
 * Example buffer filling code
 */
static gint
blink_timeout (gpointer data)
{
  GtkTextTag *tag;
  static gboolean flip = FALSE;
  
  tag = GTK_TEXT_TAG (data);

  g_object_set (G_OBJECT (tag),
                 "foreground", flip ? "blue" : "purple",
                 NULL);

  flip = !flip;

  return TRUE;
}

static gint
tag_event_handler (GtkTextTag *tag, GtkWidget *widget, GdkEvent *event,
                  const GtkTextIter *iter, gpointer user_data)
{
  gint char_index;

  char_index = gtk_text_iter_get_offset (iter);
  
  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
      printf ("Motion event at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_BUTTON_PRESS:
      printf ("Button press at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_2BUTTON_PRESS:
      printf ("Double click at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_3BUTTON_PRESS:
      printf ("Triple click at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_BUTTON_RELEASE:
      printf ("Button release at char %d tag `%s'\n",
             char_index, tag->name);
      break;
        
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      printf ("Key event at char %d tag `%s'\n",
              char_index, tag->name);
      break;
      
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_PROPERTY_NOTIFY:
    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
    default:
      break;
    }

  return FALSE;
}

static void
setup_tag (GtkTextTag *tag)
{
  g_signal_connect_data (G_OBJECT (tag),
                         "event",
                         tag_event_handler,
                         NULL, NULL, FALSE, FALSE);
}

static char  *book_closed_xpm[] = {
"16 16 6 1",
"       c None s None",
".      c black",
"X      c red",
"o      c yellow",
"O      c #808080",
"#      c white",
"                ",
"       ..       ",
"     ..XX.      ",
"   ..XXXXX.     ",
" ..XXXXXXXX.    ",
".ooXXXXXXXXX.   ",
"..ooXXXXXXXXX.  ",
".X.ooXXXXXXXXX. ",
".XX.ooXXXXXX..  ",
" .XX.ooXXX..#O  ",
"  .XX.oo..##OO. ",
"   .XX..##OO..  ",
"    .X.#OO..    ",
"     ..O..      ",
"      ..        ",
"                "};

void
fill_example_buffer (GtkTextBuffer *buffer)
{
  GtkTextIter iter, iter2;
  GtkTextTag *tag;
  GdkColor color;
  GdkColor color2;
  GdkPixbuf *pixbuf;
  int i;
  char *str;
  GtkTextChildAnchor *anchor;
  
  /* FIXME this is broken if called twice on a buffer, since
   * we try to create tags a second time.
   */
  
  tag = gtk_text_buffer_create_tag (buffer, "fg_blue");

  /*       gtk_timeout_add (1000, blink_timeout, tag); */
      
  setup_tag (tag);
  
  color.red = color.green = 0;
  color.blue = 0xffff;
  color2.red = 0xfff;
  color2.blue = 0x0;
  color2.green = 0;
  g_object_set (G_OBJECT (tag),
		 "foreground_gdk", &color,
		 "background_gdk", &color2,
                  "size_points", 24.0,
		 NULL);

  tag = gtk_text_buffer_create_tag (buffer, "fg_red");

  setup_tag (tag);
      
  color.blue = color.green = 0;
  color.red = 0xffff;
  g_object_set (G_OBJECT (tag),
		 "rise", -4 * PANGO_SCALE,
		 "foreground_gdk", &color,
		 NULL);

  tag = gtk_text_buffer_create_tag (buffer, "bg_green");

  setup_tag (tag);
      
  color.blue = color.red = 0;
  color.green = 0xffff;
  g_object_set (G_OBJECT (tag),
                "background_gdk", &color,
                "size_points", 10.0,
                NULL);

  tag = gtk_text_buffer_create_tag (buffer, "strikethrough");

  setup_tag (tag);
      
  g_object_set (G_OBJECT (tag),
                "strikethrough", TRUE,
                NULL);


  tag = gtk_text_buffer_create_tag (buffer, "underline");

  setup_tag (tag);
      
  g_object_set (G_OBJECT (tag),
                "underline", PANGO_UNDERLINE_SINGLE,
                NULL);

  setup_tag (tag);
      
  g_object_set (G_OBJECT (tag),
                "underline", PANGO_UNDERLINE_SINGLE,
                NULL);

  tag = gtk_text_buffer_create_tag (buffer, "centered");
      
  g_object_set (G_OBJECT (tag),
                "justification", GTK_JUSTIFY_CENTER,
                NULL);

  tag = gtk_text_buffer_create_tag (buffer, "rtl_quote");
      
  g_object_set (G_OBJECT (tag),
                "wrap_mode", GTK_WRAP_WORD,
                "direction", GTK_TEXT_DIR_RTL,
                "indent", 30,
                "left_margin", 20,
                  "right_margin", 20,
                NULL);


#if 0
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);

  anchor = gtk_text_buffer_create_child_anchor (buffer, &iter);

  g_object_ref (G_OBJECT (anchor));
  
  g_object_set_data_full (G_OBJECT (buffer), "anchor", anchor,
                          (GDestroyNotify) g_object_unref);
#endif
  
  pixbuf = gdk_pixbuf_new_from_xpm_data (book_closed_xpm);
  
  i = 0;
  while (i < 100)
    {
      GtkTextMark * temp_mark;
      
      gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
          
      gtk_text_buffer_insert_pixbuf (buffer, &iter, pixbuf);
          
      str = g_strdup_printf ("%d Hello World! blah blah blah blah blah blah blah blah blah blah blah blah\nwoo woo woo woo woo woo woo woo woo woo woo woo woo woo woo\n",
			    i);
      
      gtk_text_buffer_insert (buffer, &iter, str, -1);

      g_free (str);
      
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 5);
          
      gtk_text_buffer_insert (buffer, &iter,
			     "(Hello World!)\nfoo foo Hello this is some text we are using to text word wrap. It has punctuation! gee; blah - hmm, great.\nnew line with a significant quantity of text on it. This line really does contain some text. More text! More text! More text!\n"
			     /* This is UTF8 stuff, Emacs doesn't
				really know how to display it */
			     "German (Deutsch Süd) Grüß Gott Greek (Ελληνικά) Γειά σας Hebrew   שלום Japanese (日本語)\n", -1);

      temp_mark =
        gtk_text_buffer_create_mark (buffer, "tmp_mark", &iter, TRUE);

#if 1
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 6);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter2, 0, 13);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_blue", &iter, &iter2);

      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 1, 10);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter2, 1, 16);

      gtk_text_buffer_apply_tag_by_name (buffer, "underline", &iter, &iter2);

      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 1, 14);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter2, 1, 24);

      gtk_text_buffer_apply_tag_by_name (buffer, "strikethrough", &iter, &iter2);
          
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 0, 9);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter2, 0, 16);

      gtk_text_buffer_apply_tag_by_name (buffer, "bg_green", &iter, &iter2);
  
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 4, 2);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter2, 4, 10);

      gtk_text_buffer_apply_tag_by_name (buffer, "bg_green", &iter, &iter2);

      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, 4, 8);
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter2, 4, 15);

      gtk_text_buffer_apply_tag_by_name (buffer, "fg_red", &iter, &iter2);
#endif

      gtk_text_buffer_get_iter_at_mark (buffer, &iter, temp_mark);
      gtk_text_buffer_insert (buffer, &iter, "Centered text!\n", -1);
	  
      gtk_text_buffer_get_iter_at_mark (buffer, &iter2, temp_mark);
      gtk_text_buffer_apply_tag_by_name (buffer, "centered", &iter2, &iter);

      gtk_text_buffer_move_mark (buffer, temp_mark, &iter);
      gtk_text_buffer_insert (buffer, &iter, "Word wrapped, Right-to-left Quote\n", -1);
      gtk_text_buffer_insert (buffer, &iter, "وقد بدأ ثلاث من أكثر المؤسسات تقدما في شبكة اكسيون برامجها كمنظمات لا تسعى للربح، ثم تحولت في السنوات الخمس الماضية إلى مؤسسات مالية منظمة، وباتت جزءا من النظام المالي في بلدانها، ولكنها تتخصص في خدمة قطاع المشروعات الصغيرة. وأحد أكثر هذه المؤسسات نجاحا هو »بانكوسول« في بوليفيا.\n", -1);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter2, temp_mark);
      gtk_text_buffer_apply_tag_by_name (buffer, "rtl_quote", &iter2, &iter);
	  
      ++i;
    }

  g_object_unref (G_OBJECT (pixbuf));
  
  printf ("%d lines %d chars\n",
          gtk_text_buffer_get_line_count (buffer),
          gtk_text_buffer_get_char_count (buffer));

  /* Move cursor to start */
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  gtk_text_buffer_place_cursor (buffer, &iter);
  
  gtk_text_buffer_set_modified (buffer, FALSE);
}

gboolean
fill_file_buffer (GtkTextBuffer *buffer, const char *filename)
{
  FILE* f;
  gchar buf[2048];
  gint remaining = 0;
  GtkTextIter iter, end;

  f = fopen (filename, "r");
  
  if (f == NULL)
    {
      gchar *err = g_strdup_printf ("Cannot open file '%s': %s",
				    filename, g_strerror (errno));
      msgbox_run (NULL, err, "OK", NULL, NULL, 0);
      g_free (err);
      return FALSE;
    }
  
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, 0);
  while (!feof (f))
    {
      gint count;
      const char *leftover;
      int to_read = 2047  - remaining;

      count = fread (buf + remaining, 1, to_read, f);
      buf[count + remaining] = '\0';

      g_utf8_validate (buf, count + remaining, &leftover);
      
      g_assert (g_utf8_validate (buf, leftover - buf, NULL));
      gtk_text_buffer_insert (buffer, &iter, buf, leftover - buf);

      remaining = (buf + remaining + count) - leftover;
      g_memmove (buf, leftover, remaining);

      if (remaining > 6 || count < to_read)
	  break;
    }

  if (remaining)
    {
      gchar *err = g_strdup_printf ("Invalid UTF-8 data encountered reading file '%s'", filename);
      msgbox_run (NULL, err, "OK", NULL, NULL, 0);
      g_free (err);
    }
  
  /* We had a newline in the buffer to begin with. (The buffer always contains
   * a newline, so we delete to the end of the buffer to clean up.
   */
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_delete (buffer, &iter, &end);
  
  gtk_text_buffer_set_modified (buffer, FALSE);

  return TRUE;
}

static gint
delete_event_cb (GtkWidget *window, GdkEventAny *event, gpointer data)
{
  View *view = view_from_widget (window);

  push_active_window (GTK_WINDOW (window));
  check_close_view (view);
  pop_active_window ();

  return TRUE;
}

/*
 * Menu callbacks
 */

static View *
get_empty_view (View *view)
{
  if (!view->buffer->filename &&
      !gtk_text_buffer_modified (view->buffer->buffer))
    return view;
  else
    return create_view (create_buffer ());
}

static View *
view_from_widget (GtkWidget *widget)
{
  GtkWidget *app;

  if (GTK_IS_MENU_ITEM (widget))
    {
      GtkItemFactory *item_factory = gtk_item_factory_from_widget (widget);
      return g_object_get_data (G_OBJECT (item_factory), "view");      
    }
  else
    {
      GtkWidget *app = gtk_widget_get_toplevel (widget);
      return g_object_get_data (G_OBJECT (app), "view");
    }
}

static void
do_new (gpointer             callback_data,
	guint                callback_action,
	GtkWidget           *widget)
{
  create_view (create_buffer ());
}

static void
do_new_view (gpointer             callback_data,
	     guint                callback_action,
	     GtkWidget           *widget)
{
  View *view = view_from_widget (widget);
  
  create_view (view->buffer);
}

gboolean
open_ok_func (const char *filename, gpointer data)
{
  View *view = data;
  View *new_view = get_empty_view (view);

  if (!fill_file_buffer (new_view->buffer->buffer, filename))
    {
      if (new_view != view)
	close_view (new_view);
      return FALSE;
    }
  else
    {
      g_free (new_view->buffer->filename);
      new_view->buffer->filename = g_strdup (filename);
      buffer_filename_set (new_view->buffer);
      
      return TRUE;
    }
}

static void
do_open (gpointer             callback_data,
	 guint                callback_action,
	 GtkWidget           *widget)
{
  View *view = view_from_widget (widget);

  push_active_window (GTK_WINDOW (view->window));
  filesel_run (NULL, "Open File", NULL, open_ok_func, view);
  pop_active_window ();
}

static void
do_save_as (gpointer             callback_data,
	    guint                callback_action,
	    GtkWidget           *widget)
{
  View *view = view_from_widget (widget);  

  push_active_window (GTK_WINDOW (view->window));
  save_as_buffer (view->buffer);
  pop_active_window ();
}

static void
do_save (gpointer             callback_data,
	 guint                callback_action,
	 GtkWidget           *widget)
{
  View *view = view_from_widget (widget);

  push_active_window (GTK_WINDOW (view->window));
  if (!view->buffer->filename)
    do_save_as (callback_data, callback_action, widget);
  else
    save_buffer (view->buffer);
  pop_active_window ();
}

static void
do_close   (gpointer             callback_data,
	    guint                callback_action,
	    GtkWidget           *widget)
{
  View *view = view_from_widget (widget);

  push_active_window (GTK_WINDOW (view->window));
  check_close_view (view);
  pop_active_window ();
}

static void
do_exit    (gpointer             callback_data,
	    guint                callback_action,
	    GtkWidget           *widget)
{
  View *view = view_from_widget (widget);

  GSList *tmp_list = buffers;

  push_active_window (GTK_WINDOW (view->window));
  while (tmp_list)
    {
      if (!check_buffer_saved (tmp_list->data))
	return;

      tmp_list = tmp_list->next;
    }

  gtk_main_quit ();
  pop_active_window ();
}

static void
do_example (gpointer             callback_data,
	    guint                callback_action,
	    GtkWidget           *widget)
{
  View *view = view_from_widget (widget);
  View *new_view;

  new_view = get_empty_view (view);
  
  fill_example_buffer (new_view->buffer->buffer);

  view_add_example_widgets (new_view);
}

static void
do_wrap_changed (gpointer             callback_data,
		 guint                callback_action,
		 GtkWidget           *widget)
{
  View *view = view_from_widget (widget);

  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view->text_view), callback_action);
}

static void
do_direction_changed (gpointer             callback_data,
		      guint                callback_action,
		      GtkWidget           *widget)
{
  View *view = view_from_widget (widget);
  
  gtk_widget_set_direction (view->text_view, callback_action);
  gtk_widget_queue_resize (view->text_view);
}


static void
do_spacing_changed (gpointer             callback_data,
                    guint                callback_action,
                    GtkWidget           *widget)
{
  View *view = view_from_widget (widget);

  if (callback_action)
    {
      gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (view->text_view),
                                            23);
      gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (view->text_view),
                                            21);
      gtk_text_view_set_pixels_inside_wrap (GTK_TEXT_VIEW (view->text_view),
                                            9);
    }
  else
    {
      gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (view->text_view),
                                            0);
      gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (view->text_view),
                                            0);
      gtk_text_view_set_pixels_inside_wrap (GTK_TEXT_VIEW (view->text_view),
                                            0);
    }
}

static void
do_editable_changed (gpointer callback_data,
                     guint callback_action,
                     GtkWidget *widget)
{
  View *view = view_from_widget (widget);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (view->text_view), callback_action);
}

static void
do_cursor_visible_changed (gpointer callback_data,
                           guint callback_action,
                           GtkWidget *widget)
{
  View *view = view_from_widget (widget);

  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view->text_view), callback_action);
}

static void
do_color_cycle_changed (gpointer callback_data,
                        guint callback_action,
                        GtkWidget *widget)
{
  View *view = view_from_widget (widget);

  buffer_set_colors (view->buffer, callback_action);
}

static void
do_apply_editable (gpointer callback_data,
                   guint callback_action,
                   GtkWidget *widget)
{
  View *view = view_from_widget (widget);
  GtkTextIter start;
  GtkTextIter end;
  
  if (gtk_text_buffer_get_selection_bounds (view->buffer->buffer,
                                            &start, &end))
    {
      if (callback_action)
        {
          gtk_text_buffer_remove_tag (view->buffer->buffer,
                                      view->buffer->not_editable_tag,
                                      &start, &end);
        }
      else
        {
          gtk_text_buffer_apply_tag (view->buffer->buffer,
                                     view->buffer->not_editable_tag,
                                     &start, &end);
        }
    }
}

static void
do_apply_invisible (gpointer callback_data,
                    guint callback_action,
                    GtkWidget *widget)
{
  View *view = view_from_widget (widget);
  GtkTextIter start;
  GtkTextIter end;
  
  if (gtk_text_buffer_get_selection_bounds (view->buffer->buffer,
                                            &start, &end))
    {
      if (callback_action)
        {
          gtk_text_buffer_remove_tag (view->buffer->buffer,
                                      view->buffer->invisible_tag,
                                      &start, &end);
        }
      else
        {
          gtk_text_buffer_apply_tag (view->buffer->buffer,
                                     view->buffer->invisible_tag,
                                     &start, &end);
        }
    }
}

static void
do_apply_tabs (gpointer callback_data,
               guint callback_action,
               GtkWidget *widget)
{
  View *view = view_from_widget (widget);
  GtkTextIter start;
  GtkTextIter end;
  
  if (gtk_text_buffer_get_selection_bounds (view->buffer->buffer,
                                            &start, &end))
    {
      if (callback_action)
        {
          gtk_text_buffer_remove_tag (view->buffer->buffer,
                                      view->buffer->custom_tabs_tag,
                                      &start, &end);
        }
      else
        {
          gtk_text_buffer_apply_tag (view->buffer->buffer,
                                     view->buffer->custom_tabs_tag,
                                     &start, &end);
        }
    }
}

static void
do_apply_colors (gpointer callback_data,
                 guint callback_action,
                 GtkWidget *widget)
{
  View *view = view_from_widget (widget);
  Buffer *buffer = view->buffer;
  GtkTextIter start;
  GtkTextIter end;
  
  if (gtk_text_buffer_get_selection_bounds (view->buffer->buffer,
                                            &start, &end))
    {
      if (!callback_action)
        {
          GSList *tmp;
          
          tmp = buffer->color_tags;
          while (tmp != NULL)
            {
              gtk_text_buffer_remove_tag (view->buffer->buffer,
                                          tmp->data,
                                          &start, &end);              
              tmp = g_slist_next (tmp);
            }
        }
      else
        {
          GSList *tmp;
          
          tmp = buffer->color_tags;
          while (TRUE)
            {
              GtkTextIter next;
              gboolean done = FALSE;
              
              next = start;
              gtk_text_iter_forward_char (&next);
              gtk_text_iter_forward_char (&next);

              if (gtk_text_iter_compare (&next, &end) > 0)
                {
                  next = end;
                  done = TRUE;
                }
              
              gtk_text_buffer_apply_tag (view->buffer->buffer,
                                         tmp->data,
                                         &start, &next);

              start = next;

              if (done)
                return;
              
              tmp = g_slist_next (tmp);
              if (tmp == NULL)
                tmp = buffer->color_tags;
            } 
        }
    }
}

enum
{
  RESPONSE_FORWARD,
  RESPONSE_BACKWARD
};

static void
dialog_response_callback (GtkWidget *dialog, gint response_id, gpointer data)
{
  GtkTextBuffer *buffer;
  View *view = data;
  GtkTextIter start, end;
  gchar *search_string;

  if (response_id != RESPONSE_FORWARD &&
      response_id != RESPONSE_BACKWARD)
    {
      gtk_widget_destroy (dialog);
      return;
    }
  
  buffer = g_object_get_data (G_OBJECT (dialog), "buffer");

  gtk_text_buffer_get_bounds (buffer, &start, &end);

  /* Remove trailing newline */
  gtk_text_iter_backward_char (&end);
  
  search_string = gtk_text_iter_get_text (&start, &end);

  printf ("Searching for `%s'\n", search_string);

  if (response_id == RESPONSE_FORWARD)
    buffer_search_forward (view->buffer, search_string, view);
  else if (response_id == RESPONSE_BACKWARD)
    buffer_search_backward (view->buffer, search_string, view);
    
  g_free (search_string);
  
  gtk_widget_destroy (dialog);
}

static void
do_search (gpointer callback_data,
           guint callback_action,
           GtkWidget *widget)
{
  View *view = view_from_widget (widget);
  GtkWidget *dialog;
  GtkWidget *search_text;
  GtkTextBuffer *buffer;

  dialog = gtk_dialog_new_with_buttons ("Search",
                                        GTK_WINDOW (view->window),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "Forward", RESPONSE_FORWARD,
                                        "Backward", RESPONSE_BACKWARD,
                                        GTK_STOCK_BUTTON_CANCEL,
                                        GTK_RESPONSE_NONE, NULL);


  buffer = gtk_text_buffer_new (NULL);

  search_text = gtk_text_view_new_with_buffer (buffer);

  g_object_unref (G_OBJECT (buffer));
  
  gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                    search_text,
                    TRUE, TRUE, 0);

  g_object_set_data (G_OBJECT (dialog), "buffer", buffer);
  
  gtk_signal_connect (GTK_OBJECT (dialog),
                      "response",
                      GTK_SIGNAL_FUNC (dialog_response_callback),
                      view);

  gtk_widget_show (search_text);

  gtk_widget_grab_focus (search_text);
  
  gtk_widget_show_all (dialog);
}

static void
view_init_menus (View *view)
{
  GtkTextDirection direction = gtk_widget_get_direction (view->text_view);
  GtkWrapMode wrap_mode = gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (view->text_view));
  GtkWidget *menu_item = NULL;

  switch (direction)
    {
    case GTK_TEXT_DIR_LTR:
      menu_item = gtk_item_factory_get_widget (view->item_factory, "/Settings/Left-to-Right");
      break;
    case GTK_TEXT_DIR_RTL:
      menu_item = gtk_item_factory_get_widget (view->item_factory, "/Settings/Right-to-Left");
      break;
    default:
      break;
    }

  if (menu_item)
    gtk_menu_item_activate (GTK_MENU_ITEM (menu_item));

  switch (wrap_mode)
    {
    case GTK_WRAP_NONE:
      menu_item = gtk_item_factory_get_widget (view->item_factory, "/Settings/Wrap Off");
      break;
    case GTK_WRAP_WORD:
      menu_item = gtk_item_factory_get_widget (view->item_factory, "/Settings/Wrap Words");
      break;
    case GTK_WRAP_CHAR:
      menu_item = gtk_item_factory_get_widget (view->item_factory, "/Settings/Wrap Chars");
      break;
    default:
      break;
    }
  
  if (menu_item)
    gtk_menu_item_activate (GTK_MENU_ITEM (menu_item));
}

static GtkItemFactoryEntry menu_items[] =
{
  { "/_File",            NULL,         0,           0, "<Branch>" },
  { "/File/_New",        "<control>N", do_new,      0, NULL },
  { "/File/New _View",   NULL,         do_new_view, 0, NULL },
  { "/File/_Open",       "<control>O", do_open,     0, NULL },
  { "/File/_Save",       "<control>S", do_save,     0, NULL },
  { "/File/Save _As...", NULL,         do_save_as,  0, NULL },
  { "/File/sep1",        NULL,         0,           0, "<Separator>" },
  { "/File/_Close",     "<control>W" , do_close,    0, NULL },
  { "/File/E_xit",      "<control>Q" , do_exit,     0, NULL },

  { "/_Edit", NULL, 0, 0, "<Branch>" },
  { "/Edit/Find...", NULL, do_search, 0, NULL },

  { "/_Settings",   	  NULL,         0,                0, "<Branch>" },
  { "/Settings/Wrap _Off",   NULL,      do_wrap_changed,  GTK_WRAP_NONE, "<RadioItem>" },
  { "/Settings/Wrap _Words", NULL,      do_wrap_changed,  GTK_WRAP_WORD, "/Settings/Wrap Off" },
  { "/Settings/Wrap _Chars", NULL,      do_wrap_changed,  GTK_WRAP_CHAR, "/Settings/Wrap Off" },
  { "/Settings/sep1",        NULL,      0,                0, "<Separator>" },
  { "/Settings/Editable", NULL,      do_editable_changed,  TRUE, "<RadioItem>" },
  { "/Settings/Not editable",    NULL,      do_editable_changed,  FALSE, "/Settings/Editable" },
  { "/Settings/sep1",        NULL,      0,                0, "<Separator>" },

  { "/Settings/Cursor visible",    NULL,      do_cursor_visible_changed,  TRUE, "<RadioItem>" },
  { "/Settings/Cursor not visible", NULL,      do_cursor_visible_changed,  FALSE, "/Settings/Cursor visible" },
  { "/Settings/sep1",        NULL,      0,                0, "<Separator>" },
  
  { "/Settings/Left-to-Right", NULL,    do_direction_changed,  GTK_TEXT_DIR_LTR, "<RadioItem>" },
  { "/Settings/Right-to-Left", NULL,    do_direction_changed,  GTK_TEXT_DIR_RTL, "/Settings/Left-to-Right" },

  { "/Settings/sep1",        NULL,      0,                0, "<Separator>" },
  { "/Settings/Sane spacing", NULL,    do_spacing_changed,  FALSE, "<RadioItem>" },
  { "/Settings/Funky spacing", NULL,    do_spacing_changed,  TRUE, "/Settings/Sane spacing" },
  { "/Settings/sep1",        NULL,      0,                0, "<Separator>" },
  { "/Settings/Don't cycle color tags", NULL,    do_color_cycle_changed,  FALSE, "<RadioItem>" },
  { "/Settings/Cycle colors", NULL,    do_color_cycle_changed,  TRUE, "/Settings/Don't cycle color tags" },
  { "/_Attributes",   	  NULL,         0,                0, "<Branch>" },
  { "/Attributes/Editable",   	  NULL,         do_apply_editable, TRUE, NULL },
  { "/Attributes/Not editable",   	  NULL,         do_apply_editable, FALSE, NULL },
  { "/Attributes/Invisible",   	  NULL,         do_apply_invisible, FALSE, NULL },
  { "/Attributes/Visible",   	  NULL,         do_apply_invisible, TRUE, NULL },
  { "/Attributes/Custom tabs",   	  NULL,         do_apply_tabs, FALSE, NULL },
  { "/Attributes/Default tabs",   	  NULL,         do_apply_tabs, TRUE, NULL },
  { "/Attributes/Color cycles",   	  NULL,         do_apply_colors, TRUE, NULL },
  { "/Attributes/No colors",   	          NULL,         do_apply_colors, FALSE, NULL },
  { "/_Test",   	 NULL,         0,           0, "<Branch>" },
  { "/Test/_Example",  	 NULL,         do_example,  0, NULL },
};

static gboolean
save_buffer (Buffer *buffer)
{
  GtkTextIter start, end;
  gchar *chars;
  gboolean result = FALSE;
  gboolean have_backup = FALSE;
  gchar *bak_filename;
  FILE *file;

  g_return_val_if_fail (buffer->filename != NULL, FALSE);

  bak_filename = g_strconcat (buffer->filename, "~", NULL);
  
  if (rename (buffer->filename, bak_filename) != 0)
    {
      if (errno != ENOENT)
	{
	  gchar *err = g_strdup_printf ("Cannot back up '%s' to '%s': %s",
					buffer->filename, bak_filename, g_strerror (errno));
	  msgbox_run (NULL, err, "OK", NULL, NULL, 0);
	  g_free (err);
          return FALSE;
	}
    }
  else
    have_backup = TRUE;
  
  file = fopen (buffer->filename, "w");
  if (!file)
    {
      gchar *err = g_strdup_printf ("Cannot back up '%s' to '%s': %s",
				    buffer->filename, bak_filename, g_strerror (errno));
      msgbox_run (NULL, err, "OK", NULL, NULL, 0);
    }
  else
    {
      gtk_text_buffer_get_iter_at_offset (buffer->buffer, &start, 0);
      gtk_text_buffer_get_end_iter (buffer->buffer, &end);
  
      chars = gtk_text_buffer_get_slice (buffer->buffer, &start, &end, FALSE);

      if (fputs (chars, file) == EOF ||
	  fclose (file) == EOF)
	{
	  gchar *err = g_strdup_printf ("Error writing to '%s': %s",
					buffer->filename, g_strerror (errno));
	  msgbox_run (NULL, err, "OK", NULL, NULL, 0);
	  g_free (err);
	}
      else
	{
	  /* Success
	   */
	  result = TRUE;
	  gtk_text_buffer_set_modified (buffer->buffer, FALSE);	  
	}
	
      g_free (chars);
    }

  if (!result && have_backup)
    {
      if (rename (bak_filename, buffer->filename) != 0)
	{
	  gchar *err = g_strdup_printf ("Error restoring backup file '%s' to '%s': %s\nBackup left as '%s'",
					buffer->filename, bak_filename, g_strerror (errno), bak_filename);
	  msgbox_run (NULL, err, "OK", NULL, NULL, 0);
	  g_free (err);
	}
    }

  g_free (bak_filename);
  
  return result;
}

static gboolean
save_as_ok_func (const char *filename, gpointer data)
{
  Buffer *buffer = data;
  char *old_filename = buffer->filename;

  if (!buffer->filename || strcmp (filename, buffer->filename) != 0)
    {
      struct stat statbuf;

      if (stat (filename, &statbuf) == 0)
	{
	  gchar *err = g_strdup_printf ("Ovewrite existing file '%s'?", filename);
	  gint result = msgbox_run (NULL, err, "Yes", "No", NULL, 1);
	  g_free (err);

	  if (result != 0)
	    return FALSE;
	}
    }
  
  buffer->filename = g_strdup (filename);

  if (save_buffer (buffer))
    {
      g_free (old_filename);
      buffer_filename_set (buffer);
      return TRUE;
    }
  else
    {
      g_free (buffer->filename);
      buffer->filename = old_filename;
      return FALSE;
    }
}

static gboolean
save_as_buffer (Buffer *buffer)
{
  return filesel_run (NULL, "Save File", NULL, save_as_ok_func, buffer);
}

static gboolean
check_buffer_saved (Buffer *buffer)
{
  if (gtk_text_buffer_modified (buffer->buffer))
    {
      char *pretty_name = buffer_pretty_name (buffer);
      char *msg = g_strdup_printf ("Save changes to '%s'?", pretty_name);
      gint result;
      
      g_free (pretty_name);
      
      result = msgbox_run (NULL, msg, "Yes", "No", "Cancel", 0);
      g_free (msg);
  
      if (result == 0)
	return save_as_buffer (buffer);
      else if (result == 1)
	return TRUE;
      else
	return FALSE;
    }
  else
    return TRUE;
}

#define N_COLORS 16

static Buffer *
create_buffer (void)
{
  Buffer *buffer;
  PangoTabArray *tabs;
  gint i;
  
  buffer = g_new (Buffer, 1);

  buffer->buffer = gtk_text_buffer_new (NULL);
  
  buffer->refcount = 1;
  buffer->filename = NULL;
  buffer->untitled_serial = -1;

  buffer->color_tags = NULL;
  buffer->color_cycle_timeout = 0;
  buffer->start_hue = 0.0;
  
  i = 0;
  while (i < N_COLORS)
    {
      GtkTextTag *tag;

      tag = gtk_text_buffer_create_tag (buffer->buffer, NULL);
      
      buffer->color_tags = g_slist_prepend (buffer->color_tags, tag);
      
      ++i;
    }
  
  buffer->invisible_tag = gtk_text_buffer_create_tag (buffer->buffer, NULL);
  g_object_set (G_OBJECT (buffer->invisible_tag),
                  "invisible", TRUE,
                  NULL);
  
  buffer->not_editable_tag = gtk_text_buffer_create_tag (buffer->buffer, NULL);
  g_object_set (G_OBJECT (buffer->not_editable_tag),
                  "editable", FALSE,
                  "foreground", "purple", NULL);

  buffer->found_text_tag = gtk_text_buffer_create_tag (buffer->buffer, NULL);
  g_object_set (G_OBJECT (buffer->found_text_tag),
                  "foreground", "red", NULL);

  tabs = pango_tab_array_new_with_positions (4,
                                             TRUE,
                                             PANGO_TAB_LEFT, 10,
                                             PANGO_TAB_LEFT, 30,
                                             PANGO_TAB_LEFT, 60,
                                             PANGO_TAB_LEFT, 120);
  
  buffer->custom_tabs_tag = gtk_text_buffer_create_tag (buffer->buffer, NULL);
  g_object_set (G_OBJECT (buffer->custom_tabs_tag),
                  "tabs", tabs,
                  "foreground", "green", NULL);

  pango_tab_array_free (tabs);
  
  buffers = g_slist_prepend (buffers, buffer);
  
  return buffer;
}

static char *
buffer_pretty_name (Buffer *buffer)
{
  if (buffer->filename)
    {
      char *p;
      char *result = g_path_get_basename (buffer->filename);
      p = strchr (result, '/');
      if (p)
	*p = '\0';

      return result;
    }
  else
    {
      if (buffer->untitled_serial == -1)
	buffer->untitled_serial = untitled_serial++;

      if (buffer->untitled_serial == 1)
	return g_strdup ("Untitled");
      else
	return g_strdup_printf ("Untitled #%d", buffer->untitled_serial);
    }
}

static void
buffer_filename_set (Buffer *buffer)
{
  GSList *tmp_list = views;

  while (tmp_list)
    {
      View *view = tmp_list->data;

      if (view->buffer == buffer)
	view_set_title (view);

      tmp_list = tmp_list->next;
    }
}

static void
buffer_search (Buffer     *buffer,
               const char *str,
               View       *view,
               gboolean forward)
{
  GtkTextIter iter;
  GtkTextIter start, end;
  GtkWidget *dialog;
  int i;
  
  /* remove tag from whole buffer */
  gtk_text_buffer_get_bounds (buffer->buffer, &start, &end);
  gtk_text_buffer_remove_tag (buffer->buffer,  buffer->found_text_tag,
                              &start, &end );
  
  gtk_text_buffer_get_iter_at_mark (buffer->buffer, &iter,
                                    gtk_text_buffer_get_mark (buffer->buffer,
                                                              "insert"));

  i = 0;
  if (*str != '\0')
    {
      GtkTextIter match_start, match_end;

      if (forward)
        {
          while (gtk_text_iter_forward_search (&iter, str, TRUE, FALSE,
                                               &match_start, &match_end,
                                               NULL))
            {
              ++i;
              gtk_text_buffer_apply_tag (buffer->buffer, buffer->found_text_tag,
                                         &match_start, &match_end);
              
              iter = match_end;
            }
        }
      else
        {
          while (gtk_text_iter_backward_search (&iter, str, TRUE, FALSE,
                                                &match_start, &match_end,
                                                NULL))
            {
              ++i;
              gtk_text_buffer_apply_tag (buffer->buffer, buffer->found_text_tag,
                                         &match_start, &match_end);
              
              iter = match_start;
            }
        }
    }

  dialog = gtk_message_dialog_new (GTK_WINDOW (view->window),
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   "%d strings found and marked in red",
                                   i);

  gtk_signal_connect_object (GTK_OBJECT (dialog),
                             "response",
                             GTK_SIGNAL_FUNC (gtk_widget_destroy),
                             GTK_OBJECT (dialog));
  
  gtk_widget_show (dialog);
}

static void
buffer_search_forward (Buffer *buffer, const char *str,
                       View *view)
{
  buffer_search (buffer, str, view, TRUE);
}

static void
buffer_search_backward (Buffer *buffer, const char *str,
                        View *view)
{
  buffer_search (buffer, str, view, FALSE);
}

static void
buffer_ref (Buffer *buffer)
{
  buffer->refcount++;
}

static void
buffer_unref (Buffer *buffer)
{
  buffer->refcount--;
  if (buffer->refcount == 0)
    {
      buffer_set_colors (buffer, FALSE);
      buffers = g_slist_remove (buffers, buffer);
      g_object_unref (G_OBJECT (buffer->buffer));
      g_free (buffer->filename);
      g_free (buffer);
    }
}

static void
hsv_to_rgb (gdouble *h,
	    gdouble *s,
	    gdouble *v)
{
  gdouble hue, saturation, value;
  gdouble f, p, q, t;

  if (*s == 0.0)
    {
      *h = *v;
      *s = *v;
      *v = *v; /* heh */
    }
  else
    {
      hue = *h * 6.0;
      saturation = *s;
      value = *v;
      
      if (hue >= 6.0)
	hue = 0.0;
      
      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - saturation * f);
      t = value * (1.0 - saturation * (1.0 - f));
      
      switch ((int) hue)
	{
	case 0:
	  *h = value;
	  *s = t;
	  *v = p;
	  break;
	  
	case 1:
	  *h = q;
	  *s = value;
	  *v = p;
	  break;
	  
	case 2:
	  *h = p;
	  *s = value;
	  *v = t;
	  break;
	  
	case 3:
	  *h = p;
	  *s = q;
	  *v = value;
	  break;
	  
	case 4:
	  *h = t;
	  *s = p;
	  *v = value;
	  break;
	  
	case 5:
	  *h = value;
	  *s = p;
	  *v = q;
	  break;
	  
	default:
	  g_assert_not_reached ();
	}
    }
}

static void
hue_to_color (gdouble   hue,
              GdkColor *color)
{
  gdouble h, s, v;

  h = hue;
  s = 1.0;
  v = 1.0;

  g_return_if_fail (hue <= 1.0);
  
  hsv_to_rgb (&h, &s, &v);

  color->red = h * 65535;
  color->green = s * 65535;
  color->blue = v * 65535;
}


static gint
color_cycle_timeout (gpointer data)
{
  Buffer *buffer = data;

  buffer_cycle_colors (buffer);

  return TRUE;
}

static void
buffer_set_colors (Buffer  *buffer,
                   gboolean enabled)
{
  GSList *tmp;
  gdouble hue = 0.0;

  if (enabled && buffer->color_cycle_timeout == 0)
    buffer->color_cycle_timeout = gtk_timeout_add (200, color_cycle_timeout, buffer);
  else if (!enabled && buffer->color_cycle_timeout != 0)
    {
      gtk_timeout_remove (buffer->color_cycle_timeout);
      buffer->color_cycle_timeout = 0;
    }
    
  tmp = buffer->color_tags;
  while (tmp != NULL)
    {
      if (enabled)
        {
          GdkColor color;
          
          hue_to_color (hue, &color);

          g_object_set (G_OBJECT (tmp->data),
                          "foreground_gdk", &color,
                          NULL);
        }
      else
        g_object_set (G_OBJECT (tmp->data),
                        "foreground_set", FALSE,
                        NULL);

      hue += 1.0 / N_COLORS;
      
      tmp = g_slist_next (tmp);
    }
}

static void
buffer_cycle_colors (Buffer *buffer)
{
  GSList *tmp;
  gdouble hue = buffer->start_hue;
  
  tmp = buffer->color_tags;
  while (tmp != NULL)
    {
      GdkColor color;
      
      hue_to_color (hue, &color);
      
      g_object_set (G_OBJECT (tmp->data),
                    "foreground_gdk", &color,
                    NULL);

      hue += 1.0 / N_COLORS;
      if (hue > 1.0)
        hue = 0.0;
      
      tmp = g_slist_next (tmp);
    }

  buffer->start_hue += 1.0 / N_COLORS;
  if (buffer->start_hue > 1.0)
    buffer->start_hue = 0.0;
}

static void
close_view (View *view)
{
  views = g_slist_remove (views, view);
  buffer_unref (view->buffer);
  gtk_widget_destroy (view->window);
  g_object_unref (G_OBJECT (view->item_factory));
  
  g_free (view);
  
  if (!views)
    gtk_main_quit ();
}

static void
check_close_view (View *view)
{
  if (view->buffer->refcount > 1 ||
      check_buffer_saved (view->buffer))
    close_view (view);
}

static void
view_set_title (View *view)
{
  char *pretty_name = buffer_pretty_name (view->buffer);
  char *title = g_strconcat ("testtext - ", pretty_name, NULL);

  gtk_window_set_title (GTK_WINDOW (view->window), title);

  g_free (pretty_name);
  g_free (title);
}

static void
cursor_set_callback (GtkTextBuffer     *buffer,
                     const GtkTextIter *location,
                     GtkTextMark       *mark,
                     gpointer           user_data)
{
  GtkTextView *text_view;

  /* Redraw tab windows if the cursor moves
   * on the mapped widget (windows may not exist before realization...
   */
  
  text_view = GTK_TEXT_VIEW (user_data);
  
  if (GTK_WIDGET_MAPPED (text_view) &&
      mark == gtk_text_buffer_get_insert (buffer))
    {
      GdkWindow *tab_window;

      tab_window = gtk_text_view_get_window (text_view,
                                             GTK_TEXT_WINDOW_TOP);

      gdk_window_invalidate_rect (tab_window, NULL, FALSE);
      
      tab_window = gtk_text_view_get_window (text_view,
                                             GTK_TEXT_WINDOW_BOTTOM);

      gdk_window_invalidate_rect (tab_window, NULL, FALSE);
    }
}

static gint
tab_stops_expose (GtkWidget      *widget,
                  GdkEventExpose *event,
                  gpointer        user_data)
{
  gint first_x;
  gint last_x;
  gint i;
  GdkWindow *top_win;
  GdkWindow *bottom_win;
  GtkTextView *text_view;
  GtkTextWindowType type;
  GdkDrawable *target;
  gint *positions = NULL;
  gint size;
  GtkTextAttributes *attrs;
  GtkTextIter insert;
  GtkTextBuffer *buffer;
  gboolean in_pixels;
  
  text_view = GTK_TEXT_VIEW (widget);
  
  /* See if this expose is on the tab stop window */
  top_win = gtk_text_view_get_window (text_view,
                                      GTK_TEXT_WINDOW_TOP);

  bottom_win = gtk_text_view_get_window (text_view,
                                         GTK_TEXT_WINDOW_BOTTOM);

  if (event->window == top_win)
    {
      type = GTK_TEXT_WINDOW_TOP;
      target = top_win;
    }
  else if (event->window == bottom_win)
    {
      type = GTK_TEXT_WINDOW_BOTTOM;
      target = bottom_win;
    }
  else
    return FALSE;
  
  first_x = event->area.x;
  last_x = first_x + event->area.width;

  gtk_text_view_window_to_buffer_coords (text_view,
                                         type,
                                         first_x,
                                         0,
                                         &first_x,
                                         NULL);

  gtk_text_view_window_to_buffer_coords (text_view,
                                         type,
                                         last_x,
                                         0,
                                         &last_x,
                                         NULL);

  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_buffer_get_iter_at_mark (buffer,
                                    &insert,
                                    gtk_text_buffer_get_mark (buffer,
                                                              "insert"));
  
  attrs = gtk_text_attributes_new ();

  gtk_text_iter_get_attributes (&insert, attrs);

  if (attrs->tabs)
    {
      size = pango_tab_array_get_size (attrs->tabs);
      
      pango_tab_array_get_tabs (attrs->tabs,
                                NULL,
                                &positions);

      in_pixels = pango_tab_array_get_positions_in_pixels (attrs->tabs);
    }
  else
    {
      size = 0;
      in_pixels = FALSE;
    }
      
  gtk_text_attributes_unref (attrs);
  
  i = 0;
  while (i < size)
    {
      gint pos;

      if (!in_pixels)
        positions[i] = PANGO_PIXELS (positions[i]);
      
      gtk_text_view_buffer_to_window_coords (text_view,
                                             type,
                                             positions[i],
                                             0,
                                             &pos,
                                             NULL);
      
      gdk_draw_line (target, 
                     widget->style->fg_gc [widget->state],
                     pos, 0,
                     pos, 15); 
      
      ++i;
    }

  g_free (positions);

  return TRUE;
}

static void
get_lines (GtkTextView  *text_view,
           gint          first_y,
           gint          last_y,
           GArray       *buffer_coords,
           GArray       *numbers,
           gint         *countp)
{
  GtkTextIter iter;
  gint count;
  gint size;  

  g_array_set_size (buffer_coords, 0);
  g_array_set_size (numbers, 0);
  
  /* Get iter at first y */
  gtk_text_view_get_line_at_y (text_view, &iter, first_y, NULL);

  /* For each iter, get its location and add it to the arrays.
   * Stop when we pass last_y
   */
  count = 0;
  size = 0;

  while (!gtk_text_iter_is_end (&iter))
    {
      gint y, height;
      gint line_num;
      
      gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

      g_array_append_val (buffer_coords, y);
      line_num = gtk_text_iter_get_line (&iter);
      g_array_append_val (numbers, line_num);
      
      ++count;

      if ((y + height) >= last_y)
        break;
      
      gtk_text_iter_forward_line (&iter);
    }

  *countp = count;
}

static gint
line_numbers_expose (GtkWidget      *widget,
                     GdkEventExpose *event,
                     gpointer        user_data)
{
  gint count;
  GArray *numbers;
  GArray *pixels;
  gint first_y;
  gint last_y;
  gint i;
  GdkWindow *left_win;
  GdkWindow *right_win;
  PangoLayout *layout;
  GtkTextView *text_view;
  GtkTextWindowType type;
  GdkDrawable *target;
  
  text_view = GTK_TEXT_VIEW (widget);
  
  /* See if this expose is on the line numbers window */
  left_win = gtk_text_view_get_window (text_view,
                                       GTK_TEXT_WINDOW_LEFT);

  right_win = gtk_text_view_get_window (text_view,
                                        GTK_TEXT_WINDOW_RIGHT);

  if (event->window == left_win)
    {
      type = GTK_TEXT_WINDOW_LEFT;
      target = left_win;
    }
  else if (event->window == right_win)
    {
      type = GTK_TEXT_WINDOW_RIGHT;
      target = right_win;
    }
  else
    return FALSE;
  
  first_y = event->area.y;
  last_y = first_y + event->area.height;

  gtk_text_view_window_to_buffer_coords (text_view,
                                         type,
                                         0,
                                         first_y,
                                         NULL,
                                         &first_y);

  gtk_text_view_window_to_buffer_coords (text_view,
                                         type,
                                         0,
                                         last_y,
                                         NULL,
                                         &last_y);

  numbers = g_array_new (FALSE, FALSE, sizeof (gint));
  pixels = g_array_new (FALSE, FALSE, sizeof (gint));
  
  get_lines (text_view,
             first_y,
             last_y,
             pixels,
             numbers,
             &count);
  
  /* Draw fully internationalized numbers! */
  
  layout = gtk_widget_create_pango_layout (widget, "");
  
  i = 0;
  while (i < count)
    {
      gint pos;
      gchar *str;
      
      gtk_text_view_buffer_to_window_coords (text_view,
                                             type,
                                             0,
                                             g_array_index (pixels, gint, i),
                                             NULL,
                                             &pos);

      str = g_strdup_printf ("%d", g_array_index (numbers, gint, i));

      pango_layout_set_text (layout, str, -1);

      gtk_paint_layout (widget->style,
                        target,
                        GTK_WIDGET_STATE (widget),
                        NULL,
                        widget,
                        NULL,
                        2, pos + 2,
                        layout);

      g_free (str);
      
      ++i;
    }

  g_array_free (pixels, TRUE);
  g_array_free (numbers, TRUE);
  
  g_object_unref (G_OBJECT (layout));

  return TRUE;
}

static View *
create_view (Buffer *buffer)
{
  View *view;
  
  GtkWidget *sw;
  GtkWidget *vbox;
  
  view = g_new0 (View, 1);
  views = g_slist_prepend (views, view);

  view->buffer = buffer;
  buffer_ref (buffer);
  
  view->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_set_data (G_OBJECT (view->window), "view", view);
  
  gtk_signal_connect (GTK_OBJECT (view->window), "delete_event",
		      GTK_SIGNAL_FUNC (delete_event_cb), NULL);

  view->accel_group = gtk_accel_group_new ();
  view->item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", view->accel_group);
  g_object_set_data (G_OBJECT (view->item_factory), "view", view);
  
  gtk_item_factory_create_items (view->item_factory, G_N_ELEMENTS (menu_items), menu_items, view);

  gtk_window_add_accel_group (GTK_WINDOW (view->window), view->accel_group);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (view->window), vbox);

  gtk_box_pack_start (GTK_BOX (vbox),
		      gtk_item_factory_get_widget (view->item_factory, "<main>"),
		      FALSE, FALSE, 0);
  
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);

  view->text_view = gtk_text_view_new_with_buffer (buffer->buffer);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view->text_view),
                               GTK_WRAP_WORD);
  
  /* Draw tab stops in the top and bottom windows. */
  
  gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view->text_view),
                                        GTK_TEXT_WINDOW_TOP,
                                        15);

  gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view->text_view),
                                        GTK_TEXT_WINDOW_BOTTOM,
                                        15);

  gtk_signal_connect (GTK_OBJECT (view->text_view),
                      "expose_event",
                      GTK_SIGNAL_FUNC (tab_stops_expose),
                      NULL);  

  g_signal_connect_data (G_OBJECT (view->buffer->buffer),
                         "mark_set",
                         GTK_SIGNAL_FUNC (cursor_set_callback),
                         view->text_view, NULL, FALSE, FALSE);
  
  /* Draw line numbers in the side windows; we should really be
   * more scientific about what width we set them to.
   */
  gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view->text_view),
                                        GTK_TEXT_WINDOW_RIGHT,
                                        30);
  
  gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view->text_view),
                                        GTK_TEXT_WINDOW_LEFT,
                                        30);
  
  gtk_signal_connect (GTK_OBJECT (view->text_view),
                      "expose_event",
                      GTK_SIGNAL_FUNC (line_numbers_expose),
                      NULL);
  
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (sw), view->text_view);

  gtk_window_set_default_size (GTK_WINDOW (view->window), 500, 500);

  gtk_widget_grab_focus (view->text_view);

  view_set_title (view);
  view_init_menus (view);

  view_add_example_widgets (view);
  
  gtk_widget_show_all (view->window);
  return view;
}

static void
view_add_example_widgets (View *view)
{
  GtkTextChildAnchor *anchor;
  Buffer *buffer;

  return;
  
  buffer = view->buffer;
  
  anchor = g_object_get_data (G_OBJECT (buffer->buffer),
                              "anchor");

  if (anchor && !gtk_text_child_anchor_get_deleted (anchor))
    {
      GtkWidget *widget;
      
      widget = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
                                         GTK_ICON_SIZE_DIALOG);

      widget = gtk_button_new_with_label ("Foo");
      
      gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view->text_view),
                                         widget,
                                         anchor);

      gtk_widget_show (widget);
    }
}

static gboolean
file_exists (const char *filename)
{
  struct stat statbuf;

  return stat (filename, &statbuf) == 0;
}
void
test_init ()
{
  if (file_exists ("../gdk-pixbuf/.libs/libpixbufloader-pnm.so"))
    {
      putenv ("GDK_PIXBUF_MODULEDIR=../gdk-pixbuf/.libs");
      putenv ("GTK_IM_MODULE_FILE=./gtk.immodules");
    }
}

int
main (int argc, char** argv)
{
  Buffer *buffer;
  View *view;
  int i;

  test_init ();
  gtk_set_locale ();
  gtk_init (&argc, &argv);
  
  buffer = create_buffer ();
  view = create_view (buffer);
  buffer_unref (buffer);
  
  push_active_window (GTK_WINDOW (view->window));
  for (i=1; i < argc; i++)
    {
      char *filename;

      /* Quick and dirty canonicalization - better should be in GLib
       */

      if (!g_path_is_absolute (argv[i]))
	{
	  char *cwd = g_get_current_dir ();
	  filename = g_strconcat (cwd, "/", argv[i], NULL);
	  g_free (cwd);
	}
      else
	filename = argv[i];

      open_ok_func (filename, view);

      if (filename != argv[i])
	g_free (filename);
    }
  pop_active_window ();
  
  gtk_main ();

  return 0;
}


