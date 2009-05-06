/* GTK - The GIMP Toolkit
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_SIZE_H__
#define __GTK_SIZE_H__

#include <gdk/gdk.h>
#include <gtk/gtkstyle.h>

G_BEGIN_DECLS

/**
 * GtkSize:
 *
 * A data type for storing both a size and a unit.
 *
 * Note that the binary representation of a #GtkUnit may vary
 * depending on whether resolution independent rendering is enabled
 * (see gtk_enable_resolution_independence()). As such, a #GtkSize
 * cannot e.g. be stored on disk or passed to other processes.
 *
 * Since: RIMERGE
 */
typedef gint GtkSize;

/**
 * GtkUSize:
 *
 * Like #GtkSize but for unsigned sizes.
 *
 * Since: RIMERGE
 */
typedef guint GtkUSize;

typedef struct _GtkParamSpecSize GtkParamSpecSize;
struct _GtkParamSpecSize
{
  GParamSpecInt parent_instance;
};

typedef struct _GtkParamSpecUSize GtkParamSpecUSize;
struct _GtkParamSpecUSize
{
  GParamSpecUInt parent_instance;
};

/**
 * GtkSizeUnit:
 * @GTK_SIZE_UNIT_PIXEL: the size is measure in pixels
 * @GTK_SIZE_UNIT_EM: the size is measured in em
 * @GTK_SIZE_UNIT_MM: the size is measured in millimeters
 *
 * The unit used to interpret the value stored in #GtkSize or
 * #GtkUSize. Use gtk_size_get_unit().
 *
 * Since: RIMERGE
 */
typedef enum
{
  GTK_SIZE_UNIT_PIXEL   = 0,
  GTK_SIZE_UNIT_EM      = 1,
  GTK_SIZE_UNIT_MM      = 2,
} GtkSizeUnit;

/**
 * GTK_SIZE_MAXPIXEL:
 *
 * The largest pixel size allowed when using #GtkSize. When using
 * units instead of pixel sizes, use this constant instead of G_MAXINT
 * when needing to specify a huge default for a bounded value.
 *
 * Since: RIMERGE
 */
#define GTK_SIZE_MAXPIXEL ((1<<28) - 1)

/**
 * GTK_SIZE_MINPIXEL:
 *
 * The smallest pixel size allowed when using #GtkSize. When using
 * units instead of pixel sizes, use this constant instead of G_MININT
 * when needing to specify a small default for a bounded value.
 *
 * Since: RIMERGE
 */
#define GTK_SIZE_MINPIXEL (-GTK_SIZE_MAXPIXEL)

GtkSize     gtk_size_em              (gdouble      em);
GtkSize     gtk_size_mm              (gdouble      mm);
gdouble     gtk_size_get_em          (GtkSize      size);
gdouble     gtk_size_get_mm          (GtkSize      size);
GtkSizeUnit gtk_size_get_unit        (GtkSize      size);

gint        gtk_size_to_pixel        (GdkScreen   *screen,
                                      gint         monitor_num,
                                      GtkSize      size) G_GNUC_WARN_UNUSED_RESULT;
gdouble     gtk_size_to_pixel_double (GdkScreen   *screen,
                                      gint         monitor_num,
                                      GtkSize      size) G_GNUC_WARN_UNUSED_RESULT;
gchar      *gtk_size_to_string       (GtkSize      size);

/**
 * GtkSizeError:
 * @GTK_SIZE_ERROR_INVALID_VALUE: an invalid value was passed
 *
 * Error codes returned by size/unit parsing.
 *
 * Since: RIMERGE
 */
typedef enum
{
  GTK_SIZE_ERROR_INVALID_VALUE,
} GtkSizeError;

/* for now, use functions so apps can "opt in" to using resolution
 * independent units - when this is mandatory (e.g. gtk 3.x) this
 * can be changed into macros
 */
void gtk_enable_resolution_independence (void);

/**
 * GTK_SIZE_ONE_TWELFTH_EM:
 * @value: A value in 1/12th of an em
 *
 * Convenience macro returning @value / 12 em's in a #GtkSize. See
 * gtk_unit_em() for details.
 *
 * Since: RIMERGE
 */
#define GTK_SIZE_ONE_TWELFTH_EM(value) gtk_size_em((value)/12.0)

#define GTK_IS_PARAM_SPEC_SIZE(pspec) (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), GTK_TYPE_PARAM_SIZE))
#define GTK_PARAM_SPEC_SIZE(pspec)    (G_TYPE_CHECK_INSTANCE_CAST ((pspec), GTK_TYPE_PARAM_SIZE, GtkParamSpecSize))
#define GTK_TYPE_PARAM_SIZE           (gtk_param_size_get_type())

GType gtk_param_size_get_type (void) G_GNUC_CONST;

GParamSpec *gtk_param_spec_size (const gchar  *name,
                                 const gchar  *nick,
                                 const gchar  *blurb,
                                 GtkSize       minimum,
                                 GtkSize       maximum,
                                 GtkSize       default_value,
                                 GParamFlags   flags);
void gtk_value_set_size         (GValue       *value,
                                 GtkSize       v_size,
                                 gpointer      widget);
GtkSize gtk_value_get_size      (const GValue *value);

void gtk_value_size_skip_conversion (GValue *value);

#define GTK_IS_PARAM_SPEC_USIZE(pspec) (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), GTK_TYPE_PARAM_USIZE))
#define GTK_PARAM_SPEC_USIZE(pspec)    (G_TYPE_CHECK_INSTANCE_CAST ((pspec), GTK_TYPE_PARAM_USIZE, GtkParamSpecUSize))
#define GTK_TYPE_PARAM_USIZE           (gtk_param_usize_get_type())

GType gtk_param_usize_get_type (void) G_GNUC_CONST;

GParamSpec *gtk_param_spec_usize (const gchar  *name,
                                  const gchar  *nick,
                                  const gchar  *blurb,
                                  GtkUSize      minimum,
                                  GtkUSize      maximum,
                                  GtkUSize      default_value,
                                  GParamFlags   flags);
void gtk_value_set_usize         (GValue       *value,
                                  GtkUSize      v_size,
                                  gpointer      widget);
GtkUSize gtk_value_get_usize     (const GValue *value);

void gtk_value_usize_skip_conversion (GValue *value);

G_END_DECLS

#endif /* __GTK_SIZE_H__ */
