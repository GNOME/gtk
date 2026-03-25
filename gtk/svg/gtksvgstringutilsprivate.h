#pragma once

#include <glib.h>
#include <graphene.h>

G_BEGIN_DECLS

void string_append_double  (GString                *string,
                            const char             *prefix,
                            double                  value);

void string_append_point   (GString                *string,
                            const char             *prefix,
                            const graphene_point_t *point);

void string_append_base64  (GString                *string,
                            GBytes                 *bytes);

G_END_DECLS
