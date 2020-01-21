/* ATK -  Accessibility Toolkit
 * Copyright 2001 Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __ATK_OBJECT_H__
#define __ATK_OBJECT_H__

#if defined(ATK_DISABLE_SINGLE_INCLUDES) && !defined (__ATK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <atk/atk.h> can be included directly."
#endif

#include <atk/atktypes.h>
#include <atk/atkstate.h>

G_BEGIN_DECLS

/**
 * AtkAttributeSet:
 *
 * This is a singly-linked list (a #GSList) of #AtkAttribute. It is
 * used by atk_text_get_run_attributes(),
 * atk_text_get_default_attributes(),
 * atk_editable_text_set_run_attributes(),
 * atk_document_get_attributes() and atk_object_get_attributes()
 **/
typedef GSList AtkAttributeSet;

/**
 * AtkAttribute:
 * @name: The attribute name.
 * @value: the value of the attribute, represented as a string.
 *
 * AtkAttribute is a string name/value pair representing a generic
 * attribute. This can be used to expose additional information from
 * an accessible object as a whole (see atk_object_get_attributes())
 * or an document (see atk_document_get_attributes()). In the case of
 * text attributes (see atk_text_get_default_attributes()),
 * #AtkTextAttribute enum defines all the possible text attribute
 * names. You can use atk_text_attribute_get_name() to get the string
 * name from the enum value. See also atk_text_attribute_for_name()
 * and atk_text_attribute_get_value() for more information.
 *
 * A string name/value pair representing a generic attribute.
 **/
typedef struct _AtkAttribute AtkAttribute;

struct _AtkAttribute {
  gchar* name;
  gchar* value;
};

#define ATK_TYPE_OBJECT                           (atk_object_get_type ())
#define ATK_OBJECT(obj)                           (G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_OBJECT, AtkObject))
#define ATK_OBJECT_CLASS(klass)                   (G_TYPE_CHECK_CLASS_CAST ((klass), ATK_TYPE_OBJECT, AtkObjectClass))
#define ATK_IS_OBJECT(obj)                        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_OBJECT))
#define ATK_IS_OBJECT_CLASS(klass)                (G_TYPE_CHECK_CLASS_TYPE ((klass), ATK_TYPE_OBJECT))
#define ATK_OBJECT_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj), ATK_TYPE_OBJECT, AtkObjectClass))

#define ATK_TYPE_IMPLEMENTOR                      (atk_implementor_get_type ())
#define ATK_IS_IMPLEMENTOR(obj)                   G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_IMPLEMENTOR)
#define ATK_IMPLEMENTOR(obj)                      G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_IMPLEMENTOR, AtkImplementor)
#define ATK_IMPLEMENTOR_GET_IFACE(obj)            (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ATK_TYPE_IMPLEMENTOR, AtkImplementorIface))


typedef struct _AtkImplementor            AtkImplementor; /* dummy typedef */
typedef struct _AtkImplementorIface       AtkImplementorIface;


typedef struct _AtkObject                 AtkObject;
typedef struct _AtkObjectClass            AtkObjectClass;
typedef struct _AtkRelationSet            AtkRelationSet;
typedef struct _AtkStateSet               AtkStateSet;

/**
 * AtkPropertyValues:
 * @property_name: The name of the ATK property which has changed.
 * @old_value: NULL. This field is not used anymore.
 * @new_value: The new value of the named property.
 *
 * Note: @old_value field of #AtkPropertyValues will not contain a
 * valid value. This is a field defined with the purpose of contain
 * the previous value of the property, but is not used anymore.
 *
 **/
struct _AtkPropertyValues
{
  const gchar  *property_name;
  GValue old_value;
  GValue new_value;
};

typedef struct _AtkPropertyValues        AtkPropertyValues;

/**
 * AtkFunction:
 * @user_data: custom data defined by the user
 *
 * An AtkFunction is a function definition used for padding which has
 * been added to class and interface structures to allow for expansion
 * in the future.
 *
 * Returns: not used
 */
typedef gboolean (*AtkFunction)          (gpointer user_data);
/*
 * For most properties the old_value field of AtkPropertyValues will
 * not contain a valid value.
 *
 * Currently, the only property for which old_value is used is
 * accessible-state; for instance if there is a focus state the
 * property change handler will be called for the object which lost the focus
 * with the old_value containing an AtkState value corresponding to focused
 * and the property change handler will be called for the object which
 * received the focus with the new_value containing an AtkState value
 * corresponding to focused.
 */

struct _AtkObject
{
  GObject parent;

  gchar *description;
  gchar *name;
  AtkObject *accessible_parent;
  AtkRole role;
  AtkRelationSet *relation_set;
  AtkLayer layer;
};


/**
 * AtkObjectClass:
 * @connect_property_change_handler: specifies a function to be called
 *   when a property changes value. This virtual function is
 *   deprecated since 2.12 and it should not be overriden. Connect
 *   directly to property-change or notify signal instead.
 * @remove_property_change_handler: removes a property changed handler
 *   as returned by @connect_property_change_handler. This virtual
 *   function is deprecated sice 2.12 and it should not be overriden.
 * @focus_event: The signal handler which is executed when there is a
 *   focus event for an object. This virtual function is deprecated
 *   since 2.9.4 and it should not be overriden. Use
 *   the #AtkObject::state-change "focused" signal instead.
 */
struct _AtkObjectClass
{
  GObjectClass parent;

  /*
   * Gets the accessible name of the object
   */
  const gchar*             (* get_name)            (AtkObject                *accessible);
  /*
   * Gets the accessible description of the object
   */
  const gchar*             (* get_description)     (AtkObject                *accessible);
  /*
   * Gets the accessible parent of the object
   */
  AtkObject*               (*get_parent)           (AtkObject                *accessible);

  /*
   * Gets the number of accessible children of the object
   */
  gint                    (* get_n_children)       (AtkObject                *accessible);
  /*
   * Returns a reference to the specified accessible child of the object.
   * The accessible children are 0-based so the first accessible child is
   * at index 0, the second at index 1 and so on.
   */
  AtkObject*              (* ref_child)            (AtkObject                *accessible,
                                                    gint                      i);
  /*
   * Gets the 0-based index of this object in its parent; returns -1 if the
   * object does not have an accessible parent.
   */
  gint                    (* get_index_in_parent) (AtkObject                 *accessible);
  /*
   * Gets the RelationSet associated with the object
   */
  AtkRelationSet*         (* ref_relation_set)    (AtkObject                 *accessible);
  /*
   * Gets the role of the object
   */
  AtkRole                 (* get_role)            (AtkObject                 *accessible);
  AtkLayer                (* get_layer)           (AtkObject                 *accessible);
  gint                    (* get_mdi_zorder)      (AtkObject                 *accessible);
  /*
   * Gets the state set of the object
   */
  AtkStateSet*            (* ref_state_set)       (AtkObject                 *accessible);
  /*
   * Sets the accessible name of the object
   */
  void                    (* set_name)            (AtkObject                 *accessible,
                                                   const gchar               *name);
  /*
   * Sets the accessible description of the object
   */
  void                    (* set_description)     (AtkObject                 *accessible,
                                                   const gchar               *description);
  /*
   * Sets the accessible parent of the object
   */
  void                    (* set_parent)          (AtkObject                 *accessible,
                                                   AtkObject                 *parent);
  /*
   * Sets the accessible role of the object
   */
  void                    (* set_role)            (AtkObject                 *accessible,
                                                   AtkRole                   role);
  /*
   * Removes a property change handler which was specified using
   * connect_property_change_handler
   */
void                      (* remove_property_change_handler)     (AtkObject
                *accessible,
                                                                  guint
                handler_id);
void                      (* initialize)                         (AtkObject                     *accessible,
                                                                  gpointer                      data);
  /*
   * The signal handler which is executed when there is a change in the
   * children of the object
   */
  void                    (* children_changed)    (AtkObject                  *accessible,
                                                   guint                      change_index,
                                                   gpointer                   changed_child);
  /*
   * The signal handler which is executed  when there is a focus event
   * for an object.
   */
  void                    (* focus_event)         (AtkObject                  *accessible,
                                                   gboolean                   focus_in);
  /*
   * The signal handler which is executed  when there is a property_change 
   * signal for an object.
   */
  void                    (* property_change)     (AtkObject                  *accessible,
                                                   AtkPropertyValues          *values);
  /*
   * The signal handler which is executed  when there is a state_change 
   * signal for an object.
   */
  void                    (* state_change)        (AtkObject                  *accessible,
                                                   const gchar                *name,
                                                   gboolean                   state_set);
  /*
   * The signal handler which is executed when there is a change in the
   * visible data for an object
   */
  void                    (*visible_data_changed) (AtkObject                  *accessible);

  /*
   * The signal handler which is executed when there is a change in the
   * 'active' child or children of the object, for instance when 
   * interior focus changes in a table or list.  This signal should be emitted
   * by objects whose state includes ATK_STATE_MANAGES_DESCENDANTS.
   */
  void                    (*active_descendant_changed) (AtkObject                  *accessible,
                                                        gpointer                   *child);

  /*    	
   * Gets a list of properties applied to this object as a whole, as an #AtkAttributeSet consisting of name-value pairs. 
   * Since ATK 1.12
   */
  AtkAttributeSet* 	  (*get_attributes)            (AtkObject                  *accessible);

  const gchar*            (*get_object_locale)         (AtkObject                  *accessible);

  AtkFunction             pad1;
};

GDK_AVAILABLE_IN_ALL
GType            atk_object_get_type   (void);

/**
 * AtkImplementorIface:
 *
 * The AtkImplementor interface is implemented by objects for which
 * AtkObject peers may be obtained via calls to
 * iface->(ref_accessible)(implementor);
 */
struct _AtkImplementorIface
{
  GTypeInterface parent;

  AtkObject*   (*ref_accessible) (AtkImplementor *implementor);
};

GDK_AVAILABLE_IN_ALL
GType atk_implementor_get_type (void);
GDK_AVAILABLE_IN_ALL
AtkObject*              atk_implementor_ref_accessible            (AtkImplementor *implementor);

/*
 * Properties directly supported by AtkObject
 */

GDK_AVAILABLE_IN_ALL
const gchar*            atk_object_get_name                       (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
const gchar*            atk_object_get_description                (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
AtkObject*              atk_object_get_parent                     (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
AtkObject*              atk_object_peek_parent                    (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
gint                    atk_object_get_n_accessible_children      (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
AtkObject*              atk_object_ref_accessible_child           (AtkObject *accessible,
                                                                   gint        i);
GDK_AVAILABLE_IN_ALL
AtkRelationSet*         atk_object_ref_relation_set               (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
AtkRole                 atk_object_get_role                       (AtkObject *accessible);

GDK_AVAILABLE_IN_ALL
AtkAttributeSet*        atk_object_get_attributes                 (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
AtkStateSet*            atk_object_ref_state_set                  (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
gint                    atk_object_get_index_in_parent            (AtkObject *accessible);
GDK_AVAILABLE_IN_ALL
void                    atk_object_set_name                       (AtkObject *accessible,
                                                                   const gchar *name);
GDK_AVAILABLE_IN_ALL
void                    atk_object_set_description                (AtkObject *accessible,
                                                                   const gchar *description);
GDK_AVAILABLE_IN_ALL
void                    atk_object_set_parent                     (AtkObject *accessible,
                                                                   AtkObject *parent);
GDK_AVAILABLE_IN_ALL
void                    atk_object_set_role                       (AtkObject *accessible,
                                                                   AtkRole   role);


GDK_AVAILABLE_IN_ALL
void                 atk_object_notify_state_change              (AtkObject                      *accessible,
                                                                  AtkState                       state,
                                                                  gboolean                       value);
GDK_AVAILABLE_IN_ALL
void                 atk_object_initialize                       (AtkObject                     *accessible,
                                                                  gpointer                      data);

GDK_AVAILABLE_IN_ALL
const gchar*          atk_role_get_name      (AtkRole         role);
GDK_AVAILABLE_IN_ALL
AtkRole               atk_role_for_name      (const gchar     *name);


GDK_AVAILABLE_IN_ALL
gboolean              atk_object_add_relationship              (AtkObject      *object,
								AtkRelationType relationship,
								AtkObject      *target);
GDK_AVAILABLE_IN_ALL
gboolean              atk_object_remove_relationship           (AtkObject      *object,
								AtkRelationType relationship,
								AtkObject      *target);
GDK_AVAILABLE_IN_ALL
const gchar*          atk_role_get_localized_name              (AtkRole     role);
GDK_AVAILABLE_IN_ALL
const gchar*          atk_object_get_object_locale             (AtkObject   *accessible);

GDK_AVAILABLE_IN_ALL
const gchar*          atk_object_get_accessible_id             (AtkObject   *accessible);

GDK_AVAILABLE_IN_ALL
void                  atk_object_set_accessible_id             (AtkObject   *accessible,
                                                                const gchar *name);

G_END_DECLS

#endif /* __ATK_OBJECT_H__ */
