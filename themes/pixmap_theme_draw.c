
#include "pixmap_theme.h"

extern GtkStyleClass th_default_class;
extern GdkFont     *default_font;
extern GSList      *unattached_styles;

/* Theme functions to export */
GtkStyle           *
                    gtk_style_new(void);
void
                    gtk_style_set_background(GtkStyle * style,
					     GdkWindow * window,
					     GtkStateType state_type);

/* internal functions */
void
                    draw_hline(GtkStyle * style,
			       GdkWindow * window,
			       GtkStateType state_type,
			       GdkRectangle * area,
			       GtkWidget * widget,
			       gchar * detail,
			       gint x1,
			       gint x2,
			       gint y);
void
                    draw_vline(GtkStyle * style,
			       GdkWindow * window,
			       GtkStateType state_type,
			       GdkRectangle * area,
			       GtkWidget * widget,
			       gchar * detail,
			       gint y1,
			       gint y2,
			       gint x);
void
                    draw_shadow(GtkStyle * style,
				GdkWindow * window,
				GtkStateType state_type,
				GtkShadowType shadow_type,
				GdkRectangle * area,
				GtkWidget * widget,
				gchar * detail,
				gint x,
				gint y,
				gint width,
				gint height);

void
                    draw_polygon(GtkStyle * style,
				 GdkWindow * window,
				 GtkStateType state_type,
				 GtkShadowType shadow_type,
				 GdkRectangle * area,
				 GtkWidget * widget,
				 gchar * detail,
				 GdkPoint * point,
				 gint npoints,
				 gint fill);
void
                    draw_arrow(GtkStyle * style,
			       GdkWindow * window,
			       GtkStateType state_type,
			       GtkShadowType shadow_type,
			       GdkRectangle * area,
			       GtkWidget * widget,
			       gchar * detail,
			       GtkArrowType arrow_type,
			       gint fill,
			       gint x,
			       gint y,
			       gint width,
			       gint height);
void
                    draw_diamond(GtkStyle * style,
				 GdkWindow * window,
				 GtkStateType state_type,
				 GtkShadowType shadow_type,
				 GdkRectangle * area,
				 GtkWidget * widget,
				 gchar * detail,
				 gint x,
				 gint y,
				 gint width,
				 gint height);
void
                    draw_oval(GtkStyle * style,
			      GdkWindow * window,
			      GtkStateType state_type,
			      GtkShadowType shadow_type,
			      GdkRectangle * area,
			      GtkWidget * widget,
			      gchar * detail,
			      gint x,
			      gint y,
			      gint width,
			      gint height);
void
                    draw_string(GtkStyle * style,
				GdkWindow * window,
				GtkStateType state_type,
				GdkRectangle * area,
				GtkWidget * widget,
				gchar * detail,
				gint x,
				gint y,
				const gchar * string);
void
                    draw_box(GtkStyle * style,
			     GdkWindow * window,
			     GtkStateType state_type,
			     GtkShadowType shadow_type,
			     GdkRectangle * area,
			     GtkWidget * widget,
			     gchar * detail,
			     gint x,
			     gint y,
			     gint width,
			     gint height);
void
                    draw_flat_box(GtkStyle * style,
				  GdkWindow * window,
				  GtkStateType state_type,
				  GtkShadowType shadow_type,
				  GdkRectangle * area,
				  GtkWidget * widget,
				  gchar * detail,
				  gint x,
				  gint y,
				  gint width,
				  gint height);
void
                    draw_check(GtkStyle * style,
			       GdkWindow * window,
			       GtkStateType state_type,
			       GtkShadowType shadow_type,
			       GdkRectangle * area,
			       GtkWidget * widget,
			       gchar * detail,
			       gint x,
			       gint y,
			       gint width,
			       gint height);
void
                    draw_option(GtkStyle * style,
				GdkWindow * window,
				GtkStateType state_type,
				GtkShadowType shadow_type,
				GdkRectangle * area,
				GtkWidget * widget,
				gchar * detail,
				gint x,
				gint y,
				gint width,
				gint height);
void
                    draw_cross(GtkStyle * style,
			       GdkWindow * window,
			       GtkStateType state_type,
			       GtkShadowType shadow_type,
			       GdkRectangle * area,
			       GtkWidget * widget,
			       gchar * detail,
			       gint x,
			       gint y,
			       gint width,
			       gint height);
void
                    draw_ramp(GtkStyle * style,
			      GdkWindow * window,
			      GtkStateType state_type,
			      GtkShadowType shadow_type,
			      GdkRectangle * area,
			      GtkWidget * widget,
			      gchar * detail,
			      GtkArrowType arrow_type,
			      gint x,
			      gint y,
			      gint width,
			      gint height);
void
                    draw_tab(GtkStyle * style,
			     GdkWindow * window,
			     GtkStateType state_type,
			     GtkShadowType shadow_type,
			     GdkRectangle * area,
			     GtkWidget * widget,
			     gchar * detail,
			     gint x,
			     gint y,
			     gint width,
			     gint height);
void
                    draw_shadow_gap(GtkStyle * style,
				    GdkWindow * window,
				    GtkStateType state_type,
				    GtkShadowType shadow_type,
				    GdkRectangle * area,
				    GtkWidget * widget,
				    gchar * detail,
				    gint x,
				    gint y,
				    gint width,
				    gint height,
				    gint gap_side,
				    gint gap_x,
				    gint gap_width);
void
                    draw_box_gap(GtkStyle * style,
				 GdkWindow * window,
				 GtkStateType state_type,
				 GtkShadowType shadow_type,
				 GdkRectangle * area,
				 GtkWidget * widget,
				 gchar * detail,
				 gint x,
				 gint y,
				 gint width,
				 gint height,
				 gint gap_side,
				 gint gap_x,
				 gint gap_width);
void
                    draw_extension(GtkStyle * style,
				   GdkWindow * window,
				   GtkStateType state_type,
				   GtkShadowType shadow_type,
				   GdkRectangle * area,
				   GtkWidget * widget,
				   gchar * detail,
				   gint x,
				   gint y,
				   gint width,
				   gint height,
				   gint gap_side);
void
                    draw_focus(GtkStyle * style,
			       GdkWindow * window,
			       GdkRectangle * area,
			       GtkWidget * widget,
			       gchar * detail,
			       gint x,
			       gint y,
			       gint width,
			       gint height);
void
                    draw_slider(GtkStyle * style,
				GdkWindow * window,
				GtkStateType state_type,
				GtkShadowType shadow_type,
				GdkRectangle * area,
				GtkWidget * widget,
				gchar * detail,
				gint x,
				gint y,
				gint width,
				gint height,
				GtkOrientation orientation);
void
                    draw_entry(GtkStyle * style,
			       GdkWindow * window,
			       GtkStateType state_type,
			       GdkRectangle * area,
			       GtkWidget * widget,
			       gchar * detail,
			       gint x,
			       gint y,
			       gint width,
			       gint height);
void
                    draw_handle(GtkStyle * style,
				GdkWindow * window,
				GtkStateType state_type,
				GtkShadowType shadow_type,
				GdkRectangle * area,
				GtkWidget * widget,
				gchar * detail,
				gint x,
				gint y,
				gint width,
				gint height,
				GtkOrientation orientation);

/* internal data structs */

GtkStyleClass       th_default_class =
{
  2,
  2,
  draw_hline,
  draw_vline,
  draw_shadow,
  draw_polygon,
  draw_arrow,
  draw_diamond,
  draw_oval,
  draw_string,
  draw_box,
  draw_flat_box,
  draw_check,
  draw_option,
  draw_cross,
  draw_ramp,
  draw_tab,
  draw_shadow_gap,
  draw_box_gap,
  draw_extension,
  draw_focus,
  draw_slider,
  draw_entry,
  draw_handle
};

static GdkImlibImage *
load_image(char *name)
{
  return gdk_imlib_load_image(name);
}

struct theme_image *
match_theme_image(GtkStyle * style, 
		  GtkStateType state, 
		  GtkShadowType shadow_type, 
		  GtkWidget *widget, 
		  char *detail, 
		  GtkArrowType arrow_type, 
		  GtkOrientation orientation, 
		  gint gap_side, 
		  guint function)
{
  GList              *l;
  struct theme_image *i;
  char               *str = "";

  l = ((ThemeData *)(style->engine_data))->img_list;
  if (!detail) 
    detail = str;
  
  while (l)
    {
      i = (struct theme_image *)l->data;
      if ( (i) &&
	  (function == i->function) &&
	  
	  (((i->__state) &&
	    (state == i->state))
	   || (!(i->__state))) &&
	  
	  (((i->__shadow) && 
	    (shadow_type == i->shadow))
	   || (!(i->__shadow))) &&
	  
	  (((i->__arrow_direction) && 
	    (arrow_type == i->arrow_direction))
	   || (!(i->__arrow_direction))) &&
	  
	  (((i->__orientation) && 
	    (orientation == i->orientation))
	   || (!(i->__orientation))) &&

	  (((i->__gap_side) && 
	    (gap_side == i->gap_side))
	   || (!(i->__gap_side))) &&
	  
	  (((i->__state) && 
	    (state == i->state))
	   || (!(i->__state))) &&
	  
	  (((i->detail) && 
	    (!strcmp(detail, i->detail)))
	   || (!(i->detail))))
	return i;
      
      l = l->next;
    }
  return NULL;
}

void
apply_theme_image(GdkWindow *window, struct theme_image *img, gchar setbg, 
		  GdkGC *gc, GdkRectangle *area, gint x, gint y, gint width, 
		  gint height)
{
  GdkImlibImage      *im;
  GdkPixmap          *p, *m;
  GdkRectangle       rect0, rect;
  gchar              haverect = 1;
  
  if (img->file)
    {
      im = load_image(img->file);
      if (im)
	{  
	  gdk_imlib_set_image_border(im, &(img->border));
	  if (img->stretch)
	    gdk_imlib_render(im, width, height);
	  else
	    gdk_imlib_render(im, im->rgb_width, im->rgb_height);
	  p = gdk_imlib_move_image(im);
	  m = gdk_imlib_move_mask(im);
	  
	  if (area)
	    {
	      rect0.x = x;
	      rect0.y = y;
	      rect0.width = width;
	      rect0.height = height;
	      haverect = gdk_rectangle_intersect(&rect0, area, &rect);
	    }
	  else
	    {
	      rect.x = x;
	      rect.y = y;
	      rect.width = width;
	      rect.height = height;
	    }
	  if ((haverect) && (p))
	    {
	      if (setbg)
		{
		  gdk_window_set_back_pixmap(window, p, 0);
		  if (area)
		    gdk_window_clear_area(window, rect.x, rect.y,
					  rect.width, rect.height);
		  else
		    gdk_window_clear(window);
		  if (m)
		    gdk_window_shape_combine_mask(window, m, 0, 0);
		}
	      else
		{
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, m);
		      gdk_gc_set_clip_origin(gc, x, y);
		    }
		  gdk_draw_pixmap(window, gc, p, rect.x - x, rect.y - y, 
				  rect.x, rect.y, rect.width, rect.height);
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		}
	      gdk_imlib_free_pixmap(p);
	    }
	  gdk_imlib_destroy_image(im);
	}
    }
  
  if (!img->overlay_file)
    {
      if (area)
	gdk_gc_set_clip_rectangle(gc, NULL);
      return;
    }
  im = load_image(img->overlay_file);
  if (!im)
    {
      if (area)
	gdk_gc_set_clip_rectangle(gc, NULL);
      return;
    }
    
  gdk_imlib_set_image_border(im, &(img->overlay_border));
  if (img->overlay_stretch)
    gdk_imlib_render(im, width, height);
  else
    {
      x += (width - im->rgb_width) / 2;
      y += (height - im->rgb_height) / 2;
      width = im->rgb_width;
      height = im->rgb_height;
      gdk_imlib_render(im, im->rgb_width, im->rgb_height);
    }
  p = gdk_imlib_move_image(im);
  m = gdk_imlib_move_mask(im);
  haverect = 1;
  
  if (area)
    {
      rect0.x = x;
      rect0.y = y;
      rect0.width = width;
      rect0.height = height;
      haverect = gdk_rectangle_intersect(&rect0, area, &rect);
    }
  else
    {
      rect.x = x;
      rect.y = y;
      rect.width = width;
      rect.height = height;
    }
  if ((haverect) && (p))
    {
      if (m)
	{
	  gdk_gc_set_clip_mask(gc, m);
	  gdk_gc_set_clip_origin(gc, x, y);
	}
      gdk_draw_pixmap(window, gc, p, rect.x - x, rect.y - y, 
		      rect.x, rect.y, rect.width, rect.height);
      if (m)
	{
	  gdk_gc_set_clip_mask(gc, NULL);
	  gdk_gc_set_clip_origin(gc, 0, 0);
	}
      gdk_imlib_free_pixmap(p);
    }
  gdk_imlib_destroy_image(im);
}

void
apply_theme_image_border(GdkWindow *window, struct theme_image *img, gchar setbg, 
			 GdkGC *gc, GdkRectangle *area, gint x, gint y, gint width, 
			 gint height)
{
  GdkImlibImage      *im;
  GdkPixmap          *p, *m;
  GdkRectangle       rect0, rect1, rect;
  gchar              haverect = 1;
  
  if (img->file)
    {
      im = load_image(img->file);
      if (im)
	{  
	  gdk_imlib_set_image_border(im, &(img->border));
	  gdk_imlib_render(im, width, height);
	  p = gdk_imlib_move_image(im);
	  m = gdk_imlib_move_mask(im);
	  
	  if (area)
	    {
	      rect0.x = x;
	      rect0.y = y;
	      rect0.width = width;
	      rect0.height = height;
	      haverect = gdk_rectangle_intersect(&rect0, area, &rect);
	    }
	  rect.x = x;
	  rect.y = y;
	  rect.width = width;
	  rect.height = height;
	  if ((haverect) && (p))
	    {
	      if (m)
		{
		  gdk_gc_set_clip_mask(gc, m);
		  gdk_gc_set_clip_origin(gc, x, y);
		}
	      rect0.x = rect.x;
	      rect0.y = rect.y;
	      rect0.width = width;
	      rect0.height = im->border.top;
	      if ((area) && (gdk_rectangle_intersect(&rect0, area, &rect1)))
		  gdk_draw_pixmap(window, gc, p,
				  rect1.x - rect0.x, rect1.y - rect0.y,
				  rect1.x, rect1.y,
				  rect1.width, rect1.height);
	      else
		gdk_draw_pixmap(window, gc, p,
				0, 0,
				rect0.x, rect0.y,
				rect0.width, rect0.height);

	      rect0.x = rect.x;
	      rect0.y = rect.y + height - im->border.bottom;
	      rect0.width = width;
	      rect0.height = im->border.bottom;
	      if ((area) && (gdk_rectangle_intersect(&rect0, area, &rect1)))
		gdk_draw_pixmap(window, gc, p,
				rect1.x - rect0.x, rect1.y - rect0.y +
				height - im->border.bottom,
				rect1.x, rect1.y,
				rect1.width, rect1.height);
	      else
		gdk_draw_pixmap(window, gc, p,
				0, height - im->border.bottom,
				rect0.x, rect0.y,
				rect0.width, rect0.height);

	      rect0.x = rect.x;
	      rect0.y = rect.y + im->border.top;
	      rect0.width = im->border.left;
	      rect0.height = height - (im->border.top + im->border.bottom);
	      if ((area) && (gdk_rectangle_intersect(&rect0, area, &rect1)))
		gdk_draw_pixmap(window, gc, p,
				rect1.x - rect0.x, rect1.y - rect0.y +
				im->border.top,
				rect1.x, rect1.y,
				rect1.width, rect1.height);
	      else
		gdk_draw_pixmap(window, gc, p,
				0, im->border.top,
				rect0.x, rect0.y,
				rect0.width, rect0.height);

	      rect0.x = rect.x + width - im->border.right;
	      rect0.y = rect.y + im->border.top;
	      rect0.width = im->border.right;
	      rect0.height = height - (im->border.top + im->border.bottom);
	      if ((area) && (gdk_rectangle_intersect(&rect0, area, &rect1)))
		gdk_draw_pixmap(window, gc, p,
				rect1.x - rect0.x + width - im->border.right, 
				rect1.y - rect0.y + im->border.top,
				rect1.x, rect1.y,
				rect1.width, rect1.height);
	      else
		gdk_draw_pixmap(window, gc, p,
				width - im->border.right, im->border.top,
				rect0.x, rect0.y,
				rect0.width, rect0.height);

	      if (m)
		{
		  gdk_gc_set_clip_mask(gc, NULL);
		  gdk_gc_set_clip_origin(gc, 0, 0);
		}
	      gdk_imlib_free_pixmap(p);
	    }
	  gdk_imlib_destroy_image(im);
	}
    }
}

void
apply_theme_image_shadow_gap(GdkWindow *window, struct theme_image *img, gchar setbg, 
			     GdkGC *gc, GdkRectangle *area, gint x, gint y, gint width, 
			     gint height, gint gap_side, gint gap_x, gint gap_width, 
			     GtkStyle *style)
{
  GdkImlibImage      *im, *im1, *im2;
  GdkPixmap          *p, *m, *p1, *m1, *p2, *m2;
  GdkRectangle        r1, r2;
  GdkRectangle       rect0, rect1, rect;
  gchar              haverect = 1;
  
  if (gap_side == 0)
    /* top */
    {
      r1.x      = x;
      r1.y      = y;
      r1.width  = gap_x;
      r1.height = style->klass->ythickness;
      r2.x      = x + gap_x + gap_width;
      r2.y      = y;
      r2.width  = width - (gap_x + gap_width);
      r2.height = style->klass->ythickness;
    }
  else if (gap_side == 1)
    /* bottom */
    {
      r1.x      = x;
      r1.y      = y + height - style->klass->ythickness;
      r1.width  = gap_x;
      r1.height = style->klass->ythickness;
      r2.x      = x + gap_x + gap_width;
      r2.y      = y + height - style->klass->ythickness;
      r2.width  = width - (gap_x + gap_width);
      r2.height = style->klass->ythickness;
    }
  else if (gap_side == 2)
    /* left */
    {
      r1.x      = x;
      r1.y      = y;
      r1.width  = style->klass->xthickness;
      r1.height = gap_x;
      r2.x      = x;
      r2.y      = y + gap_x + gap_width;
      r2.width  = style->klass->xthickness;
      r2.height = height - (gap_x + gap_width);
    }
  else if (gap_side == 3)
    /* right */
    {
      r1.x      = x + width - style->klass->xthickness;
      r1.y      = y;
      r1.width  = style->klass->xthickness;
      r1.height = gap_x;
      r2.x      = x + width - style->klass->xthickness;
      r2.y      = y + gap_x + gap_width;
      r2.width  = style->klass->xthickness;
      r2.height = height - (gap_x + gap_width);
    }

  if ((img->file) && (img->gap_start_file) && (img->gap_end_file))
    {
      im = load_image(img->file);
      im1 = load_image(img->gap_start_file);
      im2 = load_image(img->gap_end_file);
      if ((im) && (im1) && (im2))
	{  
	  gdk_imlib_set_image_border(im, &(img->border));
	  gdk_imlib_set_image_border(im1, &(img->gap_start_border));
	  gdk_imlib_set_image_border(im2, &(img->gap_end_border));
	  gdk_imlib_render(im, width, height);
	  p = gdk_imlib_move_image(im);
	  m = gdk_imlib_move_mask(im);
	  gdk_imlib_render(im1, r1.width, r1.height);
	  p1 = gdk_imlib_move_image(im1);
	  m1 = gdk_imlib_move_mask(im1);
	  gdk_imlib_render(im2, r2.width, r2.height);
	  p2 = gdk_imlib_move_image(im2);
	  m2 = gdk_imlib_move_mask(im2);
	  
	  if (area)
	    {
	      rect0.x = x;
	      rect0.y = y;
	      rect0.width = width;
	      rect0.height = height;
	      haverect = gdk_rectangle_intersect(&rect0, area, &rect);
	    }
	  rect.x = x;
	  rect.y = y;
	  rect.width = width;
	  rect.height = height;
	  if (p)
	    {
	      if (gap_side == 0)
		/* top */
		{
		  if (m1)
		    {
		      gdk_gc_set_clip_mask(gc, m1);
		      gdk_gc_set_clip_origin(gc, r1.x, r1.y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  if (p1)
		    {
		      rect0.x = r1.x;
		      rect0.y = r1.y;
		      rect0.width = r1.width;
		      rect0.height = r1.height;
		      if (area) 
			if (gdk_rectangle_intersect(&rect0, area, &rect1))
			  gdk_draw_pixmap(window, gc, p1,
					  rect1.x - rect0.x, rect1.y - rect0.y,
					  rect1.x, rect1.y,
					  rect1.width, rect1.height);
		      else
			gdk_draw_pixmap(window, gc, p1,
					0, 0,
					rect0.x, rect0.y,
					rect0.width, rect0.height);
		    }
		  if (m2)
		    {
		      gdk_gc_set_clip_mask(gc, m2);
		      gdk_gc_set_clip_origin(gc, r2.x, r2.y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  if (p2)
		    {
		      rect0.x = r2.x;
		      rect0.y = r2.y;
		      rect0.width = r2.width;
		      rect0.height = r2.height;
		      if (area) 
			if (gdk_rectangle_intersect(&rect0, area, &rect1))
			  gdk_draw_pixmap(window, gc, p2,
					  rect1.x - rect0.x, rect1.y - rect0.y,
					  rect1.x, rect1.y,
					  rect1.width, rect1.height);
		      else
			gdk_draw_pixmap(window, gc, p2,
					0, 0,
					rect0.x, rect0.y,
					rect0.width, rect0.height);
		    }
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, m);
		      gdk_gc_set_clip_origin(gc, x, y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  
		  rect0.x = rect.x;
		  rect0.y = rect.y + height - im->border.bottom;
		  rect0.width = width;
		  rect0.height = im->border.bottom;
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x, rect1.y - rect0.y +
				      height - im->border.bottom,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, height - im->border.bottom,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  rect0.x = rect.x;
		  rect0.y = rect.y + im->border.top;
		  rect0.width = im->border.left;
		  rect0.height = height - (im->border.top + im->border.bottom);
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x,
				      rect1.y - rect0.y + im->border.top,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, im->border.top,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  rect0.x = rect.x + width - im->border.right;
		  rect0.y = rect.y + im->border.top;
		  rect0.width = im->border.right;
		  rect0.height = height - (im->border.top + im->border.bottom);
		  if (area)
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x + width - im->border.right, 
				      rect1.y - rect0.y + im->border.top,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    width - im->border.right, 
				    im->border.top,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		}
	      else if (gap_side == 1)
		/* bottom */
		{
		  if (m1)
		    {
		      gdk_gc_set_clip_mask(gc, m1);
		      gdk_gc_set_clip_origin(gc, r1.x, r1.y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  if (p1)
		    {
		      rect0.x = r1.x;
		      rect0.y = r1.y;
		      rect0.width = r1.width;
		      rect0.height = r1.height;
		      if (area) 
			if (gdk_rectangle_intersect(&rect0, area, &rect1))
			  gdk_draw_pixmap(window, gc, p1,
					  rect1.x - rect0.x, rect1.y - rect0.y,
					  rect1.x, rect1.y,
					  rect1.width, rect1.height);
		      else
			gdk_draw_pixmap(window, gc, p1,
					0, 0,
					rect0.x, rect0.y,
					rect0.width, rect0.height);
		    }
		  if (m2)
		    {
		      gdk_gc_set_clip_mask(gc, m2);
		      gdk_gc_set_clip_origin(gc, r2.x, r2.y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  if (p2)
		    {
		      rect0.x = r2.x;
		      rect0.y = r2.y;
		      rect0.width = r2.width;
		      rect0.height = r2.height;
		      if (area) 
			if (gdk_rectangle_intersect(&rect0, area, &rect1))
			  gdk_draw_pixmap(window, gc, p2,
					  rect1.x - rect0.x, rect1.y - rect0.y,
					  rect1.x, rect1.y,
					  rect1.width, rect1.height);
		      else
			gdk_draw_pixmap(window, gc, p2,
					0, 0,
					rect0.x, rect0.y,
					rect0.width, rect0.height);
		    }
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, m);
		      gdk_gc_set_clip_origin(gc, x, y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  
		  rect0.x = rect.x;
		  rect0.y = rect.y;
		  rect0.width = width;
		  rect0.height = im->border.top;
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x, rect1.y - rect0.y,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, 0,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);

		  rect0.x = rect.x;
		  rect0.y = rect.y + im->border.top;
		  rect0.width = im->border.left;
		  rect0.height = height - (im->border.top + im->border.bottom);
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x,
				      rect1.y - rect0.y + im->border.top,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, im->border.top,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  rect0.x = rect.x + width - im->border.right;
		  rect0.y = rect.y + im->border.top;
		  rect0.width = im->border.right;
		  rect0.height = height - (im->border.top + im->border.bottom);
		  if (area)
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x + width - im->border.right, 
				      rect1.y - rect0.y + im->border.top,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    width - im->border.right, 
				    im->border.top,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		}
	      else if (gap_side == 2)
		/* left */
		{
		  if (m1)
		    {
		      gdk_gc_set_clip_mask(gc, m1);
		      gdk_gc_set_clip_origin(gc, r1.x, r1.y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  if (p1)
		    {
		      rect0.x = r1.x;
		      rect0.y = r1.y;
		      rect0.width = r1.width;
		      rect0.height = r1.height;
		      if (area) 
			if (gdk_rectangle_intersect(&rect0, area, &rect1))
			  gdk_draw_pixmap(window, gc, p1,
					  rect1.x - rect0.x, rect1.y - rect0.y,
					  rect1.x, rect1.y,
					  rect1.width, rect1.height);
		      else
			gdk_draw_pixmap(window, gc, p1,
					0, 0,
					rect0.x, rect0.y,
					rect0.width, rect0.height);
		    }
		  if (m2)
		    {
		      gdk_gc_set_clip_mask(gc, m2);
		      gdk_gc_set_clip_origin(gc, r2.x, r2.y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  if (p2)
		    {
		      rect0.x = r2.x;
		      rect0.y = r2.y;
		      rect0.width = r2.width;
		      rect0.height = r2.height;
		      if (area) 
			if (gdk_rectangle_intersect(&rect0, area, &rect1))
			  gdk_draw_pixmap(window, gc, p2,
					  rect1.x - rect0.x, rect1.y - rect0.y,
					  rect1.x, rect1.y,
					  rect1.width, rect1.height);
		      else
			gdk_draw_pixmap(window, gc, p2,
					0, 0,
					rect0.x, rect0.y,
					rect0.width, rect0.height);
		    }
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, m);
		      gdk_gc_set_clip_origin(gc, x, y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  
		  rect0.x = rect.x;
		  rect0.y = rect.y;
		  rect0.width = width;
		  rect0.height = im->border.top;
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x, rect1.y - rect0.y,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, 0,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);

		  rect0.x = rect.x;
		  rect0.y = rect.y + height - im->border.bottom;
		  rect0.width = width;
		  rect0.height = im->border.bottom;
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x, rect1.y - rect0.y +
				      height - im->border.bottom,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, height - im->border.bottom,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  rect0.x = rect.x + width - im->border.right;
		  rect0.y = rect.y + im->border.top;
		  rect0.width = im->border.right;
		  rect0.height = height - (im->border.top + im->border.bottom);
		  if (area)
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x + width - im->border.right, 
				      rect1.y - rect0.y + im->border.top,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    width - im->border.right, 
				    im->border.top,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		}
	      else if (gap_side == 3)
		/* right */
		{
		  if (m1)
		    {
		      gdk_gc_set_clip_mask(gc, m1);
		      gdk_gc_set_clip_origin(gc, r1.x, r1.y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  if (p1)
		    {
		      rect0.x = r1.x;
		      rect0.y = r1.y;
		      rect0.width = r1.width;
		      rect0.height = r1.height;
		      if (area) 
			if (gdk_rectangle_intersect(&rect0, area, &rect1))
			  gdk_draw_pixmap(window, gc, p1,
					  rect1.x - rect0.x, rect1.y - rect0.y,
					  rect1.x, rect1.y,
					  rect1.width, rect1.height);
		      else
			gdk_draw_pixmap(window, gc, p1,
					0, 0,
					rect0.x, rect0.y,
					rect0.width, rect0.height);
		    }
		  if (m2)
		    {
		      gdk_gc_set_clip_mask(gc, m2);
		      gdk_gc_set_clip_origin(gc, r2.x, r2.y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  if (p2)
		    {
		      rect0.x = r2.x;
		      rect0.y = r2.y;
		      rect0.width = r2.width;
		      rect0.height = r2.height;
		      if (area) 
			if (gdk_rectangle_intersect(&rect0, area, &rect1))
			  gdk_draw_pixmap(window, gc, p2,
					  rect1.x - rect0.x, rect1.y - rect0.y,
					  rect1.x, rect1.y,
					  rect1.width, rect1.height);
		      else
			gdk_draw_pixmap(window, gc, p2,
					0, 0,
					rect0.x, rect0.y,
					rect0.width, rect0.height);
		    }
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, m);
		      gdk_gc_set_clip_origin(gc, x, y);
		    }
		  else
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		  
		  rect0.x = rect.x;
		  rect0.y = rect.y;
		  rect0.width = width;
		  rect0.height = im->border.top;
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x, rect1.y - rect0.y,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, 0,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);

		  rect0.x = rect.x;
		  rect0.y = rect.y + height - im->border.bottom;
		  rect0.width = width;
		  rect0.height = im->border.bottom;
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x, rect1.y - rect0.y +
				      height - im->border.bottom,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, height - im->border.bottom,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  rect0.x = rect.x;
		  rect0.y = rect.y + im->border.top;
		  rect0.width = im->border.left;
		  rect0.height = height - (im->border.top + im->border.bottom);
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p,
				      rect1.x - rect0.x,
				      rect1.y - rect0.y + im->border.top,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p,
				    0, im->border.top,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		  
		  if (m)
		    {
		      gdk_gc_set_clip_mask(gc, NULL);
		      gdk_gc_set_clip_origin(gc, 0, 0);
		    }
		}
	      gdk_imlib_free_pixmap(p);
	      gdk_imlib_free_pixmap(p1);
	      gdk_imlib_free_pixmap(p2);
	    }
	  gdk_imlib_destroy_image(im);
	  gdk_imlib_destroy_image(im1);
	  gdk_imlib_destroy_image(im2);
	}
    }
}

void
apply_theme_image_box_gap(GdkWindow *window, struct theme_image *img, gchar setbg, 
			  GdkGC *gc, GdkRectangle *area, gint x, gint y, gint width, 
			  gint height, gint gap_side, gint gap_x, gint gap_width, 
			  GtkStyle *style)
{
  GdkImlibImage      *im, *im1, *im2, *im3;
  GdkPixmap          *p, *m, *p1, *m1, *p2, *m2, *p3, *m3;
  GdkRectangle        r1, r2, r3;
  GdkRectangle       rect0, rect1, rect;
  gchar              haverect = 1;
  
  if (gap_side == 0)
    /* top */
    {
      r1.x      = x;
      r1.y      = y;
      r1.width  = gap_x;
      r1.height = style->klass->ythickness;
      r2.x      = x + gap_x + gap_width;
      r2.y      = y;
      r2.width  = width - (gap_x + gap_width);
      r2.height = style->klass->ythickness;
      r3.x      = x + gap_x;
      r3.y      = y;
      r3.width  = gap_width;
      r3.height = style->klass->ythickness;
    }
  else if (gap_side == 1)
    /* bottom */
    {
      r1.x      = x;
      r1.y      = y + height - style->klass->ythickness;
      r1.width  = gap_x;
      r1.height = style->klass->ythickness;
      r2.x      = x + gap_x + gap_width;
      r2.y      = y + height - style->klass->ythickness;
      r2.width  = width - (gap_x + gap_width);
      r2.height = style->klass->ythickness;
      r3.x      = x + gap_x;
      r3.y      = y + height - style->klass->ythickness;
      r3.width  = gap_width;
      r3.height = style->klass->ythickness;
    }
  else if (gap_side == 2)
    /* left */
    {
      r1.x      = x;
      r1.y      = y;
      r1.width  = style->klass->xthickness;
      r1.height = gap_x;
      r2.x      = x;
      r2.y      = y + gap_x + gap_width;
      r2.width  = style->klass->xthickness;
      r2.height = height - (gap_x + gap_width);
      r3.x      = x;
      r3.y      = y + gap_x;
      r3.width  = style->klass->xthickness;
      r3.height = gap_width;
    }
  else if (gap_side == 3)
    /* right */
    {
      r1.x      = x + width - style->klass->xthickness;
      r1.y      = y;
      r1.width  = style->klass->xthickness;
      r1.height = gap_x;
      r2.x      = x + width - style->klass->xthickness;
      r2.y      = y + gap_x + gap_width;
      r2.width  = style->klass->xthickness;
      r2.height = height - (gap_x + gap_width);
      r3.x      = x + width - style->klass->xthickness;
      r3.y      = y + gap_x;
      r3.width  = style->klass->xthickness;
      r3.height = gap_width;
    }

  if ((img->file) && (img->gap_start_file) && (img->gap_end_file) &&
      (img->gap_file))
    {
      im = load_image(img->file);
      im1 = load_image(img->gap_start_file);
      im2 = load_image(img->gap_end_file);
      im3 = load_image(img->gap_file);
      if ((im) && (im1) && (im2) && (im3))
	{  
	  gdk_imlib_set_image_border(im, &(img->border));
	  gdk_imlib_set_image_border(im1, &(img->gap_start_border));
	  gdk_imlib_set_image_border(im2, &(img->gap_end_border));
	  gdk_imlib_set_image_border(im3, &(img->gap_border));
	  gdk_imlib_render(im, width, height);
	  p = gdk_imlib_move_image(im);
	  m = gdk_imlib_move_mask(im);
	  gdk_imlib_render(im1, r1.width, r1.height);
	  p1 = gdk_imlib_move_image(im1);
	  m1 = gdk_imlib_move_mask(im1);
	  gdk_imlib_render(im2, r2.width, r2.height);
	  p2 = gdk_imlib_move_image(im2);
	  m2 = gdk_imlib_move_mask(im2);
	  gdk_imlib_render(im3, r3.width, r3.height);
	  p3 = gdk_imlib_move_image(im3);
	  m3 = gdk_imlib_move_mask(im3);
	  
	  if (area)
	    {
	      rect0.x = x;
	      rect0.y = y;
	      rect0.width = width;
	      rect0.height = height;
	      haverect = gdk_rectangle_intersect(&rect0, area, &rect);
	    }
	  rect.x = x;
	  rect.y = y;
	  rect.width = width;
	  rect.height = height;
	  if ((p) && (haverect))
	    {
	      if (m)
		{
		  gdk_gc_set_clip_mask(gc, m);
		  gdk_gc_set_clip_origin(gc, x, y);
		}
	      else
		{
		  gdk_gc_set_clip_mask(gc, NULL);
		  gdk_gc_set_clip_origin(gc, 0, 0);
		}
	      rect0.x = rect.x;
	      rect0.y = rect.y;
	      rect0.width = width;
	      rect0.height = height;
	      if (area) 
		if (gdk_rectangle_intersect(&rect0, area, &rect1))
		  gdk_draw_pixmap(window, gc, p,
				  rect1.x - rect0.x, rect1.y - rect0.y,
				  rect1.x, rect1.y,
				  rect1.width, rect1.height);
	      else
		gdk_draw_pixmap(window, gc, p,
				0, 0,
				rect0.x, rect0.y,
				rect0.width, rect0.height);
	      if (m1)
		{
		  gdk_gc_set_clip_mask(gc, m1);
		  gdk_gc_set_clip_origin(gc, r1.x, r1.y);
		}
	      else
		{
		  gdk_gc_set_clip_mask(gc, NULL);
		  gdk_gc_set_clip_origin(gc, 0, 0);
		}
	      if (p1)
		{
		  rect0.x = r1.x;
		  rect0.y = r1.y;
		  rect0.width = r1.width;
		  rect0.height = r1.height;
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p1,
				      rect1.x - rect0.x, rect1.y - rect0.y,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p1,
				    0, 0,
				    rect0.x, rect0.y,
				    rect0.width, rect0.height);
		}
	      if (m2)
		{
		  gdk_gc_set_clip_mask(gc, m2);
		  gdk_gc_set_clip_origin(gc, r2.x, r2.y);
		}
	      else
		{
		  gdk_gc_set_clip_mask(gc, NULL);
		  gdk_gc_set_clip_origin(gc, 0, 0);
		}
	      if (p2)
		{
		  rect0.x = r2.x;
		  rect0.y = r2.y;
		  rect0.width = r2.width;
		  rect0.height = r2.height;
		  if (area) 
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p2,
				      rect1.x - rect0.x, rect1.y - rect0.y,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p2,
				    rect0.x - x, rect0.y - y,
				    0, 0,
				    rect0.width, rect0.height);
		}
	      if (m3)
		{
		  gdk_gc_set_clip_mask(gc, m3);
		  gdk_gc_set_clip_origin(gc, r3.x, r3.y);
		}
	      else
		{
		  gdk_gc_set_clip_mask(gc, NULL);
		  gdk_gc_set_clip_origin(gc, 0, 0);
		}
	      if (p3)
		{
		  rect0.x = r3.x;
		  rect0.y = r3.y;
		  rect0.width = r3.width;
		  rect0.height = r3.height;
		  if (area)
		    if (gdk_rectangle_intersect(&rect0, area, &rect1))
		      gdk_draw_pixmap(window, gc, p3,
				      rect1.x - rect0.x, rect1.y - rect0.y,
				      rect1.x, rect1.y,
				      rect1.width, rect1.height);
		  else
		    gdk_draw_pixmap(window, gc, p3,
				    rect0.x - x, rect0.y - y,
				    0, 0,
				    rect0.width, rect0.height);
		}
	      if (m3)
		{
		  gdk_gc_set_clip_mask(gc, NULL);
		  gdk_gc_set_clip_origin(gc, 0, 0);
		}
	      gdk_imlib_free_pixmap(p);
	      gdk_imlib_free_pixmap(p1);
	      gdk_imlib_free_pixmap(p2);
	      gdk_imlib_free_pixmap(p3);
	    }
	  gdk_imlib_destroy_image(im);
	  gdk_imlib_destroy_image(im1);
	  gdk_imlib_destroy_image(im2);
	  gdk_imlib_destroy_image(im3);
	}
    }
}

void
draw_hline(GtkStyle * style,
	   GdkWindow * window,
	   GtkStateType state_type,
	   GdkRectangle * area,
	   GtkWidget * widget,
	   gchar * detail,
	   gint x1,
	   gint x2,
	   gint y)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  
  img = match_theme_image(style,
			  state_type,
			  GTK_SHADOW_IN,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_HLINE);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x1, y, (x2 - x1) + 1, 2);
    }
}

void
draw_vline(GtkStyle * style,
	   GdkWindow * window,
	   GtkStateType state_type,
	   GdkRectangle * area,
	   GtkWidget * widget,
	   gchar * detail,
	   gint y1,
	   gint y2,
	   gint x)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  GTK_SHADOW_IN,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_VLINE);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y1, 2, (y2 - y1) + 1);
    }
}

void
draw_shadow(GtkStyle * style,
	    GdkWindow * window,
	    GtkStateType state_type,
	    GtkShadowType shadow_type,
	    GdkRectangle * area,
	    GtkWidget * widget,
	    gchar * detail,
	    gint x,
	    gint y,
	    gint width,
	    gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  GTK_ORIENTATION_HORIZONTAL,
			  0,
			  TOKEN_D_SHADOW);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image_border(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_polygon(GtkStyle * style,
	     GdkWindow * window,
	     GtkStateType state_type,
	     GtkShadowType shadow_type,
	     GdkRectangle * area,
	     GtkWidget * widget,
	     gchar * detail,
	     GdkPoint * points,
	     gint npoints,
	     gint fill)
{
#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif /* M_PI */
#ifndef M_PI_4
#define M_PI_4  0.78539816339744830962
#endif /* M_PI_4 */

  static const gdouble pi_over_4 = M_PI_4;
  static const gdouble pi_3_over_4 = M_PI_4 * 3;

  GdkGC              *gc3;
  GdkGC              *gc4;
  gdouble             angle;
  gint                i;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);
  g_return_if_fail(points != NULL);

  switch (shadow_type)
    {
    case GTK_SHADOW_IN:
      gc3 = style->light_gc[state_type];
      gc4 = style->black_gc;
      break;
    case GTK_SHADOW_OUT:
      gc3 = style->black_gc;
      gc4 = style->light_gc[state_type];
      break;
    default:
      return;
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle(gc3, area);
      gdk_gc_set_clip_rectangle(gc4, area);
    }
  if (fill)
    gdk_draw_polygon(window, style->bg_gc[state_type], TRUE, points, npoints);

  npoints--;

  for (i = 0; i < npoints; i++)
    {
      if ((points[i].x == points[i + 1].x) &&
	  (points[i].y == points[i + 1].y))
	angle = 0;
      else
	angle = atan2(points[i + 1].y - points[i].y,
		      points[i + 1].x - points[i].x);

      if ((angle > -pi_3_over_4) && (angle < pi_over_4))
	gdk_draw_line(window, gc3,
		      points[i].x, points[i].y,
		      points[i + 1].x, points[i + 1].y);
      else
	gdk_draw_line(window, gc4,
		      points[i].x, points[i].y,
		      points[i + 1].x, points[i + 1].y);
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(gc3, NULL);
      gdk_gc_set_clip_rectangle(gc4, NULL);
    }
}

void
draw_arrow(GtkStyle * style,
	   GdkWindow * window,
	   GtkStateType state_type,
	   GtkShadowType shadow_type,
	   GdkRectangle * area,
	   GtkWidget * widget,
	   gchar * detail,
	   GtkArrowType arrow_type,
	   gint fill,
	   gint x,
	   gint y,
	   gint width,
	   gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  arrow_type,
			  orientation,
			  0,
			  TOKEN_D_ARROW);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_diamond(GtkStyle * style,
	     GdkWindow * window,
	     GtkStateType state_type,
	     GtkShadowType shadow_type,
	     GdkRectangle * area,
	     GtkWidget * widget,
	     gchar * detail,
	     gint x,
	     gint y,
	     gint width,
	     gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_DIAMOND);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_oval(GtkStyle * style,
	  GdkWindow * window,
	  GtkStateType state_type,
	  GtkShadowType shadow_type,
	  GdkRectangle * area,
	  GtkWidget * widget,
	  gchar * detail,
	  gint x,
	  gint y,
	  gint width,
	  gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_OVAL);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_string(GtkStyle * style,
	    GdkWindow * window,
	    GtkStateType state_type,
	    GdkRectangle * area,
	    GtkWidget * widget,
	    gchar * detail,
	    gint x,
	    gint y,
	    const gchar * string)
{
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if (area)
    {
      gdk_gc_set_clip_rectangle(style->white_gc, area);
      gdk_gc_set_clip_rectangle(style->fg_gc[state_type], area);
    }
  if (state_type == GTK_STATE_INSENSITIVE)
    gdk_draw_string(window, style->font, style->white_gc, x + 1, y + 1, string);
  gdk_draw_string(window, style->font, style->fg_gc[state_type], x, y, string);
  if (area)
    {
      gdk_gc_set_clip_rectangle(style->white_gc, NULL);
      gdk_gc_set_clip_rectangle(style->fg_gc[state_type], NULL);
    }
}

void
draw_box(GtkStyle * style,
	 GdkWindow * window,
	 GtkStateType state_type,
	 GtkShadowType shadow_type,
	 GdkRectangle * area,
	 GtkWidget * widget,
	 gchar * detail,
	 gint x,
	 gint y,
	 gint width,
	 gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;

  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_BOX);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_flat_box(GtkStyle * style,
	      GdkWindow * window,
	      GtkStateType state_type,
	      GtkShadowType shadow_type,
	      GdkRectangle * area,
	      GtkWidget * widget,
	      gchar * detail,
	      gint x,
	      gint y,
	      gint width,
	      gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_FLAT_BOX);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_check(GtkStyle * style,
	   GdkWindow * window,
	   GtkStateType state_type,
	   GtkShadowType shadow_type,
	   GdkRectangle * area,
	   GtkWidget * widget,
	   gchar * detail,
	   gint x,
	   gint y,
	   gint width,
	   gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_CHECK);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_option(GtkStyle * style,
	    GdkWindow * window,
	    GtkStateType state_type,
	    GtkShadowType shadow_type,
	    GdkRectangle * area,
	    GtkWidget * widget,
	    gchar * detail,
	    gint x,
	    gint y,
	    gint width,
	    gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_OPTION);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_cross(GtkStyle * style,
	   GdkWindow * window,
	   GtkStateType state_type,
	   GtkShadowType shadow_type,
	   GdkRectangle * area,
	   GtkWidget * widget,
	   gchar * detail,
	   gint x,
	   gint y,
	   gint width,
	   gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_CROSS);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_ramp(GtkStyle * style,
	  GdkWindow * window,
	  GtkStateType state_type,
	  GtkShadowType shadow_type,
	  GdkRectangle * area,
	  GtkWidget * widget,
	  gchar * detail,
	  GtkArrowType arrow_type,
	  gint x,
	  gint y,
	  gint width,
	  gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  arrow_type,
			  orientation,
			  0,
			  TOKEN_D_RAMP);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_tab(GtkStyle * style,
	 GdkWindow * window,
	 GtkStateType state_type,
	 GtkShadowType shadow_type,
	 GdkRectangle * area,
	 GtkWidget * widget,
	 gchar * detail,
	 gint x,
	 gint y,
	 gint width,
	 gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_TAB);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_shadow_gap(GtkStyle * style,
		GdkWindow * window,
		GtkStateType state_type,
		GtkShadowType shadow_type,
		GdkRectangle * area,
		GtkWidget * widget,
		gchar * detail,
		gint x,
		gint y,
		gint width,
		gint height,
		gint gap_side,
		gint gap_x,
		gint gap_width)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_SHADOW_GAP);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image_shadow_gap(window, img, setbg, gc, area, x, y, width, 
				   height, gap_side, gap_x, gap_width, style);
    }
}

void
draw_box_gap(GtkStyle * style,
	     GdkWindow * window,
	     GtkStateType state_type,
	     GtkShadowType shadow_type,
	     GdkRectangle * area,
	     GtkWidget * widget,
	     gchar * detail,
	     gint x,
	     gint y,
	     gint width,
	     gint height,
	     gint gap_side,
	     gint gap_x,
	     gint gap_width)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      setbg = 1;
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  gap_side,
			  TOKEN_D_BOX_GAP);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image_box_gap(window, img, setbg, gc, area, x, y, width, 
				height, gap_side, gap_x, gap_width, style);
    }
}

void
draw_extension(GtkStyle * style,
	       GdkWindow * window,
	       GtkStateType state_type,
	       GtkShadowType shadow_type,
	       GdkRectangle * area,
	       GtkWidget * widget,
	       gchar * detail,
	       gint x,
	       gint y,
	       gint width,
	       gint height,
	       gint gap_side)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if (width >=0)
    width++;
  if (height >=0)
    height++;
  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);
  
  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  gap_side,
			  TOKEN_D_EXTENSION);
  if (img)
    {
      gc = style->bg_gc[GTK_STATE_NORMAL];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_focus(GtkStyle * style,
	   GdkWindow * window,
	   GdkRectangle * area,
	   GtkWidget * widget,
	   gchar * detail,
	   gint x,
	   gint y,
	   gint width,
	   gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  GtkOrientation      orientation;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if (width >=0)
    width++;
  if (height >=0)
    height++;
  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);
  
  orientation = GTK_ORIENTATION_HORIZONTAL;
  if (height > width)
    orientation = GTK_ORIENTATION_VERTICAL;
  
  img = match_theme_image(style,
			  GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_FOCUS);
  if (img)
    {
      gc = style->bg_gc[GTK_STATE_NORMAL];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_slider(GtkStyle * style,
	    GdkWindow * window,
	    GtkStateType state_type,
	    GtkShadowType shadow_type,
	    GdkRectangle * area,
	    GtkWidget * widget,
	    gchar * detail,
	    gint x,
	    gint y,
	    gint width,
	    gint height,
	    GtkOrientation orientation)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      setbg = 1;
      gdk_window_get_size(window, &width, &height);
    }
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_SLIDER);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}

void
draw_entry(GtkStyle * style,
	   GdkWindow * window,
	   GtkStateType state_type,
	   GdkRectangle * area,
	   GtkWidget * widget,
	   gchar * detail,
	   gint x,
	   gint y,
	   gint width,
	   gint height)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  if (area)
    gdk_gc_set_clip_rectangle(style->base_gc[state_type], area);

  if (!strcmp("selected", detail))
    {
      if (state_type == GTK_STATE_ACTIVE)
	draw_flat_box(style, window, GTK_STATE_INSENSITIVE, GTK_SHADOW_NONE,
		      area, widget, detail,
		      x, y, width, height);
      else
	draw_flat_box(style, window, state_type, GTK_SHADOW_NONE,
		      area, widget, detail,
		      x, y, width, height);
    }
  else
    {
      img = match_theme_image(style,
			      state_type,
			      GTK_SHADOW_NONE,
			      widget,
			      detail,
			      GTK_ARROW_UP,
			      GTK_ORIENTATION_HORIZONTAL,
			      0,
			      TOKEN_D_ENTRY);
      if (img)
	{
	  gc = style->bg_gc[state_type];
	  apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
	}
    }
}

void
draw_handle(GtkStyle * style,
	    GdkWindow * window,
	    GtkStateType state_type,
	    GtkShadowType shadow_type,
	    GdkRectangle * area,
	    GtkWidget * widget,
	    gchar * detail,
	    gint x,
	    gint y,
	    gint width,
	    gint height,
	    GtkOrientation orientation)
{
  struct theme_image *img;
  GdkGC              *gc;
  gchar               setbg = 0;
  
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  img = match_theme_image(style,
			  state_type,
			  shadow_type,
			  widget,
			  detail,
			  GTK_ARROW_UP,
			  orientation,
			  0,
			  TOKEN_D_HANDLE);
  if (img)
    {
      gc = style->bg_gc[state_type];
      apply_theme_image(window, img, setbg, gc, area, x, y, width, height);
    }
}
