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



#include <iconv.h>
#include <errno.h>
#include <string.h>

gchar*
g_convert (const gchar *str,
           gint         len,
           const gchar *to_codeset,
           const gchar *from_codeset,
           gint        *bytes_converted)
{
  gchar *dest;
  gchar *outp;
  const gchar *p;
  size_t inbytes_remaining;
  size_t outbytes_remaining;
  size_t err;
  iconv_t cd;
  size_t outbuf_size;
  
  g_return_val_if_fail (str != NULL, NULL);
  g_return_val_if_fail (to_codeset != NULL, NULL);
  g_return_val_if_fail (from_codeset != NULL, NULL);
     
  cd = iconv_open (to_codeset, from_codeset);

  if (cd == (iconv_t) -1)
    {
      /* Something went wrong.  */
      if (errno == EINVAL)
        g_warning ("Conversion from character set `%s' to `%s' is not supported",
                   from_codeset, to_codeset);
      else
        g_warning ("Failed to convert character set `%s' to `%s': %s",
                   from_codeset, to_codeset, strerror (errno));

      if (bytes_converted)
        *bytes_converted = 0;
      
      return NULL;
    }

  if (len < 0)
    len = strlen (str);

  p = str;
  inbytes_remaining = len;
  outbuf_size = len + 1; /* + 1 for nul in case len == 1 */
  outbytes_remaining = outbuf_size - 1; /* -1 for nul */
  outp = dest = g_malloc (outbuf_size);

 again:
  
  err = iconv (cd, &p, &inbytes_remaining, &outp, &outbytes_remaining);

  if (err == (size_t) -1)
    {
      if (errno == E2BIG)
        {
          size_t used = outp - dest;
          outbuf_size *= 2;
          dest = g_realloc (dest, outbuf_size);

          outp = dest + used;
          outbytes_remaining = outbuf_size - used - 1; /* -1 for nul */

          goto again;
        }
      else
        g_warning ("iconv() failed: %s", strerror (errno));
    }

  *outp = '\0';
  
  if (iconv_close (cd) != 0)
    g_warning ("Failed to close iconv() conversion descriptor: %s",
               strerror (errno));

  if (bytes_converted)
    *bytes_converted = p - str;

  if (p == str)
    {
      g_free (dest);
      return NULL;
    }
  else
    return dest;
}
