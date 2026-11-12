#!/usr/bin/env python3
"""Factory presets for BrickPad's synth engines: writes src/presets.h. Run: python3 tools/mkpresets.py

Each preset: (name, category, "key=value ..."). Missing keys keep the engine's default (synth_defaults,
acid_defaults, cl_defaults/cx_defaults). Wave/type/mode values may be given by name (SAW, TUBE, PERC, 1/16...).
Values outside the engine's range stop the script (assert, never clamp).
Sound notes (from the engines): GRAND CUTOFF 45 ~ 425 Hz, 60 ~ 1.2 kHz, 80 ~ 4.6 kHz; ENV +-6 octaves;
times 25 = 10 ms, 50 = 100 ms, 75 = 1 s, 90 = 4 s. ACID CUTOFF 30 ~ 180 Hz, 50 ~ 540 Hz; DECAY 0..100 =
200 ms..2.2 s. CLOUD SIZE 55 ~ 170 ms, 80 ~ 460 ms; DENSITY 50 ~ 8/s, 80 ~ 26/s; ATTACK 66 = 0.4 s,
90 = 2 s; RELEASE 68 = 1.2 s, 90 = 4.4 s.
"""
import os

GRAND, ACID, CLOUD = 1, 2, 3

# ---- engine parameter tables: name, default, lo, hi (mirrors synth.h / acid.h / cloud.h) ----
GP = [("osc1", 1, 0, 3), ("osc2", 2, 0, 3), ("tune2", -12, -24, 24), ("detune", 6, 0, 50), ("mix", 40, 0, 100),
      ("noise", 0, 0, 100), ("cutoff", 45, 0, 100), ("reso", 25, 0, 100), ("env", 45, -100, 100),
      ("attack", 0, 0, 100), ("decay", 62, 0, 100), ("sustain", 50, 0, 100), ("release", 50, 0, 100),
      ("glide", 0, 0, 100), ("voice", 0, 0, 1)]
AP = [("wave", 0, 0, 1), ("tune", 0, -12, 12), ("cutoff", 30, 0, 100), ("reso", 70, 0, 100), ("envmod", 60, 0, 100),
      ("decay", 40, 0, 100), ("accent", 60, 0, 100), ("dtype", 0, 0, 3), ("drive", 0, 0, 100), ("tone", 70, 0, 100),
      ("mix", 100, 0, 100)]
CPX = [("pos", 30, 0, 100), ("spray", 25, 0, 100), ("size", 55, 0, 100), ("density", 50, 0, 100),
       ("shimmer", 20, 0, 100), ("motion", 30, 0, 100),
       ("sizernd", 0, 0, 100), ("window", 0, 0, 4), ("reverse", 0, 0, 100), ("pitch", 0, -24, 24),
       ("fine", 0, -50, 50), ("pitchrnd", 0, 0, 100), ("shmode", 0, 0, 2), ("sync", 0, 0, 4), ("timernd", 0, 0, 100),
       ("width", 0, 0, 100), ("scan", 0, 0, 2), ("freeze", 0, 0, 1), ("attack", 66, 0, 100), ("release", 68, 0, 100),
       ("tone", 0, -100, 100)]
TABLE = {GRAND: GP, ACID: AP, CLOUD: CPX}
# GRAND DEEP (Song.gx, GX_* in synth.h) and the PATCH cables (2026-11-10): a GRAND preset that names any of these
# carries its DEEP page at v[24..55] (v[24+31] = GX_INIT) and up to 8 cables "cable=OUT>IN" at v[56..71].
GXP = [("lrate", 50, 0, 100), ("lsync", 0, 0, 12), ("lwave", 0, 0, 4), ("lpitch", 0, 0, 100), ("lcut", 0, 0, 100),
       ("lpwm", 0, 0, 100), ("o1oct", 2, 0, 3), ("o2oct", 1, 0, 3), ("sync", 0, 0, 1), ("hp", 0, 0, 100), ("kbt", 1, 0, 2),
       ("vca", 0, 0, 2), ("spring", 0, 0, 100), ("arp", 0, 0, 1), ("amode", 0, 0, 3), ("aoct", 0, 0, 2),
       ("arate", 2, 0, 11), ("agate", 50, 10, 100), ("atten", 0, -100, 100)]
DIVS = ["1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/4D", "1/2", "1/1"]
GXSYM = {"lwave": ["TRI", "SQR", "SAW", "RAMP", "SH"], "vca": ["ENV", "GATE", "DRONE"], "lsync": ["FREE"] + DIVS,
         "arate": DIVS, "amode": ["UP", "DOWN", "ORDER", "UPDOWN"], "kbt": ["OFF", "HALF", "FULL"],
         "o1oct": ["32", "16", "8", "4"], "o2oct": ["16", "8", "4", "2"]}
JOUT = ["", "LFO", "SH", "OSC1", "OSC2", "NOISE", "MIX", "FILT", "ENV", "VCA", "KBP", "GATE", "VEL", "ATT"]
JIN = ["", "P1", "P2", "PWM", "FM", "M1", "M2", "MN", "FIN", "CUT", "RES", "VIN", "VAMT", "ATT", "LRATE"]
GX_INIT = 0x5F

SYMS = {
    GRAND: {"osc1": ["TRI", "SAW", "SQR", "PULSE"], "osc2": ["TRI", "SAW", "SQR", "PULSE"], "voice": ["MONO", "POLY"]},
    ACID: {"wave": ["SAW", "SQUARE"], "dtype": ["DIODE", "TRANSISTOR", "TUBE", "FUZZ"]},
    CLOUD: {"window": ["HANN", "TRIANGLE", "TUKEY", "PERC", "REVPERC"], "shmode": ["OCT5", "OCTAVES", "SCALE"],
            "sync": ["FREE", "1/4", "1/8", "1/16", "1/32"], "scan": ["FWD", "REV", "PINGPONG"], "freeze": ["OFF", "ON"]},
}

CATS = {
    GRAND: ["BASS", "SUB", "LEAD", "PAD", "KEYS", "PLUCK", "BRASS", "STRINGS", "PERC", "FX", "PATCH"],
    ACID: ["CLASSIC", "SQUELCH", "DIRTY", "DEEP", "SQUARE", "FX"],
    CLOUD: ["PAD", "TEXTURE", "SHIMMER", "RHYTHMIC", "DARK", "FREEZE", "GLITCH"],
}

# ---------------------------------------------------------------- GRAND
GRAND_P = [
    # BASS
    ("INIT", "BASS", ""),
    ("RUBBER LOW", "BASS", "osc2=SAW tune2=0 detune=8 mix=50 cutoff=30 reso=35 env=50 decay=55 sustain=20 release=30"),
    ("WOOD FUNK", "BASS", "osc1=SQR osc2=TRI cutoff=35 reso=20 env=60 decay=50 sustain=0 release=25"),
    ("PUNCH ROOT", "BASS", "osc2=PULSE cutoff=25 reso=15 env=70 decay=45 sustain=30 release=20"),
    ("ROUND MOTOR", "BASS", "osc1=TRI cutoff=40 reso=10 env=25 decay=60 sustain=70 release=30"),
    ("GROWL PIT", "BASS", "osc2=SAW tune2=7 detune=15 mix=45 noise=5 cutoff=28 reso=55 env=55 decay=58 sustain=35 release=30"),
    ("SLIDE WORM", "BASS", "cutoff=35 reso=45 env=40 sustain=60 release=35 glide=30"),
    ("ELASTIC", "BASS", "osc1=PULSE osc2=SAW cutoff=22 reso=60 env=75 decay=52 sustain=10 release=25"),
    ("GUT STRING", "BASS", "osc1=TRI osc2=SAW mix=25 detune=0 cutoff=30 reso=5 env=55 decay=55 sustain=0 release=40"),
    # SUB
    ("DEEP FLOOR", "SUB", "osc1=TRI osc2=TRI detune=0 mix=30 cutoff=25 reso=0 env=10 decay=60 sustain=90 release=35"),
    ("SINE WELL", "SUB", "osc1=TRI osc2=TRI tune2=0 detune=0 cutoff=20 reso=0 env=0 attack=10 sustain=100 release=40"),
    ("OCTAVE SUB", "SUB", "osc1=SQR osc2=TRI detune=0 mix=70 cutoff=22 reso=5 env=15 sustain=85 release=35"),
    ("PRESSURE", "SUB", "osc2=TRI mix=60 cutoff=18 reso=20 env=20 decay=70 sustain=80 release=45 glide=15"),
    ("REESE FLOOR", "SUB", "osc2=SAW tune2=0 detune=22 mix=50 cutoff=32 reso=15 env=10 sustain=100 release=45"),
    ("SUB DROP", "SUB", "osc1=TRI detune=0 mix=30 cutoff=20 reso=10 env=30 sustain=90 release=40 glide=45"),
    # LEAD
    ("SOLO GLIDE", "LEAD", "osc2=SAW tune2=0 detune=10 mix=50 cutoff=55 reso=30 env=35 attack=15 decay=65 sustain=70 release=45 glide=35"),
    ("SQUARE HERO", "LEAD", "osc1=SQR tune2=12 detune=5 mix=35 cutoff=62 reso=20 env=25 sustain=80 release=40 glide=20"),
    ("NEEDLE", "LEAD", "osc1=PULSE osc2=PULSE tune2=0 detune=12 mix=50 cutoff=70 reso=15 env=20 sustain=75 release=40 glide=15"),
    ("FIFTH RIDER", "LEAD", "osc2=SAW tune2=7 detune=4 mix=45 cutoff=58 reso=35 env=30 sustain=70 release=40 glide=25"),
    ("SCREAMER", "LEAD", "tune2=12 cutoff=50 reso=70 env=45 decay=66 sustain=60 release=40 glide=20"),
    ("WHISTLE TOP", "LEAD", "osc1=TRI osc2=TRI tune2=12 detune=3 mix=30 cutoff=65 reso=10 env=10 attack=25 sustain=90 release=50 glide=30"),
    ("HOLLOW RIDE", "LEAD", "osc1=SQR osc2=TRI tune2=0 detune=4 mix=40 cutoff=60 reso=45 env=-30 decay=68 sustain=80 release=40 glide=10"),
    ("LONG PORTA", "LEAD", "osc2=PULSE mix=30 cutoff=52 reso=40 env=30 sustain=75 release=55 glide=70"),
    # PAD (poly)
    ("SLOW TIDE", "PAD", "osc2=SAW tune2=0 detune=14 mix=50 cutoff=50 reso=15 env=25 attack=72 decay=78 sustain=70 release=75 voice=POLY"),
    ("GLASS FOG", "PAD", "osc1=TRI osc2=SAW tune2=12 detune=9 mix=30 cutoff=58 reso=25 env=15 attack=75 decay=80 sustain=80 release=80 voice=POLY"),
    ("WARM HALO", "PAD", "detune=18 mix=45 cutoff=42 reso=10 env=30 attack=68 decay=75 sustain=75 release=72 voice=POLY"),
    ("DARK CHOIR", "PAD", "osc2=SAW tune2=0 detune=25 mix=50 noise=8 cutoff=38 reso=30 env=20 attack=78 decay=80 sustain=85 release=82 voice=POLY"),
    ("SWELL VEIL", "PAD", "osc1=SQR tune2=7 detune=11 mix=50 cutoff=30 reso=35 env=60 attack=80 decay=85 sustain=40 release=78 voice=POLY"),
    ("NIGHT STRATA", "PAD", "osc1=PULSE osc2=SAW detune=20 mix=55 cutoff=46 reso=20 env=-20 attack=70 decay=80 sustain=90 release=85 voice=POLY"),
    ("AIR BREATH", "PAD", "osc1=TRI osc2=TRI tune2=12 detune=30 mix=40 noise=25 cutoff=62 reso=20 env=10 attack=74 decay=80 sustain=85 release=78 voice=POLY"),
    # KEYS (poly)
    ("FELT ROOM", "KEYS", "osc1=TRI tune2=12 detune=3 mix=20 cutoff=40 reso=5 env=40 attack=5 decay=72 sustain=25 release=55 voice=POLY"),
    ("TINE BELL", "KEYS", "osc1=TRI tune2=24 detune=2 mix=15 cutoff=48 reso=10 env=35 decay=70 sustain=20 release=55 voice=POLY"),
    ("ORGAN STOP", "KEYS", "osc1=SQR tune2=12 detune=2 mix=50 cutoff=60 reso=0 env=0 attack=5 decay=50 sustain=100 release=30 voice=POLY"),
    ("CLAV SNAP", "KEYS", "osc1=PULSE osc2=PULSE tune2=0 detune=3 mix=50 cutoff=45 reso=30 env=55 decay=52 sustain=15 release=30 voice=POLY"),
    ("VINTAGE COMBO", "KEYS", "tune2=12 cutoff=52 reso=15 env=20 attack=8 decay=65 sustain=60 release=45 voice=POLY"),
    ("SOFT CHORD", "KEYS", "osc2=SAW tune2=0 detune=12 mix=50 cutoff=44 reso=10 env=35 attack=20 decay=70 sustain=50 release=60 voice=POLY"),
    # PLUCK (poly)
    ("RAIN DROP", "PLUCK", "osc1=TRI tune2=12 detune=0 mix=30 cutoff=35 reso=40 env=70 decay=50 sustain=0 release=50 voice=POLY"),
    ("ZAP HARP", "PLUCK", "osc2=SAW tune2=12 mix=50 cutoff=30 reso=50 env=80 decay=48 sustain=0 release=52 voice=POLY"),
    ("THUMB PIANO", "PLUCK", "osc1=TRI osc2=TRI tune2=24 detune=0 mix=20 cutoff=40 reso=15 env=50 decay=52 sustain=0 release=45 voice=POLY"),
    ("PIZZ DOT", "PLUCK", "osc1=PULSE cutoff=32 reso=25 env=65 decay=45 sustain=0 release=35 voice=POLY"),
    ("BRIGHT PICK", "PLUCK", "osc2=PULSE tune2=0 detune=8 mix=45 cutoff=45 reso=20 env=60 decay=55 sustain=5 release=48 voice=POLY"),
    ("MALLET", "PLUCK", "osc1=TRI tune2=12 detune=0 mix=25 cutoff=38 reso=10 env=45 decay=58 sustain=0 release=50 voice=POLY"),
    ("WET KNOT", "PLUCK", "tune2=7 cutoff=25 reso=65 env=75 decay=46 sustain=0 release=40 voice=POLY"),
    # BRASS
    ("BRASS STAB", "BRASS", "osc2=SAW tune2=0 detune=9 mix=50 cutoff=40 reso=15 env=45 attack=30 decay=60 sustain=65 release=40 voice=POLY"),
    ("HORN SWELL", "BRASS", "osc2=SAW tune2=0 detune=7 mix=50 cutoff=35 reso=10 env=55 attack=55 decay=72 sustain=70 release=50 voice=POLY"),
    ("FANFARE", "BRASS", "tune2=12 detune=5 cutoff=48 reso=20 env=40 attack=25 decay=62 sustain=75 release=45 voice=POLY"),
    ("TUBA ROOT", "BRASS", "osc2=TRI mix=50 cutoff=30 reso=10 env=40 attack=35 decay=60 sustain=70 release=35 glide=10"),
    ("MUTED CORNET", "BRASS", "osc1=SQR osc2=SAW tune2=0 detune=4 cutoff=38 reso=25 env=30 attack=30 sustain=60 release=40 glide=12"),
    # STRINGS
    ("STRING HALL", "STRINGS", "osc2=SAW tune2=0 detune=16 mix=50 cutoff=55 reso=5 env=10 attack=66 decay=70 sustain=85 release=70 voice=POLY"),
    ("CELLO LINE", "STRINGS", "osc2=SAW tune2=0 detune=8 mix=50 cutoff=45 reso=10 env=15 attack=50 sustain=85 release=55 glide=25"),
    ("BOWED GLASS", "STRINGS", "osc2=TRI tune2=12 detune=10 cutoff=60 reso=15 env=10 attack=60 sustain=90 release=72 voice=POLY"),
    ("TAPE STRINGS", "STRINGS", "osc2=PULSE tune2=0 detune=28 mix=45 noise=3 cutoff=48 reso=10 env=0 attack=62 sustain=90 release=65 voice=POLY"),
    ("LOW ENSEMBLE", "STRINGS", "osc2=SAW detune=20 mix=50 cutoff=40 reso=5 env=10 attack=64 sustain=88 release=74 voice=POLY"),
    # PERC
    ("KICK THUMP", "PERC", "osc1=TRI osc2=TRI tune2=0 detune=0 cutoff=15 reso=30 env=90 decay=45 sustain=0 release=40"),
    ("SNARE HISS", "PERC", "osc1=SQR osc2=TRI tune2=12 mix=30 noise=70 cutoff=55 reso=10 env=40 decay=45 sustain=0 release=42 voice=POLY"),
    ("HAT TICK", "PERC", "osc1=PULSE osc2=PULSE tune2=7 noise=100 cutoff=88 reso=0 env=10 decay=32 sustain=0 release=30 voice=POLY"),
    ("TOM DROP", "PERC", "osc1=TRI tune2=0 detune=0 mix=20 cutoff=25 reso=50 env=70 decay=50 sustain=0 release=50 voice=POLY"),
    ("METAL BLIP", "PERC", "osc1=PULSE tune2=13 detune=30 mix=50 cutoff=60 reso=40 env=40 decay=38 sustain=0 release=38 voice=POLY"),
    ("CLAP NOISE", "PERC", "osc1=SQR tune2=0 noise=90 cutoff=62 reso=30 env=30 decay=42 sustain=0 release=46 voice=POLY"),
    # FX
    ("WIND TUNNEL", "FX", "osc1=TRI osc2=TRI mix=0 noise=100 cutoff=45 reso=75 env=30 attack=80 decay=85 sustain=50 release=85 voice=POLY"),
    ("RISER", "FX", "osc2=SAW tune2=7 detune=40 mix=50 noise=30 cutoff=20 reso=60 env=90 attack=88 decay=90 sustain=100 release=60"),
    ("LASER GLIDE", "FX", "osc1=SQR osc2=SAW tune2=24 cutoff=55 reso=75 env=40 decay=55 sustain=30 release=40 glide=60"),
    ("ALARM PULSE", "FX", "osc1=PULSE osc2=PULSE tune2=1 detune=0 mix=50 cutoff=70 reso=10 env=0 sustain=100 release=20"),
    ("STORM", "FX", "osc2=SAW tune2=-24 detune=50 mix=50 noise=60 cutoff=30 reso=55 env=60 attack=70 decay=85 sustain=30 release=88 voice=POLY"),
    ("OCEAN HUSH", "FX", "osc1=TRI osc2=TRI mix=0 noise=80 cutoff=35 reso=40 env=40 attack=85 decay=88 sustain=20 release=90 voice=POLY"),
    ("ROBOT BURBLE", "FX", "osc1=PULSE tune2=-5 detune=33 mix=50 cutoff=40 reso=85 env=60 decay=40 sustain=20 release=30"),
    # PATCH (2026-11-10): sounds that need GRAND DEEP and the cables of the PATCH page
    ("FM BELL", "PATCH", "osc1=TRI osc2=TRI tune2=0 detune=0 mix=100 cutoff=85 reso=0 env=10 decay=72 sustain=0 release=68 voice=POLY o1oct=4 atten=55 cable=OSC1>ATT cable=ATT>FM"),
    ("FM GROWL", "PATCH", "osc1=SAW osc2=TRI tune2=0 detune=0 mix=100 cutoff=55 reso=20 env=40 decay=55 sustain=40 release=30 o1oct=16 atten=80 cable=OSC1>ATT cable=ATT>FM"),
    ("SYNC SWEEP", "PATCH", "osc1=SAW osc2=SAW tune2=7 detune=0 mix=100 cutoff=75 reso=10 env=20 decay=66 sustain=30 release=40 sync=1 atten=100 cable=ENV>ATT cable=ATT>P2"),
    ("ENV ZAP", "PATCH", "osc1=TRI osc2=TRI tune2=0 detune=0 mix=30 cutoff=60 reso=10 env=40 decay=38 sustain=0 release=30 atten=100 cable=ENV>ATT cable=ATT>P1 cable=ATT>P2"),
    ("LASER DROP", "PATCH", "osc1=SQR osc2=SAW tune2=12 mix=50 cutoff=80 reso=30 env=30 decay=45 sustain=0 release=35 o1oct=4 o2oct=4 atten=100 cable=ENV>ATT cable=ATT>P1 cable=ATT>P2 cable=ENV>RES"),
    ("VIBRATO LEAD", "PATCH", "osc1=SAW osc2=SQR tune2=0 detune=10 mix=45 cutoff=62 reso=25 env=30 sustain=70 release=40 glide=20 lrate=68 atten=4 cable=LFO>ATT cable=ATT>P1 cable=ATT>P2"),
    ("VELO FILTER", "PATCH", "osc1=SAW osc2=PULSE tune2=0 detune=8 mix=50 cutoff=22 reso=30 env=15 decay=55 sustain=50 release=40 voice=POLY cable=VEL>CUT"),
    ("RANDOM STEPS", "PATCH", "osc1=SQR osc2=SAW tune2=0 detune=6 mix=40 cutoff=40 reso=62 env=10 sustain=80 release=35 lsync=1/16 atten=35 cable=SH>ATT cable=ATT>CUT"),
    ("FILTER FM", "PATCH", "osc1=SAW osc2=SAW tune2=-12 detune=0 mix=10 cutoff=38 reso=55 env=45 decay=58 sustain=30 release=30 atten=30 cable=OSC2>ATT cable=ATT>CUT"),
    ("NOISE GRIT", "PATCH", "osc1=SQR osc2=TRI cutoff=34 reso=45 env=50 decay=52 sustain=20 release=28 atten=22 cable=NOISE>ATT cable=ATT>CUT"),
    ("FEEDBACK", "PATCH", "osc1=SAW osc2=SQR tune2=-12 mix=45 noise=35 cutoff=42 reso=40 env=40 decay=60 sustain=40 release=35 cable=VCA>MN"),
    ("DRUNK WAH", "PATCH", "osc1=SAW osc2=SAW tune2=0 detune=12 mix=50 cutoff=42 reso=50 env=0 sustain=90 release=40 lrate=42 lcut=35 atten=40 cable=LFO>ATT cable=ATT>LRATE"),
    ("WOBBLE BASS", "PATCH", "osc1=SAW osc2=SQR cutoff=30 reso=45 env=10 sustain=90 release=25 lsync=1/8 lcut=45"),
    ("PWM STRINGS", "PATCH", "osc1=PULSE osc2=PULSE tune2=0 detune=12 mix=50 cutoff=62 reso=5 env=10 attack=55 sustain=85 release=70 voice=POLY lrate=45 lpwm=60 hp=20"),
    ("SPRING PLUCK", "PATCH", "osc1=SAW osc2=TRI tune2=12 mix=35 cutoff=55 reso=15 env=45 decay=42 sustain=0 release=45 voice=POLY spring=45"),
    ("ARP GLASS", "PATCH", "osc1=TRI osc2=SQR tune2=12 mix=30 cutoff=68 reso=20 env=25 decay=45 sustain=0 release=50 voice=POLY arp=1 amode=UPDOWN aoct=1 arate=1/16 agate=50 spring=25"),
    ("DRONE ORGAN", "PATCH", "osc1=SQR osc2=TRI tune2=12 detune=4 mix=40 cutoff=50 reso=10 env=0 sustain=100 release=60 voice=POLY vca=DRONE lsync=1/1 lcut=15 spring=20"),
    ("THIN PHONE", "PATCH", "osc1=SAW osc2=SQR tune2=0 detune=8 mix=50 cutoff=58 reso=20 env=20 sustain=60 release=30 hp=55 kbt=FULL"),
]

# ---------------------------------------------------------------- ACID
ACID_P = [
    # CLASSIC
    ("INIT", "CLASSIC", ""),
    ("FIRST LIGHT", "CLASSIC", "cutoff=35 reso=60 envmod=50 decay=30 accent=50"),
    ("SLOW BURN", "CLASSIC", "cutoff=20 reso=72 envmod=70 decay=70 accent=70"),
    ("RUBBER TUBE", "CLASSIC", "cutoff=28 reso=68 envmod=60 decay=45 dtype=TUBE drive=15 tone=60 mix=60"),
    ("WAREHOUSE", "CLASSIC", "cutoff=32 reso=75 envmod=65 decay=40 accent=80 drive=25 tone=65 mix=70"),
    ("SUNRISE 88", "CLASSIC", "cutoff=40 reso=55 envmod=45 decay=25 accent=55"),
    ("ROLLER", "CLASSIC", "cutoff=25 reso=62 envmod=75 decay=20 accent=65"),
    ("TWIN SPIRAL", "CLASSIC", "cutoff=22 reso=78 envmod=80 decay=55 accent=75"),
    ("BACKROOM", "CLASSIC", "cutoff=30 reso=66 envmod=55 decay=60 dtype=TRANSISTOR drive=10 mix=50"),
    ("LOW CRAWL", "CLASSIC", "tune=-12 cutoff=35 reso=60 envmod=50 decay=50"),
    ("HIGH WIRE", "CLASSIC", "tune=12 cutoff=25 reso=65 envmod=55 decay=30"),
    ("PLASTIC", "CLASSIC", "cutoff=38 reso=58 envmod=40 decay=35 accent=40 dtype=TUBE drive=20 tone=75 mix=50"),
    # SQUELCH
    ("WET SQUELCH", "SQUELCH", "cutoff=15 reso=88 envmod=85 decay=50 accent=80"),
    ("GOO", "SQUELCH", "cutoff=10 reso=92 envmod=90 decay=65 accent=85 tone=60"),
    ("BUBBLER", "SQUELCH", "cutoff=20 reso=90 envmod=70 decay=15 accent=70"),
    ("SPRING COIL", "SQUELCH", "cutoff=25 reso=85 envmod=60 decay=25 accent=90"),
    ("SCREAM VALVE", "SQUELCH", "cutoff=18 reso=95 envmod=80 decay=40 accent=75 dtype=TUBE drive=20 tone=55 mix=60"),
    ("LIQUID", "SQUELCH", "cutoff=12 reso=86 envmod=95 decay=80 accent=70"),
    ("CHIRP", "SQUELCH", "cutoff=30 reso=90 envmod=50 decay=5 accent=100"),
    ("WOBBLE MOUTH", "SQUELCH", "cutoff=8 reso=94 envmod=100 decay=90 accent=60"),
    ("ACID RAIN", "SQUELCH", "cutoff=22 reso=89 envmod=75 decay=35 accent=85 drive=15 mix=50"),
    ("SLURP", "SQUELCH", "cutoff=5 reso=84 envmod=100 decay=55 accent=95"),
    # DIRTY (every distortion type)
    ("DIODE BITE", "DIRTY", "cutoff=30 reso=65 envmod=60 decay=40 dtype=DIODE drive=55 tone=70"),
    ("CHROME TRANS", "DIRTY", "cutoff=28 reso=60 envmod=55 decay=40 dtype=TRANSISTOR drive=60 tone=65"),
    ("HOT TUBE", "DIRTY", "cutoff=32 reso=62 envmod=60 decay=45 dtype=TUBE drive=65 tone=60"),
    ("FUZZ BOMB", "DIRTY", "cutoff=30 reso=55 envmod=55 decay=35 dtype=FUZZ drive=50 tone=55"),
    ("RUST", "DIRTY", "cutoff=25 reso=50 envmod=60 decay=40 dtype=DIODE drive=80 tone=45"),
    ("GRINDER", "DIRTY", "cutoff=35 reso=45 envmod=50 decay=30 dtype=TRANSISTOR drive=85 tone=75 mix=90"),
    ("AMP STACK", "DIRTY", "cutoff=28 reso=55 envmod=65 decay=50 dtype=TUBE drive=90 tone=50"),
    ("WASP NEST", "DIRTY", "cutoff=40 reso=50 envmod=45 decay=25 dtype=FUZZ drive=75 tone=80 mix=80"),
    ("GRAVEL", "DIRTY", "wave=SQUARE cutoff=25 reso=58 envmod=60 decay=45 dtype=FUZZ drive=40 tone=40"),
    ("BLOWN SPEAKER", "DIRTY", "cutoff=20 reso=40 envmod=55 decay=35 dtype=TRANSISTOR drive=100 tone=35 mix=70"),
    ("SMOKE", "DIRTY", "cutoff=22 reso=68 envmod=50 decay=60 dtype=TUBE drive=40 tone=40 mix=70"),
    ("PARALLEL CRUSH", "DIRTY", "cutoff=30 reso=70 envmod=60 decay=40 dtype=DIODE drive=95 tone=85 mix=40"),
    # DEEP
    ("DEEP WATER", "DEEP", "cutoff=15 reso=50 envmod=30 decay=70 accent=40 tone=50"),
    ("SUBMARINE", "DEEP", "tune=-12 cutoff=20 reso=45 envmod=25 decay=60 accent=30"),
    ("DUB MOSS", "DEEP", "cutoff=18 reso=60 envmod=35 decay=85 accent=50 dtype=TUBE drive=10 tone=40 mix=50"),
    ("CAVE PULSE", "DEEP", "wave=SQUARE cutoff=12 reso=55 envmod=40 decay=50 accent=45"),
    ("MIDNIGHT", "DEEP", "cutoff=10 reso=65 envmod=45 decay=75 accent=55"),
    ("VELVET", "DEEP", "cutoff=25 reso=30 envmod=20 decay=90 accent=30 dtype=TUBE drive=20 tone=35 mix=60"),
    ("UNDERTOW", "DEEP", "cutoff=5 reso=70 envmod=55 decay=100 accent=50"),
    ("LOW FOG", "DEEP", "tune=-12 cutoff=8 reso=40 envmod=35 decay=80 accent=35 drive=15 tone=30 mix=40"),
    # SQUARE
    ("SQUARE ONE", "SQUARE", "wave=SQUARE"),
    ("HOLLOW BOX", "SQUARE", "wave=SQUARE cutoff=35 reso=60 envmod=45 decay=30 accent=50"),
    ("REED BUZZ", "SQUARE", "wave=SQUARE cutoff=40 reso=75 envmod=55 decay=35 dtype=TRANSISTOR drive=20 mix=50"),
    ("WOOD PIPE", "SQUARE", "wave=SQUARE cutoff=45 reso=50 envmod=35 decay=50 accent=40"),
    ("SQUARE SQUELCH", "SQUARE", "wave=SQUARE cutoff=15 reso=88 envmod=80 decay=45 accent=80"),
    ("PIXEL BASS", "SQUARE", "wave=SQUARE cutoff=50 reso=40 envmod=30 decay=20 dtype=FUZZ drive=25 tone=60 mix=40"),
    ("BOXER", "SQUARE", "wave=SQUARE cutoff=28 reso=65 envmod=70 decay=25 accent=90 drive=35 tone=65 mix=80"),
    ("CHEW TOY", "SQUARE", "wave=SQUARE cutoff=20 reso=80 envmod=75 decay=60 accent=70 dtype=TUBE drive=30 tone=55 mix=70"),
    # FX
    ("SIREN SWEEP", "FX", "cutoff=0 reso=96 envmod=100 decay=100 accent=100 tone=60"),
    ("ZAP GUN", "FX", "tune=12 cutoff=10 reso=92 envmod=100 decay=0 accent=100"),
    ("RADIO STATIC", "FX", "cutoff=45 reso=30 envmod=40 decay=10 dtype=FUZZ drive=100 tone=30"),
    ("WHALE CALL", "FX", "tune=-12 cutoff=0 reso=95 envmod=80 decay=100 accent=40 dtype=TUBE drive=20 tone=40 mix=50"),
    ("LASER WHIP", "FX", "cutoff=40 reso=90 envmod=100 decay=5 accent=80 dtype=TRANSISTOR drive=30 tone=90 mix=60"),
    ("METAL DRONE", "FX", "wave=SQUARE cutoff=60 reso=85 envmod=10 decay=100 accent=20 drive=45 tone=50 mix=70"),
    ("SPARK PLUG", "FX", "wave=SQUARE tune=12 cutoff=55 reso=70 envmod=60 decay=0 accent=100 dtype=FUZZ drive=60 tone=85 mix=60"),
    ("GHOST HOWL", "FX", "cutoff=35 reso=98 envmod=20 decay=100 accent=0 tone=50"),
]

# ---------------------------------------------------------------- CLOUD (shapes the pad's own sample)
CLOUD_P = [
    # PAD
    ("INIT", "PAD", ""),
    ("SOFT BLANKET", "PAD", "spray=30 size=80 density=70 shimmer=15 motion=15 attack=80 release=85 tone=-20"),
    ("WIDE SKY", "PAD", "spray=50 size=85 density=65 shimmer=30 motion=10 width=100 attack=85 release=88"),
    ("SLOW BLOOM", "PAD", "size=90 density=60 motion=5 attack=95 release=92"),
    ("WARM MIST", "PAD", "window=TRIANGLE size=75 density=70 tone=-40 attack=75 release=80"),
    ("STILL LAKE", "PAD", "window=TUKEY spray=10 size=90 density=55 motion=0 release=80"),
    ("DRIFT CHORUS", "PAD", "spray=35 size=70 density=75 pitchrnd=15 width=80"),
    ("OCTAVE BED", "PAD", "pitch=-12 shimmer=40 shmode=OCTAVES size=80 density=60 tone=-15 attack=78 release=82"),
    ("FIFTH HAZE", "PAD", "pitch=7 shimmer=25 size=75 density=65 attack=75"),
    ("TAPE WARMTH", "PAD", "fine=-15 pitchrnd=8 sizernd=30 size=80 density=60 tone=-50"),
    # TEXTURE
    ("SAND GRAIN", "TEXTURE", "spray=60 size=15 density=85 motion=20 sizernd=50 timernd=80"),
    ("BUZZ SWARM", "TEXTURE", "spray=80 size=8 density=100 pitchrnd=30 width=100"),
    ("GRAVEL ROAD", "TEXTURE", "window=PERC spray=100 size=25 density=75 sizernd=60 timernd=70"),
    ("FABRIC", "TEXTURE", "window=TRIANGLE spray=40 size=40 density=70 motion=40 sizernd=40"),
    ("HIVE", "TEXTURE", "spray=50 size=12 density=92 pitchrnd=50 tone=20"),
    ("PAPER TEAR", "TEXTURE", "window=TUKEY reverse=50 spray=70 size=20 density=80 timernd=100"),
    ("MOSS", "TEXTURE", "spray=45 size=35 density=60 motion=15 sizernd=70 tone=-55"),
    ("SCATTER FIELD", "TEXTURE", "spray=100 size=45 density=55 sizernd=80 timernd=90 width=100"),
    # SHIMMER
    ("CRYSTAL RISE", "SHIMMER", "shimmer=70 shmode=OCT5 size=75 density=70 tone=30 attack=85"),
    ("ANGEL OCTAVE", "SHIMMER", "pitch=12 shimmer=60 shmode=OCTAVES size=70 density=60 tone=20"),
    ("SCALE SPARKLE", "SHIMMER", "shimmer=80 shmode=SCALE size=40 density=75 spray=40"),
    ("GLASS CHOIR", "SHIMMER", "window=TUKEY shimmer=50 size=85 density=60 width=80"),
    ("HIGH STARS", "SHIMMER", "window=TRIANGLE pitch=24 shimmer=40 shmode=OCTAVES spray=60 size=30 density=60 tone=40"),
    ("PRISM", "SHIMMER", "shimmer=100 shmode=SCALE pitchrnd=10 size=55 density=70 width=100"),
    ("FROST HALO", "SHIMMER", "pitch=12 shimmer=65 size=70 density=55 tone=50 release=90"),
    ("CHIME DUST", "SHIMMER", "window=PERC shimmer=75 shmode=SCALE spray=70 size=35 density=50 timernd=60"),
    # RHYTHMIC (tempo grid)
    ("QUARTER PULSE", "RHYTHMIC", "sync=1/4 window=PERC size=50 density=20 attack=10 release=30"),
    ("EIGHTH CHOP", "RHYTHMIC", "sync=1/8 window=TUKEY spray=20 size=30 density=30 attack=10 release=30"),
    ("SIXTEENTH RAIN", "RHYTHMIC", "sync=1/16 window=PERC spray=50 size=20 density=40 attack=5 release=40"),
    ("STUTTER 32", "RHYTHMIC", "sync=1/32 window=PERC spray=10 size=12 density=50 motion=10 attack=5 release=25"),
    ("SCALE STEPS", "RHYTHMIC", "sync=1/16 window=PERC shimmer=90 shmode=SCALE size=25 density=35 attack=10 release=40"),
    ("SWING DUST", "RHYTHMIC", "sync=1/16 window=PERC spray=60 size=18 density=40 timernd=60 attack=10 release=40"),
    ("REVERSE PUMP", "RHYTHMIC", "sync=1/4 window=REVPERC size=70 density=20 attack=10 release=35"),
    ("BURST GRID", "RHYTHMIC", "sync=1/8 window=PERC spray=40 size=15 density=70 attack=5 release=30"),
    # DARK
    ("ABYSS", "DARK", "pitch=-24 size=85 density=55 motion=8 tone=-60 attack=85 release=90"),
    ("UNDERGROUND", "DARK", "pitch=-12 spray=40 size=70 density=60 tone=-45"),
    ("COAL SMOKE", "DARK", "window=TRIANGLE size=60 density=65 sizernd=40 pitchrnd=20 tone=-70"),
    ("BLACK TIDE", "DARK", "pitch=-12 scan=PINGPONG motion=40 size=80 density=55 tone=-50"),
    ("RUMBLE CAVE", "DARK", "pitch=-24 spray=70 size=40 density=80 tone=-80"),
    ("DREAD SWELL", "DARK", "pitch=-5 fine=-20 shimmer=30 shmode=OCTAVES size=90 density=60 tone=-40 attack=100 release=95"),
    ("HAUNTED REEL", "DARK", "reverse=60 pitch=-7 scan=REV motion=25 size=65 density=50 tone=-55"),
    ("NIGHT ENGINE", "DARK", "pitch=-12 sync=1/16 window=PERC size=30 density=45 tone=-50 attack=10 release=40"),
    # FREEZE
    ("FROZEN MOMENT", "FREEZE", "freeze=ON spray=5 size=80 density=70 motion=0"),
    ("ICE SHEET", "FREEZE", "freeze=ON window=TUKEY spray=2 size=95 density=50 tone=15"),
    ("HOLD BREATH", "FREEZE", "freeze=ON spray=15 size=60 density=75 pitchrnd=5 attack=50 release=75"),
    ("STILL SHIMMER", "FREEZE", "freeze=ON shimmer=50 shmode=OCTAVES size=75 density=65"),
    ("AMBER", "FREEZE", "freeze=ON window=TRIANGLE fine=-8 size=85 density=60 tone=-35"),
    ("SUSPENDED", "FREEZE", "freeze=ON spray=40 width=100 size=70 density=70 sizernd=30"),
    ("TIME STOP", "FREEZE", "freeze=ON window=TUKEY spray=0 size=35 density=90 attack=20 release=40"),
    # GLITCH
    ("BROKEN TAPE", "GLITCH", "reverse=50 pitchrnd=60 spray=80 size=25 density=60 timernd=100 sizernd=100"),
    ("BUFFER SKIP", "GLITCH", "sync=1/32 window=TUKEY spray=100 size=10 density=60 motion=70 attack=5 release=30"),
    ("BACKWARDS", "GLITCH", "reverse=100 window=REVPERC scan=REV motion=40 size=60 density=40"),
    ("BIT SCRAMBLE", "GLITCH", "spray=100 size=0 density=100 pitchrnd=100 width=100 timernd=100"),
    ("PING PONG SCAN", "GLITCH", "window=PERC scan=PINGPONG motion=90 size=30 density=60"),
    ("WARBLE", "GLITCH", "fine=25 pitchrnd=40 size=45 density=55 sizernd=60 tone=20"),
    ("STROBE", "GLITCH", "sync=1/32 window=PERC pitch=12 spray=60 size=5 density=30 attack=5 release=25"),
    ("CHAOS WHEEL", "GLITCH", "shimmer=100 shmode=SCALE pitchrnd=70 reverse=30 timernd=80 spray=90 motion=80"),
]


def values(engine, spec, name):
    tab = TABLE[engine]
    idx = {k: i for i, (k, _, _, _) in enumerate(tab)}
    v = [d for (_, d, _, _) in tab]
    seen = set()
    gidx = {k: i for i, (k, _, _, _) in enumerate(GXP)}
    gx = [d for (_, d, _, _) in GXP]; cables = []; deep = False
    for item in spec.split():
        k, s = item.split("=")
        if engine == GRAND and k == "cable":
            o, i = s.split(">"); assert o in JOUT[1:] and i in JIN[1:], f"{name}: bad cable {s}"
            cables.append((JOUT.index(o), JIN.index(i))); deep = True; continue
        if engine == GRAND and k in gidx:
            assert k not in seen, f"{name}: {k} given twice"
            seen.add(k); syms = GXSYM.get(k)
            val = syms.index(s) if syms and s in syms else int(s)
            assert GXP[gidx[k]][2] <= val <= GXP[gidx[k]][3], f"{name}: {k}={val} out of range"
            gx[gidx[k]] = val; deep = True; continue
        assert k in idx, f"{name}: unknown parameter {k}"
        assert k not in seen, f"{name}: {k} given twice"
        seen.add(k)
        syms = SYMS[engine].get(k)
        val = syms.index(s) if syms and s in syms else int(s)
        lo, hi = tab[idx[k]][2], tab[idx[k]][3]
        assert lo <= val <= hi, f"{name}: {k}={val} outside {lo}..{hi}"
        v[idx[k]] = val
    if deep:
        assert len(cables) <= 8, f"{name}: more than 8 cables"
        v += [0] * (24 - len(v)); g = gx + [0] * (32 - len(gx)); g[31] = GX_INIT
        v += g
        for o, i in cables: v += [o, i]
    return v


def check_name(n):
    assert n == n.upper() and len(n) <= 14 and n.strip() == n, f"bad name {n!r}"
    assert all(c.isascii() and (c.isalnum() or c in " -") for c in n), f"bad name {n!r}"


def main():
    cats = []
    for e in (GRAND, ACID, CLOUD):
        for c in CATS[e]:
            if c not in cats:
                cats.append(c)
    rows = []
    for e, lst in ((GRAND, GRAND_P), (ACID, ACID_P), (CLOUD, CLOUD_P)):
        assert len(lst) >= 55, f"engine {e}: only {len(lst)} presets"
        assert lst[0][0] == "INIT" and lst[0][2] == "", "first preset must be INIT"
        names = set(); order = -1
        for name, cat, spec in lst:
            check_name(name)
            assert name not in names, f"duplicate name {name}"
            names.add(name)
            assert cat in CATS[e], f"{name}: category {cat} not for this engine"
            co = CATS[e].index(cat)
            assert co >= order, f"{name}: out of category order"
            order = co
            v = values(e, spec, name)
            assert len(v) <= 72
            rows.append((name, e, cats.index(cat), v))
    out = ["#ifndef BP_PRESETS_H", "#define BP_PRESETS_H", "#include <stdint.h>",
           "// Factory presets (generated by tools/mkpresets.py: edit that, then run it).",
           "// v[]: GRAND sp[0..13] then v[14] = voice (0 MONO, 1 POLY); ACID ap[0..10]; CLOUD cp[0..5] then cx[0..14] at v[6..20].",
           "#define PRESET_N 72   // v[0..23] the panel; v[24..55] the DEEP page (gx / ax, with its MARK), v[56..71] GRAND cables (2026-11-10)",
           "typedef struct { const char *name; uint8_t engine, cat; int8_t v[PRESET_N]; } Preset;   // engine 1 GRAND 2 ACID 3 CLOUD (= Track.type)",
           "static const char *const preset_cat[] = { " + ", ".join(f'"{c}"' for c in cats) + " };   // category names, all engines share this list",
           f"#define NPRESET_CAT {len(cats)}",
           "static const Preset presets[] = {"]
    for name, e, c, v in rows:
        out.append(f'    {{ "{name}", {e}, {c}, {{ ' + ", ".join(str(x) for x in v) + " } },")
    out += ["};", "#define NPRESET (int)(sizeof presets / sizeof presets[0])", "#endif", ""]
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "presets.h")
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(out))
    # summary
    for e, label in ((GRAND, "GRAND"), (ACID, "ACID"), (CLOUD, "CLOUD")):
        er = [r for r in rows if r[1] == e]
        per = ", ".join(f"{c} {sum(1 for r in er if cats[r[2]] == c)}" for c in CATS[e])
        print(f"{label}: {len(er)}  ({per})")
    print(f"total {len(rows)}, categories {len(cats)} -> {os.path.normpath(path)}")


if __name__ == "__main__":
    main()
