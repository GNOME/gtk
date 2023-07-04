#include "config.h"

#include "gtktabwidgetprivate.h"

#include "gtkaccessible.h"
#include "gtkbinlayout.h"
#include "gtkdropcontrollermotion.h"
#include "gtkgestureclick.h"
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtkstack.h"
#include "gtkwidget.h"


#define TIMEOUT_EXPAND 500

struct _GtkTabWidget {
  GtkWidget parent_instance;

  GtkStackPage *page;
  GtkWidget *label;
  unsigned int position;
  unsigned int switch_timeout;
};

struct _GtkTabWidgetClass {
  GtkWidget parent_class;
};

enum {
  PROP_PAGE = 1,
  PROP_POSITION,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES] = { 0, };

G_DEFINE_TYPE (GtkTabWidget, gtk_tab_widget, GTK_TYPE_WIDGET)

static void
pressed_cb (GtkGestureClick *gesture,
            unsigned int     n_press,
            double           x,
            double           y,
            GtkTabWidget    *self)
{
  gtk_widget_activate_action (GTK_WIDGET (self), "tab.switch", "u", self->position);
}

static void
update_tab (GtkStackPage *page,
            GParamSpec   *pspec,
            GtkTabWidget *self)
{
  char *title;
  char *icon_name;
  gboolean needs_attention;
  gboolean visible;
  gboolean use_underline;

  g_object_get (page,
                "title", &title,
                "icon-name", &icon_name,
                "needs-attention", &needs_attention,
                "visible", &visible,
                "use-underline", &use_underline,
                NULL);

  gtk_label_set_label (GTK_LABEL (self->label), title);

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, title,
                                  -1);

  gtk_widget_set_visible (GTK_WIDGET (self), visible && (title != NULL || icon_name != NULL));

  if (needs_attention)
    gtk_widget_add_css_class (GTK_WIDGET (self), "needs-attention");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "needs-attention");

  g_free (title);
  g_free (icon_name);
}

static void
unset_page (GtkTabWidget *self)
{
  if (!self->page)
    return;

  g_signal_handlers_disconnect_by_func (self->page, update_tab, self);
  g_clear_object (&self->page);
}

static void
set_page (GtkTabWidget *self,
          GtkStackPage *page)
{
  if (self->page == page)
    return;

  unset_page (self);

  g_set_object (&self->page, page);
  g_signal_connect (self->page, "notify", G_CALLBACK (update_tab), self);
  update_tab (page, NULL, self);
}

static gboolean
gtk_tab_widget_switch_timeout (gpointer data)
{
  GtkTabWidget *self = data;

  gtk_widget_activate_action (GTK_WIDGET (self), "tab.switch", "u", self->position);

  self->switch_timeout = 0;

  return G_SOURCE_REMOVE;
}

static void
gtk_tab_widget_drag_enter (GtkDropControllerMotion *motion,
                           double                   x,
                           double                   y,
                           GtkTabWidget            *self)
{
  if ((gtk_widget_get_state_flags (GTK_WIDGET (self)) & GTK_STATE_FLAG_SELECTED) == 0)
    {
      self->switch_timeout = g_timeout_add (TIMEOUT_EXPAND,
                                            gtk_tab_widget_switch_timeout,
                                            self);
      gdk_source_set_static_name_by_id (self->switch_timeout,
                                        "[gtk] gtk_tab_widget_switch_timeout");
    }
}

static void
gtk_tab_widget_drag_leave (GtkDropControllerMotion *motion,
                           GtkTabWidget             *self)
{
  if (self->switch_timeout)
    {
      g_source_remove (self->switch_timeout);
      self->switch_timeout = 0;
    }
}

static void
gtk_tab_widget_init (GtkTabWidget *self)
{
  GtkEventController *controller;

  gtk_widget_set_can_focus (GTK_WIDGET (self), TRUE);
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  self->label = gtk_label_new ("");
  gtk_widget_set_parent (self->label, GTK_WIDGET (self));

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect (controller, "pressed", G_CALLBACK (pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);

  controller = gtk_drop_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_tab_widget_drag_enter), self);
  g_signal_connect (controller, "leave", G_CALLBACK (gtk_tab_widget_drag_leave), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

static void
gtk_tab_widget_get_property (GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  GtkTabWidget *self = GTK_TAB_WIDGET (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      g_value_set_object (value, self->page);
      break;

    case PROP_POSITION:
      g_value_set_uint (value, self->position);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tab_widget_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkTabWidget *self = GTK_TAB_WIDGET (object);

  switch (prop_id)
    {
    case PROP_PAGE:
      set_page (self, g_value_get_object (value));
      break;

    case PROP_POSITION:
      self->position = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tab_widget_dispose (GObject *object)
{
  GtkTabWidget *self = GTK_TAB_WIDGET (object);

  unset_page (self);
  g_clear_pointer (&self->label, gtk_widget_unparent);

  if (self->switch_timeout)
    {
      g_source_remove (self->switch_timeout);
      self->switch_timeout = 0;
    }

  G_OBJECT_CLASS (gtk_tab_widget_parent_class)->dispose (object);
}

static void
gtk_tab_widget_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_tab_widget_parent_class)->finalize (object);
}

static void
gtk_tab_widget_class_init (GtkTabWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_tab_widget_get_property;
  object_class->set_property = gtk_tab_widget_set_property;
  object_class->dispose = gtk_tab_widget_dispose;
  object_class->finalize = gtk_tab_widget_finalize;

  properties[PROP_PAGE] =
    g_param_spec_object ("page", NULL, NULL,
                         GTK_TYPE_STACK_PAGE,
                         GTK_PARAM_READWRITE);

  properties[PROP_POSITION] =
    g_param_spec_uint ("position", NULL, NULL,
                       0, G_MAXUINT, 0,
                       GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("tab"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TAB);
}

GtkWidget *
gtk_tab_widget_new (GtkStackPage *page,
                    unsigned int  position)
{
  return g_object_new (GTK_TYPE_TAB_WIDGET,
                       "page", page,
                       "position", position,
                       NULL);
}
