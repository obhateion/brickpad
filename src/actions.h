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
static int act_param_get(int s, int p) { return par_get(s, p); }
static void act_param_set(int s, int p, int v) {   // a number straight to a value (the simple view, a dial): kept in range
    if (!par_is_number(p)) return;
    int lo, hi, st, bg; par_limits(s, p, &lo, &hi, &st, &bg); edit_begin(); par_put(s, p, clampi(v, lo, hi));
}
static void act_listen_note(int s, int d) { song.lnote[s] = (int8_t)clampi(song.lnote[s] + d, -48, 48); }   // the note OPTION plays on the panel

// patterns and automation
static void act_pattern(int np) { pat_switch(np); }                           // the selected pad's pattern: queued while its clip plays
static int act_bend(int8_t *bc, int i, int d) { bc[i] = (int8_t)clampi(bc[i] + d, -100, 100); return bc[i]; }   // a segment's curve

// song
static void act_undo(void) { undo(); }
static void act_transport(void) { transport_toggle(); }
static void act_scene(int d) { live_scene_go(d); }

// clipboards: a whole pad (with its DEEP pages), a mixer strip (inserts, sends, VOL, PAN), a pattern
static Track copy_slot; static Cuts copy_cut; static int have_slot; static Pattern clip_board; static int have_clip;
static int8_t copy_cx[16], copy_ax[16], copy_gx[32]; static uint8_t copy_gp[16];
static PadMix strip_board; static int8_t strip_vol, strip_pan, strip_mbx[NINS][6]; static int have_strip;
static void act_pad_copy(int s) { copy_slot = song.t[s]; copy_cut = song.cut[s]; memcpy(copy_cx, song.cx[s], 16); memcpy(copy_ax, song.ax[s], 16); memcpy(copy_gx, song.gx[s], 32); memcpy(copy_gp, song.gp[s], 16); have_slot = 1; toast("SLOT COPIED"); }
static void act_pad_paste(int s) {
    if (!have_slot) return;
    push_undo(); int on = song.t[s].on; song.t[s] = copy_slot; song.cut[s] = copy_cut;
    memcpy(song.cx[s], copy_cx, 16); memcpy(song.ax[s], copy_ax, 16); memcpy(song.gx[s], copy_gx, 32); memcpy(song.gp[s], copy_gp, 16);
    song.t[s].on = on; song.t[s].queued = -1; sync_samples(); toast("SLOT PASTED");
}
static void act_strip_copy(int s) { strip_board = song.mix.pad[s]; memcpy(strip_mbx, song.mbx[s], sizeof strip_mbx); strip_vol = song.t[s].vol; strip_pan = song.t[s].pan; have_strip = 1; toast("STRIP COPIED"); }
static void act_strip_paste(int s) {
    if (!have_strip) return;
    push_undo(); uint8_t solo = song.mix.pad[s].solo; song.mix.pad[s] = strip_board; memcpy(song.mbx[s], strip_mbx, sizeof strip_mbx); song.mix.pad[s].solo = solo; song.t[s].vol = strip_vol; song.t[s].pan = strip_pan; toast("STRIP PASTED");
}
static void act_pattern_copy(const Pattern *p) { clip_board = *p; have_clip = 1; toast("PATTERN COPIED"); }
static void act_pattern_paste(Pattern *p) { if (have_clip) { push_undo(); *p = clip_board; toast("PATTERN PASTED"); } }

// clips and steps
static void act_clip_double(int s) {          // the clip plays twice as long, the same notes twice, ready for a variation
    Pattern *p = cur_pat(s); int n = p->len, m = n * 2 > len_hi() ? len_hi() : n * 2; char b[32];
    if (n >= len_hi()) { toast(len_hi() < NSTEP ? "MAX LENGTH (PROJECT)" : "CLIP IS FULL"); return; }
    push_undo();
    memcpy(p->trig + n, p->trig, (size_t)(m - n)); memcpy(p->note + n, p->note, (size_t)(m - n)); memcpy(p->vel + n, p->vel, (size_t)(m - n));
    memcpy(p->prob + n, p->prob, (size_t)(m - n)); memcpy(p->rep + n, p->rep, (size_t)(m - n)); memcpy(p->slice + n, p->slice, (size_t)(m - n));
    { int st = rl_d32[p->div % 6] * RT, T = n * st, T2 = m * st, cnt = 0, src[NNOTE];   // notes as the steps: hidden ones past the old length give way
      for (int i = 0; i < NNOTE; i++) if (p->roll[i].len && p->roll[i].start >= T && p->roll[i].start < T2) roll_del(p, i);
      for (int i = 0; i < NNOTE; i++) if (p->roll[i].len && p->roll[i].start < T2 - T) src[cnt++] = i;
      for (int j = 0; j < cnt; j++) { Note x = p->roll[src[j]]; int ln = x.start + T + x.len > T2 ? T2 - x.start - T : x.len;
          int k = -1; for (int q = 0; q < NNOTE; q++) if (!p->roll[q].len) { k = q; break; }
          if (k < 0) { toast("ROLL IS FULL (96 NOTES)"); break; }
          Note *y = &p->roll[k]; y->start = (uint16_t)(x.start + T); y->note = x.note; y->vel = x.vel; y->prob = x.prob; y->flags = x.flags & NF_ACC;
          __atomic_store_n(&y->len, (uint16_t)ln, __ATOMIC_RELEASE); } }
    for (int a = 0; a < NAUTO; a++) { AutoLane *al = &song.aut[s][song.t[s].cur][a]; memcpy(al->v + n, al->v, (size_t)(m - n)); int8_t *bc = song.acurve[s][song.t[s].cur][a]; memcpy(bc + n, bc, (size_t)(m - n)); }
    p->len = (uint8_t)m;                 // last: the audio thread never plays steps that are not there yet
    snprintf(b, sizeof b, "CLIP %d > %d STEPS", n, m); toast(b);
}
static void act_steps_clear(int s) {          // only the steps of the pattern on the pad: the sample stays
    edit_begin(); Track *t = &song.t[s]; Pattern *p = cur_pat(s);
    int l = p->len, dv = p->div, md = p->mode; pattern_clear(p); p->len = l; p->div = dv; p->mode = md;
    char b[24]; snprintf(b, sizeof b, "P%d CLEARED", t->cur + 1); toast(b);
}
static void act_pattern_clear(int s, int pat) { edit_begin(); pattern_clear(&song.t[s].pat[pat]); toast("PATTERN CLEARED"); }
static void act_follow_loop(int s, int pat) { edit_begin(); memset(song.fol[s][pat], 0, 6); toast("FOLLOW: LOOP"); }
static void act_pad_volume_step(int s, int d) { Track *t = &song.t[s]; edit_begin(); t->vol = clampi(t->vol + d, 0, 100); }

// a pad on the mixer: volume, pan, sends, ducking, the two inserts
static void fx_cycle(FxSlot *f, int d); static void fx_turn_on(FxSlot *f); static int8_t *unit_param(int unit, int k, int *lo, int *hi, int *def);   // main.c, below
static void act_pad_pan_step(int s, int d) { Track *t = &song.t[s]; edit_begin(); t->pan = (int8_t)clampi(t->pan + d, -50, 50); }
static void act_send_step(int s, int b, int d) {        // b 0: send A (reverb), 1: send B (delay)
    PadMix *pm = &song.mix.pad[s]; edit_begin(); uint8_t *v = b ? &pm->send_b : &pm->send_a; *v = (uint8_t)clampi(*v + d, 0, 100);
}
static void act_duck_amount_step(int s, int d) { PadMix *pm = &song.mix.pad[s]; edit_begin(); pm->spare[DK_AMT] = (int8_t)clampi(pm->spare[DK_AMT] + d, 0, 100); }
static void act_duck_source_step(int s, int d) {        // which pad's notes duck this one: none, then every other pad, round
    PadMix *pm = &song.mix.pad[s]; edit_begin();
    int v = pm->spare[DK_SRC]; do v = (v + d + NSLOT + 1) % (NSLOT + 1); while (v - 1 == s); pm->spare[DK_SRC] = (int8_t)v;
    if (v && pm->spare[DK_AMT] <= 0) pm->spare[DK_AMT] = 80;
}
static void act_insert_onoff(int s, int slot) { edit_begin(); FxSlot *f = &song.mix.pad[s].ins[slot]; if (f->on) f->on = 0; else fx_turn_on(f); }
static void act_insert_on(int s, int slot, int on) { edit_begin(); FxSlot *f = &song.mix.pad[s].ins[slot]; if (on) fx_turn_on(f); else f->on = 0; }
static void act_insert_type_step(int s, int slot, int d) { fx_cycle(&song.mix.pad[s].ins[slot], d); }
static void act_insert_param_step(int s, int slot, int k, int d, int big) {   // big: 10 where the range is wide
    FxSlot *f = &song.mix.pad[s].ins[slot]; int lo, hi; if (!f->type || !fx_pname[f->type][k][0]) return;
    fx_range(f->type, k, &lo, &hi); edit_begin(); f->p[k] = (int8_t)clampi(f->p[k] + d * (big && hi > 10 ? 10 : 1), lo, hi);
}

// the returns (reverb, delay) and the master
static void act_unit_param_add(int unit, int k, int delta) {   // a parameter of a return or master unit, by an amount
    int lo, hi, def; int8_t *v = unit_param(unit, k, &lo, &hi, &def);
    if (v) { edit_begin(); *v = (int8_t)clampi(*v + delta, lo, hi); }
}
static void act_master_step(int d) { edit_begin(); song.master = (int8_t)clampi(song.master + d, 0, 100); }
static void act_master_fx_toggle(int which) { edit_begin(); song.mix.ms[which] ^= 1; }   // MS_COMP, MS_LIM

// the song's settings (PROJECT)
static void act_tempo_step(int d) { edit_begin(); song.tempo = clampi(song.tempo + d, 40, 300); }
static void act_swing_step(int d) { edit_begin(); song.swing = clampi(song.swing + d, 0, 75); }
static void act_launch_step(int d) { edit_begin(); int v = song.launch < 1 ? 1 : song.launch; song.launch = (int8_t)((v - 1 + d + 4) % 4 + 1); }   // 1 BAR, 2, 4, NONE
static void act_metro_toggle(void) { edit_begin(); song.metro = !song.metro; }
static void act_snap_toggle(void) { edit_begin(); song.snap = !song.snap; }
static void act_key_step(int d) { edit_begin(); song.key = (song.key + d + 12) % 12; }   // +-7: a fifth
static void act_scale_step(int d) { edit_begin(); song.scale = (song.scale + d + NSCALE) % NSCALE; }
static void act_beats_step(int d) { edit_begin(); song.beats = clampi(song.beats + d, 1, 12); }
static void act_count_in_step(int d) { edit_begin(); song.count_in = (song.count_in + d + 9) % 9; }
static void act_rec_quant_step(int d) { edit_begin(); song.rec_quant = (song.rec_quant + d + 5) % 5; }
