// BrickPad - Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved. See LICENSE.
// PRB modes (2026-11-20, user's plan of 2026-11-13): EDIT on a step of the PRB lane opens its mode. NOTE: the chance
// plays the step (as always). SUB: the step plays, the chance splits it into its SUB cells, else one hit. JUMP: the step
// plays with the note or slice of another step (TYPE STEP) or one of its own (TYPE NOTE: a scale tone, a slice), from a
// LIST of up to three, each with its chance, or from a RANGE taken at the step's chance. The cells shift together.
// The data is the pool Song.jmp (bp.h): an entry only while the mode is not NOTE.
enum { PR_MODE, PR_CHANCE, PR_TYPE, PR_FORM, PR_A1, PR_A2, PR_A3 };
static int pr_step, pr_row;
static const char *const pr_mode_name[PM_N] = {"NOTE", "SUB", "JUMP"};
static void prb_open(int st) { prbing = 1; pr_step = st; pr_row = 0; }
static int pr_rows(const JumpStep *j) { return !j || j->mode != PM_JUMP ? 2 : j->form ? 6 : 7; }
static int pr_vmax(const JumpStep *j) {   // the highest source: a step of the clip, a slice, or two octaves of the scale
    int ns = track_nslices(sel); const Pattern *p = cur_pat(sel);
    return j->kind == 0 ? (p->len ? p->len : 16) - 1 : ns ? ns - 1 : 2 * scale_len(track_scale(sel)) - 1;
}
static void pr_vstr(const JumpStep *j, int v, char *b, int n) {
    if (v < 0) { snprintf(b, n, "OFF"); return; }
    if (j->kind == 0) snprintf(b, n, "STEP %d", v + 1);
    else if (track_nslices(sel)) snprintf(b, n, "SLICE %d", v + 1);
    else { char nm[8]; note_str(deg_midi(sel, v), nm); snprintf(b, n, "%s", nm); }
}
static void prb_key(int k, int down) {
    Pattern *p = cur_pat(sel); int q = song.t[sel].cur; JumpStep *j = jmp_find(sel, q, pr_step);
    if (!down) return;
    used[k] = 1;
    if (k == K_PLAY) { play_key(); return; }
    if (k == K_Y || k == K_MENU) { prbing = 0; return; }
    if (k == K_OPT && !held[K_EDIT]) { step_listen(pr_step); return; }
    int n = pr_rows(j);
    if (!held[K_EDIT]) { if (k == K_UP) pr_row = (pr_row + n - 1) % n; else if (k == K_DOWN) pr_row = (pr_row + 1) % n; return; }
    if (k != K_LEFT && k != K_RIGHT && k != K_UP && k != K_DOWN) return;
    int d = k == K_RIGHT || k == K_UP ? 1 : -1, big = k == K_UP || k == K_DOWN;
    edit_begin();
    if (pr_row == PR_CHANCE) { p->prob[pr_step] = (int8_t)clampi(p->prob[pr_step] + d * (big ? 10 : 1), 0, 100); return; }
    if (pr_row == PR_MODE) { int m = ((j ? j->mode : PM_NOTE) + d + PM_N) % PM_N;
        if (m == PM_NOTE) { if (j) j->slot = 0; return; }
        if (!j && !(j = jmp_get(sel, q, pr_step))) { toast("256 PRB STEPS: THE MOST"); return; }
        j->mode = (uint8_t)m; if (m == PM_SUB && p->rep[pr_step] < 2) toast("SET THE CELLS ON THE SUB LANE"); return; }
    if (!j) return;
    if (pr_row == PR_TYPE) { j->kind = !j->kind; j->a[0] = j->a[1] = j->a[2] = -1; if (j->form) { j->a[0] = 0; j->a[1] = (int8_t)pr_vmax(j); } return; }
    if (pr_row == PR_FORM) { j->form = !j->form; if (j->form) { j->a[0] = 0; j->a[1] = (int8_t)pr_vmax(j); j->a[2] = -1; } else j->a[0] = j->a[1] = j->a[2] = -1; return; }
    int i = pr_row - PR_A1;
    if (!j->form && big) { j->ch[i] = (int8_t)clampi(j->ch[i] + d * 5, 0, 100); return; }   // LIST: EDIT+^v its chance
    j->a[i] = (int8_t)clampi(j->a[i] + d, j->form ? 0 : -1, pr_vmax(j));                      // EDIT+<> the source
}
static void draw_prb(void) {
    Pattern *p = cur_pat(sel); const JumpStep *j = jmp_find(sel, song.t[sel].cur, pr_step); Col ec = C_CAT(PC_PATTERN); char b[48], v[32];
    int n = pr_rows(j), mode = j ? j->mode : PM_NOTE; if (pr_row >= n) pr_row = n - 1;
    snprintf(b, sizeof b, "STEP %d", pr_step + 1); ftext(32, 84, FT_MB, C_WHITE, b);
    static const char *const help[PM_N] = {"THE CHANCE PLAYS THE STEP", "THE STEP PLAYS; THE CHANCE SPLITS IT INTO ITS SUB CELLS", "THE STEP PLAYS WITH ANOTHER NOTE: THE FIRST SOURCE THAT WINS ITS CHANCE"};
    ftext_wrap(200, 80, 790, 2, FT_S, C_GREY, j && j->form && mode == PM_JUMP ? "THE STEP PLAYS; AT ITS CHANCE A NOTE FROM THE RANGE" : help[mode]);
    for (int r = 0; r < n; r++) {
        int y = 140 + r * 76, on = r == pr_row; const char *lb = ""; int bar = -1;
        switch (r) {
        case PR_MODE: lb = "MODE"; snprintf(v, sizeof v, "%s", pr_mode_name[mode]); break;
        case PR_CHANCE: lb = mode == PM_SUB ? "SPLIT CHANCE" : mode == PM_JUMP ? "RANGE CHANCE" : "CHANCE"; snprintf(v, sizeof v, "%d%%", p->prob[pr_step]); bar = p->prob[pr_step];
            if (mode == PM_JUMP && !j->form) { lb = "CHANCE"; snprintf(v, sizeof v, "(EACH SOURCE HAS ITS OWN)"); bar = -1; } break;
        case PR_TYPE: lb = "TYPE"; snprintf(v, sizeof v, "%s", j->kind ? (track_nslices(sel) ? "SLICE" : "NOTE") : "STEP"); break;
        case PR_FORM: lb = "FORM"; snprintf(v, sizeof v, "%s", j->form ? "RANGE" : "LIST"); break;
        default: { int i = r - PR_A1; char s1[16]; pr_vstr(j, j->a[i], s1, sizeof s1);
            if (j->form) { lb = i ? "TO" : "FROM"; snprintf(v, sizeof v, "%s", s1); }
            else { static const char *const sn[3] = {"SOURCE 1", "SOURCE 2", "SOURCE 3"}; lb = sn[i]; if (j->a[i] >= 0) { snprintf(v, sizeof v, "%s  %d%%", s1, j->ch[i]); bar = j->ch[i]; } else snprintf(v, sizeof v, "OFF"); } }
        }
        if (on) { rframe(26, y, 971, 66, 8, 4, ec, C_CELL); ui_focus(lb, v, bar, ec); } else rrect(26, y, 971, 66, 8, C_CELL);
        ftext(46, y + 18, FT_M, C_DIM, lb); ftext(970 - ftext_w(FT_MB, v), y + 16, FT_MB, on ? C_WHITE : C_GREY, v);
        if (bar >= 0) { rect(46, y + 54, 400, 5, C_LINE, 1); rect(46, y + 54, 400 * bar / 100, 5, ec, 1); }
    }
    draw_footer(mode == PM_JUMP && !j->form && pr_row >= PR_A1 ? "EDIT+<> SOURCE  EDIT+^v ITS CHANCE  ^v ROW  OPT HEAR  Y BACK" : "EDIT+ARROWS VALUE  ^v ROW  OPT HEAR  Y BACK");
}
