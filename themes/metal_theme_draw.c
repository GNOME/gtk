#include <gtk/gtk.h>

extern GtkStyleClass th_default_class;
extern GdkFont     *default_font;
extern GSList      *unattached_styles;

/*
 * Java Metal Theme
 * Randy Gordon, rangor@ix.netcom.com
 * A work in progress...
 */

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

/**************************************************************************/
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
  gint                thickness_light;
  gint                thickness_dark;
  gint                i;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  thickness_light = style->klass->ythickness / 2;
  thickness_dark = style->klass->ythickness - thickness_light;

  if (area)
    {
      gdk_gc_set_clip_rectangle(style->light_gc[state_type], area);
      gdk_gc_set_clip_rectangle(style->dark_gc[state_type], area);
    }
  for (i = 0; i < thickness_dark; i++)
    {
      gdk_draw_line(window, style->light_gc[state_type], x2 - i - 1, y + i, x2, y + i);
      gdk_draw_line(window, style->dark_gc[state_type], x1, y + i, x2 - i - 1, y + i);
    }

  y += thickness_dark;
  for (i = 0; i < thickness_light; i++)
    {
      gdk_draw_line(window, style->dark_gc[state_type], x1, y + i, x1 + thickness_light - i - 1, y + i);
      gdk_draw_line(window, style->light_gc[state_type], x1 + thickness_light - i - 1, y + i, x2, y + i);
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(style->light_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle(style->dark_gc[state_type], NULL);
    }
}

/**************************************************************************/
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
  gint                thickness_light;
  gint                thickness_dark;
  gint                i;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  thickness_light = style->klass->xthickness / 2;
  thickness_dark = style->klass->xthickness - thickness_light;

  if (area)
    {
      gdk_gc_set_clip_rectangle(style->light_gc[state_type], area);
      gdk_gc_set_clip_rectangle(style->dark_gc[state_type], area);
    }
  for (i = 0; i < thickness_dark; i++)
    {
      gdk_draw_line(window, style->light_gc[state_type], x + i, y2 - i - 1, x + i, y2);
      gdk_draw_line(window, style->dark_gc[state_type], x + i, y1, x + i, y2 - i - 1);
    }

  x += thickness_dark;
  for (i = 0; i < thickness_light; i++)
    {
      gdk_draw_line(window, style->dark_gc[state_type], x + i, y1, x + i, y1 + thickness_light - i);
      gdk_draw_line(window, style->light_gc[state_type], x + i, y1 + thickness_light - i, x + i, y2);
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(style->light_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle(style->dark_gc[state_type], NULL);
    }
}

/**************************************************************************/
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
  GdkGC              *gc1 = NULL;
  GdkGC              *gc2 = NULL;
  gint                thickness_light;
  gint                thickness_dark;
  gint                i;

printf("DETAIL = %s\n", detail);

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  // Override shadow-type for Metal button
  if ((detail) && (!strcmp(detail, "button") || !strcmp(detail, "buttondefault")))
    shadow_type = GTK_SHADOW_ETCHED_IN;
  if ((detail) && (!strcmp(detail, "optionmenu")))
    shadow_type = GTK_SHADOW_ETCHED_IN;

  // Short-circuit some metal styles for now
  if ((detail) && (!strcmp(detail, "frame"))) {
    gc1 = style->dark_gc[state_type];
    gdk_gc_set_clip_rectangle(gc1, NULL);
    if (area) gdk_gc_set_clip_rectangle(gc1, area);
    gdk_draw_rectangle(window, gc1, FALSE, x, y, width-1, height-1);
    return; //tbd 
  }
  if ((detail) && (!strcmp(detail, "optionmenutab"))) {
    gc1 = style->black_gc;
    gdk_draw_line(window, gc1, x, y, x+10, y);
    gdk_draw_line(window, gc1, x+1, y+1, x+9, y+1);
    gdk_draw_line(window, gc1, x+2, y+2, x+8, y+2);
    gdk_draw_line(window, gc1, x+3, y+3, x+7, y+3);
    gdk_draw_line(window, gc1, x+4, y+4, x+6, y+4);
    gdk_draw_line(window, gc1, x+5, y+5, x+5, y+4);
    return; //tbd 
  }

  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      gc1 = NULL;
      gc2 = NULL;
      break;
    case GTK_SHADOW_IN:
    case GTK_SHADOW_ETCHED_IN:
      gc1 = style->light_gc[state_type];
      gc2 = style->dark_gc[state_type];
      break;
    case GTK_SHADOW_OUT:
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      break;
    }

  gdk_gc_set_clip_rectangle(gc1, NULL);
  gdk_gc_set_clip_rectangle(gc2, NULL);
  if ((shadow_type == GTK_SHADOW_IN) ||
      (shadow_type == GTK_SHADOW_OUT))
    {
      gdk_gc_set_clip_rectangle(style->black_gc, NULL);
      gdk_gc_set_clip_rectangle(style->bg_gc[state_type], NULL);
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(gc1, area);
      gdk_gc_set_clip_rectangle(gc2, area);
      if ((shadow_type == GTK_SHADOW_IN) ||
	  (shadow_type == GTK_SHADOW_OUT))
	{
	  gdk_gc_set_clip_rectangle(style->black_gc, area);
	  gdk_gc_set_clip_rectangle(style->bg_gc[state_type], area);
	}
    }
  switch (shadow_type)
    {
    case GTK_SHADOW_NONE:
      break;
    case GTK_SHADOW_IN:
      gdk_draw_line(window, gc1,
		    x, y + height - 1, x + width - 1, y + height - 1);
      gdk_draw_line(window, gc1,
		    x + width - 1, y, x + width - 1, y + height - 1);

      gdk_draw_line(window, style->bg_gc[state_type],
		    x + 1, y + height - 2, x + width - 2, y + height - 2);
      gdk_draw_line(window, style->bg_gc[state_type],
		    x + width - 2, y + 1, x + width - 2, y + height - 2);

      gdk_draw_line(window, style->black_gc,
		    x + 1, y + 1, x + width - 2, y + 1);
      gdk_draw_line(window, style->black_gc,
		    x + 1, y + 1, x + 1, y + height - 2);

      gdk_draw_line(window, gc2,
		    x, y, x + width - 1, y);
      gdk_draw_line(window, gc2,
		    x, y, x, y + height - 1);
      break;

    case GTK_SHADOW_OUT:
      gdk_draw_line(window, gc1,
		    x + 1, y + height - 2, x + width - 2, y + height - 2);
      gdk_draw_line(window, gc1,
		    x + width - 2, y + 1, x + width - 2, y + height - 2);

      gdk_draw_line(window, gc2,
		    x, y, x + width - 1, y);
      gdk_draw_line(window, gc2,
		    x, y, x, y + height - 1);

      gdk_draw_line(window, style->bg_gc[state_type],
		    x + 1, y + 1, x + width - 2, y + 1);
      gdk_draw_line(window, style->bg_gc[state_type],
		    x + 1, y + 1, x + 1, y + height - 2);

      gdk_draw_line(window, style->black_gc,
		    x, y + height - 1, x + width - 1, y + height - 1);
      gdk_draw_line(window, style->black_gc,
		    x + width - 1, y, x + width - 1, y + height - 1);
      break;
    case GTK_SHADOW_ETCHED_IN:
    case GTK_SHADOW_ETCHED_OUT:
      thickness_light = 1;
      thickness_dark = 1;

      for (i = 0; i < thickness_dark; i++)
	{
	  gdk_draw_line(window, gc1,
			x + i,
			y + height - i - 1,
			x + width - i - 1,
			y + height - i - 1);
	  gdk_draw_line(window, gc1,
			x + width - i - 1,
			y + i,
			x + width - i - 1,
			y + height - i - 1);

	  gdk_draw_line(window, gc2,
			x + i,
			y + i,
			x + width - i - 2,
			y + i);
	  gdk_draw_line(window, gc2,
			x + i,
			y + i,
			x + i,
			y + height - i - 2);
	}

      for (i = 0; i < thickness_light; i++)
	{
	  gdk_draw_line(window, gc1,
			x + thickness_dark + i,
			y + thickness_dark + i,
			x + width - thickness_dark - i - 1,
			y + thickness_dark + i);
	  gdk_draw_line(window, gc1,
			x + thickness_dark + i,
			y + thickness_dark + i,
			x + thickness_dark + i,
			y + height - thickness_dark - i - 1);

	  gdk_draw_line(window, gc2,
			x + thickness_dark + i,
			y + height - thickness_light - i - 1,
			x + width - thickness_light - 1,
			y + height - thickness_light - i - 1);
	  gdk_draw_line(window, gc2,
			x + width - thickness_light - i - 1,
			y + thickness_dark + i,
			x + width - thickness_light - i - 1,
			y + height - thickness_light - 1);
	}
      break;
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(gc1, NULL);
      gdk_gc_set_clip_rectangle(gc2, NULL);
      if ((shadow_type == GTK_SHADOW_IN) ||
	  (shadow_type == GTK_SHADOW_OUT))
	{
	  gdk_gc_set_clip_rectangle(style->black_gc, NULL);
	  gdk_gc_set_clip_rectangle(style->bg_gc[state_type], NULL);
	}
    }
}

/**************************************************************************/
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

  GdkGC              *gc1;
  GdkGC              *gc2;
  GdkGC              *gc3;
  GdkGC              *gc4;
  gdouble             angle;
  gint                xadjust;
  gint                yadjust;
  gint                i;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);
  g_return_if_fail(points != NULL);

  switch (shadow_type)
    {
    case GTK_SHADOW_IN:
      gc1 = style->bg_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = style->light_gc[state_type];
      gc4 = style->black_gc;
      break;
    case GTK_SHADOW_ETCHED_IN:
      gc1 = style->light_gc[state_type];
      gc2 = style->dark_gc[state_type];
      gc3 = style->dark_gc[state_type];
      gc4 = style->light_gc[state_type];
      break;
    case GTK_SHADOW_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->black_gc;
      gc4 = style->bg_gc[state_type];
      break;
    case GTK_SHADOW_ETCHED_OUT:
      gc1 = style->dark_gc[state_type];
      gc2 = style->light_gc[state_type];
      gc3 = style->light_gc[state_type];
      gc4 = style->dark_gc[state_type];
      break;
    default:
      return;
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle(gc1, area);
      gdk_gc_set_clip_rectangle(gc2, area);
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
	{
	  angle = 0;
	}
      else
	{
	  angle = atan2(points[i + 1].y - points[i].y,
			points[i + 1].x - points[i].x);
	}

      if ((angle > -pi_3_over_4) && (angle < pi_over_4))
	{
	  if (angle > -pi_over_4)
	    {
	      xadjust = 0;
	      yadjust = 1;
	    }
	  else
	    {
	      xadjust = 1;
	      yadjust = 0;
	    }

	  gdk_draw_line(window, gc1,
			points[i].x - xadjust, points[i].y - yadjust,
			points[i + 1].x - xadjust, points[i + 1].y - yadjust);
	  gdk_draw_line(window, gc3,
			points[i].x, points[i].y,
			points[i + 1].x, points[i + 1].y);
	}
      else
	{
	  if ((angle < -pi_3_over_4) || (angle > pi_3_over_4))
	    {
	      xadjust = 0;
	      yadjust = 1;
	    }
	  else
	    {
	      xadjust = 1;
	      yadjust = 0;
	    }

	  gdk_draw_line(window, gc4,
			points[i].x + xadjust, points[i].y + yadjust,
			points[i + 1].x + xadjust, points[i + 1].y + yadjust);
	  gdk_draw_line(window, gc2,
			points[i].x, points[i].y,
			points[i + 1].x, points[i + 1].y);
	}
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(gc1, NULL);
      gdk_gc_set_clip_rectangle(gc2, NULL);
      gdk_gc_set_clip_rectangle(gc3, NULL);
      gdk_gc_set_clip_rectangle(gc4, NULL);
    }
}

/**************************************************************************/
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
  GdkGC              *gc;
  gint                half_width;
  gint                half_height;
  gint                xthik, ythik;
  GdkPoint            points[3];
  gchar               border = 1;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  xthik = style->klass->xthickness;
  ythik = style->klass->ythickness;

  gc = style->black_gc;

  if ((detail) && (!strcmp(detail, "menuitem")))
    {
      border = 0;
      gc = style->fg_gc[state_type];
    }
  if ((fill) && (border))
    draw_box(style, window, state_type, shadow_type, area, widget, detail,
	     x, y, width, height);

  if (area)
    {
      gdk_gc_set_clip_rectangle(gc, area);
    }
  if (!border)
    {
      x += 1;
      y += 1;
      width -= 2;
      height -= 2;
    }
  else
    {
      x += xthik;
      y += ythik;
      width -= xthik * 2;
      height -= ythik * 2;
    }

  if (!(width & 1))
    width--;
  if (!(height & 1))
    height--;

  half_width = width / 2;
  half_height = height / 2;

  switch (arrow_type)
    {
    case GTK_ARROW_UP:
      points[0].x = x;
      points[0].y = y + half_height + (half_width / 2);
      points[1].x = x + width - 1;
      points[1].y = y + half_height + (half_width / 2);
      points[2].x = x + half_width;
      points[2].y = y + half_height - (half_width / 2);

      gdk_draw_polygon(window, gc, TRUE, points, 3);
      gdk_draw_polygon(window, gc, FALSE, points, 3);
      break;
    case GTK_ARROW_DOWN:
      points[0].x = x;
      points[0].y = y + half_height - (half_width / 2);
      points[1].x = x + width - 1;
      points[1].y = y + half_height - (half_width / 2);
      points[2].x = x + half_width;
      points[2].y = y + half_height + (half_width / 2);

      gdk_draw_polygon(window, gc, TRUE, points, 3);
      gdk_draw_polygon(window, gc, FALSE, points, 3);
      break;
    case GTK_ARROW_LEFT:
      points[0].x = x + half_width + (half_height / 2);
      points[0].y = y;
      points[1].x = x + half_width + (half_height / 2);
      points[1].y = y + height - 1;
      points[2].x = x + half_width - (half_height / 2);
      points[2].y = y + half_height;

      gdk_draw_polygon(window, gc, TRUE, points, 3);
      gdk_draw_polygon(window, gc, FALSE, points, 3);
      break;
    case GTK_ARROW_RIGHT:
      points[0].x = x + half_width - (half_height / 2);
      points[0].y = y;
      points[1].x = x + half_width - (half_height / 2);
      points[1].y = y + height - 1;
      points[2].x = x + half_width + (half_height / 2);
      points[2].y = y + half_height;

      gdk_draw_polygon(window, gc, TRUE, points, 3);
      gdk_draw_polygon(window, gc, FALSE, points, 3);
      break;
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(gc, NULL);
    }
}

/**************************************************************************/
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
  gint                half_width;
  gint                half_height;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  half_width = width / 2;
  half_height = height / 2;

  if (area)
    {
      gdk_gc_set_clip_rectangle(style->light_gc[state_type], area);
      gdk_gc_set_clip_rectangle(style->bg_gc[state_type], area);
      gdk_gc_set_clip_rectangle(style->dark_gc[state_type], area);
      gdk_gc_set_clip_rectangle(style->black_gc, area);
    }
  switch (shadow_type)
    {
    case GTK_SHADOW_IN:
      gdk_draw_line(window, style->bg_gc[state_type],
		    x + 2, y + half_height,
		    x + half_width, y + height - 2);
      gdk_draw_line(window, style->bg_gc[state_type],
		    x + half_width, y + height - 2,
		    x + width - 2, y + half_height);
      gdk_draw_line(window, style->light_gc[state_type],
		    x + 1, y + half_height,
		    x + half_width, y + height - 1);
      gdk_draw_line(window, style->light_gc[state_type],
		    x + half_width, y + height - 1,
		    x + width - 1, y + half_height);
      gdk_draw_line(window, style->light_gc[state_type],
		    x, y + half_height,
		    x + half_width, y + height);
      gdk_draw_line(window, style->light_gc[state_type],
		    x + half_width, y + height,
		    x + width, y + half_height);

      gdk_draw_line(window, style->black_gc,
		    x + 2, y + half_height,
		    x + half_width, y + 2);
      gdk_draw_line(window, style->black_gc,
		    x + half_width, y + 2,
		    x + width - 2, y + half_height);
      gdk_draw_line(window, style->dark_gc[state_type],
		    x + 1, y + half_height,
		    x + half_width, y + 1);
      gdk_draw_line(window, style->dark_gc[state_type],
		    x + half_width, y + 1,
		    x + width - 1, y + half_height);
      gdk_draw_line(window, style->dark_gc[state_type],
		    x, y + half_height,
		    x + half_width, y);
      gdk_draw_line(window, style->dark_gc[state_type],
		    x + half_width, y,
		    x + width, y + half_height);
      break;
    case GTK_SHADOW_OUT:
      gdk_draw_line(window, style->dark_gc[state_type],
		    x + 2, y + half_height,
		    x + half_width, y + height - 2);
      gdk_draw_line(window, style->dark_gc[state_type],
		    x + half_width, y + height - 2,
		    x + width - 2, y + half_height);
      gdk_draw_line(window, style->dark_gc[state_type],
		    x + 1, y + half_height,
		    x + half_width, y + height - 1);
      gdk_draw_line(window, style->dark_gc[state_type],
		    x + half_width, y + height - 1,
		    x + width - 1, y + half_height);
      gdk_draw_line(window, style->black_gc,
		    x, y + half_height,
		    x + half_width, y + height);
      gdk_draw_line(window, style->black_gc,
		    x + half_width, y + height,
		    x + width, y + half_height);

      gdk_draw_line(window, style->bg_gc[state_type],
		    x + 2, y + half_height,
		    x + half_width, y + 2);
      gdk_draw_line(window, style->bg_gc[state_type],
		    x + half_width, y + 2,
		    x + width - 2, y + half_height);
      gdk_draw_line(window, style->light_gc[state_type],
		    x + 1, y + half_height,
		    x + half_width, y + 1);
      gdk_draw_line(window, style->light_gc[state_type],
		    x + half_width, y + 1,
		    x + width - 1, y + half_height);
      gdk_draw_line(window, style->light_gc[state_type],
		    x, y + half_height,
		    x + half_width, y);
      gdk_draw_line(window, style->light_gc[state_type],
		    x + half_width, y,
		    x + width, y + half_height);
      break;
    default:
      break;
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(style->light_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle(style->bg_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle(style->dark_gc[state_type], NULL);
      gdk_gc_set_clip_rectangle(style->black_gc, NULL);
    }
}

/**************************************************************************/
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
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);
}

/**************************************************************************/
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

/**************************************************************************/
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
  gint                xthik;
  gint                ythik;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  printf("%p %s %i %i\n", detail, detail, width, height);

  if ((detail) && (!strcmp("trough", detail)))
    {
      GdkPixmap          *pm;
      gint                depth;

      printf("troff\n");

      if (GTK_CHECK_TYPE(widget, gtk_progress_bar_get_type()))
	{
	  if (area)
	    {
	      gdk_gc_set_clip_rectangle(style->light_gc[GTK_STATE_NORMAL], area);
	    }
	  gdk_draw_rectangle(window, style->light_gc[GTK_STATE_NORMAL],
			     TRUE, x, y, width, height);
	  if (area)
	    {
	      gdk_gc_set_clip_rectangle(style->light_gc[GTK_STATE_NORMAL], NULL);
	    }
	  gtk_paint_shadow(style, window, state_type, shadow_type, area, widget, detail,
			   x, y, width, height);
	}
      else
	{
	  xthik = style->klass->xthickness;
	  ythik = style->klass->ythickness;

	  gdk_window_get_geometry(window, NULL, NULL, NULL, NULL, &depth);
	  pm = gdk_pixmap_new(window, 2, 2, depth);

	  gdk_draw_point(pm, style->bg_gc[GTK_STATE_NORMAL], 0, 0);
	  gdk_draw_point(pm, style->bg_gc[GTK_STATE_NORMAL], 1, 1);
	  gdk_draw_point(pm, style->light_gc[GTK_STATE_NORMAL], 1, 0);
	  gdk_draw_point(pm, style->light_gc[GTK_STATE_NORMAL], 0, 1);
	  gdk_window_set_back_pixmap(window, pm, FALSE);
	  gdk_window_clear(window);

	  gdk_pixmap_unref(pm);
	}
    }
  else if ((detail) && (!strcmp(detail, "menuitem")))
    {
      if (area)
	{
	  gdk_gc_set_clip_rectangle(style->bg_gc[GTK_STATE_SELECTED], area);
	}
      gdk_draw_rectangle(window, style->bg_gc[GTK_STATE_SELECTED],
			 TRUE, x, y, width, height);
      gdk_draw_line(window, style->dark_gc[GTK_STATE_SELECTED], x, y, x + width, y);
      gdk_draw_line(window, style->light_gc[GTK_STATE_SELECTED], x, y + height -1, x + width, y + height - 1);
      if (area)
	{
	  gdk_gc_set_clip_rectangle(style->bg_gc[GTK_STATE_SELECTED], NULL);
	}
    }
  else if ((detail) && (!strcmp("bar", detail)))
    {
      if (area)
	{
	  gdk_gc_set_clip_rectangle(style->bg_gc[GTK_STATE_SELECTED], area);
	}
      gdk_draw_rectangle(window, style->bg_gc[GTK_STATE_SELECTED],
			 TRUE, x + 1, y + 1, width - 2, height - 2);
      if (area)
	{
	  gdk_gc_set_clip_rectangle(style->bg_gc[GTK_STATE_SELECTED], NULL);
	}
    }
  else if ((detail) && (!strcmp("menubar", detail)))
    {
      if (area)
	{
	  gdk_gc_set_clip_rectangle(style->bg_gc[state_type], area);
	}
      gdk_draw_rectangle(window, style->bg_gc[state_type], TRUE,
			 x, y, width, height);
      if (area)
	{
	  gdk_gc_set_clip_rectangle(style->bg_gc[state_type], NULL);
	}
    }
  else
    {
      if ((!style->bg_pixmap[state_type]) ||
	  (gdk_window_get_type(window) == GDK_WINDOW_PIXMAP))
	{
	  if (area)
	    {
	      gdk_gc_set_clip_rectangle(style->bg_gc[state_type], area);
	    }
	  gdk_draw_rectangle(window, style->bg_gc[state_type], TRUE,
			     x, y, width, height);
	  if (area)
	    {
	      gdk_gc_set_clip_rectangle(style->bg_gc[state_type], NULL);
	    }
	}
      else
	gtk_style_apply_default_pixmap(style, window, state_type, area, x, y, width, height);
      gtk_paint_shadow(style, window, state_type, shadow_type, area, widget, detail,
		       x, y, width, height);
    }
}

/**************************************************************************/
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
  GdkGC              *gc1;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  gc1 = style->bg_gc[state_type];

  if ((detail) && (!strcmp("selected", detail)))
    gc1 = style->bg_gc[GTK_STATE_SELECTED];
  if ((detail) && (!strcmp("text", detail)) && (state_type == GTK_STATE_SELECTED))
    gc1 = style->bg_gc[GTK_STATE_SELECTED];
  else if ((detail) && (!strcmp("viewportbin", detail)))
    gc1 = style->bg_gc[GTK_STATE_NORMAL];
  if ((!style->bg_pixmap[state_type]) || (gc1 != style->bg_gc[state_type]) ||
      (gdk_window_get_type(window) == GDK_WINDOW_PIXMAP))
    {
      if (area)
	{
	  gdk_gc_set_clip_rectangle(gc1, area);
	}
      gdk_draw_rectangle(window, gc1, TRUE,
			 x, y, width, height);
      if ((detail) && (!strcmp("tooltip", detail)))
	gdk_draw_rectangle(window, style->black_gc, FALSE,
			   x, y, width - 1, height - 1);
      if (area)
	{
	  gdk_gc_set_clip_rectangle(gc1, NULL);
	}
    }
  else
    gtk_style_apply_default_pixmap(style, window, state_type, area, x, y, width, height);
}

/**************************************************************************/
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
  GdkGC              *gc1;
  GdkGC              *gc2;
  gint                xx, yy, ww, hh;

  // Fixed size only

  printf("%p %s %i %i\n", detail, detail, width, height);

//  gc2 = style->light_gc[GTK_STATE_NORMAL];
  gc2 = style->bg_gc[GTK_STATE_NORMAL];

  gc1 = style->black_gc;
//  gc1 = style->fg_gc[GTK_STATE_NORMAL];

  if (area)
    {
      gdk_gc_set_clip_rectangle(gc1, area);
      gdk_gc_set_clip_rectangle(gc2, area);
    }
//  gdk_draw_rectangle(window, gc2, TRUE,
//		     x, y, width, height);
  xx = x - 2;
  yy = y - 2;

  if (shadow_type == GTK_SHADOW_IN)
    {
      gdk_draw_line(window, gc1,
		    xx + 3, yy + 5,
		    xx + 3, yy + 9);
      gdk_draw_line(window, gc1,
		    xx + 4, yy + 5,
		    xx + 4, yy + 9);
      gdk_draw_line(window, gc1,
		    xx + 5, yy + 8,
		    xx + 9, yy + 4);
      gdk_draw_line(window, gc1,
		    xx + 5, yy + 7,
		    xx + 9, yy + 3);
/*
      xx = x + 1;
      yy = y + 1;
      ww = width - 2;
      hh = height - 2;

      gdk_draw_line(window, gc1,
		    xx + ww - 1, yy,
		    xx + (ww / 3), yy + hh - 3);
      gdk_draw_line(window, gc1,
		    xx, yy + (2 * hh / 3) - 2,
		    xx + (ww / 3), yy + hh - 3);
      gdk_draw_line(window, gc1,
		    xx + ww - 1, yy + 1,
		    xx + (ww / 3), yy + hh - 2);
      gdk_draw_line(window, gc1,
		    xx, yy + (2 * hh / 3) - 1,
		    xx + (ww / 3), yy + hh - 2);
      gdk_draw_line(window, gc1,
		    xx + ww - 1, yy + 2,
		    xx + (ww / 3), yy + hh - 1);
      gdk_draw_line(window, gc1,
		    xx, yy + (2 * hh / 3),
		    xx + (ww / 3), yy + hh - 1);
*/
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle(gc1, NULL);
      gdk_gc_set_clip_rectangle(gc2, NULL);
    }
/*
  gtk_paint_shadow(style, window, state_type, GTK_SHADOW_ETCHED_IN, area, widget, detail,
		   x - style->klass->xthickness, y - style->klass->ythickness,
		   width + 2 * style->klass->xthickness,
		   height + 2 * style->klass->ythickness);
*/
  gtk_paint_shadow(style, window, state_type, GTK_SHADOW_ETCHED_IN, area, widget, detail,
		   xx, yy, 13, 13);
}

/**************************************************************************/
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
  GdkGC              *gc0;
  GdkGC              *gc1;
  GdkGC              *gc2;
  GdkGC              *gc3;
  GdkGC              *gc4;

  x -= 1;
  y -= 1;
  width += 2;
  height += 2;

  gc0 = style->white_gc;
  gc1 = style->light_gc[GTK_STATE_NORMAL];
  gc2 = style->bg_gc[GTK_STATE_NORMAL];
  gc3 = style->dark_gc[GTK_STATE_NORMAL];
  gc4 = style->black_gc;

  if (area)
    {
      gdk_gc_set_clip_rectangle(gc1, area);
      gdk_gc_set_clip_rectangle(gc2, area);
      gdk_gc_set_clip_rectangle(gc3, area);
      gdk_gc_set_clip_rectangle(gc4, area);
    }

  // Draw radio button, metal-stle
  // There is probably a better way to do this
  // with pixmaps. Fix later.
 
  // dark
  gdk_draw_line(window, gc3, x+4, y, x+7, y);
  gdk_draw_line(window, gc3, x+2, y+1, x+3, y+1);
  gdk_draw_line(window, gc3, x+8, y+1, x+9, y+1);
  gdk_draw_line(window, gc3, x+2, y+10, x+3, y+10);
  gdk_draw_line(window, gc3, x+8, y+10, x+9, y+10);
  gdk_draw_line(window, gc3, x+4, y+11, x+7, y+11);


  gdk_draw_line(window, gc3, x, y+4, x, y+7);
  gdk_draw_line(window, gc3, x+1, y+2, x+1, y+3);
  gdk_draw_line(window, gc3, x+1, y+8, x+1, y+9);
  gdk_draw_line(window, gc3, x+10, y+2, x+10, y+3);
  gdk_draw_line(window, gc3, x+10, y+8, x+10, y+9);
  gdk_draw_line(window, gc3, x+11, y+4, x+11, y+7);

  // white
  gdk_draw_line(window, gc0, x+4, y+1, x+7, y+1);
  gdk_draw_line(window, gc0, x+2, y+2, x+3, y+2);
  gdk_draw_line(window, gc0, x+8, y+2, x+9, y+2);
  gdk_draw_line(window, gc0, x+2, y+11, x+3, y+11);
  gdk_draw_line(window, gc0, x+8, y+11, x+9, y+11);
  gdk_draw_line(window, gc0, x+4, y+12, x+7, y+12);


  gdk_draw_line(window, gc0, x+1, y+4, x+1, y+7);
  gdk_draw_line(window, gc0, x+2, y+2, x+2, y+3);
  gdk_draw_line(window, gc0, x+2, y+8, x+2, y+9);
  gdk_draw_line(window, gc0, x+11, y+2, x+11, y+3);
  gdk_draw_line(window, gc0, x+11, y+8, x+11, y+9);
  gdk_draw_line(window, gc0, x+12, y+4, x+12, y+7);
  gdk_draw_point(window, gc0, x+10, y+1);
  gdk_draw_point(window, gc0, x+10, y+10);



//  gdk_draw_arc(window, gc0, FALSE, x + 1, y + 1, width, height, 0, 360 * 64);
//  gdk_draw_arc(window, gc3, FALSE, x, y, width, height, 0, 360 * 64);
/*
  gdk_draw_arc(window, gc3, FALSE, x, y, width, height, 45 * 64, 225 * 64);
  gdk_draw_arc(window, gc3, TRUE, x, y, width, height, 45 * 64, 225 * 64);
  gdk_draw_arc(window, gc1, FALSE, x, y, width, height, 225 * 64, 180 * 64);
  gdk_draw_arc(window, gc1, TRUE, x, y, width, height, 225 * 64, 180 * 64);
  gdk_draw_arc(window, gc4, FALSE, x + 1, y + 1, width - 2, height - 2, 45 * 64, 225 * 64);
  gdk_draw_arc(window, gc4, TRUE, x + 1, y + 1, width - 2, height - 2, 45 * 64, 225 * 64);
  gdk_draw_arc(window, gc2, FALSE, x + 1, y + 1, width - 2, height - 2, 225 * 64, 180 * 64);
  gdk_draw_arc(window, gc2, TRUE, x + 1, y + 1, width - 2, height - 2, 225 * 64, 180 * 64);
  gdk_draw_arc(window, gc1, FALSE, x + 2, y + 2, width - 4, height - 4, 0, 360 * 64);
  gdk_draw_arc(window, gc1, TRUE, x + 2, y + 2, width - 4, height - 4, 0, 360 * 64);
*/

  if (shadow_type == GTK_SHADOW_IN)
    {
  gdk_draw_rectangle(window, gc4, TRUE, x+3, y+4, 6, 4);
  gdk_draw_rectangle(window, gc4, TRUE, x+4, y+3, 4, 6);
//      gdk_draw_arc(window, gc4, FALSE, x + 4, y + 4, width - 8, height - 8, 0, 360 * 64);
//      gdk_draw_arc(window, gc4, TRUE, x + 4, y + 4, width - 8, height - 8, 0, 360 * 64);
    }

  if (area)
    {
      gdk_gc_set_clip_rectangle(gc1, NULL);
      gdk_gc_set_clip_rectangle(gc2, NULL);
      gdk_gc_set_clip_rectangle(gc3, NULL);
      gdk_gc_set_clip_rectangle(gc4, NULL);
    }
}

/**************************************************************************/
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
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);
}

/**************************************************************************/
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
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);
}

/**************************************************************************/
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
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  gtk_paint_box(style, window, state_type, shadow_type, area, widget, detail,
		x, y, width, height);
}

/**************************************************************************/
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
  GdkRectangle        rect;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  gtk_paint_shadow(style, window, state_type, shadow_type, area, widget, detail,
		   x, y, width, height);

  if (gap_side == 0)
    /* top */
    {
      rect.x = x + gap_x;
      rect.y = y;
      rect.width = gap_width;
      rect.height = 2;
    }
  else if (gap_side == 1)
    /* bottom */
    {
      rect.x = x + gap_x;
      rect.y = y + height - 2;
      rect.width = gap_width;
      rect.height = 2;
    }
  else if (gap_side == 2)
    /* left */
    {
      rect.x = x;
      rect.y = y + gap_x;
      rect.width = 2;
      rect.height = gap_width;
    }
  else if (gap_side == 3)
    /* right */
    {
      rect.x = x + width - 2;
      rect.y = y + gap_x;
      rect.width = 2;
      rect.height = gap_width;
    }

  gtk_style_apply_default_pixmap(style, window, state_type, area,
				 rect.x, rect.y, rect.width, rect.height);
}

/**************************************************************************/
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
  GdkRectangle        rect;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  gtk_paint_box(style, window, state_type, shadow_type, area, widget, detail,
		x, y, width, height);

  if (gap_side == 0)
    /* top */
    {
      rect.x = x + gap_x;
      rect.y = y;
      rect.width = gap_width;
      rect.height = 2;
    }
  else if (gap_side == 1)
    /* bottom */
    {
      rect.x = x + gap_x;
      rect.y = y + height - 2;
      rect.width = gap_width;
      rect.height = 2;
    }
  else if (gap_side == 2)
    /* left */
    {
      rect.x = x;
      rect.y = y + gap_x;
      rect.width = 2;
      rect.height = gap_width;
    }
  else if (gap_side == 3)
    /* right */
    {
      rect.x = x + width - 2;
      rect.y = y + gap_x;
      rect.width = 2;
      rect.height = gap_width;
    }

  gtk_style_apply_default_pixmap(style, window, state_type, area,
				 rect.x, rect.y, rect.width, rect.height);
}

/**************************************************************************/
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
  GdkRectangle        rect;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  gtk_paint_box(style, window, state_type, shadow_type, area, widget, detail,
		x, y, width, height);

  if (gap_side == 0)
    /* top */
    {
      rect.x = x + style->klass->xthickness;
      rect.y = y;
      rect.width = width - style->klass->xthickness * 2;
      rect.height = style->klass->ythickness;
    }
  else if (gap_side == 1)
    /* bottom */
    {
      rect.x = x + style->klass->xthickness;
      rect.y = y + height - style->klass->ythickness;
      rect.width = width - style->klass->xthickness * 2;
      rect.height = style->klass->ythickness;
    }
  else if (gap_side == 2)
    /* left */
    {
      rect.x = x;
      rect.y = y + style->klass->ythickness;
      rect.width = style->klass->xthickness;
      rect.height = height - style->klass->ythickness * 2;
    }
  else if (gap_side == 3)
    /* right */
    {
      rect.x = x + width - style->klass->xthickness;
      rect.y = y + style->klass->ythickness;
      rect.width = style->klass->xthickness;
      rect.height = height - style->klass->ythickness * 2;
    }

  gtk_style_apply_default_pixmap(style, window, state_type, area,
				 rect.x, rect.y, rect.width, rect.height);
}

/**************************************************************************/
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
  return; // TBD 
/*
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    {
      gdk_window_get_size(window, &width, &height);
      width -= 1;
      height -= 1;
    }
  else if (width == -1)
    {
      gdk_window_get_size(window, &width, NULL);
      width -= 1;
    }
  else if (height == -1)
    {
      gdk_window_get_size(window, NULL, &height);
      height -= 1;
    }
  if (area)
    {
      gdk_gc_set_clip_rectangle(style->black_gc, area);
    }
  gdk_draw_rectangle(window,
		     style->black_gc, FALSE,
		     x, y, width, height);
  if (area)
    {
      gdk_gc_set_clip_rectangle(style->black_gc, NULL);
    }
*/
}

/**************************************************************************/
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
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  gtk_draw_box(style, window, state_type, shadow_type, x, y,
	       width, height);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    draw_vline(style, window, state_type, area, widget, detail,
	       style->klass->ythickness,
	       height - style->klass->ythickness - 1, width / 2);
  else
    draw_hline(style, window, state_type, area, widget, detail,
	       style->klass->xthickness,
	       width - style->klass->xthickness - 1, height / 2);
}

/**************************************************************************/
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
  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  printf("entry draw\n");
  if ((detail) && (!strcmp("selected", detail)))
    {
      gdk_draw_rectangle(window,
			 style->bg_gc[state_type],
			 TRUE,
			 x, y,
			 width,
			 height);
    }
  else
    {
      if (area)
	gdk_gc_set_clip_rectangle(style->base_gc[state_type], area);
      gdk_draw_rectangle(window,
			 style->base_gc[state_type],
			 TRUE,
			 x, y,
			 width,
			 height);
      if (area)
	gdk_gc_set_clip_rectangle(style->base_gc[state_type], NULL);
    }
}

/**************************************************************************/
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
  gint                xx, yy;
  gint                xthick, ythick;
  GdkGC              *light_gc, *dark_gc;
  GdkRectangle        dest;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if ((width == -1) && (height == -1))
    gdk_window_get_size(window, &width, &height);
  else if (width == -1)
    gdk_window_get_size(window, &width, NULL);
  else if (height == -1)
    gdk_window_get_size(window, NULL, &height);

  gtk_paint_box(style, window, state_type, shadow_type, area, widget,
		detail, x, y, width, height);

  light_gc = style->light_gc[state_type];
  dark_gc = style->dark_gc[state_type];

  xthick = style->klass->xthickness;
  ythick = style->klass->ythickness;

  dest.x = x + xthick;
  dest.y = y + ythick;
  dest.width = width - (xthick * 2);
  dest.height = height - (ythick * 2);

  gdk_gc_set_clip_rectangle(light_gc, &dest);
  gdk_gc_set_clip_rectangle(dark_gc, &dest);

  yy = y + ythick;
  for (xx = x + xthick; xx < (x + width - xthick); xx += 6)
    {
      gdk_draw_line(window, light_gc, xx, yy, xx, yy + height - ythick);
      gdk_draw_line(window, dark_gc, xx + 1, yy, xx + 1, yy + height - ythick);

      gdk_draw_line(window, light_gc, xx + 3, yy, xx + 3, yy + height - ythick);
      gdk_draw_line(window, dark_gc, xx + 4, yy, xx + 4, yy + height - ythick);
    }
  gdk_gc_set_clip_rectangle(light_gc, NULL);
  gdk_gc_set_clip_rectangle(dark_gc, NULL);
}
