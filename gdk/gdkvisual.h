/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_VISUAL_H__
#define __GDK_VISUAL_H__

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_VISUAL              (gdk_visual_get_type ())
#define GDK_VISUAL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_VISUAL, GdkVisual))
#define GDK_VISUAL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_VISUAL, GdkVisualClass))
#define GDK_IS_VISUAL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_VISUAL))
#define GDK_IS_VISUAL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_VISUAL))
#define GDK_VISUAL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_VISUAL, GdkVisualClass))

typedef struct _GdkVisualPrivate  GdkVisualPrivate;
typedef struct _GdkVisualClass    GdkVisualClass;

/**
 * GdkVisualType:
 * @GDK_VISUAL_STATIC_GRAY: Each pixel value indexes a grayscale value
 *     directly.
 * @GDK_VISUAL_GRAYSCALE: Each pixel is an index into a color map that
 *     maps pixel values into grayscale values. The color map can be
 *     changed by an application.
 * @GDK_VISUAL_STATIC_COLOR: Each pixel value is an index into a predefined,
 *     unmodifiable color map that maps pixel values into RGB values.
 * @GDK_VISUAL_PSEUDO_COLOR: Each pixel is an index into a color map that
 *     maps pixel values into rgb values. The color map can be changed by
 *     an application.
 * @GDK_VISUAL_TRUE_COLOR: Each pixel value directly contains red, green,
 *     and blue components. Use gdk_visual_get_red_pixel_details(), etc,
 *     to obtain information about how the components are assembled into
 *     a pixel value.
 * @GDK_VISUAL_DIRECT_COLOR: Each pixel value contains red, green, and blue
 *     components as for %GDK_VISUAL_TRUE_COLOR, but the components are
 *     mapped via a color table into the final output table instead of
 *     being converted directly.
 *
 * A set of values that describe the manner in which the pixel values
 * for a visual are converted into RGB values for display.
 */
typedef enum
{
  GDK_VISUAL_STATIC_GRAY,
  GDK_VISUAL_GRAYSCALE,
  GDK_VISUAL_STATIC_COLOR,
  GDK_VISUAL_PSEUDO_COLOR,
  GDK_VISUAL_TRUE_COLOR,
  GDK_VISUAL_DIRECT_COLOR
} GdkVisualType;

/**
 * GdkVisual:
 *
 * The #GdkVisual structure contains information about
 * a particular visual.
 *
 * <example id="rgbmask">
 * <title>Constructing a pixel value from components</title>
 * <programlisting>
 * guint
 * pixel_from_rgb (GdkVisual *visual,
 *                 guchar r, guchar b, guchar g)
 * {
 *   return ((r >> (16 - visual->red_prec))   << visual->red_shift) |
 *          ((g >> (16 - visual->green_prec)) << visual->green_shift) |
 *          ((r >> (16 - visual->blue_prec))  << visual->blue_shift);
 * }
 * </programlisting>
 * </example>
 */
struct _GdkVisual
{
  /*< private >*/
  GObject parent_instance;

  GdkVisualType GSEAL (type);      /* Type of visual this is (PseudoColor, TrueColor, etc) */
  gint GSEAL (depth);              /* Bit depth of this visual */
  GdkByteOrder GSEAL (byte_order);
  gint GSEAL (colormap_size);      /* Size of a colormap for this visual */
  gint GSEAL (bits_per_rgb);       /* Number of significant bits per red, green and blue. */

  /* The red, green and blue masks, shifts and precisions refer
   * to value needed to calculate pixel values in TrueColor and DirectColor
   * visuals. The "mask" is the significant bits within the pixel. The
   * "shift" is the number of bits left we must shift a primary for it
   * to be in position (according to the "mask"). "prec" refers to how
   * much precision the pixel value contains for a particular primary.
   */
  guint32 GSEAL (red_mask);
  gint GSEAL (red_shift);
  gint GSEAL (red_prec);

  guint32 GSEAL (green_mask);
  gint GSEAL (green_shift);
  gint GSEAL (green_prec);

  guint32 GSEAL (blue_mask);
  gint GSEAL (blue_shift);
  gint GSEAL (blue_prec);

  GdkVisualPrivate *priv;
};

GType         gdk_visual_get_type            (void) G_GNUC_CONST;

#ifndef GDK_MULTIHEAD_SAFE
gint	      gdk_visual_get_best_depth	     (void);
GdkVisualType gdk_visual_get_best_type	     (void);
GdkVisual*    gdk_visual_get_system	     (void);
GdkVisual*    gdk_visual_get_best	     (void);
GdkVisual*    gdk_visual_get_best_with_depth (gint	     depth);
GdkVisual*    gdk_visual_get_best_with_type  (GdkVisualType  visual_type);
GdkVisual*    gdk_visual_get_best_with_both  (gint	     depth,
					      GdkVisualType  visual_type);

void gdk_query_depths	    (gint	    **depths,
			     gint	     *count);
void gdk_query_visual_types (GdkVisualType  **visual_types,
			     gint	     *count);

GList* gdk_list_visuals (void);
#endif

GdkScreen *gdk_visual_get_screen (GdkVisual *visual);

GdkVisualType gdk_visual_get_visual_type         (GdkVisual *visual);
gint          gdk_visual_get_depth               (GdkVisual *visual);
GdkByteOrder  gdk_visual_get_byte_order          (GdkVisual *visual);
gint          gdk_visual_get_colormap_size       (GdkVisual *visual);
gint          gdk_visual_get_bits_per_rgb        (GdkVisual *visual);
void          gdk_visual_get_red_pixel_details   (GdkVisual *visual,
                                                  guint32   *mask,
                                                  gint      *shift,
                                                  gint      *precision);
void          gdk_visual_get_green_pixel_details (GdkVisual *visual,
                                                  guint32   *mask,
                                                  gint      *shift,
                                                  gint      *precision);
void          gdk_visual_get_blue_pixel_details  (GdkVisual *visual,
                                                  guint32   *mask,
                                                  gint      *shift,
                                                  gint      *precision);

G_END_DECLS

#endif /* __GDK_VISUAL_H__ */
