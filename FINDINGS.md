# What vendoring minmea found

Found against sysl 0.1.0-alpha.12. Nothing below is worked around in the package.

## 1. A package's C cannot be compiled against cstd's headers (sysl gap)

```
sysl build-lib . --target thumb-freestanding
error: the C compiler refused this file's 'c const' block:
sh/sysl/minmea/c/minmea.h:16:10: fatal error: 'ctype.h' file not found
```

minmea needs `<ctype.h>`, `<stdlib.h>` (`strtol`, `abs`), `<string.h>`, `<math.h>`, `<time.h>` and
`<stdarg.h>`. The same gap littlefs found (its FINDINGS 1).

**Status: fixed in sysl 0.1.2** (`if = "!libc"` dependencies and the vendored-C `libc` block); minmea 0.1.1 takes cstd this way.

## 2. cstd lacks parts of what minmea reaches (cstd gap)

Even with finding 1 closed, `sysl-lang/cstd` offers no `<math.h>`, which `minmea.h` includes (its
inline `tofloat` and `tocoord` return `NAN`). And its `<time.h>` has `struct timespec` but no `struct tm`
and no `timegm`, which `minmea_gettime` calls.

**Status: fixed in cstd 0.1.1** (`math.h`, and the whole of `<time.h>` with `struct tm` and `timegm`).

## Upstream quirks (not sysl's)

- `minmea_sentence(id)` indexes its table from 0, although INVALID is -1 and UNKNOWN is 0, so it answers
  "INVALID" for UNKNOWN. It is not bound.
- With no `*`, `minmea_check` accepts any printable character to the end of the line, spaces included,
  so `$GPRMC,,V,,,,,,,,,,N trailing` is a sentence. A tab or other control character after the data is
  refused.
