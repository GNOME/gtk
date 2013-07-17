#!/usr/bin/env perl

# Updates http://git.gnome.org/browse/gtk+/tree/gdk/gdkkeysyms.h from upstream (X.org 7.x),
# from http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h
# 
# Author  : Simos Xenitellis <simos at gnome dot org>.
# Author  : Bastien Nocera <hadess@hadess.net>
# Version : 1.2
#
# Input   : http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h
# Input   : http://cgit.freedesktop.org/xorg/proto/x11proto/plain/XF86keysym.h
# Output  : http://git.gnome.org/browse/gtk+/tree/gdk/gdkkeysyms.h
# 
# Notes   : It downloads keysymdef.h from the Internet, if not found locally,
# Notes   : and creates an updated gdkkeysyms.h
# Notes   : This version updates the source of gdkkeysyms.h from CVS to the GIT server.

use strict;

# Used for reading the keysymdef symbols.
my @keysymelements;

if ( ! -f "keysymdef.h" )
{
	print "Trying to download keysymdef.h from\n";
	print "http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h\n";
	die "Unable to download keysymdef.h from http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h\n" 
		unless system("wget -c -O keysymdef.h \"http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h\"") == 0;
	print " done.\n\n";
}
else
{
	print "We are using existing keysymdef.h found in this directory.\n";
	print "It is assumed that you took care and it is a recent version\n";
	print "as found at http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h\n\n";
}

if ( ! -f "XF86keysym.h" )
{
	print "Trying to download XF86keysym.h from\n";
	print "http://cgit.freedesktop.org/xorg/proto/x11proto/plain/XF86keysym.h\n";
	die "Unable to download keysymdef.h from http://cgit.freedesktop.org/xorg/proto/x11proto/plain/XF86keysym.h\n" 
		unless system("wget -c -O XF86keysym.h \"http://cgit.freedesktop.org/xorg/proto/x11proto/plain/XF86keysym.h\"") == 0;
	print " done.\n\n";
}
else
{
	print "We are using existing XF86keysym.h found in this directory.\n";
	print "It is assumed that you took care and it is a recent version\n";
	print "as found at http://cgit.freedesktop.org/xorg/proto/x11proto/plain/XF86keysym.h\n\n";
}

# Source: http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h
die "Could not open file keysymdef.h: $!\n" unless open(IN_KEYSYMDEF, "<:utf8", "keysymdef.h");

# Output: gtk+/gdk/gdkkeysyms.h
die "Could not open file gdkkeysyms.h: $!\n" unless open(OUT_GDKKEYSYMS, ">:utf8", "gdkkeysyms.h");

# Output: gtk+/gdk/gdkkeysyms-compat.h
die "Could not open file gdkkeysyms-compat.h: $!\n" unless open(OUT_GDKKEYSYMS_COMPAT, ">:utf8", "gdkkeysyms-compat.h");

my $LICENSE_HEADER= <<EOF;
/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2005, 2006, 2007, 2009 GNOME Foundation
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

EOF

print OUT_GDKKEYSYMS $LICENSE_HEADER;
print OUT_GDKKEYSYMS_COMPAT $LICENSE_HEADER;

print OUT_GDKKEYSYMS<<EOF;

/*
 * File auto-generated from script http://git.gnome.org/browse/gtk+/tree/gdk/gdkkeysyms-update.pl
 * using the input file
 * http://cgit.freedesktop.org/xorg/proto/x11proto/plain/keysymdef.h
 * and
 * http://cgit.freedesktop.org/xorg/proto/x11proto/plain/XF86keysym.h
 */

/*
 * Modified by the GTK+ Team and others 1997-2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GDK_KEYSYMS_H__
#define __GDK_KEYSYMS_H__


EOF

print OUT_GDKKEYSYMS_COMPAT<<EOF;
/*
 * Compatibility version of gdkkeysyms.h.
 *
 * In GTK3, keysyms changed to have a KEY_ prefix.  This is a compatibility header
 * your application can include to gain access to the old names as well.  Consider
 * porting to the new names instead.
 */

#ifndef __GDK_KEYSYMS_COMPAT_H__
#define __GDK_KEYSYMS_COMPAT_H__

EOF

while (<IN_KEYSYMDEF>)
{
	next if ( ! /^#define / );

	@keysymelements = split(/\s+/);
	die "Internal error, no \@keysymelements: $_\n" unless @keysymelements;

	$_ = $keysymelements[1];
	die "Internal error, was expecting \"XC_*\", found: $_\n" if ( ! /^XK_/ );
	
	$_ = $keysymelements[2];
	die "Internal error, was expecting \"0x*\", found: $_\n" if ( ! /^0x/ );

	my $element = $keysymelements[1];
	my $binding = $element;
	$binding =~ s/^XK_/GDK_KEY_/g;
	my $compat_binding = $element;
	$compat_binding =~ s/^XK_/GDK_/g;

	printf OUT_GDKKEYSYMS "#define %s 0x%03x\n", $binding, hex($keysymelements[2]);
	printf OUT_GDKKEYSYMS_COMPAT "#define %s 0x%03x\n", $compat_binding, hex($keysymelements[2]);
}

close IN_KEYSYMDEF;

# Source: http://cgit.freedesktop.org/xorg/proto/x11proto/plain/XF86keysym.h
die "Could not open file XF86keysym.h: $!\n" unless open(IN_XF86KEYSYM, "<:utf8", "XF86keysym.h");

while (<IN_XF86KEYSYM>)
{
	next if ( ! /^#define / );

	@keysymelements = split(/\s+/);
	die "Internal error, no \@keysymelements: $_\n" unless @keysymelements;

	$_ = $keysymelements[1];
	die "Internal error, was expecting \"XF86XK_*\", found: $_\n" if ( ! /^XF86XK_/ );

	# XF86XK_Clear could end up a dupe of XK_Clear
	# XF86XK_Select could end up a dupe of XK_Select
	if ($_ eq "XF86XK_Clear") {
		$keysymelements[1] = "XF86XK_WindowClear";
	}
	if ($_ eq "XF86XK_Select") {
		$keysymelements[1] = "XF86XK_SelectButton";
	}

	# Ignore XF86XK_Q
	next if ( $_ eq "XF86XK_Q");
	# XF86XK_Calculater is misspelled, and a dupe
	next if ( $_ eq "XF86XK_Calculater");

	$_ = $keysymelements[2];
	die "Internal error, was expecting \"0x*\", found: $_\n" if ( ! /^0x/ );

	my $element = $keysymelements[1];
	my $binding = $element;
	$binding =~ s/^XF86XK_/GDK_KEY_/g;
	my $compat_binding = $element;
	$compat_binding =~ s/^XF86XK_/GDK_/g;

	printf OUT_GDKKEYSYMS "#define %s 0x%03x\n", $binding, hex($keysymelements[2]);
	printf OUT_GDKKEYSYMS_COMPAT "#define %s 0x%03x\n", $compat_binding, hex($keysymelements[2]);
}

close IN_XF86KEYSYM;


print OUT_GDKKEYSYMS<<EOF;

#endif /* __GDK_KEYSYMS_H__ */
EOF

print OUT_GDKKEYSYMS_COMPAT<<EOF;

#endif /* __GDK_KEYSYMS_COMPAT_H__ */
EOF

printf "We just finished converting keysymdef.h to gdkkeysyms.h and gdkkeysyms-compat.h\nThank you\n";
