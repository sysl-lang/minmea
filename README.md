# minmea for sysl

A small NMEA 0183 sentence parser for the GPS module on a UART, bound to
[minmea](https://github.com/kosma/minmea) 1.0.0, vendored unchanged.

*An independent sysl package for minmea. It is not affiliated with or endorsed by minmea's authors or trademark holders.*

- **It parses what a GPS prints.** `$GPRMC`, `$GPGGA`, `$GPGLL`, `$GPGSA`, `$GPGST`, `$GPGSV`,
  `$GPVTG`, `$GPZDA` and `$GPGBS`, from any talker (`GP`, `GN`, `GL`, ...), with or without a checksum
  and a line ending.
- **It keeps numbers exact.** `4807.038` is the pair `{4807038, 1000}`, not a float that has already
  lost a digit. Converting to a float is a call you make, and a field the module left empty is
  recognisably empty rather than a made-up zero.
- **It needs no heap.** A line is copied into a buffer on the stack, parsed into a struct on the
  stack, and returned as a value.

```hocon
dependencies {
  minmea { git = "github.com/sysl-lang/minmea", version = "<the first tag>" }
}
```

The module is `sh.sysl.minmea`.

## Using it

```sysl
import sh.sysl.minmea.{Sentence, parse}

match parse(line)
    Ok(Sentence.Gga(g)) ->
        print(g.position.latitude.to_coord(), g.position.longitude.to_coord(), g.altitude.to_f32())
    Ok(Sentence.Rmc(r)) -> print(r.speed, r.date)
    Ok(_) -> unit
    Err(e) -> print(e)
```

`parse(line)` takes one line with or without its CR LF. `Sentence` has a variant, with a struct of
fields, for each type. South and West are negative (`Position.latitude`, `Position.longitude`).
`to_coord()` answers decimal degrees (`4807.038` is 48.1173), `to_f32()` the plain number, and
`rescale(scale)` the same value as an integer over another scale (rounding the way minmea does), for
a board that would rather not use floating point. A date or time field a sentence left empty is `-1`; a `Fixed` left empty has `scale == 0`
(`is_known()`); `Date.is_known()` and `Time.is_known()` ask the same of the others.

| | |
|---|---|
| parse | `parse`, `parse_strict` (a line with no `*hh` checksum is `Error.MissingChecksum`) |
| inspect | `check(line, strict)`, `checksum(line)`, `kind(line, strict)`, `talker(line)` |
| types | `Gga`, `Rmc`, `Gll`, `Gsa`, `Gst`, `Gsv`, `Vtg`, `Zda`, `Gbs`, `Position`, `Fixed`, `Date`, `Time`, `Satellite`, `FaaMode`, `GsaMode`, `FixType` |
| time | `unix_time(date, time)` (see Compile options) |

`Error`: `TooLong` (over `MAX_LINE`, 82 bytes, which NMEA 0183 allows counting the `$` and the CR LF),
`Invalid` (not a sentence: no `$`, a checksum that does not match, a character a sentence cannot
hold, or text after the line ending), `MissingChecksum` (strict only), `Unsupported` (a sentence of
a type not listed above, such as `$GPTXT`) and `Malformed` (a listed type whose fields do not fit,
such as a direction `Q` or a ZDA offset beyond 13 hours).

**A sysl `string` is not NUL-terminated and minmea reads C strings**, so every call copies the line
into a buffer of the binding's own with the NUL after it. A line with a NUL inside it is `Invalid`
rather than silently cut short. A slice of a longer string parses as the slice.

## What it is compiled with, and what is left out

- `MINMEA_MAX_SENTENCE_LENGTH = 83`. minmea's library code never reads it; it is the size of the
  line buffer. NMEA 0183 allows 82 characters counting the `$` and CR LF, and one more byte holds the
  NUL, so 83 holds every legal sentence where upstream's default of 80 would refuse a legal 82.
- `MINMEA_INCLUDE_COMPAT` is not set: it exists for Windows and TI-RTOS.
- `timegm` is **not** redefined as `mktime`, which upstream suggests for a C library without
  `timegm` (newlib, for one). That is right only while the process runs in UTC, and would make a
  host's timestamps depend on its time zone.

Deliberately not bound: `minmea_scan`, which is variadic (the nine `minmea_parse_*` functions are what
it is for), `minmea_getdatetime`, `minmea_sentence` (see Upstream quirks) and `minmea_isfield`. The
three `static inline` conversions and `minmea_gettime` (which takes a `struct timespec` whose layout
is the C library's) go through `shim.c`, five lines each, which call minmea's own code and do nothing
else.

## Bare metal

The package needs no operating system and no allocator. On a target with no C library of its own
(`thumbv7em-freestanding`, `riscv32-freestanding` and the rest) it takes
[cstd](https://github.com/sysl-lang/cstd) 0.1.7 as minmea's: the vendored C is compiled against cstd's
headers, and the archive carries cstd's `timegm`, `strtol`, `abs`, `memcpy`, `memset`, `memcmp` and
`<ctype.h>` tests. Where the target has a C library (the host, the Pico SDK's newlib, Zephyr) the
dependency is dropped and that library answers. This needs sysl 0.6.0 or later.

`enum minmea_faa_mode` is four bytes on the host and one on ARM bare metal, which sizes an enumeration
to its values; the binding declares the field with the width the C compiler measures for the target.

## Tests

`sysl test .` runs 46 tests. Beyond every sentence type with real lines, including minmea's own test
vectors with the values its tests assert (`tests.c` in the vendored release):

- Empty fields, a missing checksum, a bad checksum (also in lower case), south and west
  coordinates, a line of exactly 82 bytes and one of 83, a NUL inside a line, an unknown talker, a
  sentence of a type the package does not return, and strict mode.
- **Against minmea driven from C.** A C program written straight against `minmea.h`, built from the
  same vendored source with the same options (it is a string in the test, never a `.c` file in the
  tree), is given 78 lines as arguments: all of minmea's valid and invalid vectors, one of every
  sentence type, every FAA letter, both GSA modes and the fix types, a CR LF, a bad checksum, a
  truncated line and nothing at all. For each it prints whether it is a sentence (loose and strict),
  its kind, its checksum and every field of its parse, with each fixed-point value shown as value,
  scale, the bits of `tofloat` and of `tocoord`, and `rescale` to 10 and to 1000. The binding prints
  the same text from its own types, and the two outputs must be the same bytes.
- minmea's float, coordinate, rescale and `gettime` vectors (`1392382809` for 2014-02-14 13:00:09,
  the two-digit year pivot at 80, 2037, 2079, 2080).
- The layouts sysl declares for minmea's structs are checked against the header's `sizeof` and
  `offsetof` when the package compiles, for whichever target that is: every field of every struct.
- On a board: `build-c` for `thumbv7em-freestanding` and `riscv32-freestanding`, with `nm` showing the
  archive defines minmea's own entry points and cstd's definitions of every C library function minmea
  calls; for the host, that it defines none of them. With the cstd dependency taken out, the thumb
  build fails on `'ctype.h' file not found`.

Mutations, each run and each red: a latitude negated (6 red); every fixed-point scale times ten (21);
GGA and GLL swapped in the kind mapping (2); GSA and GST swapped in the dispatch (5); the public
`check` answering true (3); the explicit first checksum check deleted from `parse_with` (1, the
strict-mode test: the non-strict path survives it by design because `minmea_sentence_id` checks the
line again); the line handed to C as the string's own pointer, with no NUL of the binding's (5,
including the slice-of-a-longer-string test); FAA `D` and `M` swapped (2); the length bound off by one
(1); two GSV slots swapped (3); the GLL validity inverted (4); a date's day and month swapped (4); two
fields of `c.Rmc` swapped (refused at compile time by the `offsetof` assertions). One mutation is
equivalent and is not run: dropping `into[line.len] = 0` changes nothing because the buffer starts
zeroed. The oracle was red in every mutation that changes a field.

**AddressSanitizer**: `SYSL_EXTRA_CFLAGS="-fsanitize=address -g" sysl test .` passes 43/43, with
`__asan_report` hooks (9) linked into the test binary.

## Vendored

`sh/sysl/minmea/c/` holds `minmea.c` and `minmea.h` from
[kosma/minmea](https://github.com/kosma/minmea) release v1.0.0, unchanged, from the tarball whose
SHA-256 is `db1295d849d296ab22652a03b779215125e671eccbf2384f5d21af0041fd66f9`, plus `shim.c`, which
is this package's. Vendoring means upstream fixes do not arrive on their own: watch minmea's
releases and re-vendor.

## Upstream quirks

- `minmea_sentence(id)` indexes its table from 0 although `MINMEA_INVALID` is -1 and `MINMEA_UNKNOWN`
  is 0, so it answers `"INVALID"` for `MINMEA_UNKNOWN`. It is not bound.
- Without a `*`, `minmea_check` accepts every printable character to the end of the line, spaces
  included, so `$GPRMC,,V,,,,,,,,,,N trailing` is a sentence. A control character after the data is
  refused.

Licensing: minmea is WTFPL, with its author's grant of the MIT License or the LGPL-3.0 or later at
your option; the rest of this package is ISC. The WTFPL and MIT texts are in `LICENSE`.
