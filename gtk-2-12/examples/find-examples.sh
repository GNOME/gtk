find . -name CVS -prune -o \
     -not \( -type d -o -name Makefile.am -o -name find-examples.sh \) -print |
    sed 's%\./\(.*\)%	examples/\1  \\%'
echo "	examples/find-examples.sh"
