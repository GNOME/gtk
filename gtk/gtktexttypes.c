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

guint
gtk_text_utf_to_latin1_char(const gchar *p, guchar *l1_ch)
{
  guint charlen;
  gunichar ch;

  g_assert(inline_byte_begins_utf8_char(p));
  
  charlen = g_utf8_next_char (p) - p;
  ch = g_utf8_get_char (p);
  
  g_assert(ch != '\0');
  
  if (ch > 0xff)
    *l1_ch = '?';
  else
    *l1_ch = ch;

  return charlen;
}

gchar*
gtk_text_utf_to_latin1(const gchar *p, gint len)
{
  GString *str;
  guint i;
  gchar *retval;
  
  str = g_string_new("");

  i = 0;
  while (i < len)
    {
      guchar ch;
      guint charlen;

      charlen = gtk_text_utf_to_latin1_char(p+i, &ch);
      
      g_string_append_c(str, ch);
        
      i += charlen;
    }

  retval = str->str;
  g_string_free(str, FALSE);
  
  return retval;
}

gchar*
gtk_text_latin1_to_utf (const gchar *latin1, gint len)
{
  gint i;
  GString *retval;
  gchar *str;
  
  retval = g_string_new("");

  i = 0;
  while (i < len)
    {
      gchar utf[7];
      gint count;

      count = g_unichar_to_utf8 ((guchar)latin1[i], utf);
      
      utf[count] = '\0';
      
      g_string_append(retval, utf);

      ++i;
    }

  str = retval->str;
  g_string_free(retval, FALSE);
  return str;
}



