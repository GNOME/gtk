#ifndef __GSK_SHADER_BUILDER_PRIVATE_H__
#define __GSK_SHADER_BUILDER_PRIVATE_H__

#include <gdk/gdk.h>
#include <graphene.h>

G_BEGIN_DECLS

#define GSK_TYPE_SHADER_BUILDER (gsk_shader_builder_get_type ())

G_DECLARE_FINAL_TYPE (GskShaderBuilder, gsk_shader_builder, GSK, SHADER_BUILDER, GObject)

GskShaderBuilder *      gsk_shader_builder_new                          (void);

void                    gsk_shader_builder_set_version                  (GskShaderBuilder *builder,
                                                                         int               version);
void                    gsk_shader_builder_set_resource_base_path       (GskShaderBuilder *builder,
                                                                         const char       *base_path);

GQuark                  gsk_shader_builder_add_uniform                  (GskShaderBuilder *builder,
                                                                         const char       *uniform_name);
GQuark                  gsk_shader_builder_add_attribute                (GskShaderBuilder *builder,
                                                                         const char       *attribute_name);
void                    gsk_shader_builder_add_define                   (GskShaderBuilder *builder,
                                                                         const char       *define_name,
                                                                         const char       *define_value);

int                     gsk_shader_builder_compile_shader               (GskShaderBuilder *builder,
                                                                         int               shader_type,
                                                                         const char       *shader_preamble,
                                                                         const char       *shader_source,
                                                                         GError          **error);
int                     gsk_shader_builder_create_program               (GskShaderBuilder *builder,
                                                                         int               vertex_id,
                                                                         int               fragment_id,
                                                                         GError          **error);

int                     gsk_shader_builder_get_uniform_location         (GskShaderBuilder *builder,
                                                                         GQuark            uniform_quark);
int                     gsk_shader_builder_get_attribute_location       (GskShaderBuilder *builder,
                                                                         GQuark            attribute_quark);

G_END_DECLS

#endif /* __GSK_SHADER_BUILDER_PRIVATE_H__ */
