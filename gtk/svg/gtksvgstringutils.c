#include "gtksvgstringutilsprivate.h"

void
string_append_double (GString    *string,
                      const char *prefix,
                      double      value)
{
  char buf[64];

  g_ascii_formatd (buf, sizeof (buf), "%g", value);
  g_string_append (string, prefix);
  g_string_append (string, buf);
}

void
string_append_point (GString                *string,
                     const char             *prefix,
                     const graphene_point_t *point)
{
  string_append_double (string, prefix, point->x);
  string_append_double (string, " ", point->y);
}

void
string_append_base64 (GString *string,
                      GBytes  *bytes)
{
  const unsigned char *data;
  size_t len;
  size_t max;
  size_t before;
  char *out;
  int state = 0, outlen;
  int save = 0;

  data = g_bytes_get_data (bytes, &len);

  /* We can use a smaller limit here, since we know the saved state is 0,
     +1 is needed for trailing \0, also check for unlikely integer overflow */
  g_return_if_fail (len < ((G_MAXSIZE - 1 - string->len) / 4 - 1) * 3);

  /* The glib docs say:
   *
   * The output buffer must be large enough to fit all the data that will
   * be written to it. Due to the way base64 encodes you will need
   * at least: (@len / 3 + 1) * 4 + 4 bytes (+ 4 may be needed in case of
   * non-zero state). If you enable line-breaking you will need at least:
   * ((@len / 3 + 1) * 4 + 4) / 76 + 1 bytes of extra space.
   */
  max = (len / 3 + 1) * 4;
  max += ((len / 3 + 1) * 4 + 4) / 76 + 1;
  /* and the null byte */
  max += 1;

  before = string->len;
  g_string_set_size (string, string->len + max);
  out = string->str + before;

  outlen = g_base64_encode_step (data, len, TRUE, out, &state, &save);
  outlen += g_base64_encode_close (TRUE, out + outlen, &state, &save);
  out[outlen] = '\0';
  string->len = before + outlen;
}
