/* QMX CAT protocol regression tests. LGPL-2.1-or-later. */
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "kenwood.h"

extern struct rig_caps qrplabs_qmx_caps;

struct peer
{
    int fd;
    int selection;
    int failed;
    int writes;
    char mode;
    int auto_info_off;
    int rx_filter;
    int active_filter;
    int filter_writes;
    int reloads;
};

static void *serve(void *arg)
{
    struct peer *p = arg;
    char cmd[128], reply[64];
    size_t n = 0;

    while (read(p->fd, cmd + n, 1) == 1)
    {
        if (cmd[n++] != ';')
        {
            if (n == sizeof(cmd) - 1) { p->failed = 1; break; }
            continue;
        }
        cmd[n] = 0;
        n = 0;
        reply[0] = 0;
        if (!strcmp(cmd, "ID;")) { strcpy(reply, "ID020;"); }
        else if (!strcmp(cmd, "AI0;")) { p->auto_info_off = 1; }
        else if (!strcmp(cmd, "MMDisplay/controls|Pwr/SWR display|Power fullscale;"))
        { strcpy(reply, "MM6W;"); }
        else if (!strcmp(cmd, "VN;")) { strcpy(reply, "VN1_04_004QMX;"); }
        else if (!strcmp(cmd, "MD;"))
        { snprintf(reply, sizeof(reply), "MD%c;", p->mode); }
        else if (!strcmp(cmd, "FW;"))
        { snprintf(reply, sizeof(reply), "FW%04d;",
                   p->mode == '3' || p->mode == '7' ? 300 :
                   p->mode == '1' || p->mode == '2' ? p->active_filter : 3200); }
        else if (!strcmp(cmd, "MMSSB|Filter RX;"))
        { snprintf(reply, sizeof(reply), "MM%d;", p->rx_filter); }
        else if (sscanf(cmd, "MMSSB|Filter RX=%d;", &p->rx_filter) == 1)
        { p->filter_writes++; }
        else if (!strcmp(cmd, "MU;"))
        { p->active_filter = p->rx_filter; p->reloads++; }
        else if (strlen(cmd) == 4 && !strncmp(cmd, "MD", 2)
                 && strchr("1235679", cmd[2]))
        { p->mode = cmd[2]; p->writes++; }
        else if (!strcmp(cmd, "FA;") || !strcmp(cmd, "FB;"))
        { snprintf(reply, sizeof(reply), "%c%c00014074000;", cmd[0], cmd[1]); }
        else if (!strcmp(cmd, "FR;"))
        { snprintf(reply, sizeof(reply), "FR%d;", p->selection == 1); }
        else if (!strcmp(cmd, "FT;"))
        { snprintf(reply, sizeof(reply), "FT%d;", p->selection != 0); }
        else if (!strcmp(cmd, "SP;"))
        { snprintf(reply, sizeof(reply), "SP%d;", p->selection == 2); }
        else if (!strcmp(cmd, "FR0;") || !strcmp(cmd, "FR1;") || !strcmp(cmd, "FR2;"))
        { p->selection = cmd[2] - '0'; p->writes++; }
        else
        {
            fprintf(stderr, "Unexpected QMX command: %s\n", cmd);
            p->failed = 1;
            strcpy(reply, "?;");
        }
        if (reply[0] && write(p->fd, reply, strlen(reply)) != (ssize_t)strlen(reply))
        { p->failed = 1; break; }
    }
    return NULL;
}

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "QMX check failed at line %d: %s\n", __LINE__, #expr); \
    failed = 1; } } while (0)
#endif

int main(void)
{
#ifdef _WIN32
    return 77;
#else
    int sockets[2], failed = 0, writes;
    pthread_t thread;
    struct peer peer = {.rx_filter = 2500, .active_filter = 2500};
    RIG *rig;
    vfo_t vfo;
    split_t split;
    rmode_t mode;
    pbwidth_t width;
    const char *info;
    struct kenwood_priv_data *priv;

    rig_register(&qrplabs_qmx_caps);
    rig = rig_init(RIG_MODEL_QRPLABS_QMX);
    if (!rig || socketpair(AF_UNIX, SOCK_STREAM, 0, sockets)) { return 1; }
    peer.fd = sockets[1];
    if (pthread_create(&thread, NULL, serve, &peer)) { return 1; }
    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 500;
    RIGPORT(rig)->retry = 0;
    STATE(rig)->comm_state = 1;
    CHECK(rig->caps->rig_open(rig) == RIG_OK);
    CHECK(peer.auto_info_off);
    priv = STATE(rig)->priv;
    priv->is_emulation = 1;
    priv->curr_mode = RIG_MODE_CW;

    CHECK(rig->caps->set_vfo(rig, RIG_VFO_B) == RIG_OK);
    CHECK(rig->caps->get_vfo(rig, &vfo) == RIG_OK && vfo == RIG_VFO_B);
    CHECK(rig->caps->get_split_vfo(rig, RIG_VFO_CURR, &split, &vfo) == RIG_OK
          && split == RIG_SPLIT_OFF && vfo == RIG_VFO_B);
    CHECK(rig->caps->set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B) == RIG_OK);
    CHECK(rig->caps->get_split_vfo(rig, RIG_VFO_CURR, &split, &vfo) == RIG_OK
          && split == RIG_SPLIT_ON && vfo == RIG_VFO_B);
    CHECK(rig->caps->get_vfo(rig, &vfo) == RIG_OK && vfo == RIG_VFO_A);
    writes = peer.writes;
    CHECK(rig->caps->set_split_vfo(rig, RIG_VFO_B, RIG_SPLIT_ON, RIG_VFO_A) == -RIG_EINVAL);
    CHECK(rig->caps->set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_A) == -RIG_EINVAL);
    CHECK(rig->caps->set_vfo(rig, RIG_VFO_MEM) == -RIG_EINVAL);
    CHECK(peer.writes == writes);
    CHECK(rig->caps->set_split_vfo(rig, RIG_VFO_B, RIG_SPLIT_OFF, RIG_VFO_B) == RIG_OK);
    CHECK(rig->caps->get_split_vfo(rig, RIG_VFO_CURR, &split, &vfo) == RIG_OK
          && split == RIG_SPLIT_OFF && vfo == RIG_VFO_B);
    CHECK(rig->caps->set_split_vfo(rig, RIG_VFO_CURR, RIG_SPLIT_OFF, RIG_VFO_A) == RIG_OK);
    CHECK(rig->caps->get_vfo(rig, &vfo) == RIG_OK && vfo == RIG_VFO_B);

    {
        const rmode_t modes[] = {RIG_MODE_LSB, RIG_MODE_USB, RIG_MODE_CW,
                                RIG_MODE_AM, RIG_MODE_PKTLSB, RIG_MODE_CWR,
                                RIG_MODE_PKTUSB};
        const char codes[] = "1235679";
        size_t i;

        for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
        {
            CHECK(rig->caps->set_mode(rig, RIG_VFO_B, modes[i], RIG_PASSBAND_NORMAL) == RIG_OK);
            CHECK(rig->caps->get_mode(rig, RIG_VFO_B, &mode, &width) == RIG_OK);
            CHECK(mode == modes[i] && peer.mode == codes[i]);
            CHECK(width == (mode == RIG_MODE_CW || mode == RIG_MODE_CWR ? 300 :
                            mode == RIG_MODE_USB || mode == RIG_MODE_LSB ? 2500 : 3200));
        }
    }
    writes = peer.writes;
    CHECK(rig->caps->set_mode(rig, RIG_VFO_A, RIG_MODE_FM, RIG_PASSBAND_NORMAL) == -RIG_EINVAL);
    CHECK(rig->caps->set_mode(rig, RIG_VFO_A, RIG_MODE_NONE, RIG_PASSBAND_NORMAL) == -RIG_EINVAL);
    CHECK(rig->caps->set_mode(rig, RIG_VFO_A, RIG_MODE_CW, 500) == -RIG_ENAVAIL);
    CHECK(peer.writes == writes);
    CHECK(rig->caps->set_mode(rig, RIG_VFO_A, RIG_MODE_CW, 300) == RIG_OK);
    CHECK(rig->caps->set_mode(rig, RIG_VFO_A, RIG_MODE_USB, RIG_PASSBAND_NOCHANGE) == RIG_OK);
    info = rig->caps->get_info(rig);
    CHECK(info && !strcmp(info, "1_04_004QMX"));
    CHECK(rig->caps->dcd_type == RIG_DCD_NONE);
    CHECK(rig->caps->preamp[0] == 0 && rig->caps->attenuator[0] == 0);

    /* Public API: fresh cache entries must match the QMX's whole-VFO selector. */
    CHECK(rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B) == RIG_OK);
    CHECK(rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_OFF, RIG_VFO_A) == RIG_OK);
    CHECK(rig_set_vfo(rig, RIG_VFO_B) == RIG_OK);
    CHECK(rig_get_split_vfo(rig, RIG_VFO_CURR, &split, &vfo) == RIG_OK
          && split == RIG_SPLIT_OFF && vfo == RIG_VFO_B);
    CHECK(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_USB, RIG_PASSBAND_NORMAL) == RIG_OK);
    CHECK(rig_get_mode(rig, RIG_VFO_B, &mode, &width) == RIG_OK
          && mode == RIG_MODE_USB && width == 2500);

    CHECK(rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B) == RIG_OK);
    CHECK(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_CW, RIG_PASSBAND_NORMAL) == RIG_OK);
    CHECK(rig->caps->get_split_vfo(rig, RIG_VFO_CURR, &split, &vfo) == RIG_OK
          && split == RIG_SPLIT_ON && vfo == RIG_VFO_B);
    CHECK(rig_get_mode(rig, RIG_VFO_A, &mode, &width) == RIG_OK
          && mode == RIG_MODE_CW && width == 300);
    CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_PKTUSB, RIG_PASSBAND_NORMAL) == RIG_OK);
    CHECK(rig_get_mode(rig, RIG_VFO_B, &mode, &width) == RIG_OK
          && mode == RIG_MODE_PKTUSB && peer.mode == '9');

    /* All SSB RX widths apply even with MM Effect=On demand. */
    {
        const int widths[] = {2700, 2900, 3200, 2500};
        size_t i;
        int filter_writes;

        for (i = 0; i < sizeof(widths) / sizeof(widths[0]); i++)
        {
            CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USB, widths[i]) == RIG_OK);
            CHECK(rig_get_mode(rig, RIG_VFO_A, &mode, &width) == RIG_OK
                  && mode == RIG_MODE_USB && width == widths[i]);
            CHECK(peer.active_filter == widths[i]);
            for (size_t k = 0; k < sizeof(widths) / sizeof(widths[0]); k++)
            {
                int found = 0;
                for (int j = 0; j < HAMLIB_FLTLSTSIZ && STATE(rig)->filters[j].modes; j++)
                {
                    if ((STATE(rig)->filters[j].modes & RIG_MODE_USB)
                            && STATE(rig)->filters[j].width == widths[k]) { found = 1; }
                }
                CHECK(found);
            }
            CHECK(rig->caps->get_split_vfo(rig, RIG_VFO_CURR, &split, &vfo) == RIG_OK
                  && split == RIG_SPLIT_ON && vfo == RIG_VFO_B);
            filter_writes = peer.filter_writes;
            CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_LSB, RIG_PASSBAND_NORMAL) == RIG_OK);
            CHECK(rig_get_mode(rig, RIG_VFO_A, &mode, &width) == RIG_OK
                  && mode == RIG_MODE_LSB && width == widths[i]);
            CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USB, RIG_PASSBAND_NOCHANGE) == RIG_OK);
            CHECK(rig_get_mode(rig, RIG_VFO_B, &mode, &width) == RIG_OK
                  && mode == RIG_MODE_USB && width == widths[i]);
            CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USB, widths[i]) == RIG_OK);
            CHECK(peer.filter_writes == filter_writes);
        }
        filter_writes = peer.filter_writes;
        CHECK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USB, 2800) == -RIG_EINVAL);
        CHECK(peer.filter_writes == filter_writes);
        CHECK(peer.reloads > 0);
    }

    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    pthread_join(thread, NULL);
    close(sockets[1]);
    return failed || peer.failed;
#endif
}
