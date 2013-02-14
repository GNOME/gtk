/* GTK+ - accessibility implementations
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtklockbuttonaccessibleprivate.h"

#include "gtk/gtklockbuttonprivate.h"
#include "gtk/gtkwidgetprivate.h"

G_DEFINE_TYPE (GtkLockButtonAccessible, gtk_lock_button_accessible, GTK_TYPE_BUTTON_ACCESSIBLE)

static const gchar *
gtk_lock_button_accessible_get_name (AtkObject *obj)
{
  GtkLockButton *lockbutton;

  lockbutton = GTK_LOCK_BUTTON (gtk_accessible_get_widget (GTK_ACCESSIBLE (obj)));
  if (lockbutton == NULL)
    return NULL;

  return _gtk_lock_button_get_current_text (lockbutton);
}

static void
gtk_lock_button_accessible_class_init (GtkLockButtonAccessibleClass *klass)
{
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

  atk_object_class->get_name = gtk_lock_button_accessible_get_name;
}

static void
gtk_lock_button_accessible_init (GtkLockButtonAccessible *lockbutton)
{
}

void
_gtk_lock_button_accessible_name_changed (GtkLockButton *lockbutton)
{
  AtkObject *obj;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (lockbutton));
  if (obj == NULL)
    return;

  g_object_notify (G_OBJECT (obj), "accessible-name");
}

