# GTK Accessibility {#gtk-accessibility}

## The standard accessibility interface

The #GtkAccessible interface provides the accessibility information about
an application's user interface elements. Assistive technology (AT)
applications, like Orca, convey this information to users with disabilities,
or reduced abilities, to help them use the application.

Standard GTK controls implement the #GtkAccessible interface and are thus
accessible to ATs by default. This means that if you use GTK controls such
as #GtkButton, #GtkEntry, or #GtkListView, you only need to supply
application-specific details when the defaults values are incomplete. You
can do this by setting the appropriate properties in your #GtkBuilder
template and UI definition files, or by setting the properties defined by
the #GtkAccessible interface.

If you are implementing your own #GtkWidget derived type, you will need to
set the #GtkAccessible properties yourself, and provide an implementation
of the #GtkAccessible virtual functions.

## Accessible roles and states

The fundamental concepts of an accessible widget are *roles* and *states*;
each GTK control fills out a role and its functionality is described by a
set of *states*.

### Roles

Roles define the taxonomy and semantics of a UI control to any assistive
technology application; for instance, a button will have a role of
`GTK_ACCESSIBLE_ROLE_BUTTON`; an entry will have a role of
`GTK_ACCESSIBLE_ROLE_TEXTBOX`; a toggle button will have a role of
`GTK_ACCESSIBLE_ROLE_CHECKBOX`; etc.

Each role is part of the widget's instance, and **cannot** be changed over
time or as the result of a user action. Roles allows assistive technology
applications to identify a UI control and decide how to present it to a
user; if a part of the application's UI changes role, the control needs to
be removed and replaced with another one with the appropriate role.

#### List of accessible roles

Each role name is part of the #GtkAccessibleRole enumeration.

| Role name | Description |
|-----------|-------------|
| `ALERT` | A message with important information |
| `BUTTON` | A control that performs an action when pressed |
| `CHECKBOX` | A control that has three possible value: `true`, `false`, or `undefined` |
| `COLUMNHEADER` | The header of a column in a list or grid |
| `COMBOBOX` | A control that can be expanded to show a list of possible values to select |
| `...` | … |

See the [WAI-ARIA](https://www.w3.org/WAI/PF/aria/appendices#quickref) list
of roles for additional information.

### States

States, or properties, provide specific information about an accessible UI
control, and describe it for the assistive technology applications.

Unlike roles, states may change over time, or in response to user action;
for instance, a toggle button will change its `GTK_ACCESSIBLE_STATE_CHECKED`
state every time it is toggled, either by the user or programmatically.

#### List of accessible states

Each state name is part of the #GtkAccessibleState enumeration.

| State name | Description |
|------------|-------------|
| `...` | … |

See the [WAI-ARIA](https://www.w3.org/WAI/PF/aria/appendices#quickref) list of states for additional information.

## Application development rules

Even if standard UI controls provided by GTK have accessibility information
out of the box, there are some additional properties and considerations for
application developers. For instance, if your application presents the user
with a form to fill out, you should ensure that:

 * the container of the form has a `GTK_ACCESSIBLE_ROLE_FORM` role
 * each text entry widget in the form has the `GTK_ACCESSIBLE_RELATION_LABELLED_BY`
   relation pointing to the label widget that describes it

Another example: if you create a tool bar containing buttons with only icons,
you should ensure that:

 * the container has a `GTK_ACCESSIBLE_ROLE_TOOLBAR` role
 * each button has a `GTK_ACCESSIBLE_PROPERTY_LABEL` property set with the user
   readable and localised action performed when pressed; for instance "Copy",
   "Paste", "Add layer", or "Remove"

GTK will try to fill in some information by using ancillary UI control
property, for instance the accessible label will be taken from the label or
placeholder text used by the UI control, or from its tooltip, if the
`GTK_ACCESSIBLE_PROPERTY_LABEL` property or the `GTK_ACCESSIBLE_RELATION_LABELLED_BY`
relation are unset. Nevertheless, it is good practice and project hygiene
to explicitly specify the accessible properties, just like it's good practice
to specify tooltips and style classes.

Application developers using GTK **should** ensure that their UI controls
are accessible as part of the development process. When using `GtkBuilder`
templates and UI definition files, GTK provides a validation tool that
verifies that each UI element has a valid role and properties; this tool can
be used as part of the application's test suite to avoid regressions.

## Implementations

Each UI control implements the #GtkAccessible interface to allow widget and
application developers to specify the roles, state, and relations between UI
controls. This API is purely descriptive.

Each `GtkAccessible` implementation must provide a #GtkATContext instance,
which acts as a proxy to the specific platform's accessibility API:

 * AT-SPI on Linux/BSD
 * NSAccessibility on macOS
 * Active Accessibility on Windows

Additionally, an ad hoc accessibility backend is available for the GTK
testsuite, to ensure reproducibility of issues in the CI pipeline.
