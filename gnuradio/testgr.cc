/* -*- Mode: c++ -*- */
/*
 * Copyright 2002 Stephane Fillod
 * 
 * This file is part of Hamlib. Some parts from GNU radio.
 * 
 */

#include <VrSigSource.h>
#include <GrFFTSink.h>
#include <VrNullSink.h>
#include <GrMultiply.h>
#include <VrSum.h>
#include <VrConnect.h>
#include <VrMultiTask.h>
#include <VrGUI.h>

#include <stdio.h>
#include <hamlib/rig.h>
#include "gr_priv.h"

#define CARRIER_FREQ	          	        1.070e6	// AM 1070

static  RIG *my_rig;

void set_my_freq(double f)
{
  rig_set_freq(my_rig, RIG_VFO_CURR, (freq_t)f);
}

int main(int argc, char *argv[])
{
  int retcode;		/* generic return code from functions */
  struct gnuradio_priv_data *priv;

  GrFFTSink<float> *fftsink;

  VrGUI *guimain = new VrGUI(argc, argv);
  VrGUILayout *horiz = guimain->top->horizontal();
  VrGUILayout *vert = horiz->vertical();


  printf("testgr:hello, I am your main() !\n");

  rig_set_debug(RIG_DEBUG_TRACE);

  my_rig = rig_init(RIG_MODEL_GNURADIO);
  if (!my_rig) {
	fprintf(stderr,"Unknown rig num: %d\n", RIG_MODEL_GNURADIO);
	fprintf(stderr,"Please check riglist.h\n");
	exit(1); /* whoops! something went wrong (mem alloc?) */
  }

  // turn this into a macro
  priv = (struct gnuradio_priv_data*)my_rig->state.priv;
  if (priv == NULL) {
	printf("Internal error, can't retrieve priv data!\n");
	exit(3);
  }

  retcode = rig_open(my_rig);	/* start */
  if (retcode != RIG_OK) {
	printf("rig_open: error = %s\n", rigerror(retcode));
	exit(2);
  }

  fftsink = new GrFFTSink<float>(vert, 300, 450, 1024);

  pthread_mutex_lock(&priv->mutex_process);
  priv->m->add (fftsink);

  // now wire it all together from the sink, back to the sources
  
  NWO_CONNECT (priv->source, fftsink);
  pthread_mutex_unlock(&priv->mutex_process);

  rig_set_freq(my_rig, RIG_VFO_CURR, (freq_t)CARRIER_FREQ-kHz(10));
  rig_set_mode(my_rig, RIG_VFO_CURR, RIG_MODE_USB, RIG_PASSBAND_NORMAL);

  (void) new VrGUINumber(horiz, "Carrier", "Frequency (Hz)", set_my_freq,
			 priv->input_rate/2, CARRIER_FREQ);

  guimain->start();

  while (1) {
  pthread_mutex_lock(&priv->mutex_process);
    guimain->processEvents(10 /*ms*/);
  pthread_mutex_unlock(&priv->mutex_process);
    //priv->m->process();
  }

  rig_close(my_rig); /* close port */
  rig_cleanup(my_rig); /* if you care about memory */

  printf("Rig closed ok\n");

  return 0;
}

