/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity theme parsing */

/*
 * Copyright (C) 2001 Havoc Pennington
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include "theme-parser.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

/* We were intending to put the version number
 * in the subdirectory name, but we ended up
 * using the filename instead.  The "-1" survives
 * as a fossil.
 */
#define THEME_SUBDIR "metacity-1"

/* Highest version of the theme format to
 * look out for.
 */
#define THEME_MAJOR_VERSION 3
#define THEME_MINOR_VERSION 1
#define THEME_VERSION (1000 * THEME_MAJOR_VERSION + THEME_MINOR_VERSION)

#define METACITY_THEME_FILENAME_FORMAT "metacity-theme-%d.xml"

typedef enum
{
  STATE_START,
  STATE_THEME,
  /* info section */
  STATE_INFO,
  STATE_NAME,
  STATE_AUTHOR,
  STATE_COPYRIGHT,
  STATE_DATE,
  STATE_DESCRIPTION,
  /* constants */
  STATE_CONSTANT,
  /* geometry */
  STATE_FRAME_GEOMETRY,
  STATE_DISTANCE,
  STATE_BORDER,
  STATE_ASPECT_RATIO,
  /* draw ops */
  STATE_DRAW_OPS,
  STATE_LINE,
  STATE_RECTANGLE,
  STATE_ARC,
  STATE_CLIP,
  STATE_TINT,
  STATE_GRADIENT,
  STATE_IMAGE,
  STATE_GTK_ARROW,
  STATE_GTK_BOX,
  STATE_GTK_VLINE,
  STATE_ICON,
  STATE_TITLE,
  STATE_INCLUDE, /* include another draw op list */
  STATE_TILE,    /* tile another draw op list */
  /* sub-parts of gradient */
  STATE_COLOR,
  /* frame style */
  STATE_FRAME_STYLE,
  STATE_PIECE,
  STATE_BUTTON,
  /* style set */
  STATE_FRAME_STYLE_SET,
  STATE_FRAME,
  /* assigning style sets to windows */
  STATE_WINDOW,
  /* things we don't use any more but we can still parse: */
  STATE_MENU_ICON,
  STATE_FALLBACK
} ParseState;

typedef struct
{
  /* This two lists contain stacks of state and required version
   * (cast to pointers.) There is one list item for each currently
   * open element. */
  GSList *states;
  GSList *required_versions;

  const char *theme_name;       /* name of theme (directory it's in) */
  const char *theme_file;       /* theme filename */
  const char *theme_dir;        /* dir the theme is inside */
  MetaTheme *theme;             /* theme being parsed */
  guint format_version;         /* version of format of theme file */  
  char *name;                   /* name of named thing being parsed */
  MetaFrameLayout *layout;      /* layout being parsed if any */
  MetaDrawOpList *op_list;      /* op list being parsed if any */
  MetaDrawOp *op;               /* op being parsed if any */
  MetaFrameStyle *style;        /* frame style being parsed if any */
  MetaFrameStyleSet *style_set; /* frame style set being parsed if any */
  MetaFramePiece piece;         /* position of piece being parsed */
  MetaButtonType button_type;   /* type of button/menuitem being parsed */
  MetaButtonState button_state; /* state of button being parsed */
  int skip_level;               /* depth of elements that we're ignoring */
} ParseInfo;

typedef enum {
  THEME_PARSE_ERROR_TOO_OLD,
  THEME_PARSE_ERROR_TOO_FAILED
} ThemeParseError;

static GQuark
theme_parse_error_quark (void)
{
  return g_quark_from_static_string ("theme-parse-error-quark");
}

#define THEME_PARSE_ERROR (theme_parse_error_quark ())

static void set_error (GError             **err,
                       GMarkupParseContext *context,
                       int                  error_domain,
                       int                  error_code,
                       const char          *format,
                       ...) G_GNUC_PRINTF (5, 6);

static void add_context_to_error (GError             **err,
                                  GMarkupParseContext *context);

static void       parse_info_init (ParseInfo *info);
static void       parse_info_free (ParseInfo *info);

static void       push_state (ParseInfo  *info,
                              ParseState  state);
static void       pop_state  (ParseInfo  *info);
static ParseState peek_state (ParseInfo  *info);


static void parse_toplevel_element  (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_info_element      (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_geometry_element  (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_draw_op_element   (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_gradient_element  (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_style_element     (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_style_set_element (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);

static void parse_piece_element     (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);

static void parse_button_element    (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);

static void parse_menu_icon_element (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);

static void start_element_handler (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   const gchar         **attribute_names,
                                   const gchar         **attribute_values,
                                   gpointer              user_data,
                                   GError              **error);
static void end_element_handler   (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   gpointer              user_data,
                                   GError              **error);
static void text_handler          (GMarkupParseContext  *context,
                                   const gchar          *text,
                                   gsize                 text_len,
                                   gpointer              user_data,
                                   GError              **error);

/* Translators: This means that an attribute which should have been found
 * on an XML element was not in fact found.
 */
#define ATTRIBUTE_NOT_FOUND _("No \"%s\" attribute on element <%s>")

static GMarkupParser metacity_theme_parser = {
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  NULL
};

static void
set_error (GError             **err,
           GMarkupParseContext *context,
           int                  error_domain,
           int                  error_code,
           const char          *format,
           ...)
{
  int line, ch;
  va_list args;
  char *str;
  
  g_markup_parse_context_get_position (context, &line, &ch);

  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  g_set_error (err, error_domain, error_code,
               _("Line %d character %d: %s"),
               line, ch, str);

  g_free (str);
}

static void
add_context_to_error (GError             **err,
                      GMarkupParseContext *context)
{
  int line, ch;
  char *str;

  if (err == NULL || *err == NULL)
    return;

  g_markup_parse_context_get_position (context, &line, &ch);

  str = g_strdup_printf (_("Line %d character %d: %s"),
                         line, ch, (*err)->message);
  g_free ((*err)->message);
  (*err)->message = str;
}

static void
parse_info_init (ParseInfo *info)
{
  info->theme_file = NULL;
  info->states = g_slist_prepend (NULL, GINT_TO_POINTER (STATE_START));
  info->required_versions = NULL;
  info->theme = NULL;
  info->name = NULL;
  info->layout = NULL;
  info->op_list = NULL;
  info->op = NULL;
  info->style = NULL;
  info->style_set = NULL;
  info->piece = META_FRAME_PIECE_LAST;
  info->button_type = META_BUTTON_TYPE_LAST;
  info->button_state = META_BUTTON_STATE_LAST;
  info->skip_level = 0;
}

static void
parse_info_free (ParseInfo *info)
{
  g_slist_free (info->states);
  g_slist_free (info->required_versions);
  
  if (info->theme)
    meta_theme_free (info->theme);

  if (info->layout)
    meta_frame_layout_unref (info->layout);

  if (info->op_list)
    meta_draw_op_list_unref (info->op_list);

  if (info->op)
    meta_draw_op_free (info->op);
  
  if (info->style)
    meta_frame_style_unref (info->style);

  if (info->style_set)
    meta_frame_style_set_unref (info->style_set);
}

static void
push_state (ParseInfo  *info,
            ParseState  state)
{
  info->states = g_slist_prepend (info->states, GINT_TO_POINTER (state));
}

static void
pop_state (ParseInfo *info)
{
  g_return_if_fail (info->states != NULL);
  
  info->states = g_slist_remove (info->states, info->states->data);
}

static ParseState
peek_state (ParseInfo *info)
{
  g_return_val_if_fail (info->states != NULL, STATE_START);

  return GPOINTER_TO_INT (info->states->data);
}

static void
push_required_version (ParseInfo *info,
                       int        version)
{
  info->required_versions = g_slist_prepend (info->required_versions,
                                             GINT_TO_POINTER (version));
}

static void
pop_required_version (ParseInfo *info)
{
  g_return_if_fail (info->required_versions != NULL);

  info->required_versions = g_slist_delete_link (info->required_versions, info->required_versions);
}

static int
peek_required_version (ParseInfo *info)
{
  if (info->required_versions)
    return GPOINTER_TO_INT (info->required_versions->data);
  else
    return info->format_version;
}

#define ELEMENT_IS(name) (strcmp (element_name, (name)) == 0)

typedef struct
{
  const char  *name;
  const char **retloc;
  gboolean required;
} LocateAttr;

/* Attribute names can have a leading '!' to indicate that they are
 * required.
 */
static gboolean
locate_attributes (GMarkupParseContext *context,
                   const char  *element_name,
                   const char **attribute_names,
                   const char **attribute_values,
                   GError     **error,
                   const char  *first_attribute_name,
                   const char **first_attribute_retloc,
                   ...)
{
  va_list args;
  const char *name;
  const char **retloc;
  int n_attrs;
#define MAX_ATTRS 24
  LocateAttr attrs[MAX_ATTRS];
  gboolean retval;
  int i;

  g_return_val_if_fail (first_attribute_name != NULL, FALSE);
  g_return_val_if_fail (first_attribute_retloc != NULL, FALSE);

  retval = TRUE;

  /* FIXME: duplicated code; refactor loop */
  n_attrs = 1;
  attrs[0].name = first_attribute_name;
  attrs[0].retloc = first_attribute_retloc;
  attrs[0].required = attrs[0].name[0]=='!';
  if (attrs[0].required)
    attrs[0].name++; /* skip past it */
  *first_attribute_retloc = NULL;
  
  va_start (args, first_attribute_retloc);

  name = va_arg (args, const char*);
  retloc = va_arg (args, const char**);

  while (name != NULL)
    {
      g_return_val_if_fail (retloc != NULL, FALSE);

      g_assert (n_attrs < MAX_ATTRS);
      
      attrs[n_attrs].name = name;
      attrs[n_attrs].retloc = retloc;
      attrs[n_attrs].required = attrs[n_attrs].name[0]=='!';
      if (attrs[n_attrs].required)
        attrs[n_attrs].name++; /* skip past it */

      n_attrs += 1;
      *retloc = NULL;      

      name = va_arg (args, const char*);
      retloc = va_arg (args, const char**);
    }

  va_end (args);

  i = 0;
  while (attribute_names[i])
    {
      int j;
      gboolean found;

      /* Can be present anywhere */
      if (strcmp (attribute_names[i], "version") == 0)
        {
          ++i;
          continue;
        }

      found = FALSE;
      j = 0;
      while (j < n_attrs)
        {
          if (strcmp (attrs[j].name, attribute_names[i]) == 0)
            {
              retloc = attrs[j].retloc;

              if (*retloc != NULL)
                {
                
                  set_error (error, context,
                             G_MARKUP_ERROR,
                             G_MARKUP_ERROR_PARSE,
                             _("Attribute \"%s\" repeated twice on the same <%s> element"),
                             attrs[j].name, element_name);
                  retval = FALSE;
                  goto out;
                }

              *retloc = attribute_values[i];
              found = TRUE;
            }

          ++j;
        }

      if (!found)
        {
      j = 0;
      while (j < n_attrs)
        {
          g_warning ("It could have been %s.\n", attrs[j++].name);
        }
                  
          set_error (error, context,
                     G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Attribute \"%s\" is invalid on <%s> element in this context"),
                     attribute_names[i], element_name);
          retval = FALSE;
          goto out;
        }

      ++i;
    }

    /* Did we catch them all? */
    i = 0;
    while (i < n_attrs)
      {
        if (attrs[i].required && *(attrs[i].retloc)==NULL)
          {
            set_error (error, context,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_PARSE,
                       ATTRIBUTE_NOT_FOUND,
                       attrs[i].name, element_name);
            retval = FALSE;
            goto out;
          }

        ++i;
      }

 out:
  return retval;
}

static gboolean
check_no_attributes (GMarkupParseContext *context,
                     const char  *element_name,
                     const char **attribute_names,
                     const char **attribute_values,
                     GError     **error)
{
  int i = 0;

  /* Can be present anywhere */
  if (attribute_names[0] && strcmp (attribute_names[i], "version") == 0)
    i++;

  if (attribute_names[i] != NULL)
    {
      set_error (error, context,
                 G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Attribute \"%s\" is invalid on <%s> element in this context"),
                 attribute_names[0], element_name);
      return FALSE;
    }

  return TRUE;
}

#define MAX_REASONABLE 4096
static gboolean
parse_positive_integer (const char          *str,
                        int                 *val,
                        GMarkupParseContext *context,
                        MetaTheme           *theme,
                        GError             **error)
{
  char *end;
  long l;
  int j;

  *val = 0;
  
  end = NULL;
  
  /* Is str a constant? */

  if (META_THEME_ALLOWS (theme, META_THEME_UBIQUITOUS_CONSTANTS) &&
      meta_theme_lookup_int_constant (theme, str, &j))
    {
      /* Yes. */
      l = j;
    }
  else
    {
      /* No. Let's try parsing it instead. */

      l = strtol (str, &end, 10);

      if (end == NULL || end == str)
      {
        set_error (error, context, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_PARSE,
                   _("Could not parse \"%s\" as an integer"),
                   str);
        return FALSE;
      }

    if (*end != '\0')
      {
        set_error (error, context, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_PARSE,
                   _("Did not understand trailing characters \"%s\" in string \"%s\""),
                   end, str);
        return FALSE;
      }
    }

  if (l < 0)
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Integer %ld must be positive"), l);
      return FALSE;
    }

  if (l > MAX_REASONABLE)
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Integer %ld is too large, current max is %d"),
                 l, MAX_REASONABLE);
      return FALSE;
    }
  
  *val = (int) l;

  return TRUE;
}

static gboolean
parse_double (const char          *str,
              double              *val,
              GMarkupParseContext *context,
              GError             **error)
{
  char *end;

  *val = 0;
  
  end = NULL;
  
  *val = g_ascii_strtod (str, &end);

  if (end == NULL || end == str)
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Could not parse \"%s\" as a floating point number"),
                 str);
      return FALSE;
    }

  if (*end != '\0')
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Did not understand trailing characters \"%s\" in string \"%s\""),
                 end, str);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_boolean (const char          *str,
               gboolean            *val,
               GMarkupParseContext *context,
               GError             **error)
{
  if (strcmp ("true", str) == 0)
    *val = TRUE;
  else if (strcmp ("false", str) == 0)
    *val = FALSE;
  else
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Boolean values must be \"true\" or \"false\" not \"%s\""),
                 str);
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
parse_rounding (const char          *str,
                guint               *val,
                GMarkupParseContext *context,
                MetaTheme           *theme,
                GError             **error)
{
  if (strcmp ("true", str) == 0)
    *val = 5; /* historical "true" value */
  else if (strcmp ("false", str) == 0)
    *val = 0;
  else
    {
      int tmp;
      gboolean result;
       if (!META_THEME_ALLOWS (theme, META_THEME_VARIED_ROUND_CORNERS))
         {
           /* Not known in this version, so bail. */
           set_error (error, context, G_MARKUP_ERROR,
                      G_MARKUP_ERROR_PARSE,
                      _("Boolean values must be \"true\" or \"false\" not \"%s\""),
                      str);
           return FALSE;
         }
   
      result = parse_positive_integer (str, &tmp, context, theme, error);

      *val = tmp;

      return result;    
    }
  
  return TRUE;
}

static gboolean
parse_angle (const char          *str,
             double              *val,
             GMarkupParseContext *context,
             GError             **error)
{
  if (!parse_double (str, val, context, error))
    return FALSE;

  if (*val < (0.0 - 1e6) || *val > (360.0 + 1e6))
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Angle must be between 0.0 and 360.0, was %g\n"),
                 *val);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_alpha (const char             *str,
             MetaAlphaGradientSpec **spec_ret,
             GMarkupParseContext    *context,
             GError                **error)
{
  char **split;
  int i;
  int n_alphas;
  MetaAlphaGradientSpec *spec;

  *spec_ret = NULL;
  
  split = g_strsplit (str, ":", -1);

  i = 0;
  while (split[i])
    ++i;

  if (i == 0)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Could not parse \"%s\" as a floating point number"),
                 str);

      g_strfreev (split);
      
      return FALSE;
    }

  n_alphas = i;

  /* FIXME allow specifying horizontal/vertical/diagonal in theme format,
   * once we implement vertical/diagonal in gradient.c
   */
  spec = meta_alpha_gradient_spec_new (META_GRADIENT_HORIZONTAL,
                                       n_alphas);

  i = 0;
  while (i < n_alphas)
    {
      double v;
      
      if (!parse_double (split[i], &v, context, error))
        {
          /* clear up, but don't set error: it was set by parse_double */
          g_strfreev (split);
          meta_alpha_gradient_spec_free (spec);
          
          return FALSE;
        }

      if (v < (0.0 - 1e-6) || v > (1.0 + 1e-6))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Alpha must be between 0.0 (invisible) and 1.0 (fully opaque), was %g\n"),
                     v);

          g_strfreev (split);
          meta_alpha_gradient_spec_free (spec);          
          
          return FALSE;
        }

      spec->alphas[i] = (unsigned char) (v * 255);
      
      ++i;
    }  

  g_strfreev (split);
  
  *spec_ret = spec;
  
  return TRUE;
}

static MetaColorSpec*
parse_color (MetaTheme *theme,
             const char        *str,
             GError           **err)
{
  char* referent;

  if (META_THEME_ALLOWS (theme, META_THEME_COLOR_CONSTANTS) &&
      meta_theme_lookup_color_constant (theme, str, &referent))
    {
      if (referent)
        return meta_color_spec_new_from_string (referent, err);
      
      /* no need to free referent: it's a pointer into the actual hash table */
    }
  
  return meta_color_spec_new_from_string (str, err);
}

static gboolean
parse_title_scale (const char          *str,
                   double              *val,
                   GMarkupParseContext *context,
                   GError             **error)
{
  double factor;
  
  if (strcmp (str, "xx-small") == 0)
    factor = PANGO_SCALE_XX_SMALL;
  else if (strcmp (str, "x-small") == 0)
    factor = PANGO_SCALE_X_SMALL;
  else if (strcmp (str, "small") == 0)
    factor = PANGO_SCALE_SMALL;
  else if (strcmp (str, "medium") == 0)
    factor = PANGO_SCALE_MEDIUM;
  else if (strcmp (str, "large") == 0)
    factor = PANGO_SCALE_LARGE;
  else if (strcmp (str, "x-large") == 0)
    factor = PANGO_SCALE_X_LARGE;
  else if (strcmp (str, "xx-large") == 0)
    factor = PANGO_SCALE_XX_LARGE;
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Invalid title scale \"%s\" (must be one of xx-small,x-small,small,medium,large,x-large,xx-large)\n"),
                 str);
      return FALSE;
    }

  *val = factor;
  
  return TRUE;
}

static void
parse_toplevel_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_THEME);

  if (ELEMENT_IS ("info"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_INFO);
    }
  else if (ELEMENT_IS ("constant"))
    {
      const char *name;
      const char *value;
      int ival = 0;
      double dval = 0.0;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name, "!value", &value,
                              NULL))
        return;

      if (strchr (value, '.') && parse_double (value, &dval, context, error))
        {
          g_clear_error (error);

          if (!meta_theme_define_float_constant (info->theme,
                                                 name,
                                                 dval,
                                                 error))
            {
              add_context_to_error (error, context);
              return;
            }
        }
      else if (parse_positive_integer (value, &ival, context, info->theme, error))
        {
          g_clear_error (error);

          if (!meta_theme_define_int_constant (info->theme,
                                               name,
                                               ival,
                                               error))
            {
              add_context_to_error (error, context);
              return;
            }
        }
      else
        {
          g_clear_error (error);

          if (!meta_theme_define_color_constant (info->theme,
                                                 name,
                                                 value,
                                                 error))
            {
              add_context_to_error (error, context);
              return;
            }
        }

      push_state (info, STATE_CONSTANT);
    }
  else if (ELEMENT_IS ("frame_geometry"))
    {
      const char *name = NULL;
      const char *parent = NULL;
      const char *has_title = NULL;
      const char *title_scale = NULL;
      const char *rounded_top_left = NULL;
      const char *rounded_top_right = NULL;
      const char *rounded_bottom_left = NULL;
      const char *rounded_bottom_right = NULL;
      const char *hide_buttons = NULL;
      gboolean has_title_val;
      guint rounded_top_left_val;
      guint rounded_top_right_val;
      guint rounded_bottom_left_val;
      guint rounded_bottom_right_val;
      gboolean hide_buttons_val;
      double title_scale_val;
      MetaFrameLayout *parent_layout;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name, "parent", &parent,
                              "has_title", &has_title, "title_scale", &title_scale,
                              "rounded_top_left", &rounded_top_left,
                              "rounded_top_right", &rounded_top_right,
                              "rounded_bottom_left", &rounded_bottom_left,
                              "rounded_bottom_right", &rounded_bottom_right,
                              "hide_buttons", &hide_buttons,
                              NULL))
        return;

      has_title_val = TRUE;
      if (has_title && !parse_boolean (has_title, &has_title_val, context, error))
        return;

      hide_buttons_val = FALSE;
      if (hide_buttons && !parse_boolean (hide_buttons, &hide_buttons_val, context, error))
        return;

      rounded_top_left_val = 0;
      rounded_top_right_val = 0;
      rounded_bottom_left_val = 0;
      rounded_bottom_right_val = 0;

      if (rounded_top_left && !parse_rounding (rounded_top_left, &rounded_top_left_val, context, info->theme, error))
        return;
      if (rounded_top_right && !parse_rounding (rounded_top_right, &rounded_top_right_val, context, info->theme, error))
        return;
      if (rounded_bottom_left && !parse_rounding (rounded_bottom_left, &rounded_bottom_left_val, context, info->theme, error))
        return;      
      if (rounded_bottom_right && !parse_rounding (rounded_bottom_right, &rounded_bottom_right_val, context, info->theme, error))
        return;
      
      title_scale_val = 1.0;
      if (title_scale && !parse_title_scale (title_scale, &title_scale_val, context, error))
        return;
      
      if (meta_theme_lookup_layout (info->theme, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_layout = NULL;
      if (parent)
        {
          parent_layout = meta_theme_lookup_layout (info->theme, parent);
          if (parent_layout == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent \"%s\" has not been defined"),
                         element_name, parent);
              return;
            }
        }

      g_assert (info->layout == NULL);

      if (parent_layout)
        info->layout = meta_frame_layout_copy (parent_layout);
      else
        info->layout = meta_frame_layout_new ();

      if (has_title) /* only if explicit, otherwise inherit */
        info->layout->has_title = has_title_val;

      if (META_THEME_ALLOWS (info->theme, META_THEME_HIDDEN_BUTTONS) && hide_buttons_val)
          info->layout->hide_buttons = hide_buttons_val;

      if (title_scale)
	info->layout->title_scale = title_scale_val;

      if (rounded_top_left)
        info->layout->top_left_corner_rounded_radius = rounded_top_left_val;

      if (rounded_top_right)
        info->layout->top_right_corner_rounded_radius = rounded_top_right_val;

      if (rounded_bottom_left)
        info->layout->bottom_left_corner_rounded_radius = rounded_bottom_left_val;

      if (rounded_bottom_right)
        info->layout->bottom_right_corner_rounded_radius = rounded_bottom_right_val;
      
      meta_theme_insert_layout (info->theme, name, info->layout);

      push_state (info, STATE_FRAME_GEOMETRY);
    }
  else if (ELEMENT_IS ("draw_ops"))
    {
      const char *name = NULL;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name,
                              NULL))
        return;

      if (meta_theme_lookup_draw_op_list (info->theme, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      meta_theme_insert_draw_op_list (info->theme, name, info->op_list);

      push_state (info, STATE_DRAW_OPS);
    }
  else if (ELEMENT_IS ("frame_style"))
    {
      const char *name = NULL;
      const char *parent = NULL;
      const char *geometry = NULL;
      const char *background = NULL;
      const char *alpha = NULL;
      MetaFrameStyle *parent_style;
      MetaFrameLayout *layout;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name, "parent", &parent,
                              "geometry", &geometry,
                              "background", &background,
                              "alpha", &alpha,
                              NULL))
        return;

      if (meta_theme_lookup_style (info->theme, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_style = NULL;
      if (parent)
        {
          parent_style = meta_theme_lookup_style (info->theme, parent);
          if (parent_style == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent \"%s\" has not been defined"),
                         element_name, parent);
              return;
            }
        }

      layout = NULL;
      if (geometry)
        {
          layout = meta_theme_lookup_layout (info->theme, geometry);
          if (layout == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> geometry \"%s\" has not been defined"),
                         element_name, geometry);
              return;
            }
        }
      else if (parent_style)
        {
          layout = parent_style->layout;
        }

      if (layout == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> must specify either a geometry or a parent that has a geometry"),
                     element_name);
          return;
        }

      g_assert (info->style == NULL);

      info->style = meta_frame_style_new (parent_style);
      g_assert (info->style->layout == NULL);
      meta_frame_layout_ref (layout);
      info->style->layout = layout;

      if (background != NULL && META_THEME_ALLOWS (info->theme, META_THEME_FRAME_BACKGROUNDS))
        {
          info->style->window_background_color = meta_color_spec_new_from_string (background, error);
          if (!info->style->window_background_color)
            return;

          if (alpha != NULL)
            {
            
               gboolean success;
               MetaAlphaGradientSpec *alpha_vector;
               
               g_clear_error (error);
               /* fortunately, we already have a routine to parse alpha values,
                * though it produces a vector of them, which is a superset of
                * what we want.
                */
               success = parse_alpha (alpha, &alpha_vector, context, error); 
               if (!success)
                 return;

               /* alpha_vector->alphas must contain at least one element */
               info->style->window_background_alpha = alpha_vector->alphas[0];

               meta_alpha_gradient_spec_free (alpha_vector);
            }
        }
      else if (alpha != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("You must specify a background for an alpha value to be meaningful"));
          return;
        }

      meta_theme_insert_style (info->theme, name, info->style);

      push_state (info, STATE_FRAME_STYLE);
    }
  else if (ELEMENT_IS ("frame_style_set"))
    {
      const char *name = NULL;
      const char *parent = NULL;
      MetaFrameStyleSet *parent_set;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!name", &name, "parent", &parent,
                              NULL))
        return;

      if (meta_theme_lookup_style_set (info->theme, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_set = NULL;
      if (parent)
        {
          parent_set = meta_theme_lookup_style_set (info->theme, parent);
          if (parent_set == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent \"%s\" has not been defined"),
                         element_name, parent);
              return;
            }
        }

      g_assert (info->style_set == NULL);

      info->style_set = meta_frame_style_set_new (parent_set);

      meta_theme_insert_style_set (info->theme, name, info->style_set);

      push_state (info, STATE_FRAME_STYLE_SET);
    }
  else if (ELEMENT_IS ("window"))
    {
      const char *type_name = NULL;
      const char *style_set_name = NULL;
      MetaFrameStyleSet *style_set;
      MetaFrameType type;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!type", &type_name, "!style_set", &style_set_name,
                              NULL))
        return;

      type = meta_frame_type_from_string (type_name);

      if (type == META_FRAME_TYPE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown type \"%s\" on <%s> element"),
                     type_name, element_name);
          return;
        }

      style_set = meta_theme_lookup_style_set (info->theme,
                                               style_set_name);

      if (style_set == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown style_set \"%s\" on <%s> element"),
                     style_set_name, element_name);
          return;
        }

      if (info->theme->style_sets_by_type[type] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Window type \"%s\" has already been assigned a style set"),
                     type_name);
          return;
        }

      meta_frame_style_set_ref (style_set);
      info->theme->style_sets_by_type[type] = style_set;

      push_state (info, STATE_WINDOW);
    }
  else if (ELEMENT_IS ("menu_icon"))
    {
      /* Not supported any more, but we have to parse it if they include it,
       * for backwards compatibility.
       */
      g_assert (info->op_list == NULL);
      
      push_state (info, STATE_MENU_ICON);
    }
  else if (ELEMENT_IS ("fallback"))
    {
      /* Not supported any more, but we have to parse it if they include it,
       * for backwards compatibility.
       */
      push_state (info, STATE_FALLBACK);
    }
   else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "metacity_theme");
    }
}

static void
parse_info_element (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    const gchar         **attribute_names,
                    const gchar         **attribute_values,
                    ParseInfo            *info,
                    GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_INFO);

  if (ELEMENT_IS ("name"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_NAME);
    }
  else if (ELEMENT_IS ("author"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_AUTHOR);
    }
  else if (ELEMENT_IS ("copyright"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_COPYRIGHT);
    }
  else if (ELEMENT_IS ("description"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_DESCRIPTION);
    }
  else if (ELEMENT_IS ("date"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_DATE);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "info");
    }
}

static void
parse_distance (GMarkupParseContext  *context,
                const gchar          *element_name,
                const gchar         **attribute_names,
                const gchar         **attribute_values,
                ParseInfo            *info,
                GError              **error)
{
  const char *name;
  const char *value;
  int val;
  
  if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                          error,
                          "!name", &name, "!value", &value,
                          NULL))
    return;

  val = 0;
  if (!parse_positive_integer (value, &val, context, info->theme, error))
    return;

  g_assert (val >= 0); /* yeah, "non-negative" not "positive" get over it */
  g_assert (info->layout);

  if (strcmp (name, "left_width") == 0)
    info->layout->left_width = val;
  else if (strcmp (name, "right_width") == 0)
    info->layout->right_width = val;
  else if (strcmp (name, "bottom_height") == 0)
    info->layout->bottom_height = val;
  else if (strcmp (name, "title_vertical_pad") == 0)
    info->layout->title_vertical_pad = val;
  else if (strcmp (name, "right_titlebar_edge") == 0)
    info->layout->right_titlebar_edge = val;
  else if (strcmp (name, "left_titlebar_edge") == 0)
    info->layout->left_titlebar_edge = val;
  else if (strcmp (name, "button_width") == 0)
    {
      info->layout->button_width = val;
            
      if (!(info->layout->button_sizing == META_BUTTON_SIZING_LAST ||
            info->layout->button_sizing == META_BUTTON_SIZING_FIXED))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Cannot specify both \"button_width\"/\"button_height\" and \"aspect_ratio\" for buttons"));
          return;      
        }

      info->layout->button_sizing = META_BUTTON_SIZING_FIXED;
    }
  else if (strcmp (name, "button_height") == 0)
    {
      info->layout->button_height = val;
      
      if (!(info->layout->button_sizing == META_BUTTON_SIZING_LAST ||
            info->layout->button_sizing == META_BUTTON_SIZING_FIXED))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Cannot specify both \"button_width\"/\"button_height\" and \"aspect_ratio\" for buttons"));
          return;      
        }

      info->layout->button_sizing = META_BUTTON_SIZING_FIXED;
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Distance \"%s\" is unknown"), name);
      return;
    }
}

static void
parse_aspect_ratio (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    const gchar         **attribute_names,
                    const gchar         **attribute_values,
                    ParseInfo            *info,
                    GError              **error)
{
  const char *name;
  const char *value;
  double val;
  
  if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                          error,
                          "!name", &name, "!value", &value,
                          NULL))
    return;

  val = 0;
  if (!parse_double (value, &val, context, error))
    return;

  g_assert (info->layout);
  
  if (strcmp (name, "button") == 0)
    {
      info->layout->button_aspect = val;

      if (info->layout->button_sizing != META_BUTTON_SIZING_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Cannot specify both \"button_width\"/\"button_height\" and \"aspect_ratio\" for buttons"));
          return;
        }
      
      info->layout->button_sizing = META_BUTTON_SIZING_ASPECT;
    }
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Aspect ratio \"%s\" is unknown"), name);
      return;
    }
}

static void
parse_border (GMarkupParseContext  *context,
              const gchar          *element_name,
              const gchar         **attribute_names,
              const gchar         **attribute_values,
              ParseInfo            *info,
              GError              **error)
{
  const char *name;
  const char *top;
  const char *bottom;
  const char *left;
  const char *right;
  int top_val;
  int bottom_val;
  int left_val;
  int right_val;
  GtkBorder *border;
  
  if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                          error,
                          "!name", &name,
                          "!top", &top,
                          "!bottom", &bottom,
                          "!left", &left,
                          "!right", &right,
                          NULL))
    return;
  
  top_val = 0;
  if (!parse_positive_integer (top, &top_val, context, info->theme, error))
    return;

  bottom_val = 0;
  if (!parse_positive_integer (bottom, &bottom_val, context, info->theme, error))
    return;

  left_val = 0;
  if (!parse_positive_integer (left, &left_val, context, info->theme, error))
    return;

  right_val = 0;
  if (!parse_positive_integer (right, &right_val, context, info->theme, error))
    return;
  
  g_assert (info->layout);

  border = NULL;
  
  if (strcmp (name, "title_border") == 0)
    border = &info->layout->title_border;
  else if (strcmp (name, "button_border") == 0)
    border = &info->layout->button_border;

  if (border == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Border \"%s\" is unknown"), name);
      return;
    }

  border->top = top_val;
  border->bottom = bottom_val;
  border->left = left_val;
  border->right = right_val;
}

static void
parse_geometry_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_GEOMETRY);

  if (ELEMENT_IS ("distance"))
    {
      parse_distance (context, element_name,
                      attribute_names, attribute_values,
                      info, error);
      push_state (info, STATE_DISTANCE);
    }
  else if (ELEMENT_IS ("border"))
    {
      parse_border (context, element_name,
                    attribute_names, attribute_values,
                    info, error);
      push_state (info, STATE_BORDER);
    }
  else if (ELEMENT_IS ("aspect_ratio"))
    {
      parse_aspect_ratio (context, element_name,
                          attribute_names, attribute_values,
                          info, error);

      push_state (info, STATE_ASPECT_RATIO);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_geometry");
    }
}

#if 0
static gboolean
check_expression (PosToken            *tokens,
                  int                  n_tokens,
                  gboolean             has_object,
                  MetaTheme           *theme,
                  GMarkupParseContext *context,
                  GError             **error)
{
  MetaPositionExprEnv env;
  int x, y;

  /* We set it all to 0 to try and catch divide-by-zero screwups.
   * it's possible we should instead guarantee that widths and heights
   * are at least 1.
   */
  
  env.rect = meta_rect (0, 0, 0, 0);
  if (has_object)
    {
      env.object_width = 0;
      env.object_height = 0;
    }
  else
    {
      env.object_width = -1;
      env.object_height = -1;
    }

  env.left_width = 0;
  env.right_width = 0;
  env.top_height = 0;
  env.bottom_height = 0;
  env.title_width = 0;
  env.title_height = 0;
  
  env.icon_width = 0;
  env.icon_height = 0;
  env.mini_icon_width = 0;
  env.mini_icon_height = 0;
  env.theme = theme;
  
  if (!meta_parse_position_expression (tokens, n_tokens,
                                       &env,
                                       &x, &y,
                                       error))
    {
      add_context_to_error (error, context);
      return FALSE;
    }

  return TRUE;
}
#endif

static void
parse_draw_op_element (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       ParseInfo            *info,
                       GError              **error)
{  
  g_return_if_fail (peek_state (info) == STATE_DRAW_OPS);

  if (ELEMENT_IS ("line"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x1;
      const char *y1;
      const char *x2;
      const char *y2;
      const char *dash_on_length;
      const char *dash_off_length;
      const char *width;
      MetaColorSpec *color_spec;
      int dash_on_val;
      int dash_off_val;
      int width_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x1", &x1, "!y1", &y1,
                              "!x2", &x2, "!y2", &y2,
                              "dash_on_length", &dash_on_length,
                              "dash_off_length", &dash_off_length,
                              "width", &width,
                              NULL))
        return;

#if 0
      if (!check_expression (x1, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y1, FALSE, info->theme, context, error))
        return;

      if (!check_expression (x2, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (y2, FALSE, info->theme, context, error))
        return;
#endif
 
      dash_on_val = 0;
      if (dash_on_length &&
          !parse_positive_integer (dash_on_length, &dash_on_val, context, info->theme, error))
        return;

      dash_off_val = 0;
      if (dash_off_length &&
          !parse_positive_integer (dash_off_length, &dash_off_val, context, info->theme, error))
        return;

      width_val = 0;
      if (width &&
          !parse_positive_integer (width, &width_val, context, info->theme, error))
        return;

      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->theme, color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_LINE);

      op->data.line.color_spec = color_spec;

      op->data.line.x1 = meta_draw_spec_new (info->theme, x1, NULL);
      op->data.line.y1 = meta_draw_spec_new (info->theme, y1, NULL);

      if (strcmp(x1, x2)==0)
        op->data.line.x2 = NULL;
      else
        op->data.line.x2 = meta_draw_spec_new (info->theme, x2, NULL);

      if (strcmp(y1, y2)==0)
        op->data.line.y2 = NULL;
      else
        op->data.line.y2 = meta_draw_spec_new (info->theme, y2, NULL);

      op->data.line.width = width_val;
      op->data.line.dash_on_length = dash_on_val;
      op->data.line.dash_off_length = dash_off_val;

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_LINE);
    }
  else if (ELEMENT_IS ("rectangle"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      gboolean filled_val;
      MetaColorSpec *color_spec;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "filled", &filled,
                              NULL))
        return;

#if 0
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
#endif

      filled_val = FALSE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;
      
      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->theme, color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_RECTANGLE);

      op->data.rectangle.color_spec = color_spec;
      op->data.rectangle.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.rectangle.y = meta_draw_spec_new (info->theme, y, NULL);
      op->data.rectangle.width = meta_draw_spec_new (info->theme, width, NULL);
      op->data.rectangle.height = meta_draw_spec_new (info->theme, 
                                                      height, NULL);

      op->data.rectangle.filled = filled_val;

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_RECTANGLE);
    }
  else if (ELEMENT_IS ("arc"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      const char *start_angle;
      const char *extent_angle;
      const char *from;
      const char *to;
      gboolean filled_val;
      double start_angle_val;
      double extent_angle_val;
      MetaColorSpec *color_spec;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "filled", &filled,
                              "start_angle", &start_angle,
                              "extent_angle", &extent_angle,
                              "from", &from,
                              "to", &to,
                              NULL))
        return;

      if (META_THEME_ALLOWS (info->theme, META_THEME_DEGREES_IN_ARCS) )
        {
          if (start_angle == NULL && from == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No \"start_angle\" or \"from\" attribute on element <%s>"), element_name);
              return;
            }

          if (extent_angle == NULL && to == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No \"extent_angle\" or \"to\" attribute on element <%s>"), element_name);
              return;
            }
        }
      else
        {
          if (start_angle == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         ATTRIBUTE_NOT_FOUND, "start_angle", element_name);
              return;
            }

          if (extent_angle == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         ATTRIBUTE_NOT_FOUND, "extent_angle", element_name);
              return;
            }
        }

#if 0     
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
#endif

      if (start_angle == NULL)
        {
          if (!parse_angle (from, &start_angle_val, context, error))
            return;
          
          start_angle_val = (180-start_angle_val)/360.0;
        }
      else
        {
          if (!parse_angle (start_angle, &start_angle_val, context, error))
            return;
        }
      
      if (extent_angle == NULL)
        {
          if (!parse_angle (to, &extent_angle_val, context, error))
            return;
          
          extent_angle_val = ((180-extent_angle_val)/360.0) - start_angle_val;
        }
      else
        {
           if (!parse_angle (extent_angle, &extent_angle_val, context, error))
             return;
        }
     
      filled_val = FALSE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;
      
      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->theme, color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_ARC);

      op->data.arc.color_spec = color_spec;

      op->data.arc.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.arc.y = meta_draw_spec_new (info->theme, y, NULL);
      op->data.arc.width = meta_draw_spec_new (info->theme, width, NULL);
      op->data.arc.height = meta_draw_spec_new (info->theme, height, NULL);

      op->data.arc.filled = filled_val;
      op->data.arc.start_angle = start_angle_val;
      op->data.arc.extent_angle = extent_angle_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_ARC);
    }
  else if (ELEMENT_IS ("clip"))
    {
      MetaDrawOp *op;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              NULL))
        return;
      
#if 0
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
#endif 
      op = meta_draw_op_new (META_DRAW_CLIP);

      op->data.clip.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.clip.y = meta_draw_spec_new (info->theme, y, NULL);
      op->data.clip.width = meta_draw_spec_new (info->theme, width, NULL);
      op->data.clip.height = meta_draw_spec_new (info->theme, height, NULL);

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_CLIP);
    }
  else if (ELEMENT_IS ("tint"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      MetaAlphaGradientSpec *alpha_spec;
      MetaColorSpec *color_spec;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "!alpha", &alpha,
                              NULL))
        return;

#if 0
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
#endif
      alpha_spec = NULL;
      if (!parse_alpha (alpha, &alpha_spec, context, error))
        return;
      
      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->theme, color, error);
      if (color_spec == NULL)
        {
          if (alpha_spec)
            meta_alpha_gradient_spec_free (alpha_spec);
          
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_TINT);

      op->data.tint.color_spec = color_spec;
      op->data.tint.alpha_spec = alpha_spec;

      op->data.tint.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.tint.y = meta_draw_spec_new (info->theme, y, NULL);
      op->data.tint.width = meta_draw_spec_new (info->theme, width, NULL);
      op->data.tint.height = meta_draw_spec_new (info->theme, height, NULL);

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TINT);
    }
  else if (ELEMENT_IS ("gradient"))
    {
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *type;
      const char *alpha;
      MetaAlphaGradientSpec *alpha_spec;
      MetaGradientType type_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!type", &type,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "alpha", &alpha,
                              NULL))
        return;

#if 0
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
#endif
  
      type_val = meta_gradient_type_from_string (type);
      if (type_val == META_GRADIENT_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Did not understand value \"%s\" for type of gradient"),
                     type);
          return;
        }

      alpha_spec = NULL;
      if (alpha && !parse_alpha (alpha, &alpha_spec, context, error))
        return;
      
      g_assert (info->op == NULL);
      info->op = meta_draw_op_new (META_DRAW_GRADIENT);

      info->op->data.gradient.x = meta_draw_spec_new (info->theme, x, NULL);
      info->op->data.gradient.y = meta_draw_spec_new (info->theme, y, NULL);
      info->op->data.gradient.width = meta_draw_spec_new (info->theme, 
                                                        width, NULL);
      info->op->data.gradient.height = meta_draw_spec_new (info->theme,
                                                         height, NULL);

      info->op->data.gradient.gradient_spec = meta_gradient_spec_new (type_val);

      info->op->data.gradient.alpha_spec = alpha_spec;
      
      push_state (info, STATE_GRADIENT);

      /* op gets appended on close tag */
    }
  else if (ELEMENT_IS ("image"))
    {
      MetaDrawOp *op;
      const char *filename;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      const char *colorize;
      const char *fill_type;
      MetaAlphaGradientSpec *alpha_spec;
      GdkPixbuf *pixbuf;
      MetaColorSpec *colorize_spec = NULL;
      MetaImageFillType fill_type_val;
      int h, w, c;
      int pixbuf_width, pixbuf_height, pixbuf_n_channels, pixbuf_rowstride;
      guchar *pixbuf_pixels;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "alpha", &alpha, "!filename", &filename,
                              "colorize", &colorize,
                              "fill_type", &fill_type,
                              NULL))
        return;
      
#if 0      
      if (!check_expression (x, TRUE, info->theme, context, error))
        return;

      if (!check_expression (y, TRUE, info->theme, context, error))
        return;

      if (!check_expression (width, TRUE, info->theme, context, error))
        return;
      
      if (!check_expression (height, TRUE, info->theme, context, error))
        return;
#endif
      fill_type_val = META_IMAGE_FILL_SCALE;
      if (fill_type)
        {
          fill_type_val = meta_image_fill_type_from_string (fill_type);
          
          if (((int) fill_type_val) == -1)
            {
              set_error (error, context, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_PARSE,
                         _("Did not understand fill type \"%s\" for <%s> element"),
                         fill_type, element_name);
            }
        }
      
      /* Check last so we don't have to free it when other
       * stuff fails.
       *
       * If it's a theme image, ask for it at 64px, which is
       * the largest possible. We scale it anyway.
       */
      pixbuf = meta_theme_load_image (info->theme, filename, 64, error);

      if (pixbuf == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      if (colorize)
        {
          colorize_spec = parse_color (info->theme, colorize, error);
          
          if (colorize_spec == NULL)
            {
              add_context_to_error (error, context);
              g_object_unref (G_OBJECT (pixbuf));
              return;
            }
        }

      alpha_spec = NULL;
      if (alpha && !parse_alpha (alpha, &alpha_spec, context, error))
        {
          g_object_unref (G_OBJECT (pixbuf));
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_IMAGE);

      op->data.image.pixbuf = pixbuf;
      op->data.image.colorize_spec = colorize_spec;

      op->data.image.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.image.y = meta_draw_spec_new (info->theme, y, NULL);
      op->data.image.width = meta_draw_spec_new (info->theme, width, NULL);
      op->data.image.height = meta_draw_spec_new (info->theme, height, NULL);

      op->data.image.alpha_spec = alpha_spec;
      op->data.image.fill_type = fill_type_val;
      
      /* Check for vertical & horizontal stripes */
      pixbuf_n_channels = gdk_pixbuf_get_n_channels(pixbuf);
      pixbuf_width = gdk_pixbuf_get_width(pixbuf);
      pixbuf_height = gdk_pixbuf_get_height(pixbuf);
      pixbuf_rowstride = gdk_pixbuf_get_rowstride(pixbuf);
      pixbuf_pixels = gdk_pixbuf_get_pixels(pixbuf);

      /* Check for horizontal stripes */
      for (h = 0; h < pixbuf_height; h++)
        {
          for (w = 1; w < pixbuf_width; w++)
            {
              for (c = 0; c < pixbuf_n_channels; c++)
                {
                  if (pixbuf_pixels[(h * pixbuf_rowstride) + c] !=
                      pixbuf_pixels[(h * pixbuf_rowstride) + w + c])
                    break;
                }
              if (c < pixbuf_n_channels)
                break;
            }
          if (w < pixbuf_width)
            break;
        }

      if (h >= pixbuf_height)
        {
          op->data.image.horizontal_stripes = TRUE; 
        }
      else
        {
          op->data.image.horizontal_stripes = FALSE; 
        }

      /* Check for vertical stripes */
      for (w = 0; w < pixbuf_width; w++)
        {
          for (h = 1; h < pixbuf_height; h++)
            {
              for (c = 0; c < pixbuf_n_channels; c++)
                {
                  if (pixbuf_pixels[w + c] !=
                      pixbuf_pixels[(h * pixbuf_rowstride) + w + c])
                    break;
                }
              if (c < pixbuf_n_channels)
                break;
            }
          if (h < pixbuf_height)
            break;
        }

      if (w >= pixbuf_width)
        {
          op->data.image.vertical_stripes = TRUE; 
        }
      else
        {
          op->data.image.vertical_stripes = FALSE; 
        }
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_IMAGE);
    }
  else if (ELEMENT_IS ("gtk_arrow"))
    {
      MetaDrawOp *op;
      const char *state;
      const char *shadow;
      const char *arrow;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      gboolean filled_val;
      GtkStateType state_val;
      GtkShadowType shadow_val;
      GtkArrowType arrow_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!state", &state,
                              "!shadow", &shadow,
                              "!arrow", &arrow,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "filled", &filled,
                              NULL))
        return;

#if 0
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
#endif
      filled_val = TRUE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;

      state_val = meta_gtk_state_from_string (state);
      if (((int) state_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }

      shadow_val = meta_gtk_shadow_from_string (shadow);
      if (((int) shadow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand shadow \"%s\" for <%s> element"),
                     shadow, element_name);
          return;
        }

      arrow_val = meta_gtk_arrow_from_string (arrow);
      if (((int) arrow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand arrow \"%s\" for <%s> element"),
                     arrow, element_name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_GTK_ARROW);

      op->data.gtk_arrow.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.gtk_arrow.y = meta_draw_spec_new (info->theme, y, NULL);
      op->data.gtk_arrow.width = meta_draw_spec_new (info->theme, width, NULL);
      op->data.gtk_arrow.height = meta_draw_spec_new (info->theme, 
                                                      height, NULL);

      op->data.gtk_arrow.filled = filled_val;
      op->data.gtk_arrow.state = state_val;
      op->data.gtk_arrow.shadow = shadow_val;
      op->data.gtk_arrow.arrow = arrow_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_ARROW);
    }
  else if (ELEMENT_IS ("gtk_box"))
    {
      MetaDrawOp *op;
      const char *state;
      const char *shadow;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      GtkStateType state_val;
      GtkShadowType shadow_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!state", &state,
                              "!shadow", &shadow,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              NULL))
        return;

#if 0
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
#endif
      state_val = meta_gtk_state_from_string (state);
      if (((int) state_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }

      shadow_val = meta_gtk_shadow_from_string (shadow);
      if (((int) shadow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand shadow \"%s\" for <%s> element"),
                     shadow, element_name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_GTK_BOX);

      op->data.gtk_box.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.gtk_box.y = meta_draw_spec_new (info->theme, y, NULL);
      op->data.gtk_box.width = meta_draw_spec_new (info->theme, width, NULL);
      op->data.gtk_box.height = meta_draw_spec_new (info->theme, height, NULL);

      op->data.gtk_box.state = state_val;
      op->data.gtk_box.shadow = shadow_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_BOX);
    }
  else if (ELEMENT_IS ("gtk_vline"))
    {
      MetaDrawOp *op;
      const char *state;
      const char *x;
      const char *y1;
      const char *y2;
      GtkStateType state_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!state", &state,
                              "!x", &x, "!y1", &y1, "!y2", &y2,
                              NULL))
        return;

#if 0
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y1, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y2, FALSE, info->theme, context, error))
        return;
#endif

      state_val = meta_gtk_state_from_string (state);
      if (((int) state_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_GTK_VLINE);

      op->data.gtk_vline.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.gtk_vline.y1 = meta_draw_spec_new (info->theme, y1, NULL);
      op->data.gtk_vline.y2 = meta_draw_spec_new (info->theme, y2, NULL);

      op->data.gtk_vline.state = state_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_VLINE);
    }
  else if (ELEMENT_IS ("icon"))
    {
      MetaDrawOp *op;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      const char *fill_type;
      MetaAlphaGradientSpec *alpha_spec;
      MetaImageFillType fill_type_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!x", &x, "!y", &y,
                              "!width", &width, "!height", &height,
                              "alpha", &alpha,
                              "fill_type", &fill_type,
                              NULL))
        return;
      
#if 0      
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
#endif
      fill_type_val = META_IMAGE_FILL_SCALE;
      if (fill_type)
        {
          fill_type_val = meta_image_fill_type_from_string (fill_type);

          if (((int) fill_type_val) == -1)
            {
              set_error (error, context, G_MARKUP_ERROR,
                         G_MARKUP_ERROR_PARSE,
                         _("Did not understand fill type \"%s\" for <%s> element"),
                         fill_type, element_name);
            }
        }
      
      alpha_spec = NULL;
      if (alpha && !parse_alpha (alpha, &alpha_spec, context, error))
        return;
      
      op = meta_draw_op_new (META_DRAW_ICON);
      
      op->data.icon.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.icon.y = meta_draw_spec_new (info->theme, y, NULL);
      op->data.icon.width = meta_draw_spec_new (info->theme, width, NULL);
      op->data.icon.height = meta_draw_spec_new (info->theme, height, NULL);

      op->data.icon.alpha_spec = alpha_spec;
      op->data.icon.fill_type = fill_type_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_ICON);
    }
  else if (ELEMENT_IS ("title"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *ellipsize_width;
      MetaColorSpec *color_spec;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!color", &color,
                              "!x", &x, "!y", &y,
                              "ellipsize_width", &ellipsize_width,
                              NULL))
        return;

#if 0
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (ellipsize_width, FALSE, info->theme, context, error))
        return;
#endif

      if (ellipsize_width && peek_required_version (info) < 3001)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     ATTRIBUTE_NOT_FOUND, "ellipsize_width", element_name);
          return;
        }

      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = parse_color (info->theme, color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_TITLE);

      op->data.title.color_spec = color_spec;

      op->data.title.x = meta_draw_spec_new (info->theme, x, NULL);
      op->data.title.y = meta_draw_spec_new (info->theme, y, NULL);
      if (ellipsize_width)
        op->data.title.ellipsize_width = meta_draw_spec_new (info->theme, ellipsize_width, NULL);

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TITLE);
    }
  else if (ELEMENT_IS ("include"))
    {
      MetaDrawOp *op;
      const char *name;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      MetaDrawOpList *op_list;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "!name", &name,
                              NULL))
        return;

      /* x/y/width/height default to 0,0,width,height - should
       * probably do this for all the draw ops
       */
#if 0      
      if (x && !check_expression (x, FALSE, info->theme, context, error))
        return;

      if (y && !check_expression (y, FALSE, info->theme, context, error))
        return;

      if (width && !check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (height && !check_expression (height, FALSE, info->theme, context, error))
        return;
#endif

      op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                name);
      if (op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("No <draw_ops> called \"%s\" has been defined"),
                     name);
          return;
        }

      g_assert (info->op_list);
      
      if (op_list == info->op_list ||
          meta_draw_op_list_contains (op_list, info->op_list))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Including draw_ops \"%s\" here would create a circular reference"),
                     name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_OP_LIST);

      meta_draw_op_list_ref (op_list);
      op->data.op_list.op_list = op_list;      

      op->data.op_list.x = meta_draw_spec_new (info->theme, x ? x : "0", NULL);
      op->data.op_list.y = meta_draw_spec_new (info->theme, y ? y : "0", NULL);
      op->data.op_list.width = meta_draw_spec_new (info->theme, 
                                                   width ? width : "width",
                                                   NULL);
      op->data.op_list.height = meta_draw_spec_new (info->theme,
                                                    height ? height : "height",
                                                    NULL);

      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_INCLUDE);
    }
  else if (ELEMENT_IS ("tile"))
    {
      MetaDrawOp *op;
      const char *name;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *tile_xoffset;
      const char *tile_yoffset;
      const char *tile_width;
      const char *tile_height;
      MetaDrawOpList *op_list;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "!name", &name,
                              "tile_xoffset", &tile_xoffset,
                              "tile_yoffset", &tile_yoffset,
                              "!tile_width", &tile_width,
                              "!tile_height", &tile_height,
                              NULL))
        return;

      /* These default to 0 */
#if 0
      if (tile_xoffset && !check_expression (tile_xoffset, FALSE, info->theme, context, error))
        return;

      if (tile_yoffset && !check_expression (tile_yoffset, FALSE, info->theme, context, error))
        return;
      
      /* x/y/width/height default to 0,0,width,height - should
       * probably do this for all the draw ops
       */
      if (x && !check_expression (x, FALSE, info->theme, context, error))
        return;

      if (y && !check_expression (y, FALSE, info->theme, context, error))
        return;

      if (width && !check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (height && !check_expression (height, FALSE, info->theme, context, error))
        return;

      if (!check_expression (tile_width, FALSE, info->theme, context, error))
        return;

      if (!check_expression (tile_height, FALSE, info->theme, context, error))
        return;
#endif 
      op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                name);
      if (op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("No <draw_ops> called \"%s\" has been defined"),
                     name);
          return;
        }

      g_assert (info->op_list);
      
      if (op_list == info->op_list ||
          meta_draw_op_list_contains (op_list, info->op_list))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Including draw_ops \"%s\" here would create a circular reference"),
                     name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_TILE);

      meta_draw_op_list_ref (op_list);

      op->data.tile.x = meta_draw_spec_new (info->theme, x ? x : "0", NULL);
      op->data.tile.y = meta_draw_spec_new (info->theme, y ? y : "0", NULL);
      op->data.tile.width = meta_draw_spec_new (info->theme,
                                                width ? width : "width",
                                                NULL);
      op->data.tile.height = meta_draw_spec_new (info->theme,
                                                 height ? height : "height",
                                                 NULL);
      op->data.tile.tile_xoffset = meta_draw_spec_new (info->theme,
                                                       tile_xoffset ? tile_xoffset : "0",
                                                       NULL);
      op->data.tile.tile_yoffset = meta_draw_spec_new (info->theme,
                                                       tile_yoffset ? tile_yoffset : "0",
                                                       NULL);
      op->data.tile.tile_width = meta_draw_spec_new (info->theme, tile_width, NULL);
      op->data.tile.tile_height = meta_draw_spec_new (info->theme, tile_height, NULL);

      op->data.tile.op_list = op_list;      
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TILE);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "draw_ops");
    }
}

static void
parse_gradient_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_GRADIENT);

  if (ELEMENT_IS ("color"))
    {
      const char *value = NULL;
      MetaColorSpec *color_spec;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!value", &value,
                              NULL))
        return;

      color_spec = parse_color (info->theme, value, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      g_assert (info->op);
      g_assert (info->op->type == META_DRAW_GRADIENT);
      g_assert (info->op->data.gradient.gradient_spec != NULL);
      info->op->data.gradient.gradient_spec->color_specs =
        g_slist_append (info->op->data.gradient.gradient_spec->color_specs,
                        color_spec);
      
      push_state (info, STATE_COLOR);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "gradient");
    }
}

static void
parse_style_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     ParseInfo            *info,
                     GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_STYLE);

  g_assert (info->style);
  
  if (ELEMENT_IS ("piece"))
    {
      const char *position = NULL;
      const char *draw_ops = NULL;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!position", &position,
                              "draw_ops", &draw_ops,
                              NULL))
        return;

      info->piece = meta_frame_piece_from_string (position);
      if (info->piece == META_FRAME_PIECE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown position \"%s\" for frame piece"),
                     position);
          return;
        }
      
      if (info->style->pieces[info->piece] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Frame style already has a piece at position %s"),
                     position);
          return;
        }

      g_assert (info->op_list == NULL);
      
      if (draw_ops)
        {
          MetaDrawOpList *op_list;

          op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                    draw_ops);

          if (op_list == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No <draw_ops> with the name \"%s\" has been defined"),
                         draw_ops);
              return;
            }

          meta_draw_op_list_ref (op_list);
          info->op_list = op_list;
        }
      
      push_state (info, STATE_PIECE);
    }
  else if (ELEMENT_IS ("button"))
    {
      const char *function = NULL;
      const char *state = NULL;
      const char *draw_ops = NULL;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!function", &function,
                              "!state", &state,
                              "draw_ops", &draw_ops,
                              NULL))
        return;

      info->button_type = meta_button_type_from_string (function, info->theme);
      if (info->button_type == META_BUTTON_TYPE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown function \"%s\" for button"),
                     function);
          return;
        }

      if (meta_theme_earliest_version_with_button (info->button_type) >
          info->theme->format_version)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Button function \"%s\" does not exist in this version (%d, need %d)"),
                     function,
                     info->theme->format_version,
                     meta_theme_earliest_version_with_button (info->button_type)
                     );
          return;
        }

      info->button_state = meta_button_state_from_string (state);
      if (info->button_state == META_BUTTON_STATE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown state \"%s\" for button"),
                     state);
          return;
        }
      
      if (info->style->buttons[info->button_type][info->button_state] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Frame style already has a button for function %s state %s"),
                     function, state);
          return;
        }

      g_assert (info->op_list == NULL);
      
      if (draw_ops)
        {
          MetaDrawOpList *op_list;

          op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                    draw_ops);

          if (op_list == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No <draw_ops> with the name \"%s\" has been defined"),
                         draw_ops);
              return;
            }

          meta_draw_op_list_ref (op_list);
          info->op_list = op_list;
        }
      
      push_state (info, STATE_BUTTON);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_style");
    }
}

static void
parse_style_set_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         const gchar         **attribute_names,
                         const gchar         **attribute_values,
                         ParseInfo            *info,
                         GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_STYLE_SET);

  if (ELEMENT_IS ("frame"))
    {
      const char *focus = NULL;
      const char *state = NULL;
      const char *resize = NULL;
      const char *style = NULL;
      MetaFrameFocus frame_focus;
      MetaFrameState frame_state;
      MetaFrameResize frame_resize;
      MetaFrameStyle *frame_style;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "!focus", &focus,
                              "!state", &state,
                              "resize", &resize,
                              "!style", &style,
                              NULL))
        return;

      frame_focus = meta_frame_focus_from_string (focus);
      if (frame_focus == META_FRAME_FOCUS_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("\"%s\" is not a valid value for focus attribute"),
                     focus);
          return;
        }
      
      frame_state = meta_frame_state_from_string (state);
      if (frame_state == META_FRAME_STATE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("\"%s\" is not a valid value for state attribute"),
                     focus);
          return;
        }

      frame_style = meta_theme_lookup_style (info->theme, style);

      if (frame_style == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("A style called \"%s\" has not been defined"),
                     style);
          return;
        }

      switch (frame_state)
        {
        case META_FRAME_STATE_NORMAL:
          if (resize == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         ATTRIBUTE_NOT_FOUND,
                         "resize", element_name);
              return;
            }

          
          frame_resize = meta_frame_resize_from_string (resize);
          if (frame_resize == META_FRAME_RESIZE_LAST)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("\"%s\" is not a valid value for resize attribute"),
                         focus);
              return;
            }
          
          break;

        case META_FRAME_STATE_SHADED:
          if (META_THEME_ALLOWS (info->theme, META_THEME_UNRESIZABLE_SHADED_STYLES))
            {
              if (resize == NULL)
                /* In state="normal" we would complain here. But instead we accept
                 * not having a resize attribute and default to resize="both", since
                 * that most closely mimics what we did in v1, and thus people can
                 * upgrade a theme to v2 without as much hassle.
                 */
                frame_resize = META_FRAME_RESIZE_BOTH;
              else
                {
                  frame_resize = meta_frame_resize_from_string (resize);
                  if (frame_resize == META_FRAME_RESIZE_LAST)
                    {
                      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                                 _("\"%s\" is not a valid value for resize attribute"),
                                 focus);
                      return;
                    }
                }
            }
          else /* v1 theme */
            {
              if (resize != NULL)
                {
                  set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                       _("Should not have \"resize\" attribute on <%s> element for maximized/shaded states"),
                      element_name);
                  return;
                }

              /* resize="both" is equivalent to the old behaviour */
              frame_resize = META_FRAME_RESIZE_BOTH;
            }
          break;
          
        default:
          if (resize != NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Should not have \"resize\" attribute on <%s> element for maximized states"),
                         element_name);
              return;
            }

          frame_resize = META_FRAME_RESIZE_LAST;
        }
      
      switch (frame_state)
        {
        case META_FRAME_STATE_NORMAL:
          if (info->style_set->normal_styles[frame_resize][frame_focus])
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Style has already been specified for state %s resize %s focus %s"),
                         state, resize, focus);
              return;
            }
          meta_frame_style_ref (frame_style);
          info->style_set->normal_styles[frame_resize][frame_focus] = frame_style;
          break;
        case META_FRAME_STATE_MAXIMIZED:
          if (info->style_set->maximized_styles[frame_focus])
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Style has already been specified for state %s focus %s"),
                         state, focus);
              return;
            }
          meta_frame_style_ref (frame_style);
          info->style_set->maximized_styles[frame_focus] = frame_style;
          break;
        case META_FRAME_STATE_SHADED:
          if (info->style_set->shaded_styles[frame_resize][frame_focus])
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Style has already been specified for state %s resize %s focus %s"),
                         state, resize, focus);
              return;
            }
          meta_frame_style_ref (frame_style);
          info->style_set->shaded_styles[frame_resize][frame_focus] = frame_style;
          break;
        case META_FRAME_STATE_MAXIMIZED_AND_SHADED:
          if (info->style_set->maximized_and_shaded_styles[frame_focus])
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Style has already been specified for state %s focus %s"),
                         state, focus);
              return;
            }
          meta_frame_style_ref (frame_style);
          info->style_set->maximized_and_shaded_styles[frame_focus] = frame_style;
          break;
        case META_FRAME_STATE_LAST:
          g_assert_not_reached ();
          break;
        }

      push_state (info, STATE_FRAME);      
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_style_set");
    }
}

static void
parse_piece_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     ParseInfo            *info,
                     GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_PIECE);

  if (ELEMENT_IS ("draw_ops"))
    {
      if (info->op_list)
        {
          set_error (error, context,
                     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <piece> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }
            
      if (!check_no_attributes (context, element_name, attribute_names, attribute_values,
                                error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "piece");
    }
}

static void
parse_button_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      ParseInfo            *info,
                      GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_BUTTON);
  
  if (ELEMENT_IS ("draw_ops"))
    {
      if (info->op_list)
        {
          set_error (error, context,
                     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <button> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }
            
      if (!check_no_attributes (context, element_name, attribute_names, attribute_values,
                                error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "button");
    }
}

static void
parse_menu_icon_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         const gchar         **attribute_names,
                         const gchar         **attribute_values,
                         ParseInfo            *info,
                         GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_MENU_ICON);

  if (ELEMENT_IS ("draw_ops"))
    {
      if (info->op_list)
        {
          set_error (error, context,
                     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <menu_icon> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }
            
      if (!check_no_attributes (context, element_name, attribute_names, attribute_values,
                                error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "menu_icon");
    }
}

static const char *
find_version (const char **attribute_names,
              const char **attribute_values)
{
  int i;

  for (i = 0; attribute_names[i]; i++)
    {
      if (strcmp (attribute_names[i], "version") == 0)
        return attribute_values[i];
    }

  return NULL;
}

/* Returns whether the version element was successfully parsed.
 * If successfully parsed, then two additional items are returned:
 *
 *  satisfied:        whether this version of Mutter meets the version check
 *  minimum_required: minimum version of theme format required by version check
 */
static gboolean
check_version (GMarkupParseContext *context,
               const char          *version_str,
               gboolean            *satisfied,
               guint               *minimum_required,
               GError             **error)
{
  static GRegex *version_regex;
  GMatchInfo *info;
  char *comparison_str, *major_str, *minor_str;
  guint version;

  *minimum_required = 0;

  if (!version_regex)
    version_regex = g_regex_new ("^\\s*([<>]=?)\\s*(\\d+)(\\.\\d+)?\\s*$", 0, 0, NULL);

  if (!g_regex_match (version_regex, version_str, 0, &info))
    {
      g_match_info_free (info);
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Bad version specification '%s'"), version_str);
      return FALSE;
    }

  comparison_str = g_match_info_fetch (info, 1);
  major_str = g_match_info_fetch (info, 2);
  minor_str = g_match_info_fetch (info, 3);

  version = 1000 * atoi (major_str);
  /* might get NULL, see: https://bugzilla.gnome.org/review?bug=588217 */
  if (minor_str && minor_str[0])
    version += atoi (minor_str + 1);

  if (comparison_str[0] == '<')
    {
      if (comparison_str[1] == '=')
        *satisfied = THEME_VERSION <= version;
      else
        {
          *satisfied = THEME_VERSION < version;
        }
    }
  else
    {
      if (comparison_str[1] == '=')
        {
          *satisfied = THEME_VERSION >= version;
          *minimum_required = version;
        }
      else
        {
          *satisfied = THEME_VERSION > version;
          *minimum_required = version + 1;
        }
    }

  g_free (comparison_str);
  g_free (major_str);
  g_free (minor_str);
  g_match_info_free (info);

  return TRUE;
}

static void
start_element_handler (GMarkupParseContext *context,
                       const gchar         *element_name,
                       const gchar        **attribute_names,
                       const gchar        **attribute_values,
                       gpointer             user_data,
                       GError             **error)
{
  ParseInfo *info = user_data;
  const char *version;
  guint required_version = 0;

  if (info->skip_level > 0)
    {
      info->skip_level++;
      return;
    }

  required_version = peek_required_version (info);

  version = find_version (attribute_names, attribute_values);
  if (version != NULL)
    {
      gboolean satisfied;
      guint element_required;

      if (required_version < 3000)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("\"version\" attribute cannot be used in metacity-theme-1.xml or metacity-theme-2.xml"));
          return;
        }

      if (!check_version (context, version, &satisfied, &element_required, error))
        return;

      /* Two different ways of handling an unsatisfied version check:
       * for the toplevel element of a file, we throw an error back so
       * that the controlling code can go ahead and look for an
       * alternate metacity-theme-1.xml or metacity-theme-2.xml; for
       * other elements we just silently skip the element and children.
       */
      if (peek_state (info) == STATE_START)
        {
          if (satisfied)
            {
              if (element_required > info->format_version)
                info->format_version = element_required;
            }
          else
            {
              set_error (error, context, THEME_PARSE_ERROR, THEME_PARSE_ERROR_TOO_OLD,
                         _("Theme requires version %s but latest supported theme version is %d.%d"),
                         version, THEME_VERSION, THEME_MINOR_VERSION);
              return;
            }
        }
      else if (!satisfied)
        {
          info->skip_level = 1;
          return;
        }

      if (element_required > required_version)
        required_version = element_required;
    }

  push_required_version (info, required_version);

  switch (peek_state (info))
    {
    case STATE_START:
      if (strcmp (element_name, "metacity_theme") == 0)
        {
          info->theme = meta_theme_new ();
          info->theme->name = g_strdup (info->theme_name);
          info->theme->filename = g_strdup (info->theme_file);
          info->theme->dirname = g_strdup (info->theme_dir);
          info->theme->format_version = info->format_version;
          
          push_state (info, STATE_THEME);
        }
      else
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Outermost element in theme must be <metacity_theme> not <%s>"),
                   element_name);
      break;

    case STATE_THEME:
      parse_toplevel_element (context, element_name,
                              attribute_names, attribute_values,
                              info, error);
      break;
    case STATE_INFO:
      parse_info_element (context, element_name,
                          attribute_names, attribute_values,
                          info, error);
      break;
    case STATE_NAME:
    case STATE_AUTHOR:
    case STATE_COPYRIGHT:
    case STATE_DATE:
    case STATE_DESCRIPTION:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a name/author/date/description element"),
                 element_name);
      break;
    case STATE_CONSTANT:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <constant> element"),
                 element_name);
      break;
    case STATE_FRAME_GEOMETRY:
      parse_geometry_element (context, element_name,
                              attribute_names, attribute_values,
                              info, error);
      break;
    case STATE_DISTANCE:
    case STATE_BORDER:
    case STATE_ASPECT_RATIO:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a distance/border/aspect_ratio element"),
                 element_name);
      break;
    case STATE_DRAW_OPS:
      parse_draw_op_element (context, element_name,
                             attribute_names, attribute_values,
                             info, error);
      break;
    case STATE_LINE:
    case STATE_RECTANGLE:
    case STATE_ARC:
    case STATE_CLIP:
    case STATE_TINT:
    case STATE_IMAGE:
    case STATE_GTK_ARROW:
    case STATE_GTK_BOX:
    case STATE_GTK_VLINE:
    case STATE_ICON:
    case STATE_TITLE:
    case STATE_INCLUDE:
    case STATE_TILE:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a draw operation element"),
                 element_name);
      break;
    case STATE_GRADIENT:
      parse_gradient_element (context, element_name,
                              attribute_names, attribute_values,
                              info, error);
      break;
    case STATE_COLOR:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <%s> element"),
                 element_name, "color");
      break;
    case STATE_FRAME_STYLE:
      parse_style_element (context, element_name,
                           attribute_names, attribute_values,
                           info, error);
      break;
    case STATE_PIECE:
      parse_piece_element (context, element_name,
                           attribute_names, attribute_values,
                           info, error);
      break;
    case STATE_BUTTON:
      parse_button_element (context, element_name,
                            attribute_names, attribute_values,
                            info, error);
      break;
    case STATE_MENU_ICON:
      parse_menu_icon_element (context, element_name,
                               attribute_names, attribute_values,
                               info, error);
      break;
    case STATE_FRAME_STYLE_SET:
      parse_style_set_element (context, element_name,
                               attribute_names, attribute_values,
                               info, error);
      break;
    case STATE_FRAME:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <%s> element"),
                 element_name, "frame");
      break;
    case STATE_WINDOW:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <%s> element"),
                 element_name, "window");
      break;
    case STATE_FALLBACK:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <%s> element"),
                 element_name, "fallback");
      break;
    }
}

static void
end_element_handler (GMarkupParseContext *context,
                     const gchar         *element_name,
                     gpointer             user_data,
                     GError             **error)
{
  ParseInfo *info = user_data;

  if (info->skip_level > 0)
    {
      info->skip_level--;
      return;
    }

  switch (peek_state (info))
    {
    case STATE_START:
      break;
    case STATE_THEME:
      g_assert (info->theme);

      if (!meta_theme_validate (info->theme, error))
        {
          add_context_to_error (error, context);
          meta_theme_free (info->theme);
          info->theme = NULL;
        }
      
      pop_state (info);
      g_assert (peek_state (info) == STATE_START);
      break;
    case STATE_INFO:
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_NAME:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_AUTHOR:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_COPYRIGHT:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_DATE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_DESCRIPTION:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_CONSTANT:
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_FRAME_GEOMETRY:
      g_assert (info->layout);

      if (!meta_frame_layout_validate (info->layout,
                                       error))
        {
          add_context_to_error (error, context);
        }

      /* layout will already be stored in the theme under
       * its name
       */
      meta_frame_layout_unref (info->layout);
      info->layout = NULL;
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_DISTANCE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_GEOMETRY);
      break;
    case STATE_BORDER:
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_GEOMETRY);
      break;
    case STATE_ASPECT_RATIO:
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_GEOMETRY);
      break;
    case STATE_DRAW_OPS:
      {
        g_assert (info->op_list);
        
        if (!meta_draw_op_list_validate (info->op_list,
                                         error))
          {
            add_context_to_error (error, context);
            meta_draw_op_list_unref (info->op_list);
            info->op_list = NULL;
          }

        pop_state (info);

        switch (peek_state (info))
          {
          case STATE_BUTTON:
          case STATE_PIECE:
          case STATE_MENU_ICON:
            /* Leave info->op_list to be picked up
             * when these elements are closed
             */
            g_assert (info->op_list);
            break;
          case STATE_THEME:
            g_assert (info->op_list);
            meta_draw_op_list_unref (info->op_list);
            info->op_list = NULL;
            break;
          default:
            /* Op list can't occur in other contexts */
            g_assert_not_reached ();
            break;
          }
      }
      break;
    case STATE_LINE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_RECTANGLE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_ARC:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_CLIP:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_TINT:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_GRADIENT:
      g_assert (info->op);
      g_assert (info->op->type == META_DRAW_GRADIENT);
      if (!meta_gradient_spec_validate (info->op->data.gradient.gradient_spec,
                                        error))
        {
          add_context_to_error (error, context);
          meta_draw_op_free (info->op);
          info->op = NULL;
        }
      else
        {
          g_assert (info->op_list);
          meta_draw_op_list_append (info->op_list, info->op);
          info->op = NULL;
        }
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_IMAGE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_GTK_ARROW:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_GTK_BOX:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_GTK_VLINE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_ICON:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_TITLE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_INCLUDE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_TILE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_COLOR:
      pop_state (info);
      g_assert (peek_state (info) == STATE_GRADIENT);
      break;
    case STATE_FRAME_STYLE:
      g_assert (info->style);

      if (!meta_frame_style_validate (info->style,
                                      info->theme->format_version,
                                      error))
        {
          add_context_to_error (error, context);
        }

      /* Frame style is in the theme hash table and a ref
       * is held there
       */
      meta_frame_style_unref (info->style);
      info->style = NULL;
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_PIECE:
      g_assert (info->style);
      if (info->op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No draw_ops provided for frame piece"));
        }
      else
        {
          info->style->pieces[info->piece] = info->op_list;
          info->op_list = NULL;
        }
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_STYLE);
      break;
    case STATE_BUTTON:
      g_assert (info->style);
      if (info->op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No draw_ops provided for button"));
        }
      else
        {
          info->style->buttons[info->button_type][info->button_state] =
            info->op_list;
          info->op_list = NULL;
        }
      pop_state (info);
      break;
    case STATE_MENU_ICON:
      g_assert (info->theme);
      if (info->op_list != NULL)
        {
          meta_draw_op_list_unref (info->op_list);
          info->op_list = NULL;
        }
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_FRAME_STYLE_SET:
      g_assert (info->style_set);

      if (!meta_frame_style_set_validate (info->style_set,
                                          error))
        {
          add_context_to_error (error, context);
        }

      /* Style set is in the theme hash table and a reference
       * is held there.
       */
      meta_frame_style_set_unref (info->style_set);
      info->style_set = NULL;
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_FRAME:
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_STYLE_SET);
      break;
    case STATE_WINDOW:
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_FALLBACK:
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    }

  pop_required_version (info);
}

#define NO_TEXT(element_name) set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, _("No text is allowed inside element <%s>"), element_name)

static gboolean
all_whitespace (const char *text,
                int         text_len)
{
  const char *p;
  const char *end;
  
  p = text;
  end = text + text_len;
  
  while (p != end)
    {
      if (!g_ascii_isspace (*p))
        return FALSE;

      p = g_utf8_next_char (p);
    }

  return TRUE;
}

static void
text_handler (GMarkupParseContext *context,
              const gchar         *text,
              gsize                text_len,
              gpointer             user_data,
              GError             **error)
{
  ParseInfo *info = user_data;

  if (info->skip_level > 0)
    return;

  if (all_whitespace (text, text_len))
    return;
  
  /* FIXME http://bugzilla.gnome.org/show_bug.cgi?id=70448 would
   * allow a nice cleanup here.
   */

  switch (peek_state (info))
    {
    case STATE_START:
      g_assert_not_reached (); /* gmarkup shouldn't do this */
      break;
    case STATE_THEME:
      NO_TEXT ("metacity_theme");
      break;
    case STATE_INFO:
      NO_TEXT ("info");
      break;
    case STATE_NAME:
      if (info->theme->readable_name != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<%s> specified twice for this theme"),
                     "name");
          return;
        }

      info->theme->readable_name = g_strndup (text, text_len);
      break;
    case STATE_AUTHOR:
      if (info->theme->author != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<%s> specified twice for this theme"),
                     "author");
          return;
        }

      info->theme->author = g_strndup (text, text_len);
      break;
    case STATE_COPYRIGHT:
      if (info->theme->copyright != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<%s> specified twice for this theme"),
                     "copyright");
          return;
        }

      info->theme->copyright = g_strndup (text, text_len);
      break;
    case STATE_DATE:
      if (info->theme->date != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<%s> specified twice for this theme"),
                     "date");
          return;
        }

      info->theme->date = g_strndup (text, text_len);
      break;
    case STATE_DESCRIPTION:
      if (info->theme->description != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<%s> specified twice for this theme"),
                     "description");
          return;
        }

      info->theme->description = g_strndup (text, text_len);
      break;
    case STATE_CONSTANT:
      NO_TEXT ("constant");
      break;
    case STATE_FRAME_GEOMETRY:
      NO_TEXT ("frame_geometry");
      break;
    case STATE_DISTANCE:
      NO_TEXT ("distance");
      break;
    case STATE_BORDER:
      NO_TEXT ("border");
      break;
    case STATE_ASPECT_RATIO:
      NO_TEXT ("aspect_ratio");
      break;
    case STATE_DRAW_OPS:
      NO_TEXT ("draw_ops");
      break;
    case STATE_LINE:
      NO_TEXT ("line");
      break;
    case STATE_RECTANGLE:
      NO_TEXT ("rectangle");
      break;
    case STATE_ARC:
      NO_TEXT ("arc");
      break;
    case STATE_CLIP:
      NO_TEXT ("clip");
      break;
    case STATE_TINT:
      NO_TEXT ("tint");
      break;
    case STATE_GRADIENT:
      NO_TEXT ("gradient");
      break;
    case STATE_IMAGE:
      NO_TEXT ("image");
      break;
    case STATE_GTK_ARROW:
      NO_TEXT ("gtk_arrow");
      break;
    case STATE_GTK_BOX:
      NO_TEXT ("gtk_box");
      break;
    case STATE_GTK_VLINE:
      NO_TEXT ("gtk_vline");
      break;
    case STATE_ICON:
      NO_TEXT ("icon");
      break;
    case STATE_TITLE:
      NO_TEXT ("title");
      break;
    case STATE_INCLUDE:
      NO_TEXT ("include");
      break;
    case STATE_TILE:
      NO_TEXT ("tile");
      break;
    case STATE_COLOR:
      NO_TEXT ("color");
      break;
    case STATE_FRAME_STYLE:
      NO_TEXT ("frame_style");
      break;
    case STATE_PIECE:
      NO_TEXT ("piece");
      break;
    case STATE_BUTTON:
      NO_TEXT ("button");
      break;
    case STATE_MENU_ICON:
      NO_TEXT ("menu_icon");
      break;
    case STATE_FRAME_STYLE_SET:
      NO_TEXT ("frame_style_set");
      break;
    case STATE_FRAME:
      NO_TEXT ("frame");
      break;
    case STATE_WINDOW:
      NO_TEXT ("window");
      break;
    case STATE_FALLBACK:
      NO_TEXT ("fallback");
      break;
    }
}

/* If the theme is not-corrupt, keep looking for alternate versions
 * in other locations we might be compatible with
 */
static gboolean
theme_error_is_fatal (GError *error)
{
  return !(error->domain == G_FILE_ERROR ||
          (error->domain == THEME_PARSE_ERROR &&
           error->code == THEME_PARSE_ERROR_TOO_OLD));
}

static MetaTheme *
load_theme (const char *theme_dir,
            const char *theme_name,
            guint       major_version,
            GError    **error)
{
  GMarkupParseContext *context;
  ParseInfo info;
  char *text;
  gsize length;
  char *theme_filename;
  char *theme_file;
  MetaTheme *retval;

  g_return_val_if_fail (error && *error == NULL, NULL);

  text = NULL;
  retval = NULL;
  context = NULL;

  theme_filename = g_strdup_printf (METACITY_THEME_FILENAME_FORMAT, major_version);
  theme_file = g_build_filename (theme_dir, theme_filename, NULL);

  if (!g_file_get_contents (theme_file,
                            &text,
                            &length,
                            error))
    goto out;

  meta_topic (META_DEBUG_THEMES, "Parsing theme file %s\n", theme_file);

  parse_info_init (&info);

  info.theme_name = theme_name;
  info.theme_file = theme_file;
  info.theme_dir = theme_dir;

  info.format_version = 1000 * major_version;

  context = g_markup_parse_context_new (&metacity_theme_parser,
                                        0, &info, NULL);

  if (!g_markup_parse_context_parse (context,
                                     text,
                                     length,
                                     error))
    goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;

  retval = info.theme;
  info.theme = NULL;

 out:
  if (*error && !theme_error_is_fatal (*error))
    {
      meta_topic (META_DEBUG_THEMES, "Failed to read theme from file %s: %s\n",
                  theme_file, (*error)->message);
    }

  g_free (theme_filename);
  g_free (theme_file);
  g_free (text);

  if (context)
    {
      g_markup_parse_context_free (context);
      parse_info_free (&info);
    }

  return retval;
}

static gboolean
keep_trying (GError **error)
{
  if (*error && !theme_error_is_fatal (*error))
    {
      g_clear_error (error);
      return TRUE;
    }

  return FALSE;
}

MetaTheme*
meta_theme_load (const char *theme_name,
                 GError    **err)
{
  GError *error = NULL;
  char *theme_dir;
  MetaTheme *retval;
  const gchar* const* xdg_data_dirs;
  int major_version;
  int i;

  retval = NULL;

  if (meta_is_debugging ())
    {
      /* Try in themes in our source tree */
      /* We try all supported major versions from current to oldest */
      for (major_version = THEME_MAJOR_VERSION; (major_version > 0); major_version--)
        {
          theme_dir = g_build_filename ("./themes", theme_name, NULL);
          retval = load_theme (theme_dir, theme_name, major_version, &error);
          g_free (theme_dir);
          if (!keep_trying (&error))
            goto out;
        }
    }
  
  /* We try all supported major versions from current to oldest */
  for (major_version = THEME_MAJOR_VERSION; (major_version > 0); major_version--)
    {
      /* We try first in home dir, XDG_DATA_DIRS, then system dir for themes */

      /* Try home dir for themes */
      theme_dir = g_build_filename (g_get_home_dir (),
                                    ".themes",
                                    theme_name,
                                    THEME_SUBDIR,
                                    NULL);

      retval = load_theme (theme_dir, theme_name, major_version, &error);
      g_free (theme_dir);
      if (!keep_trying (&error))
        goto out;

      /* Try each XDG_DATA_DIRS for theme */
      xdg_data_dirs = g_get_system_data_dirs();
      for(i = 0; xdg_data_dirs[i] != NULL; i++)
        {
          theme_dir = g_build_filename (xdg_data_dirs[i],
                                        "themes",
                                        theme_name,
                                        THEME_SUBDIR,
                                        NULL);

          retval = load_theme (theme_dir, theme_name, major_version, &error);
          g_free (theme_dir);
          if (!keep_trying (&error))
            goto out;
        }

      /* Look for themes in MUTTER_DATADIR */
      theme_dir = g_build_filename (MUTTER_DATADIR,
                                    "themes",
                                    theme_name,
                                    THEME_SUBDIR,
                                    NULL);

      retval = load_theme (theme_dir, theme_name, major_version, &error);
      g_free (theme_dir);
      if (!keep_trying (&error))
        goto out;
    }

 out:
  if (!error && !retval)
    g_set_error (&error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                 _("Failed to find a valid file for theme %s\n"),
                 theme_name);

  if (error)
    {
      g_propagate_error (err, error);
    }

  return retval;
}
