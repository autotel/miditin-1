<!-- New requirements go here as plain bullets. Opus converts them into tasks.md and empties this
     file; the git history of these two files is the record of what was asked and delivered. -->
- add an audio input lfo, which measures audio rms; this is used directly as the energy level
- to signal low pass and high pass, add a "negative only" and "positive only" options: makes the filter work only on the positive edges or only on the negative edges. this way, for low pass filter, one can for example allow a fast onset and still have a slow release.

