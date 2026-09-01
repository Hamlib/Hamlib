#include "hamlib/rig.h"

int dumpconf_list(RIG *rig, FILE *fout);

/* Print the "Has data streaming support:" summary and the "Data
 * streaming capabilities:" block, running the streaming declaration
 * sanity checks. Returns the
 * number of warnings found; when warnbuf is non-NULL, appends one
 * " STREAM_*" tag per warning for the caller's summary line. */
int dumpcaps_stream(RIG *rig, FILE *fout, char *warnbuf,
                    size_t warnbuf_size);

/* Same block from the SERVED view (session caps / derived effective sets,
 * via rig_stream_caps_at) — what this rig can open right now. For
 * dumpstate(): runtime truth, no declaration sanity checks. */
int dumpcaps_stream_state(RIG *rig, FILE *fout);
