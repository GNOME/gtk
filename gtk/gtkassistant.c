/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1999  Red Hat, Inc.
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003  Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2005  Carlos Garnacho Parro <carlosg@gnome.org>
 *
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gtkassistant
 * @Short_description: A widget used to guide users through multi-step operations
 * @Title: GtkAssistant
 *
 * A #GtkAssistant is a widget used to represent a generally complex
 * operation splitted in several steps, guiding the user through its
 * pages and controlling the page flow to collect the necessary data.
 *
 * The design of GtkAssistant is that it controls what buttons to show
 * and to make sensitive, based on what it knows about the page sequence
 * and the [type][GtkAssistantPageType] of each page,
 * in addition to state information like the page
 * [completion][gtk-assistant-set-page-complete]
 * and [committed][gtk-assistant-commit] status.
 *
 * If you have a case that doesn’t quite fit in #GtkAssistants way of
 * handling buttons, you can use the #GTK_ASSISTANT_PAGE_CUSTOM page
 * type and handle buttons yourself.
 *
 * # GtkAssistant as GtkBuildable
 *
 * The GtkAssistant implementation of the #GtkBuildable interface
 * exposes the @action_area as internal children with the name
 * “action_area”.
 *
 * To add pages to an assistant in #GtkBuilder, simply add it as a
 * child to the GtkAssistant object, and set its child properties
 * as necessary.
 */

#include "config.h"

#include <atk/atk.h>

#include "gtkassistant.h"

#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkframe.h"
#include "gtknotebook.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtksettings.h"
#include "gtksizegroup.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"
#include "a11y/gtkwindowaccessible.h"


#define HEADER_SPACING 12
#define ACTION_AREA_SPACING 12

typedef struct _GtkAssistantPage GtkAssistantPage;

struct _GtkAssistantPage
{
  GtkAssistantPageType type;
  guint      complete     : 1;
  guint      complete_set : 1;

  gchar *title;

  GtkWidget *page;
  GtkWidget *regular_title;
  GtkWidget *current_title;
  GdkPixbuf *header_image;
  GdkPixbuf *sidebar_image;
};

struct _GtkAssistantPrivate
{
  GtkWidget *cancel;
  GtkWidget *forward;
  GtkWidget *back;
  GtkWidget *apply;
  GtkWidget *close;
  GtkWidget *last;

  GtkWidget *sidebar;
  GtkWidget *sidebar_frame;
  GtkWidget *content;
  GtkWidget *action_area;
  GtkWidget *headerbar;
  gint use_header_bar;
  gboolean constructed;

  GList     *pages;
  GSList    *visited_pages;
  GtkAssistantPage *current_page;

  GtkSizeGroup *button_size_group;
  GtkSizeGroup *title_size_group;

  GtkAssistantPageFunc forward_function;
  gpointer forward_function_data;
  GDestroyNotify forward_data_destroy;

  gint extra_buttons;

  guint committed : 1;
};

static void     gtk_assistant_class_init         (GtkAssistantClass *class);
static void     gtk_assistant_init               (GtkAssistant      *assistant);
static void     gtk_assistant_destroy            (GtkWidget         *widget);
static void     gtk_assistant_map                (GtkWidget         *widget);
static void     gtk_assistant_unmap              (GtkWidget         *widget);
static gboolean gtk_assistant_delete_event       (GtkWidget         *widget,
                                                  GdkEventAny       *event);
static void     gtk_assistant_add                (GtkContainer      *container,
                                                  GtkWidget         *page);
static void     gtk_assistant_remove             (GtkContainer      *container,
                                                  GtkWidget         *page);
static void     gtk_assistant_set_child_property (GtkContainer      *container,
                                                  GtkWidget         *child,
                                                  guint              property_id,
                                                  const GValue      *value,
                                                  GParamSpec        *pspec);
static void     gtk_assistant_get_child_property (GtkContainer      *container,
                                                  GtkWidget         *child,
                                                  guint              property_id,
                                                  GValue            *value,
                                                  GParamSpec        *pspec);

static void       gtk_assistant_buildable_interface_init     (GtkBuildableIface *iface);
static gboolean   gtk_assistant_buildable_custom_tag_start   (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *tagname,
                                                              GMarkupParser *parser,
                                                              gpointer      *data);
static void       gtk_assistant_buildable_custom_finished    (GtkBuildable  *buildable,
                                                              GtkBuilder    *builder,
                                                              GObject       *child,
                                                              const gchar   *tagname,
                                                              gpointer       user_data);

static GList*     find_page                                  (GtkAssistant  *assistant,
                                                              GtkWidget     *page);
static void       gtk_assistant_do_set_page_header_image     (GtkAssistant  *assistant,
                                                              GtkWidget     *page,
                                                              GdkPixbuf     *pixbuf);
static void       gtk_assistant_do_set_page_side_image       (GtkAssistant  *assistant,
                                                              GtkWidget     *page,
                                                              GdkPixbuf     *pixbuf);

static gboolean   assistant_sidebar_draw_cb                  (GtkWidget     *widget,
							      cairo_t       *cr,
							      gpointer       user_data);
static void       on_assistant_close                         (GtkWidget     *widget,
							      GtkAssistant  *assistant);
static void       on_assistant_apply                         (GtkWidget     *widget,
							      GtkAssistant  *assistant);
static void       on_assistant_forward                       (GtkWidget     *widget,
							      GtkAssistant  *assistant);
static void       on_assistant_back                          (GtkWidget     *widget,
							      GtkAssistant  *assistant);
static void       on_assistant_cancel                        (GtkWidget     *widget,
							      GtkAssistant  *assistant);
static void       on_assistant_last                          (GtkWidget     *widget,
							      GtkAssistant  *assistant);
static void       assistant_remove_page_cb                   (GtkNotebook   *notebook,
							      GtkWidget     *page,
							      GtkAssistant  *assistant);

GType             _gtk_assistant_accessible_get_type         (void);

enum
{
  CHILD_PROP_0,
  CHILD_PROP_PAGE_TYPE,
  CHILD_PROP_PAGE_TITLE,
  CHILD_PROP_PAGE_HEADER_IMAGE,
  CHILD_PROP_PAGE_SIDEBAR_IMAGE,
  CHILD_PROP_PAGE_COMPLETE
};

enum
{
  CANCEL,
  PREPARE,
  APPLY,
  CLOSE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_USE_HEADER_BAR
};

static guint signals [LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_CODE (GtkAssistant, gtk_assistant, GTK_TYPE_WINDOW,
                         G_ADD_PRIVATE (GtkAssistant)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_assistant_buildable_interface_init))

static void
set_use_header_bar (GtkAssistant *assistant,
                    gint          use_header_bar)
{
  GtkAssistantPrivate *priv = assistant->priv;

  if (use_header_bar == -1)
    return;

  priv->use_header_bar = use_header_bar;
}

static void
gtk_assistant_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);

  switch (prop_id)
    {
    case PROP_USE_HEADER_BAR:
      set_use_header_bar (assistant, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_assistant_get_property (GObject      *object,
                            guint         prop_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);
  GtkAssistantPrivate *priv = assistant->priv;

  switch (prop_id)
    {
    case PROP_USE_HEADER_BAR:
      g_value_set_int (value, priv->use_header_bar);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
add_cb (GtkContainer *container,
        GtkWidget    *widget,
        GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  if (priv->use_header_bar)
    g_warning ("Content added to the action area of a assistant using header bars");

  gtk_widget_show (GTK_WIDGET (container));
}

static void
apply_use_header_bar (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_widget_set_visible (priv->action_area, !priv->use_header_bar);
  gtk_widget_set_visible (priv->headerbar, priv->use_header_bar);
  if (!priv->use_header_bar)
    gtk_window_set_titlebar (GTK_WINDOW (assistant), NULL);
  if (priv->use_header_bar)
    g_signal_connect (priv->action_area, "add", G_CALLBACK (add_cb), assistant);
}

static void
add_to_header_bar (GtkAssistant *assistant,
                   GtkWidget    *child)
{
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);

  if (child == priv->back || child == priv->cancel)
    gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->headerbar), child);
  else
    gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->headerbar), child);
}

static void
add_action_widgets (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GList *children;
  GList *l;

  if (priv->use_header_bar)
    {
      children = gtk_container_get_children (GTK_CONTAINER (priv->action_area));
      for (l = children; l != NULL; l = l->next)
        {
          GtkWidget *child = l->data;
          gboolean has_default;

          has_default = gtk_widget_has_default (child);

          g_object_ref (child);
          gtk_container_remove (GTK_CONTAINER (priv->action_area), child);
          add_to_header_bar (assistant, child);
          g_object_unref (child);

          if (has_default)
            {
              gtk_widget_grab_default (child);
              gtk_style_context_add_class (gtk_widget_get_style_context (child), GTK_STYLE_CLASS_SUGGESTED_ACTION);
            }
        }
      g_list_free (children);
    }
}

static void
gtk_assistant_constructed (GObject *object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);
  GtkAssistantPrivate *priv = assistant->priv;

  G_OBJECT_CLASS (gtk_assistant_parent_class)->constructed (object);

  priv->constructed = TRUE;
  if (priv->use_header_bar == -1)
    priv->use_header_bar = FALSE;

  add_action_widgets (assistant);
  apply_use_header_bar (assistant);
}

static void
gtk_assistant_class_init (GtkAssistantClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class   = (GObjectClass *) class;
  widget_class    = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  gobject_class->constructed  = gtk_assistant_constructed;
  gobject_class->set_property = gtk_assistant_set_property;
  gobject_class->get_property = gtk_assistant_get_property;

  widget_class->destroy = gtk_assistant_destroy;
  widget_class->map = gtk_assistant_map;
  widget_class->unmap = gtk_assistant_unmap;
  widget_class->delete_event = gtk_assistant_delete_event;

  gtk_widget_class_set_accessible_type (widget_class, _gtk_assistant_accessible_get_type ());

  container_class->add = gtk_assistant_add;
  container_class->remove = gtk_assistant_remove;
  container_class->set_child_property = gtk_assistant_set_child_property;
  container_class->get_child_property = gtk_assistant_get_child_property;

  /**
   * GtkAssistant::cancel:
   * @assistant: the #GtkAssistant
   *
   * The ::cancel signal is emitted when then the cancel button is clicked.
   *
   * Since: 2.10
   */
  signals[CANCEL] =
    g_signal_new (I_("cancel"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAssistantClass, cancel),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkAssistant::prepare:
   * @assistant: the #GtkAssistant
   * @page: the current page
   *
   * The ::prepare signal is emitted when a new page is set as the
   * assistant's current page, before making the new page visible.
   *
   * A handler for this signal can do any preparations which are
   * necessary before showing @page.
   *
   * Since: 2.10
   */
  signals[PREPARE] =
    g_signal_new (I_("prepare"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAssistantClass, prepare),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

  /**
   * GtkAssistant::apply:
   * @assistant: the #GtkAssistant
   *
   * The ::apply signal is emitted when the apply button is clicked.
   *
   * The default behavior of the #GtkAssistant is to switch to the page
   * after the current page, unless the current page is the last one.
   *
   * A handler for the ::apply signal should carry out the actions for
   * which the wizard has collected data. If the action takes a long time
   * to complete, you might consider putting a page of type
   * %GTK_ASSISTANT_PAGE_PROGRESS after the confirmation page and handle
   * this operation within the #GtkAssistant::prepare signal of the progress
   * page.
   *
   * Since: 2.10
   */
  signals[APPLY] =
    g_signal_new (I_("apply"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAssistantClass, apply),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkAssistant::close:
   * @assistant: the #GtkAssistant
   *
   * The ::close signal is emitted either when the close button of
   * a summary page is clicked, or when the apply button in the last
   * page in the flow (of type %GTK_ASSISTANT_PAGE_CONFIRM) is clicked.
   *
   * Since: 2.10
   */
  signals[CLOSE] =
    g_signal_new (I_("close"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkAssistantClass, close),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkAssistant:use-header-bar:
   *
   * %TRUE if the assistant uses a #GtkHeaderBar for action buttons
   * instead of the action-area.
   *
   * For technical reasons, this property is declared as an integer
   * property, but you should only set it to %TRUE or %FALSE.
   *
   * Since: 3.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_HEADER_BAR,
                                   g_param_spec_int ("use-header-bar",
                                                     P_("Use Header Bar"),
                                                     P_("Use Header Bar for actions."),
                                                     -1, 1, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("header-padding",
                                                             P_("Header Padding"),
                                                             P_("Number of pixels around the header."),
                                                             0,
                                                             G_MAXINT,
                                                             6,
                                                             GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("content-padding",
                                                             P_("Content Padding"),
                                                             P_("Number of pixels around the content pages."),
                                                             0,
                                                             G_MAXINT,
                                                             1,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkAssistant:page-type:
   *
   * The type of the assistant page.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_PAGE_TYPE,
                                              g_param_spec_enum ("page-type",
                                                                 P_("Page type"),
                                                                 P_("The type of the assistant page"),
                                                                 GTK_TYPE_ASSISTANT_PAGE_TYPE,
                                                                 GTK_ASSISTANT_PAGE_CONTENT,
                                                                 GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:title:
   *
   * The title of the page.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_PAGE_TITLE,
                                              g_param_spec_string ("title",
                                                                   P_("Page title"),
                                                                   P_("The title of the assistant page"),
                                                                   NULL,
                                                                   GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:header-image:
   *
   * This image used to be displayed in the page header.
   *
   * Since: 2.10
   *
   * Deprecated: 3.2: Since GTK+ 3.2, a header is no longer shown;
   *     add your header decoration to the page content instead.
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_PAGE_HEADER_IMAGE,
                                              g_param_spec_object ("header-image",
                                                                   P_("Header image"),
                                                                   P_("Header image for the assistant page"),
                                                                   GDK_TYPE_PIXBUF,
                                                                   GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:sidebar-image:
   *
   * This image used to be displayed in the 'sidebar'.
   *
   * Since: 2.10
   *
   * Deprecated: 3.2: Since GTK+ 3.2, the sidebar image is no longer shown.
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_PAGE_SIDEBAR_IMAGE,
                                              g_param_spec_object ("sidebar-image",
                                                                   P_("Sidebar image"),
                                                                   P_("Sidebar image for the assistant page"),
                                                                   GDK_TYPE_PIXBUF,
                                                                   GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:complete:
   *
   * Setting the "complete" child property to %TRUE marks a page as
   * complete (i.e.: all the required fields are filled out). GTK+ uses
   * this information to control the sensitivity of the navigation buttons.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_PAGE_COMPLETE,
                                              g_param_spec_boolean ("complete",
                                                                    P_("Page complete"),
                                                                    P_("Whether all required fields on the page have been filled out"),
                                                                    FALSE,
                                                                    G_PARAM_READWRITE));

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkassistant.ui");

  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkAssistant, action_area);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkAssistant, headerbar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, content);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, cancel);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, forward);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, back);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, apply);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, close);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, last);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, sidebar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, sidebar_frame);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, button_size_group);
  gtk_widget_class_bind_template_child_private (widget_class, GtkAssistant, title_size_group);

  gtk_widget_class_bind_template_callback (widget_class, assistant_sidebar_draw_cb);
  gtk_widget_class_bind_template_callback (widget_class, assistant_remove_page_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_close);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_apply);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_forward);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_back);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_cancel);
  gtk_widget_class_bind_template_callback (widget_class, on_assistant_last);
}

static gint
default_forward_function (gint current_page, gpointer data)
{
  GtkAssistant *assistant;
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *page_node;

  assistant = GTK_ASSISTANT (data);
  priv = assistant->priv;

  page_node = g_list_nth (priv->pages, ++current_page);

  if (!page_node)
    return -1;

  page_info = (GtkAssistantPage *) page_node->data;

  while (page_node && !gtk_widget_get_visible (page_info->page))
    {
      page_node = page_node->next;
      current_page++;

      if (page_node)
        page_info = (GtkAssistantPage *) page_node->data;
    }

  return current_page;
}

static gboolean
last_button_visible (GtkAssistant *assistant, GtkAssistantPage *page)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  gint count, page_num, n_pages;

  if (page == NULL)
    return FALSE;

  if (page->type != GTK_ASSISTANT_PAGE_CONTENT)
    return FALSE;

  count = 0;
  page_num = g_list_index (priv->pages, page);
  n_pages  = g_list_length (priv->pages);
  page_info = page;

  while (page_num >= 0 && page_num < n_pages &&
         page_info->type == GTK_ASSISTANT_PAGE_CONTENT &&
         (count == 0 || page_info->complete) &&
         count < n_pages)
    {
      page_num = (priv->forward_function) (page_num, priv->forward_function_data);
      page_info = g_list_nth_data (priv->pages, page_num);

      count++;
    }

  /* Make the last button visible if we can skip multiple
   * pages and end on a confirmation or summary page
   */
  if (count > 1 && page_info &&
      (page_info->type == GTK_ASSISTANT_PAGE_CONFIRM ||
       page_info->type == GTK_ASSISTANT_PAGE_SUMMARY))
    return TRUE;
  else
    return FALSE;
}

static void
update_actions_size (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GList *l;
  GtkAssistantPage *page;
  gint buttons, page_buttons;

  if (!priv->current_page)
    return;

  /* Some heuristics to find out how many buttons we should
   * reserve space for. It is possible to trick this code
   * with page forward functions and invisible pages, etc.
   */
  buttons = 0;
  for (l = priv->pages; l; l = l->next)
    {
      page = l->data;

      if (!gtk_widget_get_visible (page->page))
        continue;

      page_buttons = 2; /* cancel, forward/apply/close */
      if (l != priv->pages)
        page_buttons += 1; /* back */
      if (last_button_visible (assistant, page))
        page_buttons += 1; /* last */

      buttons = MAX (buttons, page_buttons);
    }

  buttons += priv->extra_buttons;

  gtk_widget_set_size_request (priv->action_area,
                               buttons * gtk_widget_get_allocated_width (priv->cancel) + (buttons - 1) * 6,
                               -1);
}

static void
compute_last_button_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_widget_set_sensitive (priv->last, priv->current_page->complete);
  if (last_button_visible (assistant, priv->current_page))
    gtk_widget_show (priv->last);
  else
    gtk_widget_hide (priv->last);
}

static void
compute_progress_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  gint page_num, n_pages;

  n_pages = gtk_assistant_get_n_pages (assistant);
  page_num = gtk_assistant_get_current_page (assistant);

  page_num = (priv->forward_function) (page_num, priv->forward_function_data);

  if (page_num >= 0 && page_num < n_pages)
    gtk_widget_show (priv->forward);
  else
    gtk_widget_hide (priv->forward);
}

static void
update_buttons_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  if (!priv->current_page)
    return;

  switch (priv->current_page->type)
    {
    case GTK_ASSISTANT_PAGE_INTRO:
      gtk_widget_set_sensitive (priv->cancel, TRUE);
      gtk_widget_set_sensitive (priv->forward, priv->current_page->complete);
      gtk_widget_grab_default (priv->forward);
      gtk_widget_show (priv->forward);
      gtk_widget_hide (priv->back);
      gtk_widget_hide (priv->apply);
      gtk_widget_hide (priv->close);
      compute_last_button_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_CONFIRM:
      gtk_widget_set_sensitive (priv->cancel, TRUE);
      gtk_widget_set_sensitive (priv->back, TRUE);
      gtk_widget_set_sensitive (priv->apply, priv->current_page->complete);
      gtk_widget_grab_default (priv->apply);
      gtk_widget_show (priv->back);
      gtk_widget_show (priv->apply);
      gtk_widget_hide (priv->forward);
      gtk_widget_hide (priv->close);
      gtk_widget_hide (priv->last);
      break;
    case GTK_ASSISTANT_PAGE_CONTENT:
      gtk_widget_set_sensitive (priv->cancel, TRUE);
      gtk_widget_set_sensitive (priv->back, TRUE);
      gtk_widget_set_sensitive (priv->forward, priv->current_page->complete);
      gtk_widget_grab_default (priv->forward);
      gtk_widget_show (priv->back);
      gtk_widget_show (priv->forward);
      gtk_widget_hide (priv->apply);
      gtk_widget_hide (priv->close);
      compute_last_button_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_SUMMARY:
      gtk_widget_set_sensitive (priv->close, priv->current_page->complete);
      gtk_widget_grab_default (priv->close);
      gtk_widget_show (priv->close);
      gtk_widget_hide (priv->back);
      gtk_widget_hide (priv->forward);
      gtk_widget_hide (priv->apply);
      gtk_widget_hide (priv->last);
      break;
    case GTK_ASSISTANT_PAGE_PROGRESS:
      gtk_widget_set_sensitive (priv->cancel, priv->current_page->complete);
      gtk_widget_set_sensitive (priv->back, priv->current_page->complete);
      gtk_widget_set_sensitive (priv->forward, priv->current_page->complete);
      gtk_widget_grab_default (priv->forward);
      gtk_widget_show (priv->back);
      gtk_widget_hide (priv->apply);
      gtk_widget_hide (priv->close);
      gtk_widget_hide (priv->last);
      compute_progress_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_CUSTOM:
      gtk_widget_hide (priv->cancel);
      gtk_widget_hide (priv->back);
      gtk_widget_hide (priv->forward);
      gtk_widget_hide (priv->apply);
      gtk_widget_hide (priv->last);
      gtk_widget_hide (priv->close);
      break;
    default:
      g_assert_not_reached ();
    }

  if (priv->committed)
    gtk_widget_hide (priv->cancel);
  else if (priv->current_page->type == GTK_ASSISTANT_PAGE_SUMMARY ||
           priv->current_page->type == GTK_ASSISTANT_PAGE_CUSTOM)
    gtk_widget_hide (priv->cancel);
  else
    gtk_widget_show (priv->cancel);

  /* this is quite general, we don't want to
   * go back if it's the first page
   */
  if (!priv->visited_pages)
    gtk_widget_hide (priv->back);
}

static gboolean
update_page_title_state (GtkAssistant *assistant, GList *list)
{
  GtkAssistantPage *page, *other;
  GtkAssistantPrivate *priv = assistant->priv;
  gboolean visible;
  GList *l;

  page = list->data;

  if (page->title == NULL || page->title[0] == 0)
    visible = FALSE;
  else
    visible = gtk_widget_get_visible (page->page);

  if (page == priv->current_page)
    {
      gtk_widget_set_visible (page->regular_title, FALSE);
      gtk_widget_set_visible (page->current_title, visible);
    }
  else
    {
      /* If multiple consecutive pages have the same title,
       * we only show it once, since it would otherwise look
       * silly. We have to be a little careful, since we
       * _always_ show the title of the current page.
       */
      if (list->prev)
        {
          other = list->prev->data;
          if (g_strcmp0 (page->title, other->title) == 0)
            visible = FALSE;
        }
      for (l = list->next; l; l = l->next)
        {
          other = l->data;
          if (g_strcmp0 (page->title, other->title) != 0)
            break;

          if (other == priv->current_page)
            {
              visible = FALSE;
              break;
            }
        }

      gtk_widget_set_visible (page->regular_title, visible);
      gtk_widget_set_visible (page->current_title, FALSE);
    }

  return visible;
}

static void
update_title_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GList *l;
  gboolean show_titles;

  show_titles = FALSE;
  for (l = priv->pages; l != NULL; l = l->next)
    {
      if (update_page_title_state (assistant, l))
        show_titles = TRUE;
    }

  gtk_widget_set_visible (priv->sidebar_frame, show_titles);
}

static void
set_current_page (GtkAssistant *assistant,
                  gint          page_num)
{
  GtkAssistantPrivate *priv = assistant->priv;

  priv->current_page = (GtkAssistantPage *)g_list_nth_data (priv->pages, page_num);

  g_signal_emit (assistant, signals [PREPARE], 0, priv->current_page->page);
  /* do not continue if the prepare signal handler has already changed the
   * current page */
  if (priv->current_page != (GtkAssistantPage *)g_list_nth_data (priv->pages, page_num))
    return;

  update_title_state (assistant);

  gtk_window_set_title (GTK_WINDOW (assistant), priv->current_page->title);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->content), page_num);

  /* update buttons state, flow may have changed */
  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    update_buttons_state (assistant);

  if (!gtk_widget_child_focus (priv->current_page->page, GTK_DIR_TAB_FORWARD))
    {
      GtkWidget *button[6];
      gint i;

      /* find the best button to focus */
      button[0] = priv->apply;
      button[1] = priv->close;
      button[2] = priv->forward;
      button[3] = priv->back;
      button[4] = priv->cancel;
      button[5] = priv->last;
      for (i = 0; i < 6; i++)
        {
          if (gtk_widget_get_visible (button[i]) &&
              gtk_widget_get_sensitive (button[i]))
            {
              gtk_widget_grab_focus (button[i]);
              break;
            }
        }
    }

  gtk_widget_queue_resize (GTK_WIDGET (assistant));
}

static gint
compute_next_step (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  gint current_page, n_pages, next_page;

  current_page = gtk_assistant_get_current_page (assistant);
  page_info = priv->current_page;
  n_pages = gtk_assistant_get_n_pages (assistant);

  next_page = (priv->forward_function) (current_page,
                                        priv->forward_function_data);

  if (next_page >= 0 && next_page < n_pages)
    {
      priv->visited_pages = g_slist_prepend (priv->visited_pages, page_info);
      set_current_page (assistant, next_page);

      return TRUE;
    }

  return FALSE;
}

static void
on_assistant_close (GtkWidget    *widget,
                    GtkAssistant *assistant)
{
  g_signal_emit (assistant, signals [CLOSE], 0, NULL);
}

static void
on_assistant_apply (GtkWidget    *widget,
                    GtkAssistant *assistant)
{
  gboolean success;

  g_signal_emit (assistant, signals [APPLY], 0);

  success = compute_next_step (assistant);

  /* if the assistant hasn't switched to another page, just emit
   * the CLOSE signal, it't the last page in the assistant flow
   */
  if (!success)
    g_signal_emit (assistant, signals [CLOSE], 0);
}

static void
on_assistant_forward (GtkWidget    *widget,
                      GtkAssistant *assistant)
{
  gtk_assistant_next_page (assistant);
}

static void
on_assistant_back (GtkWidget    *widget,
                   GtkAssistant *assistant)
{
  gtk_assistant_previous_page (assistant);
}

static void
on_assistant_cancel (GtkWidget    *widget,
                     GtkAssistant *assistant)
{
  g_signal_emit (assistant, signals [CANCEL], 0, NULL);
}

static void
on_assistant_last (GtkWidget    *widget,
                   GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  while (priv->current_page->type == GTK_ASSISTANT_PAGE_CONTENT &&
         priv->current_page->complete)
    compute_next_step (assistant);
}

static gboolean
alternative_button_order (GtkAssistant *assistant)
{
  GtkSettings *settings;
  GdkScreen *screen;
  gboolean result;

  screen   = gtk_widget_get_screen (GTK_WIDGET (assistant));
  settings = gtk_settings_get_for_screen (screen);

  g_object_get (settings,
                "gtk-alternative-button-order", &result,
                NULL);
  return result;
}

static gboolean
assistant_sidebar_draw_cb (GtkWidget *widget,
                           cairo_t *cr,
                           gpointer user_data)
{
  gint width, height;
  GtkStyleContext *context;

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  context = gtk_widget_get_style_context (widget);

  gtk_render_background (context, cr, 0, 0, width, height);

  return FALSE;
}

static void
on_page_notify_visibility (GtkWidget  *widget,
                           GParamSpec *arg,
                           gpointer    data)
{
  GtkAssistant *assistant = GTK_ASSISTANT (data);

  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    {
      update_buttons_state (assistant);
      update_title_state (assistant);
    }
}

static void
assistant_remove_page_cb (GtkNotebook *notebook,
                          GtkWidget *page,
                          GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  GList *page_node;
  GList *element;

  element = find_page (assistant, page);
  if (!element)
    return;

  page_info = element->data;

  /* If this is the current page, we need to switch away. */
  if (page_info == priv->current_page)
    {
      if (!compute_next_step (assistant))
        {
          /* The best we can do at this point is probably to pick
           * the first visible page.
           */
          page_node = priv->pages;

          while (page_node &&
                 !gtk_widget_get_visible (((GtkAssistantPage *) page_node->data)->page))
            page_node = page_node->next;

          if (page_node == element)
            page_node = page_node->next;

          if (page_node)
            priv->current_page = page_node->data;
          else
            priv->current_page = NULL;
        }
    }

  g_signal_handlers_disconnect_by_func (page_info->page, on_page_notify_visibility, assistant);

  gtk_size_group_remove_widget (priv->title_size_group, page_info->regular_title);
  gtk_size_group_remove_widget (priv->title_size_group, page_info->current_title);

  gtk_container_remove (GTK_CONTAINER (priv->sidebar), page_info->regular_title);
  gtk_container_remove (GTK_CONTAINER (priv->sidebar), page_info->current_title);

  priv->pages = g_list_remove_link (priv->pages, element);
  priv->visited_pages = g_slist_remove_all (priv->visited_pages, page_info);

  g_free (page_info->title);

  g_slice_free (GtkAssistantPage, page_info);
  g_list_free_1 (element);

  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    {
      update_buttons_state (assistant);
      update_actions_size (assistant);
    }
}

static void
gtk_assistant_init (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  assistant->priv = gtk_assistant_get_instance_private (assistant);

  priv = assistant->priv;
  priv->pages = NULL;
  priv->current_page = NULL;
  priv->visited_pages = NULL;

  priv->forward_function = default_forward_function;
  priv->forward_function_data = assistant;
  priv->forward_data_destroy = NULL;

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (assistant)),
                "gtk-dialogs-use-header", &priv->use_header_bar,
                NULL);

  gtk_widget_init_template (GTK_WIDGET (assistant));

  if (alternative_button_order (assistant))
    {
      GList *buttons, *l;

      /* Reverse the action area children for the alternative button order setting */
      buttons = gtk_container_get_children (GTK_CONTAINER (priv->action_area));

      for (l = buttons; l; l = l->next)
	gtk_box_reorder_child (GTK_BOX (priv->action_area), GTK_WIDGET (l->data), -1);

      g_list_free (buttons);
    }
}

static void
gtk_assistant_set_child_property (GtkContainer *container,
                                  GtkWidget    *child,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_PAGE_TYPE:
      gtk_assistant_set_page_type (GTK_ASSISTANT (container), child,
                                   g_value_get_enum (value));
      break;
    case CHILD_PROP_PAGE_TITLE:
      gtk_assistant_set_page_title (GTK_ASSISTANT (container), child,
                                    g_value_get_string (value));
      break;
    case CHILD_PROP_PAGE_HEADER_IMAGE:
      gtk_assistant_do_set_page_header_image (GTK_ASSISTANT (container), child,
                                              g_value_get_object (value));
      break;
    case CHILD_PROP_PAGE_SIDEBAR_IMAGE:
      gtk_assistant_do_set_page_side_image (GTK_ASSISTANT (container), child,
                                            g_value_get_object (value));
      break;
    case CHILD_PROP_PAGE_COMPLETE:
      gtk_assistant_set_page_complete (GTK_ASSISTANT (container), child,
                                       g_value_get_boolean (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_assistant_get_child_property (GtkContainer *container,
                                  GtkWidget    *child,
                                  guint         property_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  GtkAssistant *assistant = GTK_ASSISTANT (container);

  switch (property_id)
    {
    case CHILD_PROP_PAGE_TYPE:
      g_value_set_enum (value,
                        gtk_assistant_get_page_type (assistant, child));
      break;
    case CHILD_PROP_PAGE_TITLE:
      g_value_set_string (value,
                          gtk_assistant_get_page_title (assistant, child));
      break;
    case CHILD_PROP_PAGE_HEADER_IMAGE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_value_set_object (value,
                          gtk_assistant_get_page_header_image (assistant, child));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;
    case CHILD_PROP_PAGE_SIDEBAR_IMAGE:
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_value_set_object (value,
                          gtk_assistant_get_page_side_image (assistant, child));
G_GNUC_END_IGNORE_DEPRECATIONS
      break;
    case CHILD_PROP_PAGE_COMPLETE:
      g_value_set_boolean (value,
                           gtk_assistant_get_page_complete (assistant, child));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_assistant_destroy (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;

  /* We set current to NULL so that the remove code doesn't try
   * to do anything funny
   */
  priv->current_page = NULL;

  if (priv->content)
    {
      GtkNotebook *notebook;
      GtkWidget *page;

      /* Remove all pages from the content notebook. */
      notebook = (GtkNotebook *) priv->content;
      while ((page = gtk_notebook_get_nth_page (notebook, 0)) != NULL)
        gtk_container_remove ((GtkContainer *) notebook, page);

      /* Our GtkAssistantPage list should be empty now. */
      g_warn_if_fail (priv->pages == NULL);

      priv->content = NULL;
    }

  if (priv->sidebar)
    priv->sidebar = NULL;

  if (priv->action_area)
    priv->action_area = NULL;

  if (priv->forward_function)
    {
      if (priv->forward_function_data &&
          priv->forward_data_destroy)
        priv->forward_data_destroy (priv->forward_function_data);

      priv->forward_function = NULL;
      priv->forward_function_data = NULL;
      priv->forward_data_destroy = NULL;
    }

  if (priv->visited_pages)
    {
      g_slist_free (priv->visited_pages);
      priv->visited_pages = NULL;
    }

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->destroy (widget);
}

static GList*
find_page (GtkAssistant  *assistant,
           GtkWidget     *page)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GList *child = priv->pages;

  while (child)
    {
      GtkAssistantPage *page_info = child->data;
      if (page_info->page == page)
        return child;

      child = child->next;
    }

  return NULL;
}

static void
gtk_assistant_map (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GList *page_node;
  GtkAssistantPage *page;
  gint page_num;

  /* if there's no default page, pick the first one */
  page = NULL;
  page_num = 0;
  if (!priv->current_page)
    {
      page_node = priv->pages;

      while (page_node && !gtk_widget_get_visible (((GtkAssistantPage *) page_node->data)->page))
        {
          page_node = page_node->next;
          page_num++;
        }

      if (page_node)
        page = page_node->data;
    }

  if (page && gtk_widget_get_visible (page->page))
    set_current_page (assistant, page_num);

  update_buttons_state (assistant);
  update_actions_size (assistant);
  update_title_state (assistant);

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->map (widget);
}

static void
gtk_assistant_unmap (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;

  g_slist_free (priv->visited_pages);
  priv->visited_pages = NULL;
  priv->current_page  = NULL;

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->unmap (widget);
}

static gboolean
gtk_assistant_delete_event (GtkWidget   *widget,
                            GdkEventAny *event)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;

  /* Do not allow cancelling in the middle of a progress page */
  if (priv->current_page &&
      (priv->current_page->type != GTK_ASSISTANT_PAGE_PROGRESS ||
       priv->current_page->complete))
    g_signal_emit (widget, signals [CANCEL], 0, NULL);

  return TRUE;
}

static void
gtk_assistant_add (GtkContainer *container,
                   GtkWidget    *page)
{
  /* A bit tricky here, GtkAssistant doesnt exactly play by 
   * the rules by allowing gtk_container_add() to insert pages.
   *
   * For the first invocation (from the builder template invocation),
   * let's make sure we add the actual direct container content properly.
   */
  if (!gtk_bin_get_child (GTK_BIN (container)))
    {
      gtk_widget_set_parent (page, GTK_WIDGET (container));
      _gtk_bin_set_child (GTK_BIN (container), page);
      return;
    }

  gtk_assistant_append_page (GTK_ASSISTANT (container), page);
}

static void
gtk_assistant_remove (GtkContainer *container,
                      GtkWidget    *page)
{
  GtkAssistant *assistant = (GtkAssistant*) container;

  /* Forward this removal to the content notebook */
  if (gtk_widget_get_parent (page) == assistant->priv->content)
    {
      container = (GtkContainer *) assistant->priv->content;
      gtk_container_remove (container, page);
    }
}

/**
 * gtk_assistant_new:
 *
 * Creates a new #GtkAssistant.
 *
 * Returns: a newly created #GtkAssistant
 *
 * Since: 2.10
 */
GtkWidget*
gtk_assistant_new (void)
{
  GtkWidget *assistant;

  assistant = g_object_new (GTK_TYPE_ASSISTANT, NULL);

  return assistant;
}

/**
 * gtk_assistant_get_current_page:
 * @assistant: a #GtkAssistant
 *
 * Returns the page number of the current page.
 *
 * Returns: The index (starting from 0) of the current
 *     page in the @assistant, or -1 if the @assistant has no pages,
 *     or no current page.
 *
 * Since: 2.10
 */
gint
gtk_assistant_get_current_page (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), -1);

  priv = assistant->priv;

  if (!priv->pages || !priv->current_page)
    return -1;

  return g_list_index (priv->pages, priv->current_page);
}

/**
 * gtk_assistant_set_current_page:
 * @assistant: a #GtkAssistant
 * @page_num: index of the page to switch to, starting from 0.
 *     If negative, the last page will be used. If greater
 *     than the number of pages in the @assistant, nothing
 *     will be done.
 *
 * Switches the page to @page_num.
 *
 * Note that this will only be necessary in custom buttons,
 * as the @assistant flow can be set with
 * gtk_assistant_set_forward_page_func().
 *
 * Since: 2.10
 */
void
gtk_assistant_set_current_page (GtkAssistant *assistant,
                                gint          page_num)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  priv = assistant->priv;

  if (page_num >= 0)
    page = (GtkAssistantPage *) g_list_nth_data (priv->pages, page_num);
  else
    {
      page = (GtkAssistantPage *) g_list_last (priv->pages)->data;
      page_num = g_list_length (priv->pages);
    }

  g_return_if_fail (page != NULL);

  if (priv->current_page == page)
    return;

  /* only add the page to the visited list if the assistant is mapped,
   * if not, just use it as an initial page setting, for the cases where
   * the initial page is != to 0
   */
  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    priv->visited_pages = g_slist_prepend (priv->visited_pages,
                                           priv->current_page);

  set_current_page (assistant, page_num);
}

/**
 * gtk_assistant_next_page:
 * @assistant: a #GtkAssistant
 *
 * Navigate to the next page.
 *
 * It is a programming error to call this function when
 * there is no next page.
 *
 * This function is for use when creating pages of the
 * #GTK_ASSISTANT_PAGE_CUSTOM type.
 *
 * Since: 3.0
 */
void
gtk_assistant_next_page (GtkAssistant *assistant)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  if (!compute_next_step (assistant))
    g_critical ("Page flow is broken.\n"
                "You may want to end it with a page of type\n"
                "GTK_ASSISTANT_PAGE_CONFIRM or GTK_ASSISTANT_PAGE_SUMMARY");
}

/**
 * gtk_assistant_previous_page:
 * @assistant: a #GtkAssistant
 *
 * Navigate to the previous visited page.
 *
 * It is a programming error to call this function when
 * no previous page is available.
 *
 * This function is for use when creating pages of the
 * #GTK_ASSISTANT_PAGE_CUSTOM type.
 *
 * Since: 3.0
 */
void
gtk_assistant_previous_page (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GSList *page_node;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  priv = assistant->priv;

  /* skip the progress pages when going back */
  do
    {
      page_node = priv->visited_pages;

      g_return_if_fail (page_node != NULL);

      priv->visited_pages = priv->visited_pages->next;
      page_info = (GtkAssistantPage *) page_node->data;
      g_slist_free_1 (page_node);
    }
  while (page_info->type == GTK_ASSISTANT_PAGE_PROGRESS ||
         !gtk_widget_get_visible (page_info->page));

  set_current_page (assistant, g_list_index (priv->pages, page_info));
}

/**
 * gtk_assistant_get_n_pages:
 * @assistant: a #GtkAssistant
 *
 * Returns the number of pages in the @assistant
 *
 * Returns: the number of pages in the @assistant
 *
 * Since: 2.10
 */
gint
gtk_assistant_get_n_pages (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);

  priv = assistant->priv;

  return g_list_length (priv->pages);
}

/**
 * gtk_assistant_get_nth_page:
 * @assistant: a #GtkAssistant
 * @page_num: the index of a page in the @assistant,
 *     or -1 to get the last page
 *
 * Returns the child widget contained in page number @page_num.
 *
 * Returns: (transfer none): the child widget, or %NULL
 *     if @page_num is out of bounds
 *
 * Since: 2.10
 */
GtkWidget*
gtk_assistant_get_nth_page (GtkAssistant *assistant,
                            gint          page_num)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page;
  GList *elem;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (page_num >= -1, NULL);

  priv = assistant->priv;

  if (page_num == -1)
    elem = g_list_last (priv->pages);
  else
    elem = g_list_nth (priv->pages, page_num);

  if (!elem)
    return NULL;

  page = (GtkAssistantPage *) elem->data;

  return page->page;
}

/**
 * gtk_assistant_prepend_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 *
 * Prepends a page to the @assistant.
 *
 * Returns: the index (starting at 0) of the inserted page
 *
 * Since: 2.10
 */
gint
gtk_assistant_prepend_page (GtkAssistant *assistant,
                            GtkWidget    *page)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);

  return gtk_assistant_insert_page (assistant, page, 0);
}

/**
 * gtk_assistant_append_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 *
 * Appends a page to the @assistant.
 *
 * Returns: the index (starting at 0) of the inserted page
 *
 * Since: 2.10
 */
gint
gtk_assistant_append_page (GtkAssistant *assistant,
                           GtkWidget    *page)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);

  return gtk_assistant_insert_page (assistant, page, -1);
}

/**
 * gtk_assistant_insert_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 * @position: the index (starting at 0) at which to insert the page,
 *     or -1 to append the page to the @assistant
 *
 * Inserts a page in the @assistant at a given position.
 *
 * Returns: the index (starting from 0) of the inserted page
 *
 * Since: 2.10
 */
gint
gtk_assistant_insert_page (GtkAssistant *assistant,
                           GtkWidget    *page,
                           gint          position)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  gint n_pages;
  GtkStyleContext *context;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);
  g_return_val_if_fail (gtk_widget_get_parent (page) == NULL, 0);
  g_return_val_if_fail (!gtk_widget_is_toplevel (page), 0);

  priv = assistant->priv;

  page_info = g_slice_new0 (GtkAssistantPage);
  page_info->page  = page;
  page_info->regular_title = gtk_label_new (NULL);
  gtk_widget_set_no_show_all (page_info->regular_title, TRUE);
  page_info->current_title = gtk_label_new (NULL);
  gtk_widget_set_no_show_all (page_info->current_title, TRUE);

  gtk_widget_set_halign (page_info->regular_title, GTK_ALIGN_START);
  gtk_widget_set_halign (page_info->current_title, GTK_ALIGN_START);

  gtk_widget_show (page_info->regular_title);
  gtk_widget_hide (page_info->current_title);

  context = gtk_widget_get_style_context (page_info->current_title);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_HIGHLIGHT);

  gtk_size_group_add_widget (priv->title_size_group, page_info->regular_title);
  gtk_size_group_add_widget (priv->title_size_group, page_info->current_title);

  g_signal_connect (G_OBJECT (page), "notify::visible",
                    G_CALLBACK (on_page_notify_visibility), assistant);

  n_pages = g_list_length (priv->pages);

  if (position < 0 || position > n_pages)
    position = n_pages;

  priv->pages = g_list_insert (priv->pages, page_info, position);

  gtk_box_pack_start (GTK_BOX (priv->sidebar), page_info->regular_title, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->sidebar), page_info->current_title, FALSE, FALSE, 0);
  gtk_box_reorder_child (GTK_BOX (priv->sidebar), page_info->regular_title, 2 * position);
  gtk_box_reorder_child (GTK_BOX (priv->sidebar), page_info->current_title, 2 * position + 1);

  gtk_notebook_insert_page (GTK_NOTEBOOK (priv->content), page, NULL, position);

  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    {
      update_buttons_state (assistant);
      update_actions_size (assistant);
    }

  return position;
}

/**
 * gtk_assistant_remove_page:
 * @assistant: a #GtkAssistant
 * @page_num: the index of a page in the @assistant,
 *     or -1 to remove the last page
 *
 * Removes the @page_num’s page from @assistant.
 *
 * Since: 3.2
 */
void
gtk_assistant_remove_page (GtkAssistant *assistant,
                           gint          page_num)
{
  GtkWidget *page;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  page = gtk_assistant_get_nth_page (assistant, page_num);

  if (page)
    gtk_container_remove (GTK_CONTAINER (assistant), page);
}

/**
 * gtk_assistant_set_forward_page_func:
 * @assistant: a #GtkAssistant
 * @page_func: (allow-none): the #GtkAssistantPageFunc, or %NULL
 *     to use the default one
 * @data: user data for @page_func
 * @destroy: destroy notifier for @data
 *
 * Sets the page forwarding function to be @page_func.
 *
 * This function will be used to determine what will be
 * the next page when the user presses the forward button.
 * Setting @page_func to %NULL will make the assistant to
 * use the default forward function, which just goes to the
 * next visible page.
 *
 * Since: 2.10
 */
void
gtk_assistant_set_forward_page_func (GtkAssistant         *assistant,
                                     GtkAssistantPageFunc  page_func,
                                     gpointer              data,
                                     GDestroyNotify        destroy)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  priv = assistant->priv;

  if (priv->forward_data_destroy &&
      priv->forward_function_data)
    (*priv->forward_data_destroy) (priv->forward_function_data);

  if (page_func)
    {
      priv->forward_function = page_func;
      priv->forward_function_data = data;
      priv->forward_data_destroy = destroy;
    }
  else
    {
      priv->forward_function = default_forward_function;
      priv->forward_function_data = assistant;
      priv->forward_data_destroy = NULL;
    }

  /* Page flow has possibly changed, so the
   * buttons state might need to change too
   */
  if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
    update_buttons_state (assistant);
}

static void
add_to_action_area (GtkAssistant *assistant,
                    GtkWidget    *child)
{
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE);

  gtk_box_pack_end (GTK_BOX (priv->action_area), child, FALSE, FALSE, 0);
}

/**
 * gtk_assistant_add_action_widget:
 * @assistant: a #GtkAssistant
 * @child: a #GtkWidget
 *
 * Adds a widget to the action area of a #GtkAssistant.
 *
 * Since: 2.10
 */
void
gtk_assistant_add_action_widget (GtkAssistant *assistant,
                                 GtkWidget    *child)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = assistant->priv;

  if (GTK_IS_BUTTON (child))
    {
      gtk_size_group_add_widget (priv->button_size_group, child);
      priv->extra_buttons += 1;
      if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
        update_actions_size (assistant);
    }

  if (priv->constructed && priv->use_header_bar)
    add_to_header_bar (assistant, child);
  else
    add_to_action_area (assistant, child);
}

/**
 * gtk_assistant_remove_action_widget:
 * @assistant: a #GtkAssistant
 * @child: a #GtkWidget
 *
 * Removes a widget from the action area of a #GtkAssistant.
 *
 * Since: 2.10
 */
void
gtk_assistant_remove_action_widget (GtkAssistant *assistant,
                                    GtkWidget    *child)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = assistant->priv;

  if (GTK_IS_BUTTON (child))
    {
      gtk_size_group_remove_widget (priv->button_size_group, child);
      priv->extra_buttons -= 1;
      if (gtk_widget_get_mapped (GTK_WIDGET (assistant)))
        update_actions_size (assistant);
    }

  gtk_container_remove (GTK_CONTAINER (priv->action_area), child);
}

/**
 * gtk_assistant_set_page_title:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @title: the new title for @page
 *
 * Sets a title for @page.
 *
 * The title is displayed in the header area of the assistant
 * when @page is the current page.
 *
 * Since: 2.10
 */
void
gtk_assistant_set_page_title (GtkAssistant *assistant,
                              GtkWidget    *page,
                              const gchar  *title)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  g_free (page_info->title);
  page_info->title = g_strdup (title);

  gtk_label_set_text ((GtkLabel*) page_info->regular_title, title);
  gtk_label_set_text ((GtkLabel*) page_info->current_title, title);

  gtk_widget_queue_resize (GTK_WIDGET (assistant));
  gtk_container_child_notify (GTK_CONTAINER (assistant), page, "title");
}

/**
 * gtk_assistant_get_page_title:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets the title for @page.
 *
 * Returns: the title for @page
 *
 * Since: 2.10
 */
const gchar*
gtk_assistant_get_page_title (GtkAssistant *assistant,
                              GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->title;
}

/**
 * gtk_assistant_set_page_type:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @type: the new type for @page
 *
 * Sets the page type for @page.
 *
 * The page type determines the page behavior in the @assistant.
 *
 * Since: 2.10
 */
void
gtk_assistant_set_page_type (GtkAssistant         *assistant,
                             GtkWidget            *page,
                             GtkAssistantPageType  type)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (type != page_info->type)
    {
      page_info->type = type;

      /* backwards compatibility to the era before fixing bug 604289 */
      if (type == GTK_ASSISTANT_PAGE_SUMMARY && !page_info->complete_set)
        {
          gtk_assistant_set_page_complete (assistant, page, TRUE);
          page_info->complete_set = FALSE;
        }

      /* Always set buttons state, a change in a future page
       * might change current page buttons
       */
      update_buttons_state (assistant);

      gtk_container_child_notify (GTK_CONTAINER (assistant), page, "page-type");
    }
}

/**
 * gtk_assistant_get_page_type:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets the page type of @page.
 *
 * Returns: the page type of @page
 *
 * Since: 2.10
 */
GtkAssistantPageType
gtk_assistant_get_page_type (GtkAssistant *assistant,
                             GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), GTK_ASSISTANT_PAGE_CONTENT);
  g_return_val_if_fail (GTK_IS_WIDGET (page), GTK_ASSISTANT_PAGE_CONTENT);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, GTK_ASSISTANT_PAGE_CONTENT);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->type;
}

/**
 * gtk_assistant_set_page_header_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @pixbuf: (allow-none): the new header image @page
 *
 * Sets a header image for @page.
 *
 * Since: 2.10
 *
 * Deprecated: 3.2: Since GTK+ 3.2, a header is no longer shown;
 *     add your header decoration to the page content instead.
 */
void
gtk_assistant_set_page_header_image (GtkAssistant *assistant,
                                     GtkWidget    *page,
                                     GdkPixbuf    *pixbuf)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  gtk_assistant_do_set_page_header_image (assistant, page, pixbuf);
}

static void
gtk_assistant_do_set_page_header_image (GtkAssistant *assistant,
                                        GtkWidget    *page,
                                        GdkPixbuf    *pixbuf)
{
  GtkAssistantPage *page_info;
  GList *child;

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (pixbuf != page_info->header_image)
    {
      if (page_info->header_image)
        {
          g_object_unref (page_info->header_image);
          page_info->header_image = NULL;
        }

      if (pixbuf)
        page_info->header_image = g_object_ref (pixbuf);

      gtk_container_child_notify (GTK_CONTAINER (assistant), page, "header-image");
    }
}

/**
 * gtk_assistant_get_page_header_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets the header image for @page.
 *
 * Returns: (transfer none): the header image for @page,
 *     or %NULL if there’s no header image for the page
 *
 * Since: 2.10
 *
 * Deprecated: 3.2: Since GTK+ 3.2, a header is no longer shown;
 *     add your header decoration to the page content instead.
 */
GdkPixbuf*
gtk_assistant_get_page_header_image (GtkAssistant *assistant,
                                     GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->header_image;
}

/**
 * gtk_assistant_set_page_side_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @pixbuf: (allow-none): the new side image @page
 *
 * Sets a side image for @page.
 *
 * This image used to be displayed in the side area of the assistant
 * when @page is the current page.
 *
 * Since: 2.10
 *
 * Deprecated: 3.2: Since GTK+ 3.2, sidebar images are not
 *     shown anymore.
 */
void
gtk_assistant_set_page_side_image (GtkAssistant *assistant,
                                   GtkWidget    *page,
                                   GdkPixbuf    *pixbuf)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  gtk_assistant_do_set_page_side_image (assistant, page, pixbuf);
}

static void
gtk_assistant_do_set_page_side_image (GtkAssistant *assistant,
                                      GtkWidget    *page,
                                      GdkPixbuf    *pixbuf)
{
  GtkAssistantPage *page_info;
  GList *child;

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (pixbuf != page_info->sidebar_image)
    {
      if (page_info->sidebar_image)
        {
          g_object_unref (page_info->sidebar_image);
          page_info->sidebar_image = NULL;
        }

      if (pixbuf)
        page_info->sidebar_image = g_object_ref (pixbuf);

      gtk_container_child_notify (GTK_CONTAINER (assistant), page, "sidebar-image");
    }
}

/**
 * gtk_assistant_get_page_side_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets the side image for @page.
 *
 * Returns: (transfer none): the side image for @page,
 *     or %NULL if there’s no side image for the page
 *
 * Since: 2.10
 *
 * Deprecated: 3.2: Since GTK+ 3.2, sidebar images are not
 *     shown anymore.
 */
GdkPixbuf*
gtk_assistant_get_page_side_image (GtkAssistant *assistant,
                                   GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->sidebar_image;
}

/**
 * gtk_assistant_set_page_complete:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * @complete: the completeness status of the page
 *
 * Sets whether @page contents are complete.
 *
 * This will make @assistant update the buttons state
 * to be able to continue the task.
 *
 * Since: 2.10
 */
void
gtk_assistant_set_page_complete (GtkAssistant *assistant,
                                 GtkWidget    *page,
                                 gboolean      complete)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (complete != page_info->complete)
    {
      page_info->complete = complete;
      page_info->complete_set = TRUE;

      /* Always set buttons state, a change in a future page
       * might change current page buttons
       */
      update_buttons_state (assistant);

      gtk_container_child_notify (GTK_CONTAINER (assistant), page, "complete");
    }
}

/**
 * gtk_assistant_get_page_complete:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets whether @page is complete.
 *
 * Returns: %TRUE if @page is complete.
 *
 * Since: 2.10
 */
gboolean
gtk_assistant_get_page_complete (GtkAssistant *assistant,
                                 GtkWidget    *page)
{
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (page), FALSE);

  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, FALSE);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->complete;
}

/**
 * gtk_assistant_update_buttons_state:
 * @assistant: a #GtkAssistant
 *
 * Forces @assistant to recompute the buttons state.
 *
 * GTK+ automatically takes care of this in most situations,
 * e.g. when the user goes to a different page, or when the
 * visibility or completeness of a page changes.
 *
 * One situation where it can be necessary to call this
 * function is when changing a value on the current page
 * affects the future page flow of the assistant.
 *
 * Since: 2.10
 */
void
gtk_assistant_update_buttons_state (GtkAssistant *assistant)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  update_buttons_state (assistant);
}

/**
 * gtk_assistant_commit:
 * @assistant: a #GtkAssistant
 *
 * Erases the visited page history so the back button is not
 * shown on the current page, and removes the cancel button
 * from subsequent pages.
 *
 * Use this when the information provided up to the current
 * page is hereafter deemed permanent and cannot be modified
 * or undone. For example, showing a progress page to track
 * a long-running, unreversible operation after the user has
 * clicked apply on a confirmation page.
 *
 * Since: 2.22
 */
void
gtk_assistant_commit (GtkAssistant *assistant)
{
  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  g_slist_free (assistant->priv->visited_pages);
  assistant->priv->visited_pages = NULL;

  assistant->priv->committed = TRUE;

  update_buttons_state (assistant);
}

/* accessible implementation */

/* dummy typedefs */
typedef GtkWindowAccessible      GtkAssistantAccessible;
typedef GtkWindowAccessibleClass GtkAssistantAccessibleClass;

G_DEFINE_TYPE (GtkAssistantAccessible, _gtk_assistant_accessible, GTK_TYPE_WINDOW_ACCESSIBLE);

static gint
gtk_assistant_accessible_get_n_children (AtkObject *accessible)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return 0;

  return g_list_length (GTK_ASSISTANT (widget)->priv->pages) + 2;
}

static AtkObject *
gtk_assistant_accessible_ref_child (AtkObject *accessible,
                                    gint       index)
{
  GtkAssistant *assistant;
  GtkAssistantPrivate *priv;
  GtkWidget *widget, *child;
  gint n_pages;
  AtkObject *obj;
  const gchar *title;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  assistant = GTK_ASSISTANT (widget);
  priv = assistant->priv;
  n_pages = g_list_length (priv->pages);

  if (index < 0)
    return NULL;
  else if (index < n_pages)
    {
      GtkAssistantPage *page = g_list_nth_data (priv->pages, index);

      child = page->page;
      title = gtk_assistant_get_page_title (assistant, child);
    }
  else if (index == n_pages)
    {
      child = priv->action_area;
      title = NULL;
    }
  else if (index == n_pages + 1)
    {
      child = priv->headerbar;
      title = NULL;
    }
  else
    return NULL;

  obj = gtk_widget_get_accessible (child);

  if (title)
    atk_object_set_name (obj, title);

  return g_object_ref (obj);
}

static void
_gtk_assistant_accessible_class_init (GtkAssistantAccessibleClass *klass)
{
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  atk_class->get_n_children = gtk_assistant_accessible_get_n_children;
  atk_class->ref_child = gtk_assistant_accessible_ref_child;
}

static void
_gtk_assistant_accessible_init (GtkAssistantAccessible *self)
{
}

/* buildable implementation */

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_assistant_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->custom_tag_start = gtk_assistant_buildable_custom_tag_start;
  iface->custom_finished = gtk_assistant_buildable_custom_finished;
}

gboolean
gtk_assistant_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GMarkupParser *parser,
                                          gpointer      *data)
{
  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                   tagname, parser, data);
}

static void
gtk_assistant_buildable_custom_finished (GtkBuildable *buildable,
                                         GtkBuilder   *builder,
                                         GObject      *child,
                                         const gchar  *tagname,
                                         gpointer      user_data)
{
  parent_buildable_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);
}
