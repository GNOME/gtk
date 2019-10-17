#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include "demos.h"

static GtkWidget *info_view;
static GtkWidget *source_view;

static gchar *current_file = NULL;

static GtkWidget *notebook;
static GtkWidget *treeview;
static GtkWidget *headerbar;

enum {
  NAME_COLUMN,
  TITLE_COLUMN,
  FILENAME_COLUMN,
  FUNC_COLUMN,
  STYLE_COLUMN,
  NUM_COLUMNS
};

typedef struct _CallbackData CallbackData;
struct _CallbackData
{
  GtkTreeModel *model;
  GtkTreePath *path;
};

static void
activate_about (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkApplication *app = user_data;
  const gchar *authors[] = {
    "The GTK Team",
    NULL
  };
  char *version;
  GString *s;

  s = g_string_new ("");

  g_string_append (s, "System libraries\n");
  g_string_append_printf (s, "\tGLib\t%d.%d.%d\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  g_string_append_printf (s, "\tGTK\t%d.%d.%d\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version ());
  g_string_append_printf (s, "\nA link can apppear here: <http://www.gtk.org>");

  version = g_strdup_printf ("%s\nRunning against GTK %d.%d.%d",
                             PACKAGE_VERSION,
                             gtk_get_major_version (),
                             gtk_get_minor_version (),
                             gtk_get_micro_version ());

  gtk_show_about_dialog (GTK_WINDOW (gtk_application_get_active_window (app)),
                         "program-name", "GTK Demo",
                         "version", version,
                         "copyright", "© 1997—2019 The GTK Team",
                         "license-type", GTK_LICENSE_LGPL_2_1,
                         "website", "http://www.gtk.org",
                         "comments", "Program to demonstrate GTK widgets",
                         "authors", authors,
                         "logo-icon-name", "org.gtk.Demo4",
                         "title", "About GTK Demo",
                         "system-information", s->str,
                         NULL);

  g_string_free (s, TRUE);
  g_free (version);
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

static void
activate_inspector (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  gtk_window_set_interactive_debugging (TRUE);
}

static void
window_closed_cb (GtkWidget *window, gpointer data)
{
  CallbackData *cbdata = data;
  GtkTreeIter iter;
  PangoStyle style;

  gtk_tree_model_get_iter (cbdata->model, &iter, cbdata->path);
  gtk_tree_model_get (GTK_TREE_MODEL (cbdata->model), &iter,
                      STYLE_COLUMN, &style,
                      -1);
  if (style == PANGO_STYLE_ITALIC)
    gtk_tree_store_set (GTK_TREE_STORE (cbdata->model), &iter,
                        STYLE_COLUMN, PANGO_STYLE_NORMAL,
                        -1);

  gtk_tree_path_free (cbdata->path);
  g_free (cbdata);
}

static void
run_example_for_row (GtkWidget    *window,
                     GtkTreeModel *model,
                     GtkTreeIter  *iter)
{
  PangoStyle style;
  GDoDemoFunc func;
  GtkWidget *demo;

  gtk_tree_model_get (GTK_TREE_MODEL (model),
                      iter,
                      FUNC_COLUMN, &func,
                      STYLE_COLUMN, &style,
                      -1);

  if (func)
    {
      gtk_tree_store_set (GTK_TREE_STORE (model),
                          iter,
                          STYLE_COLUMN, (style == PANGO_STYLE_ITALIC ? PANGO_STYLE_NORMAL : PANGO_STYLE_ITALIC),
                          -1);
      demo = (func) (window);

      if (demo != NULL)
        {
          CallbackData *cbdata;

          cbdata = g_new (CallbackData, 1);
          cbdata->model = model;
          cbdata->path = gtk_tree_model_get_path (model, iter);

          if (GTK_IS_WINDOW (demo))
            {
              gtk_window_set_transient_for (GTK_WINDOW (demo), GTK_WINDOW (window));
              gtk_window_set_modal (GTK_WINDOW (demo), TRUE);
            }

          g_signal_connect (demo, "destroy",
                            G_CALLBACK (window_closed_cb), cbdata);
        }
    }
}

static void
activate_run (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    run_example_for_row (window, model, &iter);
}

/* Stupid syntax highlighting.
 *
 * No regex was used in the making of this highlighting.
 * It should only work for simple cases.  This is good, as
 * that's all we should have in the demos.
 */
/* This code should not be used elsewhere, except perhaps as an example of how
 * to iterate through a text buffer.
 */
enum {
  STATE_NORMAL,
  STATE_IN_COMMENT
};

static gchar *tokens[] =
{
  "/*",
  "\"",
  NULL
};

static gchar *types[] =
{
  "static",
  "const ",
  "void",
  "gint",
  " int ",
  " char ",
  "gchar ",
  "gfloat",
  "float",
  "double",
  "gint8",
  "gint16",
  "gint32",
  "guint",
  "guint8",
  "guint16",
  "guint32",
  "guchar",
  "glong",
  "gboolean" ,
  "gshort",
  "gushort",
  "gulong",
  "gdouble",
  "gldouble",
  "gpointer",
  "NULL",
  "GList",
  "GSList",
  "FALSE",
  "TRUE",
  "FILE ",
  "GtkColorSelection ",
  "GtkWidget ",
  "GtkButton ",
  "GdkColor ",
  "GdkRectangle ",
  "GdkEventExpose ",
  "GdkGC ",
  "GdkPixbufLoader ",
  "GdkPixbuf ",
  "GError",
  "size_t",
  "GtkAboutDialog ",
  "GtkAction ",
  "GtkActionEntry ",
  "GtkRadioActionEntry ",
  "GtkIconFactory ",
  "GtkTextBuffer ",
  "GtkStatusbar ",
  "GtkTextIter ",
  "GtkTextMark ",
  "GdkEventWindowState ",
  "GtkActionGroup ",
  "GtkUIManager ",
  "GtkRadioAction ",
  "GtkActionClass ",
  "GtkToggleActionEntry ",
  "GtkAssistant ",
  "GtkBuilder ",
  "GtkSizeGroup ",
  "GtkTreeModel ",
  "GtkTreeSelection ",
  "GdkDisplay ",
  "GdkScreen ",
  "GdkSurface ",
  "GdkEventButton ",
  "GdkCursor ",
  "GtkTreeIter ",
  "GtkTreeViewColumn ",
  "GdkDisplayManager ",
  "GdkClipboard ",
  "GtkIconSize ",
  "GtkImage ",
  "GdkDragContext ",
  "GtkSelectionData ",
  "GtkDialog ",
  "GtkMenuItem ",
  "GtkListStore ",
  "GtkCellLayout ",
  "GtkCellRenderer ",
  "GtkTreePath ",
  "GtkTreeStore ",
  "GtkEntry ",
  "GtkEditable ",
  "GtkEditableInterface ",
  "GdkPixmap ",
  "GdkEventConfigure ",
  "GdkEventMotion ",
  "GdkModifierType ",
  "GtkEntryCompletion ",
  "GtkToolItem ",
  "GDir ",
  "GtkIconView ",
  "GtkCellRendererText ",
  "GtkContainer ",
  "GtkAccelGroup ",
  "GtkPaned ",
  "GtkPrintOperation ",
  "GtkPrintContext ",
  "cairo_t ",
  "PangoLayout "
  "PangoFontDescription ",
  "PangoRenderer ",
  "PangoMatrix ",
  "PangoContext ",
  "PangoLayout ",
  "GtkToggleButton ",
  "GString ",
  "GtkIconSize ",
  "GtkTreeView ",
  "GtkTextTag ",
  "GdkEvent ",
  "GdkEventKey ",
  "GtkTextView ",
  "GdkBitmap ",
  "GtkTextChildAnchor ",
  "GArray ",
  "GtkCellEditable ",
  "GtkCellRendererToggle ",
  NULL
};

static gchar *control[] =
{
  " if ",
  " while ",
  " else",
  " do ",
  " for ",
  "?",
  ":",
  "return ",
  "goto ",
  NULL
};
void
parse_chars (gchar     *text,
             gchar    **end_ptr,
             gint      *state,
             gchar    **tag,
             gboolean   start)
{
  gint i;
  gchar *next_token;

  /* Handle comments first */
  if (*state == STATE_IN_COMMENT)
    {
      *end_ptr = strstr (text, "*/");
      if (*end_ptr)
        {
          *end_ptr += 2;
          *state = STATE_NORMAL;
          *tag = "comment";
        }
      return;
    }

  *tag = NULL;
  *end_ptr = NULL;

  /* check for comment */
  if (!strncmp (text, "/*", 2))
    {
      *end_ptr = strstr (text, "*/");
      if (*end_ptr)
        *end_ptr += 2;
      else
        *state = STATE_IN_COMMENT;
      *tag = "comment";
      return;
    }

  /* check for preprocessor defines */
  if (*text == '#' && start)
    {
      *end_ptr = NULL;
      *tag = "preprocessor";
      return;
    }

  /* functions */
  if (start && * text != '\t' && *text != ' ' && *text != '{' && *text != '}')
    {
      if (strstr (text, "("))
        {
          *end_ptr = strstr (text, "(");
          *tag = "function";
          return;
        }
    }
  /* check for types */
  for (i = 0; types[i] != NULL; i++)
    if (!strncmp (text, types[i], strlen (types[i])) ||
        (start && types[i][0] == ' ' && !strncmp (text, types[i] + 1, strlen (types[i]) - 1)))
      {
        *end_ptr = text + strlen (types[i]);
        *tag = "type";
        return;
      }

  /* check for control */
  for (i = 0; control[i] != NULL; i++)
    if (!strncmp (text, control[i], strlen (control[i])))
      {
        *end_ptr = text + strlen (control[i]);
        *tag = "control";
        return;
      }

  /* check for string */
  if (text[0] == '"')
    {
      gint maybe_escape = FALSE;

      *end_ptr = text + 1;
      *tag = "string";
      while (**end_ptr != '\000')
        {
          if (**end_ptr == '\"' && !maybe_escape)
            {
              *end_ptr += 1;
              return;
            }
          if (**end_ptr == '\\')
            maybe_escape = TRUE;
          else
            maybe_escape = FALSE;
          *end_ptr += 1;
        }
      return;
    }

  /* not at the start of a tag.  Find the next one. */
  for (i = 0; tokens[i] != NULL; i++)
    {
      next_token = strstr (text, tokens[i]);
      if (next_token)
        {
          if (*end_ptr)
            *end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
          else
            *end_ptr = next_token;
        }
    }

  for (i = 0; types[i] != NULL; i++)
    {
      next_token = strstr (text, types[i]);
      if (next_token)
        {
          if (*end_ptr)
            *end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
          else
            *end_ptr = next_token;
        }
    }

  for (i = 0; control[i] != NULL; i++)
    {
      next_token = strstr (text, control[i]);
      if (next_token)
        {
          if (*end_ptr)
            *end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
          else
            *end_ptr = next_token;
        }
    }
}

/* While not as cool as c-mode, this will do as a quick attempt at highlighting */
void
fontify (GtkTextBuffer *source_buffer)
{
  GtkTextIter start_iter, next_iter, tmp_iter;
  gint state;
  gchar *text;
  gchar *start_ptr, *end_ptr;
  gchar *tag;

  gtk_text_buffer_create_tag (source_buffer, "source",
                              "font", "monospace",
                              NULL);
  gtk_text_buffer_create_tag (source_buffer, "comment",
                              "foreground", "DodgerBlue",
                              NULL);
  gtk_text_buffer_create_tag (source_buffer, "type",
                              "foreground", "ForestGreen",
                              NULL);
  gtk_text_buffer_create_tag (source_buffer, "string",
                              "foreground", "RosyBrown",
                              "weight", PANGO_WEIGHT_BOLD,
                              NULL);
  gtk_text_buffer_create_tag (source_buffer, "control",
                              "foreground", "purple",
                              NULL);
  gtk_text_buffer_create_tag (source_buffer, "preprocessor",
                              "style", PANGO_STYLE_OBLIQUE,
                              "foreground", "burlywood4",
                              NULL);
  gtk_text_buffer_create_tag (source_buffer, "function",
                              "weight", PANGO_WEIGHT_BOLD,
                              "foreground", "DarkGoldenrod4",
                              NULL);

  gtk_text_buffer_get_bounds (source_buffer, &start_iter, &tmp_iter);
  gtk_text_buffer_apply_tag_by_name (source_buffer, "source", &start_iter, &tmp_iter);

  state = STATE_NORMAL;

  gtk_text_buffer_get_iter_at_offset (source_buffer, &start_iter, 0);

  next_iter = start_iter;
  while (gtk_text_iter_forward_line (&next_iter))
    {
      gboolean start = TRUE;
      start_ptr = text = gtk_text_iter_get_text (&start_iter, &next_iter);

      do
        {
          parse_chars (start_ptr, &end_ptr, &state, &tag, start);

          start = FALSE;
          if (end_ptr)
            {
              tmp_iter = start_iter;
              gtk_text_iter_forward_chars (&tmp_iter, end_ptr - start_ptr);
            }
          else
            {
              tmp_iter = next_iter;
            }
          if (tag)
            gtk_text_buffer_apply_tag_by_name (source_buffer, tag, &start_iter, &tmp_iter);

          start_iter = tmp_iter;
          start_ptr = end_ptr;
        }
      while (end_ptr);

      g_free (text);
      start_iter = next_iter;
    }
}

static GtkWidget *
display_image (const char *resource)
{
  GtkWidget *sw, *image;

  image = gtk_image_new_from_resource (resource);
  gtk_widget_set_halign (image, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (image, GTK_ALIGN_CENTER);
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (sw), image);

  return sw;
}

static GtkWidget *
display_text (const char *resource)
{
  GtkTextBuffer *buffer;
  GtkWidget *textview, *sw;
  GBytes *bytes;

  bytes = g_resources_lookup_data (resource, 0, NULL);
  g_assert (bytes);

  g_assert (g_utf8_validate (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL));

  textview = gtk_text_view_new ();
  g_object_set (textview,
                "left-margin", 20,
                "right-margin", 20,
                "top-margin", 20,
                "bottom-margin", 20,
                NULL);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
  /* Make it a bit nicer for text. */
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
  gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (textview), 2);
  gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (textview), 2);

  buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (buffer, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
  if (g_str_has_suffix (resource, ".c"))
    fontify (buffer);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), buffer);

  g_bytes_unref (bytes);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                       GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (sw), textview);

  return sw;
}

static GtkWidget *
display_video (const char *resource)
{
  GtkWidget *video;

  video = gtk_video_new_for_resource (resource);
  gtk_video_set_loop (GTK_VIDEO (video), TRUE);

  return video;
}

static GtkWidget *
display_nothing (const char *resource)
{
  GtkWidget *widget;
  char *str;

  str = g_strdup_printf ("The lazy GTK developers forgot to add a way to display the resource '%s'", resource);
  widget = gtk_label_new (str);
  gtk_label_set_wrap (GTK_LABEL (widget), TRUE);

  g_free (str);

  return widget;
}

static struct {
  const char *extension;
  GtkWidget * (* display_func) (const char *resource);
} display_funcs[] = {
  { ".gif", display_image },
  { ".jpg", display_image },
  { ".png", display_image },
  { ".c", display_text },
  { ".css", display_text },
  { ".glsl", display_text },
  { ".h", display_text },
  { ".txt", display_text },
  { ".ui", display_text },
  { ".webm", display_video }
};

static void
add_data_tab (const gchar *demoname)
{
  gchar *resource_dir, *resource_name;
  gchar **resources;
  GtkWidget *widget, *label;
  guint i, j;

  resource_dir = g_strconcat ("/", demoname, NULL);
  resources = g_resources_enumerate_children (resource_dir, 0, NULL);
  if (resources == NULL)
    {
      g_free (resource_dir);
      return;
    }

  for (i = 0; resources[i]; i++)
    {
      resource_name = g_strconcat (resource_dir, "/", resources[i], NULL);

      for (j = 0; j < G_N_ELEMENTS(display_funcs); j++)
        {
          if (g_str_has_suffix (resource_name, display_funcs[j].extension))
            break;
        }

      if (j < G_N_ELEMENTS(display_funcs))
        widget = display_funcs[j].display_func (resource_name);
      else
        widget = display_nothing (resource_name);

      label = gtk_label_new (resources[i]);
      gtk_widget_show (label);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);
      g_object_set (gtk_notebook_get_page (GTK_NOTEBOOK (notebook), widget),
                    "tab-expand", FALSE,
                    NULL);

      g_free (resource_name);
    }

  g_strfreev (resources);
  g_free (resource_dir);
}

static void
remove_data_tabs (void)
{
  gint i;

  for (i = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) - 1; i > 1; i--)
    gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), i);
}

void
load_file (const gchar *demoname,
           const gchar *filename)
{
  GtkTextBuffer *info_buffer, *source_buffer;
  GtkTextIter start, end;
  char *resource_filename;
  GError *err = NULL;
  int state = 0;
  gboolean in_para = 0;
  gchar **lines;
  GBytes *bytes;
  gint i;

  if (!g_strcmp0 (current_file, filename))
    return;

  remove_data_tabs ();

  add_data_tab (demoname);

  g_free (current_file);
  current_file = g_strdup (filename);

  info_buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_create_tag (info_buffer, "title",
                              "font", "Sans 18",
                              "pixels-below-lines", 10,
                              NULL);

  source_buffer = gtk_text_buffer_new (NULL);

  resource_filename = g_strconcat ("/sources/", filename, NULL);
  bytes = g_resources_lookup_data (resource_filename, 0, &err);
  g_free (resource_filename);

  if (bytes == NULL)
    {
      g_warning ("Cannot open source for %s: %s", filename, err->message);
      g_error_free (err);
      return;
    }

  lines = g_strsplit (g_bytes_get_data (bytes, NULL), "\n", -1);
  g_bytes_unref (bytes);

  gtk_text_buffer_get_iter_at_offset (info_buffer, &start, 0);
  for (i = 0; lines[i] != NULL; i++)
    {
      gchar *p;
      gchar *q;
      gchar *r;

      /* Make sure \r is stripped at the end for the poor windows people */
      lines[i] = g_strchomp (lines[i]);

      p = lines[i];
      switch (state)
        {
        case 0:
          /* Reading title */
          while (*p == '/' || *p == '*' || g_ascii_isspace (*p))
            p++;
          r = p;
          while (*r != '\0')
            {
              while (*r != '/' && *r != ':' && *r != '\0')
                r++;
              if (*r == '/')
                {
                  r++;
                  p = r;
                }
              if (r[0] == ':' && r[1] == ':')
                *r = '\0';
            }
          q = p + strlen (p);
          while (q > p && g_ascii_isspace (*(q - 1)))
            q--;


          if (q > p)
            {
              int len_chars = g_utf8_pointer_to_offset (p, q);

              end = start;

              g_assert (strlen (p) >= q - p);
              gtk_text_buffer_insert (info_buffer, &end, p, q - p);
              start = end;

              gtk_text_iter_backward_chars (&start, len_chars);
              gtk_text_buffer_apply_tag_by_name (info_buffer, "title", &start, &end);

              start = end;

              while (*p && *p != '\n') p++;

              state++;
            }
          break;

        case 1:
          /* Reading body of info section */
          while (g_ascii_isspace (*p))
            p++;
          if (*p == '*' && *(p + 1) == '/')
            {
              gtk_text_buffer_get_iter_at_offset (source_buffer, &start, 0);
              state++;
            }
          else
            {
              int len;

              while (*p == '*' || g_ascii_isspace (*p))
                p++;

              len = strlen (p);
              while (g_ascii_isspace (*(p + len - 1)))
                len--;

              if (len > 0)
                {
                  if (in_para)
                    gtk_text_buffer_insert (info_buffer, &start, " ", 1);

                  g_assert (strlen (p) >= len);
                  gtk_text_buffer_insert (info_buffer, &start, p, len);
                  in_para = 1;
                }
              else
                {
                  gtk_text_buffer_insert (info_buffer, &start, "\n", 1);
                  in_para = 0;
                }
            }
          break;

        case 2:
          /* Skipping blank lines */
          while (g_ascii_isspace (*p))
            p++;

          if (!*p)
            break;

          p = lines[i];
          state++;
          /* Fall through */

        case 3:
          /* Reading program body */
          gtk_text_buffer_insert (source_buffer, &start, p, -1);
          if (lines[i+1] != NULL)
            gtk_text_buffer_insert (source_buffer, &start, "\n", 1);
          break;
        }
    }

  g_strfreev (lines);

  fontify (source_buffer);

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (source_view), source_buffer);
  g_object_unref (source_buffer);

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (info_view), info_buffer);
  g_object_unref (info_buffer);
}

static void
selection_cb (GtkTreeSelection *selection,
              GtkTreeModel     *model)
{
  GtkTreeIter iter;
  char *name;
  char *filename;
  char *title;

  if (! gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      NAME_COLUMN, &name,
                      TITLE_COLUMN, &title,
                      FILENAME_COLUMN, &filename,
                      -1);

  if (filename)
    load_file (name, filename);

  gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar), title);

  g_free (name);
  g_free (title);
  g_free (filename);
}

static void
populate_model (GtkTreeModel *model)
{
  Demo *d = gtk_demos;

  /* this code only supports 1 level of children. If we
   * want more we probably have to use a recursing function.
   */
  while (d->title)
    {
      Demo *children = d->children;
      GtkTreeIter iter;

      gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

      gtk_tree_store_set (GTK_TREE_STORE (model),
                          &iter,
                          NAME_COLUMN, d->name,
                          TITLE_COLUMN, d->title,
                          FILENAME_COLUMN, d->filename,
                          FUNC_COLUMN, d->func,
                          STYLE_COLUMN, PANGO_STYLE_NORMAL,
                          -1);

      d++;

      if (!children)
        continue;

      while (children->title)
        {
          GtkTreeIter child_iter;

          gtk_tree_store_append (GTK_TREE_STORE (model), &child_iter, &iter);

          gtk_tree_store_set (GTK_TREE_STORE (model),
                              &child_iter,
                              NAME_COLUMN, children->name,
                              TITLE_COLUMN, children->title,
                              FILENAME_COLUMN, children->filename,
                              FUNC_COLUMN, children->func,
                              STYLE_COLUMN, PANGO_STYLE_NORMAL,
                              -1);

          children++;
        }
    }

}

static void
startup (GApplication *app)
{
  GtkBuilder *builder;
  GMenuModel *appmenu;
  gchar *ids[] = { "appmenu", NULL };

  builder = gtk_builder_new ();
  gtk_builder_add_objects_from_resource (builder, "/ui/appmenu.ui", ids, NULL);

  appmenu = (GMenuModel *)gtk_builder_get_object (builder, "appmenu");

  gtk_application_set_app_menu (GTK_APPLICATION (app), appmenu);

  g_object_unref (builder);
}

static void
row_activated_cb (GtkWidget         *tree_view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column)
{
  GtkTreeIter iter;
  GtkWidget *window;
  GtkTreeModel *model;

  window = GTK_WIDGET (gtk_widget_get_root (tree_view));
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
  gtk_tree_model_get_iter (model, &iter, path);

  run_example_for_row (window, model, &iter);
}

static void
start_cb (GtkMenuItem *item, GtkWidget *scrollbar)
{
  GtkAdjustment *adj;

  adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));
  gtk_adjustment_set_value (adj, gtk_adjustment_get_lower (adj));
}

static void
end_cb (GtkMenuItem *item, GtkWidget *scrollbar)
{
  GtkAdjustment *adj;

  adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));
  gtk_adjustment_set_value (adj, gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj));
}

static gboolean
scrollbar_popup (GtkWidget *scrollbar, GtkWidget *menu)
{
  gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);

  return TRUE;
}

static void
activate (GApplication *app)
{
  GtkBuilder *builder;
  GtkWindow *window;
  GtkWidget *widget;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GError *error = NULL;
  GtkWidget *sw;
  GtkWidget *scrollbar;
  GtkWidget *menu;
  GtkWidget *item;

  static GActionEntry win_entries[] = {
    { "run", activate_run, NULL, NULL, NULL }
  };

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/ui/main.ui", &error);
  if (error != NULL)
    {
      g_critical ("%s", error->message);
      exit (1);
    }

  window = (GtkWindow *)gtk_builder_get_object (builder, "window");
  gtk_application_add_window (GTK_APPLICATION (app), window);
  g_action_map_add_action_entries (G_ACTION_MAP (window),
                                   win_entries, G_N_ELEMENTS (win_entries),
                                   window);

  notebook = (GtkWidget *)gtk_builder_get_object (builder, "notebook");

  info_view = (GtkWidget *)gtk_builder_get_object (builder, "info-textview");
  source_view = (GtkWidget *)gtk_builder_get_object (builder, "source-textview");
  headerbar = (GtkWidget *)gtk_builder_get_object (builder, "headerbar");
  treeview = (GtkWidget *)gtk_builder_get_object (builder, "treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  sw = (GtkWidget *)gtk_builder_get_object (builder, "source-scrolledwindow");
  scrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (sw));

  menu = gtk_menu_new ();

  item = gtk_menu_item_new_with_label ("Start");
  g_signal_connect (item, "activate", G_CALLBACK (start_cb), scrollbar);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label ("End");
  g_signal_connect (item, "activate", G_CALLBACK (end_cb), scrollbar);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  g_signal_connect (scrollbar, "popup-menu", G_CALLBACK (scrollbar_popup), menu);

  load_file (gtk_demos[0].name, gtk_demos[0].filename);

  populate_model (model);

  g_signal_connect (treeview, "row-activated", G_CALLBACK (row_activated_cb), model);

  widget = (GtkWidget *)gtk_builder_get_object (builder, "treeview-selection");
  g_signal_connect (widget, "changed", G_CALLBACK (selection_cb), model);

  gtk_tree_model_get_iter_first (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)), &iter);
  gtk_tree_selection_select_iter (GTK_TREE_SELECTION (widget), &iter);

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (treeview));

  gtk_widget_show (GTK_WIDGET (window));

  g_object_unref (builder);
}

static gboolean
auto_quit (gpointer data)
{
  g_application_quit (G_APPLICATION (data));
  return G_SOURCE_REMOVE;
}

static void
list_demos (void)
{
  Demo *d, *c;

  d = gtk_demos;

  while (d->title)
    {
      c = d->children;
      if (d->name)
        g_print ("%s\n", d->name);
      d++;
      while (c && c->title)
        {
          if (c->name)
            g_print ("%s\n", c->name);
          c++;
        }
    }
}

static gint
command_line (GApplication            *app,
              GApplicationCommandLine *cmdline)
{
  GVariantDict *options;
  const gchar *name = NULL;
  gboolean autoquit = FALSE;
  gboolean list = FALSE;
  Demo *d, *c;
  GDoDemoFunc func = 0;
  GtkWidget *window, *demo;

  activate (app);

  options = g_application_command_line_get_options_dict (cmdline);
  g_variant_dict_lookup (options, "run", "&s", &name);
  g_variant_dict_lookup (options, "autoquit", "b", &autoquit);
  g_variant_dict_lookup (options, "list", "b", &list);

  if (list)
    {
      list_demos ();
      g_application_quit (app);
      return 0;
    }

  if (name == NULL)
    goto out;

  window = gtk_application_get_windows (GTK_APPLICATION (app))->data;

  d = gtk_demos;

  while (d->title)
    {
      c = d->children;
      if (g_strcmp0 (d->name, name) == 0)
        {
          func = d->func;
          goto out;
        }
      d++;
      while (c && c->title)
        {
          if (g_strcmp0 (c->name, name) == 0)
            {
              func = c->func;
              goto out;
            }
          c++;
        }
    }

out:
  if (func)
    {
      demo = (func) (window);

      gtk_window_set_transient_for (GTK_WINDOW (demo), GTK_WINDOW (window));
      gtk_window_set_modal (GTK_WINDOW (demo), TRUE);
    }

  if (autoquit)
    g_timeout_add_seconds (1, auto_quit, app);

  return 0;
}

static void
print_version (void)
{
  g_print ("gtk3-demo %d.%d.%d\n",
           gtk_get_major_version (),
           gtk_get_minor_version (),
           gtk_get_micro_version ());
}

static int
local_options (GApplication *app,
               GVariantDict *options,
               gpointer      data)
{
  gboolean version = FALSE;

  g_variant_dict_lookup (options, "version", "b", &version);

  if (version)
    {
      print_version ();
      return 0;
    }

  return -1;
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  static GActionEntry app_entries[] = {
    { "about", activate_about, NULL, NULL, NULL },
    { "quit", activate_quit, NULL, NULL, NULL },
    { "inspector", activate_inspector, NULL, NULL, NULL },
  };

  /* Most code in gtk-demo is intended to be exemplary, but not
   * these few lines, which are just a hack so gtk-demo will work
   * in the GTK tree without installing it.
   */
  if (g_file_test ("../../modules/input/immodules.cache", G_FILE_TEST_EXISTS))
    {
      g_setenv ("GTK_IM_MODULE_FILE", "../../modules/input/immodules.cache", TRUE);
    }
  /* -- End of hack -- */

  app = gtk_application_new ("org.gtk.Demo4", G_APPLICATION_NON_UNIQUE|G_APPLICATION_HANDLES_COMMAND_LINE);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);

  g_application_add_main_option (G_APPLICATION (app), "version", 0, 0, G_OPTION_ARG_NONE, "Show program version", NULL);
  g_application_add_main_option (G_APPLICATION (app), "run", 0, 0, G_OPTION_ARG_STRING, "Run an example", "EXAMPLE");
  g_application_add_main_option (G_APPLICATION (app), "list", 0, 0, G_OPTION_ARG_NONE, "List examples", NULL);
  g_application_add_main_option (G_APPLICATION (app), "autoquit", 0, 0, G_OPTION_ARG_NONE, "Quit after a delay", NULL);

  g_signal_connect (app, "startup", G_CALLBACK (startup), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "command-line", G_CALLBACK (command_line), NULL);
  g_signal_connect (app, "handle-local-options", G_CALLBACK (local_options), NULL);

  g_application_run (G_APPLICATION (app), argc, argv);

  return 0;
}
