#ifndef __GTK_SHADERTOY_H__
#define __GTK_SHADERTOY_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_SHADERTOY      (gtk_shadertoy_get_type ())
#define GTK_SHADERTOY(inst)     (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                 GTK_TYPE_SHADERTOY,                 \
                                 GtkShadertoy))
#define GTK_IS_SHADERTOY(inst)  (G_TYPE_CHECK_INSTANCE_TYPE ((inst), \
                                 GTK_TYPE_SHADERTOY))

typedef struct _GtkShadertoy GtkShadertoy;
typedef struct _GtkShadertoyClass GtkShadertoyClass;

struct _GtkShadertoy {
  GtkGLArea parent;
};

struct _GtkShadertoyClass {
  GtkGLAreaClass parent_class;
};

GType       gtk_shadertoy_get_type         (void) G_GNUC_CONST;
GtkWidget  *gtk_shadertoy_new              (void);
const char *gtk_shadertoy_get_image_shader (GtkShadertoy *shadertoy);
void        gtk_shadertoy_set_image_shader (GtkShadertoy *shadertoy,
                                            const char   *shader);

G_END_DECLS

#endif /* __GTK_SHADERTOY_H__ */
