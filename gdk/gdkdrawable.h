#ifndef __GDK_DRAWABLE_H__
#define __GDK_DRAWABLE_H__

#include <gdk/gdktypes.h>
#include <gdk/gdkgc.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GdkDrawableClass GdkDrawableClass;

#define GDK_TYPE_DRAWABLE              (gdk_drawable_get_type ())
#define GDK_DRAWABLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DRAWABLE, GdkDrawable))
#define GDK_DRAWABLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DRAWABLE, GdkDrawableClass))
#define GDK_IS_DRAWABLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DRAWABLE))
#define GDK_IS_DRAWABLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DRAWABLE))
#define GDK_DRAWABLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DRAWABLE, GdkDrawableClass))

struct _GdkDrawable
{
  GObject parent_instance;
};
 
struct _GdkDrawableClass 
{
  GObjectClass parent_class;
  
  GdkGC *(*create_gc)    (GdkDrawable    *drawable,
		          GdkGCValues    *values,
		          GdkGCValuesMask mask);
  void (*draw_rectangle) (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  gint		filled,
			  gint		x,
			  gint		y,
			  gint		width,
			  gint		height);
  void (*draw_arc)       (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  gint		filled,
			  gint		x,
			  gint		y,
			  gint		width,
			  gint		height,
			  gint		angle1,
			  gint		angle2);
  void (*draw_polygon)   (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  gint		filled,
			  GdkPoint     *points,
			  gint		npoints);
  void (*draw_text)      (GdkDrawable  *drawable,
			  GdkFont      *font,
			  GdkGC	       *gc,
			  gint		x,
			  gint		y,
			  const gchar  *text,
			  gint		text_length);
  void (*draw_text_wc)   (GdkDrawable	 *drawable,
			  GdkFont	 *font,
			  GdkGC		 *gc,
			  gint		  x,
			  gint		  y,
			  const GdkWChar *text,
			  gint		  text_length);
  void (*draw_drawable)  (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  GdkDrawable  *src,
			  gint		xsrc,
			  gint		ysrc,
			  gint		xdest,
			  gint		ydest,
			  gint		width,
			  gint		height);
  void (*draw_points)	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  GdkPoint     *points,
			  gint		npoints);
  void (*draw_segments)	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  GdkSegment   *segs,
			  gint		nsegs);
 void (*draw_lines)     (GdkDrawable  *drawable,
			  GdkGC        *gc,
			  GdkPoint     *points,
			  gint          npoints);

  void (*draw_glyphs)    (GdkDrawable      *drawable,
			  GdkGC	           *gc,
			  PangoFont        *font,
			  gint              x,
			  gint              y,
			  PangoGlyphString *glyphs);

  void (*draw_image)     (GdkDrawable *drawable,
                          GdkGC	      *gc,
                          GdkImage    *image,
                          gint	       xsrc,
                          gint	       ysrc,
                          gint	       xdest,
                          gint	       ydest,
                          gint	       width,
                          gint	       height);
  
  gint (*get_depth)      (GdkDrawable  *drawable);
  void (*get_size)       (GdkDrawable  *drawable,
                          gint         *width,
                          gint         *height);

  void (*set_colormap)   (GdkDrawable  *drawable,
                          GdkColormap  *cmap);

  GdkColormap* (*get_colormap) (GdkDrawable *drawable);
  GdkVisual*   (*get_visual) (GdkDrawable  *drawable);

  GdkImage*    (*get_image)  (GdkDrawable  *drawable,
                              gint          x,
                              gint          y,
                              gint          width,
                              gint          height);

  GdkRegion*   (*get_clip_region)    (GdkDrawable  *drawable);
  GdkRegion*   (*get_visible_region) (GdkDrawable  *drawable);

  GdkDrawable* (*get_composite_drawable) (GdkDrawable *drawable,
                                          gint         x,
                                          gint         y,
                                          gint         width,
                                          gint         height,
                                          gint        *composite_x_offset,
                                          gint        *composite_y_offset);
  
  void         (*_gdk_reserved1) (void);
  void         (*_gdk_reserved2) (void);
  void         (*_gdk_reserved3) (void);
  void         (*_gdk_reserved4) (void);
};

GType           gdk_drawable_get_type     (void);

/* Manipulation of drawables
 */

#ifndef GDK_DISABLE_DEPRECATED
void            gdk_drawable_set_data     (GdkDrawable    *drawable,
					   const gchar    *key,
					   gpointer	  data,
					   GDestroyNotify  destroy_func);
gpointer        gdk_drawable_get_data     (GdkDrawable    *drawable,
					   const gchar    *key);
#endif /* GDK_DISABLE_DEPRECATED */

void            gdk_drawable_get_size     (GdkDrawable	  *drawable,
					   gint	          *width,
					   gint  	  *height);
void	        gdk_drawable_set_colormap (GdkDrawable	  *drawable,
					   GdkColormap	  *colormap);
GdkColormap*    gdk_drawable_get_colormap (GdkDrawable	  *drawable);
GdkVisual*      gdk_drawable_get_visual   (GdkDrawable	  *drawable);
gint            gdk_drawable_get_depth    (GdkDrawable	  *drawable);
GdkDrawable*    gdk_drawable_ref          (GdkDrawable    *drawable);
void            gdk_drawable_unref        (GdkDrawable    *drawable);

/* Drawing
 */
void gdk_draw_point     (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 gint              x,
			 gint              y);
void gdk_draw_line      (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 gint              x1,
			 gint              y1,
			 gint              x2,
			 gint              y2);
void gdk_draw_rectangle (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 gint              filled,
			 gint              x,
			 gint              y,
			 gint              width,
			 gint              height);
void gdk_draw_arc       (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 gint              filled,
			 gint              x,
			 gint              y,
			 gint              width,
			 gint              height,
			 gint              angle1,
			 gint              angle2);
void gdk_draw_polygon   (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 gint              filled,
			 GdkPoint         *points,
			 gint              npoints);
void gdk_draw_string    (GdkDrawable      *drawable,
			 GdkFont          *font,
			 GdkGC            *gc,
			 gint              x,
			 gint              y,
			 const gchar      *string);
void gdk_draw_text      (GdkDrawable      *drawable,
			 GdkFont          *font,
			 GdkGC            *gc,
			 gint              x,
			 gint              y,
			 const gchar      *text,
			 gint              text_length);
void gdk_draw_text_wc   (GdkDrawable      *drawable,
			 GdkFont          *font,
			 GdkGC            *gc,
			 gint              x,
			 gint              y,
			 const GdkWChar   *text,
			 gint              text_length);
void gdk_draw_drawable  (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 GdkDrawable      *src,
			 gint              xsrc,
			 gint              ysrc,
			 gint              xdest,
			 gint              ydest,
			 gint              width,
			 gint              height);
void gdk_draw_image     (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 GdkImage         *image,
			 gint              xsrc,
			 gint              ysrc,
			 gint              xdest,
			 gint              ydest,
			 gint              width,
			 gint              height);
void gdk_draw_points    (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 GdkPoint         *points,
			 gint              npoints);
void gdk_draw_segments  (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 GdkSegment       *segs,
			 gint              nsegs);
void gdk_draw_lines     (GdkDrawable      *drawable,
			 GdkGC            *gc,
			 GdkPoint         *points,
			 gint              npoints);

void gdk_draw_glyphs      (GdkDrawable      *drawable,
			   GdkGC            *gc,
			   PangoFont        *font,
			   gint              x,
			   gint              y,
			   PangoGlyphString *glyphs);
void gdk_draw_layout_line (GdkDrawable      *drawable,
			   GdkGC            *gc,
			   gint              x,
			   gint              y,
			   PangoLayoutLine  *line);
void gdk_draw_layout      (GdkDrawable      *drawable,
			   GdkGC            *gc,
			   gint              x,
			   gint              y,
			   PangoLayout      *layout);

void gdk_draw_layout_line_with_colors (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       gint             x,
                                       gint             y,
                                       PangoLayoutLine *line,
                                       GdkColor        *foreground,
                                       GdkColor        *background);
void gdk_draw_layout_with_colors      (GdkDrawable     *drawable,
                                       GdkGC           *gc,
                                       gint             x,
                                       gint             y,
                                       PangoLayout     *layout,
                                       GdkColor        *foreground,
                                       GdkColor        *background);

#ifndef GDK_DISABLE_DEPRECATED
#define gdk_draw_pixmap                gdk_draw_drawable
#define gdk_draw_bitmap                gdk_draw_drawable
#endif /* GDK_DISABLE_DEPRECATED */

GdkImage* gdk_drawable_get_image (GdkDrawable *drawable,
                                  gint         x,
                                  gint         y,
                                  gint         width,
                                  gint         height);

GdkRegion *gdk_drawable_get_clip_region    (GdkDrawable *drawable);
GdkRegion *gdk_drawable_get_visible_region (GdkDrawable *drawable);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_DRAWABLE_H__ */
