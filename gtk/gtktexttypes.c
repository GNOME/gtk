#include "gtktexttypes.h"


/*
 * Tab array
 */

GtkTextTabArray*
gtk_text_view_tab_array_new(guint size)
{
  GtkTextTabArray *array;

  array = g_new0(GtkTextTabArray, 1);

  array->refcount = 1;
  array->numTabs = size;
  array->tabs = g_new0(GtkTextTab, size);

  return array;
}

void
gtk_text_view_tab_array_ref(GtkTextTabArray *tab_array)
{
  g_return_if_fail(tab_array != NULL);

  tab_array->refcount += 1;
}

void
gtk_text_view_tab_array_unref(GtkTextTabArray *tab_array)
{
  g_return_if_fail(tab_array != NULL);
  g_return_if_fail(tab_array->refcount > 0);

  tab_array->refcount -= 1;

  if (tab_array->refcount == 0)
    {
      g_free(tab_array->tabs);
      g_free(tab_array);
    }
}

/* These are used to represent embedded non-character objects
 * if you return a string representation of a text buffer
 */
const gunichar gtk_text_unknown_char = 0xFFFD;
const gchar gtk_text_unknown_char_utf8[] = { 0xEF, 0xBF, 0xBD, '\0' };

static inline gboolean
inline_byte_begins_utf8_char(const gchar *byte)
{
  return ((*byte & 0xC0) != 0x80);
}

gboolean
gtk_text_byte_begins_utf8_char(const gchar *byte)
{
  return inline_byte_begins_utf8_char(byte);
}

