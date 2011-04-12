/* Build with
   gcc -lm -lz -O2 -Wall `pkg-config --libs --cflags cairo`  -o broadway broadway.c demo.c
*/

#include "broadway.h"
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <cairo.h>

static void
diff_surfaces (cairo_surface_t *surface,
	       cairo_surface_t *old_surface)
{
  uint8_t *data, *old_data;
  uint32_t *line, *old_line;
  int w, h, stride, old_stride;
  int x, y;

  data = cairo_image_surface_get_data (surface);
  old_data = cairo_image_surface_get_data (old_surface);

  w = cairo_image_surface_get_width (surface);
  h = cairo_image_surface_get_height (surface);

  stride = cairo_image_surface_get_stride (surface);
  old_stride = cairo_image_surface_get_stride (old_surface);

  for (y = 0; y < h; y++)
    {
      line = (uint32_t *)data;
      old_line = (uint32_t *)old_data;

      for (x = 0; x < w; x++)
	{
	  if ((*line & 0xffffff) == (*old_line & 0xffffff))
	    *old_line = 0;
	  else
	    *old_line = *line | 0xff000000;
	  line ++;
	  old_line ++;
	}

      data += stride;
      old_data += old_stride;
    }
}

static void
snippet(cairo_t *cr, int i)
{
  if (1)
    {
      cairo_save(cr);
      cairo_rotate (cr, i * 0.002);
      /* a custom shape that could be wrapped in a function */
      double x0      = 25.6,   /* parameters like cairo_rectangle */
	y0      = 25.6,
	rect_width  = 204.8,
	rect_height = 204.8,
	radius = 102.4;   /* and an approximate curvature radius */

      double x1,y1;

      x1=x0+rect_width;
      y1=y0+rect_height;
      if (rect_width/2<radius) {
	if (rect_height/2<radius) {
	  cairo_move_to  (cr, x0, (y0 + y1)/2);
	  cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1)/2, y0);
	  cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1)/2);
	  cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0)/2, y1);
	  cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1)/2);
	} else {
	  cairo_move_to  (cr, x0, y0 + radius);
	  cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1)/2, y0);
	  cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
	  cairo_line_to (cr, x1 , y1 - radius);
	  cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0)/2, y1);
	  cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
	}
      } else {
	if (rect_height/2<radius) {
	  cairo_move_to  (cr, x0, (y0 + y1)/2);
	  cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
	  cairo_line_to (cr, x1 - radius, y0);
	  cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1)/2);
	  cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
	  cairo_line_to (cr, x0 + radius, y1);
	  cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1)/2);
	} else {
	  cairo_move_to  (cr, x0, y0 + radius);
	  cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
	  cairo_line_to (cr, x1 - radius, y0);
	  cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
	  cairo_line_to (cr, x1 , y1 - radius);
	  cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
	  cairo_line_to (cr, x0 + radius, y1);
	  cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
	}
      }
      cairo_close_path (cr);

      cairo_set_source_rgb (cr, 0.5, 0.5, 1);
      cairo_fill_preserve (cr);
      cairo_set_source_rgba (cr, 0.5, 0, 0, 0.5);
      cairo_set_line_width (cr, 10.0);
      cairo_stroke (cr);
      cairo_restore(cr);
    }
  if (1)
    {
      double xc = 128.0;
      double yc = 128.0;
      double radius = 100.0;
      double angle1 = (45.0 + i * 5)  * (M_PI/180.0);  /* angles are specified */
      double angle2 = (180.0 + i * 5) * (M_PI/180.0);  /* in radians           */

      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

      cairo_set_line_width (cr, 10.0);
      cairo_arc (cr, xc, yc, radius, angle1, angle2);
      cairo_stroke (cr);

      /* draw helping lines */
      cairo_set_source_rgba (cr, 1, 0.2, 0.2, 0.6);
      cairo_set_line_width (cr, 6.0);

      cairo_arc (cr, xc, yc, 10.0, 0, 2*M_PI);
      cairo_fill (cr);

      cairo_arc (cr, xc, yc, radius, angle1, angle1);
      cairo_line_to (cr, xc, yc);
      cairo_arc (cr, xc, yc, radius, angle2, angle2);
      cairo_line_to (cr, xc, yc);
      cairo_stroke (cr);
    }
}

static void
demo2 (BroadwayOutput *output)
{
  cairo_t *cr;
  cairo_surface_t *surface, *old_surface;
  BroadwayRect rects[2];
  double da = 0;
  int i;

  broadway_output_new_surface(output,  0, 100, 100, 800, 600, 0);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					800, 600);
  old_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					    800, 600);

  cr = cairo_create (old_surface);
  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  cairo_rectangle (cr, 0, 0, 800, 600);
  cairo_fill (cr);
  cairo_destroy (cr);

  for (i = 0; i < 100; i++)
    {
      cr = cairo_create (surface);

      cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);

      cairo_rectangle (cr, 0, 0, 800, 600);
      cairo_fill (cr);

      snippet(cr, i);

      cairo_destroy (cr);

      if (i == 0)
	{
	  broadway_output_put_rgb (output, 0, 0, 0, 800, 600, 800*4,
				   cairo_image_surface_get_data(surface)
				   );
	  broadway_output_show_surface (output,  0);
	}
      else
	{
	  diff_surfaces (surface,
			 old_surface);
	  broadway_output_put_rgba (output, 0, 0, 0, 800, 600, 800*4,
				    cairo_image_surface_get_data(old_surface));
	}
      broadway_output_move_resize_surface (output, 0, 1, 100 + i, 100 + i, 0, 0, 0);

      rects[0].x = 500;
      rects[0].y = 0;
      rects[0].width = 100;
      rects[0].height = 100;
      rects[1].x = 600;
      rects[1].y = 100;
      rects[1].width = 100;
      rects[1].height = 100;
      broadway_output_copy_rectangles (output,
				       0,
				       rects, 2,
				       400, 0);

      broadway_output_flush (output);

      cr = cairo_create (old_surface);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);

      da += 10;
      usleep (50 * 1000);
  }


  cairo_surface_destroy (surface);
  broadway_output_destroy_surface(output,  0);
  broadway_output_flush (output);
}

int
main (int argc, char *argv[])
{
  BroadwayOutput *output;

  output = broadway_output_new (STDOUT_FILENO, 1);
  demo2(output);

  return 0;
}
