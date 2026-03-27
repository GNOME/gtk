#pragma once

#include <glib.h>
#include "gtksvg.h"

G_BEGIN_DECLS

void gtk_svg_location_init            (GtkSvgLocation      *location,
                                       GMarkupParseContext *context);

void gtk_svg_location_init_tag_start  (GtkSvgLocation      *location,
                                       GMarkupParseContext *context);

void gtk_svg_location_init_tag_end    (GtkSvgLocation      *location,
                                       GMarkupParseContext *context);

void gtk_svg_location_init_tag_range  (GtkSvgLocation      *start,
                                       GtkSvgLocation      *end,
                                       GMarkupParseContext *context);

void gtk_svg_location_init_attr_range (GtkSvgLocation      *start,
                                       GtkSvgLocation      *end,
                                       GMarkupParseContext *context,
                                       unsigned int         attr);

G_END_DECLS
