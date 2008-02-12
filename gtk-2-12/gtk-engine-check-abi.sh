#! /bin/sh

# Check that a theme engine does not export any 
# functions that may interfere with symbol resolution

cat >expected-abi <<EOF
theme_create_rc_style
theme_exit
theme_init
EOF

cat >optional-abi <<EOF
__bss_start
_edata
_end
_fini
_init
g_module_check_init
g_module_unload
EOF

nm -D -g --defined-only $1 | cut -d ' ' -f 3 > actual-abi

cat optional-abi >>expected-abi
sort expected-abi | uniq >expected-abi2

cat optional-abi >>actual-abi
sort actual-abi | uniq >actual-abi2

diff -u expected-abi2 actual-abi2 && rm expected-abi optional-abi actual-abi expected-abi2 actual-abi2
