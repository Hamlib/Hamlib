# test/ — automated tests

Written for: anyone adding to or debugging Hamlib's automated tests.

This directory holds every [acutest](https://github.com/mity/acutest)-based
suite, whatever subsystem it covers. `make check` builds and runs all of them,
and so does CI.

## test/ versus tests/

The two directories are easy to confuse, and the names do not help:

| | What it is |
|---|---|
| **`test/`** | Automated tests only. Nothing here is installed. Every file is a test suite, a mock, or shared test scaffolding. |
| **`tests/`** | The programs Hamlib ships — `rigctl`, `rigctld`, `rotctl`, `ampctl`, `rigstreamtest` and friends — plus shell scripts that drive them. Despite the name it is closer to an `apps/` or `tools/` directory, and renaming it has been discussed but is a change for its own day. |

So a name appearing in both places means two different things:

- `tests/rigstreamtest.c` **is** the streaming test tool, installed and run by
  hand against a radio.
- `test/test_rigstreamtest.c` is the suite that **tests that tool**, by running
  the built binary against the dummy backend and checking what it prints.

The same holds for the shell scripts: `tests/rigstreamtest-dummy.sh` runs under
`make check` (simulator-backed), while `tests/rigstreamtest-hw.sh` needs a real
radio and is never part of `make check`.

## Running them

```sh
make check                              # every suite
make -C test check                      # the same, skipping the rest of the tree

./test/test_smartsdr_conf               # one suite, directly
./test/test_smartsdr_conf --list        # what is in it
./test/test_smartsdr_conf vita_port_roundtrip   # one case, by name
./test/test_smartsdr_conf --verbose=3   # including each case's own messages
```

A suite run by `make check` writes its full output to `test/<suite>.log`; the
console gets one PASS/FAIL line per suite, and a failing suite's log is dumped.
Suites run at `RIG_DEBUG_WARN`; `RIG_TEST_DEBUG=1` raises that to TRACE (see
`test_debug.h`).

Failures print as `file.c:line: expression... failed` followed by the message
its `TEST_MSG` supplied, so a failure should say what was expected and what
arrived without anyone having to read the source.

## Conventions

- **One file per subsystem**, named `test_<subsystem>.c`, ending in a
  `TEST_LIST` of `{ "case_name", test_function }` pairs. Case names are
  snake_case and read as claims (`vita_port_rejects_out_of_range`), so the
  console line says what broke.
- **Test the contract, not the implementation.** Prefer driving a public entry
  point over reaching into internals — and where a test must reach in, it
  includes the `src/` header directly (the directory compiles as internal code,
  `-DIN_HAMLIB`).
- **No network, no hardware, no sleeping on wall-clock timing.** Suites must
  pass on a loaded CI runner: wait for a condition, with a generous ceiling,
  rather than for a fixed stretch of time.
- **Comments say why.** A test whose reason for existing is a bug that was once
  real should say so; that is what stops it being deleted as redundant.

## Standing in for hardware

Three approaches are in use, in increasing order of cost:

- **In-process mocks** — `smartsdr_mock.c` here is a TCP/UDP server on
  loopback, started and stopped inside the test process. Fastest, and the
  default choice.
- **Simulators** — `simulators/simflex` and friends are separate programs a
  test spawns (see `test_smartsdr_stream.c`). Use one when the thing under test
  is the process boundary itself.
- **The real radio** — never in `make check`. Hardware scripts live in `tests/`
  and are run by hand (`tests/rigstreamtest-hw.sh`).

A suite that cannot set up its environment should skip with a reason
(`TEST_SKIP("simflex not built")`) rather than fail, and must say plainly when
it cannot — a silent no-op test is worse than no test.

## Adding a suite

1. Write `test_<name>.c`, including `"acutest.h"` and `"test_debug.h"`.
2. Add it to `check_PROGRAMS` in `Makefile.am`, with its `_SOURCES` and
   `_LDADD` (and `_CFLAGS`/`_LIBS` for pthreads if it starts threads).
3. That is all: `TESTS = $(check_PROGRAMS)`, so `make check` and the CI
   workflows pick it up from there.

Anything the suite needs at run time but does not build — a header, a data
file — belongs in `EXTRA_DIST`, or `make distcheck` will fail on the
distribution tree.
