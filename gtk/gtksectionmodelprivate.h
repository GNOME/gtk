#ifndef __GTK_SECTION_MODEL_PRIVATE_H__
#define __GTK_SECTION_MODEL_PRIVATE_H__

#include "gtksectionmodel.h"

G_BEGIN_DECLS

void                    gtk_list_model_get_section              (GListModel           *self,
                                                                 guint                 position,
                                                                 guint                *out_start,
                                                                 guint                *out_end);


G_END_DECLS

#endif /* __GTK_SECTION_MODEL_PRIVATE_H__ */
