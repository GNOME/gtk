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

/*
 * Unicode stubs (these are wrappers to make libunicode match the Tcl/Tk
 * API, eventually should just use libunicode/Pango directly)
 */

#include <unicode.h>

#if 0
static void
trigger_efence(const gchar *str, gint len)
{
  gchar ch;
  gint i = 0;
  while (i < len)
    {
      ch = str[i];
      ((gchar*)str)[i] = ch;
      ++i;
    }
}
#else
#define trigger_efence(foo,bar)
#endif

const GtkTextUniChar gtk_text_unknown_char = 0xFFFD;
const gchar gtk_text_unknown_char_utf8[] = { 0xEF, 0xBF, 0xBD, '\0' };

gint
gtk_text_view_num_utf_chars(const gchar *str, gint len)
{
  trigger_efence(str, len);
  return unicode_strlen(str, len);
}

/* FIXME we need a version of this function with error handling, so we
   can screen incoming UTF8 for validity. */

gint
gtk_text_utf_to_unichar(const gchar *str, GtkTextUniChar *chPtr)
{
  unicode_char_t ch;
  gchar *end;

  end = unicode_get_utf8(str, &ch);

  if (end == NULL)
    g_error("Bad UTF8, need to add some error checking so this doesn't crash the program");

  *chPtr = ch;

  trigger_efence(str, end - str);
  
  return end - str;
}

gchar*
gtk_text_utf_prev(const gchar *str, const gchar *start)
{
  gchar *retval;

  trigger_efence(start, str - start);
  
  retval = unicode_previous_utf8(start, str);

  return retval;
}

static inline gboolean
inline_byte_begins_utf8_char(const gchar *byte)
{
  return ((*byte & 0xC0) != 0x80);
}

gboolean
gtk_text_byte_begins_utf8_char(const gchar *byte)
{
  trigger_efence(byte, 1);
  return inline_byte_begins_utf8_char(byte);
}

guint
gtk_text_utf_to_latin1_char(const gchar *p, guchar *l1_ch)
{
  guint charlen;
  GtkTextUniChar ch;

  g_assert(inline_byte_begins_utf8_char(p));
  
  charlen = gtk_text_utf_to_unichar(p, &ch);
  
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

  trigger_efence(p, len);
  
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

static int
gtk_text_view_unichar_to_utf(GtkTextUniChar c, char *outbuf)
{
  size_t len = 0;
  int first;
  int i;

  if (c < 0x80)
    {
      first = 0;
      len = 1;
    }
  else if (c < 0x800)
    {
      first = 0xc0;
      len = 2;
    }
  else if (c < 0x10000)
    {
      first = 0xe0;
      len = 3;
    }
   else if (c < 0x200000)
    {
      first = 0xf0;
      len = 4;
    }
  else if (c < 0x4000000)
    {
      first = 0xf8;
      len = 5;
    }
  else
    {
      first = 0xfc;
      len = 6;
    }

  for (i = len - 1; i > 0; --i)
    {
      outbuf[i] = (c & 0x3f) | 0x80;
      c >>= 6;
    }
  outbuf[0] = c | first;

  return len;
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

      count = gtk_text_view_unichar_to_utf((guchar)latin1[i], utf);
      
      utf[count] = '\0';
      
      g_string_append(retval, utf);

      ++i;
    }

  str = retval->str;
  g_string_free(retval, FALSE);
  return str;
}
