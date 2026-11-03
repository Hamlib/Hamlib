# Test build metadata

`tests/Makefile.am` contains directory-wide settings. Files in this directory
group test build metadata by subsystem and are included into the same generated
Makefile.

The existing general-purpose utility targets and legacy shell-wrapper tests
remain in `tests/Makefile.am`.

Keep each test's `check_PROGRAMS` and `TESTS` registration together with its
target-specific sources, flags, libraries, and wrapper rules. Add tests to an
existing subsystem fragment when possible.
Create another fragment only for a distinct area expected to contain multiple
tests.

Tests using generated shell wrappers belong in the same fragment as their
wrapper registration and recipe.

Keep test blocks sorted by target name. Library prerequisite rules are shared
in `src/hamlib-library.am`; `tests/Makefile.am` adds backend archive
prerequisites. Direct test builds update Hamlib through one invocation of the
core library build.
