#pragma once

#include "gtksvgtypesprivate.h"

G_BEGIN_DECLS

SvgValue * svg_inherit_new      (void);
SvgValue * svg_initial_new      (void);
SvgValue * svg_current_new      (void);
SvgValue * svg_auto_new         (void);

gboolean   svg_value_is_inherit (const SvgValue *value);
gboolean   svg_value_is_initial (const SvgValue *value);
gboolean   svg_value_is_current (const SvgValue *value);
gboolean   svg_value_is_auto    (const SvgValue *value);

G_END_DECLS
