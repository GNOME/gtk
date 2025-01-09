/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2024 Red Hat, Inc.
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

#pragma once

#include "gtkenums.h"

G_BEGIN_DECLS

char **      gtk_text_encoding_get_names  (void);
char **      gtk_text_encoding_get_labels (void);
const char * gtk_text_encoding_from_name  (const char *name);

char **      gtk_line_ending_get_names    (void);
char **      gtk_line_ending_get_labels   (void);
const char * gtk_line_ending_from_name    (const char *name);

G_END_DECLS
