#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include <zlib.h>

#include "broadway.h"

/************************************************************************
 *                Basic helpers                                         *
 ************************************************************************/

#define TRUE 1
#define FALSE 0

typedef unsigned char uchar;
typedef int boolean;

static void *
bw_malloc(size_t size)
{
  void *ptr;

  ptr = malloc(size);
  if (ptr == NULL)
    exit(1);

  return ptr;
}

static void *
bw_malloc0(size_t size)
{
  void *ptr;

  ptr = calloc(size, 1);
  if (ptr == NULL)
    exit(1);

  return ptr;
}

#define bw_new(struct_type, n_structs) (bw_malloc(sizeof(struct_type) * (n_structs)))
#define bw_new0(struct_type, n_structs) (bw_malloc0(sizeof(struct_type) * (n_structs)))

/************************************************************************
 *                Base64 implementation, from glib                      *
 ************************************************************************/

static const char base64_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t
bw_base64_encode_step (const uchar *in,
		       size_t         len,
		       boolean      break_lines,
		       char        *out,
		       int         *state,
		       int         *save)
{
  char *outptr;
  const uchar *inptr;

  if (len <= 0)
    return 0;

  inptr = in;
  outptr = out;

  if (len + ((char *) save) [0] > 2)
    {
      const uchar *inend = in+len-2;
      int c1, c2, c3;
      int already;

      already = *state;

      switch (((char *) save) [0])
        {
        case 1:
          c1 = ((unsigned char *) save) [1];
          goto skip1;
        case 2:
          c1 = ((unsigned char *) save) [1];
          c2 = ((unsigned char *) save) [2];
          goto skip2;
        }

      /*
       * yes, we jump into the loop, no i'm not going to change it,
       * it's beautiful!
       */
      while (inptr < inend)
        {
          c1 = *inptr++;
        skip1:
          c2 = *inptr++;
        skip2:
          c3 = *inptr++;
          *outptr++ = base64_alphabet [ c1 >> 2 ];
          *outptr++ = base64_alphabet [ c2 >> 4 |
                                        ((c1&0x3) << 4) ];
          *outptr++ = base64_alphabet [ ((c2 &0x0f) << 2) |
                                        (c3 >> 6) ];
          *outptr++ = base64_alphabet [ c3 & 0x3f ];
          /* this is a bit ugly ... */
          if (break_lines && (++already) >= 19)
            {
              *outptr++ = '\n';
              already = 0;
            }
        }

      ((char *)save)[0] = 0;
      len = 2 - (inptr - inend);
      *state = already;
    }

  if (len>0)
    {
      char *saveout;

      /* points to the slot for the next char to save */
      saveout = & (((char *)save)[1]) + ((char *)save)[0];

      /* len can only be 0 1 or 2 */
      switch(len)
        {
        case 2: *saveout++ = *inptr++;
        case 1: *saveout++ = *inptr++;
        }
      ((char *)save)[0] += len;
    }

  return outptr - out;
}

static size_t
bw_base64_encode_close (boolean  break_lines,
			char    *out,
			int     *state,
			int     *save)
{
  int c1, c2;
  char *outptr = out;

  c1 = ((unsigned char *) save) [1];
  c2 = ((unsigned char *) save) [2];

  switch (((char *) save) [0])
    {
    case 2:
      outptr [2] = base64_alphabet[ ( (c2 &0x0f) << 2 ) ];
      assert (outptr [2] != 0);
      goto skip;
    case 1:
      outptr[2] = '=';
    skip:
      outptr [0] = base64_alphabet [ c1 >> 2 ];
      outptr [1] = base64_alphabet [ c2 >> 4 | ( (c1&0x3) << 4 )];
      outptr [3] = '=';
      outptr += 4;
      break;
    }
  if (break_lines)
    *outptr++ = '\n';

  *save = 0;
  *state = 0;

  return outptr - out;
}

#if 0
static void
base64_uint8 (uint8_t v, char *c)
{
  c[0] = base64_alphabet[(v >> 0) & 0x3f];
  c[1] = base64_alphabet[(v >> 6) & 0x3];
}
#endif

static void
base64_uint16 (uint32_t v, char *c)
{
  c[0] = base64_alphabet[(v >> 0) & 0x3f];
  c[1] = base64_alphabet[(v >> 6) & 0x3f];
  c[2] = base64_alphabet[(v >> 12) & 0xf];
}

#if 0
static void
base64_uint24 (uint32_t v, char *c)
{
  c[0] = base64_alphabet[(v >> 0) & 0x3f];
  c[1] = base64_alphabet[(v >> 6) & 0x3f];
  c[2] = base64_alphabet[(v >> 12) & 0x3f];
  c[3] = base64_alphabet[(v >> 18) & 0x3f];
}
#endif

static void
base64_uint32 (uint32_t v, char *c)
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
to_png_rgb (int w, int h, int byte_stride, uint32_t *data)
{
  uchar header[] = {137, 80, 78, 71, 13, 10, 26, 10};
  uchar ihdr[13+12] = {0, 0, 0, 13, 'I', 'H', 'D', 'R',
			/* w: */ 0, 0, 0, 0, /* h: */ 0,0,0,0,
			/* bpp: */ 8, /* color type: */ 2,
			0, 0, 0};
  uchar idat_start[8] = { /* len: */0, 0, 0, 0,   'I', 'D', 'A', 'T' };
  uchar iend[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82};
  size_t data_size, row_size;
  char row_header[6];
  uint8_t *png, *p, *p_row, *p_idat;
  uint32_t *row;
  unsigned long adler;
  uint32_t pixel;
  size_t png_size;
  int x, y;
  char *url, *url_base64;
  int state = 0, outlen;
  int save = 0;

  *(uint32_t *)&ihdr[8] = htonl(w);
  *(uint32_t *)&ihdr[12] = htonl(h);
  *(uint32_t *)&ihdr[21] = htonl(crc(&ihdr[4], 13 + 4));

  row_size = 1 + w * 3;
  row_header[0] = 0;
  row_header[1] = row_size & 0xff;
  row_header[2] = (row_size >> 8) & 0xff;
  row_header[3] = ~row_header[1];
  row_header[4] = ~row_header[2];
  row_header[5] = 0;

  data_size = 2 + (6 + w * 3) * h + 4;

  *(uint32_t *)&idat_start[0] = htonl(data_size);

  png_size = sizeof(header) + sizeof(ihdr) + 12 + data_size + sizeof(iend);
  png = bw_malloc (png_size);

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
  *(uint32_t *)p = htonl(adler);
  p += 4;
  *(uint32_t *)p = htonl(crc(p_idat, p - p_idat));
  p += 4;

  memcpy (p, iend, sizeof(iend));
  p += sizeof(iend);

  assert(p - png == png_size);

  url = bw_malloc (strlen("data:image/png;base64,") +
		   ((png_size / 3 + 1) * 4 + 4) + 1);
  strcpy (url, "data:image/png;base64,");

  url_base64 = url + strlen("data:image/png;base64,");
  outlen = bw_base64_encode_step (png, png_size, FALSE, url_base64, &state, &save);
  outlen += bw_base64_encode_close (FALSE, url_base64 + outlen, &state, &save);
  url_base64[outlen] = 0;

  free (png);

  return url;
}

static char *
to_png_rgba (int w, int h, int byte_stride, uint32_t *data)
{
  uchar header[] = {137, 80, 78, 71, 13, 10, 26, 10};
  uchar ihdr[13+12] = {0, 0, 0, 13, 'I', 'H', 'D', 'R',
			/* w: */ 0, 0, 0, 0, /* h: */ 0,0,0,0,
			/* bpp: */ 8, /* color type: */ 6,
			0, 0, 0};
  uchar idat_start[8] = { /* len: */0, 0, 0, 0,   'I', 'D', 'A', 'T' };
  uchar iend[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82};
  size_t data_size, row_size;
  char row_header[6];
  uint8_t *png, *p, *p_row, *p_idat;
  uint32_t *row;
  unsigned long adler;
  uint32_t pixel;
  size_t png_size;
  int x, y;
  char *url, *url_base64;
  int state = 0, outlen;
  int save = 0;

  *(uint32_t *)&ihdr[8] = htonl(w);
  *(uint32_t *)&ihdr[12] = htonl(h);
  *(uint32_t *)&ihdr[21] = htonl(crc(&ihdr[4], 13 + 4));

  row_size = 1 + w * 4;
  row_header[0] = 0;
  row_header[1] = row_size & 0xff;
  row_header[2] = (row_size >> 8) & 0xff;
  row_header[3] = ~row_header[1];
  row_header[4] = ~row_header[2];
  row_header[5] = 0;

  data_size = 2 + (6 + w * 4) * h + 4;

  *(uint32_t *)&idat_start[0] = htonl(data_size);

  png_size = sizeof(header) + sizeof(ihdr) + 12 + data_size + sizeof(iend);
  png = bw_malloc (png_size);

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
  *(uint32_t *)p = htonl(adler);
  p += 4;
  *(uint32_t *)p = htonl(crc(p_idat, p - p_idat));
  p += 4;

  memcpy (p, iend, sizeof(iend));
  p += sizeof(iend);

  assert(p - png == png_size);

  url = bw_malloc (strlen("data:image/png;base64,") +
		   ((png_size / 3 + 1) * 4 + 4) + 1);
  strcpy (url, "data:image/png;base64,");

  url_base64 = url + strlen("data:image/png;base64,");
  outlen = bw_base64_encode_step (png, png_size, FALSE, url_base64, &state, &save);
  outlen += bw_base64_encode_close (FALSE, url_base64 + outlen, &state, &save);
  url_base64[outlen] = 0;

  free (png);

  return url;
}

#if 0
static char *
to_png_a (int w, int h, int byte_stride, uint8_t *data)
{
  uchar header[] = {137, 80, 78, 71, 13, 10, 26, 10};
  uchar ihdr[13+12] = {0, 0, 0, 13, 'I', 'H', 'D', 'R',
			/* w: */ 0, 0, 0, 0, /* h: */ 0,0,0,0,
			/* bpp: */ 8, /* color type: */ 4,
			0, 0, 0};
  uchar idat_start[8] = { /* len: */0, 0, 0, 0,   'I', 'D', 'A', 'T' };
  uchar iend[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82};
  size_t data_size, row_size;
  char row_header[6];
  uint8_t *png, *p, *p_row, *p_idat;
  uint8_t *row;
  unsigned long adler;
  uint32_t pixel;
  size_t png_size;
  int x, y;
  char *url, *url_base64;
  int state = 0, outlen;
  int save = 0;

  *(uint32_t *)&ihdr[8] = htonl(w);
  *(uint32_t *)&ihdr[12] = htonl(h);
  *(uint32_t *)&ihdr[21] = htonl(crc(&ihdr[4], 13 + 4));

  row_size = 1 + w * 2;
  row_header[0] = 0;
  row_header[1] = row_size & 0xff;
  row_header[2] = (row_size >> 8) & 0xff;
  row_header[3] = ~row_header[1];
  row_header[4] = ~row_header[2];
  row_header[5] = 0;

  data_size = 2 + (6 + w * 2) * h + 4;

  *(uint32_t *)&idat_start[0] = htonl(data_size);

  png_size = sizeof(header) + sizeof(ihdr) + 12 + data_size + sizeof(iend);
  png = bw_malloc (png_size);

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
  *(uint32_t *)p = htonl(adler);
  p += 4;
  *(uint32_t *)p = htonl(crc(p_idat, p - p_idat));
  p += 4;

  memcpy (p, iend, sizeof(iend));
  p += sizeof(iend);

  assert(p - png == png_size);

  url = bw_malloc (strlen("data:image/png;base64,") +
		  ((png_size / 3 + 1) * 4 + 4) + 1);
  strcpy (url, "data:image/png;base64,");

  url_base64 = url + strlen("data:image/png;base64,");
  outlen = bw_base64_encode_step (png, png_size, FALSE, url_base64, &state, &save);
  outlen += bw_base64_encode_close (FALSE, url_base64 + outlen, &state, &save);
  url_base64[outlen] = 0;

  free (png);

  return url;
}
#endif

/************************************************************************
 *                Basic I/O primitives                                  *
 ************************************************************************/

struct BroadwayClient {
  int fd;
  gzFile *zfd;
} ;

static void
broadway_client_write_raw (BroadwayClient *client,
			   const void *buf, size_t count)
{
  ssize_t res;
  int errsave;
  const char *ptr = (const char *)buf;

  while (count > 0)
    {
      res = write(client->fd, ptr, count);
      if (res == -1)
	{
	  errsave = errno;
	  if (errsave == EINTR)
	    continue;
	  fprintf(stderr, "Error on write_raw to client %d\n", errsave);
	  exit(1);
	}
      if (res == 0)
	{
	  fprintf(stderr, "Short write_raw to client\n");
	  exit(1);
	}
      count -= res;
      ptr += res;
    }
}

static void
broadway_client_write (BroadwayClient *client,
		       const void *buf, size_t count)
{
  ssize_t res;
  const char *ptr = (const char *)buf;

  while (count > 0)
    {
      res = gzwrite(client->zfd, ptr, count);
      if (res == -1)
	{
	  fprintf(stderr, "Error on write to client\n");
	  exit(1);
	}
      if (res == 0)
	{
	  fprintf(stderr, "Short write to client\n");
	  exit(1);
	}
      count -= res;
      ptr += res;
    }
}

static void
broadway_client_write_header (BroadwayClient *client)
{
  char *header;

  header =
    "HTTP/1.1 200 OK\r\n"
    "Content-type: multipart/x-mixed-replace;boundary=x\r\n"
    "Content-Encoding: gzip\r\n"
    "\r\n";
  broadway_client_write_raw (client,
			     header, strlen (header));
}

static void
send_boundary (BroadwayClient *client)
{
  char *boundary =
    "--x\r\n"
    "\r\n";

  broadway_client_write (client, boundary, strlen (boundary));
}

BroadwayClient *
broadway_client_new(int fd)
{
  BroadwayClient *client;

  client = bw_new0 (BroadwayClient, 1);

  client->fd = fd;

  broadway_client_write_header (client);

  client->zfd = gzdopen(fd, "wb");

  /* Need an initial multipart boundary */
  send_boundary (client);

  return client;
}

void
broadway_client_flush (BroadwayClient *client)
{
  send_boundary (client);
  gzflush (client->zfd, Z_SYNC_FLUSH);
}


/************************************************************************
 *                     Core rendering operations                        *
 ************************************************************************/

void
broadway_client_copy_rectangles (BroadwayClient *client,  int id,
				 BroadwayRect *rects, int n_rects,
				 int dx, int dy)
{
  char *buf;
  int len, i, p;

  len = 1 + 3 + 3 + 3*4*n_rects + 3 + 3;

  buf = bw_malloc (len);
  p = 0;
  buf[p++] = 'b';
  base64_uint16(id, &buf[p]); p +=3;
  base64_uint16(n_rects, &buf[p]); p +=3;
  for (i = 0; i < n_rects; i++)
    {
      base64_uint16(rects[i].x, &buf[p]); p +=3;
      base64_uint16(rects[i].y, &buf[p]); p +=3;
      base64_uint16(rects[i].width, &buf[p]); p +=3;
      base64_uint16(rects[i].height, &buf[p]); p +=3;
    }
  base64_uint16(dx, &buf[p]); p +=3;
  base64_uint16(dy, &buf[p]); p +=3;

  broadway_client_write (client, buf, len);
  free (buf);
}

void
broadway_client_new_surface(BroadwayClient *client,  int id, int x, int y, int w, int h)
{
  char buf[16];

  buf[0] = 's';
  base64_uint16(id, &buf[1]);
  base64_uint16(x, &buf[4]);
  base64_uint16(y, &buf[7]);
  base64_uint16(w, &buf[10]);
  base64_uint16(h, &buf[13]);

  broadway_client_write (client, buf, 16);
}

void
broadway_client_show_surface(BroadwayClient *client,  int id)
{
  char buf[4];

  buf[0] = 'S';
  base64_uint16(id, &buf[1]);

  broadway_client_write (client, buf, 4);
}

void
broadway_client_hide_surface(BroadwayClient *client,  int id)
{
  char buf[4];

  buf[0] = 'H';
  base64_uint16(id, &buf[1]);

  broadway_client_write (client, buf, 4);
}

void
broadway_client_destroy_surface(BroadwayClient *client,  int id)
{
  char buf[4];

  buf[0] = 'd';
  base64_uint16(id, &buf[1]);

  broadway_client_write (client, buf, 4);
}

void
broadway_client_move_surface(BroadwayClient *client,  int id, int x, int y)
{
  char buf[10];

  buf[0] = 'm';
  base64_uint16(id, &buf[1]);
  base64_uint16(x, &buf[4]);
  base64_uint16(y, &buf[7]);

  broadway_client_write (client, buf, 10);
}

void
broadway_client_put_rgb (BroadwayClient *client,  int id, int x, int y,
			 int w, int h, int byte_stride, void *data)
{
  char buf[16];
  size_t len;
  char *url;

  buf[0] = 'i';
  base64_uint16(id, &buf[1]);
  base64_uint16(x, &buf[4]);
  base64_uint16(y, &buf[7]);

  url = to_png_rgb (w, h, byte_stride, (uint32_t*)data);
  len = strlen (url);
  base64_uint32(len, &buf[10]);

  broadway_client_write (client, buf, 16);

  broadway_client_write (client, url, len);

  free (url);
}

static void
rgba_autocrop (unsigned char *data,
	       int byte_stride,
	       int *x_arg, int *y_arg,
	       int *w_arg, int *h_arg)
{
  uint32_t *line;
  int w, h;
  int x, y, xx, yy;
  boolean non_zero;

  x = *x_arg;
  y = *y_arg;
  w = *w_arg;
  h = *h_arg;

  while (h > 0)
    {
      line = (uint32_t *)(data + y * byte_stride + x * 4);

      non_zero = FALSE;
      for (xx = 0; xx < w; xx++)
	{
	  if (*line != 0) {
	    non_zero = TRUE;
	    break;
	  }
	  line++;
	}

      if (non_zero)
	break;

      y++;
      h--;
    }

  while (h > 0)
    {
      line = (uint32_t *)(data + (y + h - 1) * byte_stride + x * 4);

      non_zero = FALSE;
      for (xx = 0; xx < w; xx++)
	{
	  if (*line != 0) {
	    non_zero = TRUE;
	    break;
	  }
	  line++;
	}

      if (non_zero)
	break;
      h--;
    }

  while (w > 0)
    {
      line = (uint32_t *)(data + y * byte_stride + x * 4);

      non_zero = FALSE;
      for (yy = 0; yy < h; yy++)
	{
	  if (*line != 0) {
	    non_zero = TRUE;
	    break;
	  }
	  line += byte_stride / 4;
	}

      if (non_zero)
	break;

      x++;
      w--;
    }

  while (w > 0)
    {
      line = (uint32_t *)(data + y * byte_stride + (x + w - 1) * 4);

      non_zero = FALSE;
      for (yy = 0; yy < h; yy++)
	{
	  if (*line != 0) {
	    non_zero = TRUE;
	    break;
	  }
	  line += byte_stride / 4;
	}

      if (non_zero)
	break;
      w--;
    }

    *x_arg = x;
    *y_arg = y;
    *w_arg = w;
    *h_arg = h;
}

void
broadway_client_put_rgba (BroadwayClient *client,  int id, int x, int y,
			  int w, int h, int byte_stride, void *data)
{
  char buf[16];
  size_t len;
  char *url;
  int crop_x, crop_y;

  crop_x = 0;
  crop_y = 0;

  printf ("pre crop: %dx%d\n", w, h);
  rgba_autocrop (data,
		 byte_stride,
		 &crop_x, &crop_y, &w, &h);
  printf ("post crop: %dx%d %d,%d\n", w, h, crop_x, crop_y);

  if (w == 0 || h == 0)
    return;

  data = (uint8_t *)data + crop_x * 4 + crop_y * byte_stride;

  buf[0] = 'i';
  base64_uint16(id, &buf[1]);
  base64_uint16(x + crop_x, &buf[4]);
  base64_uint16(y + crop_y, &buf[7]);

  url = to_png_rgba (w, h, byte_stride, (uint32_t*)data);
  len = strlen (url);
  base64_uint32(len, &buf[10]);

  broadway_client_write (client, buf, 16);

  broadway_client_write (client, url, len);

  free (url);
}

#if 0
static void
send_image_a (BroadwayClient *client,  int id, int x, int y,
	      int w, int h, int byte_stride, uint8_t *data)
{
  char buf[16];
  size_t len;
  char *url;

  buf[0] = 'i';
  base64_uint16(id, &buf[1]);
  base64_uint16(x, &buf[4]);
  base64_uint16(y, &buf[7]);

  url = to_png_a (w, h, byte_stride, data);
  len = strlen (url);
  base64_uint32(len, &buf[10]);

  broadway_client_write (client, buf, 16);

  broadway_client_write (client, url, len);

  free (url);
}
#endif
