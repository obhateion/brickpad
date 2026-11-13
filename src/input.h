// Input (UI v2, step 1): the Brick's buttons as keys K_*, which keys are held, and whether a held key already served
// a combo (then its release is no tap). Arrows come from the d-pad hat and repeat while held.
// key_event keeps the order of what a key does; screens ask held[] and input_tap(). main.c includes this after K_*.
#pragma once

static int held[K_N], used[K_N];         // used: the key took part in a combo since it went down
static Uint32 rep_at[4];                 // when a held arrow repeats next, 0 = not held

static int map_button(int b) {   // Brick joystick buttons; EDIT = B, OPTION = A (as in the m8c profile)
    switch (b) {
    case 5: return K_OPT; case 4: return K_EDIT; case 7: return K_X; case 6: return K_Y;
    case 8: return K_L1; case 9: return K_R1; case 10: return K_L2; case 11: return K_R2;
    case 12: return K_MENU; case 13: return K_PLAY; case 14: return K_SHIFT; case 15: return K_F1; case 16: return K_F2;
    }
    return -1;
}
static void input_key(int k, int down) { held[k] = down; if (down) used[k] = 0; }
static void input_combo(int k, int down) {   // a key going down while EDIT or X is held makes that EDIT or X a combo
    if (down && held[K_EDIT] && k != K_EDIT && k != K_SHIFT) used[K_EDIT] = 1;   // any combo: its release is no tap
    if (down && held[K_X] && k != K_X) used[K_X] = 1;
}
static int input_tap(int k) { return !used[k]; }    // asked on release: the key was pressed alone
static void input_use(int k) { used[k] = 1; }       // this key served a combo: its release must not act again

static void input_arrow(int d, int down, Uint32 now) { rep_at[d] = down ? now + 300 : 0; }   // the hat: arrow d down or up
static int input_repeat(int d, Uint32 now) {        // a held arrow is due to repeat (every 50 ms after 300 ms)
    if (!rep_at[d] || now < rep_at[d]) return 0;
    rep_at[d] = now + 50; return 1;
}
