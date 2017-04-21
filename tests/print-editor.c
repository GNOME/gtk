#include <math.h>
#include <pango/pangocairo.h>
#include <gtk/gtk.h>

static GtkWidget *main_window;
static char *filename = NULL;
static GtkPageSetup *page_setup = NULL;
static GtkPrintSettings *settings = NULL;
static gboolean file_changed = FALSE;
static GtkTextBuffer *buffer;
static GtkWidget *statusbar;
static GList *active_prints = NULL;

static void
update_title (GtkWindow *window)
{
  char *basename;
  char *title;

  if (filename == NULL)
    basename = g_strdup ("Untitled");
  else
    basename = g_path_get_basename (filename);

  title = g_strdup_printf ("Simple Editor with printing - %s", basename);
  g_free (basename);

  gtk_window_set_title (window, title);
  g_free (title);
}

static void
update_statusbar (void)
{
  gchar *msg;
  gint row, col;
  GtkTextIter iter;
  const char *print_str;

  gtk_statusbar_pop (GTK_STATUSBAR (statusbar), 0);
  
  gtk_text_buffer_get_iter_at_mark (buffer,
                                    &iter,
                                    gtk_text_buffer_get_insert (buffer));

  row = gtk_text_iter_get_line (&iter);
  col = gtk_text_iter_get_line_offset (&iter);

  print_str = "";
  if (active_prints)
    {
      GtkPrintOperation *op = active_prints->data;
      print_str = gtk_print_operation_get_status_string (op);
    }
  
  msg = g_strdup_printf ("%d, %d%s %s",
                         row, col,
			 file_changed?" - Modified":"",
			 print_str);

  gtk_statusbar_push (GTK_STATUSBAR (statusbar), 0, msg);

  g_free (msg);
}

static void
update_ui (void)
{
  update_title (GTK_WINDOW (main_window));
  update_statusbar ();
}

static char *
get_text (void)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static void
set_text (const char *text, gsize len)
{
  gtk_text_buffer_set_text (buffer, text, len);
  file_changed = FALSE;
  update_ui ();
}

static void
load_file (const char *open_filename)
{
  GtkWidget *error_dialog;
  char *contents;
  GError *error;
  gsize len;

  error_dialog = NULL;
  error = NULL;
  if (g_file_get_contents (open_filename, &contents, &len, &error))
    {
      if (g_utf8_validate (contents, len, NULL))
	{
	  filename = g_strdup (open_filename);
	  set_text (contents, len);
	  g_free (contents);
	}
      else
	{
	  error_dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 "Error loading file %s:\n%s",
						 open_filename,
						 "Not valid utf8");
	}
    }
  else
    {
      error_dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_CLOSE,
					     "Error loading file %s:\n%s",
					     open_filename,
					     error->message);
      
      g_error_free (error);
    }
  if (error_dialog)
    {
      g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (error_dialog);
    }
}


static void
save_file (const char *save_filename)
{
  char *text = get_text ();
  GtkWidget *error_dialog;
  GError *error;

  error = NULL;
  if (g_file_set_contents (save_filename,
			   text, -1, &error))
    {
      if (save_filename != filename)
	{
	  g_free (filename);
	  filename = g_strdup (save_filename);
	}
      file_changed = FALSE;
      update_ui ();
    }
  else
    {
      error_dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_CLOSE,
					     "Error saving to file %s:\n%s",
					     filename,
					     error->message);
      
      g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (error_dialog);
      
      g_error_free (error);
    }
}


typedef struct {
  char *text;
  PangoLayout *layout;
  GList *page_breaks;
  GtkWidget *font_button;
  char *font;
} PrintData;

static void
begin_print (GtkPrintOperation *operation,
	     GtkPrintContext *context,
	     PrintData *print_data)
{
  PangoFontDescription *desc;
  PangoLayoutLine *layout_line;
  double width, height;
  double page_height;
  GList *page_breaks;
  int num_lines;
  int line;

  width = gtk_print_context_get_width (context);
  height = gtk_print_context_get_height (context);

  print_data->layout = gtk_print_context_create_pango_layout (context);

  desc = pango_font_description_from_string (print_data->font);
  pango_layout_set_font_description (print_data->layout, desc);
  pango_font_description_free (desc);

  pango_layout_set_width (print_data->layout, width * PANGO_SCALE);
  
  pango_layout_set_text (print_data->layout, print_data->text, -1);

  num_lines = pango_layout_get_line_count (print_data->layout);

  page_breaks = NULL;
  page_height = 0;

  for (line = 0; line < num_lines; line++)
    {
      PangoRectangle ink_rect, logical_rect;
      double line_height;
      
      layout_line = pango_layout_get_line (print_data->layout, line);
      pango_layout_line_get_extents (layout_line, &ink_rect, &logical_rect);

      line_height = logical_rect.height / 1024.0;

      if (page_height + line_height > height)
	{
	  page_breaks = g_list_prepend (page_breaks, GINT_TO_POINTER (line));
	  page_height = 0;
	}

      page_height += line_height;
    }

  page_breaks = g_list_reverse (page_breaks);
  gtk_print_operation_set_n_pages (operation, g_list_length (page_breaks) + 1);
  
  print_data->page_breaks = page_breaks;
}

static void
draw_page (GtkPrintOperation *operation,
	   GtkPrintContext *context,
	   int page_nr,
	   PrintData *print_data)
{
  cairo_t *cr;
  GList *pagebreak;
  int start, end, i;
  PangoLayoutIter *iter;
  double start_pos;

  if (page_nr == 0)
    start = 0;
  else
    {
      pagebreak = g_list_nth (print_data->page_breaks, page_nr - 1);
      start = GPOINTER_TO_INT (pagebreak->data);
    }

  pagebreak = g_list_nth (print_data->page_breaks, page_nr);
  if (pagebreak == NULL)
    end = pango_layout_get_line_count (print_data->layout);
  else
    end = GPOINTER_TO_INT (pagebreak->data);
    
  cr = gtk_print_context_get_cairo_context (context);

  cairo_set_source_rgb (cr, 0, 0, 0);
  
  i = 0;
  start_pos = 0;
  iter = pango_layout_get_iter (print_data->layout);
  do
    {
      PangoRectangle   logical_rect;
      PangoLayoutLine *line;
      int              baseline;

      if (i >= start)
	{
	  line = pango_layout_iter_get_line (iter);

	  pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
	  baseline = pango_layout_iter_get_baseline (iter);
	  
	  if (i == start)
	    start_pos = logical_rect.y / 1024.0;
	  
	  cairo_move_to (cr, logical_rect.x / 1024.0, baseline / 1024.0 - start_pos);
	  
	  pango_cairo_show_layout_line  (cr, line);
	}
      i++;
    }
  while (i < end &&
	 pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);
}

static void
status_changed_cb (GtkPrintOperation *op,
		   gpointer user_data)
{
  if (gtk_print_operation_is_finished (op))
    {
      active_prints = g_list_remove (active_prints, op);
      g_object_unref (op);
    }
  update_statusbar ();
}

static GtkWidget *
create_custom_widget (GtkPrintOperation *operation,
		      PrintData *data)
{
  GtkWidget *vbox, *hbox, *font, *label;

  gtk_print_operation_set_custom_tab_label (operation, "Other");
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (vbox), hbox);
  gtk_widget_show (hbox);

  label = gtk_label_new ("Font:");
  gtk_box_pack_start (GTK_BOX (hbox), label);
  gtk_widget_show (label);

  font = gtk_font_button_new_with_font  (data->font);
  gtk_box_pack_start (GTK_BOX (hbox), font);
  gtk_widget_show (font);
  data->font_button = font;

  return vbox;
}

static void
custom_widget_apply (GtkPrintOperation *operation,
		     GtkWidget *widget,
		     PrintData *data)
{
  const char *selected_font;
  selected_font = gtk_font_button_get_font_name  (GTK_FONT_BUTTON (data->font_button));
  g_free (data->font);
  data->font = g_strdup (selected_font);
}

static void
print_done (GtkPrintOperation *op,
	    GtkPrintOperationResult res,
	    PrintData *print_data)
{
  GError *error = NULL;

  if (res == GTK_PRINT_OPERATION_RESULT_ERROR)
    {

      GtkWidget *error_dialog;
      
      gtk_print_operation_get_error (op, &error);
      
      error_dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
					     GTK_DIALOG_DESTROY_WITH_PARENT,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_CLOSE,
					     "Error printing file:\n%s",
					     error ? error->message : "no details");
      g_signal_connect (error_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (error_dialog);
    }
  else if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
    {
      if (settings != NULL)
	g_object_unref (settings);
      settings = g_object_ref (gtk_print_operation_get_print_settings (op));
    }

  g_free (print_data->text);
  g_free (print_data->font);
  g_free (print_data);
  
  if (!gtk_print_operation_is_finished (op))
    {
      g_object_ref (op);
      active_prints = g_list_append (active_prints, op);
      update_statusbar ();
      
      /* This ref is unref:ed when we get the final state change */
      g_signal_connect (op, "status_changed",
			G_CALLBACK (status_changed_cb), NULL);
    }
}

static void
end_print (GtkPrintOperation *op, GtkPrintContext *context, PrintData *print_data)
{
  g_list_free (print_data->page_breaks);
  print_data->page_breaks = NULL;
  g_object_unref (print_data->layout);
  print_data->layout = NULL;
}

static void
print_or_preview (GSimpleAction *action, GtkPrintOperationAction print_action)
{
  GtkPrintOperation *print;
  PrintData *print_data;

  print_data = g_new0 (PrintData, 1);

  print_data->text = get_text ();
  print_data->font = g_strdup ("Sans 12");

  print = gtk_print_operation_new ();

  gtk_print_operation_set_track_print_status (print, TRUE);

  if (settings != NULL)
    gtk_print_operation_set_print_settings (print, settings);

  if (page_setup != NULL)
    gtk_print_operation_set_default_page_setup (print, page_setup);

  g_signal_connect (print, "begin_print", G_CALLBACK (begin_print), print_data);
  g_signal_connect (print, "end-print", G_CALLBACK (end_print), print_data);
  g_signal_connect (print, "draw_page", G_CALLBACK (draw_page), print_data);
  g_signal_connect (print, "create_custom_widget", G_CALLBACK (create_custom_widget), print_data);
  g_signal_connect (print, "custom_widget_apply", G_CALLBACK (custom_widget_apply), print_data);

  g_signal_connect (print, "done", G_CALLBACK (print_done), print_data);

  gtk_print_operation_set_export_filename (print, "test.pdf");

#if 0
  gtk_print_operation_set_allow_async (print, TRUE);
#endif
  gtk_print_operation_run (print, print_action, GTK_WINDOW (main_window), NULL);

  g_object_unref (print);
}

static void
activate_page_setup (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  GtkPageSetup *new_page_setup;

  new_page_setup = gtk_print_run_page_setup_dialog (GTK_WINDOW (main_window),
                                                    page_setup, settings);

  if (page_setup)
    g_object_unref (page_setup);

  page_setup = new_page_setup;
}

static void
activate_print (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  print_or_preview (action, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
activate_preview (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  print_or_preview (action, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
activate_save_as (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  GtkWidget *dialog;
  gint response;
  char *save_filename;

  dialog = gtk_file_chooser_dialog_new ("Select file",
                                        GTK_WINDOW (main_window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Save", GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      save_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      save_file (save_filename);
      g_free (save_filename);
    }

  gtk_widget_destroy (dialog);
}

static void
activate_save (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  if (filename == NULL)
    activate_save_as (action, NULL, NULL);
  else
    save_file (filename);
}

static void
activate_open (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkWidget *dialog;
  gint response;
  char *open_filename;

  dialog = gtk_file_chooser_dialog_new ("Select file",
                                        GTK_WINDOW (main_window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Open", GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_OK)
    {
      open_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      load_file (open_filename);
      g_free (open_filename);
    }

  gtk_widget_destroy (dialog);
}

static void
activate_new (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  g_free (filename);
  filename = NULL;
  set_text ("", 0);
}

static void
activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  const gchar *authors[] = {
    "Alexander Larsson",
    NULL
  };
  gtk_show_about_dialog (GTK_WINDOW (main_window),
                         "name", "Print Test Editor",
                         "logo-icon-name", "text-editor",
                         "version", "0.1",
                         "copyright", "(C) Red Hat, Inc",
                         "comments", "Program to demonstrate GTK+ printing.",
                         "authors", authors,
                         NULL);
}

static void
activate_quit (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkApplication *app = user_data;
  GtkWidget *win;
  GList *list, *next;

  list = gtk_application_get_windows (app);
  while (list)
    {
      win = list->data;
      next = list->next;

      gtk_widget_destroy (GTK_WIDGET (win));

      list = next;
    }
}

static GActionEntry app_entries[] = {
  { "new", activate_new, NULL, NULL, NULL },
  { "open", activate_open, NULL, NULL, NULL },
  { "save", activate_save, NULL, NULL, NULL },
  { "save-as", activate_save_as, NULL, NULL, NULL },
  { "quit", activate_quit, NULL, NULL, NULL },
  { "about", activate_about, NULL, NULL, NULL },
  { "page-setup", activate_page_setup, NULL, NULL, NULL },
  { "preview", activate_preview, NULL, NULL, NULL },
  { "print", activate_print, NULL, NULL, NULL }
};

static const gchar ui_info[] =
  "<interface>"
  "  <menu id='appmenu'>"
  "    <section>"
  "      <item>"
  "        <attribute name='label'>_About</attribute>"
  "        <attribute name='action'>app.about</attribute>"
  "        <attribute name='accel'>&lt;Primary&gt;a</attribute>"
  "      </item>"
  "    </section>"
  "    <section>"
  "      <item>"
  "        <attribute name='label'>_Quit</attribute>"
  "        <attribute name='action'>app.quit</attribute>"
  "        <attribute name='accel'>&lt;Primary&gt;q</attribute>"
  "      </item>"
  "    </section>"
  "  </menu>"
  "  <menu id='menubar'>"
  "    <submenu>"
  "      <attribute name='label'>_File</attribute>"
  "      <section>"
  "        <item>"
  "          <attribute name='label'>_New</attribute>"
  "          <attribute name='action'>app.new</attribute>"
  "          <attribute name='accel'>&lt;Primary&gt;n</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label'>_Open</attribute>"
  "          <attribute name='action'>app.open</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label'>_Save</attribute>"
  "          <attribute name='action'>app.save</attribute>"
  "          <attribute name='accel'>&lt;Primary&gt;s</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label'>Save _As...</attribute>"
  "          <attribute name='action'>app.save-as</attribute>"
  "          <attribute name='accel'>&lt;Primary&gt;s</attribute>"
  "        </item>"
  "      </section>"
  "      <section>"
  "        <item>"
  "          <attribute name='label'>Page Setup</attribute>"
  "          <attribute name='action'>app.page-setup</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label'>Preview</attribute>"
  "          <attribute name='action'>app.preview</attribute>"
  "        </item>"
  "        <item>"
  "          <attribute name='label'>Print</attribute>"
  "          <attribute name='action'>app.print</attribute>"
  "        </item>"
  "      </section>"
  "    </submenu>"
  "  </menu>"
  "</interface>";

static void
buffer_changed_callback (GtkTextBuffer *buffer)
{
  file_changed = TRUE;
  update_statusbar ();
}

static void
mark_set_callback (GtkTextBuffer     *buffer,
                   const GtkTextIter *new_location,
                   GtkTextMark       *mark,
                   gpointer           data)
{
  update_statusbar ();
}

static gint
command_line (GApplication            *application,
              GApplicationCommandLine *command_line)
{
  int argc;
  char **argv;

  argv = g_application_command_line_get_arguments (command_line, &argc);

  if (argc == 2)
    load_file (argv[1]);

  return 0;
}

static void
startup (GApplication *app)
{
  GtkBuilder *builder;
  GMenuModel *appmenu;
  GMenuModel *menubar;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, ui_info, -1, NULL);

  appmenu = (GMenuModel *)gtk_builder_get_object (builder, "appmenu");
  menubar = (GMenuModel *)gtk_builder_get_object (builder, "menubar");

  gtk_application_set_app_menu (GTK_APPLICATION (app), appmenu);
  gtk_application_set_menubar (GTK_APPLICATION (app), menubar);

  g_object_unref (builder);
}

static void
activate (GApplication *app)
{
  GtkWidget *box;
  GtkWidget *bar;
  GtkWidget *sw;
  GtkWidget *contents;

  main_window = gtk_application_window_new (GTK_APPLICATION (app));
  gtk_window_set_icon_name (GTK_WINDOW (main_window), "text-editor");
  gtk_window_set_default_size (GTK_WINDOW (main_window), 400, 600);
  update_title (GTK_WINDOW (main_window));

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (main_window), box);

  bar = gtk_menu_bar_new ();
  gtk_widget_show (bar);
  gtk_container_add (GTK_CONTAINER (box), bar);

  /* Create document  */
  sw = gtk_scrolled_window_new (NULL, NULL);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                       GTK_SHADOW_IN);

  gtk_widget_set_vexpand (sw, TRUE);
  gtk_container_add (GTK_CONTAINER (box), sw);

  contents = gtk_text_view_new ();
  gtk_widget_grab_focus (contents);

  gtk_container_add (GTK_CONTAINER (sw),
                     contents);

  /* Create statusbar */
  statusbar = gtk_statusbar_new ();
  gtk_container_add (GTK_CONTAINER (box), statusbar);

  /* Show text widget info in the statusbar */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (contents));

  g_signal_connect_object (buffer,
                           "changed",
                           G_CALLBACK (buffer_changed_callback),
                           NULL,
                           0);

  g_signal_connect_object (buffer,
                           "mark_set", /* cursor moved */
                           G_CALLBACK (mark_set_callback),
                           NULL,
                           0);

  update_ui ();

  gtk_widget_show (main_window);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  GError *error = NULL;

  gtk_init ();

  settings = gtk_print_settings_new_from_file ("print-settings.ini", &error);
  if (error) {
    g_print ("Failed to load print settings: %s\n", error->message);
    g_clear_error (&error);

    settings = gtk_print_settings_new ();
  }
  g_assert (settings != NULL);

  page_setup = gtk_page_setup_new_from_file ("page-setup.ini", &error);
  if (error) {
    g_print ("Failed to load page setup: %s\n", error->message);
    g_clear_error (&error);
  }

  app = gtk_application_new ("org.gtk.PrintEditor", 0);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);

  g_signal_connect (app, "startup", G_CALLBACK (startup), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  if (!gtk_print_settings_to_file (settings, "print-settings.ini", &error)) {
    g_print ("Failed to save print settings: %s\n", error->message);
    g_clear_error (&error);
  }
  if (page_setup &&
      !gtk_page_setup_to_file (page_setup, "page-setup.ini", &error)) {
    g_print ("Failed to save page setup: %s\n", error->message);
    g_clear_error (&error);
  }

  return 0;
}
