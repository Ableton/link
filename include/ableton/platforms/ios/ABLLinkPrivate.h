// Copyright: 2015, Ableton AG, Berlin. All rights reserved.

#pragma once

#include "ABLLink.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** Force a remapping of the beat time onto the host time, allowing
      the client to specify a new phase to the session. This function
      takes the tempo as well to avoid the necessity of sending
      multiple messages to the session if the phase and tempo must be
      changed at the same time.
  */
  void ABLLinkForceBeatTime(
    ABLLinkRef,
    double beatTime,
    uint64_t hostTimeAtOutput,
    double bpm);

#ifdef __cplusplus
}
#endif
