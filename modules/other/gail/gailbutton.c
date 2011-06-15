/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "gailbutton.h"
#include <libgail-util/gailmisc.h>

#define GAIL_BUTTON_ATTACHED_MENUS "gtk-attached-menus"

static void                  gail_button_class_init       (GailButtonClass *klass);
static void                  gail_button_init             (GailButton      *button);

static const gchar*          gail_button_get_name         (AtkObject       *obj);
static gint                  gail_button_get_n_children   (AtkObject       *obj);
static AtkObject*            gail_button_ref_child        (AtkObject       *obj,
                                                           gint            i);
static AtkStateSet*          gail_button_ref_state_set    (AtkObject       *obj);
static void                  gail_button_notify_label_gtk (GObject         *obj,
                                                           GParamSpec      *pspec,
                                                           gpointer        data);
static void                  gail_button_label_map_gtk    (GtkWidget       *widget,
                                                           gpointer        data);

static void                  gail_button_real_initialize  (AtkObject       *obj,
                                                           gpointer        data);
static void                  gail_button_finalize         (GObject        *object);
static void                  gail_button_init_textutil    (GailButton     *button,
                                                           GtkWidget      *label);

static void                  gail_button_pressed_enter_handler  (GtkWidget       *widget);
static void                  gail_button_released_leave_handler (GtkWidget       *widget);
static gint                  gail_button_real_add_gtk           (GtkContainer    *container,
                                                                 GtkWidget       *widget,
                                                                 gpointer        data);


static void                  atk_action_interface_init  (AtkActionIface *iface);
static gboolean              gail_button_do_action      (AtkAction      *action,
                                                         gint           i);
static gboolean              idle_do_action             (gpointer       data);
static gint                  gail_button_get_n_actions  (AtkAction      *action);
static const gchar*          gail_button_get_description(AtkAction      *action,
                                                         gint           i);
static const gchar*          gail_button_get_keybinding (AtkAction      *action,
                                                         gint           i);
static const gchar*          gail_button_action_get_name(AtkAction      *action,
                                                         gint           i);
static gboolean              gail_button_set_description(AtkAction      *action,
                                                         gint           i,
                                                         const gchar    *desc);
static void                  gail_button_notify_label_weak_ref (gpointer data,
                                                                GObject  *obj);
static void                  gail_button_notify_weak_ref       (gpointer data,
                                                                GObject  *obj);


/* AtkImage.h */
static void                  atk_image_interface_init   (AtkImageIface  *iface);
static const gchar*          gail_button_get_image_description
                                                        (AtkImage       *image);
static void	             gail_button_get_image_position
                                                        (AtkImage       *image,
                                                         gint	        *x,
                                                         gint	        *y,
                                                         AtkCoordType   coord_type);
static void                  gail_button_get_image_size (AtkImage       *image,
                                                         gint           *width,
                                                         gint           *height);
static gboolean              gail_button_set_image_description 
                                                        (AtkImage       *image,
                                                         const gchar    *description);

/* atktext.h */ 
static void	  atk_text_interface_init	   (AtkTextIface	*iface);

static gchar*	  gail_button_get_text		   (AtkText	      *text,
                                                    gint	      start_pos,
						    gint	      end_pos);
static gunichar	  gail_button_get_character_at_offset(AtkText	      *text,
						    gint	      offset);
static gchar*     gail_button_get_text_before_offset(AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_button_get_text_at_offset   (AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gchar*     gail_button_get_text_after_offset(AtkText	      *text,
 						    gint	      offset,
						    AtkTextBoundary   boundary_type,
						    gint	      *start_offset,
						    gint	      *end_offset);
static gint	  gail_button_get_character_count  (AtkText	      *text);
static void gail_button_get_character_extents      (AtkText	      *text,
						    gint 	      offset,
		                                    gint 	      *x,
                    		   	            gint 	      *y,
                                		    gint 	      *width,
                                     		    gint 	      *height,
			        		    AtkCoordType      coords);
static gint gail_button_get_offset_at_point        (AtkText           *text,
                                                    gint              x,
                                                    gint              y,
			                            AtkCoordType      coords);
static AtkAttributeSet* gail_button_get_run_attributes 
                                                   (AtkText           *text,
              					    gint 	      offset,
                                                    gint 	      *start_offset,
					            gint	      *end_offset);
static AtkAttributeSet* gail_button_get_default_attributes
                                                   (AtkText           *text);
static GtkImage*             get_image_from_button      (GtkWidget      *button);
static GtkWidget*            get_label_from_button      (GtkWidget      *button,
                                                         gint           index,
                                                         gboolean       allow_many);
static gint                  get_n_labels_from_button   (GtkWidget      *button);
static void                  set_role_for_button        (AtkObject      *accessible,
                                                         GtkWidget      *button);

static gint                  get_n_attached_menus       (GtkWidget      *widget);
static GtkWidget*            get_nth_attached_menu      (GtkWidget      *widget,
                                                         gint           index);

G_DEFINE_TYPE_WITH_CODE (GailButton, gail_button, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void
gail_button_class_init (GailButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailContainerClass *container_class;

  container_class = (GailContainerClass*)klass;

  gobject_class->finalize = gail_button_finalize;

  class->get_name = gail_button_get_name;
  class->get_n_children = gail_button_get_n_children;
  class->ref_child = gail_button_ref_child;
  class->ref_state_set = gail_button_ref_state_set;
  class->initialize = gail_button_real_initialize;

  container_class->add_gtk = gail_button_real_add_gtk;
  container_class->remove_gtk = NULL;
}

static void
gail_button_init (GailButton *button)
{
  button->click_description = NULL;
  button->press_description = NULL;
  button->release_description = NULL;
  button->click_keybinding = NULL;
  button->action_queue = NULL;
  button->action_idle_handler = 0;
  button->textutil = NULL;
}

static const gchar*
gail_button_get_name (AtkObject *obj)
{
  const gchar* name = NULL;

  g_return_val_if_fail (GAIL_IS_BUTTON (obj), NULL);

  name = ATK_OBJECT_CLASS (gail_button_parent_class)->get_name (obj);
  if (name == NULL)
    {
      /*
       * Get the text on the label
       */
      GtkWidget *widget;
      GtkWidget *child;

      widget = GTK_ACCESSIBLE (obj)->widget;
      if (widget == NULL)
        /*
         * State is defunct
         */
        return NULL;

      g_return_val_if_fail (GTK_IS_BUTTON (widget), NULL);

      child = get_label_from_button (widget, 0, FALSE);
      if (GTK_IS_LABEL (child))
        name = gtk_label_get_text (GTK_LABEL (child)); 
      else
        {
          GtkImage *image;

          image = get_image_from_button (widget);
          if (GTK_IS_IMAGE (image))
            {
              AtkObject *atk_obj;

              atk_obj = gtk_widget_get_accessible (GTK_WIDGET (image));
              name = atk_object_get_name (atk_obj);
            }
        }
    }
  return name;
}

/*
 * A DownArrow in a GtkToggltButton whose parent is not a ColorCombo
 * has press as default action.
 */
static gboolean
gail_button_is_default_press (GtkWidget *widget)
{
  GtkWidget  *child;
  GtkWidget  *parent;
  gboolean ret = FALSE;
  const gchar *parent_type_name;

  child = GTK_BIN (widget)->child;
  if (GTK_IS_ARROW (child) &&
      GTK_ARROW (child)->arrow_type == GTK_ARROW_DOWN)
    {
      parent = gtk_widget_get_parent (widget);
      if (parent)
        {
          parent_type_name = g_type_name (G_OBJECT_TYPE (parent));
          if (strcmp (parent_type_name, "ColorCombo"))
            return TRUE;
        }
    }

  return ret;
}

static void
gail_button_real_initialize (AtkObject *obj,
                             gpointer   data)
{
  GailButton *button = GAIL_BUTTON (obj);
  GtkWidget  *label;
  GtkWidget  *widget;

  ATK_OBJECT_CLASS (gail_button_parent_class)->initialize (obj, data);

  button->state = GTK_STATE_NORMAL;

  g_signal_connect (data,
                    "pressed",
                    G_CALLBACK (gail_button_pressed_enter_handler),
                    NULL);
  g_signal_connect (data,
                    "enter",
                    G_CALLBACK (gail_button_pressed_enter_handler),
                    NULL);
  g_signal_connect (data,
                    "released",
                    G_CALLBACK (gail_button_released_leave_handler),
                    NULL);
  g_signal_connect (data,
                    "leave",
                    G_CALLBACK (gail_button_released_leave_handler),
                    NULL);


  widget = GTK_WIDGET (data);
  label = get_label_from_button (widget, 0, FALSE);
  if (GTK_IS_LABEL (label))
    {
      if (gtk_widget_get_mapped (label))
        gail_button_init_textutil (button, label);
      else 
        g_signal_connect (label,
                          "map",
                          G_CALLBACK (gail_button_label_map_gtk),
                          button);
    }
  button->default_is_press = gail_button_is_default_press (widget);
    
  set_role_for_button (obj, data);
}

static void
gail_button_label_map_gtk (GtkWidget *widget,
                           gpointer data)
{
  GailButton *button; 

  button = GAIL_BUTTON (data);
  gail_button_init_textutil (button, widget);
}

static void
gail_button_notify_label_gtk (GObject           *obj,
                              GParamSpec        *pspec,
                              gpointer           data)
{
  AtkObject* atk_obj = ATK_OBJECT (data);
  GtkLabel *label;
  GailButton *gail_button;

  if (strcmp (pspec->name, "label") == 0)
    {
      const gchar* label_text;

      label = GTK_LABEL (obj);

      label_text = gtk_label_get_text (label);

      gail_button = GAIL_BUTTON (atk_obj);
      gail_text_util_text_setup (gail_button->textutil, label_text);

      if (atk_obj->name == NULL)
      {
        /*
         * The label has changed so notify a change in accessible-name
         */
        g_object_notify (G_OBJECT (atk_obj), "accessible-name");
      }
      /*
       * The label is the only property which can be changed
       */
      g_signal_emit_by_name (atk_obj, "visible_data_changed");
    }
}

static void
gail_button_notify_weak_ref (gpointer data, GObject* obj)
{
  GtkLabel *label = NULL;

  AtkObject* atk_obj = ATK_OBJECT (obj);
  if (data && GTK_IS_WIDGET (data))
    {
      label = GTK_LABEL (data);
      if (label)
        {
          g_signal_handlers_disconnect_by_func (label,
                                                (GCallback) gail_button_notify_label_gtk,
                                                GAIL_BUTTON (atk_obj));
          g_object_weak_unref (G_OBJECT (label),
                               gail_button_notify_label_weak_ref,
                               GAIL_BUTTON (atk_obj));
        }
    }
}

static void
gail_button_notify_label_weak_ref (gpointer data, GObject* obj)
{
  GtkLabel *label = NULL;
  GailButton *button = NULL;

  label = GTK_LABEL (obj);
  if (data && GAIL_IS_BUTTON (data))
    {
      button = GAIL_BUTTON (ATK_OBJECT (data));
      if (button)
        g_object_weak_unref (G_OBJECT (button), gail_button_notify_weak_ref,
                             label);
    }
}


static void
gail_button_init_textutil (GailButton  *button,
                           GtkWidget   *label)
{
  const gchar *label_text;

  if (button->textutil)
    g_object_unref (button->textutil);
  button->textutil = gail_text_util_new ();
  label_text = gtk_label_get_text (GTK_LABEL (label));
  gail_text_util_text_setup (button->textutil, label_text);
  g_object_weak_ref (G_OBJECT (button),
                     gail_button_notify_weak_ref, label);
  g_object_weak_ref (G_OBJECT (label),
                     gail_button_notify_label_weak_ref, button);
  g_signal_connect (label,
                    "notify",
                    (GCallback) gail_button_notify_label_gtk,
                    button);     
}

static gint
gail_button_real_add_gtk (GtkContainer *container,
                          GtkWidget    *widget,
                          gpointer     data)
{
  GtkLabel *label;
  GailButton *button;

  if (GTK_IS_LABEL (widget))
    {
      const gchar* label_text;

      label = GTK_LABEL (widget);


      button = GAIL_BUTTON (data);
      if (!button->textutil)
        gail_button_init_textutil (button, widget);
      else
        {
          label_text = gtk_label_get_text (label);
          gail_text_util_text_setup (button->textutil, label_text);
        }
    }

  return 1;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gail_button_do_action;
  iface->get_n_actions = gail_button_get_n_actions;
  iface->get_description = gail_button_get_description;
  iface->get_keybinding = gail_button_get_keybinding;
  iface->get_name = gail_button_action_get_name;
  iface->set_description = gail_button_set_description;
}

static gboolean
gail_button_do_action (AtkAction *action,
                       gint      i)
{
  GtkWidget *widget;
  GailButton *button;
  gboolean return_value = TRUE;

  widget = GTK_ACCESSIBLE (action)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  button = GAIL_BUTTON (action); 

  switch (i)
    {
    case 0:
    case 1:
    case 2:
      if (!button->action_queue) 
	{
	  button->action_queue = g_queue_new ();
	}
      g_queue_push_head (button->action_queue, GINT_TO_POINTER(i));
      if (!button->action_idle_handler)
	button->action_idle_handler = gdk_threads_add_idle (idle_do_action, button);
      break;
    default:
      return_value = FALSE;
      break;
    }
  return return_value; 
}

static gboolean
idle_do_action (gpointer data)
{
  GtkButton *button; 
  GtkWidget *widget;
  GailButton *gail_button;
  GdkEvent tmp_event;

  gail_button = GAIL_BUTTON (data);
  gail_button->action_idle_handler = 0;
  widget = GTK_ACCESSIBLE (gail_button)->widget;
  tmp_event.button.type = GDK_BUTTON_RELEASE;
  tmp_event.button.window = widget->window;
  tmp_event.button.button = 1;
  tmp_event.button.send_event = TRUE;
  tmp_event.button.time = GDK_CURRENT_TIME;
  tmp_event.button.axes = NULL;

  g_object_ref (gail_button);

  if (widget == NULL /* State is defunct */ ||
      !gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    {
      g_object_unref (gail_button);
      return FALSE;
    }
  else
    gtk_widget_event (widget, &tmp_event);

  button = GTK_BUTTON (widget); 
  while (!g_queue_is_empty (gail_button->action_queue)) 
    {
      gint action_number = GPOINTER_TO_INT(g_queue_pop_head (gail_button->action_queue));
      if (gail_button->default_is_press)
        {
          if (action_number == 0)
            action_number = 1;
          else if (action_number == 1)
            action_number = 0;
        }
      switch (action_number)
	{
	case 0:
	  /* first a press */ 

	  button->in_button = TRUE;
	  g_signal_emit_by_name (button, "enter");
	  /*
	   * Simulate a button press event. calling gtk_button_pressed() does
	   * not get the job done for a GtkOptionMenu.  
	   */
	  tmp_event.button.type = GDK_BUTTON_PRESS;
	  tmp_event.button.window = widget->window;
	  tmp_event.button.button = 1;
	  tmp_event.button.send_event = TRUE;
	  tmp_event.button.time = GDK_CURRENT_TIME;
	  tmp_event.button.axes = NULL;
	  
	  gtk_widget_event (widget, &tmp_event);

	  /* then a release */
	  tmp_event.button.type = GDK_BUTTON_RELEASE;
	  gtk_widget_event (widget, &tmp_event);
	  button->in_button = FALSE;
	  g_signal_emit_by_name (button, "leave");
	  break;
	case 1:
	  button->in_button = TRUE;
	  g_signal_emit_by_name (button, "enter");
	  /*
	   * Simulate a button press event. calling gtk_button_pressed() does
	   * not get the job done for a GtkOptionMenu.  
	   */
	  tmp_event.button.type = GDK_BUTTON_PRESS;
	  tmp_event.button.window = widget->window;
	  tmp_event.button.button = 1;
	  tmp_event.button.send_event = TRUE;
	  tmp_event.button.time = GDK_CURRENT_TIME;
	  tmp_event.button.axes = NULL;
	  
	  gtk_widget_event (widget, &tmp_event);
	  break;
	case 2:
	  button->in_button = FALSE;
	  g_signal_emit_by_name (button, "leave");
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
    }
  g_object_unref (gail_button);
  return FALSE;
}

static gint
gail_button_get_n_actions (AtkAction *action)
{
  return 3;
}

static const gchar*
gail_button_get_description (AtkAction *action,
                             gint      i)
{
  GailButton *button;
  const gchar *return_value;

  button = GAIL_BUTTON (action);

  if (button->default_is_press)
    {
      if (i == 0)
        i = 1;
      else if (i == 1)
        i = 0;
    }
  switch (i)
    {
    case 0:
      return_value = button->click_description;
      break;
    case 1:
      return_value = button->press_description;
      break;
    case 2:
      return_value = button->release_description;
      break;
    default:
      return_value = NULL;
      break;
    }
  return return_value; 
}

static const gchar*
gail_button_get_keybinding (AtkAction *action,
                            gint      i)
{
  GailButton *button;
  gchar *return_value = NULL;

  button = GAIL_BUTTON (action);
  if (button->default_is_press)
    {
      if (i == 0)
        i = 1;
      else if (i == 1)
        i = 0;
    }
  switch (i)
    {
    case 0:
      {
        /*
         * We look for a mnemonic on the label
         */
        GtkWidget *widget;
        GtkWidget *label;
        guint key_val; 

        widget = GTK_ACCESSIBLE (button)->widget;
        if (widget == NULL)
          /*
           * State is defunct
           */
          return NULL;

        g_return_val_if_fail (GTK_IS_BUTTON (widget), NULL);

        label = get_label_from_button (widget, 0, FALSE);
        if (GTK_IS_LABEL (label))
          {
            key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label)); 
            if (key_val != GDK_VoidSymbol)
              return_value = gtk_accelerator_name (key_val, GDK_MOD1_MASK);
          }
        if (return_value == NULL)
          {
            /* Find labelled-by relation */
            AtkRelationSet *set;
            AtkRelation *relation;
            GPtrArray *target;
            gpointer target_object;

            set = atk_object_ref_relation_set (ATK_OBJECT (action));
            if (set)
              {
                relation = atk_relation_set_get_relation_by_type (set, ATK_RELATION_LABELLED_BY);
                if (relation)
                  {              
                    target = atk_relation_get_target (relation);
            
                    target_object = g_ptr_array_index (target, 0);
                    if (GTK_IS_ACCESSIBLE (target_object))
                      {
                        label = GTK_ACCESSIBLE (target_object)->widget;
                      } 
                  }
                g_object_unref (set);
              }

            if (GTK_IS_LABEL (label))
              {
                key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label)); 
                if (key_val != GDK_VoidSymbol)
                  return_value = gtk_accelerator_name (key_val, GDK_MOD1_MASK);
              }
          }
        g_free (button->click_keybinding);
        button->click_keybinding = return_value;
        break;
      }
    default:
      break;
    }
  return return_value; 
}

static const gchar*
gail_button_action_get_name (AtkAction *action,
                             gint      i)
{
  const gchar *return_value;
  GailButton *button;

  button = GAIL_BUTTON (action);

  if (button->default_is_press)
    {
      if (i == 0)
        i = 1;
      else if (i == 1)
        i = 0;
    }
  switch (i)
    {
    case 0:
      /*
       * This action is a "click" to activate a button or "toggle" to change
       * the state of a toggle button check box or radio button.
       */ 
      return_value = "click";
      break;
    case 1:
      /*
       * This action simulates a button press by simulating moving the
       * mouse into the button followed by pressing the left mouse button.
       */
      return_value = "press";
      break;
    case 2:
      /*
       * This action simulates releasing the left mouse button outside the 
       * button.
       *
       * To simulate releasing the left mouse button inside the button use
       * the click action.
       */
      return_value = "release";
      break;
    default:
      return_value = NULL;
      break;
    }
  return return_value; 
}

static gboolean
gail_button_set_description (AtkAction      *action,
                             gint           i,
                             const gchar    *desc)
{
  GailButton *button;
  gchar **value;

  button = GAIL_BUTTON (action);

  if (button->default_is_press)
    {
      if (i == 0)
        i = 1;
      else if (i == 1)
        i = 0;
    }
  switch (i)
    {
    case 0:
      value = &button->click_description;
      break;
    case 1:
      value = &button->press_description;
      break;
    case 2:
      value = &button->release_description;
      break;
    default:
      value = NULL;
      break;
    }
  if (value)
    {
      g_free (*value);
      *value = g_strdup (desc);
      return TRUE;
    }
  else
    return FALSE;
}

static gint
gail_button_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  gint n_children;

  g_return_val_if_fail (GAIL_IS_BUTTON (obj), 0);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return 0;

  /*
   * Check whether we have an attached menus for PanelMenuButton
   */
  n_children = get_n_attached_menus (widget);
  if (n_children > 0)
    return n_children;

  n_children = get_n_labels_from_button (widget);
  if (n_children <= 1)
    n_children = 0;

  return n_children;
}

static AtkObject*
gail_button_ref_child (AtkObject *obj,
                       gint      i)
{
  GtkWidget *widget;
  GtkWidget *child_widget;
  AtkObject *child;

  g_return_val_if_fail (GAIL_IS_BUTTON (obj), NULL);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  if (i >= gail_button_get_n_children (obj))
    return NULL;

  if (get_n_attached_menus (widget) > 0)
  {
    child_widget = get_nth_attached_menu (widget, i);
  }
  else
    child_widget = NULL;    

  if (!child_widget) 
    {
      if (get_n_labels_from_button (widget) > 1)
        {
          child_widget = get_label_from_button (widget, i, TRUE);
        }
    }

  if (child_widget)
    {
      child = gtk_widget_get_accessible (child_widget);
      g_object_ref (child);
    }
  else
    child = NULL;

  return child;
}

static AtkStateSet*
gail_button_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gail_button_parent_class)->ref_state_set (obj);
  widget = GTK_ACCESSIBLE (obj)->widget;

  if (widget == NULL)
    return state_set;

  if (gtk_widget_get_state (widget) == GTK_STATE_ACTIVE)
    atk_state_set_add_state (state_set, ATK_STATE_ARMED);

  if (!gtk_widget_get_can_focus (widget))
    atk_state_set_remove_state (state_set, ATK_STATE_SELECTABLE);


  return state_set;
}

/*
 * This is the signal handler for the "pressed" or "enter" signal handler
 * on the GtkButton.
 *
 * If the state is now GTK_STATE_ACTIVE we notify a property change
 */
static void
gail_button_pressed_enter_handler (GtkWidget       *widget)
{
  AtkObject *accessible;

  if (gtk_widget_get_state (widget) == GTK_STATE_ACTIVE)
    {
      accessible = gtk_widget_get_accessible (widget);
      atk_object_notify_state_change (accessible, ATK_STATE_ARMED, TRUE);
      GAIL_BUTTON (accessible)->state = GTK_STATE_ACTIVE;
    }
}

/*
 * This is the signal handler for the "released" or "leave" signal handler
 * on the GtkButton.
 *
 * If the state was GTK_STATE_ACTIVE we notify a property change
 */
static void
gail_button_released_leave_handler (GtkWidget       *widget)
{
  AtkObject *accessible;

  accessible = gtk_widget_get_accessible (widget);
  if (GAIL_BUTTON (accessible)->state == GTK_STATE_ACTIVE)
    {
      atk_object_notify_state_change (accessible, ATK_STATE_ARMED, FALSE);
      GAIL_BUTTON (accessible)->state = GTK_STATE_NORMAL;
    }
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  iface->get_image_description = gail_button_get_image_description;
  iface->get_image_position = gail_button_get_image_position;
  iface->get_image_size = gail_button_get_image_size;
  iface->set_image_description = gail_button_set_image_description;
}

static GtkImage*
get_image_from_button (GtkWidget *button)
{
  GtkWidget *child;
  GList *list;
  GtkImage *image = NULL;

  child = gtk_bin_get_child (GTK_BIN (button));
  if (GTK_IS_IMAGE (child))
    image = GTK_IMAGE (child);
  else
    {
      if (GTK_IS_ALIGNMENT (child))
        child = gtk_bin_get_child (GTK_BIN (child));
      if (GTK_IS_CONTAINER (child))
        {
          list = gtk_container_get_children (GTK_CONTAINER (child));
          if (!list)
            return NULL;
          if (GTK_IS_IMAGE (list->data))
            image = GTK_IMAGE (list->data);
          g_list_free (list);
        }
    }

  return image;
}

static const gchar*
gail_button_get_image_description (AtkImage *image) {

  GtkWidget *widget;
  GtkImage  *button_image;
  AtkObject *obj;

  widget = GTK_ACCESSIBLE (image)->widget;

  if (widget == NULL)
    /*
     * State is defunct
     */
    return NULL;

  button_image = get_image_from_button (widget);

  if (button_image != NULL)
    {
      obj = gtk_widget_get_accessible (GTK_WIDGET (button_image));
      return atk_image_get_image_description (ATK_IMAGE (obj));
    }
  else 
    return NULL;
}

static void
gail_button_get_image_position (AtkImage     *image,
                                gint	     *x,
                                gint	     *y,
                                AtkCoordType coord_type)
{
  GtkWidget *widget;
  GtkImage  *button_image;
  AtkObject *obj;

  widget = GTK_ACCESSIBLE (image)->widget;

  if (widget == NULL)
    {
    /*
     * State is defunct
     */
      *x = G_MININT;
      *y = G_MININT;
      return;
    }

  button_image = get_image_from_button (widget);

  if (button_image != NULL)
    {
      obj = gtk_widget_get_accessible (GTK_WIDGET (button_image));
      atk_component_get_position (ATK_COMPONENT (obj), x, y, coord_type); 
    }
  else
    {
      *x = G_MININT;
      *y = G_MININT;
    }
}

static void
gail_button_get_image_size (AtkImage *image,
                            gint     *width,
                            gint     *height)
{
  GtkWidget *widget;
  GtkImage  *button_image;
  AtkObject *obj;

  widget = GTK_ACCESSIBLE (image)->widget;

  if (widget == NULL)
    {
    /*
     * State is defunct
     */
      *width = -1;
      *height = -1;
      return;
    }

  button_image = get_image_from_button (widget);

  if (button_image != NULL)
    {
      obj = gtk_widget_get_accessible (GTK_WIDGET (button_image));
      atk_image_get_image_size (ATK_IMAGE (obj), width, height); 
    }
  else
    {
      *width = -1;
      *height = -1;
    }
}

static gboolean
gail_button_set_image_description (AtkImage    *image,
                                   const gchar *description)
{
  GtkWidget *widget;
  GtkImage  *button_image;
  AtkObject *obj;

  widget = GTK_ACCESSIBLE (image)->widget;

  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  button_image = get_image_from_button (widget);

  if (button_image != NULL) 
    {
      obj = gtk_widget_get_accessible (GTK_WIDGET (button_image));
      return atk_image_set_image_description (ATK_IMAGE (obj), description);
    }
  else 
    return FALSE;
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_button_get_text;
  iface->get_character_at_offset = gail_button_get_character_at_offset;
  iface->get_text_before_offset = gail_button_get_text_before_offset;
  iface->get_text_at_offset = gail_button_get_text_at_offset;
  iface->get_text_after_offset = gail_button_get_text_after_offset;
  iface->get_character_count = gail_button_get_character_count;
  iface->get_character_extents = gail_button_get_character_extents;
  iface->get_offset_at_point = gail_button_get_offset_at_point;
  iface->get_run_attributes = gail_button_get_run_attributes;
  iface->get_default_attributes = gail_button_get_default_attributes;
}

static gchar*
gail_button_get_text (AtkText *text,
                      gint    start_pos,
                      gint    end_pos)
{
  GtkWidget *widget;
  GtkWidget  *label;
  GailButton *button;
  const gchar *label_text;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL (label))
    return NULL;

  button = GAIL_BUTTON (text);
  if (!button->textutil) 
    gail_button_init_textutil (button, label);

  label_text = gtk_label_get_text (GTK_LABEL (label));

  if (label_text == NULL)
    return NULL;
  else
  {
    return gail_text_util_get_substring (button->textutil, 
                                         start_pos, end_pos);
  }
}

static gchar*
gail_button_get_text_before_offset (AtkText         *text,
				    gint            offset,
				    AtkTextBoundary boundary_type,
				    gint            *start_offset,
				    gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailButton *button;
  
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return NULL;

  button = GAIL_BUTTON (text);
  if (!button->textutil)
    gail_button_init_textutil (button, label);

  return gail_text_util_get_text (button->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_BEFORE_OFFSET, 
                           boundary_type, offset, start_offset, end_offset); 
}

static gchar*
gail_button_get_text_at_offset (AtkText         *text,
			        gint            offset,
			        AtkTextBoundary boundary_type,
 			        gint            *start_offset,
			        gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailButton *button;
 
  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return NULL;

  button = GAIL_BUTTON (text);
  if (!button->textutil)
    gail_button_init_textutil (button, label);

  return gail_text_util_get_text (button->textutil,
                              gtk_label_get_layout (GTK_LABEL (label)), GAIL_AT_OFFSET, 
                              boundary_type, offset, start_offset, end_offset);
}

static gchar*
gail_button_get_text_after_offset (AtkText         *text,
				   gint            offset,
				   AtkTextBoundary boundary_type,
				   gint            *start_offset,
				   gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailButton *button;

  widget = GTK_ACCESSIBLE (text)->widget;
  
  if (widget == NULL)
  {
    /* State is defunct */
    return NULL;
  }
  
  /* Get label */
  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return NULL;

  button = GAIL_BUTTON (text);
  if (!button->textutil)
    gail_button_init_textutil (button, label);

  return gail_text_util_get_text (button->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_AFTER_OFFSET, 
                           boundary_type, offset, start_offset, end_offset);
}

static gint
gail_button_get_character_count (AtkText *text)
{
  GtkWidget *widget;
  GtkWidget *label;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return 0;

  return g_utf8_strlen (gtk_label_get_text (GTK_LABEL (label)), -1);
}

static void
gail_button_get_character_extents (AtkText      *text,
				   gint         offset,
		                   gint         *x,
                    		   gint 	*y,
                                   gint 	*width,
                                   gint 	*height,
			           AtkCoordType coords)
{
  GtkWidget *widget;
  GtkWidget *label;
  PangoRectangle char_rect;
  gint index, x_layout, y_layout;
  const gchar *label_text;
 
  widget = GTK_ACCESSIBLE (text)->widget;

  if (widget == NULL)
    /* State is defunct */
    return;

  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return;
  
  gtk_label_get_layout_offsets (GTK_LABEL (label), &x_layout, &y_layout);
  label_text = gtk_label_get_text (GTK_LABEL (label));
  index = g_utf8_offset_to_pointer (label_text, offset) - label_text;
  pango_layout_index_to_pos (gtk_label_get_layout (GTK_LABEL (label)), index, &char_rect);
  
  gail_misc_get_extents_from_pango_rectangle (label, &char_rect, 
                    x_layout, y_layout, x, y, width, height, coords);
} 

static gint 
gail_button_get_offset_at_point (AtkText      *text,
                                 gint         x,
                                 gint         y,
			         AtkCoordType coords)
{ 
  GtkWidget *widget;
  GtkWidget *label;
  gint index, x_layout, y_layout;
  const gchar *label_text;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return -1;
  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return -1;
  
  gtk_label_get_layout_offsets (GTK_LABEL (label), &x_layout, &y_layout);
  
  index = gail_misc_get_index_at_point_in_layout (label, 
                                              gtk_label_get_layout (GTK_LABEL (label)), 
                                              x_layout, y_layout, x, y, coords);
  label_text = gtk_label_get_text (GTK_LABEL (label));
  if (index == -1)
    {
      if (coords == ATK_XY_WINDOW || coords == ATK_XY_SCREEN)
        return g_utf8_strlen (label_text, -1);

      return index;  
    }
  else
    return g_utf8_pointer_to_offset (label_text, label_text + index);  
}

static AtkAttributeSet*
gail_button_get_run_attributes (AtkText        *text,
                                gint 	      offset,
                                gint 	      *start_offset,
	                        gint	      *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkAttributeSet *at_set = NULL;
  GtkJustification justify;
  GtkTextDirection dir;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return NULL;
  
  /* Get values set for entire label, if any */
  justify = gtk_label_get_justify (GTK_LABEL (label));
  if (justify != GTK_JUSTIFY_CENTER)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_JUSTIFICATION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_JUSTIFICATION, justify)));
    }
  dir = gtk_widget_get_direction (label);
  if (dir == GTK_TEXT_DIR_RTL)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_DIRECTION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, dir)));
    }

  at_set = gail_misc_layout_get_run_attributes (at_set,
                                                gtk_label_get_layout (GTK_LABEL (label)),
                                                (gchar *) gtk_label_get_text (GTK_LABEL (label)),
                                                offset,
                                                start_offset,
                                                end_offset);
  return at_set;
}

static AtkAttributeSet*
gail_button_get_default_attributes (AtkText        *text)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkAttributeSet *at_set = NULL;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return NULL;

  at_set = gail_misc_get_default_attributes (at_set,
                                             gtk_label_get_layout (GTK_LABEL (label)),
                                             widget);
  return at_set;
}

static gunichar 
gail_button_get_character_at_offset (AtkText	         *text,
                                     gint	         offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  const gchar *string;
  gchar *index;

  widget = GTK_ACCESSIBLE (text)->widget;
  if (widget == NULL)
    /* State is defunct */
    return '\0';

  label = get_label_from_button (widget, 0, FALSE);

  if (!GTK_IS_LABEL(label))
    return '\0';
  string = gtk_label_get_text (GTK_LABEL (label));
  if (offset >= g_utf8_strlen (string, -1))
    return '\0';
  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}

static void
gail_button_finalize (GObject            *object)
{
  GailButton *button = GAIL_BUTTON (object);

  g_free (button->click_description);
  g_free (button->press_description);
  g_free (button->release_description);
  g_free (button->click_keybinding);
  if (button->action_idle_handler)
    {
      g_source_remove (button->action_idle_handler);
      button->action_idle_handler = 0;
    }
  if (button->action_queue)
    {
      g_queue_free (button->action_queue);
    }
  if (button->textutil)
    {
      g_object_unref (button->textutil);
    }
  G_OBJECT_CLASS (gail_button_parent_class)->finalize (object);
}

static GtkWidget*
find_label_child (GtkContainer *container,
                  gint         *index,
                  gboolean     allow_many)
{
  GList *children, *tmp_list;
  GtkWidget *child;
 
  children = gtk_container_get_children (container);

  child = NULL;
  for (tmp_list = children; tmp_list != NULL; tmp_list = tmp_list->next) 
    {
      if (GTK_IS_LABEL (tmp_list->data))
        {
          if (!allow_many)
            {
              if (child)
                {
                  child = NULL;
                  break;
                }
              child = GTK_WIDGET (tmp_list->data);
            }
          else
            {
              if (*index == 0)
                {
                  child = GTK_WIDGET (tmp_list->data);
                  break;
                }
              (*index)--;
	    }
        }
       /*
        * Label for button which are GtkTreeView column headers are in a 
        * GtkHBox in a GtkAlignment.
        */
      else if (GTK_IS_ALIGNMENT (tmp_list->data))
        {
          GtkWidget *widget;

          widget = gtk_bin_get_child (GTK_BIN (tmp_list->data));
          if (GTK_IS_LABEL (widget))
            {
              if (!allow_many)
                {
                  if (child)
                    {
                      child = NULL;
                      break;
                    }
                  child = widget;
                }
              else
                {
                  if (*index == 0)
                    {
	              child = widget;
                      break;
                    }
                  (*index)--;
	        }
	    }
        }
      else if (GTK_IS_CONTAINER (tmp_list->data))
        {
          child = find_label_child (GTK_CONTAINER (tmp_list->data), index, allow_many);
          if (child)
            break;
        } 
    }
  g_list_free (children);
  return child;
}

static GtkWidget*
get_label_from_button (GtkWidget *button,
                       gint      index,
                       gboolean  allow_many)
{
  GtkWidget *child;

  if (index > 0 && !allow_many)
    g_warning ("Inconsistent values passed to get_label_from_button");

  child = gtk_bin_get_child (GTK_BIN (button));
  if (GTK_IS_ALIGNMENT (child))
    child = gtk_bin_get_child (GTK_BIN (child));

  if (GTK_IS_CONTAINER (child))
    child = find_label_child (GTK_CONTAINER (child), &index, allow_many);
  else if (!GTK_IS_LABEL (child))
    child = NULL;

  return child;
}

static void
count_labels (GtkContainer *container,
              gint         *n_labels)
{
  GList *children, *tmp_list;
 
  children = gtk_container_get_children (container);

  for (tmp_list = children; tmp_list != NULL; tmp_list = tmp_list->next) 
    {
      if (GTK_IS_LABEL (tmp_list->data))
        {
          (*n_labels)++;
        }
       /*
        * Label for button which are GtkTreeView column headers are in a 
        * GtkHBox in a GtkAlignment.
        */
      else if (GTK_IS_ALIGNMENT (tmp_list->data))
        {
          GtkWidget *widget;

          widget = gtk_bin_get_child (GTK_BIN (tmp_list->data));
          if (GTK_IS_LABEL (widget))
            (*n_labels)++;
        }
      else if (GTK_IS_CONTAINER (tmp_list->data))
        {
          count_labels (GTK_CONTAINER (tmp_list->data), n_labels);
        } 
    }
  g_list_free (children);
}

static gint
get_n_labels_from_button (GtkWidget *button)
{
  GtkWidget *child;
  gint n_labels;

  n_labels = 0;

  child = gtk_bin_get_child (GTK_BIN (button));
  if (GTK_IS_ALIGNMENT (child))
    child = gtk_bin_get_child (GTK_BIN (child));

  if (GTK_IS_CONTAINER (child))
    count_labels (GTK_CONTAINER (child), &n_labels);

  return n_labels;
}

static void
set_role_for_button (AtkObject *accessible,
                     GtkWidget *button)
{
  GtkWidget *parent;
  AtkRole role;

  parent = gtk_widget_get_parent (button);
  if (GTK_IS_TREE_VIEW (parent))
    {
      role = ATK_ROLE_TABLE_COLUMN_HEADER;
      /*
       * Even though the accessible parent of the column header will
       * be reported as the table because the parent widget of the
       * GtkTreeViewColumn's button is the GtkTreeView we set
       * the accessible parent for column header to be the table
       * to ensure that atk_object_get_index_in_parent() returns
       * the correct value; see gail_widget_get_index_in_parent().
       */
      atk_object_set_parent (accessible, gtk_widget_get_accessible (parent));
    }
  else
    role = ATK_ROLE_PUSH_BUTTON;

  accessible->role =  role;
}

static gint
get_n_attached_menus (GtkWidget  *widget)
{
  GList *list_menus;
  
  if (widget == NULL)
    return 0;

  list_menus = g_object_get_data (G_OBJECT (widget), GAIL_BUTTON_ATTACHED_MENUS);
  if (list_menus == NULL)
    return 0;

  return g_list_length (list_menus);
}

static GtkWidget*
get_nth_attached_menu (GtkWidget  *widget,
                       gint       index)
{
  GtkWidget *attached_menu;
  GList *list_menus;

  if (widget == NULL)
    return NULL;

  list_menus = g_object_get_data (G_OBJECT (widget), GAIL_BUTTON_ATTACHED_MENUS);
  if (list_menus == NULL ||
      index >= g_list_length (list_menus))
    return NULL;

  attached_menu = (GtkWidget *) g_list_nth_data (list_menus, index);

  return attached_menu;
}
