.. _gtk4-builder-tool(1):

=================
gtk4-builder-tool
=================

-----------------------
GtkBuilder File Utility
-----------------------

SYNOPSIS
--------
|   **gtk4-builder-tool** <COMMAND> [OPTIONS...] <FILE>
|
|   **gtk4-builder-tool** validate <FILE>
|   **gtk4-builder-tool** enumerate <FILE>
|   **gtk4-builder-tool** simplify [OPTIONS...] <FILE>
|   **gtk4-builder-tool** preview [OPTIONS...] <FILE>
|   **gtk4-builder-tool** precompile [OPTIONS...] <FILE>

DESCRIPTION
-----------

``gtk4-builder-tool`` can perform various operations on GtkBuilder UI definition
files.

COMMANDS
--------

Validation
^^^^^^^^^^

The ``validate`` command validates the given UI definition file and reports
errors to ``stderr``.

Enumeration
^^^^^^^^^^^

The ``enumerate`` command lists all the named objects that are present in the UI
definition file.

Preview
^^^^^^^

The ``preview`` command displays the UI dfinition file.

This command accepts options to specify the ID of the toplevel object and a CSS
file to use.

``--id=ID``

  The ID of the object to preview. If not specified, gtk4-builder-tool will
  choose a suitable object on its own.

``--css=FILE``

  Load style information from the given CSS file.

Simplification
^^^^^^^^^^^^^^

The ``simplify`` command simplifies the UI definition file by removing
properties that are set to their default values and writes the resulting XML to
the standard output, or back to the input file.

When the ``--3to4`` option is specified, the ``simplify`` command interprets the
input as a GTK 3 UI definuition file and attempts to convert it to GTK 4
equivalents. It performs various conversions, such as renaming properties,
translating child properties to layout properties, rewriting the setup for
GtkNotebook, GtkStack, GtkAssistant  or changing toolbars into boxes.

You should always test the modified UI definition files produced by
gtk4-builder-tool before using them in production.

Note in particular that the conversion done with ``--3to4`` is meant as a
starting point for a port from GTK 3 to GTK 4. It is expected that you will have
to do manual fixups  after the initial conversion.

``--replace``

  Write the content back to the UI definition file instead of using the standard
  output.

``--3to4``

  Transform a GTK 3 UI definition file to the equivalent GTK 4 definitions.

Precompilation
^^^^^^^^^^^^^^

The ``precompile`` command creates a more compact, and faster to load compiled
form of the ui file that is understood by GtkBuilder. The output is written
to a file with the extension ``.precompiled``.
