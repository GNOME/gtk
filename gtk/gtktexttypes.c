#include <config.h>
#include "gtktexttypes.h"
#include "gtkalias.h"

/* These are used to represent embedded non-character objects
 * if you return a string representation of a text buffer
 */
const gchar gtk_text_unknown_char_utf8[] = { '\xEF', '\xBF', '\xBC', '\0' };

static inline gboolean
inline_byte_begins_utf8_char (const gchar *byte)
{
  return ((*byte & 0xC0) != 0x80);
}

gboolean
gtk_text_byte_begins_utf8_char (const gchar *byte)
{
  return inline_byte_begins_utf8_char (byte);
}

#define __GTK_TEXT_TYPES_C__
#include "gtkaliasdef.c"
