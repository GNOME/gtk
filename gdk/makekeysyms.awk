$1 == "#define" && NF >= 3 {
    sub(/^XK/,"GDK",$2)
    sub(/0X/,"0x",$3)
    print $1,$2,$3
}
