/* gtktimingfunctionprivate.h: Private declarations for GtkTimingFunction
 *
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gtktimingfunction.h"
#include "gtkcssparserprivate.h"

G_BEGIN_DECLS

gboolean        gtk_timing_function_parser_parse        (GtkCssParser       *parser,
                                                         GtkTimingFunction **out_function);

G_END_DECLS
