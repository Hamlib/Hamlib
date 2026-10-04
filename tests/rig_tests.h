#ifndef RIG_TESTS_H
#define RIG_TESTS_H

#include <stddef.h>

#include "hamlib/rig.h"

int rig_test_cw(RIG *rig);
int rigctl_format_startup_args(char *buffer, size_t buffer_size,
                               const char *prefix, int argc,
                               char *const argv[]);
void rigctl_wipe_password(char *password);

#endif
