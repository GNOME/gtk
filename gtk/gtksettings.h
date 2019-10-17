/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#ifndef __GTK_SETTINGS_H__
#define __GTK_SETTINGS_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtktypes.h>

G_BEGIN_DECLS


/* -- type macros --- */
#define GTK_TYPE_SETTINGS             (gtk_settings_get_type ())
#define GTK_SETTINGS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SETTINGS, GtkSettings))
#define GTK_IS_SETTINGS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SETTINGS))


/* --- typedefs --- */
typedef struct _GtkSettingsValue GtkSettingsValue;


/* --- structures --- */
struct _GtkSettings
{
  GObject parent_instance;
};

/**
 * GtkSettingsValue:
 * @origin: Origin should be something like “filename:linenumber” for
 *    rc files, or e.g. “XProperty” for other sources.
 * @value: Valid types are LONG, DOUBLE and STRING corresponding to
 *    the token parsed, or a GSTRING holding an unparsed statement
 */
struct _GtkSettingsValue
{
  /* origin should be something like "filename:linenumber" for rc files,
   * or e.g. "XProperty" for other sources
   */
  gchar *origin;

  /* valid types are LONG, DOUBLE and STRING corresponding to the token parsed,
   * or a GSTRING holding an unparsed statement
   */
  GValue value;
};


/* --- functions --- */
GDK_AVAILABLE_IN_ALL
GType           gtk_settings_get_type                (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkSettings*    gtk_settings_get_default             (void);
GDK_AVAILABLE_IN_ALL
GtkSettings*    gtk_settings_get_for_display         (GdkDisplay *display);

/* --- precoded parsing functions --- */
GDK_AVAILABLE_IN_ALL
gboolean gtk_rc_property_parse_color       (const GParamSpec *pspec,
                                            const GString    *gstring,
                                            GValue           *property_value);
GDK_AVAILABLE_IN_ALL
gboolean gtk_rc_property_parse_enum        (const GParamSpec *pspec,
                                            const GString    *gstring,
                                            GValue           *property_value);
GDK_AVAILABLE_IN_ALL
gboolean gtk_rc_property_parse_flags       (const GParamSpec *pspec,
                                            const GString    *gstring,
                                            GValue           *property_value);
GDK_AVAILABLE_IN_ALL
gboolean gtk_rc_property_parse_requisition (const GParamSpec *pspec,
                                            const GString    *gstring,
                                            GValue           *property_value);
GDK_AVAILABLE_IN_ALL
gboolean gtk_rc_property_parse_border      (const GParamSpec *pspec,
                                            const GString    *gstring,
                                            GValue           *property_value);

GDK_AVAILABLE_IN_ALL
void     gtk_settings_reset_property       (GtkSettings            *settings,
                                            const gchar            *name);

G_END_DECLS

#endif /* __GTK_SETTINGS_H__ */
