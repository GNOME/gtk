#!/bin/bash

cat << EOF
<?xml version="1.0" encoding="UTF-8"?>
<interface>
  <!-- interface-requires gtk+ 3.0 -->
  <object class="GtkWindow" id="window1">
    <property name="can_focus">False</property>
    <property name="type">popup</property>
    <child>
      <object class="GtkFixed" id="fixed1">
        <property name="visible">True</property>
        <property name="can_focus">False</property>
EOF

y=0
for vrepeat in stretch repeat round space; do

  x=0
  for hrepeat in stretch repeat round space; do

    for side in 0 1; do
      case $hrepeat in
      "stretch")
cat << EOF
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-hstretch$side">
            <property name="name">yellow</property>
            <property name="width_request">13</property>
            <property name="height_request">5</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + 5`</property>
            <property name="y">`expr $y \* 25 + $side \* 18`</property>
          </packing>
        </child>
EOF
        ;;
      "repeat")
cat << EOF
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-hrepeat$side">
            <property name="name">yellow-3</property>
            <property name="width_request">15</property>
            <property name="height_request">5</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + 4`</property>
            <property name="y">`expr $y \* 25 + $side \* 18`</property>
          </packing>
        </child>
EOF
        ;;
      "round")
cat << EOF
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-hround$side">
            <property name="name">yellow-3</property>
            <property name="width_request">13</property>
            <property name="height_request">5</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + 5`</property>
            <property name="y">`expr $y \* 25 + $side \* 18`</property>
          </packing>
        </child>
EOF
        ;;
      "space")
cat << EOF
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-hspace0$side">
            <property name="name">yellow</property>
            <property name="width_request">5</property>
            <property name="height_request">5</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + 6`</property>
            <property name="y">`expr $y \* 25 + $side \* 18`</property>
          </packing>
        </child>
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-hspace1$side">
            <property name="name">yellow</property>
            <property name="width_request">5</property>
            <property name="height_request">5</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + 12`</property>
            <property name="y">`expr $y \* 25 + $side \* 18`</property>
          </packing>
        </child>
EOF
      esac

      case $vrepeat in
      "stretch")
cat << EOF
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-vstretch$side">
            <property name="name">green</property>
            <property name="width_request">5</property>
            <property name="height_request">13</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + $side \* 18`</property>
            <property name="y">`expr $y \* 25 + 5`</property>
          </packing>
        </child>
EOF
        ;;
      "repeat")
cat << EOF
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-vrepeat$side">
            <property name="name">green-3</property>
            <property name="width_request">5</property>
            <property name="height_request">15</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + $side \* 18`</property>
            <property name="y">`expr $y \* 25 + 4`</property>
          </packing>
        </child>
EOF
        ;;
      "round")
cat << EOF
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-vround$side">
            <property name="name">green-3</property>
            <property name="width_request">5</property>
            <property name="height_request">13</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + $side \* 18`</property>
            <property name="y">`expr $y \* 25 + 5`</property>
          </packing>
        </child>
EOF
        ;;
      "space")
cat << EOF
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-vspace0$side">
            <property name="name">green</property>
            <property name="width_request">5</property>
            <property name="height_request">5</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + $side \* 18`</property>
            <property name="y">`expr $y \* 25 + 6`</property>
          </packing>
        </child>
        <child>
          <object class="GtkToolbar" id="toolbar-$hrepeat-$vrepeat-vspace1$side">
            <property name="name">green</property>
            <property name="width_request">5</property>
            <property name="height_request">5</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="show_arrow">False</property>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + $side \* 18`</property>
            <property name="y">`expr $y \* 25 + 12`</property>
          </packing>
        </child>
EOF
      esac
    done

    for ycorner in 0 1; do
      for xcorner in 0 1; do
cat << EOF
        <child>
          <object class="GtkEventBox" id="eventbox-corner-$hrepeat-$vrepeat-$xcorner$ycorner">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <child>
              <object class="GtkToolbar" id="toolbar-corner-$hrepeat-$vrepeat-$xcorner$ycorner">
                <property name="name">red</property>
                <property name="width_request">5</property>
                <property name="height_request">5</property>
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <property name="show_arrow">False</property>
              </object>
            </child>
          </object>
          <packing>
            <property name="x">`expr $x \* 25 + $xcorner \* 18`</property>
            <property name="y">`expr $y \* 25 + $ycorner \* 18`</property>
          </packing>
        </child>
EOF
      done
    done

    x=`expr $x + 1`
  done

  y=`expr $y + 1`
done

cat << EOF
      </object>
    </child>
  </object>
</interface>
EOF
