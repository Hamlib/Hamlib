
#ifndef _COMMRADIO_H
#define _COMMRADIO_H

int commradio_transaction(RIG *rig, const unsigned char *cmd, int cmd_len,
		unsigned char *data, int *data_len);
int commradio_init(RIG *rig);
int commradio_cleanup(RIG *rig);
int commradio_rig_open(RIG *rig);
int commradio_rig_close(RIG *rig);
int commradio_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int commradio_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int commradio_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int commradio_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int commradio_set_vfo(RIG *rig, vfo_t vfo);
int commradio_get_vfo(RIG *rig, vfo_t *vfo);

extern struct rig_caps ctx10_caps;

#endif /* _COMMRADIO_H */

