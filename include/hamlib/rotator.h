/*
 *  Hamlib Interface - Rotator API header
 *  Copyright (c) 2000,2001 by Stephane Fillod
 *
 *		$Id: rotator.h,v 1.1 2001-12-27 21:41:11 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _ROT_H
#define _ROT_H 1

#include <hamlib/rig.h>
#include <hamlib/rotlist.h>
#include <stdio.h>		/* required for FILE definition */
#include <sys/time.h>		/* required for struct timeval */


__BEGIN_DECLS

/* Forward struct references */

struct rot;
struct rot_state;

typedef struct rot ROT;

typedef float elevation_t;
typedef float azimuth_t;


#define ROT_FLAG_AZIMUTH		(1<<1)
#define ROT_FLAG_ELEVATION		(1<<2)


#define ROT_TYPE_OTHER		0



/* Basic rot type, can store some useful
* info about different rotators. Each lib must
* be able to populate this structure, so we can make
* useful enquiries about capablilities.
*/

/* 
 * The main idea of this struct is that it will be defined by the backend
 * rig driver, and will remain readonly for the application.
 * Fields that need to be modifiable by the application are
 * copied into the struct rot_state, which is a kind of private
 * of the ROT instance.
 * This way, you can have several rigs running within the same application,
 * sharing the struct rot_caps of the backend, while keeping their own
 * customized data.
 * NB: don't move fields around, as backend depends on it when initializing
 * 		their caps.
 */
struct rot_caps {
  rot_model_t rot_model;
  const char *model_name;
  const char *mfg_name;
  const char *version;
  const char *copyright;
  enum rig_status_e status;

  int rot_type;
  enum rig_port_e port_type;

  int serial_rate_min;
  int serial_rate_max;
  int serial_data_bits;
  int serial_stop_bits;
  enum serial_parity_e serial_parity;
  enum serial_handshake_e serial_handshake;

  int write_delay;
  int post_write_delay;
  int timeout;
  int retry;

  /* 
   * Movement range, az is relative to North
   * negative values allowed for overlap
   */
  azimuth_t min_az;
  azimuth_t max_az;
  elevation_t min_el;
  elevation_t max_el;


  const struct confparams *cfgparams;
  const rig_ptr_t priv;

  /*
   * Rot Admin API
   *
   */
 
  int (*rot_init)(ROT *rot);
  int (*rot_cleanup)(ROT *rot);
  int (*rot_open)(ROT *rot);
  int (*rot_close)(ROT *rot);
  
  int (*set_conf)(ROT *rot, token_t token, const char *val);
  int (*get_conf)(ROT *rot, token_t token, char *val);

  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */
  
  int (*set_position)(ROT *rot, azimuth_t azimuth, elevation_t elevation);
  int (*get_position)(ROT *rot, azimuth_t *azimuth, elevation_t *elevation);

  int (*stop)(ROT *rot);
  int (*park)(ROT *rot);

  int (*reset)(ROT *rot, reset_t reset);

  /* get firmware info, etc. */
  const char* (*get_info)(ROT *rot);

  /* more to come... */
};


/* 
 * Rotator state
 *
 * This struct contains live data, as well as a copy of capability fields
 * that may be updated (ie. customized)
 *
 * It is fine to move fields around, as this kind of struct should
 * not be initialized like caps are.
 */
struct rot_state {
	/*
	 * overridable fields
	 */
  azimuth_t min_az;
  azimuth_t max_az;
  elevation_t min_el;
  elevation_t max_el;

	/*
	 * non overridable fields, internal use
	 */
  port_t rotport;

  int comm_state;	/* opened or not */
  /*
   * Pointer to private data
   */
  rig_ptr_t priv;
  
  /*
   * internal use by hamlib++ for event handling
   */
  rig_ptr_t obj;

  /* etc... */
};

/* 
 * struct rot is the master data structure, 
 * acting as a handle for the controlled rotator.
 */
struct rot {
	struct rot_caps *caps;
	struct rot_state state;
};



/* --------------- API function prototypes -----------------*/

extern HAMLIB_EXPORT(ROT *) rot_init HAMLIB_PARAMS((rot_model_t rot_model));
extern HAMLIB_EXPORT(int) rot_open HAMLIB_PARAMS((ROT *rot));
extern HAMLIB_EXPORT(int) rot_close HAMLIB_PARAMS((ROT *rot));
extern HAMLIB_EXPORT(int) rot_cleanup HAMLIB_PARAMS((ROT *rot));

  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */

extern HAMLIB_EXPORT(int) rot_set_position HAMLIB_PARAMS((ROT *rot, azimuth_t azimuth, elevation_t elevation));
extern HAMLIB_EXPORT(int) rot_get_position HAMLIB_PARAMS((ROT *rot, azimuth_t *azimuth, elevation_t *elevation));

extern HAMLIB_EXPORT(int) rot_register HAMLIB_PARAMS((const struct rot_caps *caps));
extern HAMLIB_EXPORT(int) rot_unregister HAMLIB_PARAMS((rot_model_t rot_model));
extern HAMLIB_EXPORT(int) rot_list_foreach HAMLIB_PARAMS((int (*cfunc)(const struct rot_caps*, rig_ptr_t), rig_ptr_t data));
extern HAMLIB_EXPORT(int) rot_load_backend HAMLIB_PARAMS((const char *be_name));
extern HAMLIB_EXPORT(int) rot_check_backend HAMLIB_PARAMS((rot_model_t rot_model));
extern HAMLIB_EXPORT(int) rot_load_all_backends HAMLIB_PARAMS(());
extern HAMLIB_EXPORT(rot_model_t) rot_probe_all HAMLIB_PARAMS((port_t *p));

extern HAMLIB_EXPORT(const struct rot_caps *) rot_get_caps HAMLIB_PARAMS((rot_model_t rot_model));


/*
 * 1 towards 2
 * returns qrb in km
 * and azimuth in decimal degrees
 */
extern HAMLIB_EXPORT(int) qrb HAMLIB_PARAMS((double lon1, double lat1, 
						double lon2, double lat2, 
						double *bearing, double *azimuth));
extern HAMLIB_EXPORT(double) bearing_long_path HAMLIB_PARAMS((double bearing));
extern HAMLIB_EXPORT(double) azimuth_long_path HAMLIB_PARAMS((double azimuth));

extern HAMLIB_EXPORT(int) longlat2locator HAMLIB_PARAMS((double longitude, 
						double latitude, char *locator));
extern HAMLIB_EXPORT(int) locator2longlat HAMLIB_PARAMS((double *longitude, 
						double *latitude, const char *locator));

extern HAMLIB_EXPORT(double) dms2dec HAMLIB_PARAMS((int degs, int minutes, 
						int seconds));
extern HAMLIB_EXPORT(void) dec2dms HAMLIB_PARAMS((double dec, int *degs, 
						int *minutes, int *seconds));


#define rot_debug rig_debug

__END_DECLS

#endif /* _ROT_H */

