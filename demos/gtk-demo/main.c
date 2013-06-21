#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include "demos.h"

static GtkWidget *info_view;
static GtkWidget *source_view;

static gchar *current_file = NULL;

static GtkWidget *notebook;

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
  "GtkIconSet ",
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
  "GdkWindow ",
  "GdkEventButton ",
  "GdkCursor ",
  "GtkTreeIter ",
  "GtkTreeViewColumn ",
  "GdkDisplayManager ",
  "GtkClipboard ",
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
  "GtkTable ",
  "GtkToggleButton ",
  "GString ",
  "GtkIconSize ",
  "GtkTreeView ",
  "GtkTextTag ",
  "GdkEvent ",
  "GdkEventKey ",
  "GtkTextView ",
  "GdkEventVisibility ",
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
static void
fontify (GtkTextBuffer *source_buffer)
{
  GtkTextIter start_iter, next_iter, tmp_iter;
  gint state;
  gchar *text;
  gchar *start_ptr, *end_ptr;
  gchar *tag;

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

static GtkWidget *create_text (GtkWidget **text_view, gboolean is_source);

static void
add_data_tab (const gchar *demoname)
{
  gchar *resource_dir, *resource_name, *content_type, *content_mime;
  gchar **resources;
  GBytes *bytes;
  GtkWidget *widget, *label;
  guint i;

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
      bytes = g_resources_lookup_data (resource_name, 0, NULL);
      g_assert (bytes);

      content_type = g_content_type_guess (resource_name,
                                           g_bytes_get_data (bytes, NULL),
                                           g_bytes_get_size (bytes),
                                           NULL);
      content_mime = g_content_type_get_mime_type (content_type);

      /* In theory we should look at all the mime types gdk-pixbuf supports
       * and go from there, but we know what file types we've added.
       */
      if (g_content_type_is_a (content_mime, "image/png") ||
          g_content_type_is_a (content_mime, "image/gif") ||
          g_content_type_is_a (content_mime, "image/jpeg"))
        {
          widget = gtk_image_new_from_resource (resource_name);
        }
      else if (g_content_type_is_a (content_mime, "text/plain") ||
               g_content_type_is_a (content_mime, "application/x-ext-ui") ||
               g_content_type_is_a (content_mime, "text/css"))
        {
          GtkTextBuffer *buffer;
          GtkWidget *textview;

          widget = create_text (&textview, FALSE);
          buffer = gtk_text_buffer_new (NULL);
          gtk_text_buffer_set_text (buffer, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
          gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), buffer);
        }
      else
        {

          g_warning ("Don't know how to display resource '%s' of type '%s'\n", resource_name, content_mime);
          widget = NULL;
        }

      gtk_widget_show_all (widget);
      label = gtk_label_new (resources[i]);
      gtk_widget_show (label);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), widget, label);

      g_free (content_mime);
      g_free (content_type);
      g_free (resource_name);
      g_bytes_unref (bytes);
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

  resource_filename = g_strconcat ("/sources/", filename, NULL);
  bytes = g_resources_lookup_data (resource_filename, 0, &err);
  g_free (resource_filename);

  if (bytes == NULL)
    {
      g_warning ("Cannot open source for %s: %s\n", filename, err->message);
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
          if (*p)
            {
              p = lines[i];
              state++;
              /* Fall through */
            }
          else
            break;

        case 3:
          /* Reading program body */
          gtk_text_buffer_insert (source_buffer, &start, p, -1);
          gtk_text_buffer_insert (source_buffer, &start, "\n", 1);
          break;
        }
    }

  fontify (source_buffer);

  g_strfreev (lines);

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (info_view), info_buffer);
  g_object_unref (info_buffer);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (source_view), source_buffer);
  g_object_unref (source_buffer);
}

void
row_activated_cb (GtkTreeView       *tree_view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column)
{
  GtkTreeIter iter;
  PangoStyle style;
  GDoDemoFunc func;
  GtkWidget *window;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (tree_view);

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (model),
                      &iter,
                      FUNC_COLUMN, &func,
                      STYLE_COLUMN, &style,
                      -1);

  if (func)
    {
      gtk_tree_store_set (GTK_TREE_STORE (model),
                          &iter,
                          STYLE_COLUMN, (style == PANGO_STYLE_ITALIC ? PANGO_STYLE_NORMAL : PANGO_STYLE_ITALIC),
                          -1);
      window = (func) (gtk_widget_get_toplevel (GTK_WIDGET (tree_view)));

      if (window != NULL)
        {
          CallbackData *cbdata;

          cbdata = g_new (CallbackData, 1);
          cbdata->model = model;
          cbdata->path = gtk_tree_path_copy (path);

          g_signal_connect (window, "destroy",
                            G_CALLBACK (window_closed_cb), cbdata);
        }
    }
}

static void
selection_cb (GtkTreeSelection *selection,
              GtkTreeModel     *model)
{
  GtkTreeIter iter;
  char *name, *filename;

  if (! gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_get (model, &iter,
                      NAME_COLUMN, &name,
                      FILENAME_COLUMN, &filename,
                      -1);

  if (filename)
    load_file (name, filename);

  g_free (name);
  g_free (filename);
}

static GtkWidget *
create_text (GtkWidget **view,
             gboolean    is_source)
{
  GtkWidget *scrolled_window;
  GtkWidget *text_view;
  PangoFontDescription *font_desc;

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_IN);

  *view = text_view = gtk_text_view_new ();
  g_object_set (text_view, "margin", 20, NULL);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);

  gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);

  if (is_source)
    {
      font_desc = pango_font_description_from_string ("monospace");
      gtk_widget_override_font (text_view, font_desc);
      pango_font_description_free (font_desc);

      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),
                                   GTK_WRAP_NONE);
    }
  else
    {
      /* Make it a bit nicer for text. */
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),
                                   GTK_WRAP_WORD);
      gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (text_view),
                                            2);
      gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (text_view),
                                            2);
    }

  return scrolled_window;
}

static GtkWidget *
create_tree (void)
{
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkTreeStore *model;
  GtkTreeIter iter;
  GtkWidget *box, *label, *scrolled_window;

  Demo *d = gtk_demos;

  model = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT);
  tree_view = gtk_tree_view_new ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (model));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
                               GTK_SELECTION_BROWSE);
  gtk_widget_set_size_request (tree_view, 200, -1);

  /* this code only supports 1 level of children. If we
   * want more we probably have to use a recursing function.
   */
  while (d->title)
    {
      Demo *children = d->children;

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

  cell = gtk_cell_renderer_text_new ();

  column = gtk_tree_view_column_new_with_attributes ("Widget (double click for demo)",
                                                     cell,
                                                     "text", TITLE_COLUMN,
                                                     "style", STYLE_COLUMN,
                                                     NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
                               GTK_TREE_VIEW_COLUMN (column));

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);
  gtk_tree_selection_select_iter (GTK_TREE_SELECTION (selection), &iter);

  g_signal_connect (selection, "changed", G_CALLBACK (selection_cb), model);
  g_signal_connect (tree_view, "row_activated", G_CALLBACK (row_activated_cb), model);

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (tree_view));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

  label = gtk_label_new ("Widget (double click for demo)");

  box = gtk_notebook_new ();
  gtk_notebook_append_page (GTK_NOTEBOOK (box), scrolled_window, label);

  gtk_widget_grab_focus (tree_view);

   g_object_unref (model);

  return box;
}

static void
setup_default_icon (void)
{
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new_from_resource ("/gtk-logo-old.png", NULL);
  /* We load a resource, so we can guarantee that loading it is successful */
  g_assert (pixbuf);

  gtk_window_set_default_icon (pixbuf);
  
  g_object_unref (pixbuf);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *hbox;
  GtkWidget *tree;

  /* Most code in gtk-demo is intended to be exemplary, but not
   * these few lines, which are just a hack so gtk-demo will work
   * in the GTK tree without installing it.
   */
  if (g_file_test ("../../modules/input/immodules.cache", G_FILE_TEST_EXISTS))
    {
      g_setenv ("GTK_IM_MODULE_FILE", "../../modules/input/immodules.cache", TRUE);
    }
  /* -- End of hack -- */

  gtk_init (&argc, &argv);

  setup_default_icon ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GTK+ Code Demos");
  g_signal_connect_after (window, "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  tree = create_tree ();
  gtk_box_pack_start (GTK_BOX (hbox), tree, FALSE, FALSE, 0);

  notebook = gtk_notebook_new ();
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), notebook, TRUE, TRUE, 0);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            create_text (&info_view, FALSE),
                            gtk_label_new_with_mnemonic ("_Info"));

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            create_text (&source_view, TRUE),
                            gtk_label_new_with_mnemonic ("_Source"));

  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  gtk_widget_show_all (window);

  load_file (gtk_demos[0].name, gtk_demos[0].filename);

  gtk_main ();

  return 0;
}
