// Actions (UI v2, step 1): what a button does to the song, with the pad passed in, never read from the cursor.
// A screen decides WHAT (from where its cursor is), an action does it. The advanced and the simple view of UI v2
// call the same functions. main.c includes this once, right before key_event, so everything above is in reach.
#pragma once

// pads
static void act_select_pad(int s) { sel = (s % NSLOT + NSLOT) % NSLOT; }
static void act_select_step(int d) { act_select_pad(sel + d); }               // the next pad (+1) or the one before (-1), round
static void act_mute_toggle(int s) { song.t[s].mute = !song.t[s].mute; }     // a live move: no undo step
static void act_solo_toggle(int s) { song.mix.pad[s].solo = !song.mix.pad[s].solo; }
static void act_clip_toggle(int s) { clip_toggle(s); }
static void act_listen(int s) { listen(s); }
static void act_freeze(int s, int on) { freeze_set(s, on); }

// a pad's sound: every parameter by its P_* id
static void act_param_step(int s, int p, int d, int big) { par_adjust(s, p, d, big); }
static void act_param_reset(int s, int p) { par_default(s, p); }
static void act_param_text(int s, int p, char *b, int n) { par_str(s, p, b, n); }
static void act_listen_note(int s, int d) { song.lnote[s] = (int8_t)clampi(song.lnote[s] + d, -48, 48); }   // the note OPTION plays on the panel

// patterns and automation
static void act_pattern(int np) { pat_switch(np); }                           // the selected pad's pattern: queued while its clip plays
static int act_bend(int8_t *bc, int i, int d) { bc[i] = (int8_t)clampi(bc[i] + d, -100, 100); return bc[i]; }   // a segment's curve

// song
static void act_undo(void) { undo(); }
static void act_transport(void) { transport_toggle(); }
static void act_scene(int d) { live_scene_go(d); }
