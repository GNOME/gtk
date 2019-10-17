/* gtkbuilderprivate.h
 * Copyright (C) 2006-2007 Async Open Source,
 *                         Johan Dahlin <jdahlin@async.com.br>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_BUILDER_PRIVATE_H__
#define __GTK_BUILDER_PRIVATE_H__

#include "gtkbuilder.h"
#include "gtkbuildable.h"

enum {
  TAG_PROPERTY,
  TAG_MENU,
  TAG_REQUIRES,
  TAG_OBJECT,
  TAG_CHILD,
  TAG_SIGNAL,
  TAG_INTERFACE,
  TAG_TEMPLATE,
};

typedef struct {
  guint tag_type;
} CommonInfo;

typedef struct {
  guint tag_type;
  GType type;
  GObjectClass *oclass;
  gchar *id;
  gchar *constructor;
  GSList *properties;
  GSList *signals;
  GSList *bindings;
  GObject *object;
  CommonInfo *parent;
  gboolean applied_properties;
} ObjectInfo;

typedef struct {
  guint tag_type;
  GSList *packing_properties;
  GObject *object;
  CommonInfo *parent;
  gchar *type;
  gchar *internal_child;
  gboolean added;
} ChildInfo;

typedef struct {
  guint tag_type;
  GParamSpec *pspec;
  GString *text;
  gboolean translatable:1;
  gboolean bound:1;
  gchar *context;
  gint line;
  gint col;
} PropertyInfo;

typedef struct {
  guint tag_type;
  gchar *object_name;
  guint  id;
  GQuark detail;
  gchar *handler;
  GConnectFlags flags;
  gchar *connect_object_name;
} SignalInfo;

typedef struct
{
  GObject *target;
  GParamSpec *target_pspec;
  gchar *source;
  gchar *source_property;
  GBindingFlags flags;
  gint line;
  gint col;
} BindingInfo;

typedef struct {
  guint    tag_type;
  gchar   *library;
  gint     major;
  gint     minor;
} RequiresInfo;

struct _GtkBuildableParseContext {
  const GMarkupParser *internal_callbacks;
  GMarkupParseContext *ctx;

  const GtkBuildableParser *parser;
  gpointer user_data;

  GPtrArray *tag_stack;

  GArray *subparser_stack;
  gpointer held_user_data;
  gboolean awaiting_pop;
};

typedef struct {
  GtkBuildableParser *parser;
  gchar *tagname;
  const gchar *start;
  gpointer data;
  GObject *object;
  GObject *child;
} SubParser;

typedef struct {
  const gchar *last_element;
  GtkBuilder *builder;
  gchar *domain;
  GSList *stack;
  SubParser *subparser;
  GtkBuildableParseContext ctx;
  const gchar *filename;
  GSList *finalizers;
  GSList *custom_finalizers;

  char **requested_objects; /* NULL if all the objects are requested */
  gboolean inside_requested_object;
  gint requested_object_level;
  gint cur_object_level;

  gint object_counter;

  GHashTable *object_ids;
} ParserData;

typedef GType (*GTypeGetFunc) (void);

/* Things only GtkBuilder should use */
GBytes * _gtk_buildable_parser_precompile (const gchar              *text,
                                           gssize                    text_len,
                                           GError                  **error);
gboolean _gtk_buildable_parser_is_precompiled (const gchar          *data,
                                               gssize                data_len);
gboolean _gtk_buildable_parser_replay_precompiled (GtkBuildableParseContext *context,
                                                   const gchar          *data,
                                                   gssize                data_len,
                                                   GError              **error);
void _gtk_builder_parser_parse_buffer (GtkBuilder *builder,
                                       const gchar *filename,
                                       const gchar *buffer,
                                       gssize length,
                                       gchar **requested_objs,
                                       GError **error);
GObject * _gtk_builder_construct (GtkBuilder *builder,
                                  ObjectInfo *info,
				  GError    **error);
void      _gtk_builder_apply_properties (GtkBuilder *builder,
					 ObjectInfo *info,
					 GError **error);
void      _gtk_builder_add_object (GtkBuilder  *builder,
                                   const gchar *id,
                                   GObject     *object);
void      _gtk_builder_add (GtkBuilder *builder,
                            ChildInfo *child_info);
void      _gtk_builder_add_signals (GtkBuilder *builder,
				    GSList     *signals);
void      _gtk_builder_finish (GtkBuilder *builder);
void _free_signal_info (SignalInfo *info,
                        gpointer user_data);

/* Internal API which might be made public at some point */
gboolean _gtk_builder_boolean_from_string (const gchar  *string,
					   gboolean     *value,
					   GError      **error);
gboolean _gtk_builder_enum_from_string (GType         type,
                                        const gchar  *string,
                                        gint         *enum_value,
                                        GError      **error);
gboolean  _gtk_builder_flags_from_string (GType         type,
                                          GFlagsValue  *aliases,
					  const char   *string,
					  guint        *value,
					  GError      **error);
const gchar * _gtk_builder_parser_translate (const gchar *domain,
                                             const gchar *context,
                                             const gchar *text);
gchar *   _gtk_builder_get_resource_path (GtkBuilder *builder,
					  const gchar *string);
gchar *   _gtk_builder_get_absolute_filename (GtkBuilder *builder,
					      const gchar *string);

void      _gtk_builder_menu_start (ParserData   *parser_data,
                                   const gchar  *element_name,
                                   const gchar **attribute_names,
                                   const gchar **attribute_values,
                                   GError      **error);
void      _gtk_builder_menu_end   (ParserData  *parser_data);

GType     _gtk_builder_get_template_type (GtkBuilder *builder);

void     _gtk_builder_prefix_error        (GtkBuilder                *builder,
                                           GtkBuildableParseContext  *context,
                                           GError                   **error);
void     _gtk_builder_error_unhandled_tag (GtkBuilder                *builder,
                                           GtkBuildableParseContext  *context,
                                           const gchar               *object,
                                           const gchar               *element_name,
                                           GError                   **error);
gboolean _gtk_builder_check_parent        (GtkBuilder                *builder,
                                           GtkBuildableParseContext  *context,
                                           const gchar               *parent_name,
                                           GError                   **error);
GObject *_gtk_builder_lookup_object       (GtkBuilder                *builder,
                                           const gchar               *name,
                                           gint                       line,
                                           gint                       col);
gboolean _gtk_builder_lookup_failed       (GtkBuilder                *builder,
                                           GError                   **error);



#endif /* __GTK_BUILDER_PRIVATE_H__ */
