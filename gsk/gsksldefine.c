/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gsksldefineprivate.h"

typedef struct _GskSlDefineToken GskSlDefineToken;

struct _GskSlDefineToken {
  GskCodeLocation location;
  GskSlToken token;
};

struct _GskSlDefine {
  int ref_count;

  char *name;
  GFile *source_file;

  GArray *tokens;
};

GskSlDefine *
gsk_sl_define_new (const char *name,
                   GFile      *source_file)
{
  GskSlDefine *result;

  result = g_slice_new0 (GskSlDefine);

  result->ref_count = 1;
  result->name = g_strdup (name);
  if (source_file)
    result->source_file = g_object_ref (source_file);
  result->tokens = g_array_new (FALSE, FALSE, sizeof (GskSlDefineToken));

  return result;
}

GskSlDefine *
gsk_sl_define_ref (GskSlDefine *define)
{
  g_return_val_if_fail (define != NULL, NULL);

  define->ref_count += 1;

  return define;
}

void
gsk_sl_define_unref (GskSlDefine *define)
{
  if (define == NULL)
    return;

  define->ref_count -= 1;
  if (define->ref_count > 0)
    return;

  g_array_free (define->tokens, TRUE);

  if (define->source_file)
    g_object_unref (define->source_file);
  g_free (define->name);

  g_slice_free (GskSlDefine, define);
}

const char *
gsk_sl_define_get_name (GskSlDefine *define)
{
  return define->name;
}

GFile *
gsk_sl_define_get_source_file (GskSlDefine *define)
{
  return define->source_file;
}

guint
gsk_sl_define_get_n_tokens (GskSlDefine *define)
{
  return define->tokens->len;
}

void
gsk_sl_define_get_token (GskSlDefine     *define,
                         guint            i,
                         GskCodeLocation *location,
                         GskSlToken      *token)
{
  GskSlDefineToken *dt;
  
  dt = &g_array_index (define->tokens, GskSlDefineToken, i);

  if (location)
    *location = dt->location;
  if (token)
    gsk_sl_token_copy (token, &dt->token);
}

void
gsk_sl_define_add_token (GskSlDefine           *define,
                         const GskCodeLocation *location,
                         const GskSlToken      *token)
{
  GskSlDefineToken dt;

  dt.location = *location;
  gsk_sl_token_copy (&dt.token, token);

  g_array_append_val (define->tokens, dt);
}


