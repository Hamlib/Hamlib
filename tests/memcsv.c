/*
 * memcsv.c - (C) Stephane Fillod 2003-2005
 *
 * This program exercises the backup and restore of a radio
 * using Hamlib. CSV primitives
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <getopt.h>

#include <hamlib/rig.h>
#include "misc.h"
#include "sprintflst.h"


/*
 * external prototype
 */

extern int all;

char csv_sep = ','; /* CSV separator */

/*
 * Prototypes
 */
static int dump_csv_chan(RIG *rig,
                         vfo_t vfo,
                         channel_t **chan,
                         int channel_num,
                         const chan_t *chan_list,
                         rig_ptr_t arg);

static void dump_csv_name(const channel_cap_t *mem_caps, FILE *f);

static int set_channel_data(RIG *rig,
                            channel_t *chan,
                            char **line_key,
                            char **line_data);

static char *mystrtok(char *s, char delim);

static int  tokenize_line(char *line,
                          char **token_list,
                          size_t siz,
                          char delim);

static int find_on_list(char **list, char *what);

int csv_save(RIG *rig, const char *outfilename);
int csv_load(RIG *rig, const char *infilename);

int csv_parm_save(RIG *rig, const char *outfilename);
int csv_parm_load(RIG *rig, const char *infilename);


int csv_save(RIG *rig, const char *outfilename)
{
    int status;
    FILE *f;

    f = fopen(outfilename, "w");

    if (!f)
    {
        return -1;
    }

    if (rig->caps->clone_combo_get)
    {
        printf("About to save data, enter cloning mode: %s\n",
               rig->caps->clone_combo_get);
    }

    status = rig_get_chan_all_cb(rig, RIG_VFO_NONE, dump_csv_chan, f);

    fclose(f);

    return status;
}


/**  csv_load assumes the first line in a csv file is a key line,
     defining entries and their number. First line should not
     contain 'empty column', i.e. two adjacent commas.
     Each next line should contain the same number of entries.
     However, empty columns (two adjacent commas) are allowed.
     \param rig - a pointer to the rig
     \param infilename - a string with a file name to write to
*/
int csv_load(RIG *rig, const char *infilename)
{
    int status = RIG_OK;
    FILE *f;
    char *key_list[ 64 ];
    char *value_list[ 64 ];
    char keys[ 256 ];
    char line[ 256 ];
    channel_t chan;

    f = fopen(infilename, "r");

    if (!f)
    {
        return -1;
    }

    /* First read the first line, containing the key */
    if (fgets(keys, sizeof(keys), f) != NULL)
    {

        /* fgets stores '\n' in a buffer, get rid of it */
        keys[ strlen(keys) - 1 ] = '\0';
        printf("Read the key: %s\n", keys);

        /* Tokenize the key list */
        if (!tokenize_line(keys,
                           key_list,
                           sizeof(key_list) / sizeof(char *),
                           ','))
        {
            fprintf(stderr,
                    "Invalid (possibly too long or empty) key line, cannot continue.\n");
            fclose(f);
            return -1;
        }
    }
    else
    {
        /* File exists, but is empty */
        fclose(f);
        return -1;
    }

    /* Next, read the file line by line */
    while (fgets(line, sizeof line, f) != NULL)
    {
        /* Tokenize the line */
        if (!tokenize_line(line,
                           value_list,
                           sizeof(value_list) / sizeof(char *),
                           ','))
        {

            fprintf(stderr, "Invalid (possibly too long or empty) line ignored\n");
            continue;
        }

        /* Parse a line, write channel data into chan */
        set_channel_data(rig, &chan, key_list, value_list);

        /* Write a rig memory */
        status = rig_set_channel(rig, RIG_VFO_NONE, &chan);

        if (status != RIG_OK)
        {
            fprintf(stderr, "rig_get_channel: error = %s \n", rigerror(status));
            fclose(f);
            return status;
        }

    }

    fclose(f);
    return status;
}


/**  Function to break a line into a list of tokens. Delimiters are
    replaced by end-of-string characters ('\0'), and a list of pointers
    to thus created substrings is created.
    \param line (input) - a line to be tokenized, the line will be modified!
    \param token_list (output) - a resulting table containing pointers to
         tokens, or NULLs (the table will be initially nulled )
         all the pointers should point to addresses within the line
    \param siz (input) - size of the table
    \param delim (input) - delimiter character
    \return number of tokens on success, 0 if \param token_list is too small to contain all the tokens,
            or if line was empty.
*/
static int tokenize_line(char *line, char **token_list, size_t siz, char delim)
{
    size_t i;
    char *tok;

    /* Erase the table */
    for (i = 0; i < siz; i++)
    {
        token_list[i] = NULL;
    }

    /* Empty line passed? */
    if (line == NULL)
    {
        return 0;
    }

    /* Reinitialize, find the first token */
    i = 0;
    tok = mystrtok(line, delim);

    /* Line contains no delim */
    if (tok == NULL)
    {
        return 0;
    }

    token_list[ i++ ] = tok;

    /* Find the remaining tokens */
    while (i < siz)
    {
        tok = mystrtok(NULL, delim);

        /* If NULL, no more tokens left */
        if (tok == NULL)
        {
            break;
        }

        /* Add token to the list */
        token_list[ i++ ] = tok;
    }

    /* Any tokens left? */
    if (i == siz)
    {
        return 0;
    }
    else
    {
        return i;
    }
}


/** Tokenizer that handles two delimiters by returning an empty string.
    First param (input) is a string to be tokenized, second is a delimiter (input)
    This tokenizer accepts only one delimiter!
    \param s - string to divide on first run, NULL on each next
    \param delim - delimiter
    \return pointer to token, or NULL if there are no more tokens
    \sa "man strtok"
    */
static char *mystrtok(char *s, char delim)
{
    static size_t pos = 0, length = 0;
    static char *str = 0;
    size_t i, ent_pos;

    if (s != NULL)
    {
        str = s;
        pos = 0;
        length = strlen(str);
    }
    else
    {
    }

    if (str && str[ pos + 1 ] == '\0')
    {
        return NULL;
    }

    ent_pos = pos;

    for (i = pos; i < length;)
    {
        if (str[i] == delim)
        {
            str[i] = '\0';
            pos = i + 1;
            return str + ent_pos;
        }
        else
        {
            i++;
        }
    }

    return str + ent_pos;
}


static int print_parm_name(RIG *rig,
                           const struct confparams *cfp,
                           rig_ptr_t ptr)
{
    fprintf((FILE *)ptr, "%s%c", cfp->name, csv_sep);

    return 1;   /* process them all */
}


static int print_parm_val(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr)
{
    value_t val;
    FILE *f = (FILE *)ptr;
    rig_get_ext_parm(rig, cfp->token, &val);

    switch (cfp->type)
    {
    case RIG_CONF_CHECKBUTTON:
    case RIG_CONF_COMBO:
        fprintf(f, "%d%c", val.i, csv_sep);
        break;

    case RIG_CONF_NUMERIC:
        fprintf(f, "%f%c", val.f, csv_sep);
        break;

    case RIG_CONF_STRING:
        fprintf(f, "%s%c", val.s, csv_sep);
        break;

    default:
        fprintf(f, "unknown%c", csv_sep);
    }

    return 1;   /* process them all */
}


int csv_parm_save(RIG *rig, const char *outfilename)
{
    int i, ret;
    FILE *f;
    setting_t get_parm = all ? 0x7fffffff : rig->state.has_get_parm;

    f = fopen(outfilename, "w");

    if (!f)
    {
        return -1;
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms = rig_strparm(get_parm & rig_idx2setting(i));

        if (!ms || !ms[0])
        {
            continue;
        }

        fprintf(f, "%s%c", ms, csv_sep);
    }

    rig_ext_parm_foreach(rig, print_parm_name, f);
    fprintf(f, "\n");

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        const char *ms;
        value_t val;
        setting_t parm;

        parm = get_parm & rig_idx2setting(i);
        ms = rig_strparm(parm);

        if (!ms || !ms[0])
        {
            continue;
        }

        ret = rig_get_parm(rig, parm, &val);

        if (ret != RIG_OK)
        {
            return ret;
        }

        if (RIG_PARM_IS_FLOAT(parm))
        {
            fprintf(f, "%f%c", val.f, csv_sep);
        }
        else
        {
            fprintf(f, "%d%c", val.i, csv_sep);
        }
    }


    rig_ext_parm_foreach(rig, print_parm_val, f);
    fprintf(f, "\n");
    fclose(f);

    return 0;
}


int csv_parm_load(RIG *rig, const char *infilename)
{
    return -RIG_ENIMPL;
    /* for every parm/ext_parm, fscanf, parse, set_parm's */
}


/* *********************** */

/* Caution! Keep the function consistent with dump_csv_chan and set_channel_data! */
void dump_csv_name(const channel_cap_t *mem_caps, FILE *f)
{
    fprintf(f, "num%c", csv_sep);

    if (mem_caps->bank_num)
    {
        fprintf(f, "bank_num%c", csv_sep);
    }

    if (mem_caps->channel_desc)
    {
        fprintf(f, "channel_desc%c", csv_sep);
    }

    if (mem_caps->vfo)
    {
        fprintf(f, "vfo%c", csv_sep);
    }

    if (mem_caps->ant)
    {
        fprintf(f, "ant%c", csv_sep);
    }

    if (mem_caps->freq)
    {
        fprintf(f, "freq%c", csv_sep);
    }

    if (mem_caps->mode)
    {
        fprintf(f, "mode%c", csv_sep);
    }

    if (mem_caps->width)
    {
        fprintf(f, "width%c", csv_sep);
    }

    if (mem_caps->tx_freq)
    {
        fprintf(f, "tx_freq%c", csv_sep);
    }

    if (mem_caps->tx_mode)
    {
        fprintf(f, "tx_mode%c", csv_sep);
    }

    if (mem_caps->tx_width)
    {
        fprintf(f, "tx_width%c", csv_sep);
    }

    if (mem_caps->split)
    {
        fprintf(f, "split%c", csv_sep);
    }

    if (mem_caps->tx_vfo)
    {
        fprintf(f, "tx_vfo%c", csv_sep);
    }

    if (mem_caps->rptr_shift)
    {
        fprintf(f, "rptr_shift%c", csv_sep);
    }

    if (mem_caps->rptr_offs)
    {
        fprintf(f, "rptr_offs%c", csv_sep);
    }

    if (mem_caps->tuning_step)
    {
        fprintf(f, "tuning_step%c", csv_sep);
    }

    if (mem_caps->rit)
    {
        fprintf(f, "rit%c", csv_sep);
    }

    if (mem_caps->xit)
    {
        fprintf(f, "xit%c", csv_sep);
    }

    if (mem_caps->funcs)
    {
        fprintf(f, "funcs%c", csv_sep);
    }

    if (mem_caps->ctcss_tone)
    {
        fprintf(f, "ctcss_tone%c", csv_sep);
    }

    if (mem_caps->ctcss_sql)
    {
        fprintf(f, "ctcss_sql%c", csv_sep);
    }

    if (mem_caps->dcs_code)
    {
        fprintf(f, "dcs_code%c", csv_sep);
    }

    if (mem_caps->dcs_sql)
    {
        fprintf(f, "dcs_sql%c", csv_sep);
    }

    if (mem_caps->scan_group)
    {
        fprintf(f, "scan_group%c", csv_sep);
    }

    if (mem_caps->flags)
    {
        fprintf(f, "flags%c", csv_sep);
    }

    fprintf(f, "\n");
}


/* Caution! Keep the function consistent with dump_csv_name and set_channel_data! */
int dump_csv_chan(RIG *rig,
                  vfo_t vfo,
                  channel_t **chan_pp,
                  int channel_num,
                  const chan_t *chan_list,
                  rig_ptr_t arg)
{
    FILE *f = arg;
    static channel_t chan;
    static int first_time = 1;
    const channel_cap_t *mem_caps = &chan_list->mem_caps;

    if (first_time)
    {
        dump_csv_name(mem_caps, f);
        first_time = 0;
    }

    if (*chan_pp == NULL)
    {
        /*
         * Hamlib frontend demand application an allocated
         * channel_t pointer for next round.
         */
        *chan_pp = &chan;

        return RIG_OK;
    }

    fprintf(f, "%d%c", chan.channel_num, csv_sep);

    if (mem_caps->bank_num)
    {
        fprintf(f, "%d%c", chan.bank_num, csv_sep);
    }

    if (mem_caps->channel_desc)
    {
        fprintf(f, "%s%c", chan.channel_desc, csv_sep);
    }

    if (mem_caps->vfo)
    {
        fprintf(f, "%s%c", rig_strvfo(chan.vfo), csv_sep);
    }

    if (mem_caps->ant)
    {
        fprintf(f, "%u%c", chan.ant, csv_sep);
    }

    if (mem_caps->freq)
    {
        fprintf(f, "%"PRIfreq"%c", chan.freq, csv_sep);
    }

    if (mem_caps->mode)
    {
        fprintf(f, "%s%c", rig_strrmode(chan.mode), csv_sep);
    }

    if (mem_caps->width)
    {
        fprintf(f, "%d%c", (int)chan.width, csv_sep);
    }

    if (mem_caps->tx_freq)
    {
        fprintf(f, "%"PRIfreq"%c", chan.tx_freq, csv_sep);
    }

    if (mem_caps->tx_mode)
    {
        fprintf(f, "%s%c", rig_strrmode(chan.tx_mode), csv_sep);
    }

    if (mem_caps->tx_width)
    {
        fprintf(f, "%d%c", (int)chan.tx_width, csv_sep);
    }

    if (mem_caps->split)
    {
        fprintf(f, "%s%c", chan.split == RIG_SPLIT_ON ? "on" : "off", csv_sep);
    }

    if (mem_caps->tx_vfo)
    {
        fprintf(f, "%s%c", rig_strvfo(chan.tx_vfo), csv_sep);
    }

    if (mem_caps->rptr_shift)
    {
        fprintf(f, "%s%c", rig_strptrshift(chan.rptr_shift), csv_sep);
    }

    if (mem_caps->rptr_offs)
    {
        fprintf(f, "%d%c", (int)chan.rptr_offs, csv_sep);
    }

    if (mem_caps->tuning_step)
    {
        fprintf(f, "%d%c", (int)chan.tuning_step, csv_sep);
    }

    if (mem_caps->rit)
    {
        fprintf(f, "%d%c", (int)chan.rit, csv_sep);
    }

    if (mem_caps->xit)
    {
        fprintf(f, "%d%c", (int)chan.xit, csv_sep);
    }

    if (mem_caps->funcs)
    {
        fprintf(f, "%"PRXll"%c", chan.funcs, csv_sep);
    }

    if (mem_caps->ctcss_tone)
    {
        fprintf(f, "%u%c", chan.ctcss_tone, csv_sep);
    }

    if (mem_caps->ctcss_sql)
    {
        fprintf(f, "%u%c", chan.ctcss_sql, csv_sep);
    }

    if (mem_caps->dcs_code)
    {
        fprintf(f, "%u%c", chan.dcs_code, csv_sep);
    }

    if (mem_caps->dcs_sql)
    {
        fprintf(f, "%u%c", chan.dcs_sql, csv_sep);
    }

    if (mem_caps->scan_group)
    {
        fprintf(f, "%d%c", chan.scan_group, csv_sep);
    }

    if (mem_caps->flags)
    {
        fprintf(f, "%x%c", chan.flags, csv_sep);
    }

    fprintf(f, "\n");

    /*
     * keep the same *chan_pp for next round, thanks
     * to chan being static
     */

    return RIG_OK;
}


/**  Function to parse a line read from csv file and store the data into
     appropriate fields of channel_t. The function must be consistent
     with dump_csv_name and dump_csv_chan.
     \param rig - a pointer to the rig
     \param chan - a pointer to channel_t structure with channel data
     \param line_key_list - a pointer to a table of strings with "keys"
     \param line_data_list - a pointer to a table of strings with values
     \return 0 on success, negative value on error
*/
int set_channel_data(RIG *rig,
                     channel_t *chan,
                     char **line_key_list,
                     char **line_data_list)
{

    int i, j, n;
    const channel_cap_t *mem_caps;

    memset(chan, 0, sizeof(channel_t));
    chan->vfo = RIG_VFO_CURR;

    i = find_on_list(line_key_list, "num");

    if (i < 0)
    {
        fprintf(stderr, "No channel number\n");
        return -1;
    }

    n = chan->channel_num = atoi(line_data_list[ i ]);

    /* find channel caps of appropriate memory group? */
    for (j = 0; j < HAMLIB_CHANLSTSIZ; j++)
    {
        if (rig->state.chan_list[j].startc <= n && rig->state.chan_list[j].endc >= n)
        {
            break;
        }
    }

    if (j == HAMLIB_CHANLSTSIZ)
    {
        return -RIG_EINVAL;
    }

    printf("Requested channel number %d, list number %d\n", n, j);

    mem_caps = &rig->state.chan_list[j].mem_caps;

    if (mem_caps->bank_num)
    {
        i = find_on_list(line_key_list,  "bank_num");

        if (i >= 0)
        {
            chan->bank_num = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->channel_desc)
    {
        i = find_on_list(line_key_list,  "channel_desc");

        if (i >= 0)
        {
            strncpy(chan->channel_desc, line_data_list[ i ], rig->caps->chan_desc_sz - 1);
            chan->channel_desc[ rig->caps->chan_desc_sz ] = '\0';
        }
    }

    if (mem_caps->ant)
    {
        i = find_on_list(line_key_list,  "ant");

        if (i >= 0)
        {
            chan->ant = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->freq)
    {
        i = find_on_list(line_key_list,  "freq");

        if (i >= 0)
        {
            chan->freq = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->mode)
    {
        i = find_on_list(line_key_list,  "mode");

        if (i >= 0)
        {
            chan->mode = rig_parse_mode(line_data_list[ i ]);
        }
    }

    if (mem_caps->width)
    {
        i = find_on_list(line_key_list,  "width");

        if (i >= 0)
        {
            chan->width = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->tx_freq)
    {
        i = find_on_list(line_key_list,  "tx_freq");

        if (i >= 0)
        {
            sscanf(line_data_list[i], "%"SCNfreq, &chan->tx_freq);
        }
    }

    if (mem_caps->tx_mode)
    {
        i = find_on_list(line_key_list,  "tx_mode");

        if (i >= 0)
        {
            chan->tx_mode = rig_parse_mode(line_data_list[ i ]);
        }
    }

    if (mem_caps->tx_width)
    {
        i = find_on_list(line_key_list,  "tx_width");

        if (i >= 0)
        {
            chan->tx_width = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->split)
    {
        chan->split = RIG_SPLIT_OFF;
        i = find_on_list(line_key_list,  "split");

        if (i >= 0)
        {
            if (strcmp(line_data_list[i], "on") == 0)
            {
                chan->split = RIG_SPLIT_ON;

                if (mem_caps->tx_vfo)
                {
                    i = find_on_list(line_key_list, "tx_vfo");

                    if (i >= 0)
                    {
                        sscanf(line_data_list[i], "%x", &chan->tx_vfo);
                    }
                }
            }
        }
    }

    if (mem_caps->rptr_shift)
    {
        i = find_on_list(line_key_list,  "rptr_shift");

        if (i >= 0)
        {
            switch (line_data_list[i][0])
            {
            case '=':
                chan->rptr_shift = RIG_RPT_SHIFT_NONE;
                break;

            case '+':
                chan->rptr_shift = RIG_RPT_SHIFT_PLUS;
                break;

            case '-':
                chan->rptr_shift = RIG_RPT_SHIFT_MINUS;
                break;
            }

            if (mem_caps->rptr_offs && chan->rptr_shift != RIG_RPT_SHIFT_NONE)
            {
                i = find_on_list(line_key_list, "rptr_offs");

                if (i >= 0)
                {
                    chan->rptr_offs = atoi(line_data_list[ i ]);
                }
            }
        }
    }

    if (mem_caps->tuning_step)
    {
        i = find_on_list(line_key_list,  "tuning_step");

        if (i >= 0)
        {
            chan->tuning_step = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->rit)
    {
        i = find_on_list(line_key_list,  "rit");

        if (i >= 0)
        {
            chan->rit = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->xit)
    {
        i = find_on_list(line_key_list,  "xit");

        if (i >= 0)
        {
            chan->xit = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->funcs)
    {
        i = find_on_list(line_key_list,  "funcs");

        if (i >= 0)
        {
            sscanf(line_data_list[i], "%"SCNXll, &chan->funcs);
        }
    }

    if (mem_caps->ctcss_tone)
    {
        i = find_on_list(line_key_list,  "ctcss_tone");

        if (i >= 0)
        {
            chan->ctcss_tone = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->ctcss_sql)
    {
        i = find_on_list(line_key_list,  "ctcss_sql");

        if (i >= 0)
        {
            chan->ctcss_sql = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->dcs_code)
    {
        i = find_on_list(line_key_list,  "dcs_code");

        if (i >= 0)
        {
            chan->dcs_code = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->dcs_sql)
    {
        i = find_on_list(line_key_list,  "dcs_sql");

        if (i >= 0)
        {
            chan->dcs_sql = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->scan_group)
    {
        i = find_on_list(line_key_list,  "scan_group");

        if (i >= 0)
        {
            chan->scan_group = atoi(line_data_list[ i ]);
        }
    }

    if (mem_caps->flags)
    {
        i = find_on_list(line_key_list,  "flags");

        if (i >= 0)
        {
            sscanf(line_data_list[i], "%x", &chan->flags);
        }
    }

    return 0;
}


/** Find a string on the list. Assumes the last element on the list is NULL
    \param list - a list
    \param what - a string to find
    \return string position on the list on success,
           -1 if string not found or if string is empty
*/
int find_on_list(char **list, char *what)
{
    int i = 0;

    if (!what)
    {
        return -1;
    }

    if (!list[i])
    {
        return -1;
    }

    while (list[i] != NULL)
    {
        if (strcmp(list[i], what) == 0)
        {
            return i;
        }
        else
        {
            i++;
        }
    }

    return i;
}
