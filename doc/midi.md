Overview
========

Though libvomid doesn't use any file formats other than plain SMF,
internally file operations are implemented in terms of import/export, rather than open/save.
It means that:
* We can't tell for sure if the file just opened will be changed if we save it.
* libvomid in fact deals with some subset of MIDI standard (actually, it's much wider than
most existing sequencers can understand)

File format description
=======================

libvomid produces plain standard-conforming (actually, I'm not quite sure, but
fucking MMA seems to lack money, so they have to sell MIDI specification instead of
just allowing to download it) SMF files, google for SMF format description if you need to.
Only libvomid-specific details follow.

MThd
----

Format = 1
NumTracks = [number of tracks]
Division = 240

MTrk
----

No sysex or realtime events. All libvomid-specific data is contained in 'Proprietary Event'
metaevent (see below).

Voice Events
------------

* *Note On*
TODO: algorithm for getting actual note value

* *Note Off*
When we have two notes of the same channel and the same MIDI value placed on different tracks
one right after another, we can't be sure that first note's Note Off will be sent before
second note's Note On. So in this case first note's Note Off will be placed on the
*second* note's track, right before second note's Note On.

If there are several Note Offs for a Note On occuring at the same time (that means,
having only zero delta-times between them), the off velocity is taken from the *last* one,
not the first. That is just for simplicity of implementation (stack is used instead of queue).

* *Note Aftertouch*
Nothing special.

* *Controller*
No special purpose controllers (All Sound Off, Omni Modes, etc). No fine adjustments. No NRPNs.
The only RPN supported is Pitch Bend Range, that is set only if
there actually are bends bigger than +- 2 semitones. It can't be fine adjusted, too.

TODO: list of supported controllers

* *Program Change*

* *Channel Pressure*
Not supported.

* *Pitch Wheel*

Meta-Events
----------

* *Proprietary Event*
libvomid-specific data is contained in these metaevents. First 4 bytes must be: 'V', 'o', 'm', 1D.
Next byte determines info type. Currently the only type is 0 - Note Pitch. TODO: explanation
