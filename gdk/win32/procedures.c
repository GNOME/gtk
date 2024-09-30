/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2024 the GTK team
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

#include "apisets.h"
#include "procedures.h"
#include "gdkmain-win32.h"
#include "gdkprivate-win32.h"

# define PROCEDURE(name, api_set_id, module_id, os_version) \
    name##_t ptr##name;
  PROCEDURES
# undef PROCEDURE

struct procedure {
  void **pp;
  char *name;
  int api_set_id;
  int module_id;
  OSVersion os_version;
};

static struct procedure
procedures[] = {
# define PROCEDURE(name, api_set_id, module_id, os_version) \
    {(void**)&ptr##name, #name, api_set_id, module_id, os_version},
  PROCEDURES
# undef PROCEDURE
};

static void *
load_common (HMODULE module_handle,
             const char *type,
             const wchar_t *m_name,
             const char *name,
             OSVersion os_version)
{
  void *proc = GetProcAddress (module_handle, name);
  if (proc == NULL)
    {
      DWORD code = GetLastError ();

      if (code != ERROR_PROC_NOT_FOUND)
        WIN32_API_FAILED ("GetProcAddress");
      else if (gdk_win32_get_os_version () >= os_version)
        g_warning ("Could not find procedure %s in %s %S",
                   name, type, m_name);
    }

  return proc;
}

static bool
load_by_api_set (struct procedure *procedure)
{
  struct api_set *api_set = gdk_win32_get_api_set (procedure->api_set_id);

  if (api_set->module_handle)
    *procedure->pp = load_common (api_set->module_handle,
                                  "api set",
                                  api_set->name_wide,
                                  procedure->name,
                                  procedure->os_version);

  return *procedure->pp != NULL;
}

static bool
load_by_module (struct procedure *procedure)
{
  struct module *module = gdk_win32_get_module (procedure->module_id);

  if (module->module_handle)
    *procedure->pp = load_common (module->module_handle,
                                  "module",
                                  module->name,
                                  procedure->name,
                                  procedure->os_version);

  return *procedure->pp != NULL;
}

#if 0

static void
load_by_package (struct procedure *procedure)
{
  struct package *package = gdk_win32_get_package (procedure->package_id);

  if (package->module_handle)
    *procedure->pp = load_common (package->module_handle,
                                  "package",
                                  package->name,
                                  procedure->name,
                                  procedure->os_version);
}

#endif

static void
procedures_load_internal (void *)
{
  for (size_t i = 0; i < G_N_ELEMENTS (procedures); i++)
    {
      struct procedure *procedure = &procedures[i];

      g_assert (*procedure->pp == NULL);

      load_by_api_set (procedure) ||
      load_by_module (procedure) ||
      load_by_package (procedure);
    }
}

void
gdk_win32_procedures_load (void)
{
  gdk_win32_invoke_callback (procedures_load_internal,
                             NULL,
                             gdk_win32_with_loader_error_mode,
                             gdk_win32_with_activation_context,
                             NULL);
}

void
gdk_win32_procedures_unload (void)
{
  for (size_t i = 0; i < G_N_ELEMENTS (procedures); i++)
    *procedures[i].pp = NULL;
}

