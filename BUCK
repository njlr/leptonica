include_defs('//BUCKAROO_DEPS')

def merge_dicts(x, y):
  z = x.copy()
  z.update(y)
  return z

ENDIANNESS_H = """
#if !defined (L_BIG_ENDIAN) && !defined (L_LITTLE_ENDIAN)
# if defined (__APPLE_CC__)
#  ifdef __BIG_ENDIAN__
#   define L_BIG_ENDIAN
#  else
#   define L_LITTLE_ENDIAN
#  endif
# else
#  define L_LITTLE_ENDIAN
# endif
#endif
"""

genrule(
  name = 'endianness.h',
  out = 'endianness.h',
  cmd = 'echo "' + ENDIANNESS_H + '" > $OUT',
)

macos_flags = [
  '-DOS_IOS=1',
]

linux_flags = [
  '-DHAVE_FMEMOPEN=1',
]

cxx_library(
  name = 'leptonica',
  header_namespace = '',
  exported_headers = merge_dicts(subdir_glob([
    ('src', '*.h'),
  ]), {
    'endianness.h': ':endianness.h',
  }),
  srcs = glob([
    'src/*.c',
  ]),
  compiler_flags = [
    # '-DHAVE_CONFIG_H=1',
    # '-DHAVE_FMEMOPEN=0',
    '-DHAVE_DLFCN_H=1',
    '-DHAVE_STDLIB_H=1',
    '-DHAVE_LIBJPEG=1',
    '-DHAVE_LIBPNG=1',
    '-DHAVE_LIBTIFF=1',
    '-DHAVE_LIBWEBP=1',
    '-DHAVE_LIBZ=1',
  ],
  platform_compiler_flags = [
    ('default', macos_flags),
    ('^macos.*', macos_flags),
    ('^linux.*', linux_flags),
  ],
  visibility = [
    'PUBLIC',
  ],
  deps = BUCKAROO_DEPS,
)
