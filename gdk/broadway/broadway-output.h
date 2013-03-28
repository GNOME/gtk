#ifndef __BROADWAY_H__
#define __BROADWAY_H__

#include <glib.h>
#include <gio/gio.h>
#include "broadway-protocol.h"

typedef struct BroadwayOutput BroadwayOutput;

typedef enum {
  BROADWAY_WS_CONTINUATION = 0,
  BROADWAY_WS_TEXT = 1,
  BROADWAY_WS_BINARY = 2,
  BROADWAY_WS_CNX_CLOSE = 8,
  BROADWAY_WS_CNX_PING = 9,
  BROADWAY_WS_CNX_PONG = 0xa
} BroadwayWSOpCode;

BroadwayOutput *broadway_output_new             (GOutputStream  *out,
						 guint32         serial,
						 gboolean        proto_v7_plus,
						 gboolean        binary);
void            broadway_output_free            (BroadwayOutput *output);
int             broadway_output_flush           (BroadwayOutput *output);
int             broadway_output_has_error       (BroadwayOutput *output);
void            broadway_output_set_next_serial (BroadwayOutput *output,
						 guint32         serial);
guint32         broadway_output_get_next_serial (BroadwayOutput *output);
void            broadway_output_new_surface     (BroadwayOutput *output,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h,
						 gboolean        is_temp);
void            broadway_output_request_auth    (BroadwayOutput *output);
void            broadway_output_auth_ok         (BroadwayOutput *output);
void            broadway_output_disconnected    (BroadwayOutput *output);
void            broadway_output_show_surface    (BroadwayOutput *output,
						 int             id);
void            broadway_output_hide_surface    (BroadwayOutput *output,
						 int             id);
void            broadway_output_destroy_surface (BroadwayOutput *output,
						 int             id);
void            broadway_output_move_resize_surface (BroadwayOutput *output,
						     int             id,
						     gboolean        has_pos,
						     int             x,
						     int             y,
						     gboolean        has_size,
						     int             w,
						     int             h);
void            broadway_output_set_transient_for (BroadwayOutput *output,
						   int             id,
						   int             parent_id);
void            broadway_output_put_rgb         (BroadwayOutput *output,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h,
						 int             byte_stride,
						 void           *data);
void            broadway_output_put_rgba        (BroadwayOutput *output,
						 int             id,
						 int             x,
						 int             y,
						 int             w,
						 int             h,
						 int             byte_stride,
						 void           *data);
void            broadway_output_surface_flush   (BroadwayOutput *output,
						 int             id);
void            broadway_output_copy_rectangles (BroadwayOutput *output,
						 int             id,
						 BroadwayRect   *rects,
						 int             n_rects,
						 int             dx,
						 int             dy);
void            broadway_output_grab_pointer    (BroadwayOutput *output,
						 int id,
						 gboolean owner_event);
guint32         broadway_output_ungrab_pointer  (BroadwayOutput *output);
void            broadway_output_pong            (BroadwayOutput *output);

#endif /* __BROADWAY_H__ */
