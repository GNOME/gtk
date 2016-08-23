#!/usr/bin/rdmd

import std.stdio;
import std.file;
import std.process;
import std.regex;
import std.array;
import std.algorithm;

void main(string[] args) {
	string out_file   = args[1];
	string[] in_files = args[3..$];

	auto regex = ctRegex!("g[td]k_[a-zA-Z0-9_]*_get_type");

	string[] funcs;

	foreach (filename; in_files) {
		auto file = File(filename);
		foreach (line; file.byLine()) {
			auto match = line.matchFirst(regex);
			if (!match.empty) {
				// *cough*, not exactly the fastest way to do this...
				if (!funcs.canFind(match.hit))
					funcs ~= cast(string)match.hit.idup;
			}
		}
	}

	funcs.sort();

	//writeln(funcs);
	//writeln(funcs.length);

	string file_output = "G_GNUC_BEGIN_IGNORE_DEPRECATIONS\n";
	foreach (func; funcs) {
		if (func.startsWith("gdk_x11") || func.startsWith("gtk_socket") || func.startsWith("gtk_plug")) {
			file_output ~= "#ifdef GDK_WINDOWING_X11\n*tp++ = " ~ func ~ "();\n#endif\n";
		} else if (func != ("gtk_text_handle_get_type")){
			file_output ~= "*tp++ = " ~ func ~ "();\n";
		}
	}

	std.file.write(out_file, file_output);
}
