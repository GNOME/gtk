/*
 * Copyright © 2023 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "a11yoverlay.h"

#include "gtkwidget.h"
#include "gtkroot.h"
#include "gtknative.h"
#include "gtkwidgetprivate.h"
#include "gtkatcontextprivate.h"
#include "gtkaccessibleprivate.h"
#include "gtktypebuiltins.h"

struct _GtkA11yOverlay
{
  GtkInspectorOverlay parent_instance;

  GdkRGBA recommend_color;
  GdkRGBA error_color;

  GArray *context;
};

struct _GtkA11yOverlayClass
{
  GtkInspectorOverlayClass parent_class;
};

G_DEFINE_TYPE (GtkA11yOverlay, gtk_a11y_overlay, GTK_TYPE_INSPECTOR_OVERLAY)

typedef enum
{
  NAMING_DISCRETIONARY,
  NAMING_REQUIRED,
  NAMING_RECOMMENDED,
  NAMING_CONDITIONAL,
  NAMING_PROHIBITED
} NamingNecessity;

static NamingNecessity name_for_role[] = {
  NAMING_DISCRETIONARY, // ALERT
  NAMING_REQUIRED, // ALERT_DIALOG
  NAMING_DISCRETIONARY, // BANNER
  NAMING_CONDITIONAL, // BUTTON
  NAMING_PROHIBITED, // CAPTION
  NAMING_CONDITIONAL, // CELL
  NAMING_CONDITIONAL, // CHECKBOX
  NAMING_CONDITIONAL, // COLUMN_HEADER
  NAMING_REQUIRED, // COMBO_BOX
  NAMING_DISCRETIONARY, // COMMAND
  NAMING_DISCRETIONARY, // COMPOSITE
  NAMING_REQUIRED, // DIALOG
  NAMING_DISCRETIONARY, // DOCUMENT
  NAMING_RECOMMENDED, // FEED
  NAMING_RECOMMENDED, // FORM
  NAMING_PROHIBITED, // GENERIC
  NAMING_REQUIRED, // GRID
  NAMING_CONDITIONAL, // GRID_CELL
  NAMING_DISCRETIONARY, // GROUP
  NAMING_CONDITIONAL, // HEADING
  NAMING_REQUIRED, // IMG
  NAMING_DISCRETIONARY, // INPUT
  NAMING_DISCRETIONARY, // LABEL
  NAMING_DISCRETIONARY, // LANDMARK
  NAMING_DISCRETIONARY, // LEGEND
  NAMING_CONDITIONAL, // LINK
  NAMING_DISCRETIONARY, // LIST
  NAMING_REQUIRED, // LIST_BOX
  NAMING_PROHIBITED, // LIST_ITEM
  NAMING_DISCRETIONARY, // LOG
  NAMING_DISCRETIONARY, // MAIN
  NAMING_DISCRETIONARY, // MARQUEE
  NAMING_RECOMMENDED, // MATH
  NAMING_REQUIRED, // METER
  NAMING_RECOMMENDED, // MENU
  NAMING_RECOMMENDED, // MENU_BAR
  NAMING_CONDITIONAL, // MENU_ITEM
  NAMING_CONDITIONAL, // MENU_ITEM_CHECKBOX
  NAMING_CONDITIONAL, // MENU_ITEM_RADIO
  NAMING_RECOMMENDED, // NAVIGATION
  NAMING_PROHIBITED, // NONE
  NAMING_DISCRETIONARY, // NOTE
  NAMING_CONDITIONAL, // OPTION
  NAMING_PROHIBITED, // PRESENTATION
  NAMING_REQUIRED, // PROGRESS_BAR
  NAMING_CONDITIONAL, // RADIO
  NAMING_REQUIRED, // RADIO_GROUP
  NAMING_DISCRETIONARY, // RANGE
  NAMING_REQUIRED, // REGION
  NAMING_CONDITIONAL, // ROW
  NAMING_PROHIBITED, // ROW_GROUP
  NAMING_CONDITIONAL, // ROW_HEADER
  NAMING_DISCRETIONARY, // SCROLLBAR
  NAMING_RECOMMENDED, // SEARCH
  NAMING_REQUIRED, // SEARCH_BOX
  NAMING_DISCRETIONARY, // SECTION
  NAMING_DISCRETIONARY, // SECTION_HEAD
  NAMING_DISCRETIONARY, // SELECT
  NAMING_DISCRETIONARY, // SEPARATOR
  NAMING_REQUIRED, // SLIDER
  NAMING_REQUIRED, // SPIN_BUTTON
  NAMING_DISCRETIONARY, // STATUS
  NAMING_DISCRETIONARY, // STRUCTURE
  NAMING_CONDITIONAL, // SWITCH
  NAMING_CONDITIONAL, // TAB
  NAMING_REQUIRED, // TABLE
  NAMING_RECOMMENDED, // TAB_LIST
  NAMING_REQUIRED, // TAB_PANEL
  NAMING_REQUIRED, // TEXT_BOX
  NAMING_PROHIBITED, // TIME
  NAMING_DISCRETIONARY, // TIMER
  NAMING_RECOMMENDED, // TOOLBAR
  NAMING_CONDITIONAL, // TOOLTIP
  NAMING_REQUIRED, // TREE
  NAMING_REQUIRED, // TREE_GRID
  NAMING_CONDITIONAL, // TREE_ITEM
  NAMING_DISCRETIONARY, // WIDGET
  NAMING_DISCRETIONARY, // WINDOW
  NAMING_CONDITIONAL, // TOGGLE_BUTTON
};

static GtkAccessibleRole abstract_roles[] = {
  GTK_ACCESSIBLE_ROLE_COMMAND,
  GTK_ACCESSIBLE_ROLE_COMPOSITE,
  GTK_ACCESSIBLE_ROLE_INPUT,
  GTK_ACCESSIBLE_ROLE_LANDMARK,
  GTK_ACCESSIBLE_ROLE_RANGE,
  GTK_ACCESSIBLE_ROLE_SECTION,
  GTK_ACCESSIBLE_ROLE_SECTION_HEAD,
  GTK_ACCESSIBLE_ROLE_SELECT,
  GTK_ACCESSIBLE_ROLE_STRUCTURE,
#if 0
  /* FIXME: ARIA considers these abstract.
   * But we are using them for widgets
   */
  GTK_ACCESSIBLE_ROLE_WIDGET,
  GTK_ACCESSIBLE_ROLE_WINDOW
#endif
};

typedef enum
{
  SEVERITY_GOOD,
  SEVERITY_RECOMMENDATION,
  SEVERITY_ERROR
} FixSeverity;

typedef enum
{
  ATTRIBUTE_STATE,
  ATTRIBUTE_PROPERTY,
  ATTRIBUTE_RELATION
} AttributeType;

static struct {
  GtkAccessibleRole role;
  AttributeType type;
  int id;
} required_attributes[] = {
  { GTK_ACCESSIBLE_ROLE_CHECKBOX, ATTRIBUTE_STATE, GTK_ACCESSIBLE_STATE_CHECKED },
  { GTK_ACCESSIBLE_ROLE_COMBO_BOX, ATTRIBUTE_STATE, GTK_ACCESSIBLE_STATE_EXPANDED },
  { GTK_ACCESSIBLE_ROLE_COMBO_BOX, ATTRIBUTE_RELATION, GTK_ACCESSIBLE_RELATION_CONTROLS },
  { GTK_ACCESSIBLE_ROLE_HEADING, ATTRIBUTE_PROPERTY, GTK_ACCESSIBLE_PROPERTY_LEVEL },
  { GTK_ACCESSIBLE_ROLE_SCROLLBAR, ATTRIBUTE_RELATION, GTK_ACCESSIBLE_RELATION_CONTROLS },
  { GTK_ACCESSIBLE_ROLE_SCROLLBAR, ATTRIBUTE_PROPERTY, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW },
  { GTK_ACCESSIBLE_ROLE_SWITCH, ATTRIBUTE_STATE, GTK_ACCESSIBLE_STATE_CHECKED },
};

static struct {
  GtkAccessibleRole role;
  GtkAccessibleRole context;
} required_context[] = {
  { GTK_ACCESSIBLE_ROLE_CAPTION, GTK_ACCESSIBLE_ROLE_GRID },
  { GTK_ACCESSIBLE_ROLE_CAPTION, GTK_ACCESSIBLE_ROLE_TABLE },
  { GTK_ACCESSIBLE_ROLE_CAPTION, GTK_ACCESSIBLE_ROLE_TREE_GRID },
  { GTK_ACCESSIBLE_ROLE_CELL, GTK_ACCESSIBLE_ROLE_ROW },
  { GTK_ACCESSIBLE_ROLE_COLUMN_HEADER, GTK_ACCESSIBLE_ROLE_ROW },
  { GTK_ACCESSIBLE_ROLE_GRID_CELL, GTK_ACCESSIBLE_ROLE_ROW },
  { GTK_ACCESSIBLE_ROLE_LIST_ITEM, GTK_ACCESSIBLE_ROLE_LIST },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM, GTK_ACCESSIBLE_ROLE_GROUP },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM, GTK_ACCESSIBLE_ROLE_MENU },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM, GTK_ACCESSIBLE_ROLE_MENU_BAR },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX, GTK_ACCESSIBLE_ROLE_GROUP },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX, GTK_ACCESSIBLE_ROLE_MENU },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX, GTK_ACCESSIBLE_ROLE_MENU_BAR },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO, GTK_ACCESSIBLE_ROLE_GROUP },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO, GTK_ACCESSIBLE_ROLE_MENU },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO, GTK_ACCESSIBLE_ROLE_MENU_BAR },
  { GTK_ACCESSIBLE_ROLE_OPTION, GTK_ACCESSIBLE_ROLE_GROUP },
  { GTK_ACCESSIBLE_ROLE_OPTION, GTK_ACCESSIBLE_ROLE_LIST_BOX },
  { GTK_ACCESSIBLE_ROLE_ROW, GTK_ACCESSIBLE_ROLE_GRID },
  { GTK_ACCESSIBLE_ROLE_ROW, GTK_ACCESSIBLE_ROLE_ROW_GROUP },
  { GTK_ACCESSIBLE_ROLE_ROW, GTK_ACCESSIBLE_ROLE_TABLE },
  { GTK_ACCESSIBLE_ROLE_ROW, GTK_ACCESSIBLE_ROLE_TREE_GRID },
  { GTK_ACCESSIBLE_ROLE_ROW_GROUP, GTK_ACCESSIBLE_ROLE_GRID },
  { GTK_ACCESSIBLE_ROLE_ROW_GROUP, GTK_ACCESSIBLE_ROLE_TABLE },
  { GTK_ACCESSIBLE_ROLE_ROW_GROUP, GTK_ACCESSIBLE_ROLE_TREE_GRID },
  { GTK_ACCESSIBLE_ROLE_ROW_HEADER, GTK_ACCESSIBLE_ROLE_ROW },
  { GTK_ACCESSIBLE_ROLE_TAB, GTK_ACCESSIBLE_ROLE_TAB_LIST },
  { GTK_ACCESSIBLE_ROLE_TREE_ITEM, GTK_ACCESSIBLE_ROLE_GROUP },
  { GTK_ACCESSIBLE_ROLE_TREE_ITEM, GTK_ACCESSIBLE_ROLE_TREE },
};

static FixSeverity
check_accessibility_errors (GtkWidget  *widget,
                            GArray     *context_elements,
                            char      **hint)
{
  GtkAccessibleRole role;
  GtkATContext *context;
  gboolean label_set;
  const char *role_name;
  GEnumClass *states;
  GEnumClass *properties;
  GEnumClass *relations;
  gboolean has_context;

  *hint = NULL;

  role = gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (widget));
  role_name = gtk_accessible_role_to_name (role, NULL);

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (widget));
  if (!gtk_at_context_is_realized (context))
    gtk_at_context_realize (context);

  /* Check for abstract roles */
  for (unsigned int i = 0; i < G_N_ELEMENTS (abstract_roles); i++)
    {
      if (role == abstract_roles[i])
        {
          *hint = g_strdup_printf ("%s is an abstract role", role_name);

          return SEVERITY_ERROR;
        }
    }

  /* Check for name and description */
  label_set = gtk_at_context_has_accessible_property (context, GTK_ACCESSIBLE_PROPERTY_LABEL) ||
              gtk_at_context_has_accessible_relation (context, GTK_ACCESSIBLE_RELATION_LABELLED_BY);

  switch (name_for_role[role])
    {
    case NAMING_DISCRETIONARY:
      return SEVERITY_GOOD;

    case NAMING_REQUIRED:
      if (label_set)
        {
          return SEVERITY_GOOD;
        }
      else
        {
          *hint = g_strdup_printf ("%s must have label or labelled-by", role_name);

          return SEVERITY_ERROR;
        }
      break;

    case NAMING_PROHIBITED:
      if (label_set)
        {
          *hint = g_strdup_printf ("%s can't have label or labelled-by", role_name);

          return SEVERITY_ERROR;
        }
      else
        {
          return SEVERITY_GOOD;
        }
      break;

    case NAMING_RECOMMENDED:
      if (label_set)
        {
          return SEVERITY_GOOD;
        }
      else
        {
          *hint = g_strdup_printf ("label or labelled-by recommended for %s", role_name);

          return SEVERITY_RECOMMENDATION;
        }
      break;

    case NAMING_CONDITIONAL:
      {
        char *name = gtk_at_context_get_name (context);

        if (strcmp (name, "") == 0)
          {
            g_free (name);
            *hint = g_strdup_printf ("%s must have text content, label or labelled-by",
                                     role_name);

            return SEVERITY_ERROR;
          }
        else
          {
            return SEVERITY_GOOD;
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }

  /* Check for required attributes */
  states = g_type_class_peek (GTK_TYPE_ACCESSIBLE_STATE);
  properties = g_type_class_peek (GTK_TYPE_ACCESSIBLE_PROPERTY);
  relations = g_type_class_peek (GTK_TYPE_ACCESSIBLE_RELATION);

  for (unsigned int i = 0; i < G_N_ELEMENTS (required_attributes); i++)
    {
      if (role == required_attributes[i].role)
        {
          switch (required_attributes[i].type)
            {
            case ATTRIBUTE_STATE:
              if (!gtk_at_context_has_accessible_state (context, required_attributes[i].id))
                {
                  *hint = g_strdup_printf ("%s must have state %s", role_name, g_enum_get_value (states, required_attributes[i].id)->value_nick);
                  return SEVERITY_ERROR;
                }
              break;

            case ATTRIBUTE_PROPERTY:
              if (!gtk_at_context_has_accessible_property (context, required_attributes[i].id))
                {
                  *hint = g_strdup_printf ("%s must have property %s", role_name, g_enum_get_value (properties, required_attributes[i].id)->value_nick);
                  return SEVERITY_ERROR;
                }
              break;

            case ATTRIBUTE_RELATION:
              if (!gtk_at_context_has_accessible_relation (context, required_attributes[i].id))
                {
                  *hint = g_strdup_printf ("%s must have relation %s", role_name, g_enum_get_value (relations, required_attributes[i].id)->value_nick);
                  return SEVERITY_ERROR;
                }
              break;

            default:
              g_assert_not_reached ();
            }
        }
    }

  /* Check for required context */
  has_context = TRUE;

  for (unsigned int i = 0; i < G_N_ELEMENTS (required_context); i++)
    {
      if (required_context[i].role != role)
        continue;

      has_context = FALSE;

      for (unsigned int j = 0; j < context_elements->len; j++)
        {
          GtkAccessibleRole elt = g_array_index (context_elements, GtkAccessibleRole, j);

          if (required_context[i].context == elt)
            {
              has_context = TRUE;
              break;
            }
        }

      if (has_context)
        break;
    }

  if (!has_context)
    {
      GString *s = g_string_new ("");

      for (unsigned int i = 0; i < G_N_ELEMENTS (required_context); i++)
        {
          if (required_context[i].role != role)
            continue;

          if (s->len > 0)
            g_string_append (s, ", ");

          g_string_append (s, gtk_accessible_role_to_name (required_context[i].context, NULL));
        }

      *hint = g_strdup_printf ("%s requires context: %s", role_name, s->str);

      g_string_free (s, TRUE);

      return SEVERITY_ERROR;
    }

  return SEVERITY_GOOD;
}

static void
recurse_child_widgets (GtkA11yOverlay *self,
                       GtkWidget      *widget,
                       GtkSnapshot    *snapshot)
{
  GtkWidget *child;
  char *hint;
  FixSeverity severity;
  GtkAccessibleRole role;

  if (!gtk_widget_get_mapped (widget))
    return;

  severity = check_accessibility_errors (widget, self->context, &hint);

  if (severity != SEVERITY_GOOD)
    {
      int width, height;
      GdkRGBA color;

      width = gtk_widget_get_width (widget);
      height = gtk_widget_get_height (widget);

      if (severity == SEVERITY_ERROR)
        color = self->error_color;
      else
        color = self->recommend_color;

      gtk_snapshot_save (snapshot);
      gtk_snapshot_push_debug (snapshot, "Widget a11y debugging");

      gtk_snapshot_append_color (snapshot, &color,
                                 &GRAPHENE_RECT_INIT (0, 0, width, height));

      if (hint)
        {
          PangoLayout *layout;
          PangoRectangle extents;
          GdkRGBA black = { 0, 0, 0, 1 };
          float widths[4] = { 1, 1, 1, 1 };
          GdkRGBA colors[4] = {
            { 0, 0, 0, 1 },
            { 0, 0, 0, 1 },
            { 0, 0, 0, 1 },
            { 0, 0, 0, 1 },
          };

          gtk_snapshot_save (snapshot);

          layout = gtk_widget_create_pango_layout (widget, hint);
          pango_layout_set_width (layout, width * PANGO_SCALE);

          pango_layout_get_pixel_extents (layout, NULL, &extents);

          extents.x -= 5;
          extents.y -= 5;
          extents.width += 10;
          extents.height += 10;

          color.alpha = 0.8f;

          gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (0.5 * (width - extents.width), 0.5 * (height - extents.height)));

          gtk_snapshot_append_border (snapshot,
                                       &GSK_ROUNDED_RECT_INIT (extents.x, extents.y,
                                                               extents.width, extents.height),
                                      widths, colors);
          gtk_snapshot_append_color (snapshot, &color,
                                     &GRAPHENE_RECT_INIT (extents.x, extents.y,
                                                          extents.width, extents.height));

          gtk_snapshot_append_layout (snapshot, layout, &black);
          g_object_unref (layout);

          gtk_snapshot_restore (snapshot);
        }

      gtk_snapshot_pop (snapshot);
      gtk_snapshot_restore (snapshot);
   }

  g_free (hint);

  /* Recurse into child widgets */

  role = gtk_accessible_get_accessible_role (GTK_ACCESSIBLE (widget));
  g_array_append_val (self->context, role);

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_transform (snapshot, child->priv->transform);

      recurse_child_widgets (self, child, snapshot);

      gtk_snapshot_restore (snapshot);
    }

  g_array_remove_index (self->context, self->context->len - 1);
}

static void
gtk_a11y_overlay_snapshot (GtkInspectorOverlay *overlay,
                           GtkSnapshot         *snapshot,
                           GskRenderNode       *node,
                           GtkWidget           *widget)
{
  GtkA11yOverlay *self = GTK_A11Y_OVERLAY (overlay);

  g_assert (self->context->len == 0);

  recurse_child_widgets (self, widget, snapshot);

  g_assert (self->context->len == 0);
}

static void
gtk_a11y_overlay_finalize (GObject *object)
{
  GtkA11yOverlay *self = GTK_A11Y_OVERLAY (object);

  g_array_free (self->context, TRUE);

  G_OBJECT_CLASS (gtk_a11y_overlay_parent_class)->finalize (object);
}

static void
gtk_a11y_overlay_class_init (GtkA11yOverlayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkInspectorOverlayClass *overlay_class = GTK_INSPECTOR_OVERLAY_CLASS (klass);

  object_class->finalize = gtk_a11y_overlay_finalize;

  overlay_class->snapshot = gtk_a11y_overlay_snapshot;
}

static void
gtk_a11y_overlay_init (GtkA11yOverlay *self)
{
  self->recommend_color = (GdkRGBA) { 0.0, 0.5, 1.0, 0.2 };
  self->error_color = (GdkRGBA) { 1.0, 0.0, 0.0, 0.2 };

  self->context = g_array_new (FALSE, FALSE, sizeof (GtkAccessibleRole));
}

GtkInspectorOverlay *
gtk_a11y_overlay_new (void)
{
  GtkA11yOverlay *self;

  self = g_object_new (GTK_TYPE_A11Y_OVERLAY, NULL);

  return GTK_INSPECTOR_OVERLAY (self);
}