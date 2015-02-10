/*
 * Copyright (c) 2015 Christian Hergert <chergert@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Christian Hergert <chergert@gnome.org>
 *
 */

#ifndef __GTK_TEXT_ATTRIBUTE_PRIVATE_H__
#define __GTK_TEXT_ATTRIBUTE_PRIVATE_H__

/*
 * The following macros are used to store and extract information about the
 * Pango underline and strikethrough colors in the unused pixel member of
 * the GdkColor members of GtkTextAppearance.
 *
 * In 4.0, we should revisit this.
 */

#define GTK_TEXT_APPEARANCE_GET_UNDERLINE_RGBA_SET(appr) \
    (((guint8*)&(appr)->bg_color)[3] != 0)
#define GTK_TEXT_APPEARANCE_SET_UNDERLINE_RGBA_SET(appr,val) \
  G_STMT_START { \
    ((guint8*)&(appr)->bg_color)[3] = !!val; \
  } G_STMT_END
#define GTK_TEXT_APPEARANCE_GET_UNDERLINE_RGBA(appr,rgba) \
  G_STMT_START { \
    (rgba)->red = ((guint8*)&(appr)->bg_color)[0] / 255.; \
    (rgba)->green = ((guint8*)&(appr)->bg_color)[1] / 255.; \
    (rgba)->blue = ((guint8*)&(appr)->bg_color)[2] / 255.; \
    (rgba)->alpha = 1.0; \
  } G_STMT_END
#define GTK_TEXT_APPEARANCE_SET_UNDERLINE_RGBA(appr,rgba) \
  G_STMT_START { \
    ((guint8*)&(appr)->bg_color)[0] = (rgba)->red * 255; \
    ((guint8*)&(appr)->bg_color)[1] = (rgba)->green * 255; \
    ((guint8*)&(appr)->bg_color)[2] = (rgba)->blue * 255; \
  } G_STMT_END


#define GTK_TEXT_APPEARANCE_GET_STRIKETHROUGH_RGBA_SET(appr) \
    (((guint8*)&(appr)->fg_color)[3] != 0)
#define GTK_TEXT_APPEARANCE_SET_STRIKETHROUGH_RGBA_SET(appr,val) \
  G_STMT_START { \
    ((guint8*)&(appr)->fg_color)[3] = !!val; \
  } G_STMT_END
#define GTK_TEXT_APPEARANCE_GET_STRIKETHROUGH_RGBA(appr,rgba) \
  G_STMT_START { \
    (rgba)->red = ((guint8*)&(appr)->fg_color)[0] / 255.; \
    (rgba)->green = ((guint8*)&(appr)->fg_color)[1] / 255.; \
    (rgba)->blue = ((guint8*)&(appr)->fg_color)[2] / 255.; \
    (rgba)->alpha = 1.0; \
  } G_STMT_END
#define GTK_TEXT_APPEARANCE_SET_STRIKETHROUGH_RGBA(appr,rgba) \
  G_STMT_START { \
    ((guint8*)&(appr)->fg_color)[0] = (rgba)->red * 255; \
    ((guint8*)&(appr)->fg_color)[1] = (rgba)->green * 255; \
    ((guint8*)&(appr)->fg_color)[2] = (rgba)->blue * 255; \
  } G_STMT_END


#endif /* __GTK_TEXT_ATTRIBUTE_PRIVATE_H__ */
