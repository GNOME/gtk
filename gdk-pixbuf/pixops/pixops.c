#include <math.h>
#include <glib.h>
#include "config.h"

#include "pixops.h"
#include "pixops-internal.h"

#define SUBSAMPLE_BITS 4
#define SUBSAMPLE (1 << SUBSAMPLE_BITS)
#define SUBSAMPLE_MASK ((1 << SUBSAMPLE_BITS)-1)
#define SCALE_SHIFT 16

typedef struct _PixopsFilter PixopsFilter;

struct _PixopsFilter
{
  int *weights;
  int n_x;
  int n_y;
  double x_offset;
  double y_offset;
}; 

typedef art_u8 *(*PixopsLineFunc) (int *weights, int n_x, int n_y,
				   art_u8 *dest, int dest_x, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
				   art_u8 **src, int src_channels, art_boolean src_has_alpha,
				   int x_init, int x_step, int src_width,
				   int check_size, art_u32 color1, art_u32 color2);

typedef void (*PixopsPixelFunc) (art_u8 *dest, int dest_x, int dest_channels, int dest_has_alpha,
				 int src_has_alpha, int check_size, art_u32 color1,
				 art_u32 color2,
				 int r, int g, int b, int a);

static int
get_check_shift (int check_size)
{
  int check_shift = 0;
  g_return_val_if_fail (check_size >= 0, 4);

  while (!(check_size & 1))
    {
      check_shift++;
      check_size >>= 1;
    }

  return check_shift;
}

static void
pixops_scale_nearest (art_u8        *dest_buf,
		      int            render_x0,
		      int            render_y0,
		      int            render_x1,
		      int            render_y1,
		      int            dest_rowstride,
		      int            dest_channels,
		      art_boolean    dest_has_alpha,
		      const art_u8  *src_buf,
		      int            src_width,
		      int            src_height,
		      int            src_rowstride,
		      int            src_channels,
		      art_boolean    src_has_alpha,
		      double         scale_x,
		      double         scale_y)
{
  int i, j;
  int x;
  int x_step = (1 << SCALE_SHIFT) / scale_x;
  int y_step = (1 << SCALE_SHIFT) / scale_y;

#define INNER_LOOP(SRC_CHANNELS,DEST_CHANNELS) 			\
      for (j=0; j < (render_x1 - render_x0); j++)		\
	{							\
	  const art_u8 *p = src + (x >> SCALE_SHIFT) * SRC_CHANNELS;	\
								\
	  dest[0] = p[0];					\
	  dest[1] = p[1];					\
	  dest[2] = p[2];					\
						       		\
	  if (DEST_CHANNELS == 4)				\
	    {							\
	      if (SRC_CHANNELS == 4)				\
		dest[3] = p[3];					\
	      else						\
		dest[3] = 0xff;					\
	    }							\
								\
	  dest += DEST_CHANNELS;				\
	  x += x_step;						\
	}

  for (i = 0; i < (render_y1 - render_y0); i++)
    {
      const art_u8 *src  = src_buf + ((i * y_step + y_step / 2) >> SCALE_SHIFT) * src_rowstride;
      art_u8       *dest = dest_buf + i * dest_rowstride;

      x = render_x0 * x_step + x_step / 2;

      if (src_channels == 3)
	{
	  if (dest_channels == 3)
	    {
	      INNER_LOOP (3, 3);
	    }
	  else
	    {
	      INNER_LOOP (3, 4);
	    }
	}
      else if (src_channels == 4)
	{
	  if (dest_channels == 3)
	    {
	      INNER_LOOP (4, 3);
	    }
	  else
	    {
	      for (j=0; j < (render_x1 - render_x0); j++)
		{
		  const art_u8 *p = src + (x >> SCALE_SHIFT) * 4;

		  *(art_u32 *)dest = *(art_u32 *)p;
		  
		  dest += 4;
		  x += x_step;
		}
	    }
	}
    }
#undef INNER_LOOP  
}

static void
pixops_composite_nearest (art_u8        *dest_buf,
			  int            render_x0,
			  int            render_y0,
			  int            render_x1,
			  int            render_y1,
			  int            dest_rowstride,
			  int            dest_channels,
			  art_boolean    dest_has_alpha,
			  const art_u8  *src_buf,
			  int            src_width,
			  int            src_height,
			  int            src_rowstride,
			  int            src_channels,
			  art_boolean    src_has_alpha,
			  double         scale_x,
			  double         scale_y,
			  int            overall_alpha)
{
  int i, j;
  int x;
  int x_step = (1 << SCALE_SHIFT) / scale_x;
  int y_step = (1 << SCALE_SHIFT) / scale_y;

  for (i = 0; i < (render_y1 - render_y0); i++)
    {
      const art_u8 *src  = src_buf + (((i + render_y0) * y_step + y_step / 2) >> SCALE_SHIFT) * src_rowstride;
      art_u8       *dest = dest_buf + i * dest_rowstride + render_x0 * dest_channels;

      x = render_x0 * x_step + x_step / 2;
      
      for (j=0; j < (render_x1 - render_x0); j++)
	{
	  const art_u8 *p = src + (x >> SCALE_SHIFT) * src_channels;
          unsigned int  a0;

	  if (src_has_alpha)
	    a0 = (p[3] * overall_alpha + 0xff) >> 8;
	  else
	    a0 = overall_alpha;

	  if (dest_has_alpha)
	    {
	      unsigned int a1 = dest[3];
	      unsigned int total = a0 + a1;

	      if (total)
		{
		  dest[0] = (a0 * src[0] + a1 * dest[0]) / (total);
		  dest[1] = (a0 * src[1] + a1 * dest[1]) / (total);
		  dest[2] = (a0 * src[2] + a1 * dest[2]) / (total);
		  dest[3] = total - ((a0 * a1 + 0xff) >> 8);
		}
	      else
		{
		  dest[0] = 0;
		  dest[1] = 0;
		  dest[2] = 0;
		  dest[3] = 0;
		}
	    }
	  else
	    {
	      dest[0] = dest[0] + ((a0 * (p[0] - dest[0]) + 0xff) >> 8);
	      dest[1] = dest[1] + ((a0 * (p[1] - dest[1]) + 0xff) >> 8);
	      dest[2] = dest[2] + ((a0 * (p[2] - dest[2]) + 0xff) >> 8);

	      if (dest_channels == 4)
		dest[3] = 0xff;
	    }

	  dest += dest_channels;
	  x += x_step;
	}
    }
}

static void
pixops_composite_color_nearest (art_u8        *dest_buf,
				int            render_x0,
				int            render_y0,
				int            render_x1,
				int            render_y1,
				int            dest_rowstride,
				int            dest_channels,
				art_boolean    dest_has_alpha,
				const art_u8  *src_buf,
				int            src_width,
				int            src_height,
				int            src_rowstride,
				int            src_channels,
				art_boolean    src_has_alpha,
				double         scale_x,
				double         scale_y,
				int            overall_alpha,
				int            check_x,
				int            check_y,
				int            check_size,
				art_u32        color1,
				art_u32        color2)
{
  int i, j;
  int x;
  int x_step = (1 << SCALE_SHIFT) / scale_x;
  int y_step = (1 << SCALE_SHIFT) / scale_y;
  int r1, g1, b1, r2, g2, b2;
  int check_shift = get_check_shift (check_size);

  for (i = 0; i < (render_y1 - render_y0); i++)
    {
      const art_u8 *src  = src_buf + (((i + render_y0) * y_step + y_step/2) >> SCALE_SHIFT) * src_rowstride;
      art_u8       *dest = dest_buf + i * dest_rowstride;

      x = render_x0 * x_step + x_step / 2;
      
      if (((i + check_y) >> check_shift) & 1)
	{
	  r1 = color2 & 0xff0000 >> 16;
	  g1 = color2 & 0xff00 >> 8;
	  b1 = color2 & 0xff;

	  r2 = color1 & 0xff0000 >> 16;
	  g2 = color1 & 0xff00 >> 8;
	  b2 = color1 & 0xff;
	}
      else
	{
	  r1 = color1 & 0xff0000 >> 16;
	  g1 = color1 & 0xff00 >> 8;
	  b1 = color1 & 0xff;

	  r2 = color2 & 0xff0000 >> 16;
	  g2 = color2 & 0xff00 >> 8;
	  b2 = color2 & 0xff;
	}

      for (j=0 ; j < (render_x1 - render_x0); j++)
	{
	  const art_u8 *p = src + (x >> SCALE_SHIFT) * src_channels;
          unsigned int  a0;

	  if (src_has_alpha)
	    a0 = (p[3] * overall_alpha + 0xff) >> 8;
	  else
	    a0 = overall_alpha;

	  if (((j + check_x) >> check_shift) & 1)
	    {
	      dest[0] = r2 + ((a0 * ((int)p[0] - r2) + 0xff) >> 8);
	      dest[1] = g2 + ((a0 * ((int)p[1] - g2) + 0xff) >> 8);
	      dest[2] = b2 + ((a0 * ((int)p[2] - b2) + 0xff) >> 8);
	    }
	  else
	    {
	      dest[0] = r1 + ((a0 * ((int)p[0] - r1) + 0xff) >> 8);
	      dest[1] = g1 + ((a0 * ((int)p[1] - g1) + 0xff) >> 8);
	      dest[2] = b1 + ((a0 * ((int)p[2] - b1) + 0xff) >> 8);
	    }
	  
	  if (dest_channels == 4)
	    dest[3] = 0xff;

	  dest += dest_channels;
	  x += x_step;
	}
    }
}

static void
composite_pixel (art_u8 *dest, int dest_x, int dest_channels, int dest_has_alpha,
		 int src_has_alpha, int check_size, art_u32 color1, art_u32 color2,
		 int r, int g, int b, int a)
{
  if (dest_has_alpha)
    {
      unsigned int w = (((1 << 16) - a) * dest[3]) >> 8;
      unsigned int total = a + w;

      if (total)
	{
	  dest[0] = (r + w * dest[0]) / total;
	  dest[1] = (g + w * dest[1]) / total;
	  dest[2] = (b + w * dest[2]) / total;
	  dest[3] = (r * w) >> 16;
	}
      else
	{
	  dest[0] = 0;
	  dest[1] = 0;
	  dest[2] = 0;
	  dest[3] = 0;
	}
    }
  else
    {
      dest[0] = ((0xff0000 - a) * dest[0] + r) >> 24;
      dest[1] = ((0xff0000 - a) * dest[1] + g) >> 24;
      dest[2] = ((0xff0000 - a) * dest[2] + b) >> 24;
    }
}

static art_u8 *
composite_line (int *weights, int n_x, int n_y,
		art_u8 *dest, int dest_x, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
		art_u8 **src, int src_channels, art_boolean src_has_alpha,
		int x_init, int x_step, int src_width,
		int check_size, art_u32 color1, art_u32 color2)
{
  int x = x_init;
  int i, j;

  while (dest < dest_end)
    {
      int x_scaled = x >> SCALE_SHIFT;
      unsigned int r = 0, g = 0, b = 0, a = 0;
      int *pixel_weights;
      
      pixel_weights = weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * n_x * n_y;

      for (i=0; i<n_y; i++)
	{
	  art_u8 *q = src[i] + x_scaled * src_channels;
	  int *line_weights = pixel_weights + n_x * i;
	  
	  for (j=0; j<n_x; j++)
	    {
	      unsigned int ta;

	      if (src_has_alpha)
		ta = q[3] * line_weights[j];
	      else
		ta = 0xff * line_weights[j];
		  
	      r += ta * q[0];
	      g += ta * q[1];
	      b += ta * q[2];
	      a += ta;

	      q += src_channels;
	    }
	}

      if (dest_has_alpha)
	{
	  unsigned int w = (((1 << 16) - a) * dest[3]) >> 8;
	  unsigned int total = a + w;

	  if (total)
	    {
	      dest[0] = (r + w * dest[0]) / total;
	      dest[1] = (r + w * dest[1]) / total;
	      dest[2] = (r + w * dest[2]) / total;
	      dest[3] = (r * w) >> 16;
	    }
	  else
	    {
	      dest[0] = 0;
	      dest[1] = 0;
	      dest[2] = 0;
	      dest[3] = 0;
	    }
	}
      else
	{
	  dest[0] = ((0xff0000 - a) * dest[0] + r) >> 24;
	  dest[1] = ((0xff0000 - a) * dest[1] + g) >> 24;
	  dest[2] = ((0xff0000 - a) * dest[2] + b) >> 24;
	}
      
      dest += dest_channels;
      x += x_step;
    }

  return dest;
}

static art_u8 *
composite_line_22_4a4 (int *weights, int n_x, int n_y,
		       art_u8 *dest, int dest_x, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
		       art_u8 **src, int src_channels, art_boolean src_has_alpha,
		       int x_init, int x_step, int src_width,
		       int check_size, art_u32 color1, art_u32 color2)
{
  int x = x_init;
  art_u8 *src0 = src[0];
  art_u8 *src1 = src[1];

  g_return_val_if_fail (src_channels != 3, dest);
  g_return_val_if_fail (src_has_alpha, dest);
  
  while (dest < dest_end)
    {
      int x_scaled = x >> SCALE_SHIFT;
      unsigned int r, g, b, a, ta;
      int *pixel_weights;
      art_u8 *q0, *q1;
      int w1, w2, w3, w4;
      
      q0 = src0 + x_scaled * 4;
      q1 = src1 + x_scaled * 4;
      
      pixel_weights = (int *)((char *)weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS - 4)) & (SUBSAMPLE_MASK << 4)));
      
      w1 = pixel_weights[0];
      w2 = pixel_weights[1];
      w3 = pixel_weights[2];
      w4 = pixel_weights[3];

      a = w1 * q0[3];
      r = a * q0[0];
      g = a * q0[1];
      b = a * q0[2];

      ta = w2 * q0[7];
      r += ta * q0[4];
      g += ta * q0[5];
      b += ta * q0[6];
      a += ta;

      ta = w3 * q0[3];
      r += ta * q0[0];
      g += ta * q0[1];
      b += ta * q0[2];
      a += ta;

      ta += w4 * q1[7];
      r += ta * q1[4];
      g += ta * q1[5];
      b += ta * q1[6];
      a += ta;

      dest[0] = ((0xff0000 - a) * dest[0] + r) >> 24;
      dest[1] = ((0xff0000 - a) * dest[1] + g) >> 24;
      dest[2] = ((0xff0000 - a) * dest[2] + b) >> 24;
      dest[3] = a >> 16;
      
      dest += 4;
      x += x_step;
    }

  return dest;
}

#ifdef USE_MMX
static art_u8 *
composite_line_22_4a4_mmx_stub (int *weights, int n_x, int n_y,
				art_u8 *dest, int dest_x, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
				art_u8 **src, int src_channels, art_boolean src_has_alpha,
				int x_init, int x_step, int src_width,
				int check_size, art_u32 color1, art_u32 color2)
{
  art_u32 mmx_weights[16][8];
  int j;

  for (j=0; j<16; j++)
    {
      mmx_weights[j][0] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][1] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][2] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][3] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][4] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][5] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][6] = 0x00010001 * (weights[4*j + 3] >> 8);
      mmx_weights[j][7] = 0x00010001 * (weights[4*j + 3] >> 8);
    }

  return pixops_composite_line_22_4a4_mmx (mmx_weights, dest, src[0], src[1], x_step, dest_end, x_init);
}
#endif /* USE_MMX */

static void
composite_pixel_color (art_u8 *dest, int dest_x, int dest_channels, int dest_has_alpha,
		       int src_has_alpha, int check_size, art_u32 color1, art_u32 color2,
		       int r, int g, int b, int a)
{
  int dest_r, dest_g, dest_b;
  int check_shift = get_check_shift (check_size);

  if ((dest_x >> check_shift) & 1)
    {
      dest_r = color2 & 0xff0000 >> 16;
      dest_g = color2 & 0xff00 >> 8;
      dest_b = color2 & 0xff;
    }
  else
    {
      dest_r = color1 & 0xff0000 >> 16;
      dest_g = color1 & 0xff00 >> 8;
      dest_b = color1 & 0xff;
    }

  dest[0] = ((0xff0000 - a) * dest_r + r) >> 24;
  dest[1] = ((0xff0000 - a) * dest_g + g) >> 24;
  dest[2] = ((0xff0000 - a) * dest_b + b) >> 24;

  if (dest_has_alpha)
    dest[3] = 0xff;
  else if (dest_channels == 4)
    dest[3] = a >> 16;
}

static art_u8 *
composite_line_color (int *weights, int n_x, int n_y,
		      art_u8 *dest, int dest_x, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
		      art_u8 **src, int src_channels, art_boolean src_has_alpha,
		      int x_init, int x_step, int src_width,
		      int check_size, art_u32 color1, art_u32 color2)
{
  int x = x_init;
  int i, j;
  int check_shift = get_check_shift (check_size);
  int dest_r1, dest_g1, dest_b1;
  int dest_r2, dest_g2, dest_b2;

  g_return_val_if_fail (check_size != 0, dest);

  dest_r1 = color1 & 0xff0000 >> 16;
  dest_g1 = color1 & 0xff00 >> 8;
  dest_b1 = color1 & 0xff;

  dest_r2 = color2 & 0xff0000 >> 16;
  dest_g2 = color2 & 0xff00 >> 8;
  dest_b2 = color2 & 0xff;

  while (dest < dest_end)
    {
      int x_scaled = x >> SCALE_SHIFT;
      unsigned int r = 0, g = 0, b = 0, a = 0;
      int *pixel_weights;
      
      pixel_weights = weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * n_x * n_y;

      for (i=0; i<n_y; i++)
	{
	  art_u8 *q = src[i] + x_scaled * src_channels;
	  int *line_weights = pixel_weights + n_x * i;
	  
	  for (j=0; j<n_x; j++)
	    {
	      unsigned int ta;
	      
	      if (src_has_alpha)
		ta = q[3] * line_weights[j];
	      else
		ta = 0xff * line_weights[j];
		  
	      r += ta * q[0];
	      g += ta * q[1];
	      b += ta * q[2];
	      a += ta;

	      q += src_channels;
	    }
	}

      if ((dest_x >> check_shift) & 1)
	{
	  dest[0] = ((0xff0000 - a) * dest_r2 + r) >> 24;
	  dest[1] = ((0xff0000 - a) * dest_g2 + g) >> 24;
	  dest[2] = ((0xff0000 - a) * dest_b2 + b) >> 24;
	}
      else
	{
	  dest[0] = ((0xff0000 - a) * dest_r1 + r) >> 24;
	  dest[1] = ((0xff0000 - a) * dest_g1 + g) >> 24;
	  dest[2] = ((0xff0000 - a) * dest_b1 + b) >> 24;
	}

      if (dest_has_alpha)
	dest[3] = 0xff;
      else if (dest_channels == 4)
	dest[3] = a >> 16;
	
      dest += dest_channels;
      x += x_step;
      dest_x++;
    }

  return dest;
}

#ifdef USE_MMX
static art_u8 *
composite_line_color_22_4a4_mmx_stub (int *weights, int n_x, int n_y,
				      art_u8 *dest, int dest_x, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
				      art_u8 **src, int src_channels, art_boolean src_has_alpha,
				      int x_init, int x_step, int src_width,
				      int check_size, art_u32 color1, art_u32 color2)
{
  art_u32 mmx_weights[16][8];
  int check_shift = get_check_shift (check_size);
  int colors[4];
  int j;

  for (j=0; j<16; j++)
    {
      mmx_weights[j][0] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][1] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][2] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][3] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][4] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][5] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][6] = 0x00010001 * (weights[4*j + 3] >> 8);
      mmx_weights[j][7] = 0x00010001 * (weights[4*j + 3] >> 8);
    }

  colors[0] = (color1 & 0xff00) << 8 | (color1 & 0xff);
  colors[1] = (color1 & 0xff0000) >> 16;
  colors[2] = (color2 & 0xff00) << 8 | (color2 & 0xff);
  colors[3] = (color2 & 0xff0000) >> 16;

  return pixops_composite_line_color_22_4a4_mmx (mmx_weights, dest, src[0], src[1], x_step, dest_end, x_init,
						 dest_x, check_shift, colors);
}
#endif /* USE_MMX */

static void
scale_pixel (art_u8 *dest, int dest_x, int dest_channels, int dest_has_alpha,
	     int src_has_alpha, int check_size, art_u32 color1, art_u32 color2,
	     int r, int g, int b, int a)
{
  if (src_has_alpha)
    {
      if (a)
	{
	  dest[0] = r / a;
	  dest[1] = g / a;
	  dest[2] = b / a;
	  dest[3] = a >> 16;
	}
      else
	{
	  dest[0] = 0;
	  dest[1] = 0;
	  dest[2] = 0;
	  dest[3] = 0;
	}
    }
  else
    {
      dest[0] = (r + 0xffffff) >> 24;
      dest[1] = (g + 0xffffff) >> 24;
      dest[2] = (b + 0xffffff) >> 24;
      
      if (dest_has_alpha)
	dest[3] = 0xff;
    }
}

static art_u8 *
scale_line (int *weights, int n_x, int n_y,
	    art_u8 *dest, int dest_x, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
	    art_u8 **src, int src_channels, art_boolean src_has_alpha,
	    int x_init, int x_step, int src_width,
	    int check_size, art_u32 color1, art_u32 color2)
{
  int x = x_init;
  int i, j;

  while (dest < dest_end)
    {
      int x_scaled = x >> SCALE_SHIFT;
      int *pixel_weights;

      pixel_weights = weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * n_x * n_y;

      if (src_has_alpha)
	{
	  unsigned int r = 0, g = 0, b = 0, a = 0;
	  for (i=0; i<n_y; i++)
	    {
	      art_u8 *q = src[i] + x_scaled * src_channels;
	      int *line_weights  = pixel_weights + n_x * i;
	      
	      for (j=0; j<n_x; j++)
		{
		  unsigned int ta;
		  
		  ta = q[3] * line_weights[j];
		  r += ta * q[0];
		  g += ta * q[1];
		  b += ta * q[2];
		  a += ta;
		  
		  q += src_channels;
		}
	    }

	  if (a)
	    {
	      dest[0] = r / a;
	      dest[1] = g / a;
	      dest[2] = b / a;
	      dest[3] = a >> 16;
	    }
	  else
	    {
	      dest[0] = 0;
	      dest[1] = 0;
	      dest[2] = 0;
	      dest[3] = 0;
	    }
	}
      else
	{
	  unsigned int r = 0, g = 0, b = 0;
	  for (i=0; i<n_y; i++)
	    {
	      art_u8 *q = src[i] + x_scaled * src_channels;
	      int *line_weights  = pixel_weights + n_x * i;
	      
	      for (j=0; j<n_x; j++)
		{
		  unsigned int ta = line_weights[j];
		  
		  r += ta * q[0];
		  g += ta * q[1];
		  b += ta * q[2];

		  q += src_channels;
		}
	    }

	  dest[0] = (r + 0xffff) >> 16;
	  dest[1] = (g + 0xffff) >> 16;
	  dest[2] = (b + 0xffff) >> 16;
	  
	  if (dest_has_alpha)
	    dest[3] = 0xff;
	}

      dest += dest_channels;
      
      x += x_step;
    }

  return dest;
}

#ifdef USE_MMX 
static art_u8 *
scale_line_22_33_mmx_stub (int *weights, int n_x, int n_y,
			   art_u8 *dest, int dest_x, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
			   art_u8 **src, int src_channels, art_boolean src_has_alpha,
			   int x_init, int x_step, int src_width,
			   int check_size, art_u32 color1, art_u32 color2)
{
  art_u32 mmx_weights[16][8];
  int j;

  for (j=0; j<16; j++)
    {
      mmx_weights[j][0] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][1] = 0x00010001 * (weights[4*j] >> 8);
      mmx_weights[j][2] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][3] = 0x00010001 * (weights[4*j + 1] >> 8);
      mmx_weights[j][4] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][5] = 0x00010001 * (weights[4*j + 2] >> 8);
      mmx_weights[j][6] = 0x00010001 * (weights[4*j + 3] >> 8);
      mmx_weights[j][7] = 0x00010001 * (weights[4*j + 3] >> 8);
    }

  return pixops_scale_line_22_33_mmx (mmx_weights, dest, src[0], src[1], x_step, dest_end, x_init);
}
#endif /* USE_MMX */

static art_u8 *
scale_line_22_33 (int *weights, int n_x, int n_y,
		  art_u8 *dest, art_u8 *dest_end, int dest_channels, int dest_has_alpha,
		  art_u8 **src, int src_channels, art_boolean src_has_alpha,
		  int x_init, int x_step, int src_width,
		  int check_size, art_u32 color1, art_u32 color2)
{
  int x = x_init;
  art_u8 *src0 = src[0];
  art_u8 *src1 = src[1];
  
  while (dest < dest_end)
    {
      unsigned int r, g, b;
      int x_scaled = x >> SCALE_SHIFT;
      int *pixel_weights;
      art_u8 *q0, *q1;
      int w1, w2, w3, w4;

      q0 = src0 + x_scaled * 3;
      q1 = src1 + x_scaled * 3;
      
      pixel_weights = (int *)((char *)weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS - 4)) & (SUBSAMPLE_MASK << 4)));
      
      w1 = pixel_weights[0];
      w2 = pixel_weights[1];
      w3 = pixel_weights[2];
      w4 = pixel_weights[3];

      r = w1 * q0[0];
      g = w1 * q0[1];
      b = w1 * q0[2];

      r += w2 * q0[3];
      g += w2 * q0[4];
      b += w2 * q0[5];

      r += w3 * q1[0];
      g += w3 * q1[1];
      b += w3 * q1[2];

      r += w4 * q1[4];
      g += w4 * q1[5];
      b += w4 * q1[6];

      dest[0] = r >> 16;
      dest[1] = g >> 16;
      dest[2] = b >> 16;
      
      dest += 3;
      
      x += x_step;
    }
  
  return dest;
}

static void
process_pixel (int *weights, int n_x, int n_y,
	       art_u8 *dest, int dest_x, int dest_channels, int dest_has_alpha,
	       art_u8 **src, int src_channels, art_boolean src_has_alpha,
	       int x_start, int src_width,
	       int check_size, art_u32 color1, art_u32 color2,
	       PixopsPixelFunc pixel_func)
{
  unsigned int r = 0, g = 0, b = 0, a = 0;
  int i, j;
  
  for (i=0; i<n_y; i++)
    {
      int *line_weights  = weights + n_x * i;

      for (j=0; j<n_x; j++)
	{
	  unsigned int ta;
	  art_u8 *q;

	  if (x_start + j < 0)
	    q = src[i];
	  else if (x_start + j < src_width)
	    q = src[i] + (x_start + j) * src_channels;
	  else
	    q = src[i] + (src_width - 1) * src_channels;

	  if (src_has_alpha)
	    ta = q[3] * line_weights[j];
	  else
	    ta = 0xff * line_weights[j];

	  r += ta * q[0];
	  g += ta * q[1];
	  b += ta * q[2];
	  a += ta;
	}
    }

  (*pixel_func) (dest, dest_x, dest_channels, dest_has_alpha, src_has_alpha, check_size, color1, color2, r, g, b, a);
}

static void
pixops_process (art_u8         *dest_buf,
		int             render_x0,
		int             render_y0,
		int             render_x1,
		int             render_y1,
		int             dest_rowstride,
		int             dest_channels,
		art_boolean     dest_has_alpha,
		const art_u8   *src_buf,
		int             src_width,
		int             src_height,
		int             src_rowstride,
		int             src_channels,
		art_boolean     src_has_alpha,
		double          scale_x,
		double          scale_y,
		int             check_x,
		int             check_y,
		int             check_size,
		art_u32         color1,
		art_u32         color2,
		PixopsFilter   *filter,
		PixopsLineFunc  line_func,
		PixopsPixelFunc pixel_func)
{
  int i, j;
  int x, y;
  art_u8 **line_bufs = g_new (art_u8 *, filter->n_y);

  int x_step = (1 << SCALE_SHIFT) / scale_x;
  int y_step = (1 << SCALE_SHIFT) / scale_y;

  int dest_x;
  int scaled_x_offset = floor (filter->x_offset * (1 << SCALE_SHIFT));

  int run_end_index = (((src_width - filter->n_x + 1) << SCALE_SHIFT) - scaled_x_offset - 1) / x_step + 1 - render_x0;
  int check_shift = check_size ? get_check_shift (check_size) : 0;

  y = render_y0 * y_step + floor (filter->y_offset * (1 << SCALE_SHIFT));
  for (i = 0; i < (render_y1 - render_y0); i++)
    {
      int y_start = y >> SCALE_SHIFT;
      int x_start;
      int *run_weights = filter->weights + ((y >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * filter->n_x * filter->n_y * SUBSAMPLE;
      art_u8 *new_outbuf;
      art_u32 tcolor1, tcolor2;
      
      art_u8 *outbuf = dest_buf + dest_rowstride * i;
      art_u8 *outbuf_end = outbuf + dest_channels * (render_x1 - render_x0);

      if (((i + check_y) >> check_shift) & 1)
	{
	  tcolor1 = color2;
	  tcolor2 = color1;
	}
      else
	{
	  tcolor1 = color1;
	  tcolor2 = color2;
	}

      for (j=0; j<filter->n_y; j++)
	{
	  if (y_start <  0)
	    line_bufs[j] = (art_u8 *)src_buf;
	  else if (y_start < src_height)
	    line_bufs[j] = (art_u8 *)src_buf + src_rowstride * y_start;
	  else
	    line_bufs[j] = (art_u8 *)src_buf + src_rowstride * (src_height - 1);

	  y_start++;
	}

      dest_x = check_x;
      x = render_x0 * x_step + scaled_x_offset;
      x_start = x >> SCALE_SHIFT;

      while (x_start < 0 && outbuf < outbuf_end)
	{
	  process_pixel (run_weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * (filter->n_x * filter->n_y), filter->n_x, filter->n_y,
			 outbuf, dest_x, dest_channels, dest_has_alpha,
			 line_bufs, src_channels, src_has_alpha,
			 x >> SCALE_SHIFT, src_width,
			 check_size, tcolor1, tcolor2, pixel_func);
	  
	  x += x_step;
	  x_start = x >> SCALE_SHIFT;
	  dest_x++;
	  outbuf += dest_channels;
	}

      new_outbuf = (*line_func)(run_weights, filter->n_x, filter->n_y,
				outbuf, dest_x,
				MIN (outbuf_end, dest_buf + dest_rowstride * i + run_end_index * dest_channels),
				dest_channels, dest_has_alpha,
				line_bufs, src_channels, src_has_alpha,
				x, x_step, src_width, check_size, tcolor1, tcolor2);

      dest_x += (new_outbuf - outbuf) / dest_channels;

      x = dest_x * x_step + scaled_x_offset;
      outbuf = new_outbuf;

      while (outbuf < outbuf_end)
	{
	  process_pixel (run_weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * (filter->n_x * filter->n_y), filter->n_x, filter->n_y,
			 outbuf, dest_x, dest_channels, dest_has_alpha,
			 line_bufs, src_channels, src_has_alpha,
			 x >> SCALE_SHIFT, src_width,
			 check_size, tcolor1, tcolor2, pixel_func);
	  
	  x += x_step;
	  dest_x++;
	  outbuf += dest_channels;
	}

      y += y_step;
    }

  g_free (line_bufs);
}

static void
tile_make_weights (PixopsFilter *filter, double x_scale, double y_scale, double overall_alpha)
{
  int i_offset, j_offset;

  int n_x = ceil(1/x_scale + 1);
  int n_y = ceil(1/y_scale + 1);

  filter->x_offset = 0;
  filter->y_offset = 0;
  filter->n_x = n_x;
  filter->n_y = n_y;
  filter->weights = g_new (int, SUBSAMPLE * SUBSAMPLE * n_x * n_y);

  for (i_offset=0; i_offset<SUBSAMPLE; i_offset++)
    for (j_offset=0; j_offset<SUBSAMPLE; j_offset++)
      {
	int *pixel_weights = filter->weights + ((i_offset*SUBSAMPLE) + j_offset) * n_x * n_y;
	double x = (double)j_offset / 16;
	double y = (double)i_offset / 16;
	int i,j;
	  
	for (i = 0; i < n_y; i++)
	  {
	    double tw, th;
		
	    if (i < y)
	      {
		if (i + 1 > y)
		  th = MIN(i+1, y + 1/y_scale) - y;
		else
		  th = 0;
	      }
	    else
	      {
		if (y + 1/y_scale > i)
		  th = MIN(i+1, y + 1/y_scale) - i;
		else
		  th = 0;
	      }
		
	    for (j = 0; j < n_x; j++)
	      {
		if (j < x)
		  {
		    if (j + 1 > x)
		      tw = MIN(j+1, x + 1/x_scale) - x;
		    else
		      tw = 0;
		  }
		else
		  {
		    if (x + 1/x_scale > j)
		      tw = MIN(j+1, x + 1/x_scale) - j;
		    else
		      tw = 0;
		  }

		*(pixel_weights + n_x * i + j) = 65536 * tw * x_scale * th * y_scale * overall_alpha;
	      }
	  }
      }
}

static void
bilinear_make_fast_weights (PixopsFilter *filter, double x_scale, double y_scale, double overall_alpha)
{
  int i_offset, j_offset;
  double *x_weights, *y_weights;
  int n_x, n_y;

  if (x_scale > 1.0)		/* Bilinear */
    {
      n_x = 2;
      filter->x_offset = 0.5 * (1/x_scale - 1);
    }
  else				/* Tile */
    {
      n_x = ceil(1.0 + 1.0/x_scale);
      filter->x_offset = 0.0;
    }

  if (y_scale > 1.0)		/* Bilinear */
    {
      n_y = 2;
      filter->y_offset = 0.5 * (1/y_scale - 1);
    }
  else				/* Tile */
    {
      n_y = ceil(1.0 + 1.0/y_scale);
      filter->y_offset = 0.0;
    }

  filter->n_y = n_y;
  filter->n_x = n_x;
  filter->weights = g_new (int, SUBSAMPLE * SUBSAMPLE * n_x * n_y);

  x_weights = g_new (double, n_x);
  y_weights = g_new (double, n_y);

  for (i_offset=0; i_offset<SUBSAMPLE; i_offset++)
    for (j_offset=0; j_offset<SUBSAMPLE; j_offset++)
      {
	int *pixel_weights = filter->weights + ((i_offset*SUBSAMPLE) + j_offset) * n_x * n_y;
	double x = (double)j_offset / 16;
	double y = (double)i_offset / 16;
	int i,j;

	if (x_scale > 1.0)	/* Bilinear */
	  {
	    for (i = 0; i < n_x; i++)
	      {
		x_weights[i] = ((i == 0) ? (1 - x) : x) / x_scale;
	      }
	  }
	else			/* Tile */
	  {
	    for (i = 0; i < n_x; i++)
	      {
		if (i < x)
		  {
		    if (i + 1 > x)
		      x_weights[i] = MIN(i+1, x + 1/x_scale) - x;
		    else
		      x_weights[i] = 0;
		  }
		else
		  {
		    if (x + 1/x_scale > i)
		      x_weights[i] = MIN(i+1, x + 1/x_scale) - i;
		    else
		      x_weights[i] = 0;
		  }
	      }
	  }

	if (y_scale > 1.0)	/* Bilinear */
	  {
	    for (i = 0; i < n_y; i++)
	      {
		y_weights[i] = ((i == 0) ? (1 - y) : y) / y_scale;
	      }
	  }
	else			/* Tile */
	  {
	    for (i = 0; i < n_y; i++)
	      {
		if (i < y)
		  {
		    if (i + 1 > y)
		      y_weights[i] = MIN(i+1, y + 1/y_scale) - y;
		    else
		      y_weights[i] = 0;
		  }
		else
		  {
		    if (y + 1/y_scale > i)
		      y_weights[i] = MIN(i+1, y + 1/y_scale) - i;
		    else
		      y_weights[i] = 0;
		  }
	      }
	  }

	for (i = 0; i < n_y; i++)
	  for (j = 0; j < n_x; j++)
	    *(pixel_weights + n_x * i + j) = 65536 * x_weights[j] * x_scale * y_weights[i] * y_scale * overall_alpha;
      }

  g_free (x_weights);
  g_free (y_weights);
}

static double
bilinear_quadrant (double bx0, double bx1, double by0, double by1)
{
  double ax0, ax1, ay0, ay1;
  double x0, x1, y0, y1;

  ax0 = 0.;
  ax1 = 1.;
  ay0 = 0.;
  ay1 = 1.;

  if (ax0 < bx0)
    {
      if (ax1 > bx0)
	{
	  x0 = bx0;
	  x1 = MIN (ax1, bx1);
	}
      else
	return 0;
    }
  else
    {
      if (bx1 > ax0)
	{
	  x0 = ax0;
	  x1 = MIN (ax1, bx1);
	}
      else
	return 0;
    }

  if (ay0 < by0)
    {
      if (ay1 > by0)
	{
	  y0 = by0;
	  y1 = MIN (ay1, by1);
	}
      else
	return 0;
    }
  else
    {
      if (by1 > ay0)
	{
	  y0 = ay0;
	  y1 = MIN (ay1, by1);
	}
      else
	return 0;
    }

  return 0.25 * (x1*x1 - x0*x0) * (y1*y1 - y0*y0);
}

static void
bilinear_make_weights (PixopsFilter *filter, double x_scale, double y_scale, double overall_alpha)
{
  int i_offset, j_offset;

  int n_x = ceil(1/x_scale + 2.0);
  int n_y = ceil(1/y_scale + 2.0);

  filter->x_offset = -1.0;
  filter->y_offset = -1.0;
  filter->n_x = n_x;
  filter->n_y = n_y;
  
  filter->weights = g_new (int, SUBSAMPLE * SUBSAMPLE * n_x * n_y);

  for (i_offset=0; i_offset<SUBSAMPLE; i_offset++)
    for (j_offset=0; j_offset<SUBSAMPLE; j_offset++)
      {
	int *pixel_weights = filter->weights + ((i_offset*SUBSAMPLE) + j_offset) * n_x * n_y;
	double x = (double)j_offset / 16;
	double y = (double)i_offset / 16;
	int i,j;
	  
	for (i = 0; i < n_y; i++)
	  for (j = 0; j < n_x; j++)
	    {
	      double w;

	      w = bilinear_quadrant  (0.5 + j - (x + 1 / x_scale), 0.5 + j - x, 0.5 + i - (y + 1 / y_scale), 0.5 + i - y);
	      w += bilinear_quadrant (1.5 + x - j, 1.5 + (x + 1 / x_scale) - j, 0.5 + i - (y + 1 / y_scale), 0.5 + i - y);
	      w += bilinear_quadrant (0.5 + j - (x + 1 / x_scale), 0.5 + j - x, 1.5 + y - i, 1.5 + (y + 1 / y_scale) - i);
	      w += bilinear_quadrant (1.5 + x - j, 1.5 + (x + 1 / x_scale) - j, 1.5 + y - i, 1.5 + (y + 1 / y_scale) - i);
	      
	      *(pixel_weights + n_x * i + j) = 65536 * w * x_scale * y_scale * overall_alpha;
	    }
      }
}

void
pixops_composite_color (art_u8         *dest_buf,
			int             render_x0,
			int             render_y0,
			int             render_x1,
			int             render_y1,
			int             dest_rowstride,
			int             dest_channels,
			art_boolean     dest_has_alpha,
			const art_u8   *src_buf,
			int             src_width,
			int             src_height,
			int             src_rowstride,
			int             src_channels,
			art_boolean     src_has_alpha,
			double          scale_x,
			double          scale_y,
			ArtFilterLevel  filter_level,
			int             overall_alpha,
			int             check_x,
			int             check_y,
			int             check_size,
			art_u32         color1,
			art_u32         color2)
{
  PixopsFilter filter;
  PixopsLineFunc line_func;
  
#ifdef USE_MMX
  art_boolean found_mmx = pixops_have_mmx();
#endif

  g_return_if_fail (!(dest_channels == 3 && dest_has_alpha));
  g_return_if_fail (!(src_channels == 3 && src_has_alpha));

  if (scale_x == 0 || scale_y == 0)
    return;

  if (!src_has_alpha && overall_alpha == 255)
    pixops_scale (dest_buf, render_x0, render_y0, render_x1, render_y1,
		  dest_rowstride, dest_channels, dest_has_alpha,
		  src_buf, src_width, src_height, src_rowstride, src_channels,
		  src_has_alpha, scale_x, scale_y, filter_level);

  switch (filter_level)
    {
    case ART_FILTER_NEAREST:
      pixops_composite_color_nearest (dest_buf, render_x0, render_y0, render_x1, render_y1,
				      dest_rowstride, dest_channels, dest_has_alpha,
				      src_buf, src_width, src_height, src_rowstride, src_channels, src_has_alpha,
				      scale_x, scale_y, overall_alpha,
				      check_x, check_y, check_size, color1, color2);
      return;

    case ART_FILTER_TILES:
      tile_make_weights (&filter, scale_x, scale_y, overall_alpha / 255.);
      break;
      
    case ART_FILTER_BILINEAR:
      bilinear_make_fast_weights (&filter, scale_x, scale_y, overall_alpha / 255.);
      break;
      
    case ART_FILTER_HYPER:
      bilinear_make_weights (&filter, scale_x, scale_y, overall_alpha / 255.);
      break;
    }

#ifdef USE_MMX
  if (filter.n_x == 2 && filter.n_y == 2 &&
      dest_channels == 4 && src_channels == 4 && src_has_alpha && !dest_has_alpha && found_mmx)
    line_func = composite_line_color_22_4a4_mmx_stub;
  else
#endif    
    line_func = composite_line_color;
  
  pixops_process (dest_buf, render_x0, render_y0, render_x1, render_y1,
		  dest_rowstride, dest_channels, dest_has_alpha,
		  src_buf, src_width, src_height, src_rowstride, src_channels,
		  src_has_alpha, scale_x, scale_y, check_x, check_y, check_size, color1, color2,
		  &filter, line_func, composite_pixel_color);

  g_free (filter.weights);
}

void
pixops_composite (art_u8        *dest_buf,
		  int            render_x0,
		  int            render_y0,
		  int            render_x1,
		  int            render_y1,
		  int            dest_rowstride,
		  int            dest_channels,
		  art_boolean    dest_has_alpha,
		  const art_u8  *src_buf,
		  int            src_width,
		  int            src_height,
		  int            src_rowstride,
		  int            src_channels,
		  art_boolean    src_has_alpha,
		  double         scale_x,
		  double         scale_y,
		  ArtFilterLevel filter_level,
		  int            overall_alpha)
{
  PixopsFilter filter;
  PixopsLineFunc line_func;
  
#ifdef USE_MMX
  art_boolean found_mmx = pixops_have_mmx();
#endif

  g_return_if_fail (!(dest_channels == 3 && dest_has_alpha));
  g_return_if_fail (!(src_channels == 3 && src_has_alpha));

  if (scale_x == 0 || scale_y == 0)
    return;

  if (!src_has_alpha && overall_alpha == 255)
    pixops_scale (dest_buf, render_x0, render_y0, render_x1, render_y1,
		  dest_rowstride, dest_channels, dest_has_alpha,
		  src_buf, src_width, src_height, src_rowstride, src_channels,
		  src_has_alpha, scale_x, scale_y, filter_level);

  switch (filter_level)
    {
    case ART_FILTER_NEAREST:
      pixops_composite_nearest (dest_buf, render_x0, render_y0, render_x1, render_y1,
				dest_rowstride, dest_channels, dest_has_alpha,
				src_buf, src_width, src_height, src_rowstride, src_channels,
				src_has_alpha, scale_x, scale_y, overall_alpha);
      return;

    case ART_FILTER_TILES:
      tile_make_weights (&filter, scale_x, scale_y, overall_alpha / 255.);
      break;
      
    case ART_FILTER_BILINEAR:
      bilinear_make_fast_weights (&filter, scale_x, scale_y, overall_alpha / 255.);
      break;
      
    case ART_FILTER_HYPER:
      bilinear_make_weights (&filter, scale_x, scale_y, overall_alpha / 255.);
      break;
    }

  if (filter.n_x == 2 && filter.n_y == 2 &&
      dest_channels == 4 && src_channels == 4 && src_has_alpha && !dest_has_alpha)
    {
#ifdef USE_MMX
      if (found_mmx)
	line_func = composite_line_22_4a4_mmx_stub;
      else
#endif	
	line_func = composite_line_22_4a4;
    }
  else
    line_func = composite_line;
  
  pixops_process (dest_buf, render_x0, render_y0, render_x1, render_y1,
		  dest_rowstride, dest_channels, dest_has_alpha,
		  src_buf, src_width, src_height, src_rowstride, src_channels,
		  src_has_alpha, scale_x, scale_y, 0, 0, 0, 0, 0, 
		  &filter, line_func, composite_pixel);

  g_free (filter.weights);
}

void
pixops_scale (art_u8        *dest_buf,
	      int            render_x0,
	      int            render_y0,
	      int            render_x1,
	      int            render_y1,
	      int            dest_rowstride,
	      int            dest_channels,
	      art_boolean    dest_has_alpha,
	      const art_u8  *src_buf,
	      int            src_width,
	      int            src_height,
	      int            src_rowstride,
	      int            src_channels,
	      art_boolean    src_has_alpha,
	      double         scale_x,
	      double         scale_y,
	      ArtFilterLevel filter_level)
{
  PixopsFilter filter;
  PixopsLineFunc line_func;

#ifdef USE_MMX
  art_boolean found_mmx = pixops_have_mmx();
#endif

  g_return_if_fail (!(dest_channels == 3 && dest_has_alpha));
  g_return_if_fail (!(src_channels == 3 && src_has_alpha));
  g_return_if_fail (!(src_has_alpha && !dest_has_alpha));

  if (scale_x == 0 || scale_y == 0)
    return;

  switch (filter_level)
    {
    case ART_FILTER_NEAREST:
      pixops_scale_nearest (dest_buf, render_x0, render_y0, render_x1, render_y1,
			    dest_rowstride, dest_channels, dest_has_alpha,
			    src_buf, src_width, src_height, src_rowstride, src_channels, src_has_alpha,
			    scale_x, scale_y);
      return;

    case ART_FILTER_TILES:
      tile_make_weights (&filter, scale_x, scale_y, 1.0);
      break;
      
    case ART_FILTER_BILINEAR:
      bilinear_make_fast_weights (&filter, scale_x, scale_y, 1.0);
      break;
      
    case ART_FILTER_HYPER:
      bilinear_make_weights (&filter, scale_x, scale_y, 1.0);
      break;
    }

#ifdef USE_MMX
  if (filter.n_x == 2 && filter.n_y == 2 &&
      found_mmx && dest_channels == 3 && src_channels == 3)
    line_func = scale_line_22_33_mmx_stub;
  else
#endif    
    line_func = scale_line;
  
  pixops_process (dest_buf, render_x0, render_y0, render_x1, render_y1,
		  dest_rowstride, dest_channels, dest_has_alpha,
		  src_buf, src_width, src_height, src_rowstride, src_channels,
		  src_has_alpha, scale_x, scale_y, 0, 0, 0, 0, 0,
		  &filter, line_func, scale_pixel);

  g_free (filter.weights);
}

