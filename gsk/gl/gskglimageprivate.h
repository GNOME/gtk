#ifndef __GSK_GL_IMAGE_H__
#define __GSK_GL_IMAGE_H__

#include "gskgldriverprivate.h"
#include <cairo.h>

typedef struct
{
  guint texture_id;
  int width;
  int height;
} GskGLImage;

typedef struct
{
  guchar *data;
  gsize width;
  gsize height;
  gsize stride;
  gsize x;
  gsize y;
} GskImageRegion;

void gsk_gl_image_create         (GskGLImage           *self,
                                  GskGLDriver          *gl_driver,
                                  int                   width,
                                  int                   height,
                                  int                   min_filter,
                                  int                   mag_filter);
void gsk_gl_image_destroy        (GskGLImage           *self,
                                  GskGLDriver          *gl_driver);
void gsk_gl_image_write_to_png   (const GskGLImage     *self,
                                  GskGLDriver          *gl_driver,
                                  const char           *filename);
void gsk_gl_image_upload_region  (GskGLImage           *self,
                                  GskGLDriver          *gl_driver,
                                  const GskImageRegion *region);


#endif

