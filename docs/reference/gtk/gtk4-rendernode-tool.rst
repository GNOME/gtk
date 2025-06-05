.. _gtk4-rendernode-tool(1):

====================
gtk4-rendernode-tool
====================

-----------------------
GskRenderNode Utility
-----------------------

:Version: GTK
:Manual section: 1
:Manual group: GTK commands

SYNOPSIS
--------
|   **gtk4-rendernode-tool** <COMMAND> [OPTIONS...] <FILE>
|
|   **gtk4-rendernode-tool** benchmark [OPTIONS...] <FILE>
|   **gtk4-rendernode-tool** compare [OPTIONS...] <FILE1> <FILE2>
|   **gtk4-rendernode-tool** convert [OPTIONS...] <FILE1>
|   **gtk4-rendernode-tool** extract [OPTIONS...] <FILE>
|   **gtk4-rendernode-tool** info [OPTIONS...] <FILE>
|   **gtk4-rendernode-tool** render [OPTIONS...] <FILE> [<FILE>]
|   **gtk4-rendernode-tool** show [OPTIONS...] <FILE>

DESCRIPTION
-----------

``gtk4-rendernode-tool`` can perform various operations on serialized rendernodes.

COMMANDS
--------

Information
^^^^^^^^^^^

The ``info`` command shows general information about the rendernode, such
as the number of nodes, and the depth of the tree.

Showing
^^^^^^^

The ``show`` command displays the rendernode.

``--offload``

Puts the node into a GraphicsOffload. Intended for testing offloading capabilities
of simple rendernodes.

``--undecorated``

Removes window decorations. This is meant for rendering of exactly the rendernode
without any titlebar.

Rendering
^^^^^^^^^

The ``render`` command saves a rendering of the rendernode as a png, tiff or svg
image or as pdf document. The name of the file to write can be specified as a second
FILE argument.

``--renderer=RENDERER``

  Use the given renderer. Use ``--renderer=help`` to get a information
  about possible values for the ``RENDERER``.

``--dont-move``

Keep the node position unchanged, relative to the pixel grid.

Benchmark
^^^^^^^^^

The ``benchmark`` command benchmarks rendering of a node with the existing renderers
and prints the runtimes.

``--renderer=RENDERER``

  Add the given renderer. This argument can be passed multiple times to test multiple
  renderers. By default, all major GTK renderers are run.

``--runs=RUNS``

  Number of times to render the node on each renderer. By default, this is 3 times.
  Keep in mind that the first run is often used to populate caches and might be
  significantly slower.

``--no-download``

  Do not attempt to download the result. This may cause the measurement to not include
  the execution of the commands on the GPU. It can be useful to use this flag to test
  command submission performance.

Compare
^^^^^^^

The ``compare`` command compares the rendering of a node with a reference image,
or the renderings of two nodes, or two images. If any differences are found, the
exit code is 1. If the images are identical, it is 0.

``--renderer=RENDERER``

  Use the given renderer.

``--output=FILE``

  Save the differences as a png image in ``FILE``.

``--quiet``

  Don't write results to stdout.

Convert
^^^^^^^

The ``convert`` command converts a symbolic SVG icon into a node and writes
the result to stdout.

``--recolor``

  If this option is used, the resulting node will have its symbolic colors
  replaced by the values given to the ``--fg``, ``--success``, ``--warning``
  and ``--error`` options. Otherwise, the node will have the unreplaced
  colors, which can be useful to diagnose rendering issues.

``--fg=COLOR``
``--success=COLOR``
``--warning=COLOR``
``--error=COLOR``

  Set the symbolic colors to recolor the icon with. If not specified, default
  colors are used.

``--size=SIZE``

  Sets the size to render the icon at. The size can be either a number or
  a string of the form ``WIDTHxHEIGHT``.

Extract
^^^^^^^

The ``extract`` command saves all the data urls found in a node file to a given
directory. The file names for the extracted files are derived from the mimetype
of the url.

``--dir=DIRECTORY``

  Save extracted files in ``DIRECTORY`` (defaults to the current directory).
