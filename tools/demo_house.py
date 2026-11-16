# The house demo, played on the buttons like a person would (UI v2). Prints the key script for play.sh.
# usage: python3 -I house.py [PARTS...]  (parts: song kick clap ...; none = all). ! marks a screenshot.
import sys
ROWS = ["ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-_. "]
def name(word, clear=20):   # the name keyboard: clear, then walk to each letter from the top left, PLAY = OK
    out = ["OPT*%d" % clear, "UP*4"]; r, c = 0, 7   # the keyboard opens on OK (bottom row, column 7)
    for ch in word.upper():
        for rr, row in enumerate(ROWS):
            if ch in row: tr, tc = rr, row.index(ch)
        if tr > r: out.append("DOWN*%d" % (tr - r))
        if tr < r: out.append("UP*%d" % (r - tr))
        if tc > c: out.append("RIGHT*%d" % (tc - c))
        if tc < c: out.append("LEFT*%d" % (c - tc))
        out.append("EDIT"); r, c = tr, tc
    out.append("PLAY")
    return " ".join(out)
def steps(on):              # STEPS: the cursor starts on step 1, puts a note on each step listed
    out, at = [], 1
    for s in on:
        d = s - at
        if d > 0: out.append("RIGHT*%d" % d)
        if d < 0: out.append("LEFT*%d" % -d)
        out.append("EDIT"); at = s
    return " ".join(out), at
def goto(at, s):
    d = s - at
    return ("RIGHT*%d" % d if d > 0 else "LEFT*%d" % -d if d < 0 else ""), s
P = {}
# 1. the song: NEW PROJECT, 124 BPM, swing 8 %, F minor
P["song"] = "F1 DOWN*3 EDIT DOWN*4 EDIT EDIT w3 Y UP*3 EDIT EDIT+RIGHT*4 DOWN EDIT+RIGHT*8 DOWN EDIT+RIGHT*5 DOWN EDIT+RIGHT*3 Y Y"
# 2. KICK: SAMPLER 808_01, four on the floor, its name
s, at = steps([1, 5, 9, 13])
P["kick"] = "EDIT EDIT EDIT EDIT EDIT EDIT " + s + " Y Y Y SHIFT+X " + name("KICK") + " !"
# 3. CLAP on 2 and 4, sent to the reverb (MIX: SEND A)
s, at = steps([5, 13])
P["clap"] = "RIGHT EDIT EDIT DOWN*3 EDIT EDIT EDIT " + s + " Y Y SHIFT+LEFT UP*3 LEFT DOWN EDIT+UP*6 Y Y SHIFT+X " + name("CLAP") + " !"
# 4. HATS: sixteenths, some by chance, louder offbeats, a double hit at the end
s, at = steps(range(1, 17))
h = [s, "R1 R1"]
for st in (2, 6, 10, 14): m, at = goto(at, st); h += [m, "EDIT+DOWN*3"]
h.append("L1")
for st in (3, 7, 11, 15): m, at = goto(at, st); h += [m, "EDIT+UP*2"]
for st in (2, 4, 6, 8, 10, 12, 14, 16): m, at = goto(at, st); h += [m, "EDIT+DOWN*2"]
h.append("R1 R1"); m, at = goto(at, 16); h += [m, "EDIT+RIGHT !"]
P["hats"] = "RIGHT EDIT EDIT DOWN*5 EDIT EDIT EDIT " + " ".join(h) + " Y Y Y SHIFT+X " + name("HATS")
# 5. OPEN HAT on the offbeats
s, at = steps([3, 7, 11, 15])
P["ohat"] = "RIGHT EDIT EDIT DOWN*6 EDIT EDIT EDIT " + s + " Y Y Y SHIFT+X " + name("OPEN HAT") + " !"
# 6. BASS: ACID on the offbeats, accents, a slide; its filter and drive; an LFO on CUTOFF; ducked by the kick
s, at = steps([3, 7, 8, 11, 15, 16])
b = [s]
m, at = goto(at, 8); b += [m, "EDIT+UP"]
m, at = goto(at, 16); b += [m, "EDIT+DOWN"]
b.append("R1*4")                            # ACC
for st in (3, 11): m, at = goto(at, st); b += [m, "EDIT"]
b.append("R1")                              # SLIDE
m, at = goto(at, 7); b += [m, "EDIT !"]
b.append("Y Y SHIFT+RIGHT R1 EDIT+LEFT*8 RIGHT EDIT+UP*2 LEFT DOWN EDIT+UP*2 ! R1 RIGHT EDIT+UP*3 !")   # FILTER, DIST
b.append("Y DOWN*2 EDIT EDIT EDIT+RIGHT*8 DOWN RIGHT EDIT+LEFT*20 !")   # MOD > LFO 1: CUTOFF, depth
b.append("Y DOWN*2 EDIT RIGHT EDIT+RIGHT LEFT DOWN EDIT+UP ! Y Y")    # MOD > DUCK: by pad 1, 60 %
P["bass"] = "DOWN LEFT*3 EDIT DOWN*2 EDIT EDIT EDIT " + " ".join(b) + " Y SHIFT+X " + name("BASS")
# 7. CHORDS: GRAND, the preset SOFT CHORD, the pattern as a piano roll with triads on the offbeats, CHORUS, the sends,
#    an automation of CUTOFF with a bend, a PATCH cable LFO -> PWM
c = ["RIGHT EDIT DOWN EDIT",                                   # an empty pad: GRAND
     "SHIFT+RIGHT UP EDIT R1 DOWN*35 ! EDIT",                  # SOUND > the picture: PRESETS, FACTORY, SOFT CHORD
     "Y SHIFT+UP Y DOWN EDIT DOWN*2 RIGHT*2 EDIT+RIGHT ! Y UP EDIT",   # PATTERN > SELECT > MODE: ROLL; then ROLL
     "RIGHT*2 X+EDIT RIGHT*4 X+EDIT DOWN*2 RIGHT*4 X+EDIT RIGHT*4 X+EDIT !",
     "Y Y SHIFT+DOWN EDIT*3 !",                                # FX: insert 1 = CHORUS
     "Y SHIFT+LEFT UP*3 LEFT DOWN RIGHT EDIT+UP*4 LEFT EDIT+UP*3 !",     # MIX (the cursor is kept: to the top first): SEND B, SEND A
     "Y UP EDIT DOWN*2 EDIT UP EDIT+RIGHT*12 DOWN EDIT EDIT+DOWN*3 RIGHT*8 EDIT EDIT+UP*6 LEFT*4 X+RIGHT*4 !",   # AUTOMATION A1: CUTOFF
     "Y Y SHIFT+RIGHT R1*3 EDIT RIGHT DOWN*2 EDIT !",        # SOUND > PATCH: LFO -> PWM
     "Y Y SHIFT+X " + name("CHORDS")]
P["chords"] = " ".join(c)
# 8. ATMOS: CLOUD on a cinematic loop, one long note, the grains spread; frozen to save the processor
P["atmos"] = "RIGHT EDIT DOWN*3 EDIT LEFT DOWN EDIT EDIT EDIT EDIT EDIT EDIT EDIT Y Y SHIFT+RIGHT RIGHT EDIT+UP*2 DOWN EDIT+UP*2 ! Y SHIFT+LEFT DOWN EDIT ! Y Y EDIT+LEFT*6 SHIFT+X " + name("ATMOS") + " !"
# 9. PERC: SAMPLER rim, its steps made by RANDOM (SHIFT+X on the steps), GENERATE
P["perc"] = "RIGHT EDIT EDIT ! LEFT*4 ! UP*3 EDIT DOWN*4 ! EDIT EDIT EDIT SHIFT+X ! EDIT ! Y Y Y EDIT+LEFT*3 SHIFT+X " + name("PERC") + " !"
# 10. KICK P02: a copy of P01 with two more hits; FOLLOW: P01 three times, then P02, then back
P["p2"] = ("UP LEFT*3 EDIT EDIT DOWN EDIT SHIFT+OPT RIGHT SHIFT+EDIT EDIT EDIT Y UP EDIT RIGHT*14 EDIT RIGHT EDIT Y DOWN EDIT "
           "LEFT EDIT DOWN*3 EDIT+RIGHT*2 RIGHT EDIT+RIGHT ! UP*3 LEFT*7 RIGHT EDIT DOWN*3 RIGHT EDIT+RIGHT*3 ! Y Y Y")
# 11. the MIXER: hats and open hat lower; RETURNS: a bigger reverb, the master's compressor; FREEZE seen
P["mix"] = "PLAY w5 F2 F2 LEFT*2 EDIT+DOWN RIGHT EDIT+DOWN*2 ! R1 DOWN EDIT+UP*2 RIGHT RIGHT EDIT+UP ! R1 ! Y"
# 12. SCENES: scene 2 = P02 (the kick alone, a break), then back to scene 1
P["scenes"] = "F1 DOWN EDIT w20 ! UP EDIT w10 F1"
# 13. KEYS: the chords pad on the twelve buttons
P["keys"] = "DOWN RIGHT MENU MENU ! UP RIGHT DOWN LEFT w3 ! MENU"
# 14. the simple view: PLAY, NOTES, SOUND, then out with F1+F2 held
P["simple"] = "F1+F2 LEFT UP*4 DOWN*4 EDIT EDIT ! LEFT*2 EDIT w5 ! MENU ! MENU ! +F1 +F2 w35 -F1 -F2 !"
# 15. SAVE AS HOUSE, EXPORT WAV
P["save"] = "F1+F2 LEFT UP*4 DOWN*3 EDIT DOWN*2 EDIT " + name("HOUSE") + " ! DOWN EDIT ! EDIT w50 PLAY"
open("/dev/stdout", "w").write(" ".join(P[k] for k in (sys.argv[1:] or P)) + "\n")
