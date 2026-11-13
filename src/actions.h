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
