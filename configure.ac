#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.



AC_PREREQ([2.69])
AC_INIT([TACHYON], [0.6.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([src/main.cpp])

AC_CONFIG_HEADERS([include/config.h])
AC_DEFINE([TACHYON_VERSION_MAJOR], [0], [Major version])
AC_DEFINE([TACHYON_VERSION_MINOR], [6], [Minor version])
AC_DEFINE([TACHYON_VERSION_PATCH], [1], [Patch version])
AC_DEFINE([VERSION], [PACKAGE_VERSION], [Patch version])



# Checks for programs.
AC_PROG_CXX
AC_PROG_CC


# Checks for libraries.
# FIXME: Replace `main' with a function in `-lcrypto':
m4_ifdef([PKG_PROG_PKG_CONFIG], [PKG_PROG_PKG_CONFIG], [PKG_CONFIG=""])

# FIXME: Replace `main' with a function in `-lhts':
PKG_CHECK_MODULES([ZSTD], libzstd, [
    LIBS="$ZSTD_LIBS $LIBS"
    CFLAGS="$ZSTD_CFLAGS $CFLAGS"
    AC_DEFINE([WITH_ZSTD], [1], [Use zstd])
  ], [
    AC_SEARCH_LIBS(ZSTD_decompressStream, zstd,

      [AC_MSG_ERROR(libzstd was not found)])
  ])

PKG_CHECK_MODULES([XXHASH], libxxhash, [
    LIBS="XXHASH_LIBS $LIBS"
    CFLAGS="$XXHASH_CFLAGS $CFLAGS"
  ], [
    AC_SEARCH_LIBS(XXH64_update, xxhash,[

],

      [AC_MSG_ERROR(xxhash was not found)])
  ])


PKG_CHECK_MODULES([CRYPTO], libcrypto, [
    LIBS="$CRYPTO_LIBS $LIBS"
    CFLAGS="$CRYPTO_CFLAGS $CFLAGS"
    AC_DEFINE([WITH_CRYPTO], [1], [Use crypto])
  ], [
    AC_SEARCH_LIBS(ZSTD_decompressStream, crypto,

      [AC_MSG_ERROR(cryptolib was not found)])
  ])



PKG_CHECK_MODULES([HTS], htslib, [
    LIBS="$HTS_LIBS $LIBS"
    CFLAGS="$HTS_CFLAGS $CFLAGS"
  ], [
    AC_SEARCH_LIBS(bcf_init1, hts,

      [AC_MSG_ERROR(htslib was not found)])
  ])

# FIXME: Replace `main' with a function in `-lzstd':
dnl AC_SEARCH_LIBS([zstd], [ZSTD_createCCtx],[],[
dnl 		     AC_MSG_ERROR([unable to find ZSTD_createCCtx()])
dnl ])

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stdint.h stdlib.h strings.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

dnl # Checks for library functions.
AC_CHECK_FUNCS([gettimeofday memset pow strncasecmp])
AC_SUBST(LIBS)
AC_SUBST(CFLAGS)

AC_CONFIG_FILES([Makefile])

#AC_CONFIG_FILES([makefile])
AC_OUTPUT
