/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GDK_H__
#define __GDK_H__


#include <gdk/gdktypes.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Initialization, exit and events
 */
void   gdk_init		   (int	   *argc,
			    char ***argv);
void   gdk_exit		   (int	    error_code);
gchar* gdk_set_locale	   (void);

gint gdk_events_pending	 (void);
GdkEvent *gdk_event_get	 (void);
GdkEvent *gdk_event_get_graphics_expose (GdkWindow *window);
void gdk_event_put	 (GdkEvent     *event);

GdkEvent *gdk_event_copy (GdkEvent *event);
void	  gdk_event_free (GdkEvent *event);

void gdk_set_show_events (gint	show_events);
void gdk_set_use_xshm	 (gint	use_xshm);

gint gdk_get_show_events (void);
gint gdk_get_use_xshm	 (void);
gchar *gdk_get_display (void);

guint32 gdk_time_get	  (void);
guint32 gdk_timer_get	  (void);
void	gdk_timer_set	  (guint32 milliseconds);
void	gdk_timer_enable  (void);
void	gdk_timer_disable (void);

gint gdk_input_add_full	  (gint		     source,
			   GdkInputCondition condition,
			   GdkInputFunction  function,
			   gpointer	     data,
			   GdkDestroyNotify  destroy);
#define gdk_input_add_interp gdk_input_add_full	 
gint gdk_input_add	  (gint		     source,
			   GdkInputCondition condition,
			   GdkInputFunction  function,
			   gpointer	     data);
void gdk_input_remove	  (gint		     tag);

gint gdk_pointer_grab	(GdkWindow *	 window,
			 gint		 owner_events,
			 GdkEventMask	 event_mask,
			 GdkWindow *	 confine_to,
			 GdkCursor *	 cursor,
			 guint32	 time);
void gdk_pointer_ungrab (guint32	 time);

gint gdk_keyboard_grab	 (GdkWindow *	  window,
			  gint		  owner_events,
			  guint32	  time);
void gdk_keyboard_ungrab (guint32	  time);

gint gdk_pointer_is_grabbed (void);

gint gdk_screen_width  (void);
gint gdk_screen_height (void);

void gdk_flush (void);
void gdk_beep (void);

void gdk_key_repeat_disable (void);
void gdk_key_repeat_restore (void);


/* Visuals
 */
gint	      gdk_visual_get_best_depth	     (void);
GdkVisualType gdk_visual_get_best_type	     (void);
GdkVisual*    gdk_visual_get_system	     (void);
GdkVisual*    gdk_visual_get_best	     (void);
GdkVisual*    gdk_visual_get_best_with_depth (gint	     depth);
GdkVisual*    gdk_visual_get_best_with_type  (GdkVisualType  visual_type);
GdkVisual*    gdk_visual_get_best_with_both  (gint	     depth,
					      GdkVisualType  visual_type);

/* Actually, these are no-ops... */
GdkVisual* gdk_visual_ref (GdkVisual *visual);
void	   gdk_visual_unref (GdkVisual *visual);

void gdk_query_depths	    (gint	    **depths,
			     gint	     *count);
void gdk_query_visual_types (GdkVisualType  **visual_types,
			     gint	     *count);

GList* gdk_list_visuals (void);


/* Windows
 */
GdkWindow*    gdk_window_new	     (GdkWindow	    *parent,
				      GdkWindowAttr *attributes,
				      gint	     attributes_mask);

GdkWindow *   gdk_window_foreign_new (guint32	     anid);
void	      gdk_window_destroy     (GdkWindow	    *window);
GdkWindow*    gdk_window_ref	     (GdkWindow	    *window);
void	      gdk_window_unref	     (GdkWindow	    *window);

void	      gdk_window_show	     (GdkWindow	   *window);
void	      gdk_window_hide	     (GdkWindow	   *window);
void	      gdk_window_withdraw    (GdkWindow	   *window);
void	      gdk_window_move	     (GdkWindow	   *window,
				      gint	    x,
				      gint	    y);
void	      gdk_window_resize	     (GdkWindow	   *window,
				      gint	    width,
				      gint	    height);
void	      gdk_window_move_resize (GdkWindow	   *window,
				      gint	    x,
				      gint	    y,
				      gint	    width,
				      gint	    height);
void	      gdk_window_reparent    (GdkWindow	   *window,
				      GdkWindow	   *new_parent,
				      gint	    x,
				      gint	    y);
void	      gdk_window_clear	     (GdkWindow	   *window);
void	      gdk_window_clear_area  (GdkWindow	   *window,
				      gint	    x,
				      gint	    y,
				      gint	    width,
				      gint	    height);
void	      gdk_window_clear_area_e(GdkWindow	   *window,
				      gint	    x,
				      gint	    y,
				      gint	    width,
				      gint	    height);
void	      gdk_window_copy_area   (GdkWindow	   *window,
				      GdkGC	   *gc,
				      gint	    x,
				      gint	    y,
				      GdkWindow	   *source_window,
				      gint	    source_x,
				      gint	    source_y,
				      gint	    width,
				      gint	    height);
void	      gdk_window_raise	     (GdkWindow	   *window);
void	      gdk_window_lower	     (GdkWindow	   *window);

void	      gdk_window_set_user_data	 (GdkWindow	  *window,
					  gpointer	   user_data);
void	      gdk_window_set_override_redirect(GdkWindow  *window,
					       gboolean override_redirect);

void	      gdk_window_add_filter	(GdkWindow     *window,
					 GdkFilterFunc	function,
					 gpointer	data);
void	      gdk_window_remove_filter	(GdkWindow     *window,
					 GdkFilterFunc	function,
					 gpointer	data);

/* 
 * This allows for making shaped (partially transparent) windows
 * - cool feature, needed for Drag and Drag for example.
 *  The shape_mask can be the mask
 *  from gdk_pixmap_create_from_xpm.   Stefan Wille
 */
void gdk_window_shape_combine_mask (GdkWindow	    *window,
				    GdkBitmap	    *shape_mask,
				    gint	     offset_x,
				    gint	     offset_y);

/* 
 * Drag & Drop
 * Algorithm (drop source):
 * A window being dragged will be sent a GDK_DRAG_BEGIN message.
 * It will then do gdk_dnd_drag_addwindow() for any other windows that are to be
 * dragged.
 * When we get a DROP_ENTER incoming, we send it on to the window in question.
 * That window needs to use gdk_dnd_drop_enter_reply() to indicate the state of
 * things (it must call that even if it's not going to accept the drop)
 *
 * These two turn on/off drag or drop, and if enabling it also
 * sets the list of types supported. The list of types passed in
 * should be in order of decreasing preference.
 */
void gdk_window_dnd_drag_set (GdkWindow	 *window,
			      guint8	  drag_enable,
			      gchar	**typelist,
			      guint	  numtypes);

/* 
 *XXX todo: add a GDK_DROP_ENTER which can look at actual data
 */
void gdk_window_dnd_drop_set (GdkWindow	 *window,
			      guint8	  drop_enable,
			      gchar	**typelist,
			      guint	  numtypes,
			      guint8	  destructive_op);

/* 
 * This is used by the GDK_DRAG_BEGIN handler. An example of usage would be a
 * file manager where multiple icons were selected and the drag began.
 * The icon that the drag actually began on would gdk_dnd_drag_addwindow
 * for all the other icons that were being dragged... 
 */
void gdk_dnd_drag_addwindow  (GdkWindow	 *window);
void gdk_window_dnd_data_set (GdkWindow	 *window,
			      GdkEvent	 *event,
			      gpointer	  data,
			      gulong	  data_numbytes);
void gdk_dnd_set_drag_cursors(GdkCursor *default_cursor,
			      GdkCursor *goahead_cursor);
void gdk_dnd_set_drag_shape(GdkWindow *default_pixmapwin,
			    GdkPoint *default_hotspot,
			    GdkWindow *goahead_pixmapwin,
			    GdkPoint *goahead_hotspot);

void	      gdk_window_set_hints	 (GdkWindow	  *window,
					  gint		   x,
					  gint		   y,
					  gint		   min_width,
					  gint		   min_height,
					  gint		   max_width,
					  gint		   max_height,
					  gint		   flags);
void	      gdk_window_set_title	 (GdkWindow	  *window,
					  const gchar	  *title);
void	      gdk_window_set_background	 (GdkWindow	  *window,
					  GdkColor	  *color);
void	      gdk_window_set_back_pixmap (GdkWindow	  *window,
					  GdkPixmap	  *pixmap,
					  gint		   parent_relative);
void	      gdk_window_set_cursor	 (GdkWindow	  *window,
					  GdkCursor	  *cursor);
void	      gdk_window_set_colormap	 (GdkWindow	  *window,
					  GdkColormap	  *colormap);
void	      gdk_window_get_user_data	 (GdkWindow	  *window,
					  gpointer	  *data);
void	      gdk_window_get_geometry	 (GdkWindow	  *window,
					  gint		  *x,
					  gint		  *y,
					  gint		  *width,
					  gint		  *height,
					  gint		  *depth);
void	      gdk_window_get_position	 (GdkWindow	  *window,
					  gint		  *x,
					  gint		  *y);
void	      gdk_window_get_size	 (GdkWindow	  *window,
					  gint		  *width,
					  gint		  *height);
GdkVisual*    gdk_window_get_visual	 (GdkWindow	  *window);
GdkColormap*  gdk_window_get_colormap	 (GdkWindow	  *window);
GdkWindowType gdk_window_get_type	 (GdkWindow	  *window);
gint	      gdk_window_get_origin	 (GdkWindow	  *window,
					  gint		  *x,
					  gint		  *y);
GdkWindow*    gdk_window_get_pointer	 (GdkWindow	  *window,
					  gint		  *x,
					  gint		  *y,
					  GdkModifierType *mask);
GdkWindow*    gdk_window_get_parent	 (GdkWindow	  *window);
GdkWindow*    gdk_window_get_toplevel	 (GdkWindow	  *window);
GList*	      gdk_window_get_children	 (GdkWindow	  *window);
GdkEventMask  gdk_window_get_events	 (GdkWindow	  *window);
void	      gdk_window_set_events	 (GdkWindow	  *window,
					  GdkEventMask	   event_mask);

void	      gdk_window_set_icon	 (GdkWindow	  *window, 
					  GdkWindow	  *icon_window,
					  GdkPixmap	  *pixmap,
					  GdkBitmap	  *mask);
void	      gdk_window_set_icon_name	 (GdkWindow	  *window, 
					  gchar		  *name);
void	      gdk_window_set_group	 (GdkWindow	  *window, 
					  GdkWindow	  *leader);
void	      gdk_window_set_decorations (GdkWindow	  *window,
					  GdkWMDecoration  decorations);
void	      gdk_window_set_functions	 (GdkWindow	  *window,
					  GdkWMFunction	   functions);

/* Cursors
 */
GdkCursor* gdk_cursor_new		 (GdkCursorType	  cursor_type);
GdkCursor* gdk_cursor_new_from_pixmap	 (GdkPixmap	  *source,
					  GdkPixmap	  *mask,
					  GdkColor	  *fg,
					  GdkColor	  *bg,
					  gint		   x,
					  gint		   y);
void	   gdk_cursor_destroy		 (GdkCursor	 *cursor);


/* GCs
 */
GdkGC* gdk_gc_new		  (GdkWindow	    *window);
GdkGC* gdk_gc_new_with_values	  (GdkWindow	    *window,
				   GdkGCValues	    *values,
				   GdkGCValuesMask   values_mask);
GdkGC* gdk_gc_ref		  (GdkGC	    *gc);
void   gdk_gc_unref		  (GdkGC	    *gc);
void   gdk_gc_destroy		  (GdkGC	    *gc);
void   gdk_gc_get_values	  (GdkGC	    *gc,
				   GdkGCValues	    *values);
void   gdk_gc_set_foreground	  (GdkGC	    *gc,
				   GdkColor	    *color);
void   gdk_gc_set_background	  (GdkGC	    *gc,
				   GdkColor	    *color);
void   gdk_gc_set_font		  (GdkGC	    *gc,
				   GdkFont	    *font);
void   gdk_gc_set_function	  (GdkGC	    *gc,
				   GdkFunction	     function);
void   gdk_gc_set_fill		  (GdkGC	    *gc,
				   GdkFill	     fill);
void   gdk_gc_set_tile		  (GdkGC	    *gc,
				   GdkPixmap	    *tile);
void   gdk_gc_set_stipple	  (GdkGC	    *gc,
				   GdkPixmap	    *stipple);
void   gdk_gc_set_ts_origin	  (GdkGC	    *gc,
				   gint		     x,
				   gint		     y);
void   gdk_gc_set_clip_origin	  (GdkGC	    *gc,
				   gint		     x,
				   gint		     y);
void   gdk_gc_set_clip_mask	  (GdkGC	    *gc,
				   GdkBitmap	    *mask);
void   gdk_gc_set_clip_rectangle  (GdkGC	    *gc,
				   GdkRectangle	    *rectangle);
void   gdk_gc_set_clip_region	  (GdkGC	    *gc,
				   GdkRegion	    *region);
void   gdk_gc_set_subwindow	  (GdkGC	    *gc,
				   GdkSubwindowMode  mode);
void   gdk_gc_set_exposures	  (GdkGC	    *gc,
				   gint		     exposures);
void   gdk_gc_set_line_attributes (GdkGC	    *gc,
				   gint		     line_width,
				   GdkLineStyle	     line_style,
				   GdkCapStyle	     cap_style,
				   GdkJoinStyle	     join_style);
void   gdk_gc_copy		  (GdkGC	     *dst_gc,
				   GdkGC	     *src_gc);


/* Pixmaps
 */
GdkPixmap* gdk_pixmap_new		(GdkWindow  *window,
					 gint	     width,
					 gint	     height,
					 gint	     depth);
GdkBitmap* gdk_bitmap_create_from_data	(GdkWindow  *window,
					 gchar	    *data,
					 gint	     width,
					 gint	     height);
GdkPixmap* gdk_pixmap_create_from_data	(GdkWindow  *window,
					 gchar	    *data,
					 gint	     width,
					 gint	     height,
					 gint	     depth,
					 GdkColor   *fg,
					 GdkColor   *bg);
GdkPixmap* gdk_pixmap_create_from_xpm	(GdkWindow  *window,
					 GdkBitmap **mask,
					 GdkColor   *transparent_color,
					 const gchar *filename);
GdkPixmap* gdk_pixmap_colormap_create_from_xpm 
                                        (GdkWindow   *window,
					 GdkColormap *colormap,
					 GdkBitmap  **mask,
					 GdkColor    *transparent_color,
					 const gchar *filename);
GdkPixmap* gdk_pixmap_create_from_xpm_d (GdkWindow  *window,
					 GdkBitmap **mask,
					 GdkColor   *transparent_color,
					 gchar	   **data);
GdkPixmap* gdk_pixmap_colormap_create_from_xpm_d 
                                        (GdkWindow   *window,
					 GdkColormap *colormap,
					 GdkBitmap  **mask,
					 GdkColor    *transparent_color,
					 gchar     **data);
GdkPixmap *gdk_pixmap_ref		(GdkPixmap  *pixmap);
void	   gdk_pixmap_unref		(GdkPixmap  *pixmap);

GdkBitmap *gdk_bitmap_ref		(GdkBitmap  *pixmap);
void	   gdk_bitmap_unref		(GdkBitmap  *pixmap);


/* Images
 */
GdkImage* gdk_image_new_bitmap(GdkVisual *,
				gpointer,
				gint,
				gint);
GdkImage*  gdk_image_new       (GdkImageType  type,
				GdkVisual    *visual,
				gint	      width,
				gint	      height);
GdkImage*  gdk_image_get       (GdkWindow    *window,
				gint	      x,
				gint	      y,
				gint	      width,
				gint	      height);
void	   gdk_image_put_pixel (GdkImage     *image,
				gint	      x,
				gint	      y,
				guint32	      pixel);
guint32	   gdk_image_get_pixel (GdkImage     *image,
				gint	      x,
				gint	      y);
void	   gdk_image_destroy   (GdkImage     *image);


/* Color
 */
GdkColormap* gdk_colormap_new	  (GdkVisual   *visual,
				   gint		allocate);
GdkColormap* gdk_colormap_ref	  (GdkColormap *cmap);
void	     gdk_colormap_unref	  (GdkColormap *cmap);

GdkColormap* gdk_colormap_get_system	   (void);
gint	     gdk_colormap_get_system_size  (void);

void gdk_colormap_change (GdkColormap	*colormap,
			  gint		 ncolors);
void gdk_colors_store	 (GdkColormap	*colormap,
			  GdkColor	*colors,
			  gint		 ncolors);
gint gdk_colors_alloc	 (GdkColormap	*colormap,
			  gint		 contiguous,
			  gulong	*planes,
			  gint		 nplanes,
			  gulong	*pixels,
			  gint		 npixels);
void gdk_colors_free	 (GdkColormap	*colormap,
			  gulong	*pixels,
			  gint		 npixels,
			  gulong	 planes);
gint gdk_color_white	 (GdkColormap	*colormap,
			  GdkColor	*color);
gint gdk_color_black	 (GdkColormap	*colormap,
			  GdkColor	*color);
gint gdk_color_parse	 (const gchar	*spec,
			  GdkColor	*color);
gint gdk_color_alloc	 (GdkColormap	*colormap,
			  GdkColor	*color);
gint gdk_color_change	 (GdkColormap	*colormap,
			  GdkColor	*color);
gint gdk_color_equal	 (GdkColor	*colora,
			  GdkColor	*colorb);


/* Fonts
 */
GdkFont* gdk_font_load	    (const gchar    *font_name);
GdkFont* gdk_fontset_load   (gchar    *fontset_name);
GdkFont* gdk_font_ref	    (GdkFont  *font);
void	 gdk_font_unref	    (GdkFont  *font);
gint	 gdk_font_id	    (GdkFont  *font);
gint	 gdk_font_equal	    (GdkFont  *fonta,
			     GdkFont  *fontb);
gint	 gdk_string_width   (GdkFont  *font,
			     const gchar    *string);
gint	 gdk_text_width	    (GdkFont  *font,
			     const gchar    *text,
			     gint      text_length);
gint	 gdk_char_width	    (GdkFont  *font,
			     gchar     character);
gint	 gdk_string_measure (GdkFont  *font,
			     const gchar    *string);
gint	 gdk_text_measure   (GdkFont  *font,
			     const gchar    *text,
			     gint      text_length);
gint	 gdk_char_measure   (GdkFont  *font,
			     gchar     character);


/* Drawing
 */
void gdk_draw_point	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  gint		x,
			  gint		y);
void gdk_draw_line	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  gint		x1,
			  gint		y1,
			  gint		x2,
			  gint		y2);
void gdk_draw_rectangle	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  gint		filled,
			  gint		x,
			  gint		y,
			  gint		width,
			  gint		height);
void gdk_draw_arc	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  gint		filled,
			  gint		x,
			  gint		y,
			  gint		width,
			  gint		height,
			  gint		angle1,
			  gint		angle2);
void gdk_draw_polygon	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  gint		filled,
			  GdkPoint     *points,
			  gint		npoints);
void gdk_draw_string	 (GdkDrawable  *drawable,
			  GdkFont      *font,
			  GdkGC	       *gc,
			  gint		x,
			  gint		y,
			  const gchar	     *string);
void gdk_draw_text	 (GdkDrawable  *drawable,
			  GdkFont      *font,
			  GdkGC	       *gc,
			  gint		x,
			  gint		y,
			  const gchar	     *text,
			  gint		text_length);
void gdk_draw_pixmap	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  GdkDrawable  *src,
			  gint		xsrc,
			  gint		ysrc,
			  gint		xdest,
			  gint		ydest,
			  gint		width,
			  gint		height);
void gdk_draw_bitmap	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  GdkDrawable  *src,
			  gint		xsrc,
			  gint		ysrc,
			  gint		xdest,
			  gint		ydest,
			  gint		width,
			  gint		height);
void gdk_draw_image	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  GdkImage     *image,
			  gint		xsrc,
			  gint		ysrc,
			  gint		xdest,
			  gint		ydest,
			  gint		width,
			  gint		height);
void gdk_draw_points	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  GdkPoint     *points,
			  gint		npoints);
void gdk_draw_segments	 (GdkDrawable  *drawable,
			  GdkGC	       *gc,
			  GdkSegment   *segs,
			  gint		nsegs);
void gdk_draw_lines      (GdkDrawable  *drawable,
                          GdkGC        *gc,
                          GdkPoint     *points,
                          gint          npoints);
 



/* Selections
 */
gint	   gdk_selection_owner_set (GdkWindow	 *owner,
				    GdkAtom	  selection,
				    guint32	  time,
				    gint	  send_event);
GdkWindow* gdk_selection_owner_get (GdkAtom	  selection);
void	   gdk_selection_convert   (GdkWindow	 *requestor,
				    GdkAtom	  selection,
				    GdkAtom	  target,
				    guint32	  time);
gint	   gdk_selection_property_get (GdkWindow  *requestor,
				       guchar	 **data,
				       GdkAtom	  *prop_type,
				       gint	  *prop_format);
void	   gdk_selection_send_notify (guint32	    requestor,
				      GdkAtom	    selection,
				      GdkAtom	    target,
				      GdkAtom	    property,
				      guint32	    time);

gint	   gdk_text_property_to_text_list (GdkAtom encoding, gint format,
					   guchar *text, gint length,
					   gchar ***list);
void	   gdk_free_text_list		  (gchar **list);
gint	   gdk_string_to_compound_text	  (gchar *str,
					   GdkAtom *encoding, gint *format,
					   guchar **ctext, gint *length);
void	   gdk_free_compound_text	  (guchar *ctext);

/* Properties
 */
GdkAtom gdk_atom_intern	    (const gchar *atom_name,
			     gint	  only_if_exists);
gchar*	gdk_atom_name	    (GdkAtom atom);
gint	gdk_property_get    (GdkWindow	 *window,
			     GdkAtom	  property,
			     GdkAtom	  type,
			     gulong	  offset,
			     gulong	  length,
			     gint	  pdelete,
			     GdkAtom	 *actual_property_type,
			     gint	 *actual_format,
			     gint	 *actual_length,
			     guchar	**data);

void	gdk_property_change (GdkWindow	 *window,
			     GdkAtom	  property,
			     GdkAtom	  type,
			     gint	  format,
			     GdkPropMode  mode,
			     guchar	 *data,
			     gint	  nelements);
void	gdk_property_delete (GdkWindow	 *window,
			     GdkAtom	  property);


/* Rectangle utilities
 */
gint gdk_rectangle_intersect (GdkRectangle *src1,
			      GdkRectangle *src2,
			      GdkRectangle *dest);

/* XInput support
 */

void gdk_input_init			    (void);
void gdk_input_exit			    (void);
GList *gdk_input_list_devices		    (void);
void gdk_input_set_extension_events	    (GdkWindow *window,
					     gint mask,
					     GdkExtensionMode mode);
void gdk_input_set_source		    (guint32 deviceid,
					     GdkInputSource source);
gint gdk_input_set_mode			    (guint32 deviceid,
					     GdkInputMode mode);
void gdk_input_set_axes			    (guint32 deviceid,
					     GdkAxisUse *axes);
void gdk_input_set_key			    (guint32 deviceid,
					     guint   index,
					     guint   keyval,
					     GdkModifierType modifiers);
void gdk_input_window_get_pointer     (GdkWindow       *window,
				       guint32	       deviceid,
				       gdouble	       *x,
				       gdouble	       *y,
				       gdouble	       *pressure,
				       gdouble	       *xtilt,
				       gdouble	       *ytilt,
				       GdkModifierType *mask);

GdkTimeCoord *gdk_input_motion_events (GdkWindow *window,
				       guint32 deviceid,
				       guint32 start,
				       guint32 stop,
				       gint *nevents_return);

/* International Input Method Support Functions
 */

gint   gdk_im_ready		(void);

void   gdk_im_begin		(GdkIC ic, GdkWindow* window);
void   gdk_im_end		(void);
GdkIMStyle gdk_im_decide_style	(GdkIMStyle supported_style);
GdkIMStyle gdk_im_set_best_style (GdkIMStyle best_allowed_style);
GdkIC  gdk_ic_new		(GdkWindow* client_window,
				 GdkWindow* focus_window,
				 GdkIMStyle style, ...);
void   gdk_ic_destroy		(GdkIC ic);
GdkIMStyle   gdk_ic_get_style	(GdkIC ic);
void   gdk_ic_set_values	(GdkIC ic, ...);
void   gdk_ic_get_values	(GdkIC ic, ...);
void   gdk_ic_set_attr		(GdkIC ic, const char *target, ...);
void   gdk_ic_get_attr		(GdkIC ic, const char *target, ...);
GdkEventMask gdk_ic_get_events	(GdkIC ic);

/* Miscellaneous */
void gdk_event_send_clientmessage_toall(GdkEvent *event);

/* Color Context */

GdkColorContext *gdk_color_context_new			  (GdkVisual   *visual,
							   GdkColormap *colormap);

GdkColorContext *gdk_color_context_new_mono		  (GdkVisual   *visual,
							   GdkColormap *colormap);

void		 gdk_color_context_free			  (GdkColorContext *cc);

gulong		 gdk_color_context_get_pixel		  (GdkColorContext *cc,
							   gushort	    red,
							   gushort	    green,
							   gushort	    blue,
							   gint		   *failed);
void		 gdk_color_context_get_pixels		  (GdkColorContext *cc,
							   gushort	   *reds,
							   gushort	   *greens,
							   gushort	   *blues,
							   gint		    ncolors,
							   gulong	   *colors,
							   gint		   *nallocated);
void		 gdk_color_context_get_pixels_incremental (GdkColorContext *cc,
							   gushort	   *reds,
							   gushort	   *greens,
							   gushort	   *blues,
							   gint		    ncolors,
							   gint		   *used,
							   gulong	   *colors,
							   gint		   *nallocated);

gint		 gdk_color_context_query_color		  (GdkColorContext *cc,
							   GdkColor	   *color);
gint		 gdk_color_context_query_colors		  (GdkColorContext *cc,
							   GdkColor	   *colors,
							   gint		    num_colors);

gint		 gdk_color_context_add_palette		  (GdkColorContext *cc,
							   GdkColor	   *palette,
							   gint		    num_palette);

void		 gdk_color_context_init_dither		  (GdkColorContext *cc);
void		 gdk_color_context_free_dither		  (GdkColorContext *cc);

gulong		 gdk_color_context_get_pixel_from_palette (GdkColorContext *cc,
							   gushort	   *red,
							   gushort	   *green,
							   gushort	   *blue,
							   gint		   *failed);
guchar		 gdk_color_context_get_index_from_palette (GdkColorContext *cc,
							   gint		   *red,
							   gint		   *green,
							   gint		   *blue,
							   gint		   *failed);
/* Regions
 */

GdkRegion*     gdk_region_new	    (void);
void	       gdk_region_destroy   (GdkRegion	   *region);

gboolean       gdk_region_empty	    (GdkRegion	   *region);
gboolean       gdk_region_equal	    (GdkRegion	   *region1,
				     GdkRegion	   *region2);
gboolean       gdk_region_point_in  (GdkRegion	   *region,
				     int		   x,
				     int		   y);
GdkOverlapType gdk_region_rect_in   (GdkRegion	   *region,
				     GdkRectangle  *rect);

GdkRegion*     gdk_region_polygon   (GdkPoint      *points,
				     gint           npoints,
				     GdkFillRule    fill_rule);

void	       gdk_region_offset   (GdkRegion	   *region,
				    gint	   dx,
				    gint	   dy);
void	       gdk_region_shrink   (GdkRegion	   *region,
				    gint	   dx,
				    gint	   dy);

GdkRegion*    gdk_region_union_with_rect  (GdkRegion	  *region,
					   GdkRectangle	  *rect);
GdkRegion*    gdk_regions_intersect	  (GdkRegion	  *source1,
					   GdkRegion	  *source2);
GdkRegion*    gdk_regions_union		  (GdkRegion	  *source1,
					   GdkRegion	  *source2);
GdkRegion*    gdk_regions_subtract	  (GdkRegion	  *source1,
					   GdkRegion	  *source2);
GdkRegion*    gdk_regions_xor		  (GdkRegion	  *source1,
					   GdkRegion	  *source2);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GDK_H__ */
