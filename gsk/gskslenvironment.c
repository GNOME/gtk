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

#include "gskslenvironmentprivate.h"

#include "gskslnativefunctionprivate.h"
#include "gskslscopeprivate.h"

#include <string.h>

struct _GskSlEnvironment
{
  int ref_count;

  GskSlShaderStage stage;
  GskSlProfile profile;
  guint version;
};

GskSlEnvironment *
gsk_sl_environment_new (GskSlShaderStage stage,
                        GskSlProfile     profile,
                        guint            version)
{
  GskSlEnvironment *environment;
  
  environment = g_slice_new0 (GskSlEnvironment);
  environment->ref_count = 1;

  environment->stage = stage;
  environment->profile = profile;
  environment->version = version;

  return environment;
}

GskSlEnvironment *
gsk_sl_environment_new_similar (GskSlEnvironment  *environment,
                                GskSlProfile       profile,
                                guint              version,
                                GError           **error)
{
  return gsk_sl_environment_new (environment->stage,
                                 profile == GSK_SL_PROFILE_NONE ? environment->profile : profile,
                                 version);
}

GskSlEnvironment *
gsk_sl_environment_ref (GskSlEnvironment *environment)
{
  g_return_val_if_fail (environment != NULL, NULL);

  environment->ref_count += 1;

  return environment;
}

void
gsk_sl_environment_unref (GskSlEnvironment *environment)
{
  if (environment == NULL)
    return;

  environment->ref_count -= 1;
  if (environment->ref_count > 0)
    return;

  g_slice_free (GskSlEnvironment, environment);
}

GskSlShaderStage
gsk_sl_environment_get_stage (GskSlEnvironment *environment)
{
  return environment->stage;
}

GskSlProfile
gsk_sl_environment_get_profile (GskSlEnvironment *environment)
{
  return environment->profile;
}

guint
gsk_sl_environment_get_version (GskSlEnvironment *environment)
{
  return environment->version;
}

GskSlScope *
gsk_sl_environment_create_scope (GskSlEnvironment *environment)
{
  GskSlScope *scope;

  scope = gsk_sl_scope_new (NULL, NULL);

  gsk_sl_native_functions_add (scope, environment);

  return scope;
}

