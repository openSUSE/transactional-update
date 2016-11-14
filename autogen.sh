#!/bin/sh -x

rm -fv ltmain.sh config.sub config.guess config.h.in
aclocal -I m4
automake --add-missing --copy --force
autoreconf
chmod 755 configure
