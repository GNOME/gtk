/*
 * Copyright © 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright © 1998-2002 Tor Lillqvist
 * Copyright © 2005-2008 Imendio AB
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gdkmacosdisplay-private.h"
#include "gdkmacosutils-private.h"

gboolean
_gdk_macos_display_get_setting (GdkMacosDisplay *self,
                                const gchar     *setting,
                                GValue          *value)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  gboolean ret = FALSE;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), FALSE);
  g_return_val_if_fail (setting != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);


  if (FALSE) {}
  else if (strcmp (setting, "gtk-double-click-time") == 0)
    {
      NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
      float t = [defaults floatForKey:@"com.apple.mouse.doubleClickThreshold"];

      /* No user setting, use the default in OS X. */
      if (t == 0.0)
        t = 0.5;

      g_value_set_int (value, t * 1000);

      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-font-name") == 0)
    {
      NSString *name;
      char *str;
      gint size;

      name = [[NSFont systemFontOfSize:0] familyName];
      size = (gint)[[NSFont userFontOfSize:0] pointSize];

      /* Let's try to use the "views" font size (12pt) by default. This is
       * used for lists/text/other "content" which is the largest parts of
       * apps, using the "regular control" size (13pt) looks a bit out of
       * place. We might have to tweak this.
       */

      /* The size has to be hardcoded as there doesn't seem to be a way to
       * get the views font size programmatically.
       */
      str = g_strdup_printf ("%s %d", [name UTF8String], size);
      g_value_take_string (value, g_steal_pointer (&str));

      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-primary-button-warps-slider") == 0)
    {
      BOOL b = [[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollerPagingBehavior"];
      /* If the Apple property is YES, it means "warp" */
      g_value_set_boolean (value, b == YES);
      ret = TRUE;
    }
  else if (strcmp (setting, "gtk-shell-shows-desktop") == 0)
    {
      g_value_set_boolean (value, TRUE);
      ret = TRUE;
    }

  GDK_END_MACOS_ALLOC_POOL;

  return ret;
}
