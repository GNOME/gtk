#include "config.h"

#include "gtktabbar.h"

#include "gtkboxlayout.h"
#include "gtkenums.h"
#include "gtklabel.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkselectionmodel.h"
#include "gtkstack.h"
#include "gtktabwidgetprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"


/**
 * GtkTabBar:
 *
 * `GtkTabBar` is a stack switcher that can be used with `GtkStack`
 * to prove an user experience similar to `GtkNotebook`.
 *
 * Since: 4.12
 */

struct _GtkTabBar {
  GtkWidget parent_instance;

  GtkStack *stack;
  GtkSelectionModel *pages;
  GPtrArray *tabs;

  GtkPositionType position;
};

struct _GtkTabBarClass {
  GtkWidget parent_class;
};

enum {
  PROP_POSITION = 1,
  PROP_STACK,
  NUM_PROPERTIES,
  PROP_ORIENTATION = NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { 0, };

G_DEFINE_TYPE_WITH_CODE (GtkTabBar, gtk_tab_bar, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
create_tabs (GtkTabBar *self)
{
  for (unsigned int i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->pages)); i++)
    {
      GtkStackPage *page;
      GtkWidget *tab;

      page = g_list_model_get_item (G_LIST_MODEL (self->pages), i);
      tab = gtk_tab_widget_new (page, i);

      gtk_widget_set_parent (tab, GTK_WIDGET (self));

      /* Note: self->tabs matches pages for order, not tabs */
      g_ptr_array_add (self->tabs, tab);

      g_object_unref (page);
    }
}

static void
clear_tabs (GtkTabBar *self)
{
  for (unsigned int i = 0; i < self->tabs->len; i++)
    {
      GtkWidget *tab = g_ptr_array_index (self->tabs, i);
      gtk_widget_unparent (tab);
    }

  g_ptr_array_set_size (self->tabs, 0);
}

static void
items_changed_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added,
                  GtkTabBar  *self)
{
  clear_tabs (self);
  create_tabs (self);
}

static void
selection_changed_cb (GtkSelectionModel *model,
                      guint              position,
                      guint              n_items,
                      GtkTabBar         *self)
{
  for (unsigned int i = position; i < position + n_items; i++)
    {
      GtkStackPage *page;
      GtkWidget *tab;
      gboolean selected;

      page = g_list_model_get_item (G_LIST_MODEL (self->pages), i);
      tab = g_ptr_array_index (self->tabs, i);
      selected = gtk_selection_model_is_selected (self->pages, i);

      if (selected)
        gtk_widget_set_state_flags (tab, GTK_STATE_FLAG_SELECTED, FALSE);
      else
        gtk_widget_unset_state_flags (tab, GTK_STATE_FLAG_SELECTED);

      gtk_accessible_update_state (GTK_ACCESSIBLE (tab),
                                   GTK_ACCESSIBLE_STATE_SELECTED, selected,
                                   -1);

      g_object_unref (page);
    }
}

static void
set_stack (GtkTabBar *self,
           GtkStack  *stack)
{
  g_assert (self->stack == NULL);
  if (stack)
    {
      self->stack = g_object_ref (stack);
      self->pages = gtk_stack_get_pages (stack);

      create_tabs (self);
      selection_changed_cb (self->pages,
                            0, g_list_model_get_n_items (G_LIST_MODEL (self->pages)),
                            self);

      g_signal_connect (self->pages,
                        "items-changed", G_CALLBACK (items_changed_cb), self);
      g_signal_connect (self->pages,
                        "selection-changed", G_CALLBACK (selection_changed_cb), self);
    }
}

static void
unset_stack (GtkTabBar *self)
{
  if (self->stack)
    {
      g_signal_handlers_disconnect_by_func (self->pages, items_changed_cb, self);
      g_signal_handlers_disconnect_by_func (self->pages, selection_changed_cb, self);

      clear_tabs (self);
      g_clear_object (&self->pages);
      g_clear_object (&self->stack);
    }
}

static void
gtk_tab_bar_init (GtkTabBar *self)
{
  self->position = GTK_POS_TOP;
  self->tabs = g_ptr_array_new ();
  gtk_widget_add_css_class (GTK_WIDGET (self), "top");
}

static void
gtk_tab_bar_get_property (GObject      *object,
                          guint         prop_id,
                          GValue       *value,
                          GParamSpec   *pspec)
{
  GtkTabBar *self = GTK_TAB_BAR (object);
  GtkLayoutManager *box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, gtk_orientable_get_orientation (GTK_ORIENTABLE (box_layout)));
      break;

    case PROP_POSITION:
      g_value_set_enum (value, self->position);
      break;

    case PROP_STACK:
      g_value_set_object (value, self->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tab_bar_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkTabBar *self = GTK_TAB_BAR (object);
  GtkLayoutManager *box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (self));

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box_layout)) != orientation)
          {
            gtk_orientable_set_orientation (GTK_ORIENTABLE (box_layout), orientation);
            gtk_widget_update_orientation (GTK_WIDGET (self), orientation);
            g_object_notify_by_pspec (object, pspec);
          }
      }
      break;

    case PROP_POSITION:
      gtk_tab_bar_set_position (self, g_value_get_enum (value));
      break;

    case PROP_STACK:
      gtk_tab_bar_set_stack (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tab_bar_dispose (GObject *object)
{
  GtkTabBar *self = GTK_TAB_BAR (object);

  unset_stack (self);

  G_OBJECT_CLASS (gtk_tab_bar_parent_class)->dispose (object);
}

static void
gtk_tab_bar_finalize (GObject *object)
{
  GtkTabBar *self = GTK_TAB_BAR (object);

  g_assert (self->tabs->len == 0);
  g_ptr_array_unref (self->tabs);

  G_OBJECT_CLASS (gtk_tab_bar_parent_class)->finalize (object);
}

static void
gtk_tab_bar_switch_tab (GtkWidget  *widget,
                        const char *action_name,
                        GVariant   *parameters)
{
  GtkTabBar *self = GTK_TAB_BAR (widget);
  unsigned int position;

  if (!self->stack)
    return;

  g_variant_get (parameters, "u", &position);

  gtk_selection_model_select_item (self->pages, position, TRUE);
}

static GtkDirectionType
get_effective_direction (GtkTabBar        *self,
                         GtkDirectionType  direction)
{
  /* Remap the directions into the effective direction it would be for a
   * ltr-and-top tabbar.
   */

#define D(rest) GTK_DIR_##rest

  static const GtkDirectionType translate_direction[2][4][6] = {
    /* LEFT */   {{ D(TAB_FORWARD),  D(TAB_BACKWARD), D(LEFT), D(RIGHT), D(UP),   D(DOWN) },
    /* RIGHT */  { D(TAB_BACKWARD), D(TAB_FORWARD),  D(LEFT), D(RIGHT), D(DOWN), D(UP)   },
    /* TOP */    { D(TAB_FORWARD),  D(TAB_BACKWARD), D(UP),   D(DOWN),  D(LEFT), D(RIGHT) },
    /* BOTTOM */ { D(TAB_BACKWARD), D(TAB_FORWARD),  D(DOWN), D(UP),    D(LEFT), D(RIGHT) }},
    /* LEFT */  {{ D(TAB_BACKWARD), D(TAB_FORWARD),  D(LEFT), D(RIGHT), D(DOWN), D(UP)   },
    /* RIGHT */  { D(TAB_FORWARD),  D(TAB_BACKWARD), D(LEFT), D(RIGHT), D(UP),   D(DOWN) },
    /* TOP */    { D(TAB_FORWARD),  D(TAB_BACKWARD), D(UP),   D(DOWN),  D(RIGHT), D(LEFT) },
    /* BOTTOM */ { D(TAB_BACKWARD), D(TAB_FORWARD),  D(DOWN), D(UP),    D(RIGHT), D(LEFT) }},
  };

#undef D

  int text_dir = gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL ? 1 : 0;

  return translate_direction[text_dir][self->position][direction];
}

static GtkPositionType
get_effective_position (GtkTabBar *self)
{
  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_RTL)
    {
      switch ((int)self->position)
        {
        case GTK_POS_LEFT:
          return GTK_POS_RIGHT;
        case GTK_POS_RIGHT:
          return GTK_POS_LEFT;
        default: ;
        }
    }

  return self->position;
}

static gboolean
gtk_tab_bar_focus (GtkWidget        *widget,
                   GtkDirectionType  dir)
{
  GtkTabBar *self = GTK_TAB_BAR (widget);
  GtkWidget *old_focus_child;
  unsigned int n_items;
  GtkDirectionType direction;

  direction = get_effective_direction (self, dir);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->pages));

  old_focus_child = gtk_widget_get_focus_child (widget);

  if (old_focus_child)
    {
      unsigned int position;

      g_object_get (old_focus_child, "position", &position, NULL);

      if (direction == GTK_DIR_TAB_FORWARD ||
          direction == GTK_DIR_TAB_BACKWARD)
        return FALSE;

      if (direction == GTK_DIR_LEFT ||
          direction == GTK_DIR_RIGHT)
        {
          GtkWidget *tab;

          if (direction == GTK_DIR_LEFT)
            position = (position + n_items - 1) % n_items;
          else
            position = (position + 1) % n_items;

          tab = g_ptr_array_index (self->tabs, position);
          gtk_widget_grab_focus (tab);
          gtk_selection_model_select_item (self->pages, position, TRUE);

          return TRUE;
        }

      if (gtk_widget_child_focus (old_focus_child, direction))
        {
          return TRUE;
        }
    }
  else
    {
      for (unsigned int i = 0; i < n_items; i++)
        {
          if (gtk_selection_model_is_selected (self->pages, i))
            {
              GtkWidget *tab;

              tab = g_ptr_array_index (self->tabs, i);
              gtk_widget_grab_focus (tab);

              return TRUE;
            }
        }
    }

  return gtk_widget_focus_child (widget, direction);
}

static void
gtk_tab_bar_class_init (GtkTabBarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_tab_bar_get_property;
  object_class->set_property = gtk_tab_bar_set_property;
  object_class->dispose = gtk_tab_bar_dispose;
  object_class->finalize = gtk_tab_bar_finalize;

  widget_class->focus = gtk_tab_bar_focus;

  /**
   * GtkTabBar:position: (attributes org.gtk.Property.get=gtk_tab_bar_get_position org.gtk.Property.set=gtk_tab_bar_set_position)
   *
   * The position of the tab bar relative to the stack it controls.
   *
   * This information is used in keynav and tab rendering.
   */
  properties[PROP_POSITION] =
    g_param_spec_enum ("position", NULL, NULL,
                       GTK_TYPE_POSITION_TYPE,
                       GTK_POS_TOP,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkTabBar:stack: (attributes org.gtk.Property.get=gtk_tab_bar_get_stack org.gtk.Property.set=gtk_tab_bar_set_stack)
   *
   * The stack that is controlled by this tab bar.
   */
  properties[PROP_STACK] =
    g_param_spec_object ("stack", NULL, NULL,
                         GTK_TYPE_STACK,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  gtk_widget_class_install_action (widget_class, "tab.switch", "u",
                                   gtk_tab_bar_switch_tab);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("tabbar"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TAB_LIST);
}

/**
 * gtk_tab_bar_new:
 *
 * Creates a new `GtkTabBar`.
 *
 * Returns: the newly created widget
 *
 * Since: 4.12
 */
GtkTabBar *
gtk_tab_bar_new (void)
{
  return g_object_new (GTK_TYPE_TAB_BAR, NULL);
}

/**
 * gtk_stack_bar_set_stack:
 * @self: a `GtkTabBar`
 * @stack: (nullable): a `GtkStack`
 *
 * Sets the stack that is controlled by this tab bar.
 *
 * Since: 4.12
 */
void
gtk_tab_bar_set_stack (GtkTabBar *self,
                       GtkStack  *stack)
{
  g_return_if_fail (GTK_IS_TAB_BAR (self));
  g_return_if_fail (stack == NULL || GTK_IS_STACK (stack));

  if (self->stack == stack)
    return;

  unset_stack (self);
  set_stack (self, stack);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STACK]);
}

/**
 * gtk_tab_bar_get_stack:
 * @self: a `GtkStack`
 *
 * Returns the stack that is controlled by this tab bar.
 *
 * Returns: (nullable): the stack
 *
 * Since: 4.12
 */
GtkStack *
gtk_tab_bar_get_stack (GtkTabBar *self)
{
  g_return_val_if_fail (GTK_IS_TAB_BAR (self), NULL);

  return self->stack;
}

static void
update_css_class_for_position (GtkTabBar *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_remove_css_class (widget, "top");
  gtk_widget_remove_css_class (widget, "bottom");
  gtk_widget_remove_css_class (widget, "left");
  gtk_widget_remove_css_class (widget, "right");

  switch (get_effective_position (self))
    {
    case GTK_POS_TOP:
      gtk_widget_add_css_class (widget, "top");
      break;
    case GTK_POS_BOTTOM:
      gtk_widget_add_css_class (widget, "bottom");
      break;
    case GTK_POS_LEFT:
      gtk_widget_add_css_class (widget, "left");
      break;
    case GTK_POS_RIGHT:
      gtk_widget_add_css_class (widget, "right");
      break;
    default:
      g_assert_not_reached ();
    }
}

/**
 * gtk_tab_bar_set_position:
 * @self: a `GtkTabBar`
 * @position: the position of the tabs
 *
 * Sets the position of the tab bar relative to
 * the stack that it controls.
 *
 * This information is used in keynav and for
 * drawing the tabs. Setting the position also
 * updates the orientation accordingly.
 *
 * Since: 4.12
 */
void
gtk_tab_bar_set_position (GtkTabBar       *self,
                          GtkPositionType  position)
{
  g_return_if_fail (GTK_IS_TAB_BAR (self));

  if (self->position == position)
    return;

  self->position = position;

  if (position == GTK_POS_LEFT || position == GTK_POS_RIGHT)
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  else
    gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);

  update_css_class_for_position (self);

  gtk_widget_queue_resize (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POSITION]);
}

/**
 * gtk_tab_bar_get_position:
 * @self: a `GtkTabBar`
 *
 * Gets the position of the tab bar relative to
 * the stack that it controls.
 *
 * Returns: the position of the tabss
 *
 * Since: 4.12
 */
GtkPositionType
gtk_tab_bar_get_position (GtkTabBar *self)
{
  g_return_val_if_fail (GTK_IS_TAB_BAR (self), GTK_POS_TOP);

  return self->position;
}
