#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import re
import os
from collections import *

out_file = sys.argv[1]
in_files = sys.argv[2:]


file_output = """
typedef GtkWidget *(*GDoDemoFunc) (GtkWidget *do_widget);

typedef struct _Demo Demo;

struct _Demo
{
  gchar *name;
  gchar *title;
  gchar *filename;
  GDoDemoFunc func;
  Demo *children;
};

"""

# Demo = namedtuple('Demo', ['name', 'title', 'file', 'func'])

demos = []

for demo_file in in_files:
    filename =  demo_file[demo_file.rfind('/')+1:]
    demo_name = filename.replace(".c", "")
    with open(demo_file, 'r', encoding='utf-8') as f:
        title = f.readline().replace("/*", "").strip()


    file_output += "GtkWidget *do_" + demo_name + " (GtkWidget *do_widget);\n"
    # demos += Demo(name = demo_name,
                  # title = title,
                  # file = demo_file,
                  # func = "do_" + title)
    demos.append((demo_name, title, filename, "do_" + demo_name, -1))

# Generate a List of "Parent names"
parents = []
parent_ids = []
parent_index = 0
for demo in demos:
    if "/" in demo[1]:
        slash_index = demo[1].index('/')
        parent_name = demo[1][:slash_index]
        do_break = False

        # Check for duplicates
        if not parent_name in parents:
            parents.append(parent_name)
            parent_ids.append(parent_index)
            demos.append(("NULL", parent_name, "NULL", "NULL", parent_index))

        parent_index = parent_index + 1


# For every child with a parent, generate a list of child demos
i = 0
for parent in parents:
    id = parent_ids[i]
    file_output += "\nDemo child" + str(id) + "[] = {\n"
    # iterate over all demos and check if the name starts with the given parent name
    for child in demos:
        if child[1].startswith(parent + "/"):
            title = child[1][child[1].rfind('/') + 1:]
            file_output += "  { \"" + child[0] + "\", \"" + title + "\", \"" + child[2] + "\", " + child[3] + ", NULL },\n"


    file_output += "  { NULL }\n};\n"
    i = i + 1


# Sort demos by title
demos = sorted(demos, key=lambda x: x[1])

file_output += "\nDemo gtk_demos[] = {\n"
for demo in demos:
    # Do not generate one of these for demos with a parent demo
    if "/" not in demo[1]:
        child_array = "NULL"
        name = demo[0];
        title = demo[1];
        file = demo[2]
        if name != "NULL":
            name = "\"" + name + "\""
        if title != "NULL":
            title = "\"" + title + "\""
        if file != "NULL":
            file = "\"" + file + "\""

        if demo[4] != -1:
            child_array = "child" + str(demo[4])
        file_output += "  { " + name + ", " + title + ", " + file + ", " + demo[3] + ", " + child_array + " },\n"

file_output += "  { NULL }\n};\n"

ofile = open(out_file, "w", encoding="utf-8")
ofile.write(file_output)
ofile.close()
