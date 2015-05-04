AC_INIT([Nanotalk voice client], [1.0], [xjaros32@stud.feec.vutbr.cz], [nanotalk], [https://github.com/martinjaros/nanotalk])
AM_INIT_AUTOMAKE([foreign no-define])
AC_PROG_CC
PKG_CHECK_MODULES([PACKAGES], [alsa >= 1.0.25 opus >= 0.9.14 libcrypto > 1.0.1d], [LIBS="$LIBS $PACKAGES_LIBS"])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
