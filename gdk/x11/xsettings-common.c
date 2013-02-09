/*
 * Copyright © 2001 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Owen Taylor, Red Hat, Inc.
 */

#include "config.h"

#include "xsettings-common.h"

#include "string.h"
#include "stdlib.h"

#include <X11/Xlib.h>
#include <X11/Xmd.h>		/* For CARD32 */

int
xsettings_setting_equal (XSettingsSetting *setting_a,
			 XSettingsSetting *setting_b)
{
  if (setting_a->type != setting_b->type)
    return 0;

  if (strcmp (setting_a->name, setting_b->name) != 0)
    return 0;

  switch (setting_a->type)
    {
    case XSETTINGS_TYPE_INT:
      return setting_a->data.v_int == setting_b->data.v_int;
    case XSETTINGS_TYPE_COLOR:
      return (setting_a->data.v_color.red == setting_b->data.v_color.red &&
	      setting_a->data.v_color.green == setting_b->data.v_color.green &&
	      setting_a->data.v_color.blue == setting_b->data.v_color.blue &&
	      setting_a->data.v_color.alpha == setting_b->data.v_color.alpha);
    case XSETTINGS_TYPE_STRING:
      return strcmp (setting_a->data.v_string, setting_b->data.v_string) == 0;
    }

  return 0;
}

void
xsettings_setting_free (XSettingsSetting *setting)
{
  if (setting->type == XSETTINGS_TYPE_STRING)
    free (setting->data.v_string);

  if (setting->name)
    free (setting->name);
  
  free (setting);
}

