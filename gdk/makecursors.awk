$1 == "#define" && NF >= 3 {
    sub(/^XC/,"GDK",$2)
    printf("%s = %s,\n",toupper($2),$3)
}
