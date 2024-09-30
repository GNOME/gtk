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

#include "gdkprivate-win32.h"

static struct api_set
api_sets[] = {
# define API_SET(id, name, os_version) \
    {name, L"" name, NULL, FALSE, os_version},
  API_SETS
# undef API_SET
};

static struct module
modules[] = {
# define MODULE(id, name, folder, flags) \
    {L"" name, folder, flags, NULL, FALSE},
  MODULES
# undef MODULE
};

static void
load_api_set_internal (struct api_set *api_set)
{
  if (ptrIsApiSetImplemented)
    if (!ptrIsApiSetImplemented (api_set->name_narrow))
      return;

  /* Use LOAD_LIBRARY_SEARCH_SYSTEM32 for security (if IsApiSetImplemented
   * is not present)
   *
   * If we can't use IsApiSetImplemented, we should try loading the api set
   * directly. However, if the OS is too old and the api set is unknown,
   * LoadLibrary will look for a corresponding DLL file in the search paths.
   * Here we use LOAD_LIBRARY_SEARCH_SYSTEM32 to restrict search to a safe
   * folder. */
  const DWORD flags = LOAD_LIBRARY_SEARCH_SYSTEM32;

  api_set->module_handle = LoadLibraryEx (api_set->name_wide, NULL, flags);
  if (api_set->module_handle == NULL)
    {
      DWORD code = GetLastError ();
      if (code == ERROR_INVALID_PARAMETER)
        {
          /* LOAD_LIBRARY_SEARCH_SYSTEM32 is supported on Windows Vista/7
           * only with update KB2533623 installed. If we can't use that
           * flag, it's best to return early and rely on classic modules.
           */
          return;
        }
      else if (code == ERROR_MOD_NOT_FOUND)
        {
          if (strncmp (api_set->name_narrow, "api", strlen ("api")) == 0 &&
              api_set->os_version <= gdk_win32_get_os_version ())
            {
              g_message ("%s missing\n", api_set->name_narrow);
            }
        }
      else
        WIN32_API_FAILED ("LoadLibraryEx");
    }
}

static void
load_api_set (struct api_set *api_set)
{
  g_assert (api_set->module_handle == NULL);
  g_assert (api_set->checked == false);

  load_api_set_internal (api_set);

  api_set->checked = true;
}

/** gdk_win32_get_api_set:
 */
struct api_set *
gdk_win32_get_api_set (int api_set_id)
{
  if (api_set_id >= 0 && api_set_id < API_SET_COUNT)
    {
      struct api_set *api_set = &api_sets[api_set_id];

      if (!api_set->checked)
        load_api_set (api_set);

      return api_set;
    }

  return NULL;
}

static void
load_module_internal (struct module *module)
{
  if (module->flags & MODULE_FLAG_PACKAGED)
    {
      if (gdk_win32_check_app_packaged ())
        {
          g_assert (ptrLoadPackagedLibrary);

          module->module_handle = ptrLoadPackagedLibrary (module->name, 0);
          if (module->module_handle == NULL)
            {
              if (GetLastError () != ERROR_MOD_NOT_FOUND)
                WIN32_API_FAILED ("LoadPackagedLibrary");
            }

          return;
        }
      else
        {
          /* TODO
           *
           * https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/framework-packages/use-the-dynamic-dependency-api
           * https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/use-windows-app-sdk-run-time
           * https://github.com/microsoft/WindowsAppSDK/issues/89
           */
        }
    }

  switch (module->folder)
    {
      case FOLDER_APP:
        if (can_use_app_folder < 0)
          can_use_app_folder = check_can_use_app_folder ();

        if (can_use_app_folder)
          {
            module->module_handle = gdk_win32_load_library_from_app_folder (module->name);
            break;
          }

        G_GNUC_FALLTHROUGH;
      case FOLDER_SYSTEM32:
        module->module_handle = gdk_win32_load_library_from_system32 (module->name, false, true);
      break;
    }
}

static void
load_module (struct module *module)
{
  g_assert (module->module_handle == NULL);
  g_assert (module->checked == false);

  load_module_internal (module);

  module->checked = true;
}

/** gdk_win32_get_module:
 */
struct module *
gdk_win32_get_module (int module_id)
{
  if (module_id >= 0 && module_id < MODULE_COUNT)
    {
      struct module *module = &modules[module_id];

      if (!module->checked)
        load_module (module);

      return module;
    }

  return NULL;
}

// TODO:
// Check Dynamic-link library redirection
// Check .exe.manifest file alongside exe
// PSP_USEFUSIONCONTEXT
// Manifest and WinRT activatable classes?
// Check if GetProcAddress works in app container (do we really need a delay-load table for it to work?)
//   (can it really access the calling module?)

// Is it possible to generate a synthetic delay-load import table?


