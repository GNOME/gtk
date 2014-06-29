/*
 * Copyright © 2011, 2012 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkbuilderprivate.h"
#include "gtkintl.h"

#include <gio/gio.h>
#include <string.h>

struct frame
{
  GMenu        *menu;
  GMenuItem    *item;
  struct frame *prev;
};

typedef struct
{
  ParserData *parser_data;
  struct frame frame;

  /* attributes */
  gchar        *attribute;
  GVariantType *type;
  GString      *string;

  /* translation */
  gchar        *context;
  gboolean      translatable;
} GtkBuilderMenuState;

static void
gtk_builder_menu_push_frame (GtkBuilderMenuState *state,
                             GMenu               *menu,
                             GMenuItem           *item)
{
  struct frame *new;

  new = g_slice_new (struct frame);
  *new = state->frame;

  state->frame.menu = menu;
  state->frame.item = item;
  state->frame.prev = new;
}

static void
gtk_builder_menu_pop_frame (GtkBuilderMenuState *state)
{
  struct frame *prev = state->frame.prev;

  if (state->frame.item)
    {
      g_assert (prev->menu != NULL);
      g_menu_append_item (prev->menu, state->frame.item);
      g_object_unref (state->frame.item);
    }

  state->frame = *prev;

  g_slice_free (struct frame, prev);
}

static void
gtk_builder_menu_start_element (GMarkupParseContext  *context,
                                const gchar          *element_name,
                                const gchar         **attribute_names,
                                const gchar         **attribute_values,
                                gpointer              user_data,
                                GError              **error)
{
  GtkBuilderMenuState *state = user_data;

#define COLLECT(first, ...) \
  g_markup_collect_attributes (element_name,                                 \
                               attribute_names, attribute_values, error,     \
                               first, __VA_ARGS__, G_MARKUP_COLLECT_INVALID)
#define OPTIONAL   G_MARKUP_COLLECT_OPTIONAL
#define BOOLEAN    G_MARKUP_COLLECT_BOOLEAN
#define STRING     G_MARKUP_COLLECT_STRING

  if (state->frame.menu)
    {
      /* Can have '<item>', '<submenu>' or '<section>' here. */
      if (g_str_equal (element_name, "item"))
        {
          GMenuItem *item;

          if (COLLECT (G_MARKUP_COLLECT_INVALID, NULL))
            {
              item = g_menu_item_new (NULL, NULL);
              gtk_builder_menu_push_frame (state, NULL, item);
            }

          return;
        }

      else if (g_str_equal (element_name, "submenu"))
        {
          const gchar *id;

          if (COLLECT (STRING | OPTIONAL, "id", &id))
            {
              GMenuItem *item;
              GMenu *menu;

              menu = g_menu_new ();
              item = g_menu_item_new_submenu (NULL, G_MENU_MODEL (menu));
              gtk_builder_menu_push_frame (state, menu, item);

              if (id != NULL)
                _gtk_builder_add_object (state->parser_data->builder, id, G_OBJECT (menu));
              g_object_unref (menu);
            }

          return;
        }

      else if (g_str_equal (element_name, "section"))
        {
          const gchar *id;

          if (COLLECT (STRING | OPTIONAL, "id", &id))
            {
              GMenuItem *item;
              GMenu *menu;

              menu = g_menu_new ();
              item = g_menu_item_new_section (NULL, G_MENU_MODEL (menu));
              gtk_builder_menu_push_frame (state, menu, item);

              if (id != NULL)
                _gtk_builder_add_object (state->parser_data->builder, id, G_OBJECT (menu));
              g_object_unref (menu);
            }

          return;
        }
    }

  if (state->frame.item)
    {
      /* Can have '<attribute>' or '<link>' here. */
      if (g_str_equal (element_name, "attribute"))
        {
          const gchar *typestr;
          const gchar *name;
          const gchar *context;

          if (COLLECT (STRING,             "name", &name,
                       OPTIONAL | BOOLEAN, "translatable", &state->translatable,
                       OPTIONAL | STRING,  "context", &context,
                       OPTIONAL | STRING,  "comments", NULL, /* ignore, just for translators */
                       OPTIONAL | STRING,  "type", &typestr))
            {
              if (typestr && !g_variant_type_string_is_valid (typestr))
                {
                  g_set_error (error, G_VARIANT_PARSE_ERROR,
                               G_VARIANT_PARSE_ERROR_INVALID_TYPE_STRING,
                               "Invalid GVariant type string '%s'", typestr);
                  return;
                }

              state->type = typestr ? g_variant_type_new (typestr) : NULL;
              state->string = g_string_new (NULL);
              state->attribute = g_strdup (name);
              state->context = g_strdup (context);

              gtk_builder_menu_push_frame (state, NULL, NULL);
            }

          return;
        }

      if (g_str_equal (element_name, "link"))
        {
          const gchar *name;
          const gchar *id;

          if (COLLECT (STRING,            "name", &name,
                       STRING | OPTIONAL, "id",   &id))
            {
              GMenu *menu;

              menu = g_menu_new ();
              g_menu_item_set_link (state->frame.item, name, G_MENU_MODEL (menu));
              gtk_builder_menu_push_frame (state, menu, NULL);

              if (id != NULL)
                _gtk_builder_add_object (state->parser_data->builder, id, G_OBJECT (menu));
              g_object_unref (menu);
            }

          return;
        }
    }

  {
    const GSList *element_stack;

    element_stack = g_markup_parse_context_get_element_stack (context);

    if (element_stack->next)
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                   _("Element <%s> not allowed inside <%s>"),
                   element_name, (const gchar *) element_stack->next->data);

    else
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                   _("Element <%s> not allowed at toplevel"), element_name);
  }
}

static void
gtk_builder_menu_end_element (GMarkupParseContext  *context,
                              const gchar          *element_name,
                              gpointer              user_data,
                              GError              **error)
{
  GtkBuilderMenuState *state = user_data;

  gtk_builder_menu_pop_frame (state);

  if (state->string)
    {
      GVariant *value;
      gchar *text;

      text = g_string_free (state->string, FALSE);
      state->string = NULL;

      /* do the translation if necessary */
      if (state->translatable)
        {
          const gchar *translated;

          if (state->context)
            translated = g_dpgettext2 (state->parser_data->domain, state->context, text);
          else
            translated = g_dgettext (state->parser_data->domain, text);

         if (translated != text)
           {
             /* it's safe because we know that translated != text */
             g_free (text);
             text = g_strdup (translated);
           }
        }

      if (state->type == NULL)
        /* No type string specified -> it's a normal string. */
        g_menu_item_set_attribute (state->frame.item, state->attribute, "s", text);

      /* Else, we try to parse it according to the type string.  If
       * error is set here, it will follow us out, ending the parse.
       *
       * We still need to free everything, though, so ignore it here.
       */
      else if ((value = g_variant_parse (state->type, text, NULL, NULL, error)))
        {
          g_menu_item_set_attribute_value (state->frame.item, state->attribute, value);
          g_variant_unref (value);
        }

      if (state->type)
        {
          g_variant_type_free (state->type);
          state->type = NULL;
        }

      g_free (state->context);
      state->context = NULL;

      g_free (state->attribute);
      state->attribute = NULL;

      g_free (text);
    }
}

static void
gtk_builder_menu_text (GMarkupParseContext  *context,
                       const gchar          *text,
                       gsize                 text_len,
                       gpointer              user_data,
                       GError              **error)
{
  GtkBuilderMenuState *state = user_data;
  gint i;

  for (i = 0; i < text_len; i++)
    if (!g_ascii_isspace (text[i]))
      {
        if (state->string)
          g_string_append_len (state->string, text, text_len);

        else
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       _("text may not appear inside <%s>"),
                       g_markup_parse_context_get_element (context));
        break;
      }
}

static void
gtk_builder_menu_error (GMarkupParseContext *context,
                        GError              *error,
                        gpointer             user_data)
{
  GtkBuilderMenuState *state = user_data;

  while (state->frame.prev)
    {
      struct frame *prev = state->frame.prev;

      state->frame = *prev;

      g_slice_free (struct frame, prev);
    }

  if (state->string)
    g_string_free (state->string, TRUE);

  if (state->type)
    g_variant_type_free (state->type);

  g_free (state->attribute);
  g_free (state->context);

  g_slice_free (GtkBuilderMenuState, state);
}

static GMarkupParser gtk_builder_menu_subparser =
{
  gtk_builder_menu_start_element,
  gtk_builder_menu_end_element,
  gtk_builder_menu_text,
  NULL,                            /* passthrough */
  gtk_builder_menu_error
};

void
_gtk_builder_menu_start (ParserData   *parser_data,
                         const gchar  *element_name,
                         const gchar **attribute_names,
                         const gchar **attribute_values,
                         GError      **error)
{
  GtkBuilderMenuState *state;
  gchar *id;

  state = g_slice_new0 (GtkBuilderMenuState);
  state->parser_data = parser_data;
  g_markup_parse_context_push (parser_data->ctx, &gtk_builder_menu_subparser, state);

  if (COLLECT (STRING, "id", &id))
    {
      GMenu *menu;

      menu = g_menu_new ();
      _gtk_builder_add_object (state->parser_data->builder, id, G_OBJECT (menu));
      gtk_builder_menu_push_frame (state, menu, NULL);
      g_object_unref (menu);
    }
}

void
_gtk_builder_menu_end (ParserData *parser_data)
{
  GtkBuilderMenuState *state;

  state = g_markup_parse_context_pop (parser_data->ctx);
  gtk_builder_menu_pop_frame (state);

  g_assert (state->frame.prev == NULL);
  g_assert (state->frame.item == NULL);
  g_assert (state->frame.menu == NULL);
  g_slice_free (GtkBuilderMenuState, state);
}
