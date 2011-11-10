#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <cairo.h>

#include "broadway.h"

/************************************************************************
 *                Base64 functions                                      *
 ************************************************************************/

static const char base64_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#if 0 /* Unused for now */
static void
base64_uint8 (guint8 v, char *c)
{
  c[0] = base64_alphabet[(v >> 0) & 0x3f];
  c[1] = base64_alphabet[(v >> 6) & 0x3];
}
#endif

static void
base64_uint16 (guint32 v, char *c)
{
  c[0] = base64_alphabet[(v >> 0) & 0x3f];
  c[1] = base64_alphabet[(v >> 6) & 0x3f];
  c[2] = base64_alphabet[(v >> 12) & 0xf];
}

#if 0
static void
base64_uint24 (guint32 v, char *c)
{
  c[0] = base64_alphabet[(v >> 0) & 0x3f];
  c[1] = base64_alphabet[(v >> 6) & 0x3f];
  c[2] = base64_alphabet[(v >> 12) & 0x3f];
  c[3] = base64_alphabet[(v >> 18) & 0x3f];
}
#endif

static void
base64_uint32 (guint32 v, char *c)
{
  c[0] = base64_alphabet[(v >> 0) & 0x3f];
  c[1] = base64_alphabet[(v >> 6) & 0x3f];
  c[2] = base64_alphabet[(v >> 12) & 0x3f];
  c[3] = base64_alphabet[(v >> 18) & 0x3f];
  c[4] = base64_alphabet[(v >> 24) & 0x3f];
  c[5] = base64_alphabet[(v >> 30) & 0x2];
}

/***********************************************************
 *  conversion of raw image data to png data: uris         *
 ***********************************************************/

struct PngTarget {
  GString *url;
  int state;
  int save;
};

static cairo_status_t
write_png_url (void		  *closure,
	       const unsigned char *data,
	       unsigned int	   data_len)
{
  struct PngTarget *target = closure;
  gsize res, old_len;

  old_len = target->url->len;
  g_string_set_size (target->url,
		     old_len + (data_len / 3 + 1) * 4 + 4);

  res = g_base64_encode_step (data, data_len, FALSE,
			      target->url->str + old_len,
			      &target->state, &target->save);

  g_string_set_size (target->url,  old_len + res);

  return CAIRO_STATUS_SUCCESS;
}

static char *
to_png_rgb (int w, int h, int byte_stride, guint32 *data)
{
  cairo_surface_t *surface;
  struct PngTarget target;
  gsize res, old_len;

  target.url = g_string_new ("data:image/png;base64,");
  target.state = 0;
  target.save = 0;

  surface = cairo_image_surface_create_for_data ((guchar *)data,
						 CAIRO_FORMAT_RGB24, w, h, byte_stride);

  cairo_surface_write_to_png_stream (surface, write_png_url, &target);

  old_len = target.url->len;

  g_string_set_size (target.url, old_len + 4);
  res = g_base64_encode_close (FALSE,
			       target.url->str + old_len,
			       &target.state, &target.save);
  g_string_set_size (target.url, old_len + res);

  return g_string_free (target.url, FALSE);
}

static char *
to_png_rgba (int w, int h, int byte_stride, guint32 *data)
{
  cairo_surface_t *surface;
  struct PngTarget target;
  gsize res, old_len;

  target.url = g_string_new ("data:image/png;base64,");
  target.state = 0;
  target.save = 0;

  surface = cairo_image_surface_create_for_data ((guchar *)data,
						 CAIRO_FORMAT_ARGB32, w, h, byte_stride);

  cairo_surface_write_to_png_stream (surface, write_png_url, &target);

  old_len = target.url->len;

  g_string_set_size (target.url, old_len + 4);
  res = g_base64_encode_close (FALSE,
			       target.url->str + old_len,
			       &target.state, &target.save);
  g_string_set_size (target.url, old_len + res);

  return g_string_free (target.url, FALSE);
}

#if 0
static char *
to_png_a (int w, int h, int byte_stride, guint8 *data)
{
  cairo_surface_t *surface;
  struct PngTarget target;
  gsize res, old_len;

  target.url = g_string_new ("data:image/png;base64,");
  target.state = 0;
  target.save = 0;

  surface = cairo_image_surface_create_for_data (data,
						 CAIRO_FORMAT_A8, w, h, byte_stride);

  cairo_surface_write_to_png_stream (surface, write_png_url, &target);

  old_len = target.url->len;

  g_string_set_size (target.url, old_len + 4);
  res = g_base64_encode_close (FALSE,
			       target.url->str + old_len,
			       &target.state, &target.save);
  g_string_set_size (target.url, old_len + res);

  return g_string_free (target.url, FALSE);
}
#endif

/************************************************************************
 *                Basic I/O primitives                                  *
 ************************************************************************/

struct BroadwayOutput {
  GOutputStream *out;
  int error;
  guint32 serial;
  gboolean proto_v7_plus;
};

static void
broadway_output_send_cmd (BroadwayOutput *output,
			  gboolean fin, BroadwayWSOpCode code,
			  const void *buf, gsize count)
{
  gboolean mask = FALSE;
  guchar header[16];
  size_t p;

  gboolean mid_header = count > 125 && count <= 65535;
  gboolean long_header = count > 65535;

  /* NB. big-endian spec => bit 0 == MSB */
  header[0] = ( (fin ? 0x80 : 0) | (code & 0x0f) );
  header[1] = ( (mask ? 0x80 : 0) |
                (mid_header ? 126 : long_header ? 127 : count) );
  p = 2;
  if (mid_header)
    {
      *(guint16 *)(header + p) = GUINT16_TO_BE( (guint16)count );
      p += 2;
    }
  else if (long_header)
    {
      *(guint64 *)(header + p) = GUINT64_TO_BE( count );
      p += 8;
    }
  // FIXME: if we are paranoid we should 'mask' the data
  // FIXME: we should really emit these as a single write
  g_output_stream_write_all (output->out, header, p, NULL, NULL, NULL);
  g_output_stream_write_all (output->out, buf, count, NULL, NULL, NULL);
}

static void
broadway_output_sendmsg (BroadwayOutput *output,
			 const void *buf, gsize count)
{
  if (!output->proto_v7_plus)
    g_output_stream_write_all (output->out, buf, count, NULL, NULL, NULL);
  else
    broadway_output_send_cmd (output, TRUE, BROADWAY_WS_TEXT, buf, count);
}

void broadway_output_pong (BroadwayOutput *output)
{
  if (output->proto_v7_plus)
    broadway_output_send_cmd (output, TRUE, BROADWAY_WS_CNX_PONG, NULL, 0);
}

static void
broadway_output_sendmsg_initiate (BroadwayOutput *output)
{
  if (!output->proto_v7_plus)
    g_output_stream_write (output->out, "\0", 1, NULL, NULL);
  else
    {
    }
}

int
broadway_output_flush (BroadwayOutput *output)
{
  if (!output->proto_v7_plus)
    {
      broadway_output_sendmsg (output, "\xff", 1);
      broadway_output_sendmsg (output, "\0", 1);
      return !output->error;
    }
  else /* no need to flush */
    return !output->error;
}

BroadwayOutput *
broadway_output_new(GOutputStream *out, guint32 serial,
		    gboolean proto_v7_plus)
{
  BroadwayOutput *output;

  output = g_new0 (BroadwayOutput, 1);

  output->out = g_object_ref (out);
  output->serial = serial;
  output->proto_v7_plus = proto_v7_plus;

  broadway_output_sendmsg_initiate (output);

  return output;
}

void
broadway_output_free (BroadwayOutput *output)
{
  g_object_unref (output->out);
  free (output);
}

guint32
broadway_output_get_next_serial (BroadwayOutput *output)
{
  return output->serial;
}


/************************************************************************
 *                     Core rendering operations                        *
 ************************************************************************/

#define HEADER_LEN (1+6)

static void
append_uint16 (guint32 v, char *buf, int *p)
{
  base64_uint16 (v, &buf[*p]);
  *p += 3;
}

static void
append_uint32 (guint32 v, char *buf, int *p)
{
  base64_uint32 (v, &buf[*p]);
  *p += 6;
}

static int
write_header(BroadwayOutput *output, char *buf, char op)
{
  int p;

  p = 0;
  buf[p++] = op;
  append_uint32 (output->serial++, buf, &p);

  return p;
}

void
broadway_output_copy_rectangles (BroadwayOutput *output,  int id,
				 BroadwayRect *rects, int n_rects,
				 int dx, int dy)
{
  char *buf;
  int len, i, p;

  len = HEADER_LEN + 3 + 3 + 3*4*n_rects + 3 + 3;

  buf = g_malloc (len);
  p = write_header (output, buf, 'b');
  append_uint16 (id, buf, &p);
  append_uint16 (n_rects, buf, &p);
  for (i = 0; i < n_rects; i++)
    {
      append_uint16 (rects[i].x, buf, &p);
      append_uint16 (rects[i].y, buf, &p);
      append_uint16 (rects[i].width, buf, &p);
      append_uint16 (rects[i].height, buf, &p);
    }
  append_uint16 (dx, buf, &p);
  append_uint16 (dy, buf, &p);

  assert (p == len);

  broadway_output_sendmsg (output, buf, len);
  free (buf);
}

void
broadway_output_grab_pointer (BroadwayOutput *output,
			      int id,
			      gboolean owner_event)
{
  char buf[HEADER_LEN + 3 + 1];
  int p;

  p = write_header (output, buf, 'g');
  append_uint16 (id, buf, &p);
  buf[p++] = owner_event ? '1': '0';

  assert (p == sizeof (buf));

  broadway_output_sendmsg (output, buf, sizeof (buf));
}

guint32
broadway_output_ungrab_pointer (BroadwayOutput *output)
{
  char buf[HEADER_LEN];
  guint32 serial;
  int p;

  serial = output->serial;
  p = write_header (output, buf, 'u');

  assert (p == sizeof (buf));

  broadway_output_sendmsg (output, buf, sizeof (buf));

  return serial;
}

void
broadway_output_new_surface(BroadwayOutput *output,
			    int id, int x, int y, int w, int h,
			    gboolean is_temp)
{
  char buf[HEADER_LEN + 16];
  int p;

  p = write_header (output, buf, 's');
  append_uint16 (id, buf, &p);
  append_uint16 (x, buf, &p);
  append_uint16 (y, buf, &p);
  append_uint16 (w, buf, &p);
  append_uint16 (h, buf, &p);
  buf[p++] = is_temp ? '1' : '0';

  assert (p == sizeof (buf));

  broadway_output_sendmsg (output, buf, sizeof (buf));
}

void
broadway_output_show_surface(BroadwayOutput *output,  int id)
{
  char buf[HEADER_LEN + 3];
  int p;

  p = write_header (output, buf, 'S');
  append_uint16 (id, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_sendmsg (output, buf, sizeof (buf));
}

void
broadway_output_hide_surface(BroadwayOutput *output,  int id)
{
  char buf[HEADER_LEN + 3];
  int p;

  p = write_header (output, buf, 'H');
  append_uint16 (id, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_sendmsg (output, buf, sizeof (buf));
}

void
broadway_output_destroy_surface(BroadwayOutput *output,  int id)
{
  char buf[HEADER_LEN + 3];
  int p;

  p = write_header (output, buf, 'd');
  append_uint16 (id, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_sendmsg (output, buf, sizeof (buf));
}


void
broadway_output_move_resize_surface (BroadwayOutput *output,
				     int             id,
				     gboolean        has_pos,
				     int             x,
				     int             y,
				     gboolean        has_size,
				     int             w,
				     int             h)
{
  char buf[HEADER_LEN+3+1+6+6];
  int p;
  int val;

  if (!has_pos && !has_size)
    return;

  p = write_header (output, buf, 'm');

  val = (!!has_pos) | ((!!has_size) << 1);
  append_uint16 (id, buf, &p);
  buf[p++] = val + '0';
  if (has_pos)
    {
      append_uint16 (x, buf, &p);
      append_uint16 (y, buf, &p);
    }
  if (has_size)
    {
      append_uint16 (w, buf, &p);
      append_uint16 (h, buf, &p);
    }
  assert (p <= sizeof (buf));

  broadway_output_sendmsg (output, buf, p);
}

void
broadway_output_set_transient_for (BroadwayOutput *output,
				   int             id,
				   int             parent_id)
{
  char buf[HEADER_LEN + 6];
  int p;

  p = write_header (output, buf, 'p');

  append_uint16 (id, buf, &p);
  append_uint16 (parent_id, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_sendmsg (output, buf, sizeof (buf));
}


void
broadway_output_put_rgb (BroadwayOutput *output,  int id, int x, int y,
			 int w, int h, int byte_stride, void *data)
{
  gsize buf_size;
  gsize url_len;
  char *url, *buf;
  int p;

  url = to_png_rgb (w, h, byte_stride, (guint32*)data);
  url_len = strlen (url);

  buf_size = HEADER_LEN + 15 + url_len;
  buf = g_malloc (buf_size);

  p = write_header (output, buf, 'i');

  append_uint16 (id, buf, &p);
  append_uint16 (x, buf, &p);
  append_uint16 (y, buf, &p);

  append_uint32 (url_len, buf, &p);

  g_assert (p == HEADER_LEN + 15);
  strncpy (buf + p, url, url_len);

  broadway_output_sendmsg (output, buf, buf_size);

  g_free (buf);
  free (url);
}

typedef struct  {
  int x1, y1;
  int x2, y2;
} BroadwayBox;

static int
is_any_x_set (unsigned char *data,
	      int box_x1, int box_x2,
	      int x1, int x2, int y, int *x_set,
	      int byte_stride)
{
  int w ;
  guint32 *ptr;

  if (x1 < box_x1)
    x1 = box_x1;

  if (x2 > box_x2)
    x2 = box_x2;

  w = x2 - x1;
  if (w > 0)
    {
      ptr = (guint32 *)(data + y * byte_stride + x1 * 4);
      while (w-- > 0)
	{
	  if (*ptr != 0)
	    {
	      if (x_set)
		*x_set = x1;
	      return 1;
	    }
	  ptr++;
	  x1++;
	}
    }
  return 0;
}


#define EXTEND_X_FUZZ 10
#define EXTEND_Y_FUZZ 10

static int
extend_x_range (unsigned char *data,
		int box_x1, int box_y1,
		int box_x2, int box_y2,
		int *x1, int *x2, int y,
		int byte_stride)
{
  int extended = 0;
  int new_x;

  while (is_any_x_set (data, box_x1, box_x2, *x1 - EXTEND_X_FUZZ, *x1, y, &new_x, byte_stride))
    {
      *x1 = new_x;
      extended = 1;
    }

  while (is_any_x_set (data, box_x1, box_x2, *x2, *x2 + EXTEND_X_FUZZ, y, &new_x, byte_stride))
    {
      *x2 = new_x + 1;
      extended = 1;
    }

  return extended;
}

static int
extend_y_range (unsigned char *data,
		int box_x1, int box_y1,
		int box_x2, int box_y2,
		int x1, int x2, int *y,
		int byte_stride)
{
  int extended = 0;
  int found_set;
  int yy, y2;

  while (*y < box_y2)
    {
      found_set = 0;

      y2 = *y + EXTEND_Y_FUZZ;
      if (y2 > box_y2)
	y2 = box_y2;

      for (yy = y2; yy > *y + 1; yy--)
	{
	  if (is_any_x_set (data, box_x1, box_x2, x1, x2, yy - 1, NULL, byte_stride))
	    {
	      found_set = 1;
	      break;
	    }
	}
      if (!found_set)
	break;
      *y = yy;
      extended = 1;
    }

  return extended;
}


static void
rgba_find_rects_extents (unsigned char *data,
			 int box_x1, int box_y1,
			 int box_x2, int box_y2,
			 int x, int y,
			 BroadwayBox *rect,
			 int byte_stride)
{
  int x1, x2, y1, y2, yy;
  int extended;

  x1 = x;
  x2 = x + 1;
  y1 = y;
  y2 = y + 1;

  do
    {
      /* Expand maximally for all known rows */
      do
	{
	  extended = 0;

	  for (yy = y1; yy < y2; yy++)
	    extended |= extend_x_range (data,
					box_x1, box_y1,
					box_x2, box_y2,
					&x1, &x2, yy,
					byte_stride);
	}
      while (extended);
    }
  while (extend_y_range(data,
			box_x1, box_y1,
			box_x2, box_y2,
			x1, x2, &y2,
			byte_stride));

  rect->x1 = x1;
  rect->x2 = x2;
  rect->y1 = y1;
  rect->y2 = y2;
}

static void
rgba_find_rects_sub (unsigned char *data,
		     int box_x1, int box_y1,
		     int box_x2, int box_y2,
		     int byte_stride,
		     BroadwayBox **rects,
		     int *n_rects, int *alloc_rects)
{
  guint32 *line;
  BroadwayBox rect;
  int x, y;

  if (box_x1 == box_x2 || box_y1 == box_y2)
    return;

  for (y = box_y1; y < box_y2; y++)
    {
      line = (guint32 *)(data + y * byte_stride + box_x1 * 4);

      for (x = box_x1; x < box_x2; x++)
	{
	  if (*line != 0)
	    {
	      rgba_find_rects_extents (data,
				       box_x1, box_y1, box_x2, box_y2,
				       x, y, &rect, byte_stride);
	      if (*n_rects == *alloc_rects)
		{
		  (*alloc_rects) *= 2;
		  *rects = g_renew (BroadwayBox, *rects, *alloc_rects);
		}
	      (*rects)[*n_rects] = rect;
	      (*n_rects)++;
	      rgba_find_rects_sub (data,
				   box_x1, rect.y1,
				   rect.x1, rect.y2,
				   byte_stride,
				   rects, n_rects, alloc_rects);
	      rgba_find_rects_sub (data,
				   rect.x2, rect.y1,
				   box_x2, rect.y2,
				   byte_stride,
				   rects, n_rects, alloc_rects);
	      rgba_find_rects_sub (data,
				   box_x1, rect.y2,
				   box_x2, box_y2,
				   byte_stride,
				   rects, n_rects, alloc_rects);
	      return;
	    }
	  line++;
	}
    }
}

static BroadwayBox *
rgba_find_rects (unsigned char *data,
		 int w, int h, int byte_stride,
		 int *n_rects)
{
  BroadwayBox *rects;
  int alloc_rects;

  alloc_rects = 20;
  rects = g_new (BroadwayBox, alloc_rects);

  *n_rects = 0;
  rgba_find_rects_sub (data,
		       0, 0, w, h, byte_stride,
		       &rects, n_rects, &alloc_rects);

  return rects;
}

void
broadway_output_put_rgba (BroadwayOutput *output,  int id, int x, int y,
			  int w, int h, int byte_stride, void *data)
{
  BroadwayBox *rects;
  int p, i, n_rects;

  rects = rgba_find_rects (data, w, h, byte_stride, &n_rects);

  for (i = 0; i < n_rects; i++)
    {
      gsize url_len, buf_size;
      char *buf, *url;
      guint8 *subdata;

      subdata = (guint8 *)data + rects[i].x1 * 4 + rects[i].y1 * byte_stride;
      url = to_png_rgba (rects[i].x2 - rects[i].x1,
			 rects[i].y2 - rects[i].y1,
			 byte_stride, (guint32*)subdata);

      url_len = strlen (url);
      buf_size = HEADER_LEN + 15 + url_len;
      buf = g_malloc (buf_size);


      p = write_header (output, buf, 'i');
      append_uint16 (id, buf, &p);
      append_uint16 (x + rects[i].x1, buf, &p);
      append_uint16 (y + rects[i].y1, buf, &p);

      append_uint32 (url_len, buf, &p);
      g_assert (p == HEADER_LEN + 15);
      strncpy (buf + p, url, url_len);

      broadway_output_sendmsg (output, buf, buf_size);

      free (url);
      g_free (buf);
    }

  free (rects);
}

void
broadway_output_surface_flush (BroadwayOutput *output,
			       int             id)
{
  char buf[HEADER_LEN + 3];
  int p;

  p = write_header (output, buf, 'f');
  append_uint16 (id, buf, &p);

  g_assert (p == sizeof (buf));

  broadway_output_sendmsg (output, buf, sizeof (buf));
}
