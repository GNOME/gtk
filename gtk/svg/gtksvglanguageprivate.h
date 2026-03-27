#pragma once

#include <glib.h>
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"
#include <pango/pango.h>

G_BEGIN_DECLS

SvgValue * svg_language_new_list       (unsigned int    len,
                                        PangoLanguage **langs);
SvgValue * svg_language_new            (PangoLanguage  *language);
SvgValue * svg_language_new_default    (void);

unsigned int   svg_language_get_length (const SvgValue *value);
PangoLanguage *svg_language_get        (const SvgValue *value,
                                        unsigned int    pos);

G_END_DECLS
