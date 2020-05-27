# How to contribute to GTK's documentation

The GTK documentation is divided in two major components:

 - the API reference, which is generated from special comments in the GTK
   source code
 - static pages that provide an overview of specific sections of the API

In both cases, the contents are parsed, converted into DocBook format, and
cross-linked in order to match types, functions, signals, and properties.
From the DocBook output, we generate HTML, which can be used to read the
documentation both offline and online.

In both cases, contributing to the GTK documentation requires modifying
files tracked in the source control repository, and follows the same steps
as any other code contribution as outlined in the GTK [contribution
guide][contributing]. Please, refer to that document for any further
question on the mechanics of contributing to GTK.

GTK uses [gtk-doc][gtkdoc] to generate its documentation. Please, visit the
gtk-doc website to read the project's documentation.

[contributing]: ../../CONTRIBUTING.md
[gtkdoc]: https://wiki.gnome.org/DocumentationProject/GtkDoc

## Contributing to the API reference

Whenever you need to add or modify the documentation of a type or a
function, you will need to edit a `gtk-doc` comment stanza, typically right
above the type or function declaration. For instance:

```c
/**
 * gtk_foo_set_bar:
 * @self: a #GtkFoo
 * @bar: a #GtkBar
 *
 * Sets the given #GtkBar instance on a #GtkFoo widget.
 */
void
gtk_foo_set_bar (GtkFoo *self,
		 GtkBar *bar)
{
  ...
```

Or, for types:

```c
/**
 * GtkFoo:
 *
 * A foo widget instance.
 *
 * The contents of this structure are private and should never
 * be accessed directly.
 */
struct _GtkFoo
{
  GtkWidget parent_instance;
};
```

Each public function and type in the GTK API reference must be listed in the
`sections.txt` file for the specific namespace to which it belongs: GDK,
GSK, or GTK. For instance, if you add a function named `gtk_foo_set_bar()`,
you will need to:

 1. open `docs/reference/gtk/gtk4-sections.txt`
 1. find the section that lists the symbols of the `GtkFoo` type
 1. add `gtk_foo_set_bar` to the list

New classes require:

 1. a new section in the `sections.txt` file
 1. the `get_type` function added to the `.types` file
 1. an `xinclude` element in the `docs.xml` file

The GTK documentation also contains a number of 'freestanding' chapters
for which the source is in .md files in docs/reference/gtk.

## Style guide

Like the [coding style][coding], these rules try to formalize the existing
documentation style; in general, you should only ever modify existing code
that does not match the rules if you're already changing that code for
unrelated reasons.

[coding]: ../CODING-STYLE.md

### Syntax

The input syntax for GTK documentation is markdown, in a flavor that is
similar to what you see on gitlab or github. The markdown support for
fragments that are extracted from sources is more limited than for
freestanding chapters.

As an exception, man pages for tools are currently maintained in docbook,
since the conversion from markdown to docbook is losing too much of the
expected formatting.

In addition to typical markdown formatting such as \*emphasis\*, gtk-doc
supports a few abbreviations for cross-references and formatting:

`#ClassName`
 : Creates a link to the docs for a class

`function()`
 : Creates a link to the docs for a function

`%constant`
 : Generates suitable markup for enum values or constants

### Sections

 - The "section" of each type must contain a name, to be referenced in the
   `sections.txt` file; a title; and a short description. For instance:

```c
/**
 * SECTION:gtkshortcut
 * @Title: GtkShortcut
 * @Short_desc: A key shortcut
 *
 * ...
```

   For classes, the title should be the name of the class. While it's
   possible to add section titles directly to the `sections.txt` file, this
   is considered deprecated, and should not be done for newly written code.
 - For classes, the long description should contain an overview of the type;
   what it does; typical use cases; and idiomatic examples of its use.
 - For widget classes, the long description of a section should also contain:
   - special XML elements and attributes parsed by the class, in case of a
     custom GtkBuildable implementation
   - the CSS element name to be used by selectors
   - the CSS selector hierarchy for children, in case of a composite widget

### Functions

 - The argument names must match in the declaration, definition, and
   documentation stanza.
 - The description should refer to the function as the subject, e.g.:

```
Adds a shortcut to the shortcuts controller.
```

   Or:

```
Checks whether the widget is set to be visible or not.
```

### Methods

 - Methods are special functions whose first argument is always the instance
   of a certain class. The instance argument for newly written code should be
   called `self`.
 - If a method is a setter or a getter for an object property, link the
   property in the methods's description.
 - If a method changes one or more properties as side effect, link those
   properties in the method's description
 - If a method is a signal emitter, link the signal in the method's
   description.

### Signals

 - While GObject can introspect argument and return types for signals,
   you should *always* document them with an explicit gtk-doc stanza.
 - The syntax for signal stanzas is similar to functions:

      /**
       * GtkFoo::signal-name:
       * @arg1: ...
       * @arg2: ...
       *
       * ...

### Properties

 - While GObject properties contain text that can be extracted
   programmatically in order to build their documentation, you should
   *always* document them with an explicit gtk-doc stanza. The text
   associated to the property is short and meant to be used when
   programmatically building user interfaces, and not for documentation
   purposes.
 - Always note if setting a property has side effects, like causing another
   property to change state.
 - The syntax for property documentation is:

      /**
       * GtkFoo:property-name:
       *
       * ...

### Actions

 - Actions are new in GTK 4, and gtk-doc had to learn a a new syntax
   to document them:

     /**
      * GtkFoo|action-name:
      * @arg1: ...
      * @arg2: ...
      *
      * ...

