/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmimemagic.h: Private file.  Datastructure for storing the globs.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 * 
 * Copyright (C) 2003  Red Hat, Inc.
 * Copyright (C) 2003  Jonathan Blandford <jrb@alum.mit.edu>
 *
 * Licensed under the Academic Free License version 1.2
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __XDG_MIME_MAGIC_H__
#define __XDG_MIME_MAGIC_H__

#include <unistd.h>

typedef struct XdgMimeMagic XdgMimeMagic;

XdgMimeMagic *_xdg_mime_magic_new                (void);
void          _xdg_mime_magic_read_from_file     (XdgMimeMagic *mime_magic,
						  const char   *file_name);
void          _xdg_mime_magic_free               (XdgMimeMagic *mime_magic);
int           _xdg_mime_magic_get_buffer_extents (XdgMimeMagic *mime_magic);
const char   *_xdg_mime_magic_lookup_data        (XdgMimeMagic *mime_magic,
						  const void   *data,
						  size_t        len);

#endif /* __XDG_MIME_MAGIC_H__ */
