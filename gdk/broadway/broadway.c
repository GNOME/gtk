#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

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

/************************************************************************
 *  conversion of raw image data to uncompressed png data: uris         *
 ************************************************************************/

/* Table of CRCs of all 8-bit messages. */
static unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
static int crc_table_computed = 0;

/* Make the table for a fast CRC. */
static void
make_crc_table(void)
{
  unsigned long c;
  int n, k;

  for (n = 0; n < 256; n++) {
    c = (unsigned long) n;
    for (k = 0; k < 8; k++) {
      if (c & 1)
	c = 0xedb88320L ^ (c >> 1);
      else
	c = c >> 1;
    }
    crc_table[n] = c;
  }
  crc_table_computed = 1;
}

static unsigned long
update_crc(unsigned long crc, unsigned char *buf, int len)
{
  unsigned long c = crc;
  int n;

  if (!crc_table_computed)
    make_crc_table();
  for (n = 0; n < len; n++) {
    c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
  }
  return c;
}

static unsigned long
crc(unsigned char *buf, int len)
{
  return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

#define BASE 65521 /* largest prime smaller than 65536 */
static unsigned long
update_adler32(unsigned long adler, unsigned char *buf, int len)
{
  unsigned long s1 = adler & 0xffff;
  unsigned long s2 = (adler >> 16) & 0xffff;
  int n;

  for (n = 0; n < len; n++) {
    s1 = (s1 + buf[n]) % BASE;
    s2 = (s2 + s1)     % BASE;
  }
  return (s2 << 16) + s1;
}

static char *
to_png_rgb (int w, int h, int byte_stride, guint32 *data)
{
  guchar header[] = {137, 80, 78, 71, 13, 10, 26, 10};
  guchar ihdr[13+12] = {0, 0, 0, 13, 'I', 'H', 'D', 'R',
			/* w: */ 0, 0, 0, 0, /* h: */ 0,0,0,0,
			/* bpp: */ 8, /* color type: */ 2,
			0, 0, 0};
  guchar idat_start[8] = { /* len: */0, 0, 0, 0,   'I', 'D', 'A', 'T' };
  guchar iend[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82};
  gsize data_size, row_size;
  char row_header[6];
  guint8 *png, *p, *p_row, *p_idat;
  guint32 *row;
  unsigned long adler;
  guint32 pixel;
  gsize png_size;
  int x, y;
  char *url, *url_base64;
  int state = 0, outlen;
  int save = 0;

  *(guint32 *)&ihdr[8] = GUINT32_TO_BE(w);
  *(guint32 *)&ihdr[12] = GUINT32_TO_BE(h);
  *(guint32 *)&ihdr[21] = GUINT32_TO_BE(crc(&ihdr[4], 13 + 4));

  row_size = 1 + w * 3;
  row_header[0] = 0;
  row_header[1] = row_size & 0xff;
  row_header[2] = (row_size >> 8) & 0xff;
  row_header[3] = ~row_header[1];
  row_header[4] = ~row_header[2];
  row_header[5] = 0;

  data_size = 2 + (6 + w * 3) * h + 4;

  *(guint32 *)&idat_start[0] = GUINT32_TO_BE(data_size);

  png_size = sizeof(header) + sizeof(ihdr) + 12 + data_size + sizeof(iend);
  png = g_malloc (png_size);

  p = png;
  memcpy (p, header, sizeof(header));
  p += sizeof(header);
  memcpy (p, ihdr, sizeof(ihdr));
  p += sizeof(ihdr);
  memcpy (p, idat_start, sizeof(idat_start));
  p += sizeof(idat_start);

  /* IDAT data:

     zlib header:  0x78, 0x01 ,
     h * scanline: row_header[] + width * r,g,b
     checksum: adler32
  */

  p_idat = p - 4;

  /* zlib header */
  *p++ = 0x78;
  *p++ = 0x01;

  adler = 1;

  /* scanline data */
  for (y = 0; y < h; y++) {
    if (y == h - 1)
      row_header[0] = 1; /* final block */
    memcpy (p, row_header, sizeof(row_header));
    p += sizeof(row_header);
    p_row = p - 1;
    row = data;
    data += byte_stride / 4;
    for (x = 0; x < w; x++) {
      pixel = *row++;
      *p++ = (pixel >> 16) & 0xff; /* red */
      *p++ = (pixel >> 8) & 0xff; /* green */
      *p++ = (pixel >> 0) & 0xff; /* blue */
    }
    adler = update_adler32(adler, p_row, p - p_row);
  }

  /* adler32 */
  *(guint32 *)p = GUINT32_TO_BE(adler);
  p += 4;
  *(guint32 *)p = GUINT32_TO_BE(crc(p_idat, p - p_idat));
  p += 4;

  memcpy (p, iend, sizeof(iend));
  p += sizeof(iend);

  assert(p - png == png_size);

  url = g_malloc (strlen("data:image/png;base64,") +
		   ((png_size / 3 + 1) * 4 + 4) + 1);
  strcpy (url, "data:image/png;base64,");

  url_base64 = url + strlen("data:image/png;base64,");
  outlen = g_base64_encode_step (png, png_size, FALSE, url_base64, &state, &save);
  outlen += g_base64_encode_close (FALSE, url_base64 + outlen, &state, &save);
  url_base64[outlen] = 0;

  free (png);

  return url;
}

static char *
to_png_rgba (int w, int h, int byte_stride, guint32 *data)
{
  guchar header[] = {137, 80, 78, 71, 13, 10, 26, 10};
  guchar ihdr[13+12] = {0, 0, 0, 13, 'I', 'H', 'D', 'R',
			/* w: */ 0, 0, 0, 0, /* h: */ 0,0,0,0,
			/* bpp: */ 8, /* color type: */ 6,
			0, 0, 0};
  guchar idat_start[8] = { /* len: */0, 0, 0, 0,   'I', 'D', 'A', 'T' };
  guchar iend[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82};
  gsize data_size, row_size;
  char row_header[6];
  guint8 *png, *p, *p_row, *p_idat;
  guint32 *row;
  unsigned long adler;
  guint32 pixel;
  gsize png_size;
  int x, y;
  char *url, *url_base64;
  int state = 0, outlen;
  int save = 0;

  *(guint32 *)&ihdr[8] = GUINT32_TO_BE(w);
  *(guint32 *)&ihdr[12] = GUINT32_TO_BE(h);
  *(guint32 *)&ihdr[21] = GUINT32_TO_BE(crc(&ihdr[4], 13 + 4));

  row_size = 1 + w * 4;
  row_header[0] = 0;
  row_header[1] = row_size & 0xff;
  row_header[2] = (row_size >> 8) & 0xff;
  row_header[3] = ~row_header[1];
  row_header[4] = ~row_header[2];
  row_header[5] = 0;

  data_size = 2 + (6 + w * 4) * h + 4;

  *(guint32 *)&idat_start[0] = GUINT32_TO_BE(data_size);

  png_size = sizeof(header) + sizeof(ihdr) + 12 + data_size + sizeof(iend);
  png = g_malloc (png_size);

  p = png;
  memcpy (p, header, sizeof(header));
  p += sizeof(header);
  memcpy (p, ihdr, sizeof(ihdr));
  p += sizeof(ihdr);
  memcpy (p, idat_start, sizeof(idat_start));
  p += sizeof(idat_start);

  /* IDAT data:

     zlib header:  0x78, 0x01 ,
     h * scanline: row_header[] + width * r,g,b,a
     checksum: adler32
  */

  p_idat = p - 4;

  /* zlib header */
  *p++ = 0x78;
  *p++ = 0x01;

  adler = 1;

  /* scanline data */
  for (y = 0; y < h; y++) {
    if (y == h - 1)
      row_header[0] = 1; /* final block */
    memcpy (p, row_header, sizeof(row_header));
    p += sizeof(row_header);
    p_row = p - 1;
    row = data;
    data += byte_stride / 4;
    for (x = 0; x < w; x++) {
      pixel = *row++;
      *p++ = (pixel >> 16) & 0xff; /* red */
      *p++ = (pixel >> 8) & 0xff; /* green */
      *p++ = (pixel >> 0) & 0xff; /* blue */
      *p++ = (pixel >> 24) & 0xff; /* alpha */
    }
    adler = update_adler32(adler, p_row, p - p_row);
  }

  /* adler32 */
  *(guint32 *)p = GUINT32_TO_BE(adler);
  p += 4;
  *(guint32 *)p = GUINT32_TO_BE(crc(p_idat, p - p_idat));
  p += 4;

  memcpy (p, iend, sizeof(iend));
  p += sizeof(iend);

  assert(p - png == png_size);

  url = g_malloc (strlen("data:image/png;base64,") +
		   ((png_size / 3 + 1) * 4 + 4) + 1);
  strcpy (url, "data:image/png;base64,");

  url_base64 = url + strlen("data:image/png;base64,");
  outlen = g_base64_encode_step (png, png_size, FALSE, url_base64, &state, &save);
  outlen += g_base64_encode_close (FALSE, url_base64 + outlen, &state, &save);
  url_base64[outlen] = 0;

  free (png);

  return url;
}

#if 0
static char *
to_png_a (int w, int h, int byte_stride, guint8 *data)
{
  guchar header[] = {137, 80, 78, 71, 13, 10, 26, 10};
  guchar ihdr[13+12] = {0, 0, 0, 13, 'I', 'H', 'D', 'R',
			/* w: */ 0, 0, 0, 0, /* h: */ 0,0,0,0,
			/* bpp: */ 8, /* color type: */ 4,
			0, 0, 0};
  guchar idat_start[8] = { /* len: */0, 0, 0, 0,   'I', 'D', 'A', 'T' };
  guchar iend[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82};
  gsize data_size, row_size;
  char row_header[6];
  guint8 *png, *p, *p_row, *p_idat;
  guint8 *row;
  unsigned long adler;
  guint32 pixel;
  gsize png_size;
  int x, y;
  char *url, *url_base64;
  int state = 0, outlen;
  int save = 0;

  *(guint32 *)&ihdr[8] = GUINT32_TO_BE(w);
  *(guint32 *)&ihdr[12] = GUINT32_TO_BE(h);
  *(guint32 *)&ihdr[21] = GUINT32_TO_BE(crc(&ihdr[4], 13 + 4));

  row_size = 1 + w * 2;
  row_header[0] = 0;
  row_header[1] = row_size & 0xff;
  row_header[2] = (row_size >> 8) & 0xff;
  row_header[3] = ~row_header[1];
  row_header[4] = ~row_header[2];
  row_header[5] = 0;

  data_size = 2 + (6 + w * 2) * h + 4;

  *(guint32 *)&idat_start[0] = GUINT32_TO_BE(data_size);

  png_size = sizeof(header) + sizeof(ihdr) + 12 + data_size + sizeof(iend);
  png = g_malloc (png_size);

  p = png;
  memcpy (p, header, sizeof(header));
  p += sizeof(header);
  memcpy (p, ihdr, sizeof(ihdr));
  p += sizeof(ihdr);
  memcpy (p, idat_start, sizeof(idat_start));
  p += sizeof(idat_start);

  /* IDAT data:

     zlib header:  0x78, 0x01 ,
     h * scanline: row_header[] + width * r,g,b,a
     checksum: adler32
  */

  p_idat = p - 4;

  /* zlib header */
  *p++ = 0x78;
  *p++ = 0x01;

  adler = 1;

  /* scanline data */
  for (y = 0; y < h; y++) {
    if (y == h - 1)
      row_header[0] = 1; /* final block */
    memcpy (p, row_header, sizeof(row_header));
    p += sizeof(row_header);
    p_row = p - 1;
    row = data;
    data += byte_stride / 4;
    for (x = 0; x < w; x++) {
      pixel = *row++;
      *p++ = 0x00; /* gray */
      *p++ = pixel; /* alpha */
    }
    adler = update_adler32(adler, p_row, p - p_row);
  }

  /* adler32 */
  *(guint32 *)p = GUINT32_TO_BE(adler);
  p += 4;
  *(guint32 *)p = GUINT32_TO_BE(crc(p_idat, p - p_idat));
  p += 4;

  memcpy (p, iend, sizeof(iend));
  p += sizeof(iend);

  assert(p - png == png_size);

  url = g_malloc (strlen("data:image/png;base64,") +
		  ((png_size / 3 + 1) * 4 + 4) + 1);
  strcpy (url, "data:image/png;base64,");

  url_base64 = url + strlen("data:image/png;base64,");
  outlen = g_base64_encode_step (png, png_size, FALSE, url_base64, &state, &save);
  outlen += g_base64_encode_close (FALSE, url_base64 + outlen, &state, &save);
  url_base64[outlen] = 0;

  free (png);

  return url;
}
#endif

/************************************************************************
 *                Basic I/O primitives                                  *
 ************************************************************************/

struct BroadwayOutput {
  int fd;
  gzFile *zfd;
  int error;
  guint32 serial;
};

static void
broadway_output_write_raw (BroadwayOutput *output,
			   const void *buf, gsize count)
{
  gssize res;
  int errsave;
  const char *ptr = (const char *)buf;

  if (output->error)
    return;

  while (count > 0)
    {
      res = write(output->fd, ptr, count);
      if (res == -1)
	{
	  errsave = errno;
	  if (errsave == EINTR)
	    continue;
	  output->error = TRUE;
	  return;
	}
      if (res == 0)
	{
	  output->error = TRUE;
	  return;
	}
      count -= res;
      ptr += res;
    }
}

static void
broadway_output_write (BroadwayOutput *output,
		       const void *buf, gsize count)
{
  gssize res;
  const char *ptr = (const char *)buf;

  if (output->error)
    return;

  while (count > 0)
    {
      res = gzwrite(output->zfd, ptr, count);
      if (res == -1)
	{
	  output->error = TRUE;
	  return;
	}
      if (res == 0)
	{
	  output->error = TRUE;
	  return;
	}
      count -= res;
      ptr += res;
    }
}

static void
broadway_output_write_header (BroadwayOutput *output)
{
  char *header;

  header =
    "HTTP/1.1 200 OK\r\n"
    "Content-type: multipart/x-mixed-replace;boundary=x\r\n"
    "Content-Encoding: gzip\r\n"
    "\r\n";
  broadway_output_write_raw (output,
			     header, strlen (header));
}

static void
send_boundary (BroadwayOutput *output)
{
  char *boundary =
    "--x\r\n"
    "\r\n";

  broadway_output_write (output, boundary, strlen (boundary));
}

BroadwayOutput *
broadway_output_new(int fd, guint32 serial)
{
  BroadwayOutput *output;
  int flag = 1;

  output = g_new0 (BroadwayOutput, 1);

  output->fd = fd;
  output->serial = serial;

  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

  broadway_output_write_header (output);

  output->zfd = gzdopen(fd, "wb");

  /* Need an initial multipart boundary */
  send_boundary (output);

  return output;
}

void
broadway_output_free (BroadwayOutput *output)
{
  if (output->zfd)
    gzclose (output->zfd);
  else
    close (output->fd);
  free (output);
}

guint32
broadway_output_get_next_serial (BroadwayOutput *output)
{
  return output->serial;
}

int
broadway_output_flush (BroadwayOutput *output)
{
  send_boundary (output);
  gzflush (output->zfd, Z_SYNC_FLUSH);
  return !output->error;
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

  broadway_output_write (output, buf, len);
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

  broadway_output_write (output, buf, sizeof (buf));
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

  broadway_output_write (output, buf, sizeof (buf));

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

  broadway_output_write (output, buf, sizeof (buf));
}

void
broadway_output_show_surface(BroadwayOutput *output,  int id)
{
  char buf[HEADER_LEN + 3];
  int p;

  p = write_header (output, buf, 'S');
  append_uint16 (id, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_write (output, buf, sizeof (buf));
}

void
broadway_output_hide_surface(BroadwayOutput *output,  int id)
{
  char buf[HEADER_LEN + 3];
  int p;

  p = write_header (output, buf, 'H');
  append_uint16 (id, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_write (output, buf, sizeof (buf));
}

void
broadway_output_destroy_surface(BroadwayOutput *output,  int id)
{
  char buf[HEADER_LEN + 3];
  int p;

  p = write_header (output, buf, 'd');
  append_uint16 (id, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_write (output, buf, sizeof (buf));
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

  broadway_output_write (output, buf, p);
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

  broadway_output_write (output, buf, sizeof (buf));
}


void
broadway_output_put_rgb (BroadwayOutput *output,  int id, int x, int y,
			 int w, int h, int byte_stride, void *data)
{
  char buf[HEADER_LEN + 15];
  gsize len;
  char *url;
  int p;

  p = write_header (output, buf, 'i');

  append_uint16 (id, buf, &p);
  append_uint16 (x, buf, &p);
  append_uint16 (y, buf, &p);

  url = to_png_rgb (w, h, byte_stride, (guint32*)data);
  len = strlen (url);
  append_uint32 (len, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_write (output, buf, sizeof (buf));

  broadway_output_write (output, url, len);

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
  char buf[HEADER_LEN + 15];
  gsize len;
  char *url;
  BroadwayBox *rects;
  int p, i, n_rects;
  guint8 *subdata;

  rects = rgba_find_rects (data, w, h, byte_stride, &n_rects);

  for (i = 0; i < n_rects; i++)
    {
      subdata = (guint8 *)data + rects[i].x1 * 4 + rects[i].y1 * byte_stride;

      p = write_header (output, buf, 'i');
      append_uint16 (id, buf, &p);
      append_uint16 (x + rects[i].x1, buf, &p);
      append_uint16 (y + rects[i].y1, buf, &p);

      url = to_png_rgba (rects[i].x2 - rects[i].x1,
			 rects[i].y2 - rects[i].y1,
			 byte_stride, (guint32*)subdata);
      len = strlen (url);
      append_uint32 (len, buf, &p);

      assert (p == sizeof (buf));

      broadway_output_write (output, buf, sizeof (buf));

      broadway_output_write (output, url, len);

      free (url);
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

  assert (p == sizeof (buf));

  broadway_output_write (output, buf, sizeof (buf));
}

#if 0
static void
send_image_a (BroadwayOutput *output,  int id, int x, int y,
	      int w, int h, int byte_stride, guint8 *data)
{
  char buf[HEADER_LEN + 15];
  gsize len;
  char *url;

  p = write_header (output, buf, 'i');
  append_uint16 (id, buf, &p);
  append_uint16 (x, buf, &p);
  append_uint16 (y, buf, &p);

  url = to_png_a (w, h, byte_stride, data);
  len = strlen (url);
  append_uint32 (len, buf, &p);

  assert (p == sizeof (buf));

  broadway_output_write (output, buf, sizeof (buf));

  broadway_output_write (output, url, len);

  free (url);
}
#endif
