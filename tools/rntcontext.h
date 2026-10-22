#pragma once

#include <gtk/gtk.h>

typedef struct _RntContext RntContext;

RntContext *            rnt_context_new                         (void);
void                    rnt_context_free                        (RntContext             *self);

GOptionGroup *          rnt_context_create_option_group         (RntContext             *self);

void                    rnt_context_push_node                   (RntContext             *self,
                                                                 GskRenderNode          *node);
GskRenderNode *         rnt_context_pop_node                    (RntContext             *self);
void                    rnt_context_clear_nodes                 (RntContext             *self);
GskRenderNode *         rnt_context_get_node                    (RntContext             *self,
                                                                 gsize                   id);
size_t                  rnt_context_get_n_nodes                 (RntContext             *self);

GskRenderNode *         rnt_context_get_null_node               (RntContext             *ctx);

gboolean                rnt_context_get_failed                  (RntContext             *ctx);
void                    rnt_context_set_failed                  (RntContext             *ctx,
                                                                 gboolean                failed);
gboolean                rnt_context_get_verbose                 (RntContext             *self);
void                    rnt_context_set_verbose                 (RntContext             *ctx,
                                                                 gboolean                verbose);

void                    rnt_context_print                       (RntContext             *self,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);
void                    rnt_context_verbose                     (RntContext             *self,
                                                                 const char             *format,
                                                                 ...) G_GNUC_PRINTF(2, 3);

