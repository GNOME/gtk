#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <gio/gunixoutputstream.h>
#include <fcntl.h>


/* This is the guts of gtk_text_buffer_insert_markup,
 * copied here so we can make an incremental version.
 */
static void
insert_tags_for_attributes (GtkTextBuffer     *buffer,
                            PangoAttrIterator *iter,
                            GtkTextIter       *start,
                            GtkTextIter       *end)
{
  GtkTextTagTable *table;
  PangoAttribute *attr;
  GtkTextTag *tag;
  char name[256];

  table = gtk_text_buffer_get_tag_table (buffer);

#define STRING_ATTR(pango_attr_name, attr_name) \
  attr = pango_attr_iterator_get (iter, pango_attr_name); \
  if (attr) \
    { \
      const char *string = ((PangoAttrString*)attr)->value; \
      g_snprintf (name, 256, #attr_name "=%s", string); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, string, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define INT_ATTR(pango_attr_name, attr_name) \
  attr = pango_attr_iterator_get (iter, pango_attr_name); \
  if (attr) \
    { \
      int value = ((PangoAttrInt*)attr)->value; \
      g_snprintf (name, 256, #attr_name "=%d", value); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, value, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define FLOAT_ATTR(pango_attr_name, attr_name) \
  attr = pango_attr_iterator_get (iter, pango_attr_name); \
  if (attr) \
    { \
      float value = ((PangoAttrFloat*)attr)->value; \
      g_snprintf (name, 256, #attr_name "=%g", value); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, value, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

#define RGBA_ATTR(pango_attr_name, attr_name) \
  attr = pango_attr_iterator_get (iter, pango_attr_name); \
  if (attr) \
    { \
      PangoColor *color; \
      GdkRGBA rgba; \
      color = &((PangoAttrColor*)attr)->color; \
      rgba.red = color->red / 65535.; \
      rgba.green = color->green / 65535.; \
      rgba.blue = color->blue / 65535.; \
      rgba.alpha = 1.; \
      char *str = gdk_rgba_to_string (&rgba); \
      g_snprintf (name, 256, #attr_name "=%s", str); \
      g_free (str); \
      tag = gtk_text_tag_table_lookup (table, name); \
      if (!tag) \
        { \
          tag = gtk_text_tag_new (name); \
          g_object_set (tag, #attr_name, &rgba, NULL); \
          gtk_text_tag_table_add (table, tag); \
          g_object_unref (tag); \
        } \
      gtk_text_buffer_apply_tag (buffer, tag, start, end); \
    }

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_LANGUAGE);
  if (attr)
    {
      const char *language = pango_language_to_string (((PangoAttrLanguage*)attr)->value);
      g_snprintf (name, 256, "language=%s", language);
      tag = gtk_text_tag_table_lookup (table, name);
      if (!tag)
        {
          tag = gtk_text_tag_new (name);
          g_object_set (tag, "language", language, NULL);
          gtk_text_tag_table_add (table, tag);
          g_object_unref (tag);
        }
      gtk_text_buffer_apply_tag (buffer, tag, start, end);
    }

  STRING_ATTR (PANGO_ATTR_FAMILY, family)
  INT_ATTR    (PANGO_ATTR_STYLE, style)
  INT_ATTR    (PANGO_ATTR_WEIGHT, weight)
  INT_ATTR    (PANGO_ATTR_VARIANT, variant)
  INT_ATTR    (PANGO_ATTR_STRETCH, stretch)
  INT_ATTR    (PANGO_ATTR_SIZE, size)

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_FONT_DESC);
  if (attr)
    {
      PangoFontDescription *desc = ((PangoAttrFontDesc*)attr)->desc;
      char *str = pango_font_description_to_string (desc);
      g_snprintf (name, 256, "font-desc=%s", str);
      g_free (str);
      tag = gtk_text_tag_table_lookup (table, name);
      if (!tag)
        {
          tag = gtk_text_tag_new (name);
          g_object_set (tag, "font-desc", desc, NULL);
          gtk_text_tag_table_add (table, tag);
          g_object_unref (tag);
        }
      gtk_text_buffer_apply_tag (buffer, tag, start, end);
    }

  RGBA_ATTR   (PANGO_ATTR_FOREGROUND, foreground_rgba)
  RGBA_ATTR   (PANGO_ATTR_BACKGROUND, background_rgba)
  INT_ATTR    (PANGO_ATTR_UNDERLINE, underline)
  RGBA_ATTR   (PANGO_ATTR_UNDERLINE_COLOR, underline_rgba)
  INT_ATTR    (PANGO_ATTR_OVERLINE, overline)
  RGBA_ATTR   (PANGO_ATTR_OVERLINE_COLOR, overline_rgba)
  INT_ATTR    (PANGO_ATTR_STRIKETHROUGH, strikethrough)
  RGBA_ATTR   (PANGO_ATTR_STRIKETHROUGH_COLOR, strikethrough_rgba)
  INT_ATTR    (PANGO_ATTR_RISE, rise)
  FLOAT_ATTR  (PANGO_ATTR_SCALE, scale)
  INT_ATTR    (PANGO_ATTR_FALLBACK, fallback)
  INT_ATTR    (PANGO_ATTR_LETTER_SPACING, letter_spacing)
  STRING_ATTR (PANGO_ATTR_FONT_FEATURES, font_features)
  INT_ATTR    (PANGO_ATTR_ALLOW_BREAKS, allow_breaks)
  INT_ATTR    (PANGO_ATTR_SHOW, show_spaces)
  INT_ATTR    (PANGO_ATTR_INSERT_HYPHENS, insert_hyphens)
}

typedef struct
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;
  PangoAttrList *attributes;
  char *text;
  PangoAttrIterator *attr;
} MarkupData;

static void
free_markup_data (MarkupData *mdata)
{
  gtk_text_buffer_delete_mark (mdata->buffer, mdata->mark);
  pango_attr_iterator_destroy (mdata->attr);
  pango_attr_list_unref (mdata->attributes);
  g_free (mdata->text);
  g_object_unref (mdata->buffer);
  g_free (mdata);
}

static gboolean
insert_markup_idle (gpointer data)
{
  MarkupData *mdata = data;
  gint64 begin;

  begin = g_get_monotonic_time ();

  do
    {
      int start, end;
      int start_offset;
      GtkTextIter start_iter;

      if (g_get_monotonic_time () - begin > G_TIME_SPAN_MILLISECOND)
        {
          g_idle_add (insert_markup_idle, data);
          return G_SOURCE_REMOVE;
        }

      pango_attr_iterator_range (mdata->attr, &start, &end);

      if (end == G_MAXINT) /* last chunk */
        end = start - 1; /* resulting in -1 to be passed to _insert */

      start_offset = gtk_text_iter_get_offset (&mdata->iter);
      gtk_text_buffer_insert (mdata->buffer, &mdata->iter, mdata->text + start, end - start);
      gtk_text_buffer_get_iter_at_offset (mdata->buffer, &start_iter, start_offset);

      insert_tags_for_attributes (mdata->buffer, mdata->attr, &start_iter, &mdata->iter);

      gtk_text_buffer_get_iter_at_mark (mdata->buffer, &mdata->iter, mdata->mark);
    }
  while (pango_attr_iterator_next (mdata->attr));

  free_markup_data (mdata);
  return G_SOURCE_REMOVE;
}

static void
insert_markup (GtkTextBuffer *buffer,
               GtkTextIter   *iter,
               const char    *markup,
               int            len)
{
  char *text;
  PangoAttrList *attributes;
  GError *error = NULL;
  MarkupData *data;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (!pango_parse_markup (markup, len, 0, &attributes, &text, NULL, &error))
    {
      g_warning ("Invalid markup string: %s", error->message);
      g_error_free (error);
      return;
    }

  if (!attributes)
    {
      gtk_text_buffer_insert (buffer, iter, text, -1);
      g_free (text);
      return;
    }

  data = g_new (MarkupData, 1);

  data->buffer = g_object_ref (buffer);
  data->iter = *iter;
  data->attributes = attributes;
  data->text = text;

  /* create mark with right gravity */
  data->mark = gtk_text_buffer_create_mark (buffer, NULL, iter, FALSE);
  data->attr = pango_attr_list_get_iterator (attributes);

  insert_markup_idle (data);
}

static void
fontify_finish (GObject *source,
                GAsyncResult *result,
                gpointer data)
{
  GSubprocess *subprocess = G_SUBPROCESS (source);
  GtkTextBuffer *buffer = data;
  GBytes *stdout_buf = NULL;
  GBytes *stderr_buf = NULL;
  GError *error = NULL;

  if (!g_subprocess_communicate_finish (subprocess,
                                        result,
                                        &stdout_buf,
                                        &stderr_buf,
                                        &error))
    {
      g_clear_pointer (&stdout_buf, g_bytes_unref);
      g_clear_pointer (&stderr_buf, g_bytes_unref);

      g_warning ("%s", error->message);
      g_clear_error (&error);

      g_object_unref (subprocess);
      g_object_unref (buffer);
      return;
    }

  if (g_subprocess_get_exit_status (subprocess) != 0)
    {
      if (stderr_buf)
        g_warning ("%s", (char *)g_bytes_get_data (stderr_buf, NULL));
      g_clear_pointer (&stderr_buf, g_bytes_unref);
    }

  g_object_unref (subprocess);

  g_clear_pointer (&stderr_buf, g_bytes_unref);

  if (stdout_buf)
    {
      char *markup;
      gsize len;
      char *p;
      GtkTextIter start;

      gtk_text_buffer_set_text (buffer, "", 0);

      /* highlight puts a span with font and size around its output,
       * which we don't want.
       */
      markup = g_bytes_unref_to_data (stdout_buf, &len);
      for (p = markup + strlen ("<span "); *p != '>'; p++) *p = ' ';

      gtk_text_buffer_get_start_iter (buffer, &start);
      insert_markup (buffer, &start, markup, len);
   }

  g_object_unref (buffer);
}

void
fontify (const char    *format,
         GtkTextBuffer *source_buffer)
{
  GSubprocess *subprocess;
  char *format_arg;
  GtkSettings *settings;
  char *theme;
  gboolean prefer_dark;
  const char *style_arg;
  const char *text;
  GtkTextIter start, end;
  GBytes *bytes;
  GError *error = NULL;

  settings = gtk_settings_get_default ();
  g_object_get (settings,
                "gtk-theme-name", &theme,
                "gtk-application-prefer-dark-theme", &prefer_dark,
                NULL);

  if (prefer_dark || strcmp (theme, "HighContrastInverse") == 0)
    style_arg = "--style=edit-vim-dark";
  else
    style_arg = "--style=edit-kwrite";

  g_free (theme);

  format_arg = g_strconcat ("--syntax=", format, NULL);
  subprocess = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                 G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                 G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                 &error,
                                 "highlight",
                                 format_arg,
                                 "--out-format=pango",
                                 style_arg,
                                 NULL);
  g_free (format_arg);

  if (!subprocess)
    {
      if (g_error_matches (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT))
        {
          static gboolean warned = FALSE;

          if (!warned)
            {
              warned = TRUE;
              g_message ("For syntax highlighting, install the “highlight” program");
            }
        }
      else
        g_warning ("%s", error->message);

      g_clear_error (&error);

      return;
    }

  gtk_text_buffer_get_bounds (source_buffer, &start, &end);
  text = gtk_text_buffer_get_text (source_buffer, &start, &end, TRUE);
  bytes = g_bytes_new_static (text, strlen (text));

  /* Work around https://gitlab.gnome.org/GNOME/glib/-/issues/2182 */
  if (G_IS_UNIX_OUTPUT_STREAM (g_subprocess_get_stdin_pipe (subprocess)))
    {
      GOutputStream *stdin_pipe = g_subprocess_get_stdin_pipe (subprocess);
      int fd = g_unix_output_stream_get_fd (G_UNIX_OUTPUT_STREAM (stdin_pipe));
      fcntl (fd, F_SETFL, O_NONBLOCK);
    }

  g_subprocess_communicate_async (subprocess,
                                  bytes,
                                  NULL,
                                  fontify_finish,
                                  g_object_ref (source_buffer));
}
