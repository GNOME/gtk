#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# compose-parse.py, version 1.4
#
# multifunction script that helps manage the compose sequence table in GTK+ (gtk/gtkimcontextsimple.c)
# the script produces statistics and information about the whole process, run with --help for more.
#
# You may need to switch your python installation to utf-8, if you get 'ascii' codec errors.
#
# Complain to Simos Xenitellis (simos@gnome.org, http://simos.info/blog) for this craft.

from re			import findall, match, split, sub
from string		import atoi
from unicodedata	import normalize
from urllib 		import urlretrieve
from os.path		import isfile, getsize
from copy 		import copy

import sys
import getopt

# We grab files off the web, left and right.
URL_COMPOSE = 'http://cgit.freedesktop.org/xorg/lib/libX11/plain/nls/en_US.UTF-8/Compose.pre'
URL_KEYSYMSTXT = "http://www.cl.cam.ac.uk/~mgk25/ucs/keysyms.txt"
URL_GDKKEYSYMSH = "http://git.gnome.org/browse/gtk%2B/plain/gdk/gdkkeysyms.h"
URL_UNICODEDATATXT = 'http://www.unicode.org/Public/6.0.0/ucd/UnicodeData.txt'
FILENAME_COMPOSE_SUPPLEMENTARY = 'gtk-compose-lookaside.txt'

# We currently support keysyms of size 2; once upstream xorg gets sorted, 
# we might produce some tables with size 2 and some with size 4.
SIZEOFINT = 2

# Current max compose sequence length; in case it gets increased.
WIDTHOFCOMPOSETABLE = 5

keysymdatabase = {}
keysymunicodedatabase = {}
unicodedatabase = {}

headerfile_start = """/* GTK - The GIMP Tool Kit
 * Copyright (C) 2007, 2008 GNOME Foundation
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

/*
 * File auto-generated from script found at http://bugzilla.gnome.org/show_bug.cgi?id=321896
 * using the input files
 *  Input   : http://cgit.freedesktop.org/xorg/lib/libX11/plain/nls/en_US.UTF-8/Compose.pre
 *  Input   : http://www.cl.cam.ac.uk/~mgk25/ucs/keysyms.txt
 *  Input   : http://www.unicode.org/Public/UNIDATA/UnicodeData.txt
 *
 * This table is optimised for space and requires special handling to access the content.
 * This table is used solely by http://svn.gnome.org/viewcvs/gtk%2B/trunk/gtk/gtkimcontextsimple.c
 * 
 * The resulting file is placed at http://svn.gnome.org/viewcvs/gtk%2B/trunk/gtk/gtkimcontextsimpleseqs.h
 * This file is described in bug report http://bugzilla.gnome.org/show_bug.cgi?id=321896
 */

/*
 * Modified by the GTK+ Team and others 2007, 2008.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_IM_CONTEXT_SIMPLE_SEQS_H__
#define __GTK_IM_CONTEXT_SIMPLE_SEQS_H__

/* === These are the original comments of the file; we keep for historical purposes ===
 *
 * The following table was generated from the X compose tables include with
 * XFree86 4.0 using a set of Perl scripts. Contact Owen Taylor <otaylor@redhat.com>
 * to obtain the relevant perl scripts.
 *
 * The following compose letter letter sequences confliced
 *   Dstroke/dstroke and ETH/eth; resolved to Dstroke (Croation, Vietnamese, Lappish), over
 *                                ETH (Icelandic, Faroese, old English, IPA)  [ D- -D d- -d ]
 *   Amacron/amacron and ordfeminine; resolved to ordfeminine                 [ _A A_ a_ _a ]
 *   Amacron/amacron and Atilde/atilde; resolved to atilde                    [ -A A- a- -a ]
 *   Omacron/Omacron and masculine; resolved to masculine                     [ _O O_ o_ _o ]
 *   Omacron/omacron and Otilde/atilde; resolved to otilde                    [ -O O- o- -o ]
 *
 * [ Amacron and Omacron are in Latin-4 (Baltic). ordfeminine and masculine are used for
 *   spanish. atilde and otilde are used at least for Portuguese ]
 *
 *   at and Aring; resolved to Aring                                          [ AA ]
 *   guillemotleft and caron; resolved to guillemotleft                       [ << ]
 *   ogonek and cedilla; resolved to cedilla                                  [ ,, ]
 *
 * This probably should be resolved by first checking an additional set of compose tables
 * that depend on the locale or selected input method.
 */

static const guint16 gtk_compose_seqs_compact[] = {"""

headerfile_end = """};

#endif /* __GTK_IM_CONTEXT_SIMPLE_SEQS_H__ */
"""

def stringtohex(str): return atoi(str, 16)

def factorial(n): 
	if n <= 1:
		return 1
	else:
		return n * factorial(n-1)

def uniq(*args) :
	""" Performs a uniq operation on a list or lists """
    	theInputList = []
    	for theList in args:
    	   theInputList += theList
    	theFinalList = []
    	for elem in theInputList:
		if elem not in theFinalList:
          		theFinalList.append(elem)
    	return theFinalList



def all_permutations(seq):
	""" Borrowed from http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/252178 """
	""" Produces all permutations of the items of a list """
    	if len(seq) <=1:
    	    yield seq
    	else:
    	    for perm in all_permutations(seq[1:]):
    	        for i in range(len(perm)+1):
    	            #nb str[0:1] works in both string and list contexts
        	        yield perm[:i] + seq[0:1] + perm[i:]

def usage():
	print """compose-parse available parameters:
	-h, --help		this craft
	-s, --statistics	show overall statistics (both algorithmic, non-algorithmic)
	-a, --algorithmic	show sequences saved with algorithmic optimisation
	-g, --gtk		show entries that go to GTK+
	-u, --unicodedatatxt	show compose sequences derived from UnicodeData.txt (from unicode.org)
	-v, --verbose		show verbose output
        -p, --plane1		show plane1 compose sequences
	-n, --numeric		when used with --gtk, create file with numeric values only
	-e, --gtk-expanded	when used with --gtk, create file that repeats first column; not usable in GTK+

	Default is to show statistics.
	"""

try: 
	opts, args = getopt.getopt(sys.argv[1:], "pvgashune", ["help", "algorithmic", "statistics", "unicodedatatxt", 
		"stats", "gtk", "verbose", "plane1", "numeric", "gtk-expanded"])
except: 
	usage()
	sys.exit(2)

opt_statistics = False
opt_algorithmic = False
opt_gtk = False
opt_unicodedatatxt = False
opt_verbose = False
opt_plane1 = False
opt_numeric = False
opt_gtkexpanded = False

for o, a in opts:
	if o in ("-h", "--help"):
		usage()
		sys.exit()
	if o in ("-s", "--statistics"):
		opt_statistics = True
	if o in ("-a", "--algorithmic"):
		opt_algorithmic = True
	if o in ("-g", "--gtk"):
		opt_gtk = True	
	if o in ("-u", "--unicodedatatxt"):
		opt_unicodedatatxt = True
	if o in ("-v", "--verbose"):
		opt_verbose = True
	if o in ("-p", "--plane1"):
		opt_plane1 = True
	if o in ("-n", "--numeric"):
		opt_numeric = True
	if o in ("-e", "--gtk-expanded"):
		opt_gtkexpanded = True

if not opt_algorithmic and not opt_gtk and not opt_unicodedatatxt:
	opt_statistics = True

def download_hook(blocks_transferred, block_size, file_size):
	""" A download hook to provide some feedback when downloading """
	if blocks_transferred == 0:
		if file_size > 0:
			if opt_verbose:
				print "Downloading", file_size, "bytes: ",
		else:	
			if opt_verbose:
				print "Downloading: ",
	sys.stdout.write('#')
	sys.stdout.flush()


def download_file(url):
	""" Downloads a file provided a URL. Returns the filename. """
	""" Borks on failure """
	localfilename = url.split('/')[-1]
        if not isfile(localfilename) or getsize(localfilename) <= 0:
		if opt_verbose:
			print "Downloading ", url, "..."
		try: 
			urlretrieve(url, localfilename, download_hook)
		except IOError, (errno, strerror):
			print "I/O error(%s): %s" % (errno, strerror)
			sys.exit(-1)
		except:
			print "Unexpected error: ", sys.exc_info()[0]
			sys.exit(-1)
		print " done."
        else:
		if opt_verbose:
                	print "Using cached file for ", url
	return localfilename

def process_gdkkeysymsh():
	""" Opens the gdkkeysyms.h file from GTK+/gdk/gdkkeysyms.h """
	""" Fills up keysymdb with contents """
	filename_gdkkeysymsh = download_file(URL_GDKKEYSYMSH)
	try: 
		gdkkeysymsh = open(filename_gdkkeysymsh, 'r')
	except IOError, (errno, strerror):
		print "I/O error(%s): %s" % (errno, strerror)
		sys.exit(-1)
	except:
		print "Unexpected error: ", sys.exc_info()[0]
		sys.exit(-1)

	""" Parse the gdkkeysyms.h file and place contents in  keysymdb """
	linenum_gdkkeysymsh = 0
	keysymdb = {}
	for line in gdkkeysymsh.readlines():
		linenum_gdkkeysymsh += 1
		line = line.strip()
		if line == "" or not match('^#define GDK_KEY_', line):
			continue
		components = split('\s+', line)
		if len(components) < 3:
			print "Invalid line %(linenum)d in %(filename)s: %(line)s"\
			% {'linenum': linenum_gdkkeysymsh, 'filename': filename_gdkkeysymsh, 'line': line}
			print "Was expecting 3 items in the line"
			sys.exit(-1)
		if not match('^GDK_KEY_', components[1]):
			print "Invalid line %(linenum)d in %(filename)s: %(line)s"\
			% {'linenum': linenum_gdkkeysymsh, 'filename': filename_gdkkeysymsh, 'line': line}
			print "Was expecting a keysym starting with GDK_KEY_"
			sys.exit(-1)
		if match('^0x[0-9a-fA-F]+$', components[2]):
			unival = long(components[2][2:], 16)
			if unival == 0:
				continue
			keysymdb[components[1][8:]] = unival
		else:
			print "Invalid line %(linenum)d in %(filename)s: %(line)s"\
			% {'linenum': linenum_gdkkeysymsh, 'filename': filename_gdkkeysymsh, 'line': line}
			print "Was expecting a hexadecimal number at the end of the line"
			sys.exit(-1)
	gdkkeysymsh.close()

	""" Patch up the keysymdb with some of our own stuff """

	""" This is for a missing keysym from the currently upstream file """
	###keysymdb['dead_stroke'] = 0x338

	""" This is for a missing keysym from the currently upstream file """
	###keysymdb['dead_belowring'] = 0x323
	###keysymdb['dead_belowmacron'] = 0x331
	###keysymdb['dead_belowcircumflex'] = 0x32d
	###keysymdb['dead_belowtilde'] = 0x330
	###keysymdb['dead_belowbreve'] = 0x32e
	###keysymdb['dead_belowdiaeresis'] = 0x324

	""" This is^Wwas preferential treatment for Greek """
	# keysymdb['dead_tilde'] = 0x342  		
	""" This is^was preferential treatment for Greek """
	#keysymdb['combining_tilde'] = 0x342	

	""" Fixing VoidSymbol """
	keysymdb['VoidSymbol'] = 0xFFFF

	return keysymdb

def process_keysymstxt():
	""" Grabs and opens the keysyms.txt file that Markus Kuhn maintains """
	""" This file keeps a record between keysyms <-> unicode chars """
	filename_keysymstxt = download_file(URL_KEYSYMSTXT)
	try: 
		keysymstxt = open(filename_keysymstxt, 'r')
	except IOError, (errno, strerror):
		print "I/O error(%s): %s" % (errno, strerror)
		sys.exit(-1)
	except:
		print "Unexpected error: ", sys.exc_info()[0]
		sys.exit(-1)

	""" Parse the keysyms.txt file and place content in  keysymdb """
	linenum_keysymstxt = 0
	keysymdb = {}
	for line in keysymstxt.readlines():
		linenum_keysymstxt += 1
		line = line.strip()
		if line == "" or match('^#', line):
			continue
		components = split('\s+', line)
		if len(components) < 5:
			print "Invalid line %(linenum)d in %(filename)s: %(line)s'"\
			% {'linenum': linenum_keysymstxt, 'filename': filename_keysymstxt, 'line': line}
			print "Was expecting 5 items in the line"
			sys.exit(-1)
		if match('^U[0-9a-fA-F]+$', components[1]):
			unival = long(components[1][1:], 16)
		if unival == 0:
			continue
		keysymdb[components[4]] = unival
	keysymstxt.close()

	""" Patch up the keysymdb with some of our own stuff """
	""" This is for a missing keysym from the currently upstream file """
	keysymdb['dead_belowring'] = 0x323
	keysymdb['dead_belowmacron'] = 0x331
	keysymdb['dead_belowcircumflex'] = 0x32d
	keysymdb['dead_belowtilde'] = 0x330
	keysymdb['dead_belowbreve'] = 0x32e
	keysymdb['dead_belowdiaeresis'] = 0x324

	""" This is preferential treatment for Greek """
	""" => we get more savings if used for Greek """
	# keysymdb['dead_tilde'] = 0x342  		
	""" This is preferential treatment for Greek """
	# keysymdb['combining_tilde'] = 0x342	

	""" This is for a missing keysym from Markus Kuhn's db """
	keysymdb['dead_stroke'] = 0x338
	""" This is for a missing keysym from Markus Kuhn's db """
	keysymdb['Oslash'] = 0x0d8		
	""" This is for a missing keysym from Markus Kuhn's db """
	keysymdb['Ssharp'] = 0x1e9e

	""" This is for a missing (recently added) keysym """
	keysymdb['dead_psili'] = 0x313		
	""" This is for a missing (recently added) keysym """
	keysymdb['dead_dasia'] = 0x314		

	""" Allows to import Multi_key sequences """
	keysymdb['Multi_key'] = 0xff20

        keysymdb['zerosubscript'] = 0x2080
        keysymdb['onesubscript'] = 0x2081
        keysymdb['twosubscript'] = 0x2082
        keysymdb['threesubscript'] = 0x2083
        keysymdb['foursubscript'] = 0x2084
        keysymdb['fivesubscript'] = 0x2085
        keysymdb['sixsubscript'] = 0x2086
        keysymdb['sevensubscript'] = 0x2087
        keysymdb['eightsubscript'] = 0x2088
        keysymdb['ninesubscript'] = 0x2089
        keysymdb['dead_doublegrave'] = 0x030F
        keysymdb['dead_invertedbreve'] = 0x0311
        keysymdb['dead_belowcomma'] = 0xfe6e
        keysymdb['dead_currency'] = 0xfe6f
        keysymdb['dead_greek'] = 0xfe8c

	return keysymdb

def keysymvalue(keysym, file = "n/a", linenum = 0):
	""" Extracts a value from the keysym """
	""" Find the value of keysym, using the data from keysyms """
	""" Use file and linenum to when reporting errors """
	if keysym == "":
		return 0
       	if keysymdatabase.has_key(keysym):
               	return keysymdatabase[keysym]
       	elif keysym[0] == 'U' and match('[0-9a-fA-F]+$', keysym[1:]):
               	return atoi(keysym[1:], 16)
       	elif keysym[:2] == '0x' and match('[0-9a-fA-F]+$', keysym[2:]):
		return atoi(keysym[2:], 16)
	else:
        	print 'keysymvalue: UNKNOWN{%(keysym)s}' % { "keysym": keysym }
               	#return -1
		sys.exit(-1)

def keysymunicodevalue(keysym, file = "n/a", linenum = 0):
	""" Extracts a value from the keysym """
	""" Find the value of keysym, using the data from keysyms """
	""" Use file and linenum to when reporting errors """
	if keysym == "":
		return 0
       	if keysymunicodedatabase.has_key(keysym):
               	return keysymunicodedatabase[keysym]
       	elif keysym[0] == 'U' and match('[0-9a-fA-F]+$', keysym[1:]):
               	return atoi(keysym[1:], 16)
       	elif keysym[:2] == '0x' and match('[0-9a-fA-F]+$', keysym[2:]):
		return atoi(keysym[2:], 16)
	else:
        	print 'keysymunicodevalue: UNKNOWN{%(keysym)s}' % { "keysym": keysym }
               	sys.exit(-1)

def rename_combining(seq):
	filtered_sequence = []
	for ks in seq:
		if findall('^combining_', ks):
			ks = sub('^combining_', 'dead_', ks)
                if ks == 'dead_double_grave':
                        ks = 'dead_doublegrave'
                if ks == 'dead_inverted_breve':
                        ks = 'dead_invertedbreve'
		filtered_sequence.append(ks)
	return filtered_sequence


keysymunicodedatabase = process_keysymstxt()
keysymdatabase = process_gdkkeysymsh()

""" Grab and open the compose file from upstream """
filename_compose = download_file(URL_COMPOSE)
try: 
	composefile = open(filename_compose, 'r')
except IOError, (errno, strerror):
	print "I/O error(%s): %s" % (errno, strerror)
	sys.exit(-1)
except:
	print "Unexpected error: ", sys.exc_info()[0]
	sys.exit(-1)

""" Look if there is a lookaside (supplementary) compose file in the current
    directory, and if so, open, then merge with upstream Compose file.
"""
xorg_compose_sequences_raw = []
for seq in composefile.readlines():
        xorg_compose_sequences_raw.append(seq)

try:
        composefile_lookaside = open(FILENAME_COMPOSE_SUPPLEMENTARY, 'r')
        for seq in composefile_lookaside.readlines():
                xorg_compose_sequences_raw.append(seq)
except IOError, (errno, strerror):
        if opt_verbose:
                print "I/O error(%s): %s" % (errno, strerror)
                print "Did not find lookaside compose file. Continuing..."
except:
        print "Unexpected error: ", sys.exc_info()[0]
        sys.exit(-1)

""" Parse the compose file in  xorg_compose_sequences"""
xorg_compose_sequences = []
xorg_compose_sequences_algorithmic = []
linenum_compose = 0
comment_nest_depth = 0
for line in xorg_compose_sequences_raw:
	linenum_compose += 1
	line = line.strip()
	if match("^XCOMM", line) or match("^#", line):
		continue

	line = sub(r"\/\*([^\*]*|[\*][^/])\*\/", "", line)

	comment_start = line.find("/*")

	if comment_start >= 0:
		if comment_nest_depth == 0:
			line = line[:comment_start]
		else:
			line = ""

		comment_nest_depth += 1
	else:
		comment_end = line.find("*/")

		if comment_end >= 0:
			comment_nest_depth -= 1

		if comment_nest_depth < 0:
			print "Invalid comment %(linenum_compose)d in %(filename)s: \
			Closing '*/' without opening '/*'" % { "linenum_compose": linenum_compose, "filename": filename_compose }
			exit(-1)

		if comment_nest_depth > 0:
			line = ""
		else:
			line = line[comment_end + 2:]

	if line is "":
		continue

	#line = line[:-1]
	components = split(':', line, 1)
	if len(components) != 2:
		print "Invalid line %(linenum_compose)d in %(filename)s: No sequence\
		/value pair found" % { "linenum_compose": linenum_compose, "filename": filename_compose }
		exit(-1)
	(seq, val ) = split(':', line, 1)
	seq = seq.strip()
	val = val.strip()
	raw_sequence = findall('\w+', seq)
	values = split('\s+', val)
	unichar_temp = split('"', values[0])
	unichar_utf8 = unichar_temp[1]
	if len(values) == 1:
		continue
	codepointstr = values[1]
	if values[1] == '#':
		# No codepoints that are >1 characters yet.
		continue
	if raw_sequence[0][0] == 'U' and match('[0-9a-fA-F]+$', raw_sequence[0][1:]):
		raw_sequence[0] = '0x' + raw_sequence[0][1:]
	if  match('^U[0-9a-fA-F]+$', codepointstr):
		codepoint = long(codepointstr[1:], 16)
	elif keysymunicodedatabase.has_key(codepointstr):
		#if keysymdatabase[codepointstr] != keysymunicodedatabase[codepointstr]:
			#print "DIFFERENCE: 0x%(a)X 0x%(b)X" % { "a": keysymdatabase[codepointstr], "b": keysymunicodedatabase[codepointstr]},
			#print raw_sequence, codepointstr
		codepoint = keysymunicodedatabase[codepointstr]
	else:
		unichar = unicode(unichar_utf8, 'utf-8')
		codepoint = ord(unichar)
	sequence = rename_combining(raw_sequence)
	reject_this = False
	for i in sequence:
		if keysymvalue(i) > 0xFFFF:
			reject_this = True
			if opt_plane1:
				print sequence
			break
		if keysymvalue(i) < 0:
			reject_this = True
			break
	if reject_this:
		continue
	if "U0342" in sequence or \
		"U0313" in sequence or \
		"U0314" in sequence or \
		"0x0313" in sequence or \
		"0x0342" in sequence or \
		"0x0314" in sequence:
		continue
	if codepoint > 0xFFFF:
                if opt_verbose:
		    print "Ignore the line greater than guint16:\n%s" % line
		continue
	#for i in range(len(sequence)):
	#	if sequence[i] == "0x0342":
	#		sequence[i] = "dead_tilde"
	if "Multi_key" not in sequence:
		""" Ignore for now >0xFFFF keysyms """
		if codepoint < 0xFFFF:
			original_sequence = copy(sequence)
			stats_sequence = copy(sequence)
			base = sequence.pop()
			basechar = keysymvalue(base, filename_compose, linenum_compose)
			
			if basechar < 0xFFFF:
				counter = 1
				unisequence = []
				not_normalised = True
				skipping_this = False
				for i in range(0, len(sequence)):
					""" If the sequence has dead_tilde and is for Greek, we don't do algorithmically 
					    because of lack of dead_perispomeni (i.e. conflict)
					"""
					bc = basechar
					"""if sequence[-1] == "dead_tilde" and (bc >= 0x370 and bc <= 0x3ff) or (bc >= 0x1f00 and bc <= 0x1fff):
						skipping_this = True
						break
					if sequence[-1] == "dead_horn" and (bc >= 0x370 and bc <= 0x3ff) or (bc >= 0x1f00 and bc <= 0x1fff):
						skipping_this = True
						break
					if sequence[-1] == "dead_ogonek" and (bc >= 0x370 and bc <= 0x3ff) or (bc >= 0x1f00 and bc <= 0x1fff):
						skipping_this = True
						break
					if sequence[-1] == "dead_psili":
						sequence[i] = "dead_horn"
					if sequence[-1] == "dead_dasia":
						sequence[-1] = "dead_ogonek"
					"""
					unisequence.append(unichr(keysymunicodevalue(sequence.pop(), filename_compose, linenum_compose)))
					
				if skipping_this:
					unisequence = []
				for perm in all_permutations(unisequence):
					# print counter, original_sequence, unichr(basechar) + "".join(perm)
					# print counter, map(unichr, perm)
					normalized = normalize('NFC', unichr(basechar) + "".join(perm))
					if len(normalized) == 1:
						# print 'Base: %(base)s [%(basechar)s], produces [%(unichar)s] (0x%(codepoint)04X)' \
						# % { "base": base, "basechar": unichr(basechar), "unichar": unichar, "codepoint": codepoint },
						# print "Normalized: [%(normalized)s] SUCCESS %(c)d" % { "normalized": normalized, "c": counter }
						stats_sequence_data = map(keysymunicodevalue, stats_sequence)
						stats_sequence_data.append(normalized)
						xorg_compose_sequences_algorithmic.append(stats_sequence_data)
						not_normalised = False
						break;
					counter += 1
				if not_normalised:
					original_sequence.append(codepoint)
					xorg_compose_sequences.append(original_sequence)
					""" print xorg_compose_sequences[-1] """
					
			else:
				print "Error in base char !?!"
				exit(-2)
		else:
			print "OVER", sequence
			exit(-1)
	else:
		sequence.append(codepoint)
		xorg_compose_sequences.append(sequence)
		""" print xorg_compose_sequences[-1] """

def sequence_cmp(x, y):
	if keysymvalue(x[0]) > keysymvalue(y[0]):
		return 1
	elif keysymvalue(x[0]) < keysymvalue(y[0]):
		return -1
	elif len(x) > len(y):
		return 1
	elif len(x) < len(y):
		return -1
	elif keysymvalue(x[1]) > keysymvalue(y[1]):
		return 1
	elif keysymvalue(x[1]) < keysymvalue(y[1]):
		return -1
	elif len(x) < 4:
		return 0
	elif keysymvalue(x[2]) > keysymvalue(y[2]):
		return 1
	elif keysymvalue(x[2]) < keysymvalue(y[2]):
		return -1
	elif len(x) < 5:
		return 0
	elif keysymvalue(x[3]) > keysymvalue(y[3]):
		return 1
	elif keysymvalue(x[3]) < keysymvalue(y[3]):
		return -1
	elif len(x) < 6:
		return 0
	elif keysymvalue(x[4]) > keysymvalue(y[4]):
		return 1
	elif keysymvalue(x[4]) < keysymvalue(y[4]):
		return -1
	else:
		return 0

def sequence_unicode_cmp(x, y):
	if keysymunicodevalue(x[0]) > keysymunicodevalue(y[0]):
		return 1
	elif keysymunicodevalue(x[0]) < keysymunicodevalue(y[0]):
		return -1
	elif len(x) > len(y):
		return 1
	elif len(x) < len(y):
		return -1
	elif keysymunicodevalue(x[1]) > keysymunicodevalue(y[1]):
		return 1
	elif keysymunicodevalue(x[1]) < keysymunicodevalue(y[1]):
		return -1
	elif len(x) < 4:
		return 0
	elif keysymunicodevalue(x[2]) > keysymunicodevalue(y[2]):
		return 1
	elif keysymunicodevalue(x[2]) < keysymunicodevalue(y[2]):
		return -1
	elif len(x) < 5:
		return 0
	elif keysymunicodevalue(x[3]) > keysymunicodevalue(y[3]):
		return 1
	elif keysymunicodevalue(x[3]) < keysymunicodevalue(y[3]):
		return -1
	elif len(x) < 6:
		return 0
	elif keysymunicodevalue(x[4]) > keysymunicodevalue(y[4]):
		return 1
	elif keysymunicodevalue(x[4]) < keysymunicodevalue(y[4]):
		return -1
	else:
		return 0

def sequence_algorithmic_cmp(x, y):
	if len(x) < len(y):
		return -1
	elif len(x) > len(y):
		return 1
	else:
		for i in range(len(x)):
			if x[i] < y[i]:
				return -1
			elif x[i] > y[i]:
				return 1
	return 0


xorg_compose_sequences.sort(sequence_cmp)

xorg_compose_sequences_uniqued = []
first_time = True
item = None
for next_item in xorg_compose_sequences:
	if first_time:
		first_time = False
		item = next_item
	if sequence_unicode_cmp(item, next_item) != 0:
		xorg_compose_sequences_uniqued.append(item)
	item = next_item

xorg_compose_sequences = copy(xorg_compose_sequences_uniqued)

counter_multikey = 0
for item in xorg_compose_sequences:
	if findall('Multi_key', "".join(item[:-1])) != []:
		counter_multikey += 1

xorg_compose_sequences_algorithmic.sort(sequence_algorithmic_cmp)
xorg_compose_sequences_algorithmic_uniqued = uniq(xorg_compose_sequences_algorithmic)

firstitem = ""
num_first_keysyms = 0
zeroes = 0
num_entries = 0
num_algorithmic_greek = 0
for sequence in xorg_compose_sequences:
	if keysymvalue(firstitem) != keysymvalue(sequence[0]): 
		firstitem = sequence[0]
		num_first_keysyms += 1
	zeroes += 6 - len(sequence) + 1
	num_entries += 1

for sequence in xorg_compose_sequences_algorithmic_uniqued:
	ch = ord(sequence[-1:][0])
	if ch >= 0x370 and ch <= 0x3ff or ch >= 0x1f00 and ch <= 0x1fff:
		num_algorithmic_greek += 1
		

if opt_algorithmic:
	for sequence in xorg_compose_sequences_algorithmic_uniqued:
		letter = "".join(sequence[-1:])
		print '0x%(cp)04X, %(uni)s, seq: [ <0x%(base)04X>,' % { 'cp': ord(unicode(letter)), 'uni': letter.encode('utf-8'), 'base': sequence[-2] },
		for elem in sequence[:-2]:
			print "<0x%(keysym)04X>," % { 'keysym': elem },
		""" Yeah, verified... We just want to keep the output similar to -u, so we can compare/sort easily """
		print "], recomposed as", letter.encode('utf-8'), "verified"

def num_of_keysyms(seq):
	return len(seq) - 1

def convert_UnotationToHex(arg):
	if isinstance(arg, str):
		if match('^U[0-9A-F][0-9A-F][0-9A-F][0-9A-F]$', arg):
			return sub('^U', '0x', arg)
	return arg

def addprefix_GDK(arg):
	if match('^0x', arg):
		return '%(arg)s, ' % { 'arg': arg }
	elif match('^U[0-9A-F][0-9A-F][0-9A-F][0-9A-F]$', arg.upper()):
                keysym = ''
                for k, c in keysymunicodedatabase.items():
                    if c == keysymvalue(arg):
                        keysym = k
                        break
                if keysym != '':
		    return 'GDK_KEY_%(arg)s, ' % { 'arg': keysym }
                else:
		    return '0x%(arg)04X, ' % { 'arg': keysymvalue(arg) }
	else:
		return 'GDK_KEY_%(arg)s, ' % { 'arg': arg }

if opt_gtk:
	first_keysym = ""
	sequence = []
	compose_table = []
	ct_second_part = []
	ct_sequence_width = 2
	start_offset = num_first_keysyms * (WIDTHOFCOMPOSETABLE+1)
	we_finished = False
	counter = 0

	sequence_iterator = iter(xorg_compose_sequences)
	sequence = sequence_iterator.next()
	while True:
		first_keysym = sequence[0]					# Set the first keysym
		compose_table.append([first_keysym, 0, 0, 0, 0, 0])
		while sequence[0] == first_keysym:
			compose_table[counter][num_of_keysyms(sequence)-1] += 1
			try:
				sequence = sequence_iterator.next()
			except StopIteration:
				we_finished = True
				break
		if we_finished:
			break
		counter += 1

	ct_index = start_offset
	for line_num in range(len(compose_table)):
		for i in range(WIDTHOFCOMPOSETABLE):
			occurences = compose_table[line_num][i+1]
			compose_table[line_num][i+1] = ct_index
			ct_index += occurences * (i+2)

	for sequence in xorg_compose_sequences:
		ct_second_part.append(map(convert_UnotationToHex, sequence))

	print headerfile_start
	for i in compose_table:
		if opt_gtkexpanded:
			print "0x%(ks)04X," % { "ks": keysymvalue(i[0]) },
			print '%(str)s' % { 'str': "".join(map(lambda x : str(x) + ", ", i[1:])) }
		elif not match('^0x', i[0]):
			print 'GDK_KEY_%(str)s' % { 'str': "".join(map(lambda x : str(x) + ", ", i)) }
		else:
			print '%(str)s' % { 'str': "".join(map(lambda x : str(x) + ", ", i)) }
	for i in ct_second_part:
		if opt_numeric:
			for ks in i[1:][:-1]:
				print '0x%(seq)04X, ' % { 'seq': keysymvalue(ks) },
			print '0x%(cp)04X, ' % { 'cp':i[-1] }
			"""
			for ks in i[:-1]:
				print '0x%(seq)04X, ' % { 'seq': keysymvalue(ks) },
			print '0x%(cp)04X, ' % { 'cp':i[-1] }
			"""
		elif opt_gtkexpanded:
			print '%(seq)s0x%(cp)04X, ' % { 'seq': "".join(map(addprefix_GDK, i[:-1])), 'cp':i[-1] }
		else:
			print '%(seq)s0x%(cp)04X, ' % { 'seq': "".join(map(addprefix_GDK, i[:-1][1:])), 'cp':i[-1] }
	print headerfile_end 

def redecompose(codepoint):
	(name, decomposition, combiningclass) = unicodedatabase[codepoint]
	if decomposition[0] == '' or decomposition[0] == '0':
		return [codepoint]
	if match('<\w+>', decomposition[0]):
		numdecomposition = map(stringtohex, decomposition[1:])
		return map(redecompose, numdecomposition)
	numdecomposition = map(stringtohex, decomposition)
	return map(redecompose, numdecomposition)

def process_unicodedata_file(verbose = False):
	""" Grab from wget http://www.unicode.org/Public/UNIDATA/UnicodeData.txt """
	filename_unicodedatatxt = download_file(URL_UNICODEDATATXT)
	try: 
		unicodedatatxt = open(filename_unicodedatatxt, 'r')
	except IOError, (errno, strerror):
		print "I/O error(%s): %s" % (errno, strerror)
		sys.exit(-1)
	except:
		print "Unexpected error: ", sys.exc_info()[0]
		sys.exit(-1)
	for line in unicodedatatxt.readlines():
		if line[0] == "" or line[0] == '#':
			continue
		line = line[:-1]
		uniproperties = split(';', line)
		codepoint = stringtohex(uniproperties[0])
		""" We don't do Plane 1 or CJK blocks. The latter require reading additional files. """
		if codepoint > 0xFFFF or (codepoint >= 0x4E00 and codepoint <= 0x9FFF) or (codepoint >= 0xF900 and codepoint <= 0xFAFF): 
			continue
		name = uniproperties[1]
		category = uniproperties[2]
		combiningclass = uniproperties[3]
		decomposition = uniproperties[5]
		unicodedatabase[codepoint] = [name, split('\s+', decomposition), combiningclass]
	
	counter_combinations = 0
	counter_combinations_greek = 0
	counter_entries = 0
	counter_entries_greek = 0

	for item in unicodedatabase.keys():
		(name, decomposition, combiningclass) = unicodedatabase[item]
		if decomposition[0] == '':
			continue
			print name, "is empty"
		elif match('<\w+>', decomposition[0]):
			continue
			print name, "has weird", decomposition[0]
		else:
			sequence = map(stringtohex, decomposition)
			chrsequence = map(unichr, sequence)
			normalized = normalize('NFC', "".join(chrsequence))
			
			""" print name, sequence, "Combining: ", "".join(chrsequence), normalized, len(normalized),  """
			decomposedsequence = []
			for subseq in map(redecompose, sequence):
				for seqitem in subseq:
					if isinstance(seqitem, list):
						for i in seqitem:
							if isinstance(i, list):
								for j in i:
									decomposedsequence.append(j)
							else:
								decomposedsequence.append(i)
					else:
						decomposedsequence.append(seqitem)
			recomposedchar = normalize('NFC', "".join(map(unichr, decomposedsequence)))
			if len(recomposedchar) == 1 and len(decomposedsequence) > 1:
				counter_entries += 1
				counter_combinations += factorial(len(decomposedsequence)-1)
				ch = item
				if ch >= 0x370 and ch <= 0x3ff or ch >= 0x1f00 and ch <= 0x1fff:
					counter_entries_greek += 1
					counter_combinations_greek += factorial(len(decomposedsequence)-1)
				if verbose:
					print "0x%(cp)04X, %(uni)c, seq:" % { 'cp':item, 'uni':unichr(item) },
					print "[",
					for elem in decomposedsequence:
						print '<0x%(hex)04X>,' % { 'hex': elem },
					print "], recomposed as", recomposedchar,
					if unichr(item) == recomposedchar:
						print "verified"
	
	if verbose == False:
		print "Unicode statistics from UnicodeData.txt"
		print "Number of entries that can be algorithmically produced     :", counter_entries
		print "  of which are for Greek                                   :", counter_entries_greek
		print "Number of compose sequence combinations requiring          :", counter_combinations
		print "  of which are for Greek                                   :", counter_combinations_greek
		print "Note: We do not include partial compositions, "
		print "thus the slight discrepancy in the figures"
		print

if opt_unicodedatatxt:
	process_unicodedata_file(True)

if opt_statistics:
	print
	print "Total number of compose sequences (from file)              :", len(xorg_compose_sequences) + len(xorg_compose_sequences_algorithmic)
	print "  of which can be expressed algorithmically                :", len(xorg_compose_sequences_algorithmic)
	print "  of which cannot be expressed algorithmically             :", len(xorg_compose_sequences) 
	print "    of which have Multi_key                                :", counter_multikey
	print 
	print "Algorithmic (stats for Xorg Compose file)"
	print "Number of sequences off due to algo from file (len(array)) :", len(xorg_compose_sequences_algorithmic)
	print "Number of sequences off due to algo (uniq(sort(array)))    :", len(xorg_compose_sequences_algorithmic_uniqued)
	print "  of which are for Greek                                   :", num_algorithmic_greek
	print 
	process_unicodedata_file()
	print "Not algorithmic (stats from Xorg Compose file)"
	print "Number of sequences                                        :", len(xorg_compose_sequences) 
	print "Flat array looks like                                      :", len(xorg_compose_sequences), "rows of 6 integers (2 bytes per int, or 12 bytes per row)"
	print "Flat array would have taken up (in bytes)                  :", num_entries * 2 * 6, "bytes from the GTK+ library"
	print "Number of items in flat array                              :", len(xorg_compose_sequences) * 6
	print "  of which are zeroes                                      :", zeroes, "or ", (100 * zeroes) / (len(xorg_compose_sequences) * 6), " per cent"
	print "Number of different first items                            :", num_first_keysyms
	print "Number of max bytes (if using flat array)                  :", num_entries * 2 * 6
	print "Number of savings                                          :", zeroes * 2 - num_first_keysyms * 2 * 5
	print 
	print "Memory needs if both algorithmic+optimised table in latest Xorg compose file"
	print "                                                           :", num_entries * 2 * 6 - zeroes * 2 + num_first_keysyms * 2 * 5
	print
	print "Existing (old) implementation in GTK+"
	print "Number of sequences in old gtkimcontextsimple.c            :", 691
	print "The existing (old) implementation in GTK+ takes up         :", 691 * 2 * 12, "bytes"
