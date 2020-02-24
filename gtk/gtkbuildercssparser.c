

#include "gtkbuildercssparserprivate.h"
#include "gtkbuildable.h"

GtkBuilderCssParser *
gtk_builder_css_parser_new (void)
{
  return g_new0 (GtkBuilderCssParser, 1);
}

static GObject *
parse_object (GtkBuilderCssParser *self,
              GObject             *template_object)
{
  GtkCssParser *parser = self->css_parser;
  GObject *object = NULL;
  GObjectClass *object_class;
  const char *type_name;
  GType type;

  gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);

  /* Get us the ident, which will determine what we parse and how we parse it */
  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_parser_error_syntax (parser, "Expected type name");
      goto fail;
    }

  type_name = gtk_css_parser_get_token (parser)->string.string;
  type = g_type_from_name (type_name);

  if (type == G_TYPE_INVALID)
    {
      gtk_css_parser_error_syntax (parser, "Unknown type name '%s'", type_name);
      goto fail;
    }

  if (template_object)
    {
      object = g_object_ref (template_object);
      // TODO Check that typeof(object) == given type
    }
  else
    {
      object = g_object_new (type, NULL);
    }

  gtk_css_parser_consume_token (parser);

  object_class = G_OBJECT_GET_CLASS (object);

  gtk_css_parser_end_block_prelude (parser);
  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);

      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          gtk_css_parser_error_syntax (parser, "Expected property name");
        }
      else
        {
          const char *prop_name = gtk_css_parser_get_token (parser)->string.string;
          GParamSpec *pspec;

          pspec = g_object_class_find_property (object_class, prop_name);
          if (!pspec)
            {
              char *prop_name_dup = g_strdup (prop_name);
              gboolean parsed = FALSE;

              if (GTK_IS_CSS_BUILDABLE (object))
                {
                  GtkCssBuildableIface *iface = GTK_CSS_BUILDABLE_GET_IFACE (object);
                  /*g_message ("%s Is a  css buildalbe!!", G_OBJECT_TYPE_NAME (object));*/

                  /* TODO: WALK UP HIERARCHY AND SHIT */
                  parsed = iface->parse_declaration (GTK_CSS_BUILDABLE (object),
                                                     parser,
                                                     prop_name,
                                                     strlen (prop_name));
                }

              if (!parsed)
                {
                  gtk_css_parser_error_syntax (parser, "Invalid property '%s' for class '%s'",
                                               prop_name, G_OBJECT_TYPE_NAME (object));
                  goto fail;
                }
            }
          else
            {
              // TODO: Validate pspec for correct flags, e.g. writable etc.
              gtk_css_parser_consume_token (parser);
              if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
                {
                  gtk_css_parser_error_syntax (parser, "Expected ':' after property name");
                  goto fail;
                }

              g_message ("parsing property %s", pspec->name);
              if (pspec->value_type == G_TYPE_BOOLEAN)
                {
                  const GtkCssToken *token = gtk_css_parser_get_token (parser);

                  if (token->type != GTK_CSS_TOKEN_IDENT)
                    {
                      gtk_css_parser_error_syntax (parser, "Invalid boolean value: '%s'",
                                                   token->string.string);
                      goto fail;
                    }
                  else
                    {
                      if (strcmp (token->string.string, "true") == 0)
                        g_object_set (object, pspec->name, TRUE, NULL);
                      else if (strcmp (token->string.string, "false") == 0)
                        g_object_set (object, pspec->name, FALSE, NULL);
                      else
                        {
                          gtk_css_parser_error_syntax (parser, "Invalid boolean value: '%s'",
                                                       token->string.string);
                          goto fail;
                        }

                      gtk_css_parser_consume_token (parser);
                    }
                }
              else
                g_warning ("TODO: Add parser for properties of type %s", g_type_name (pspec->value_type));
            }
        }

      gtk_css_parser_end_block (parser); /* Property block */
    }

  gtk_css_parser_end_block (parser);
  return object;

fail:
  gtk_css_parser_end_block (parser);

  if (object)
    g_object_unref (object);

  return NULL;
}

static void
parse_objects (GtkBuilderCssParser *self,
               GObject             *template_object)
{
  const GtkCssToken *token;
  GtkCssParser *parser = self->css_parser;

  for (token = gtk_css_parser_get_token (parser);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_parser_get_token (parser))
    {
      parse_object (self, template_object);
    }
}

static void
parser_error_func (GtkCssParser         *parser,
                   const GtkCssLocation *start,
                   const GtkCssLocation *end,
                   const GError         *error,
                   gpointer              user_data)
{
  GtkCssSection *section = gtk_css_section_new (gtk_css_parser_get_file (parser), start, end);

  g_warning ("%s: %s", error->message, gtk_css_section_to_string (section));

  gtk_css_section_unref (section);
}


void
gtk_builder_css_parser_extend_with_template (GtkBuilderCssParser *self,
                                             GType                template_type,
                                             GObject             *template_object,
                                             GBytes              *bytes)
{
  self->css_parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, parser_error_func,
                                                   NULL, NULL);

  parse_objects (self, template_object);
}
