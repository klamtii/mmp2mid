A Command-Line Tool to convert Projects saved with the Linux Multimedia Studio into MIDI-Files.

1. Usage
Syntax: mmp2mid [-v[+]] [-1track] <file.mmp>  [-o <outfile.mid>]

-v[+]   - be [very] verbose
-1track encode ase type 0 Midi-file (all channels in one track) instead of type 1 (one track per channel).

2. Licence
The programme is Free Software. You can redistribute it, and its source code.
No waranty or liabilty for any harm, use at your own risk.
If you use significant portions of the Code in own Projcts, it would be nice, if you give credit.

Files under /test/ subdirectory are example-songs for testing. The Music is GEMA-Free.


3. LMMS-Content Respected
-MIDI Tracks (using Instrument "SF2-Player")
-Automation Tracks changing the Volume, Panning and Pitch of MIDI-tracks
-Beat/Bassline Tracks containing the Above
-Low Frequency Oscillators (LFOs) changing Settings changeable by Automation tracks

4. Tipps
- If you want to convert a Non-MIDI INstrument to Midi in LMMS, drag the Instrument-Plugin over the Track with the Mouse. Then set the Patch/Bank of the Instrument.
- LMMS may incorrectly Imports Drum Tracks from Midi as Piano. However, if you set up Drums in LMMS (usually by a higher Bank), they will correctly written as Drums in MIDI, wich per convetion are stored in Channel 9 (0-based).

5. About
The Programme is written in C using only the standard libraries, and was compiled using MinGW (GCC for Windows).

Authors: Christian Klamt.
(c) 2024