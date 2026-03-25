#include "gtksvgutilsprivate.h"
#include "gtksvgprivate.h"
#include "gtksvgnumberprivate.h"

/* Break str into tokens that are separated
 * by whitespace and the given separator.
 * If sep contains just a non-space byte,
 * the separator is mandatory. If it contains
 * a space as well, the separator is optional.
 * If a mandatory separator is missing, NULL
 * is returned.
 */
char **
strsplit_set (const char *str,
              const char *sep)
{
  const char *p, *p0;
  GPtrArray *array = g_ptr_array_new ();

  p = str;
  while (*p)
    {
      while (*p == ' ')
        p++;

      if (!*p)
        break;

      if (array->len > 0)
        {
          if (strchr (sep, *p))
            {
              p++;

              if (!*p)
                break;

              while (*p == ' ')
                p++;

              if (!*p)
                break;
            }
          else if (!strchr (sep, ' '))
            {
              g_ptr_array_free (array, TRUE);
              return g_new0 (char *, 1);
            }
        }

      p0 = p;
      while (!strchr (sep, *p) && *p != ' ')
        p++;

      g_ptr_array_add (array, g_strndup (p0, p - p0));
    }

  g_ptr_array_add (array, NULL);

  return (char **) g_ptr_array_free (array, FALSE);
}

static gboolean
parse_numeric (const char   *value,
               double        min,
               double        max,
               unsigned int  flags,
               double       *f,
               SvgUnit      *unit)
{
  char *endp = NULL;

  *f = g_ascii_strtod (value, &endp);

  if (endp == value)
    return FALSE;

  if (endp && *endp != '\0')
    {
      unsigned int i;

      if (*endp == '%')
        {
          *unit = SVG_UNIT_PERCENTAGE;
          return (flags & PERCENTAGE) != 0;
        }

      for (i = 0; i <= LAST_ANGLE_UNIT; i++)
        {
          if (strcmp (endp, svg_unit_name (i)) == 0)
            {
              if ((flags & LENGTH) != 0 &&
                  FIRST_LENGTH_UNIT <= i && i <= LAST_LENGTH_UNIT)
                {
                  *unit = i;
                  break;
                }
              if ((flags & ANGLE) != 0 &&
                  FIRST_ANGLE_UNIT <= i && i <= LAST_ANGLE_UNIT)
                {
                  *unit = i;
                  break;
                }
            }
        }
      if (i > LAST_ANGLE_UNIT)
        return FALSE;

      if (*f < min || *f > max)
        return FALSE;

      return TRUE;
    }
  else
    {
      if (flags & NUMBER)
        *unit = SVG_UNIT_NUMBER;
      else
        return FALSE;

      if (*f < min || *f > max)
        return FALSE;

      return TRUE;
    }
}

GArray *
parse_numbers2 (const char *value,
                const char *sep,
                double      min,
                double      max)
{
  GArray *array;
  GStrv strv;

  strv = strsplit_set (value, sep);

  array = g_array_new (FALSE, FALSE, sizeof (double));

  for (unsigned int i = 0; strv[i]; i++)
    {
      char *s = g_strstrip (strv[i]);
      double v;
      SvgUnit unit;

      if (*s == '\0' && strv[i] == NULL)
        break;

      if (!parse_numeric (s, min, max, NUMBER, &v, &unit))
        {
          g_array_unref (array);
          g_strfreev (strv);
          return NULL;
        }

      g_array_append_val (array, v);
    }

  g_strfreev (strv);
  return array;
}

gboolean
parse_numbers (const char   *value,
               const char   *sep,
               double        min,
               double        max,
               double       *values,
               unsigned int  length,
               unsigned int *n_values)
{
  GStrv strv;

  strv = strsplit_set (value, sep);

  *n_values = g_strv_length (strv);

  for (unsigned int i = 0; strv[i]; i++)
    {
      char *s = g_strstrip (strv[i]);
      SvgUnit unit;

      if (*s == '\0' && strv[i] == NULL)
        {
          *n_values -= 1;
          break;
        }

      if (i == length)
        {
          g_strfreev (strv);
          return FALSE;
        }

      if (!parse_numeric (s, min, max, NUMBER, &values[i], &unit))
        {
          g_strfreev (strv);
          return FALSE;
        }
    }

  g_strfreev (strv);
  return TRUE;
}

gboolean
parse_number (const char *value,
              double      min,
              double      max,
              double     *f)
{
  SvgUnit unit;

  return parse_numeric (value, min, max, NUMBER, f, &unit);
}

static void
skip_whitespace (const char **p)
{
  while (g_ascii_isspace (**p))
    (*p)++;
}

gboolean
match_str_len (const char *value,
               const char *str,
               size_t      len)
{
  const char *p = value;

  skip_whitespace (&p);

  if (strncmp (p, str, len) != 0)
    return FALSE;

  p += len;
  skip_whitespace (&p);

  return *p == '\0';
}

gboolean
parse_enum (const char    *value,
            const char   **values,
            size_t         n_values,
            unsigned int  *result)
{
  for (unsigned int i = 0; i < n_values; i++)
    {
      if (values[i] && match_str (value, values[i]))
        {
          *result = i;
          return TRUE;
        }
    }
  return FALSE;
}
