/* -*- Mode: c++ -*- */
/*
 * Copyright 2002 Stephane Fillod
 * 
 * This file is part of Hamlib. Some parts from GNU radio.
 * 
 */

#include <stdio.h>
#include <hamlib/rig.h>
#include "gr_priv.h"

#include <VrSigSource.h>
#include <GrFFTSink.h>
#include <VrNullSink.h>
#include <GrMultiply.h>
#include <VrSum.h>
#include <VrConnect.h>
#include <VrMultiTask.h>
#include "VrGUI.h" 

#define SAMPLING_FREQUENCY                          5e6
#define CARRIER_FREQ	          	        1.070e6	// AM 1070
#define MAX_FREQUENCY           (SAMPLING_FREQUENCY / 2)

static  RIG *my_rig;

void set_my_freq(double f)
{
  rig_set_freq(my_rig, RIG_VFO_CURR, (freq_t)f);
}

int main(int argc, char *argv[])
{
  int retcode;		/* generic return code from functions */
  struct gnuradio_priv_data *priv;

  GrFFTSink<IOTYPE> *fftsink;

  VrGUI *guimain = new VrGUI(argc, argv);
  VrGUILayout *horiz = guimain->top->horizontal();
  VrGUILayout *vert = horiz->vertical();


  printf("testgr:hello, I am your main() !\n");

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


  fftsink = new GrFFTSink<IOTYPE>(vert, 300, 450, 1024);

  priv->m->add (fftsink);

  (void) new VrGUINumber(horiz, "Carrier", "Frequency (Hz)", set_my_freq,
			 MAX_FREQUENCY, CARRIER_FREQ);


  // now wire it all together from the sink, back to the sources
  
  NWO_CONNECT (priv->source, fftsink);

  retcode = rig_open(my_rig);	/* start */
  if (retcode != RIG_OK) {
	printf("rig_open: error = %s\n", rigerror(retcode));
	exit(2);
  }

  guimain->start();

  while (1) {
    guimain->processEvents(10 /*ms*/);
    priv->m->process();
  }

  rig_close(my_rig); /* close port */
  rig_cleanup(my_rig); /* if you care about memory */

  printf("Rig closed ok\n");

  return 0;
}

