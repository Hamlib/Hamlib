/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * class.h - Copyright (C) 2000,2001 Frank Singleton and Stephane Fillod
 * This program defines the Hamlib API and rig capabilities structures that
 * will be used for obtaining rig capabilities. C++ version.
 *
 *
 *	$Id: rigclass.h,v 1.1 2001-06-15 06:58:12 f4cfe Exp $
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#ifndef _RIGCLASS_H
#define _RIGCLASS_H 1

#include <hamlib/rig.h>

#include <hamlib/rig_dll.h>


class HAMLIB_IMPEXP Rig {
private:
  RIG* theRig;  // Global ref. to the rig

protected:
public:
  Rig(rig_model_t rig_model);

  virtual ~Rig();

  const struct rig_caps *caps;

  // This method open the communication port to the rig
  void open(void);

  // This method close the communication port to the rig
  void close(void);

  void setFreq(freq_t freq, vfo_t vfo = RIG_VFO_CURR);
  freq_t getFreq(vfo_t vfo = RIG_VFO_CURR);
  void setMode(rmode_t, pbwidth_t width = RIG_PASSBAND_NORMAL, vfo_t vfo = RIG_VFO_CURR);
  rmode_t getMode(pbwidth_t&, vfo_t vfo = RIG_VFO_CURR);
  void setVFO(vfo_t);
  vfo_t getVFO();

  void setPTT (ptt_t ptt, vfo_t vfo = RIG_VFO_CURR);
  ptt_t getPTT (vfo_t vfo = RIG_VFO_CURR);
  dcd_t getDCD (vfo_t vfo = RIG_VFO_CURR);

  void setLevel(setting_t level, int vali, vfo_t vfo = RIG_VFO_CURR);
  void setLevel(setting_t level, float valf, vfo_t vfo = RIG_VFO_CURR);
  void getLevel(setting_t level, int& vali, vfo_t vfo = RIG_VFO_CURR);
  void getLevel(setting_t level, float& valf, vfo_t vfo = RIG_VFO_CURR);
  setting_t hasGetLevel (setting_t level);
  setting_t hasSetLevel (setting_t level);

  const char *getInfo (void);
  pbwidth_t passbandNormal (rmode_t);
  pbwidth_t passbandNarrow (rmode_t);
  pbwidth_t passbandWide (rmode_t);

  // callbacks available in your derived object
  virtual int FreqEvent(vfo_t, freq_t) const {
		  return RIG_OK;
  }
  virtual int ModeEvent(vfo_t, rmode_t, pbwidth_t) const {
		  return RIG_OK;
  }
  virtual int VFOEvent(vfo_t) const {
		  return RIG_OK;
  }
  virtual int PTTEvent(vfo_t, ptt_t) const {
		  return RIG_OK;
  }
  virtual int DCDEvent(vfo_t, dcd_t) const {
		  return RIG_OK;
  }


};
 

#if 0

extern HAMLIB_EXPORT(int) rig_set_rptr_shift HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift));
extern HAMLIB_EXPORT(int) rig_get_rptr_shift HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift));
extern HAMLIB_EXPORT(int) rig_set_rptr_offs HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t rptr_offs));
extern HAMLIB_EXPORT(int) rig_get_rptr_offs HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs));

extern HAMLIB_EXPORT(int) rig_set_ctcss HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t tone));
extern HAMLIB_EXPORT(int) rig_get_ctcss HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t *tone));
extern HAMLIB_EXPORT(int) rig_set_dcs HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t code));
extern HAMLIB_EXPORT(int) rig_get_dcs HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t *code));

extern HAMLIB_EXPORT(int) rig_set_ctcss_sql HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t tone));
extern HAMLIB_EXPORT(int) rig_get_ctcss_sql HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t *tone));
extern HAMLIB_EXPORT(int) rig_set_dcs_sql HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t code));
extern HAMLIB_EXPORT(int) rig_get_dcs_sql HAMLIB_PARAMS((RIG *rig, vfo_t vfo, tone_t *code));

extern HAMLIB_EXPORT(int) rig_set_split_freq HAMLIB_PARAMS((RIG *rig, vfo_t vfo, freq_t rx_freq, freq_t tx_freq));
extern HAMLIB_EXPORT(int) rig_get_split_freq HAMLIB_PARAMS((RIG *rig, vfo_t vfo, freq_t *rx_freq, freq_t *tx_freq));
extern HAMLIB_EXPORT(int) rig_set_split_mode HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rmode_t rx_mode, pbwidth_t rx_width, rmode_t tx_mode, pbwidth_t tx_width));
extern HAMLIB_EXPORT(int) rig_get_split_mode HAMLIB_PARAMS((RIG *rig, vfo_t vfo, rmode_t *rx_mode, pbwidth_t *rx_width, rmode_t *tx_mode, pbwidth_t *tx_width));
extern HAMLIB_EXPORT(int) rig_set_split HAMLIB_PARAMS((RIG *rig, vfo_t vfo, split_t split));
extern HAMLIB_EXPORT(int) rig_get_split HAMLIB_PARAMS((RIG *rig, vfo_t vfo, split_t *split));

extern HAMLIB_EXPORT(int) rig_set_rit HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t rit));
extern HAMLIB_EXPORT(int) rig_get_rit HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t *rit));
extern HAMLIB_EXPORT(int) rig_set_xit HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t xit));
extern HAMLIB_EXPORT(int) rig_get_xit HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t *xit));

extern HAMLIB_EXPORT(int) rig_set_ts HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t ts));
extern HAMLIB_EXPORT(int) rig_get_ts HAMLIB_PARAMS((RIG *rig, vfo_t vfo, shortfreq_t *ts));

extern HAMLIB_EXPORT(int) rig_power2mW HAMLIB_PARAMS((RIG *rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode));
extern HAMLIB_EXPORT(int) rig_mW2power HAMLIB_PARAMS((RIG *rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode));

extern HAMLIB_EXPORT(shortfreq_t) rig_get_resolution HAMLIB_PARAMS((RIG *rig, rmode_t mode));


#define rig_get_strength(r,v,s) rig_get_level((r),(v),RIG_LEVEL_STRENGTH, (value_t*)(s))

extern HAMLIB_EXPORT(int) rig_set_parm HAMLIB_PARAMS((RIG *rig, setting_t parm, value_t val));
extern HAMLIB_EXPORT(int) rig_get_parm HAMLIB_PARAMS((RIG *rig, setting_t parm, value_t *val));

extern HAMLIB_EXPORT(int) rig_set_powerstat HAMLIB_PARAMS((RIG *rig, powerstat_t status));
extern HAMLIB_EXPORT(int) rig_get_powerstat HAMLIB_PARAMS((RIG *rig, powerstat_t *status));

extern HAMLIB_EXPORT(int) rig_reset HAMLIB_PARAMS((RIG *rig, reset_t reset));	/* dangerous! */

extern HAMLIB_EXPORT(int) rig_set_ant HAMLIB_PARAMS((RIG *rig, vfo_t vfo, ant_t ant));	/* antenna */
extern HAMLIB_EXPORT(int) rig_get_ant HAMLIB_PARAMS((RIG *rig, vfo_t vfo, ant_t *ant));


extern HAMLIB_EXPORT(setting_t) rig_has_get_parm HAMLIB_PARAMS((RIG *rig, setting_t parm));
extern HAMLIB_EXPORT(setting_t) rig_has_set_parm HAMLIB_PARAMS((RIG *rig, setting_t parm));

extern HAMLIB_EXPORT(setting_t) rig_has_get_func HAMLIB_PARAMS((RIG *rig, setting_t func));
extern HAMLIB_EXPORT(setting_t) rig_has_set_func HAMLIB_PARAMS((RIG *rig, setting_t func));

extern HAMLIB_EXPORT(int) rig_set_func HAMLIB_PARAMS((RIG *rig, vfo_t vfo, setting_t func, int status));
extern HAMLIB_EXPORT(int) rig_get_func HAMLIB_PARAMS((RIG *rig, vfo_t vfo, setting_t func, int *status));

extern HAMLIB_EXPORT(int) rig_send_dtmf HAMLIB_PARAMS((RIG *rig, vfo_t vfo, const char *digits));
extern HAMLIB_EXPORT(int) rig_recv_dtmf HAMLIB_PARAMS((RIG *rig, vfo_t vfo, char *digits, int *length));
extern HAMLIB_EXPORT(int) rig_send_morse HAMLIB_PARAMS((RIG *rig, vfo_t vfo, const char *msg));

extern HAMLIB_EXPORT(int) rig_set_bank HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int bank));
extern HAMLIB_EXPORT(int) rig_set_mem HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int ch));
extern HAMLIB_EXPORT(int) rig_get_mem HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int *ch));
extern HAMLIB_EXPORT(int) rig_vfo_op HAMLIB_PARAMS((RIG *rig, vfo_t vfo, vfo_op_t op));
extern HAMLIB_EXPORT(vfo_op_t) rig_has_vfo_op HAMLIB_PARAMS((RIG *rig, vfo_op_t op));

extern HAMLIB_EXPORT(int) rig_restore_channel HAMLIB_PARAMS((RIG *rig, const channel_t *chan)); /* curr VFO */
extern HAMLIB_EXPORT(int) rig_save_channel HAMLIB_PARAMS((RIG *rig, channel_t *chan));
extern HAMLIB_EXPORT(int) rig_set_channel HAMLIB_PARAMS((RIG *rig, const channel_t *chan));	/* mem */
extern HAMLIB_EXPORT(int) rig_get_channel HAMLIB_PARAMS((RIG *rig, channel_t *chan));

extern HAMLIB_EXPORT(int) rig_set_trn HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int trn));
extern HAMLIB_EXPORT(int) rig_get_trn HAMLIB_PARAMS((RIG *rig, vfo_t vfo, int *trn));

#endif 


#ifdef __GNUG__
#  if ((__GNUG__ <= 2) && (__GNUC_MINOR__ < 8))
#    if HAVE_TYPEINFO
#      include <typeinfo>
#    endif
#  endif
#endif

#if defined(__GNUG__)
#  if HAVE_BUILTIN_H || HAVE_GXX_BUILTIN_H || HAVE_GPP_BUILTIN_H
#    if ETIP_NEEDS_MATH_H
#      if ETIP_NEEDS_MATH_EXCEPTION
#        undef exception
#        define exception math_exception
#      endif
#      include <math.h>
#    endif
#    undef exception
#    define exception builtin_exception
#    if HAVE_GPP_BUILTIN_H
#     include <gpp/builtin.h>
#    elif HAVE_GXX_BUILTIN_H
#     include <g++/builtin.h>
#    else
#     include <builtin.h>
#    endif
#    undef exception
#  endif
#elif defined (__SUNPRO_CC)
#  include <generic.h>
#  include <string.h>
#else
#  include <string.h>
#endif


extern "C" {
#if HAVE_VALUES_H
#  include <values.h>
#endif

#include <assert.h>
#include <eti.h>
#include <errno.h>
}

// Forward Declarations

class HAMLIB_IMPEXP RigException
{
public:
  const char *message;
  int errorno;

  RigException (const char* msg, int err)
    : message(msg), errorno (err)
    {};

  RigException (int err)
    : message(rigerror(err)), errorno (err)
    {};

  RigException (const char* msg)
    : message(msg), errorno (-RIG_EINTERNAL)
    {};

  virtual const char *classname() const {
    return "Rig";
  }
};

#if !(defined(__GNUG__)||defined(__SUNPRO_CC))
#  include <iostream.h>
   extern "C" void exit(int);
#endif

inline void THROW(const RigException *e) {
#if defined(__GNUG__)
#  if ((__GNUG__ <= 2) && (__GNUC_MINOR__ < 8))
      (*lib_error_handler)(e?e->classname():"",e?e->message:"");
#else
      throw *e;
#endif
#elif defined(__SUNPRO_CC)
  genericerror(1, ((e != 0) ? (char *)(e->message) : ""));
#else
  if (e)
    cerr << e->message << endl;
  exit(0);
#endif
}

#define THROWS(s)


#endif	// _RIGCLASS_H
