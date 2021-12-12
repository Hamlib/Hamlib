#include "frame.h"
#include "misc.h"
extern int ic7300_set_clock(RIG *rig, int year, int month, int day, int hour,
                            int min, int sec, double msec, int utc_offset);
extern int ic7300_get_clock(RIG *rig, int *year, int *month, int *day,
                            int *hour,
                            int *min, int *sec, double *msec, int *utc_offset);

