/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2022 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkchoice.h"

struct _GtkChoice
{
  GObject parent_instance;

  char *label;
  char **options;
};

G_DEFINE_TYPE (GtkChoice, gtk_choice, G_TYPE_OBJECT)

static void
gtk_choice_init (GtkChoice *self)
{
}

static void
gtk_choice_finalize (GObject *object)
{
  GtkChoice *self = GTK_CHOICE (object);

  g_free (self->label);
  g_strfreev (self->options);

  G_OBJECT_CLASS (gtk_choice_parent_class)->finalize (object);
}

static void
gtk_choice_class_init (GtkChoiceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_choice_finalize;
}

/* }}} */
/* {{{ Public API */

GtkChoice *
gtk_choice_new (const char         *label,
                const char * const *options)
{
  GtkChoice *self;

  self = g_object_new (GTK_TYPE_CHOICE, NULL);

  self->label = g_strdup (label);
  self->options = g_strdupv ((char **)options);

  return self;
}

const char *
gtk_choice_get_label (GtkChoice *choice)
{
  g_return_val_if_fail (GTK_IS_CHOICE (choice), NULL);

  return choice->label;
}

const char * const *
gtk_choice_get_options (GtkChoice *choice)
{
  g_return_val_if_fail (GTK_IS_CHOICE (choice), NULL);

  return (const char * const *)choice->options;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
