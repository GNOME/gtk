/*
 * Copyright © 2001, 2007 Red Hat, Inc.
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
#ifndef XSETTINGS_CLIENT_H
#define XSETTINGS_CLIENT_H

#include <gdk/gdkscreen.h>
#include <X11/Xlib.h>

/* Renames for GDK inclusion */

#define xsettings_client_destroy         _gdk_x11_xsettings_client_destroy
#define xsettings_client_get_setting     _gdk_x11_xsettings_client_get_setting
#define xsettings_client_new             _gdk_x11_xsettings_client_new
#define xsettings_client_process_event   _gdk_x11_xsettings_client_process_event
#define xsettings_setting_equal          _gdk_x11_xsettings_setting_equal
#define xsettings_setting_free           _gdk_x11_xsettings_setting_free

typedef struct _XSettingsColor   XSettingsColor;
typedef struct _XSettingsSetting XSettingsSetting;
typedef struct _XSettingsClient XSettingsClient;

/* Types of settings possible. Enum values correspond to
 * protocol values.
 */
typedef enum 
{
  XSETTINGS_TYPE_INT     = 0,
  XSETTINGS_TYPE_STRING  = 1,
  XSETTINGS_TYPE_COLOR   = 2
} XSettingsType;

typedef enum 
{
  XSETTINGS_ACTION_NEW,
  XSETTINGS_ACTION_CHANGED,
  XSETTINGS_ACTION_DELETED
} XSettingsAction;

struct _XSettingsColor
{
  unsigned short red, green, blue, alpha;
};

struct _XSettingsSetting
{
  XSettingsType type;
  
  union {
    int v_int;
    char *v_string;
    XSettingsColor v_color;
  } data;
};

typedef void (*XSettingsNotifyFunc) (const char       *name,
				     XSettingsAction   action,
				     XSettingsSetting *setting,
				     void             *cb_data);
typedef Bool (*XSettingsWatchFunc)  (Window            window,
				     Bool              is_start,
				     long              mask,
				     void             *cb_data);

void              xsettings_setting_free          (XSettingsSetting    *setting);
int               xsettings_setting_equal         (XSettingsSetting    *setting_a,
					           XSettingsSetting    *setting_b);

XSettingsClient *xsettings_client_new             (GdkScreen           *screen,
						   XSettingsNotifyFunc  notify,
						   XSettingsWatchFunc   watch,
						   void                *cb_data);
void             xsettings_client_destroy         (XSettingsClient     *client);
Bool             xsettings_client_process_event   (XSettingsClient     *client,
						   XEvent              *xev);
const XSettingsSetting *
                 xsettings_client_get_setting     (XSettingsClient     *client,
						   const char          *name);

#endif /* XSETTINGS_CLIENT_H */
