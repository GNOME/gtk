

#include "gtkbuildercssparserprivate.h"
#include "gtkbuilderscopeprivate.h"
#include "gtkbuildable.h"

static GObject *          parse_object     (GtkBuilderCssParser *self,
                                            GObject             *template_object);

typedef struct {
  guint signal_id;
  GQuark signal_detail;
  char *signal_name; // TODO: Remove?
  char *handler_name;
  guint after: 1;
  guint swapped: 1;
} SignalConnectionData;

GtkBuilderCssParser *
gtk_builder_css_parser_new (void)
{
  GtkBuilderCssParser *self = g_new0 (GtkBuilderCssParser, 1);

  self->object_table  = g_hash_table_new (g_str_hash,
                                          g_str_equal);

  return self;
}

static guint
translated_string_parse_func (GtkCssParser *parser,
                              guint         arg,
                              gpointer      data)
{
  char **str = data;
  g_assert (arg == 0);

  *str = g_strdup (gtk_css_parser_get_token (parser)->string.string);

  return 0;
}

static SignalConnectionData *
parse_signals (GtkBuilderCssParser *self,
               GType                object_type,
               guint               *out_n_connections)
{
  GtkCssParser *parser = self->css_parser;
  GArray *connection_data;

  if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
    {
      gtk_css_parser_error_syntax (parser, "Expected colon after signal keyword");
      *out_n_connections = 0;
      return NULL;
    }

  connection_data = g_array_new (FALSE, TRUE, sizeof (SignalConnectionData));
  gtk_css_parser_end_block_prelude (parser);
  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      SignalConnectionData connection;

      if (token->type == GTK_CSS_TOKEN_IDENT ||
          token->type == GTK_CSS_TOKEN_STRING)
        {
          connection.signal_name = g_strdup (token->string.string);
          if (!g_signal_parse_name (token->string.string, object_type,
                                    &connection.signal_id,
                                    &connection.signal_detail,
                                    FALSE))
            {
              // TODO: Check for proper signal detail
              gtk_css_parser_error_syntax (parser, "Signal '%s' is invalid for type '%s'",
                                           token->string.string,
                                           g_type_name (object_type));
              goto next_signal;
            }
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "Expected signal name");
          goto next_signal;
        }

      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);

      gtk_css_parser_consume_token (parser); /* Skip signal name */
      if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (parser, "Expected colon after signal name, but got %s",
                                       gtk_css_token_to_string (gtk_css_parser_get_token (parser)));
          goto next_signal;
        }

      /* Now parse the actual signal */
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
        {
          token = gtk_css_parser_get_token (parser);
        }
      else
        {
          gtk_css_parser_end_block_prelude (parser);

          // TODO Implement other signal stuff, like swapped.
          connection.after = FALSE;
          connection.swapped = FALSE;

          if (!gtk_css_parser_has_ident (parser, "handler"))
            {
              gtk_css_parser_error_syntax (parser, "Expected 'handler'");
              goto next_signal;
            }
          gtk_css_parser_consume_token (parser);
          if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
            {
              gtk_css_parser_error_syntax (parser, "Expected colon");
              goto next_signal;
            }
          token = gtk_css_parser_get_token (parser);
        }
      connection.handler_name = g_strdup (token->string.string);

      g_array_append_val (connection_data, connection);

next_signal:
      gtk_css_parser_end_block (parser); /* Signal block */
    }

  *out_n_connections = connection_data->len;
  return (SignalConnectionData *)g_array_free (connection_data, FALSE);
}

static gboolean
parse_property (GtkBuilderCssParser *self,
                GParamSpec          *pspec,
                GValue              *out_value)
{
  GtkCssParser *parser = self->css_parser;

  if (pspec->value_type == G_TYPE_BOOLEAN)
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);

      if (token->type != GTK_CSS_TOKEN_IDENT)
        {
          gtk_css_parser_error_syntax (parser, "Invalid boolean value: '%s'",
                                       token->string.string);
          return FALSE;
        }

      if (strcmp (token->string.string, "true") == 0)
        {
          g_value_init (out_value, G_TYPE_BOOLEAN);
          g_value_set_boolean (out_value, TRUE);
        }
      else if (strcmp (token->string.string, "false") == 0)
        {
          g_value_init (out_value, G_TYPE_BOOLEAN);
          g_value_set_boolean (out_value, FALSE);
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "Invalid boolean value: '%s'",
                                       token->string.string);
          return FALSE;
        }

      gtk_css_parser_consume_token (parser);
    }
  else if (pspec->value_type == G_TYPE_STRING)
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);

      if (gtk_css_parser_has_function (parser, "_"))
        {
          char *string;

          gtk_css_parser_consume_function (parser, 1, 1, translated_string_parse_func, &string);
          // TODO: Imlpement translation stuff

          g_value_init (out_value, G_TYPE_STRING);
          g_value_take_string (out_value, string);
        }
      else if (token->type == GTK_CSS_TOKEN_STRING)
        {
          g_value_init (out_value, G_TYPE_STRING);
          g_value_set_string (out_value, token->string.string);
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "Expected a string value");
          return FALSE;
        }
    }
  else if (pspec->value_type == G_TYPE_FLOAT ||
           pspec->value_type == G_TYPE_DOUBLE ||
           pspec->value_type == G_TYPE_INT)
    {
      double d;

      if (!gtk_css_parser_consume_number (parser, &d))
        {
          gtk_css_parser_error_syntax (parser, "Expected a number");
          return FALSE;
        }
      // TODO: We should probably handle int differently, so we show a warning when
      //   finding a float/double?

      g_value_init (out_value, pspec->value_type);
      if (pspec->value_type == G_TYPE_FLOAT)
        g_value_set_float (out_value, d);
      else if (pspec->value_type == G_TYPE_DOUBLE)
        g_value_set_double (out_value, d);
      else if (pspec->value_type == G_TYPE_INT)
        g_value_set_int (out_value, d);
      else
        g_assert_not_reached ();
    }
  else if (G_TYPE_IS_ENUM (pspec->value_type))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      const GEnumValue *enum_value;
      GEnumClass *enum_class;

      if (token->type != GTK_CSS_TOKEN_IDENT)
        {
          gtk_css_parser_error_syntax (parser, "Expected an enum value name");
          return FALSE;
        }

      enum_class = g_type_class_ref (pspec->value_type);
      enum_value = g_enum_get_value_by_nick (enum_class, token->string.string);

      g_value_init (out_value, pspec->value_type);
      g_value_set_enum (out_value, enum_value->value);
      g_type_class_unref (enum_class);
    }
  else if (pspec->value_type == G_TYPE_STRV)
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      GPtrArray *strings = g_ptr_array_sized_new (16);

      while (token->type != GTK_CSS_TOKEN_EOF)
        {
          if (token->type != GTK_CSS_TOKEN_STRING)
            {
              gtk_css_parser_error_syntax (parser, "Expected a string");
              return FALSE;
            }

          g_ptr_array_add (strings, gtk_css_parser_consume_string (parser));

          token = gtk_css_parser_get_token (parser);
          if (token->type == GTK_CSS_TOKEN_EOF)
            break;

          if (token->type != GTK_CSS_TOKEN_COMMA)
            {
              gtk_css_parser_error_syntax (parser, "Expected comma after string when parsing GStrv typed property");
              return FALSE;
            }

          gtk_css_parser_consume_token (parser);
          token = gtk_css_parser_get_token (parser);
        }

      g_ptr_array_add (strings, NULL);

      g_value_init (out_value, pspec->value_type);
      g_value_set_boxed (out_value, g_ptr_array_free (strings, FALSE));
      return TRUE;
    }
  else if (g_type_is_a (pspec->value_type, G_TYPE_OBJECT))
    {
      GObject *obj = parse_object (self, NULL);

      g_value_init (out_value, pspec->value_type);
      g_value_set_object (out_value, obj);
      return TRUE;
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Unable to parse properties of type %s",
                                   g_type_name (pspec->value_type));
      return FALSE;
    }

  return TRUE;
}

static GObject *
parse_object (GtkBuilderCssParser *self,
              GObject             *template_object)
{
#define MAX_PROPERTIES 64
  GtkCssParser *parser = self->css_parser;
  GObject *object = NULL;
  GObjectClass *object_class;
  const char *type_name;
  GType type;
  char *buildable_id = NULL;
  char *property_names[MAX_PROPERTIES];
  GValue property_values[MAX_PROPERTIES];
  int n_properties = 0;
  char *buildable_prop_names[MAX_PROPERTIES];
  GValue buildable_prop_values[MAX_PROPERTIES];
  int n_buildable_properties = 0;
  guint n_signal_connections = 0;
  SignalConnectionData *signal_connections;
  int i;

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
  if (template_object &&
      strcmp (type_name, G_OBJECT_TYPE_NAME (template_object)) != 0)
    {
      gtk_css_parser_error_syntax (parser,
                                   "Expected object type '%s' but found '%s'",
                                   G_OBJECT_TYPE_NAME (template_object),
                                   type_name);
      g_object_unref (object);
      goto fail;
    }
  gtk_css_parser_consume_token (parser);

  object_class = g_type_class_ref (type);
  g_assert (object_class);

  memset (property_values, 0, MAX_PROPERTIES * sizeof (GValue));
  memset (buildable_prop_values, 0, MAX_PROPERTIES * sizeof (GValue));
  gtk_css_parser_end_block_prelude (parser);
  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      const char *token_string;
      char *prop_name = NULL;
      GParamSpec *pspec;

      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          gtk_css_parser_error_syntax (parser, "Expected property name");
          goto next_prop;
        }

      token_string = gtk_css_parser_get_token (parser)->string.string;
      /* Special cases */
      if (strcmp (token_string, "signals") == 0)
        {
          gtk_css_parser_consume_token (parser);
          gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);
          signal_connections = parse_signals (self, type, &n_signal_connections);
          goto next_prop;
        }

      prop_name = g_strdup (token_string);
      pspec = g_object_class_find_property (object_class, prop_name);
      gtk_css_parser_consume_token (parser);

      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);
      if (pspec)
        {
          // TODO: Validate pspec for correct flags, e.g. writable etc.
          if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
            {
              gtk_css_parser_error_syntax (parser, "Expected ':' after property name");
              goto next_prop;
            }

          if (!parse_property (self, pspec, &property_values[n_properties]))
            goto next_prop;

          property_names[n_properties] = g_steal_pointer (&prop_name);
          n_properties++;
        }
      else
        {
          if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
            {
              gtk_css_parser_error_syntax (parser, "Expected colon after property name");
              goto next_prop;
            }

          /* Buildable ID special case */
          if (strcmp (prop_name, "id") == 0)
            {
              buildable_id = gtk_css_parser_consume_string (parser);
              if (!buildable_id)
                gtk_css_parser_error_syntax (parser, "Expected string ID");

              goto next_prop;
            }

          if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
            gtk_css_parser_end_block_prelude (parser);

          // TODO: Parse other things than objects
          while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
            {
              GObject *o = parse_object (self, NULL);

              if (!o)
                goto next_prop;

              g_value_init (&buildable_prop_values[n_buildable_properties], G_TYPE_OBJECT);
              g_value_set_object (&buildable_prop_values[n_buildable_properties], o);
              buildable_prop_names[n_buildable_properties] = g_strdup (prop_name);
              n_buildable_properties++;
            }
        }

next_prop:
      gtk_css_parser_end_block (parser); /* Property block */
      g_free (prop_name);
    }

  if (template_object)
    {
      object = g_object_ref (template_object);
      g_object_setv (object, n_properties,
                     (const char **)property_names,
                     property_values);
    }
  else /* Create a new object */
    {
      object = g_object_new_with_properties (type, n_properties,
                                             (const char **)property_names,
                                             property_values);
    }

  /* Now set the buildable properties */
  for (i = 0; i < n_buildable_properties; i++)
    {
      // TODO: Fix this to allow non-GObject values
      if (GTK_IS_CSS_BUILDABLE (object))
        {
          GObject *o = g_value_get_object (&buildable_prop_values[i]);
          GtkCssBuildableIface *iface = GTK_CSS_BUILDABLE_GET_IFACE (object);
          /* TODO: WALK UP HIERARCHY AND SHIT */
          iface->set_property (GTK_CSS_BUILDABLE (object),
                               buildable_prop_names[i], strlen (buildable_prop_names[i]),
                               G_OBJECT_TYPE (o), o);
        }
    }

  if (buildable_id)
    {
      GtkCssBuildableIface *iface = GTK_CSS_BUILDABLE_GET_IFACE (object);
      if (iface)
        iface->set_name (GTK_CSS_BUILDABLE (object), buildable_id);

      g_hash_table_insert (self->object_table,
                           g_steal_pointer (&buildable_id),
                           object);
    }

  /* Connect signal handlers */
  for (i = 0; i < n_signal_connections; i++)
    {
      SignalConnectionData *data = &signal_connections[i];
      GClosure *closure;
      GError *error = NULL;

      // TODO vvvv
      GtkBuilder *b = gtk_builder_new ();
      closure = gtk_builder_scope_create_closure (self->builder_scope,
                                                  b,
                                                  data->handler_name,
                                                  data->swapped,
                                                 self->template_object ? self->template_object : object,
                                                  &error);

      if (error)
        {
          g_error ("%s", error->message);
        }

      g_signal_connect_closure_by_id (object,
                                      data->signal_id,
                                      data->signal_detail,
                                      closure,
                                      data->after);

    }

  gtk_css_parser_end_block (parser); /* Object block */
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

  self->template_object = template_object;
  parse_objects (self, template_object);
}

GObject *
gtk_builder_css_parser_get_object (GtkBuilderCssParser *self,
                                   const char          *object_name)
{
  return g_hash_table_lookup (self->object_table, object_name);
}
