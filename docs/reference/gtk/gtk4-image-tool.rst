.. _gtk4-image-tool(1):

====================
gtk4-image-tool
====================

-----------------------
Image Utility
-----------------------

:Version: GTK
:Manual section: 1
:Manual group: GTK commands

SYNOPSIS
--------
|   **gtk4-image-tool** <COMMAND> [OPTIONS...] <FILE>...
|
|   **gtk4-image-tool** compare [OPTIONS...] <FILE1> <FILE2>
|   **gtk4-image-tool** convert [OPTIONS...] <FILE1> <FILE2>
|   **gtk4-image-tool** info [OPTIONS...] <FILE>
|   **gtk4-image-tool** relabel [OPTIONS...] <FILE1> <FILE2>
|   **gtk4-image-tool** show [OPTIONS...] <FILE>...

DESCRIPTION
-----------

``gtk4-image-tool`` can perform various operations on images.

COMMANDS
--------

Information
^^^^^^^^^^^

The ``info`` command shows general information about the image, such
as its format and color state.

Showing
^^^^^^^

The ``show`` command displays one or more images, side-by-side.

``--undecorated``

Removes window decorations. This is meant for rendering of exactly the image
without any titlebar.

Compare
^^^^^^^

The ``compare`` command compares two images. If any differences are found,
the exit code is 1. If the images are identical, it is 0.

``--output=FILE``

  Save the differences as a png image in ``FILE``.

``--quiet``

  Don't write results to stdout.

Conversion
^^^^^^^^^^

The ``convert`` command converts the image to a different format or color state.

``--format=FORMAT``

  Convert to the given format. The supported formats can be listed
  with ``--format=list``.

``--color-state=COLORSTATE``

  Convert to the given color state. The supported color states can be
  listed with ``--format=list``.

``--cicp=CICP``

  Convert to a color state that is specified as a cicp tuple. The cicp tuple
  must be specified as four numbers, separated by /, e.g. 1/13/6/0.

Relabeling
^^^^^^^^^^

The ``relabel`` command changes the color state of an image without conversion.
This can be useful to produce wrong color renderings for diagnostics.

``--color-state=COLORSTATE``

  Relabel to the given color state. The supported color states can be
  listed with ``--format=list``.

``--cicp=CICP``

  Relabel to a color state that is specified as a cicp tuple. The cicp tuple
  must be specified as four numbers, separated by /, e.g. 1/13/6/0.
