#pragma once

#include "gtksvg.h"
#include <glib.h>

G_BEGIN_DECLS

void gtk_svg_error_set_element   (GError               *error,
                                  const char           *element);

void gtk_svg_error_set_attribute (GError               *error,
                                  const char           *attribute);

void gtk_svg_error_set_location  (GError               *error,
                                  const GtkSvgLocation *start,
                                  const GtkSvgLocation *end);

G_END_DECLS
