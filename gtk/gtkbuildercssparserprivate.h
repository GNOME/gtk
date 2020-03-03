#ifndef __GTK_BUILDER_CSS_PARSER_PRIVATE_H__
#define __GTK_BUILDER_CSS_PARSER_PRIVATE_H__

#include "gtkwidget.h"
#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcssparserprivate.h"

typedef struct _GtkBuilderCssParser GtkBuilderCssParser;

struct _GtkBuilderCssParser
{
  GtkCssParser *css_parser;

  GObject *template_object;
  GtkBuilderScope *builder_scope;
  GHashTable *object_table; /* Name -> Object */
};


GtkBuilderCssParser *       gtk_builder_css_parser_new         (void);

void                        gtk_builder_css_parser_extend_with_template (GtkBuilderCssParser *self,
                                                                         GType                template_type,
                                                                         GObject             *object,
                                                                         GBytes              *buffer);
GObject *                   gtk_builder_css_parser_get_object           (GtkBuilderCssParser *self,
                                                                         const char          *object_name);

#endif
