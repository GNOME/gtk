/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_DEVICE_H__
#define __GDK_DEVICE_H__

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE         (gdk_device_get_type ())
#define GDK_DEVICE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE, GdkDevice))
#define GDK_DEVICE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE, GdkDeviceClass))
#define GDK_IS_DEVICE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE))
#define GDK_IS_DEVICE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE))
#define GDK_DEVICE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE, GdkDeviceClass))

typedef struct _GdkDevice GdkDevice;
typedef struct _GdkDeviceClass GdkDeviceClass;

typedef struct _GdkDeviceKey GdkDeviceKey;
typedef struct _GdkDeviceAxis GdkDeviceAxis;
typedef struct _GdkTimeCoord GdkTimeCoord;

typedef enum
{
  GDK_EXTENSION_EVENTS_NONE,
  GDK_EXTENSION_EVENTS_ALL,
  GDK_EXTENSION_EVENTS_CURSOR
} GdkExtensionMode;

typedef enum
{
  GDK_SOURCE_MOUSE,
  GDK_SOURCE_PEN,
  GDK_SOURCE_ERASER,
  GDK_SOURCE_CURSOR,
  GDK_SOURCE_KEYBOARD
} GdkInputSource;

typedef enum
{
  GDK_MODE_DISABLED,
  GDK_MODE_SCREEN,
  GDK_MODE_WINDOW
} GdkInputMode;

typedef enum
{
  GDK_AXIS_IGNORE,
  GDK_AXIS_X,
  GDK_AXIS_Y,
  GDK_AXIS_PRESSURE,
  GDK_AXIS_XTILT,
  GDK_AXIS_YTILT,
  GDK_AXIS_WHEEL,
  GDK_AXIS_LAST
} GdkAxisUse;

struct _GdkDeviceKey
{
  guint keyval;
  GdkModifierType modifiers;
};

struct _GdkDeviceAxis
{
  GdkAxisUse use;
  gdouble    min;
  gdouble    max;
};

/* We don't allocate each coordinate this big, but we use it to
 * be ANSI compliant and avoid accessing past the defined limits.
 */
#define GDK_MAX_TIMECOORD_AXES 128

struct _GdkTimeCoord
{
  guint32 time;
  gdouble axes[GDK_MAX_TIMECOORD_AXES];
};

struct _GdkDevice
{
  GObject parent_instance;

  /* All fields are read-only */
  gchar *name;
  GdkInputSource source;
  GdkInputMode mode;
  gboolean has_cursor;	     /* TRUE if the X pointer follows device motion */

  gint num_axes;
  GdkDeviceAxis *axes;

  gint num_keys;
  GdkDeviceKey *keys;
};

struct _GdkDeviceClass
{
  GObjectClass parent_class;

  gboolean (* get_history) (GdkDevice      *device,
                            GdkWindow      *window,
                            guint32         start,
                            guint32         stop,
                            GdkTimeCoord ***events,
                            guint          *n_events);

  void (* get_state) (GdkDevice       *device,
                      GdkWindow       *window,
                      gdouble         *axes,
                      GdkModifierType *mask);

  gboolean (* get_axis) (GdkDevice  *device,
                         gdouble    *axes,
                         GdkAxisUse  use,
                         gdouble    *value);

  void (* set_window_cursor) (GdkDevice *device,
                              GdkWindow *window,
                              GdkCursor *cursor);
};

GType gdk_device_get_type (void) G_GNUC_CONST;

/* Functions to configure a device */
void           gdk_device_set_source    (GdkDevice      *device,
					 GdkInputSource  source);

gboolean       gdk_device_set_mode      (GdkDevice      *device,
					 GdkInputMode    mode);

void           gdk_device_set_key       (GdkDevice      *device,
					 guint           index_,
					 guint           keyval,
					 GdkModifierType modifiers);

void           gdk_device_set_axis_use  (GdkDevice         *device,
                                         guint              index_,
                                         GdkAxisUse         use);


void     gdk_device_get_state    (GdkDevice         *device,
				  GdkWindow         *window,
				  gdouble           *axes,
				  GdkModifierType   *mask);
gboolean gdk_device_get_history  (GdkDevice         *device,
				  GdkWindow         *window,
				  guint32            start,
				  guint32            stop,
				  GdkTimeCoord    ***events,
				  guint             *n_events);
void     gdk_device_free_history (GdkTimeCoord     **events,
				  gint               n_events);
gboolean gdk_device_get_axis     (GdkDevice         *device,
				  gdouble           *axes,
				  GdkAxisUse         use,
				  gdouble           *value);

G_END_DECLS

#endif /* __GDK_DEVICE_H__ */
