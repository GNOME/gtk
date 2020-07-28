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

## Accessible roles and attributes

The fundamental concepts of an accessible widget are *roles* and
*attributes*; each GTK control has a role, while its functionality is
described by a set of *attributes*.

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

| Role name | Description | Related GTK widget |
|-----------|-------------|--------------------|
| `ALERT` | A message with important information | - |
| `BUTTON` | A control that performs an action when pressed | #GtkButton |
| `CHECKBOX` | A control that has three possible value: `true`, `false`, or `undefined` | #GtkCheckButton |
| `COLUMNHEADER` | The header of a column in a list or grid | - |
| `COMBOBOX` | A control that can be expanded to show a list of possible values to select | #GtkComboBox |
| `DIALOG` | A dialog that prompts the user to enter information or require a response | #GtkDialog and subclasses |
| `IMG` | An image | #GtkImage, #GtkPicture |
| `LABEL` | A visible name or caption for a user interface component. | #GtkLabel |
| `PROGRESS_BAR` | An element that display progress | #GtkProgressBar |
| `RADIO` | A checkable input in a group of radio roles | #GtkRadioButton |
| `SCROLLBAR` | A graphical object controlling the scolling of content | #GtkScrollbar |
| `SEARCH_BOX` | A text box for entering search criteria | #GtkSearchEntry |
| `SEPARATOR` | A divider that separates sections of content or groups of items | #GtkSeparator |
| `SPIN_BUTTON` | A range control that allows seelcting among discrete choices | #GtkSpinButton |
| `SWITCH` | A control that represents on/off values | #GtkSwitch |
| `TEXT_BOX` | A type of input that allows free-form text as its value. | #GtkEntry |
| `WINDOW` | An application window | #GtkWindow |
| `...` | … |

See the [WAI-ARIA](https://www.w3.org/WAI/PF/aria/appendices#quickref) list
of roles for additional information.

### Attributes

Attributes provide specific information about an accessible UI
control, and describe it for the assistive technology applications. GTK
divides the accessible attributes into three categories:

 - *properties*, described by the values of the #GtkAccessibleProperty
   enumeration
 - *relations*, described by the values of the #GtkAccessibleRelation
   enumeration
 - *states*, described by the values of the #GtkAccessibleState enumeration

Each attribute accepts a value of a specific type.

Unlike roles, attributes may change over time, or in response to user action;
for instance:

 - a toggle button will change its %GTK_ACCESSIBLE_STATE_CHECKED state every
   time it is toggled, either by the user or programmatically
 - setting the mnemonic widget on a #GtkLabel will update the
   %GTK_ACCESSIBLE_RELATION_LABELLED_BY relation on the widget with a
   reference to the label
 - changing the #GtkAdjustment instance on a #GtkScrollbar will change the
   %GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, %GTK_ACCESSIBLE_PROPERTY_VALUE_MIN,
   and %GTK_ACCESSIBLE_PROPERTY_VALUE_NOW properties with the upper, lower,
   and value properties of the #GtkAdjustment

See the [WAI-ARIA](https://www.w3.org/WAI/PF/aria/appendices#quickref) list
of attributes for additional information.

#### List of accessible properties

Each state name is part of the #GtkAccessibleProperty enumeration.

| State name | ARIA attribute | Value type | Notes |
|------------|----------------|------------|-------|
| %GTK_ACCESSIBLE_STATE_BUSY | “aria-busy” | boolean |
| %GTK_ACCESSIBLE_STATE_CHECKED | “aria-checked” | #GtkAccessibleTristate | Indicates the current state of a #GtkCheckButton |
| %GTK_ACCESSIBLE_STATE_DISABLED | “aria-disabled” | boolean | Corresponds to the #GtkWidget:sensitive property on #GtkWidget |
| %GTK_ACCESSIBLE_STATE_EXPANDED | “aria-expanded” | boolean or undefined | Corresponds to the  #GtkExpander:expanded property on #GtkExpander |
| %GTK_ACCESSIBLE_STATE_HIDDEN | “aria-hidden” | boolean | Corresponds to the #GtkWidget:visible property on #GtkWidget |
| %GTK_ACCESSIBLE_STATE_INVALID | “aria-invalid” | #GtkAccessibleInvalidState | Set when a widget is showing an error |
| %GTK_ACCESSIBLE_STATE_PRESSED | “aria-pressed” | #GtkAccessibleTristate | Indicates the current state of a #GtkToggleButton |
| %GTK_ACCESSIBLE_STATE_SELECTED | “aria-selected” | boolean or undefined | Set when a widget is selected |

#### List of accessible relations

Each state name is part of the #GtkAccessibleRelation enumeration.

| State name | ARIA attribute | Value type |
|------------|----------------|------------|
| %GTK_ACCESSIBLE_PROPERTY_AUTOCOMPLETE | “aria-autocomplete” | #GtkAccessibleAutocomplete |
| %GTK_ACCESSIBLE_PROPERTY_DESCRIPTION | “aria-description” | translatable string |
| %GTK_ACCESSIBLE_PROPERTY_HAS_POPUP | “aria-haspopup” | boolean |
| %GTK_ACCESSIBLE_PROPERTY_KEY_SHORTCUTS | “aria-keyshortcuts” | string |
| %GTK_ACCESSIBLE_PROPERTY_LABEL | “aria-label” | translatable string |
| %GTK_ACCESSIBLE_PROPERTY_LEVEL | “aria-level” | integer |
| %GTK_ACCESSIBLE_PROPERTY_MODAL | “aria-modal” | boolean |
| %GTK_ACCESSIBLE_PROPERTY_MULTI_LINE | “aria-multiline” | boolean |
| %GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE | “aria-multiselectable” | boolean |
| %GTK_ACCESSIBLE_PROPERTY_ORIENTATION | “aria-orientation” | #GtkOrientation |
| %GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER | “aria-placeholder” | translatable string |
| %GTK_ACCESSIBLE_PROPERTY_READ_ONLY | “aria-readonly” | boolean |
| %GTK_ACCESSIBLE_PROPERTY_REQUIRED | “aria-required” | boolean |
| %GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION | “aria-roledescription” | translatable string |
| %GTK_ACCESSIBLE_PROPERTY_SORT | “aria-sort” | #GtkAccessibleSort |
| %GTK_ACCESSIBLE_PROPERTY_VALUE_MAX | “aria-valuemax” | double |
| %GTK_ACCESSIBLE_PROPERTY_VALUE_MIN | “aria-valuemin” | double |
| %GTK_ACCESSIBLE_PROPERTY_VALUE_NOW | “aria-valuenow” | double |
| %GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT | “aria-valuetext” | translatable string |

#### List of accessible states

Each state name is part of the #GtkAccessibleState enumeration.

| State name | ARIA attribute | Value type |
|------------|----------------|------------|
| %GTK_ACCESSIBLE_RELATION_ACTIVE_DESCENDANT | “aria-activedescendant” | #GtkAccessible |
| %GTK_ACCESSIBLE_RELATION_COL_COUNT | “aria-colcount” | integer |
| %GTK_ACCESSIBLE_RELATION_COL_INDEX | “aria-colindex” | integer |
| %GTK_ACCESSIBLE_RELATION_COL_INDEX_TEXT | “aria-colindextext” | translatable string |
| %GTK_ACCESSIBLE_RELATION_COL_SPAN | “aria-colspan” | integer |
| %GTK_ACCESSIBLE_RELATION_CONTROLS | “aria-controls” | a #GList of #GtkAccessible |
| %GTK_ACCESSIBLE_RELATION_DESCRIBED_BY | “aria-describedby” | a #GList of #GtkAccessible |
| %GTK_ACCESSIBLE_RELATION_DETAILS | “aria-details” | a #GList of #GtkAccessible |
| %GTK_ACCESSIBLE_RELATION_ERROR_MESSAGE | “aria-errormessage” | #GtkAccessible |
| %GTK_ACCESSIBLE_RELATION_FLOW_TO | “aria-flowto” | a #GList of #GtkAccessible |
| %GTK_ACCESSIBLE_RELATION_LABELLED_BY | “aria-labelledby” | a #GList of #GtkAccessible |
| %GTK_ACCESSIBLE_RELATION_OWNS | “aria-owns” | a #GList of #GtkAccessible |
| %GTK_ACCESSIBLE_RELATION_POS_IN_SET | “aria-posinset” | integer |
| %GTK_ACCESSIBLE_RELATION_ROW_COUNT | “aria-rowcount” | integer |
| %GTK_ACCESSIBLE_RELATION_ROW_INDEX | “aria-rowindex” | integer |
| %GTK_ACCESSIBLE_RELATION_ROW_INDEX_TEXT | “aria-rowindextext” | translatable string |
| %GTK_ACCESSIBLE_RELATION_ROW_SPAN | “aria-rowspan” | integer |
| %GTK_ACCESSIBLE_RELATION_SET_SIZE | “aria-setsize” | integer |

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
