/* GdkPixbuf library - XPM image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <unistd.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"



/* I have must have done something to deserve this.
 * XPM is such a crappy format to handle.
 * This code is an ugly hybred from gdkpixmap.c
 * modified to respect transparent colors.
 * It's still a mess, though.
 */

enum buf_op {
	op_header,
	op_cmap,
	op_body
};

typedef struct {
	gchar *color_string;
	guint16 red;
	guint16 green;
	guint16 blue;
	gint transparent;
} XPMColor;

struct file_handle {
	FILE *infile;
	gchar *buffer;
	guint buffer_size;
};

struct mem_handle {
	const gchar **data;
	int offset;
};

/* The following 2 routines (parse_color, find_color) come from Tk, via the Win32
 * port of GDK. The licensing terms on these (longer than the functions) is:
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The following
 * terms apply to all files associated with the software unless explicitly
 * disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 * 
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal 
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense, the
 * software shall be classified as "Commercial Computer Software" and the
 * Government shall have only "Restricted Rights" as defined in Clause
 * 252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
 * authors grant the U.S. Government and others acting in its behalf
 * permission to use and distribute the software in accordance with the
 * terms specified in this license.
 */

typedef struct {
    const char *name;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} XPMColorEntry;

static XPMColorEntry xColors[] = {
    { "alice blue", 240, 248, 255 },
    { "AliceBlue", 240, 248, 255 },
    { "antique white", 250, 235, 215 },
    { "AntiqueWhite", 250, 235, 215 },
    { "AntiqueWhite1", 255, 239, 219 },
    { "AntiqueWhite2", 238, 223, 204 },
    { "AntiqueWhite3", 205, 192, 176 },
    { "AntiqueWhite4", 139, 131, 120 },
    { "aquamarine", 127, 255, 212 },
    { "aquamarine1", 127, 255, 212 },
    { "aquamarine2", 118, 238, 198 },
    { "aquamarine3", 102, 205, 170 },
    { "aquamarine4", 69, 139, 116 },
    { "azure", 240, 255, 255 },
    { "azure1", 240, 255, 255 },
    { "azure2", 224, 238, 238 },
    { "azure3", 193, 205, 205 },
    { "azure4", 131, 139, 139 },
    { "beige", 245, 245, 220 },
    { "bisque", 255, 228, 196 },
    { "bisque1", 255, 228, 196 },
    { "bisque2", 238, 213, 183 },
    { "bisque3", 205, 183, 158 },
    { "bisque4", 139, 125, 107 },
    { "black", 0, 0, 0 },
    { "blanched almond", 255, 235, 205 },
    { "BlanchedAlmond", 255, 235, 205 },
    { "blue", 0, 0, 255 },
    { "blue violet", 138, 43, 226 },
    { "blue1", 0, 0, 255 },
    { "blue2", 0, 0, 238 },
    { "blue3", 0, 0, 205 },
    { "blue4", 0, 0, 139 },
    { "BlueViolet", 138, 43, 226 },
    { "brown", 165, 42, 42 },
    { "brown1", 255, 64, 64 },
    { "brown2", 238, 59, 59 },
    { "brown3", 205, 51, 51 },
    { "brown4", 139, 35, 35 },
    { "burlywood", 222, 184, 135 },
    { "burlywood1", 255, 211, 155 },
    { "burlywood2", 238, 197, 145 },
    { "burlywood3", 205, 170, 125 },
    { "burlywood4", 139, 115, 85 },
    { "cadet blue", 95, 158, 160 },
    { "CadetBlue", 95, 158, 160 },
    { "CadetBlue1", 152, 245, 255 },
    { "CadetBlue2", 142, 229, 238 },
    { "CadetBlue3", 122, 197, 205 },
    { "CadetBlue4", 83, 134, 139 },
    { "chartreuse", 127, 255, 0 },
    { "chartreuse1", 127, 255, 0 },
    { "chartreuse2", 118, 238, 0 },
    { "chartreuse3", 102, 205, 0 },
    { "chartreuse4", 69, 139, 0 },
    { "chocolate", 210, 105, 30 },
    { "chocolate1", 255, 127, 36 },
    { "chocolate2", 238, 118, 33 },
    { "chocolate3", 205, 102, 29 },
    { "chocolate4", 139, 69, 19 },
    { "coral", 255, 127, 80 },
    { "coral1", 255, 114, 86 },
    { "coral2", 238, 106, 80 },
    { "coral3", 205, 91, 69 },
    { "coral4", 139, 62, 47 },
    { "cornflower blue", 100, 149, 237 },
    { "CornflowerBlue", 100, 149, 237 },
    { "cornsilk", 255, 248, 220 },
    { "cornsilk1", 255, 248, 220 },
    { "cornsilk2", 238, 232, 205 },
    { "cornsilk3", 205, 200, 177 },
    { "cornsilk4", 139, 136, 120 },
    { "cyan", 0, 255, 255 },
    { "cyan1", 0, 255, 255 },
    { "cyan2", 0, 238, 238 },
    { "cyan3", 0, 205, 205 },
    { "cyan4", 0, 139, 139 },
    { "dark blue", 0, 0, 139 },
    { "dark cyan", 0, 139, 139 },
    { "dark goldenrod", 184, 134, 11 },
    { "dark gray", 169, 169, 169 },
    { "dark green", 0, 100, 0 },
    { "dark grey", 169, 169, 169 },
    { "dark khaki", 189, 183, 107 },
    { "dark magenta", 139, 0, 139 },
    { "dark olive green", 85, 107, 47 },
    { "dark orange", 255, 140, 0 },
    { "dark orchid", 153, 50, 204 },
    { "dark red", 139, 0, 0 },
    { "dark salmon", 233, 150, 122 },
    { "dark sea green", 143, 188, 143 },
    { "dark slate blue", 72, 61, 139 },
    { "dark slate gray", 47, 79, 79 },
    { "dark slate grey", 47, 79, 79 },
    { "dark turquoise", 0, 206, 209 },
    { "dark violet", 148, 0, 211 },
    { "DarkBlue", 0, 0, 139 },
    { "DarkCyan", 0, 139, 139 },
    { "DarkGoldenrod", 184, 134, 11 },
    { "DarkGoldenrod1", 255, 185, 15 },
    { "DarkGoldenrod2", 238, 173, 14 },
    { "DarkGoldenrod3", 205, 149, 12 },
    { "DarkGoldenrod4", 139, 101, 8 },
    { "DarkGray", 169, 169, 169 },
    { "DarkGreen", 0, 100, 0 },
    { "DarkGrey", 169, 169, 169 },
    { "DarkKhaki", 189, 183, 107 },
    { "DarkMagenta", 139, 0, 139 },
    { "DarkOliveGreen", 85, 107, 47 },
    { "DarkOliveGreen1", 202, 255, 112 },
    { "DarkOliveGreen2", 188, 238, 104 },
    { "DarkOliveGreen3", 162, 205, 90 },
    { "DarkOliveGreen4", 110, 139, 61 },
    { "DarkOrange", 255, 140, 0 },
    { "DarkOrange1", 255, 127, 0 },
    { "DarkOrange2", 238, 118, 0 },
    { "DarkOrange3", 205, 102, 0 },
    { "DarkOrange4", 139, 69, 0 },
    { "DarkOrchid", 153, 50, 204 },
    { "DarkOrchid1", 191, 62, 255 },
    { "DarkOrchid2", 178, 58, 238 },
    { "DarkOrchid3", 154, 50, 205 },
    { "DarkOrchid4", 104, 34, 139 },
    { "DarkRed", 139, 0, 0 },
    { "DarkSalmon", 233, 150, 122 },
    { "DarkSeaGreen", 143, 188, 143 },
    { "DarkSeaGreen1", 193, 255, 193 },
    { "DarkSeaGreen2", 180, 238, 180 },
    { "DarkSeaGreen3", 155, 205, 155 },
    { "DarkSeaGreen4", 105, 139, 105 },
    { "DarkSlateBlue", 72, 61, 139 },
    { "DarkSlateGray", 47, 79, 79 },
    { "DarkSlateGray1", 151, 255, 255 },
    { "DarkSlateGray2", 141, 238, 238 },
    { "DarkSlateGray3", 121, 205, 205 },
    { "DarkSlateGray4", 82, 139, 139 },
    { "DarkSlateGrey", 47, 79, 79 },
    { "DarkTurquoise", 0, 206, 209 },
    { "DarkViolet", 148, 0, 211 },
    { "deep pink", 255, 20, 147 },
    { "deep sky blue", 0, 191, 255 },
    { "DeepPink", 255, 20, 147 },
    { "DeepPink1", 255, 20, 147 },
    { "DeepPink2", 238, 18, 137 },
    { "DeepPink3", 205, 16, 118 },
    { "DeepPink4", 139, 10, 80 },
    { "DeepSkyBlue", 0, 191, 255 },
    { "DeepSkyBlue1", 0, 191, 255 },
    { "DeepSkyBlue2", 0, 178, 238 },
    { "DeepSkyBlue3", 0, 154, 205 },
    { "DeepSkyBlue4", 0, 104, 139 },
    { "dim gray", 105, 105, 105 },
    { "dim grey", 105, 105, 105 },
    { "DimGray", 105, 105, 105 },
    { "DimGrey", 105, 105, 105 },
    { "dodger blue", 30, 144, 255 },
    { "DodgerBlue", 30, 144, 255 },
    { "DodgerBlue1", 30, 144, 255 },
    { "DodgerBlue2", 28, 134, 238 },
    { "DodgerBlue3", 24, 116, 205 },
    { "DodgerBlue4", 16, 78, 139 },
    { "firebrick", 178, 34, 34 },
    { "firebrick1", 255, 48, 48 },
    { "firebrick2", 238, 44, 44 },
    { "firebrick3", 205, 38, 38 },
    { "firebrick4", 139, 26, 26 },
    { "floral white", 255, 250, 240 },
    { "FloralWhite", 255, 250, 240 },
    { "forest green", 34, 139, 34 },
    { "ForestGreen", 34, 139, 34 },
    { "gainsboro", 220, 220, 220 },
    { "ghost white", 248, 248, 255 },
    { "GhostWhite", 248, 248, 255 },
    { "gold", 255, 215, 0 },
    { "gold1", 255, 215, 0 },
    { "gold2", 238, 201, 0 },
    { "gold3", 205, 173, 0 },
    { "gold4", 139, 117, 0 },
    { "goldenrod", 218, 165, 32 },
    { "goldenrod1", 255, 193, 37 },
    { "goldenrod2", 238, 180, 34 },
    { "goldenrod3", 205, 155, 29 },
    { "goldenrod4", 139, 105, 20 },
    { "gray", 190, 190, 190 },
    { "gray0", 0, 0, 0 },
    { "gray1", 3, 3, 3 },
    { "gray10", 26, 26, 26 },
    { "gray100", 255, 255, 255 },
    { "gray11", 28, 28, 28 },
    { "gray12", 31, 31, 31 },
    { "gray13", 33, 33, 33 },
    { "gray14", 36, 36, 36 },
    { "gray15", 38, 38, 38 },
    { "gray16", 41, 41, 41 },
    { "gray17", 43, 43, 43 },
    { "gray18", 46, 46, 46 },
    { "gray19", 48, 48, 48 },
    { "gray2", 5, 5, 5 },
    { "gray20", 51, 51, 51 },
    { "gray21", 54, 54, 54 },
    { "gray22", 56, 56, 56 },
    { "gray23", 59, 59, 59 },
    { "gray24", 61, 61, 61 },
    { "gray25", 64, 64, 64 },
    { "gray26", 66, 66, 66 },
    { "gray27", 69, 69, 69 },
    { "gray28", 71, 71, 71 },
    { "gray29", 74, 74, 74 },
    { "gray3", 8, 8, 8 },
    { "gray30", 77, 77, 77 },
    { "gray31", 79, 79, 79 },
    { "gray32", 82, 82, 82 },
    { "gray33", 84, 84, 84 },
    { "gray34", 87, 87, 87 },
    { "gray35", 89, 89, 89 },
    { "gray36", 92, 92, 92 },
    { "gray37", 94, 94, 94 },
    { "gray38", 97, 97, 97 },
    { "gray39", 99, 99, 99 },
    { "gray4", 10, 10, 10 },
    { "gray40", 102, 102, 102 },
    { "gray41", 105, 105, 105 },
    { "gray42", 107, 107, 107 },
    { "gray43", 110, 110, 110 },
    { "gray44", 112, 112, 112 },
    { "gray45", 115, 115, 115 },
    { "gray46", 117, 117, 117 },
    { "gray47", 120, 120, 120 },
    { "gray48", 122, 122, 122 },
    { "gray49", 125, 125, 125 },
    { "gray5", 13, 13, 13 },
    { "gray50", 127, 127, 127 },
    { "gray51", 130, 130, 130 },
    { "gray52", 133, 133, 133 },
    { "gray53", 135, 135, 135 },
    { "gray54", 138, 138, 138 },
    { "gray55", 140, 140, 140 },
    { "gray56", 143, 143, 143 },
    { "gray57", 145, 145, 145 },
    { "gray58", 148, 148, 148 },
    { "gray59", 150, 150, 150 },
    { "gray6", 15, 15, 15 },
    { "gray60", 153, 153, 153 },
    { "gray61", 156, 156, 156 },
    { "gray62", 158, 158, 158 },
    { "gray63", 161, 161, 161 },
    { "gray64", 163, 163, 163 },
    { "gray65", 166, 166, 166 },
    { "gray66", 168, 168, 168 },
    { "gray67", 171, 171, 171 },
    { "gray68", 173, 173, 173 },
    { "gray69", 176, 176, 176 },
    { "gray7", 18, 18, 18 },
    { "gray70", 179, 179, 179 },
    { "gray71", 181, 181, 181 },
    { "gray72", 184, 184, 184 },
    { "gray73", 186, 186, 186 },
    { "gray74", 189, 189, 189 },
    { "gray75", 191, 191, 191 },
    { "gray76", 194, 194, 194 },
    { "gray77", 196, 196, 196 },
    { "gray78", 199, 199, 199 },
    { "gray79", 201, 201, 201 },
    { "gray8", 20, 20, 20 },
    { "gray80", 204, 204, 204 },
    { "gray81", 207, 207, 207 },
    { "gray82", 209, 209, 209 },
    { "gray83", 212, 212, 212 },
    { "gray84", 214, 214, 214 },
    { "gray85", 217, 217, 217 },
    { "gray86", 219, 219, 219 },
    { "gray87", 222, 222, 222 },
    { "gray88", 224, 224, 224 },
    { "gray89", 227, 227, 227 },
    { "gray9", 23, 23, 23 },
    { "gray90", 229, 229, 229 },
    { "gray91", 232, 232, 232 },
    { "gray92", 235, 235, 235 },
    { "gray93", 237, 237, 237 },
    { "gray94", 240, 240, 240 },
    { "gray95", 242, 242, 242 },
    { "gray96", 245, 245, 245 },
    { "gray97", 247, 247, 247 },
    { "gray98", 250, 250, 250 },
    { "gray99", 252, 252, 252 },
    { "green", 0, 255, 0 },
    { "green yellow", 173, 255, 47 },
    { "green1", 0, 255, 0 },
    { "green2", 0, 238, 0 },
    { "green3", 0, 205, 0 },
    { "green4", 0, 139, 0 },
    { "GreenYellow", 173, 255, 47 },
    { "grey", 190, 190, 190 },
    { "grey0", 0, 0, 0 },
    { "grey1", 3, 3, 3 },
    { "grey10", 26, 26, 26 },
    { "grey100", 255, 255, 255 },
    { "grey11", 28, 28, 28 },
    { "grey12", 31, 31, 31 },
    { "grey13", 33, 33, 33 },
    { "grey14", 36, 36, 36 },
    { "grey15", 38, 38, 38 },
    { "grey16", 41, 41, 41 },
    { "grey17", 43, 43, 43 },
    { "grey18", 46, 46, 46 },
    { "grey19", 48, 48, 48 },
    { "grey2", 5, 5, 5 },
    { "grey20", 51, 51, 51 },
    { "grey21", 54, 54, 54 },
    { "grey22", 56, 56, 56 },
    { "grey23", 59, 59, 59 },
    { "grey24", 61, 61, 61 },
    { "grey25", 64, 64, 64 },
    { "grey26", 66, 66, 66 },
    { "grey27", 69, 69, 69 },
    { "grey28", 71, 71, 71 },
    { "grey29", 74, 74, 74 },
    { "grey3", 8, 8, 8 },
    { "grey30", 77, 77, 77 },
    { "grey31", 79, 79, 79 },
    { "grey32", 82, 82, 82 },
    { "grey33", 84, 84, 84 },
    { "grey34", 87, 87, 87 },
    { "grey35", 89, 89, 89 },
    { "grey36", 92, 92, 92 },
    { "grey37", 94, 94, 94 },
    { "grey38", 97, 97, 97 },
    { "grey39", 99, 99, 99 },
    { "grey4", 10, 10, 10 },
    { "grey40", 102, 102, 102 },
    { "grey41", 105, 105, 105 },
    { "grey42", 107, 107, 107 },
    { "grey43", 110, 110, 110 },
    { "grey44", 112, 112, 112 },
    { "grey45", 115, 115, 115 },
    { "grey46", 117, 117, 117 },
    { "grey47", 120, 120, 120 },
    { "grey48", 122, 122, 122 },
    { "grey49", 125, 125, 125 },
    { "grey5", 13, 13, 13 },
    { "grey50", 127, 127, 127 },
    { "grey51", 130, 130, 130 },
    { "grey52", 133, 133, 133 },
    { "grey53", 135, 135, 135 },
    { "grey54", 138, 138, 138 },
    { "grey55", 140, 140, 140 },
    { "grey56", 143, 143, 143 },
    { "grey57", 145, 145, 145 },
    { "grey58", 148, 148, 148 },
    { "grey59", 150, 150, 150 },
    { "grey6", 15, 15, 15 },
    { "grey60", 153, 153, 153 },
    { "grey61", 156, 156, 156 },
    { "grey62", 158, 158, 158 },
    { "grey63", 161, 161, 161 },
    { "grey64", 163, 163, 163 },
    { "grey65", 166, 166, 166 },
    { "grey66", 168, 168, 168 },
    { "grey67", 171, 171, 171 },
    { "grey68", 173, 173, 173 },
    { "grey69", 176, 176, 176 },
    { "grey7", 18, 18, 18 },
    { "grey70", 179, 179, 179 },
    { "grey71", 181, 181, 181 },
    { "grey72", 184, 184, 184 },
    { "grey73", 186, 186, 186 },
    { "grey74", 189, 189, 189 },
    { "grey75", 191, 191, 191 },
    { "grey76", 194, 194, 194 },
    { "grey77", 196, 196, 196 },
    { "grey78", 199, 199, 199 },
    { "grey79", 201, 201, 201 },
    { "grey8", 20, 20, 20 },
    { "grey80", 204, 204, 204 },
    { "grey81", 207, 207, 207 },
    { "grey82", 209, 209, 209 },
    { "grey83", 212, 212, 212 },
    { "grey84", 214, 214, 214 },
    { "grey85", 217, 217, 217 },
    { "grey86", 219, 219, 219 },
    { "grey87", 222, 222, 222 },
    { "grey88", 224, 224, 224 },
    { "grey89", 227, 227, 227 },
    { "grey9", 23, 23, 23 },
    { "grey90", 229, 229, 229 },
    { "grey91", 232, 232, 232 },
    { "grey92", 235, 235, 235 },
    { "grey93", 237, 237, 237 },
    { "grey94", 240, 240, 240 },
    { "grey95", 242, 242, 242 },
    { "grey96", 245, 245, 245 },
    { "grey97", 247, 247, 247 },
    { "grey98", 250, 250, 250 },
    { "grey99", 252, 252, 252 },
    { "honeydew", 240, 255, 240 },
    { "honeydew1", 240, 255, 240 },
    { "honeydew2", 224, 238, 224 },
    { "honeydew3", 193, 205, 193 },
    { "honeydew4", 131, 139, 131 },
    { "hot pink", 255, 105, 180 },
    { "HotPink", 255, 105, 180 },
    { "HotPink1", 255, 110, 180 },
    { "HotPink2", 238, 106, 167 },
    { "HotPink3", 205, 96, 144 },
    { "HotPink4", 139, 58, 98 },
    { "indian red", 205, 92, 92 },
    { "IndianRed", 205, 92, 92 },
    { "IndianRed1", 255, 106, 106 },
    { "IndianRed2", 238, 99, 99 },
    { "IndianRed3", 205, 85, 85 },
    { "IndianRed4", 139, 58, 58 },
    { "ivory", 255, 255, 240 },
    { "ivory1", 255, 255, 240 },
    { "ivory2", 238, 238, 224 },
    { "ivory3", 205, 205, 193 },
    { "ivory4", 139, 139, 131 },
    { "khaki", 240, 230, 140 },
    { "khaki1", 255, 246, 143 },
    { "khaki2", 238, 230, 133 },
    { "khaki3", 205, 198, 115 },
    { "khaki4", 139, 134, 78 },
    { "lavender", 230, 230, 250 },
    { "lavender blush", 255, 240, 245 },
    { "LavenderBlush", 255, 240, 245 },
    { "LavenderBlush1", 255, 240, 245 },
    { "LavenderBlush2", 238, 224, 229 },
    { "LavenderBlush3", 205, 193, 197 },
    { "LavenderBlush4", 139, 131, 134 },
    { "lawn green", 124, 252, 0 },
    { "LawnGreen", 124, 252, 0 },
    { "lemon chiffon", 255, 250, 205 },
    { "LemonChiffon", 255, 250, 205 },
    { "LemonChiffon1", 255, 250, 205 },
    { "LemonChiffon2", 238, 233, 191 },
    { "LemonChiffon3", 205, 201, 165 },
    { "LemonChiffon4", 139, 137, 112 },
    { "light blue", 173, 216, 230 },
    { "light coral", 240, 128, 128 },
    { "light cyan", 224, 255, 255 },
    { "light goldenrod", 238, 221, 130 },
    { "light goldenrod yellow", 250, 250, 210 },
    { "light gray", 211, 211, 211 },
    { "light green", 144, 238, 144 },
    { "light grey", 211, 211, 211 },
    { "light pink", 255, 182, 193 },
    { "light salmon", 255, 160, 122 },
    { "light sea green", 32, 178, 170 },
    { "light sky blue", 135, 206, 250 },
    { "light slate blue", 132, 112, 255 },
    { "light slate gray", 119, 136, 153 },
    { "light slate grey", 119, 136, 153 },
    { "light steel blue", 176, 196, 222 },
    { "light yellow", 255, 255, 224 },
    { "LightBlue", 173, 216, 230 },
    { "LightBlue1", 191, 239, 255 },
    { "LightBlue2", 178, 223, 238 },
    { "LightBlue3", 154, 192, 205 },
    { "LightBlue4", 104, 131, 139 },
    { "LightCoral", 240, 128, 128 },
    { "LightCyan", 224, 255, 255 },
    { "LightCyan1", 224, 255, 255 },
    { "LightCyan2", 209, 238, 238 },
    { "LightCyan3", 180, 205, 205 },
    { "LightCyan4", 122, 139, 139 },
    { "LightGoldenrod", 238, 221, 130 },
    { "LightGoldenrod1", 255, 236, 139 },
    { "LightGoldenrod2", 238, 220, 130 },
    { "LightGoldenrod3", 205, 190, 112 },
    { "LightGoldenrod4", 139, 129, 76 },
    { "LightGoldenrodYellow", 250, 250, 210 },
    { "LightGray", 211, 211, 211 },
    { "LightGreen", 144, 238, 144 },
    { "LightGrey", 211, 211, 211 },
    { "LightPink", 255, 182, 193 },
    { "LightPink1", 255, 174, 185 },
    { "LightPink2", 238, 162, 173 },
    { "LightPink3", 205, 140, 149 },
    { "LightPink4", 139, 95, 101 },
    { "LightSalmon", 255, 160, 122 },
    { "LightSalmon1", 255, 160, 122 },
    { "LightSalmon2", 238, 149, 114 },
    { "LightSalmon3", 205, 129, 98 },
    { "LightSalmon4", 139, 87, 66 },
    { "LightSeaGreen", 32, 178, 170 },
    { "LightSkyBlue", 135, 206, 250 },
    { "LightSkyBlue1", 176, 226, 255 },
    { "LightSkyBlue2", 164, 211, 238 },
    { "LightSkyBlue3", 141, 182, 205 },
    { "LightSkyBlue4", 96, 123, 139 },
    { "LightSlateBlue", 132, 112, 255 },
    { "LightSlateGray", 119, 136, 153 },
    { "LightSlateGrey", 119, 136, 153 },
    { "LightSteelBlue", 176, 196, 222 },
    { "LightSteelBlue1", 202, 225, 255 },
    { "LightSteelBlue2", 188, 210, 238 },
    { "LightSteelBlue3", 162, 181, 205 },
    { "LightSteelBlue4", 110, 123, 139 },
    { "LightYellow", 255, 255, 224 },
    { "LightYellow1", 255, 255, 224 },
    { "LightYellow2", 238, 238, 209 },
    { "LightYellow3", 205, 205, 180 },
    { "LightYellow4", 139, 139, 122 },
    { "lime green", 50, 205, 50 },
    { "LimeGreen", 50, 205, 50 },
    { "linen", 250, 240, 230 },
    { "magenta", 255, 0, 255 },
    { "magenta1", 255, 0, 255 },
    { "magenta2", 238, 0, 238 },
    { "magenta3", 205, 0, 205 },
    { "magenta4", 139, 0, 139 },
    { "maroon", 176, 48, 96 },
    { "maroon1", 255, 52, 179 },
    { "maroon2", 238, 48, 167 },
    { "maroon3", 205, 41, 144 },
    { "maroon4", 139, 28, 98 },
    { "medium aquamarine", 102, 205, 170 },
    { "medium blue", 0, 0, 205 },
    { "medium orchid", 186, 85, 211 },
    { "medium purple", 147, 112, 219 },
    { "medium sea green", 60, 179, 113 },
    { "medium slate blue", 123, 104, 238 },
    { "medium spring green", 0, 250, 154 },
    { "medium turquoise", 72, 209, 204 },
    { "medium violet red", 199, 21, 133 },
    { "MediumAquamarine", 102, 205, 170 },
    { "MediumBlue", 0, 0, 205 },
    { "MediumOrchid", 186, 85, 211 },
    { "MediumOrchid1", 224, 102, 255 },
    { "MediumOrchid2", 209, 95, 238 },
    { "MediumOrchid3", 180, 82, 205 },
    { "MediumOrchid4", 122, 55, 139 },
    { "MediumPurple", 147, 112, 219 },
    { "MediumPurple1", 171, 130, 255 },
    { "MediumPurple2", 159, 121, 238 },
    { "MediumPurple3", 137, 104, 205 },
    { "MediumPurple4", 93, 71, 139 },
    { "MediumSeaGreen", 60, 179, 113 },
    { "MediumSlateBlue", 123, 104, 238 },
    { "MediumSpringGreen", 0, 250, 154 },
    { "MediumTurquoise", 72, 209, 204 },
    { "MediumVioletRed", 199, 21, 133 },
    { "midnight blue", 25, 25, 112 },
    { "MidnightBlue", 25, 25, 112 },
    { "mint cream", 245, 255, 250 },
    { "MintCream", 245, 255, 250 },
    { "misty rose", 255, 228, 225 },
    { "MistyRose", 255, 228, 225 },
    { "MistyRose1", 255, 228, 225 },
    { "MistyRose2", 238, 213, 210 },
    { "MistyRose3", 205, 183, 181 },
    { "MistyRose4", 139, 125, 123 },
    { "moccasin", 255, 228, 181 },
    { "navajo white", 255, 222, 173 },
    { "NavajoWhite", 255, 222, 173 },
    { "NavajoWhite1", 255, 222, 173 },
    { "NavajoWhite2", 238, 207, 161 },
    { "NavajoWhite3", 205, 179, 139 },
    { "NavajoWhite4", 139, 121, 94 },
    { "navy", 0, 0, 128 },
    { "navy blue", 0, 0, 128 },
    { "NavyBlue", 0, 0, 128 },
    { "old lace", 253, 245, 230 },
    { "OldLace", 253, 245, 230 },
    { "olive drab", 107, 142, 35 },
    { "OliveDrab", 107, 142, 35 },
    { "OliveDrab1", 192, 255, 62 },
    { "OliveDrab2", 179, 238, 58 },
    { "OliveDrab3", 154, 205, 50 },
    { "OliveDrab4", 105, 139, 34 },
    { "orange", 255, 165, 0 },
    { "orange red", 255, 69, 0 },
    { "orange1", 255, 165, 0 },
    { "orange2", 238, 154, 0 },
    { "orange3", 205, 133, 0 },
    { "orange4", 139, 90, 0 },
    { "OrangeRed", 255, 69, 0 },
    { "OrangeRed1", 255, 69, 0 },
    { "OrangeRed2", 238, 64, 0 },
    { "OrangeRed3", 205, 55, 0 },
    { "OrangeRed4", 139, 37, 0 },
    { "orchid", 218, 112, 214 },
    { "orchid1", 255, 131, 250 },
    { "orchid2", 238, 122, 233 },
    { "orchid3", 205, 105, 201 },
    { "orchid4", 139, 71, 137 },
    { "pale goldenrod", 238, 232, 170 },
    { "pale green", 152, 251, 152 },
    { "pale turquoise", 175, 238, 238 },
    { "pale violet red", 219, 112, 147 },
    { "PaleGoldenrod", 238, 232, 170 },
    { "PaleGreen", 152, 251, 152 },
    { "PaleGreen1", 154, 255, 154 },
    { "PaleGreen2", 144, 238, 144 },
    { "PaleGreen3", 124, 205, 124 },
    { "PaleGreen4", 84, 139, 84 },
    { "PaleTurquoise", 175, 238, 238 },
    { "PaleTurquoise1", 187, 255, 255 },
    { "PaleTurquoise2", 174, 238, 238 },
    { "PaleTurquoise3", 150, 205, 205 },
    { "PaleTurquoise4", 102, 139, 139 },
    { "PaleVioletRed", 219, 112, 147 },
    { "PaleVioletRed1", 255, 130, 171 },
    { "PaleVioletRed2", 238, 121, 159 },
    { "PaleVioletRed3", 205, 104, 137 },
    { "PaleVioletRed4", 139, 71, 93 },
    { "papaya whip", 255, 239, 213 },
    { "PapayaWhip", 255, 239, 213 },
    { "peach puff", 255, 218, 185 },
    { "PeachPuff", 255, 218, 185 },
    { "PeachPuff1", 255, 218, 185 },
    { "PeachPuff2", 238, 203, 173 },
    { "PeachPuff3", 205, 175, 149 },
    { "PeachPuff4", 139, 119, 101 },
    { "peru", 205, 133, 63 },
    { "pink", 255, 192, 203 },
    { "pink1", 255, 181, 197 },
    { "pink2", 238, 169, 184 },
    { "pink3", 205, 145, 158 },
    { "pink4", 139, 99, 108 },
    { "plum", 221, 160, 221 },
    { "plum1", 255, 187, 255 },
    { "plum2", 238, 174, 238 },
    { "plum3", 205, 150, 205 },
    { "plum4", 139, 102, 139 },
    { "powder blue", 176, 224, 230 },
    { "PowderBlue", 176, 224, 230 },
    { "purple", 160, 32, 240 },
    { "purple1", 155, 48, 255 },
    { "purple2", 145, 44, 238 },
    { "purple3", 125, 38, 205 },
    { "purple4", 85, 26, 139 },
    { "red", 255, 0, 0 },
    { "red1", 255, 0, 0 },
    { "red2", 238, 0, 0 },
    { "red3", 205, 0, 0 },
    { "red4", 139, 0, 0 },
    { "rosy brown", 188, 143, 143 },
    { "RosyBrown", 188, 143, 143 },
    { "RosyBrown1", 255, 193, 193 },
    { "RosyBrown2", 238, 180, 180 },
    { "RosyBrown3", 205, 155, 155 },
    { "RosyBrown4", 139, 105, 105 },
    { "royal blue", 65, 105, 225 },
    { "RoyalBlue", 65, 105, 225 },
    { "RoyalBlue1", 72, 118, 255 },
    { "RoyalBlue2", 67, 110, 238 },
    { "RoyalBlue3", 58, 95, 205 },
    { "RoyalBlue4", 39, 64, 139 },
    { "saddle brown", 139, 69, 19 },
    { "SaddleBrown", 139, 69, 19 },
    { "salmon", 250, 128, 114 },
    { "salmon1", 255, 140, 105 },
    { "salmon2", 238, 130, 98 },
    { "salmon3", 205, 112, 84 },
    { "salmon4", 139, 76, 57 },
    { "sandy brown", 244, 164, 96 },
    { "SandyBrown", 244, 164, 96 },
    { "sea green", 46, 139, 87 },
    { "SeaGreen", 46, 139, 87 },
    { "SeaGreen1", 84, 255, 159 },
    { "SeaGreen2", 78, 238, 148 },
    { "SeaGreen3", 67, 205, 128 },
    { "SeaGreen4", 46, 139, 87 },
    { "seashell", 255, 245, 238 },
    { "seashell1", 255, 245, 238 },
    { "seashell2", 238, 229, 222 },
    { "seashell3", 205, 197, 191 },
    { "seashell4", 139, 134, 130 },
    { "sienna", 160, 82, 45 },
    { "sienna1", 255, 130, 71 },
    { "sienna2", 238, 121, 66 },
    { "sienna3", 205, 104, 57 },
    { "sienna4", 139, 71, 38 },
    { "sky blue", 135, 206, 235 },
    { "SkyBlue", 135, 206, 235 },
    { "SkyBlue1", 135, 206, 255 },
    { "SkyBlue2", 126, 192, 238 },
    { "SkyBlue3", 108, 166, 205 },
    { "SkyBlue4", 74, 112, 139 },
    { "slate blue", 106, 90, 205 },
    { "slate gray", 112, 128, 144 },
    { "slate grey", 112, 128, 144 },
    { "SlateBlue", 106, 90, 205 },
    { "SlateBlue1", 131, 111, 255 },
    { "SlateBlue2", 122, 103, 238 },
    { "SlateBlue3", 105, 89, 205 },
    { "SlateBlue4", 71, 60, 139 },
    { "SlateGray", 112, 128, 144 },
    { "SlateGray1", 198, 226, 255 },
    { "SlateGray2", 185, 211, 238 },
    { "SlateGray3", 159, 182, 205 },
    { "SlateGray4", 108, 123, 139 },
    { "SlateGrey", 112, 128, 144 },
    { "snow", 255, 250, 250 },
    { "snow1", 255, 250, 250 },
    { "snow2", 238, 233, 233 },
    { "snow3", 205, 201, 201 },
    { "snow4", 139, 137, 137 },
    { "spring green", 0, 255, 127 },
    { "SpringGreen", 0, 255, 127 },
    { "SpringGreen1", 0, 255, 127 },
    { "SpringGreen2", 0, 238, 118 },
    { "SpringGreen3", 0, 205, 102 },
    { "SpringGreen4", 0, 139, 69 },
    { "steel blue", 70, 130, 180 },
    { "SteelBlue", 70, 130, 180 },
    { "SteelBlue1", 99, 184, 255 },
    { "SteelBlue2", 92, 172, 238 },
    { "SteelBlue3", 79, 148, 205 },
    { "SteelBlue4", 54, 100, 139 },
    { "tan", 210, 180, 140 },
    { "tan1", 255, 165, 79 },
    { "tan2", 238, 154, 73 },
    { "tan3", 205, 133, 63 },
    { "tan4", 139, 90, 43 },
    { "thistle", 216, 191, 216 },
    { "thistle1", 255, 225, 255 },
    { "thistle2", 238, 210, 238 },
    { "thistle3", 205, 181, 205 },
    { "thistle4", 139, 123, 139 },
    { "tomato", 255, 99, 71 },
    { "tomato1", 255, 99, 71 },
    { "tomato2", 238, 92, 66 },
    { "tomato3", 205, 79, 57 },
    { "tomato4", 139, 54, 38 },
    { "turquoise", 64, 224, 208 },
    { "turquoise1", 0, 245, 255 },
    { "turquoise2", 0, 229, 238 },
    { "turquoise3", 0, 197, 205 },
    { "turquoise4", 0, 134, 139 },
    { "violet", 238, 130, 238 },
    { "violet red", 208, 32, 144 },
    { "VioletRed", 208, 32, 144 },
    { "VioletRed1", 255, 62, 150 },
    { "VioletRed2", 238, 58, 140 },
    { "VioletRed3", 205, 50, 120 },
    { "VioletRed4", 139, 34, 82 },
    { "wheat", 245, 222, 179 },
    { "wheat1", 255, 231, 186 },
    { "wheat2", 238, 216, 174 },
    { "wheat3", 205, 186, 150 },
    { "wheat4", 139, 126, 102 },
    { "white", 255, 255, 255 },
    { "white smoke", 245, 245, 245 },
    { "WhiteSmoke", 245, 245, 245 },
    { "yellow", 255, 255, 0 },
    { "yellow green", 154, 205, 50 },
    { "yellow1", 255, 255, 0 },
    { "yellow2", 238, 238, 0 },
    { "yellow3", 205, 205, 0 },
    { "yellow4", 139, 139, 0 },
    { "YellowGreen", 154, 205, 50 }
};
 
#define numXColors (sizeof (xColors) / sizeof (*xColors))
 
/*
 *----------------------------------------------------------------------
 *
 * find_color --
 *
 *	This routine finds the color entry that corresponds to the
 *	specified color.
 *
 * Results:
 *	Returns non-zero on success.  The RGB values of the XColor
 *	will be initialized to the proper values on success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
compare_xcolor_entries (const void *a, const void *b)
{
  return strcasecmp ((const char *) a, ((const XPMColorEntry *) b)->name);
}

static gboolean
find_color(const char *name,
	   XPMColor   *colorPtr)
{
	XPMColorEntry *found;

	found = bsearch (name, xColors, numXColors, sizeof (XPMColorEntry),
			 compare_xcolor_entries);
	if (found == NULL)
	  return 0;
	
	colorPtr->red = (found->red * 65535) / 255;
	colorPtr->green = (found->green * 65535) / 255;
	colorPtr->blue = (found->blue * 65535) / 255;
	
	return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * parse_color --
 *
 *	Partial implementation of X color name parsing interface.
 *
 * Results:
 *	Returns non-zero on success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static gboolean
parse_color (const char *spec,
	     XPMColor   *colorPtr)
{
	if (spec[0] == '#') {
		char fmt[16];
		int i, red, green, blue;

		if ((i = strlen(spec+1))%3) {
			return FALSE;
		}
		i /= 3;

		sprintf(fmt, "%%%dx%%%dx%%%dx", i, i, i);
		if (sscanf(spec+1, fmt, &red, &green, &blue) != 3) {
			return FALSE;
		}
		if (i == 4) {
			colorPtr->red = red;
			colorPtr->green = green;
			colorPtr->blue = blue;
		} else if (i == 1) {
			colorPtr->red = (red * 65535) / 15;
			colorPtr->green = (green * 65535) / 15;
			colorPtr->blue = (blue * 65535) / 15;
		} else if (i == 2)
		{
			colorPtr->red = (red * 65535) / 255;
			colorPtr->green = (green * 65535) / 255;
			colorPtr->blue = (blue * 65535) / 255;
		} else /* if (i == 3) */ {
			colorPtr->red = (red * 65535) / 4095;
			colorPtr->green = (green * 65535) / 4095;
			colorPtr->blue = (blue * 65535) / 4095;
		}
	} else {
		if (!find_color(spec, colorPtr))
			return FALSE;
	}
	return TRUE;
}

static gint
xpm_seek_string (FILE *infile, const gchar *str, gint skip_comments)
{
	char instr[1024];

	while (!feof (infile)) {
		if (fscanf (infile, "%1023s", instr) < 0)
                        return FALSE;
		if (skip_comments == TRUE && strcmp (instr, "/*") == 0) {
			fscanf (infile, "%1023s", instr);
			while (!feof (infile) && strcmp (instr, "*/") != 0)
				fscanf (infile, "%1023s", instr);
			fscanf (infile, "%1023s", instr);
		}

		if (strcmp (instr, str) == 0)
			return TRUE;
	}

	return FALSE;
}

static gint
xpm_seek_char (FILE *infile, gchar c)
{
	gint b, oldb;

	while ((b = getc (infile)) != EOF) {
		if (c != b && b == '/') {
			b = getc (infile);
			if (b == EOF)
				return FALSE;

			else if (b == '*') {	/* we have a comment */
				b = -1;
				do {
					oldb = b;
					b = getc (infile);
					if (b == EOF)
						return FALSE;
				} while (!(oldb == '*' && b == '/'));
			}
		} else if (c == b)
			return TRUE;
	}

	return FALSE;
}

static gint
xpm_read_string (FILE *infile, gchar **buffer, guint *buffer_size)
{
	gint c;
	guint cnt = 0, bufsiz, ret = FALSE;
	gchar *buf;

	buf = *buffer;
	bufsiz = *buffer_size;
	if (buf == NULL) {
		bufsiz = 10 * sizeof (gchar);
		buf = g_new (gchar, bufsiz);
	}

	do {
		c = getc (infile);
	} while (c != EOF && c != '"');

	if (c != '"')
		goto out;

	while ((c = getc (infile)) != EOF) {
		if (cnt == bufsiz) {
			guint new_size = bufsiz * 2;

			if (new_size > bufsiz)
				bufsiz = new_size;
			else
				goto out;

			buf = g_realloc (buf, bufsiz);
			buf[bufsiz - 1] = '\0';
		}

		if (c != '"')
			buf[cnt++] = c;
		else {
			buf[cnt] = 0;
			ret = TRUE;
			break;
		}
	}

 out:
	buf[bufsiz - 1] = '\0';	/* ensure null termination for errors */
	*buffer = buf;
	*buffer_size = bufsiz;
	return ret;
}

static const gchar *
xpm_skip_whitespaces (const gchar *buffer)
{
	gint32 index = 0;

	while (buffer[index] != 0 && (buffer[index] == 0x20 || buffer[index] == 0x09))
		index++;

	return &buffer[index];
}

static const gchar *
xpm_skip_string (const gchar *buffer)
{
	gint32 index = 0;

	while (buffer[index] != 0 && buffer[index] != 0x20 && buffer[index] != 0x09)
		index++;

	return &buffer[index];
}

/* Xlib crashed once at a color name lengths around 125 */
#define MAX_COLOR_LEN 120

static gchar *
xpm_extract_color (const gchar *buffer)
{
	gint counter, numnames;
	const gchar *ptr = NULL;
        gchar ch, temp[128];
	gchar color[MAX_COLOR_LEN], *retcol;
	gint space;

	counter = 0;
	while (ptr == NULL) {
		if ((buffer[counter] == 'c') || (buffer[counter] == 'g')) {
			ch = buffer[counter + 1];
			if (ch == 0x20 || ch == 0x09)
				ptr = &buffer[counter + 1];
		} else if (buffer[counter] == 0)
			return NULL;

		counter++;
	}
	ptr = xpm_skip_whitespaces (ptr);

	if (ptr[0] == 0)
		return NULL;
	else if (ptr[0] == '#') {
		counter = 1;
		while (ptr[counter] != 0 &&
		       ((ptr[counter] >= '0' && ptr[counter] <= '9') ||
			(ptr[counter] >= 'a' && ptr[counter] <= 'f') ||
			(ptr[counter] >= 'A' && ptr[counter] <= 'F')))
			counter++;
		retcol = g_new (gchar, counter + 1);
		strncpy (retcol, ptr, counter);

		retcol[counter] = 0;

		return retcol;
	}
	color[0] = 0;
	numnames = 0;

	space = MAX_COLOR_LEN - 1;
	while (space > 0) {
		sscanf (ptr, "%127s", temp);

		if (((gint) ptr[0] == 0) ||
		    (strcmp ("s", temp) == 0) || (strcmp ("m", temp) == 0) ||
		    (strcmp ("g", temp) == 0) || (strcmp ("g4", temp) == 0))
			break;
		else {
			if (numnames > 0) {
				space -= 1;
				strcat (color, " ");
			}

			strncat (color, temp, space);
			space -= MIN (space, strlen (temp));
			ptr = xpm_skip_string (ptr);
			ptr = xpm_skip_whitespaces (ptr);
			numnames++;
		}
	}

	retcol = g_strdup (color);
	return retcol;
}

/* (almost) direct copy from gdkpixmap.c... loads an XPM from a file */

static const gchar *
file_buffer (enum buf_op op, gpointer handle)
{
	struct file_handle *h = handle;

	switch (op) {
	case op_header:
		if (xpm_seek_string (h->infile, "XPM", FALSE) != TRUE)
			break;

		if (xpm_seek_char (h->infile, '{') != TRUE)
			break;
		/* Fall through to the next xpm_seek_char. */

	case op_cmap:
		xpm_seek_char (h->infile, '"');
		fseek (h->infile, -1, SEEK_CUR);
		/* Fall through to the xpm_read_string. */

	case op_body:
		xpm_read_string (h->infile, &h->buffer, &h->buffer_size);
		return h->buffer;

	default:
		g_assert_not_reached ();
	}

	return NULL;
}

/* This reads from memory */
static const gchar *
mem_buffer (enum buf_op op, gpointer handle)
{
	struct mem_handle *h = handle;
	switch (op) {
	case op_header:
	case op_cmap:
	case op_body:
                if (h->data[h->offset]) {
                        const gchar* retval;

                        retval = h->data[h->offset];
                        h->offset += 1;
                        return retval;
                }
                break;

	default:
		g_assert_not_reached ();
                break;
	}

	return NULL;
}

/* Destroy notification function for the pixbuf */
static void
free_buffer (guchar *pixels, gpointer data)
{
	free (pixels);
}

/* This function does all the work. */
static GdkPixbuf *
pixbuf_create_from_xpm (const gchar * (*get_buf) (enum buf_op op, gpointer handle), gpointer handle)
{
	gint w, h, n_col, cpp;
	gint cnt, xcnt, ycnt, wbytes, n, ns;
	gint is_trans = FALSE;
	const gchar *buffer;
        gchar *name_buf;
	gchar pixel_str[32];
	GHashTable *color_hash;
	XPMColor *colors, *color, *fallbackcolor;
	guchar *pixels, *pixtmp;

	fallbackcolor = NULL;

	buffer = (*get_buf) (op_header, handle);
	if (!buffer) {
		g_warning ("No XPM header found");
		return NULL;
	}
	sscanf (buffer, "%d %d %d %d", &w, &h, &n_col, &cpp);
	if (cpp >= 32) {
		g_warning ("XPM has more than 31 chars per pixel.");
		return NULL;
	}

	/* The hash is used for fast lookups of color from chars */
	color_hash = g_hash_table_new (g_str_hash, g_str_equal);

	name_buf = g_new (gchar, n_col * (cpp + 1));
	colors = g_new (XPMColor, n_col);

	for (cnt = 0; cnt < n_col; cnt++) {
		gchar *color_name;

		buffer = (*get_buf) (op_cmap, handle);
		if (!buffer) {
			g_warning ("Can't load XPM colormap");
			g_hash_table_destroy (color_hash);
			g_free (name_buf);
			g_free (colors);
			return NULL;
		}

		color = &colors[cnt];
		color->color_string = &name_buf[cnt * (cpp + 1)];
		strncpy (color->color_string, buffer, cpp);
		color->color_string[cpp] = 0;
		buffer += strlen (color->color_string);
		color->transparent = FALSE;

		color_name = xpm_extract_color (buffer);

		if ((color_name == NULL) || (g_strcasecmp (color_name, "None") == 0)
		    || (parse_color (color_name, color) == FALSE)) {
			color->transparent = TRUE;
			is_trans = TRUE;
		}

		g_free (color_name);
		g_hash_table_insert (color_hash, color->color_string, color);

		if (cnt == 0)
			fallbackcolor = color;
	}

	if (is_trans)
		pixels = malloc (w * h * 4);
	else
		pixels = malloc (w * h * 3);

	if (!pixels) {
		g_hash_table_destroy (color_hash);
		g_free (colors);
		g_free (name_buf);
		return NULL;
	}

	wbytes = w * cpp;
	pixtmp = pixels;

	for (ycnt = 0; ycnt < h; ycnt++) {
		buffer = (*get_buf) (op_body, handle);
		if ((!buffer) || (strlen (buffer) < wbytes))
			continue;

		for (n = 0, cnt = 0, xcnt = 0; n < wbytes; n += cpp, xcnt++) {
			strncpy (pixel_str, &buffer[n], cpp);
			pixel_str[cpp] = 0;
			ns = 0;

			color = g_hash_table_lookup (color_hash, pixel_str);

			/* Bad XPM...punt */
			if (!color)
				color = fallbackcolor;

			*pixtmp++ = color->red >> 8;
			*pixtmp++ = color->green >> 8;
			*pixtmp++ = color->blue >> 8;

			if (is_trans && color->transparent)
				*pixtmp++ = 0;
			else if (is_trans)
				*pixtmp++ = 0xFF;
		}
	}

	g_hash_table_destroy (color_hash);
	g_free (colors);
	g_free (name_buf);

	return gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, is_trans, 8,
					 w, h, is_trans ? (w * 4) : (w * 3),
					 free_buffer, NULL);
}

/* Shared library entry point for file loading */
GdkPixbuf *
gdk_pixbuf__xpm_image_load (FILE *f)
{
	GdkPixbuf *pixbuf;
	struct file_handle h;

	memset (&h, 0, sizeof (h));
	h.infile = f;
	pixbuf = pixbuf_create_from_xpm (file_buffer, &h);
	g_free (h.buffer);

	return pixbuf;
}

/* Shared library entry point for memory loading */
GdkPixbuf *
gdk_pixbuf__xpm_image_load_xpm_data (const gchar **data)
{
        GdkPixbuf *pixbuf;
        struct mem_handle h;

        h.data = data;
        h.offset = 0;
        
	pixbuf = pixbuf_create_from_xpm (mem_buffer, &h);
        
	return pixbuf;
}

/* Progressive loader */
typedef struct _XPMContext XPMContext;
struct _XPMContext
{
       ModulePreparedNotifyFunc prepare_func;
       ModuleUpdatedNotifyFunc update_func;
       gpointer user_data;

       gchar *tempname;
       FILE *file;
       gboolean all_okay;
};

/*
 * FIXME xpm loading progressively is not properly implemented.
 * Instead we will buffer to a file then load that file when done.
 * This is very broken but it should be relayively simple to fix
 * in the future.
 */
gpointer
gdk_pixbuf__xpm_image_begin_load (ModulePreparedNotifyFunc prepare_func,
                                  ModuleUpdatedNotifyFunc update_func,
                                  ModuleFrameDoneNotifyFunc frame_done_func,
                                  ModuleAnimationDoneNotifyFunc anim_done_func,
                                  gpointer user_data,
                                  GError **error)
{
       XPMContext *context;
       gint fd;

       g_warning ("load start");
       context = g_new (XPMContext, 1);
       context->prepare_func = prepare_func;
       context->update_func = update_func;
       context->user_data = user_data;
       context->all_okay = TRUE;
       fd = g_file_open_tmp ("gdkpixbuf-xpm-tmp.XXXXXX", &context->tempname,
			     NULL);
       if (fd < 0) {
               g_free (context);
               return NULL;
       }

       context->file = fdopen (fd, "w+");
       if (context->file == NULL) {
               g_free (context->tempname);
               g_free (context);
               return NULL;
       }

       return context;
}

void
gdk_pixbuf__xpm_image_stop_load (gpointer data)
{
       XPMContext *context = (XPMContext*) data;
       GdkPixbuf *pixbuf;

       g_return_if_fail (data != NULL);
       g_warning ("stopped loading");

       fflush (context->file);
       rewind (context->file);
       if (context->all_okay) {
               pixbuf = gdk_pixbuf__xpm_image_load (context->file);

               (* context->prepare_func) (pixbuf, context->user_data);
               (* context->update_func) (pixbuf, 0, 0, pixbuf->width, pixbuf->height, context->user_data);
               gdk_pixbuf_unref (pixbuf);
       }

       fclose (context->file);
       unlink (context->tempname);
       g_free (context->tempname);
       g_free ((XPMContext *) context);
}

gboolean
gdk_pixbuf__xpm_image_load_increment (gpointer data, guchar *buf, guint size)
{
       XPMContext *context = (XPMContext *) data;

       g_return_val_if_fail (data != NULL, FALSE);
       g_warning ("load increment");

       if (fwrite (buf, sizeof (guchar), size, context->file) != size) {
               context->all_okay = FALSE;
               return FALSE;
       }

       return TRUE;
}
