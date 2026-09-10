# Test build metadata

`tests/Makefile.am` contains directory-wide settings. Files in this directory
group test build metadata by subsystem and are included into the same generated
Makefile.

Keep each test's `check_PROGRAMS` and `TESTS` registration together with its
target-specific sources, flags, libraries, wrapper rules, and backend
prerequisites. Add tests to an existing subsystem fragment when possible.
Create another fragment only for a distinct area expected to contain multiple
tests.

Tests using generated shell wrappers belong in the same fragment as their
wrapper registration and recipe.
