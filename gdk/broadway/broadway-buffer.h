#ifndef __BROADWAY_BUFFER__
#define __BROADWAY_BUFFER__

#include "broadway-protocol.h"
#include <glib-object.h>

typedef struct _BroadwayBuffer BroadwayBuffer;

BroadwayBuffer *broadway_buffer_create     (int             width,
                                            int             height,
                                            guint8         *data,
                                            int             stride);
void            broadway_buffer_destroy    (BroadwayBuffer *buffer);
void            broadway_buffer_encode     (BroadwayBuffer *buffer,
                                            BroadwayBuffer *prev,
                                            GString        *dest);
int             broadway_buffer_get_width  (BroadwayBuffer *buffer);
int             broadway_buffer_get_height (BroadwayBuffer *buffer);

#endif /* __BROADWAY_BUFFER__ */
