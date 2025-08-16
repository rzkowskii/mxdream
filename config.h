/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */
/* #undef ENABLE_NLS */
/* #undef HAVE_CATGETS */
/* #undef HAVE_GETTEXT */
#define GETTEXT_PACKAGE "lxdream"
#define HAVE_LC_MESSAGES 1
/* #undef HAVE_STPCPY */
/* #undef HAVE_LIBSM */

/* Building on an apple platform. Things are different... */
#define APPLE_BUILD 1

/* CPP to use for build tools */
#define BUILD_CPP_PROG "gcc -E"

/* Enable dynamic plugin support */
#define BUILD_PLUGINS 1

/* Sed to use for build tools */
#define BUILD_SED_PROG "/usr/bin/sed"

/* always defined to indicate that i18n is enabled */
/* #undef ENABLE_NLS */

/* Enable SH4 statistics */
/* #undef ENABLE_SH4STATS */

/* Enable IO tracing */
/* #undef ENABLE_TRACE_IO */

/* Enable watchpoints */
/* #undef ENABLE_WATCH */

/* Forcibly inline code */
#define FORCEINLINE __attribute__((always_inline))

/* translation domain */
#define GETTEXT_PACKAGE "lxdream"

/* Have alsa support */
/* #undef HAVE_ALSA */

/* Define to 1 if you have the 'bind_textdomain_codeset' function. */
/* #undef HAVE_BIND_TEXTDOMAIN_CODESET */

/* Define to 1 if you have the Mac OS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
#define HAVE_CFLOCALECOPYCURRENT 1

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
#define HAVE_CFPREFERENCESCOPYAPPVALUE 1

/* Have Cocoa framework */
#define HAVE_COCOA 1

/* Have Apple CoreAudio support */
#define HAVE_CORE_AUDIO 1

/* Define to 1 if you have the 'dcgettext' function. */
/* #undef HAVE_DCGETTEXT */

/* Have esound support */
/* #undef HAVE_ESOUND */

/* Have exception stack-frame information */
#define HAVE_EXCEPTIONS 1

/* Use fast register-passing calling conventions */
/* #undef HAVE_FASTCALL */

/* Define if we have a working builtin frame_address */
/* #undef HAVE_FRAME_ADDRESS */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Using GLESv2 */
/* #undef HAVE_GLES2 */

/* Have GLX support */
/* #undef HAVE_GLX */

/* Have GTK libraries */
/* #undef HAVE_GTK */

/* Building with GTK+Cocoa */
/* #undef HAVE_GTK_OSX */

/* Building with GTK+X11 */
/* #undef HAVE_GTK_X11 */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define to 1 if you have the 'm' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the 'OSMesa' library (-lOSMesa). */
/* #undef HAVE_LIBOSMESA */

/* Define to 1 if you have the 'z' library (-lz). */
#define HAVE_LIBZ 1

/* Using the linux native CDROM driver */
/* #undef HAVE_LINUX_CDROM */

/* Have linux joystick support */
/* #undef HAVE_LINUX_JOYSTICK */

/* Have LIRC support */
/* #undef HAVE_LIRC */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Have NSOpenGL support */
#define HAVE_NSGL 1

/* Have Color Clamp */
#define HAVE_OPENGL_CLAMP_COLOR 1

/* Have glClearDepthf function */
#define HAVE_OPENGL_CLEAR_DEPTHF 1

/* Have glDrawBuffer function */
#define HAVE_OPENGL_DRAW_BUFFER 1

/* Have 2.0 framebuffer_object support */
#define HAVE_OPENGL_FBO 1

/* Have EXT_framebuffer_object support */
#define HAVE_OPENGL_FBO_EXT 1

/* Have OpenGL fixed-functionality */
#define HAVE_OPENGL_FIXEDFUNC 1

/* Have 2.0 shader support */
#define HAVE_OPENGL_SHADER 1

/* Have ARB shader support */
#define HAVE_OPENGL_SHADER_ARB 1

/* Have glAreTexturesResident function */
#define HAVE_OPENGL_TEX_RESIDENT 1

/* Building with the OSMesa video driver */
/* #undef HAVE_OSMESA */

/* Have pulseaudio support */
/* #undef HAVE_PULSE */

/* Have SDL support */
/* #undef HAVE_SDL */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Generating a bundled application */
#define OSX_BUNDLE 1

/* Name of package */
#define PACKAGE "lxdream"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "lxdream"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "lxdream 0.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "lxdream"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.1"

/* SH4 Translator to use (if any) */
/* #undef SH4_TRANSLATOR */

/* The size of 'void *', as computed by sizeof. */
#define SIZEOF_VOID_P 8

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.1"
