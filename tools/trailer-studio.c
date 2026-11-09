/*
 * trailer-studio.c - Pure ISO C11 Official Trailer Score & Plate Studio
 *
 * Synthesizes:
 *   1. A 60.0-second, 48 kHz, 16-bit stereo calm ambient score (WAV):
 *      - Soft wool-felt acoustic piano physical model (raised-cosine felt
 *        hammer, natural two-stage string decay, gentle string inharmonicity,
 *        per-partial high-frequency damping, zero sharp transients)
 *      - Slow-breathing D-Major / D-Lydian warm cello-pad ensemble
 *      - Stereo 6-comb + 2-allpass damped concert-hall reverb (zero drums,
 *        zero rhythmic pulses, zero harsh waveforms)
 *   2. Seven 1920x1080 RGBA architectural vector overlay plates (ch01..ch07.svg)
 *      showcasing the v5.3.0 Pure ISO C11 Core, mono-ipc AF_UNIX engine,
 *      mono-tui 6-tab Control Deck, live Wayland session, bespoke cursors,
 *      keyboard topology studio, and 20 live animated wallpapers.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 48000
#define DURATION_SEC 60
#define TOTAL_FRAMES (SAMPLE_RATE * DURATION_SEC)

/* -------------------------------------------------------------------------
 * 1. Calm Felt-Piano + Warm Pad + Damped Concert-Hall Reverb DSP Engine
 * ----------------------------------------------------------------------- */

static double midi_to_hz(int midi) {
    /* Warm concert pitch (A4 = 440 Hz, soft temperament) */
    return 440.0 * pow(2.0, ((double)midi - 69.0) / 12.0);
}

typedef struct {
    double start_sec;
    int    midi;
    double vel;
    double dur_sec;
    double pan; /* -1.0 left .. +1.0 right */
} PianoNote;

typedef struct {
    double start_sec;
    double end_sec;
    int    notes[6];
    int    count;
    double gain;
} PadChord;

/* Sparse, contemplative D-Major / D-Lydian felt-piano phrases (0 percussion) */
static const PianoNote PIANO_SCORE[] = {
    /* Phrase 1 (0..8.5s): Dmaj9 - spacious, unhurried opening */
    {  0.90, 62, 0.42, 6.5, -0.12 }, /* D4  */
    {  2.35, 69, 0.38, 5.8,  0.10 }, /* A4  */
    {  4.10, 66, 0.36, 5.5,  0.04 }, /* F#4 */
    {  6.20, 64, 0.33, 5.2, -0.05 }, /* E4  */

    /* Phrase 2 (8.5..17.0s): Bm11 - gentle reflective warmth */
    {  9.10, 59, 0.40, 6.2, -0.15 }, /* B3  */
    { 10.80, 66, 0.36, 5.6,  0.08 }, /* F#4 */
    { 12.70, 69, 0.35, 5.4,  0.14 }, /* A4  */
    { 14.85, 62, 0.32, 5.0, -0.06 }, /* D4  */

    /* Phrase 3 (17.0..26.0s): Gmaj9(#11) - serene Lydian float */
    { 17.60, 55, 0.40, 6.6, -0.18 }, /* G3  */
    { 19.30, 62, 0.37, 5.8, -0.04 }, /* D4  */
    { 21.15, 66, 0.36, 5.6,  0.09 }, /* F#4 */
    { 23.40, 69, 0.33, 5.2,  0.15 }, /* A4  */

    /* Phrase 4 (26.0..35.0s): Asus4 -> Aadd9 - calm clarity */
    { 26.70, 57, 0.39, 6.4, -0.16 }, /* A3  */
    { 28.55, 64, 0.36, 5.6,  0.05 }, /* E4  */
    { 30.60, 62, 0.34, 4.8, -0.02 }, /* D4  */
    { 32.75, 61, 0.32, 5.2,  0.07 }, /* C#4 */

    /* Phrase 5 (35.0..43.5s): Em9 - intimate studio calm */
    { 35.60, 52, 0.38, 6.4, -0.20 }, /* E3  */
    { 37.35, 59, 0.36, 5.8, -0.06 }, /* B3  */
    { 39.20, 66, 0.35, 5.4,  0.10 }, /* F#4 */
    { 41.30, 62, 0.32, 5.0,  0.02 }, /* D4  */

    /* Phrase 6 (43.5..52.0s): Gmaj7 -> D/A - panoramic stillness */
    { 44.10, 55, 0.39, 6.5, -0.16 }, /* G3  */
    { 45.95, 66, 0.36, 5.6,  0.08 }, /* F#4 */
    { 47.90, 64, 0.34, 5.4,  0.03 }, /* E4  */
    { 49.85, 62, 0.33, 5.2, -0.05 }, /* D4  */

    /* Phrase 7 (52.0..60.0s): Dadd9 - quiet home resolution & decay */
    { 52.40, 50, 0.40, 6.8, -0.18 }, /* D3  */
    { 53.55, 57, 0.36, 6.0, -0.08 }, /* A3  */
    { 54.70, 64, 0.34, 4.9,  0.06 }, /* E4  */
    { 55.85, 66, 0.32, 3.9,  0.12 }  /* F#4 */
};

/* Slow overlapping warm string/cello chords (zero harsh harmonics) */
static const PadChord PAD_SCORE[] = {
    {  0.0,  9.8, { 38, 45, 54, 57, 61, 64 }, 6, 0.20 }, /* Dmaj9     */
    {  7.6, 18.2, { 35, 42, 50, 54, 57, 62 }, 6, 0.20 }, /* Bm11      */
    { 16.0, 27.2, { 31, 38, 47, 54, 57, 62 }, 6, 0.20 }, /* Gmaj9     */
    { 25.0, 36.2, { 33, 40, 47, 52, 57, 61 }, 6, 0.19 }, /* Aadd9     */
    { 34.0, 44.8, { 40, 47, 55, 59, 62, 66 }, 6, 0.19 }, /* Em9       */
    { 42.6, 53.2, { 31, 38, 45, 54, 59, 62 }, 6, 0.19 }, /* Gmaj9/D   */
    { 51.0, 59.8, { 38, 45, 52, 54, 57, 62 }, 6, 0.21 }  /* Dadd9 end */
};

typedef struct {
    double *buf;
    int     len;
    int     idx;
    double  filter_state;
    double  feedback;
    double  damp;
} DampedComb;

typedef struct {
    double *buf;
    int     len;
    int     idx;
    double  gain;
} AllPass;

static void comb_init(DampedComb *c, int len, double feedback, double damp) {
    c->buf = (double *)calloc((size_t)len, sizeof(double));
    c->len = len;
    c->idx = 0;
    c->filter_state = 0.0;
    c->feedback = feedback;
    c->damp = damp;
}

static double comb_process(DampedComb *c, double in) {
    double out = c->buf[c->idx];
    c->filter_state = out * (1.0 - c->damp) + c->filter_state * c->damp;
    c->buf[c->idx] = in + c->filter_state * c->feedback;
    c->idx++;
    if (c->idx >= c->len) c->idx = 0;
    return out;
}

static void allpass_init(AllPass *a, int len, double gain) {
    a->buf = (double *)calloc((size_t)len, sizeof(double));
    a->len = len;
    a->idx = 0;
    a->gain = gain;
}

static double allpass_process(AllPass *a, double in) {
    double buf_out = a->buf[a->idx];
    double out = -in + buf_out;
    a->buf[a->idx] = in + buf_out * a->gain;
    a->idx++;
    if (a->idx >= a->len) a->idx = 0;
    return out;
}

static void write_le16(FILE *fp, uint16_t v) {
    uint8_t b[2] = { (uint8_t)(v & 0xffu), (uint8_t)((v >> 8) & 0xffu) };
    (void)fwrite(b, 1, 2, fp);
}

static void write_le32(FILE *fp, uint32_t v) {
    uint8_t b[4] = {
        (uint8_t)(v & 0xffu),
        (uint8_t)((v >> 8) & 0xffu),
        (uint8_t)((v >> 16) & 0xffu),
        (uint8_t)((v >> 24) & 0xffu)
    };
    (void)fwrite(b, 1, 4, fp);
}

static int render_calm_score(const char *wav_path) {
    double *dry_l = (double *)calloc(TOTAL_FRAMES, sizeof(double));
    double *dry_r = (double *)calloc(TOTAL_FRAMES, sizeof(double));
    if (!dry_l || !dry_r) {
        free(dry_l);
        free(dry_r);
        return 1;
    }

    /* 1. Synthesize Warm Wool-Felt Piano Notes */
    size_t n_notes = sizeof(PIANO_SCORE) / sizeof(PIANO_SCORE[0]);
    for (size_t n = 0; n < n_notes; n++) {
        const PianoNote *pn = &PIANO_SCORE[n];
        int i0 = (int)(pn->start_sec * SAMPLE_RATE);
        int dur_samples = (int)(pn->dur_sec * SAMPLE_RATE);
        double f0 = midi_to_hz(pn->midi);
        double pan_l = cos((pn->pan + 1.0) * 0.25 * M_PI);
        double pan_r = sin((pn->pan + 1.0) * 0.25 * M_PI);
        double phase_off = (double)(n * 1.371);

        for (int s = 0; s < dur_samples && (i0 + s) < TOTAL_FRAMES; s++) {
            double t = (double)s / (double)SAMPLE_RATE;

            /* Soft 30ms raised-cosine wool-felt attack (zero percussive click) */
            double attack = (t < 0.030) ? (0.5 * (1.0 - cos(M_PI * t / 0.030))) : 1.0;

            /* Smooth release at end of note window */
            double rem = pn->dur_sec - t;
            double release = (rem < 0.45) ? (0.5 * (1.0 - cos(M_PI * rem / 0.45))) : 1.0;

            /* Dual-stage acoustic string sustain envelope */
            double body_env = 0.74 * exp(-t / 3.9) + 0.26 * exp(-t / 0.95);

            double sig = 0.0;
            /* 5 warm partials with slight piano string inharmonicity & steep felt roll-off */
            for (int k = 1; k <= 5; k++) {
                double inharm = sqrt(1.0 + 0.00014 * (double)(k * k));
                double fk = f0 * (double)k * inharm;
                if (fk > 3400.0) break;
                double partial_amp = 1.0 / pow((double)k, 2.85);
                double partial_decay = exp(-t * 0.72 * (double)(k - 1));
                /* Soft unison string pair (+-0.35 cents) */
                double s1 = sin(2.0 * M_PI * fk * 0.9998 * t + phase_off * k);
                double s2 = sin(2.0 * M_PI * fk * 1.0002 * t + phase_off * k + 0.4);
                sig += 0.5 * (s1 + s2) * partial_amp * partial_decay;
            }

            double sample = sig * pn->vel * attack * release * body_env * 0.34;
            dry_l[i0 + s] += sample * pan_l;
            dry_r[i0 + s] += sample * pan_r;
        }
    }

    /* 2. Synthesize Slow-Breathing Warm Cello / Velvet Pad Ensemble */
    size_t n_chords = sizeof(PAD_SCORE) / sizeof(PAD_SCORE[0]);
    for (size_t c = 0; c < n_chords; c++) {
        const PadChord *ch = &PAD_SCORE[c];
        int i0 = (int)(ch->start_sec * SAMPLE_RATE);
        int i1 = (int)(ch->end_sec * SAMPLE_RATE);
        if (i1 > TOTAL_FRAMES) i1 = TOTAL_FRAMES;
        double span = ch->end_sec - ch->start_sec;

        for (int v = 0; v < ch->count; v++) {
            double f0 = midi_to_hz(ch->notes[v]);
            double vpan = -0.35 + 0.70 * ((double)v / (double)(ch->count > 1 ? ch->count - 1 : 1));
            double gl = cos((vpan + 1.0) * 0.25 * M_PI);
            double gr = sin((vpan + 1.0) * 0.25 * M_PI);
            double lp = 0.0;
            /* Low-pass coefficient around 580 Hz for warm analog/cello tone */
            double alpha = 1.0 - exp(-2.0 * M_PI * 580.0 / (double)SAMPLE_RATE);
            double ph1 = (double)(c * 0.91 + v * 1.73);
            double ph2 = ph1 + 1.19;
            double ph3 = ph1 + 2.41;

            for (int idx = i0; idx < i1; idx++) {
                double t = (double)(idx - i0) / (double)SAMPLE_RATE;
                /* Slow 2.8s cosine fade-in & 3.2s cosine fade-out */
                double env_in = (t < 2.8) ? (0.5 * (1.0 - cos(M_PI * t / 2.8))) : 1.0;
                double rem = span - t;
                double env_out = (rem < 3.2) ? (0.5 * (1.0 - cos(M_PI * rem / 3.2))) : 1.0;
                double breath = 0.90 + 0.10 * sin(2.0 * M_PI * 0.11 * t + (double)v);

                /* Warm fundamental + soft octave + gentle detuned chorus */
                double osc =
                    0.52 * sin(2.0 * M_PI * f0 * 0.9993 * t + ph1) +
                    0.52 * sin(2.0 * M_PI * f0 * 1.0007 * t + ph2) +
                    0.18 * sin(2.0 * M_PI * (f0 * 2.0) * t + ph3);

                lp += alpha * (osc - lp);
                double val = lp * (ch->gain / (double)ch->count) * env_in * env_out * breath;
                dry_l[idx] += val * gl;
                dry_r[idx] += val * gr;
            }
        }
    }

    /* 3. Lush Stereo Damped Concert-Hall Reverb (6-Comb + 2-Allpass per side) */
    static const int COMB_L[6] = { 1597, 1823, 2089, 2377, 2693, 2999 };
    static const int COMB_R[6] = { 1627, 1861, 2131, 2417, 2731, 3037 };
    static const int AP_L[2]   = { 389, 127 };
    static const int AP_R[2]   = { 409, 139 };

    DampedComb combs_l[6], combs_r[6];
    AllPass ap_l[2], ap_r[2];
    for (int i = 0; i < 6; i++) {
        comb_init(&combs_l[i], COMB_L[i], 0.855, 0.46);
        comb_init(&combs_r[i], COMB_R[i], 0.855, 0.46);
    }
    for (int i = 0; i < 2; i++) {
        allpass_init(&ap_l[i], AP_L[i], 0.5);
        allpass_init(&ap_r[i], AP_R[i], 0.5);
    }

    double *out_l = (double *)calloc(TOTAL_FRAMES, sizeof(double));
    double *out_r = (double *)calloc(TOTAL_FRAMES, sizeof(double));
    if (!out_l || !out_r) {
        return 1;
    }

    double peak = 1e-9;
    for (int i = 0; i < TOTAL_FRAMES; i++) {
        double in_mono = 0.5 * (dry_l[i] + dry_r[i]);
        double rev_l = 0.0;
        double rev_r = 0.0;
        for (int c = 0; c < 6; c++) {
            rev_l += comb_process(&combs_l[c], in_mono + 0.15 * dry_l[i]);
            rev_r += comb_process(&combs_r[c], in_mono + 0.15 * dry_r[i]);
        }
        rev_l *= (1.0 / 6.0);
        rev_r *= (1.0 / 6.0);
        for (int a = 0; a < 2; a++) {
            rev_l = allpass_process(&ap_l[a], rev_l);
            rev_r = allpass_process(&ap_r[a], rev_r);
        }

        double t = (double)i / (double)SAMPLE_RATE;
        double master_in  = (t < 2.2) ? (0.5 * (1.0 - cos(M_PI * t / 2.2))) : 1.0;
        double rem        = (double)DURATION_SEC - t;
        double master_out = (rem < 4.2) ? (0.5 * (1.0 - cos(M_PI * rem / 4.2))) : 1.0;
        double master_env = master_in * master_out;

        double mix_l = (0.64 * dry_l[i] + 0.42 * rev_l) * master_env;
        double mix_r = (0.64 * dry_r[i] + 0.42 * rev_r) * master_env;
        out_l[i] = mix_l;
        out_r[i] = mix_r;

        if (fabs(mix_l) > peak) peak = fabs(mix_l);
        if (fabs(mix_r) > peak) peak = fabs(mix_r);
    }

    /* Normalize to a calm, soothing -7.5 dBFS peak (~0.42 full-scale) */
    double norm = 0.42 / peak;

    FILE *fp = fopen(wav_path, "wb");
    if (!fp) return 1;

    uint32_t data_bytes = (uint32_t)(TOTAL_FRAMES * 2 * 2);
    (void)fwrite("RIFF", 1, 4, fp);
    write_le32(fp, 36u + data_bytes);
    (void)fwrite("WAVEfmt ", 1, 8, fp);
    write_le32(fp, 16u);
    write_le16(fp, 1u); /* PCM */
    write_le16(fp, 2u); /* Stereo */
    write_le32(fp, (uint32_t)SAMPLE_RATE);
    write_le32(fp, (uint32_t)(SAMPLE_RATE * 4));
    write_le16(fp, 4u);
    write_le16(fp, 16u);
    (void)fwrite("data", 1, 4, fp);
    write_le32(fp, data_bytes);

    for (int i = 0; i < TOTAL_FRAMES; i++) {
        double sl = out_l[i] * norm;
        double sr = out_r[i] * norm;
        if (sl > 0.98) sl = 0.98;
        if (sl < -0.98) sl = -0.98;
        if (sr > 0.98) sr = 0.98;
        if (sr < -0.98) sr = -0.98;
        int16_t il = (int16_t)lrint(sl * 32767.0);
        int16_t ir = (int16_t)lrint(sr * 32767.0);
        write_le16(fp, (uint16_t)il);
        write_le16(fp, (uint16_t)ir);
    }
    fclose(fp);

    for (int i = 0; i < 6; i++) {
        free(combs_l[i].buf);
        free(combs_r[i].buf);
    }
    for (int i = 0; i < 2; i++) {
        free(ap_l[i].buf);
        free(ap_r[i].buf);
    }
    free(dry_l);
    free(dry_r);
    free(out_l);
    free(out_r);
    return 0;
}

/* -------------------------------------------------------------------------
 * 2. Pure ISO C11 1920x1080 Architectural SVG Cinema Plate Generator
 * ----------------------------------------------------------------------- */

static void write_svg_frame_head(FILE *f, const char *chapter_tag) {
    fprintf(f,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1920\" height=\"1080\" viewBox=\"0 0 1920 1080\">\n"
        "  <style>\n"
        "    .sans { font-family: 'DejaVu Sans', sans-serif; }\n"
        "    .mono { font-family: 'DejaVu Sans Mono', monospace; }\n"
        "  </style>\n"
        /* Subtle cinema vignette + top/bottom architectural letterbox hairline */
        "  <rect x=\"36\" y=\"28\" width=\"1848\" height=\"1024\" rx=\"4\" fill=\"none\" stroke=\"#ffffff\" stroke-opacity=\"0.12\" stroke-width=\"1\"/>\n"
        "  <line x1=\"36\" y1=\"72\" x2=\"1884\" y2=\"72\" stroke=\"#ffffff\" stroke-opacity=\"0.12\" stroke-width=\"1\"/>\n"
        "  <rect x=\"54\" y=\"46\" width=\"8\" height=\"8\" fill=\"#ffffff\"/>\n"
        "  <text x=\"74\" y=\"55\" class=\"mono\" font-size=\"13\" font-weight=\"bold\" fill=\"#f5f5f5\" letter-spacing=\"2\">HYPRLAND MONOCHROME</text>\n"
        "  <text x=\"292\" y=\"55\" class=\"mono\" font-size=\"12\" fill=\"#8e8a8a\" letter-spacing=\"1.5\">// OFFICIAL LAUNCH FILM // v5.3.0</text>\n"
        "  <text x=\"1862\" y=\"55\" text-anchor=\"end\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\" letter-spacing=\"1.5\">%s</text>\n",
        chapter_tag);
}

static void write_svg_frame_foot(FILE *f) {
    fprintf(f, "</svg>\n");
}

static int render_svg_plates(const char *dir) {
    char path[4096];
    FILE *f = NULL;

    /* CHAPTER 01: Opening Architectural Title */
    if (snprintf(path, sizeof(path), "%s/ch01.svg", dir) >= (int)sizeof(path)) return 1;
    f = fopen(path, "w");
    if (!f) return 1;
    write_svg_frame_head(f, "01 / 07  .  PURE ISO C11 EDITION");
    fprintf(f,
        "  <rect x=\"410\" y=\"360\" width=\"1100\" height=\"350\" rx=\"12\" fill=\"#090808\" fill-opacity=\"0.78\" stroke=\"#ffffff\" stroke-opacity=\"0.28\" stroke-width=\"1\"/>\n"
        "  <rect x=\"750\" y=\"404\" width=\"420\" height=\"28\" rx=\"14\" fill=\"#ffffff\"/>\n"
        "  <text x=\"960\" y=\"423\" text-anchor=\"middle\" class=\"mono\" font-size=\"12\" font-weight=\"bold\" fill=\"#080808\" letter-spacing=\"2\">RELEASE v5.3.0  /  100%% PURE ISO C11</text>\n"
        "  <text x=\"960\" y=\"516\" text-anchor=\"middle\" class=\"sans\" font-size=\"68\" font-weight=\"bold\" fill=\"#ffffff\" letter-spacing=\"6\">HYPRLAND</text>\n"
        "  <text x=\"960\" y=\"578\" text-anchor=\"middle\" class=\"sans\" font-size=\"44\" fill=\"#d8d5d5\" letter-spacing=\"14\">MONOCHROME</text>\n"
        "  <line x1=\"560\" y1=\"614\" x2=\"1360\" y2=\"614\" stroke=\"#ffffff\" stroke-opacity=\"0.16\" stroke-width=\"1\"/>\n"
        "  <text x=\"960\" y=\"654\" text-anchor=\"middle\" class=\"mono\" font-size=\"14\" fill=\"#b8b4b4\" letter-spacing=\"2\">ZERO PYTHON  .  ZERO C++  .  ZERO RUNTIME JQ  .  LUA 5.4 COMPOSITOR</text>\n"
        "  <text x=\"960\" y=\"1024\" text-anchor=\"middle\" class=\"mono\" font-size=\"12\" fill=\"#8a8686\" letter-spacing=\"2.5\">ARCHITECTURAL WAYLAND ENVIRONMENT  /  CALM SCORE IN D LYDIAN</text>\n");
    write_svg_frame_foot(f);
    fclose(f);

    /* CHAPTER 02: mono-ipc AF_UNIX Socket Engine & 11 Pure C11 Binaries */
    if (snprintf(path, sizeof(path), "%s/ch02.svg", dir) >= (int)sizeof(path)) return 1;
    f = fopen(path, "w");
    if (!f) return 1;
    write_svg_frame_head(f, "02 / 07  .  AF_UNIX NATIVE SOCKET IPC");
    fprintf(f,
        "  <rect x=\"96\" y=\"148\" width=\"840\" height=\"710\" rx=\"12\" fill=\"#080808\" fill-opacity=\"0.86\" stroke=\"#ffffff\" stroke-opacity=\"0.26\" stroke-width=\"1\"/>\n"
        "  <text x=\"136\" y=\"196\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\" letter-spacing=\"2\">DIRECT COMPOSITOR WIRE PROTOCOL</text>\n"
        "  <text x=\"136\" y=\"236\" class=\"sans\" font-size=\"30\" font-weight=\"bold\" fill=\"#ffffff\">mono-ipc AF_UNIX Socket Engine</text>\n"
        "  <rect x=\"742\" y=\"208\" width=\"156\" height=\"30\" rx=\"6\" fill=\"#181616\" stroke=\"#4a4545\" stroke-width=\"1\"/>\n"
        "  <text x=\"820\" y=\"228\" text-anchor=\"middle\" class=\"mono\" font-size=\"12\" font-weight=\"bold\" fill=\"#ffffff\">0.07 ms WIRE</text>\n"
        /* Wire nodes */
        "  <rect x=\"136\" y=\"274\" width=\"760\" height=\"66\" rx=\"8\" fill=\"#100f0f\" stroke=\"#383434\" stroke-width=\"1\"/>\n"
        "  <rect x=\"156\" y=\"291\" width=\"124\" height=\"32\" rx=\"5\" fill=\"#ffffff\"/>\n"
        "  <text x=\"218\" y=\"312\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" font-weight=\"bold\" fill=\"#080808\">mono-ipc</text>\n"
        "  <text x=\"304\" y=\"312\" class=\"mono\" font-size=\"14\" fill=\"#9e9a9a\">--&gt;</text>\n"
        "  <rect x=\"346\" y=\"291\" width=\"198\" height=\"32\" rx=\"5\" fill=\"#1a1818\" stroke=\"#524d4d\" stroke-width=\"1\"/>\n"
        "  <text x=\"445\" y=\"312\" text-anchor=\"middle\" class=\"mono\" font-size=\"12\" fill=\"#f2f0f0\">$HIS/.socket.sock</text>\n"
        "  <text x=\"564\" y=\"312\" class=\"mono\" font-size=\"14\" fill=\"#9e9a9a\">--&gt;</text>\n"
        "  <rect x=\"606\" y=\"291\" width=\"268\" height=\"32\" rx=\"5\" fill=\"#1a1818\" stroke=\"#524d4d\" stroke-width=\"1\"/>\n"
        "  <text x=\"740\" y=\"312\" text-anchor=\"middle\" class=\"mono\" font-size=\"12\" fill=\"#f2f0f0\">RFC-8259 C11 JSON Parser</text>\n"
        /* Terminal output box */
        "  <rect x=\"136\" y=\"366\" width=\"760\" height=\"310\" rx=\"8\" fill=\"#050505\" stroke=\"#2e2b2b\" stroke-width=\"1\"/>\n"
        "  <text x=\"164\" y=\"406\" class=\"mono\" font-size=\"15\" fill=\"#ffffff\">$ mono-ipc raw \"j/activewindow\"</text>\n"
        "  <text x=\"164\" y=\"438\" class=\"mono\" font-size=\"14\" fill=\"#cfcbcb\">{ \"class\": \"kitty\", \"workspace\": { \"id\": 1 }, \"size\": [939, 1014] }</text>\n"
        "  <text x=\"164\" y=\"488\" class=\"mono\" font-size=\"15\" fill=\"#ffffff\">$ mono-ipc watch   # .socket2.sock push stream</text>\n"
        "  <text x=\"164\" y=\"520\" class=\"mono\" font-size=\"14\" fill=\"#cfcbcb\">workspacev2&gt;&gt;1,1</text>\n"
        "  <text x=\"164\" y=\"548\" class=\"mono\" font-size=\"14\" fill=\"#cfcbcb\">activelayout&gt;&gt;keyboard,English (Colemak-DH)</text>\n"
        "  <text x=\"164\" y=\"576\" class=\"mono\" font-size=\"14\" fill=\"#cfcbcb\">screencastv2&gt;&gt;1,monitor,DP-1   [Dunst auto-paused]</text>\n"
        "  <text x=\"164\" y=\"626\" class=\"mono\" font-size=\"15\" fill=\"#ffffff\">$ mono-ipc sysgraph</text>\n"
        "  <text x=\"164\" y=\"654\" class=\"mono\" font-size=\"14\" fill=\"#cfcbcb\">{\"text\":\" 14%%\",\"tooltip\":\"Zero-fork Pure ISO C11 /proc telemetry\"}</text>\n"
        "  <text x=\"136\" y=\"726\" class=\"mono\" font-size=\"13\" fill=\"#b8b4b4\">Replaces hyprctl + jq pipelines across all 37 runtime scripts.</text>\n"
        "  <text x=\"136\" y=\"756\" class=\"mono\" font-size=\"13\" fill=\"#ffffff\" font-weight=\"bold\">0 Python files  .  0 C++ files  .  0 runtime jq subprocesses</text>\n"
        /* Right card: 11 C11 binaries */
        "  <rect x=\"984\" y=\"148\" width=\"840\" height=\"710\" rx=\"12\" fill=\"#080808\" fill-opacity=\"0.86\" stroke=\"#ffffff\" stroke-opacity=\"0.26\" stroke-width=\"1\"/>\n"
        "  <text x=\"1024\" y=\"196\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\" letter-spacing=\"2\">CC -STD=C11 -O3 -WALL -WEXTRA -WERROR</text>\n"
        "  <text x=\"1024\" y=\"236\" class=\"sans\" font-size=\"30\" font-weight=\"bold\" fill=\"#ffffff\">Native Pure ISO C11 Toolchain</text>\n");

    static const char *BINS[12][2] = {
        { "mono-ipc",          "AF_UNIX socket &amp; JSON engine" },
        { "mono-tui",          "6-tab POSIX termios control deck" },
        { "mono-calc",         "Recursive-descent math parser" },
        { "cursor-gen",        "hyprcursor .hlc + Xcursor compiler" },
        { "wallpaper-anim",    "20-loop parallel GIF89a studio" },
        { "wallpaper-gen",     "Procedural 4K greyscale synth" },
        { "serve-site",        "POSIX HTTP/1.1 byte-range server" },
        { "keybind-table",     "176-shortcut binds.lua parser" },
        { "mono-util",         "Pure C11 JSON &amp; TOML verifier" },
        { "build-preview",     "Self-contained preview bundler" },
        { "build-windows-kit", "Cross-OS monochrome profile gen" },
        { "trailer-studio",    "48 kHz calm piano &amp; plate synth" }
    };
    for (int i = 0; i < 12; i++) {
        int col = i % 2;
        int row = i / 2;
        int bx = 1024 + col * 386;
        int by = 274 + row * 88;
        fprintf(f,
            "  <rect x=\"%d\" y=\"%d\" width=\"370\" height=\"72\" rx=\"8\" fill=\"#111010\" stroke=\"#343030\" stroke-width=\"1\"/>\n"
            "  <text x=\"%d\" y=\"%d\" class=\"mono\" font-size=\"15\" font-weight=\"bold\" fill=\"#ffffff\">%s</text>\n"
            "  <text x=\"%d\" y=\"%d\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\">%s</text>\n",
            bx, by, bx + 18, by + 30, BINS[i][0], bx + 18, by + 52, BINS[i][1]);
    }
    fprintf(f,
        "  <rect x=\"96\" y=\"904\" width=\"1728\" height=\"96\" rx=\"10\" fill=\"#090808\" fill-opacity=\"0.84\" stroke=\"#ffffff\" stroke-opacity=\"0.20\" stroke-width=\"1\"/>\n"
        "  <text x=\"136\" y=\"946\" class=\"sans\" font-size=\"22\" font-weight=\"bold\" fill=\"#ffffff\">SUB-MILLISECOND COMPOSITOR TELEMETRY</text>\n"
        "  <text x=\"136\" y=\"976\" class=\"mono\" font-size=\"13\" fill=\"#a8a4a4\">Every panel, keybind menu, Waybar module, and reactive daemon communicates directly over POSIX AF_UNIX sockets.</text>\n");
    write_svg_frame_foot(f);
    fclose(f);

    /* CHAPTER 03: mono-tui 6-Tab Terminal Control Deck */
    if (snprintf(path, sizeof(path), "%s/ch03.svg", dir) >= (int)sizeof(path)) return 1;
    f = fopen(path, "w");
    if (!f) return 1;
    write_svg_frame_head(f, "03 / 07  .  MONO-TUI TERMINAL CONTROL DECK");
    fprintf(f,
        "  <rect x=\"180\" y=\"138\" width=\"1560\" height=\"730\" rx=\"14\" fill=\"#070707\" fill-opacity=\"0.90\" stroke=\"#ffffff\" stroke-opacity=\"0.32\" stroke-width=\"1\"/>\n"
        "  <text x=\"228\" y=\"188\" class=\"mono\" font-size=\"15\" font-weight=\"bold\" fill=\"#ffffff\">hyprland-monochrome v5.3.0  //  Pure ISO C11 Control Deck (mono-tui)</text>\n"
        "  <rect x=\"1480\" y=\"164\" width=\"212\" height=\"32\" rx=\"6\" fill=\"#ffffff\"/>\n"
        "  <text x=\"1586\" y=\"185\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" font-weight=\"bold\" fill=\"#080808\">Super + Ctrl + M</text>\n"
        "  <line x1=\"220\" y1=\"214\" x2=\"1700\" y2=\"214\" stroke=\"#2e2b2b\" stroke-width=\"1\"/>\n");

    static const char *TABS[6] = {
        "[ 1:TELEMETRY ]", "  2:VELOCITY  ", "  3:SHADERS  ",
        "  4:INPUT+CURSOR  ", "  5:WALLPAPERS  ", "  6:BENCHMARK  "
    };
    int tx = 228;
    for (int t = 0; t < 6; t++) {
        int w = (t == 3) ? 210 : 185;
        fprintf(f,
            "  <rect x=\"%d\" y=\"234\" width=\"%d\" height=\"38\" rx=\"6\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1\"/>\n"
            "  <text x=\"%d\" y=\"258\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" font-weight=\"bold\" fill=\"%s\">%s</text>\n",
            tx, w,
            t == 0 ? "#ffffff" : "#121111",
            t == 0 ? "#ffffff" : "#383434",
            tx + w / 2,
            t == 0 ? "#080808" : "#a8a4a4",
            TABS[t]);
        tx += w + 14;
    }

    /* Left half inside TUI: CPU + Per-core meters + Memory */
    fprintf(f,
        "  <rect x=\"228\" y=\"296\" width=\"820\" height=\"524\" rx=\"10\" fill=\"#0c0b0b\" stroke=\"#2c2929\" stroke-width=\"1\"/>\n"
        "  <text x=\"260\" y=\"338\" class=\"mono\" font-size=\"14\" font-weight=\"bold\" fill=\"#ffffff\">LIVE /PROC TELEMETRY  (ZERO SUBPROCESS FORKS)</text>\n"
        "  <text x=\"260\" y=\"378\" class=\"mono\" font-size=\"14\" fill=\"#d8d4d4\">CPU TOTAL</text>\n"
        "  <rect x=\"380\" y=\"364\" width=\"490\" height=\"16\" rx=\"3\" fill=\"#1a1818\"/>\n"
        "  <rect x=\"380\" y=\"364\" width=\"118\" height=\"16\" rx=\"3\" fill=\"#ffffff\"/>\n"
        "  <text x=\"890\" y=\"378\" class=\"mono\" font-size=\"14\" font-weight=\"bold\" fill=\"#ffffff\">16%%  /  44.0 C</text>\n");

    static const int CORE_W[8] = { 104, 72, 138, 64, 96, 78, 122, 58 };
    static const int CORE_P[8] = { 21, 14, 28, 12, 19, 15, 24, 11 };
    for (int c = 0; c < 8; c++) {
        int cy = 418 + (c % 4) * 38;
        int cx = 260 + (c / 4) * 390;
        fprintf(f,
            "  <text x=\"%d\" y=\"%d\" class=\"mono\" font-size=\"13\" fill=\"#9e9a9a\">core%d</text>\n"
            "  <rect x=\"%d\" y=\"%d\" width=\"240\" height=\"12\" rx=\"2\" fill=\"#191717\"/>\n"
            "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"12\" rx=\"2\" fill=\"#dcd9d9\"/>\n"
            "  <text x=\"%d\" y=\"%d\" class=\"mono\" font-size=\"13\" fill=\"#e8e6e6\">%d%%</text>\n",
            cx, cy, c, cx + 64, cy - 11, cx + 64, cy - 11, CORE_W[c], cx + 318, cy, CORE_P[c]);
    }

    fprintf(f,
        "  <text x=\"260\" y=\"604\" class=\"mono\" font-size=\"14\" fill=\"#d8d4d4\">MEMORY</text>\n"
        "  <rect x=\"380\" y=\"590\" width=\"490\" height=\"16\" rx=\"3\" fill=\"#1a1818\"/>\n"
        "  <rect x=\"380\" y=\"590\" width=\"132\" height=\"16\" rx=\"3\" fill=\"#ffffff\"/>\n"
        "  <text x=\"890\" y=\"604\" class=\"mono\" font-size=\"14\" fill=\"#d8d4d4\">4.2 / 16.0 GiB</text>\n"
        "  <line x1=\"260\" y1=\"642\" x2=\"1012\" y2=\"642\" stroke=\"#262323\" stroke-width=\"1\"/>\n"
        "  <text x=\"260\" y=\"682\" class=\"mono\" font-size=\"14\" fill=\"#9e9a9a\">Motion Physics : <tspan fill=\"#ffffff\" font-weight=\"bold\">fluid silk spring</tspan>     Optical Glass : <tspan fill=\"#ffffff\" font-weight=\"bold\">2-pass kawase</tspan></text>\n"
        "  <text x=\"260\" y=\"718\" class=\"mono\" font-size=\"14\" fill=\"#9e9a9a\">Screen Shader  : <tspan fill=\"#ffffff\" font-weight=\"bold\">noir luminance</tspan>        Cursor Theme  : <tspan fill=\"#ffffff\" font-weight=\"bold\">Monochrome-Obsidian (24px)</tspan></text>\n"
        "  <text x=\"260\" y=\"754\" class=\"mono\" font-size=\"14\" fill=\"#9e9a9a\">Keyboard Type  : <tspan fill=\"#ffffff\" font-weight=\"bold\">compact60 / CMK-DH</tspan>    IPC Socket    : <tspan fill=\"#ffffff\" font-weight=\"bold\">.socket.sock CONNECTED</tspan></text>\n"
        "  <text x=\"260\" y=\"794\" class=\"mono\" font-size=\"12\" fill=\"#787474\">KEYS: [1-6/Tab] Switch Tab   [j/k] Navigate   [Enter] Apply Live   [r] Re-Bench   [q] Quit</text>\n");

    /* Right half inside TUI: CLOCK_MONOTONIC Microsecond Benchmark */
    fprintf(f,
        "  <rect x=\"1076\" y=\"296\" width=\"616\" height=\"524\" rx=\"10\" fill=\"#0c0b0b\" stroke=\"#2c2929\" stroke-width=\"1\"/>\n"
        "  <text x=\"1108\" y=\"338\" class=\"mono\" font-size=\"14\" font-weight=\"bold\" fill=\"#ffffff\">CLOCK_MONOTONIC MICROSECOND AUDIT</text>\n"
        "  <rect x=\"1108\" y=\"368\" width=\"552\" height=\"76\" rx=\"8\" fill=\"#141212\" stroke=\"#343030\" stroke-width=\"1\"/>\n"
        "  <text x=\"1132\" y=\"400\" class=\"mono\" font-size=\"13\" fill=\"#b8b4b4\">/proc/stat + /proc/meminfo + /sys/bus/pci</text>\n"
        "  <text x=\"1132\" y=\"426\" class=\"mono\" font-size=\"18\" font-weight=\"bold\" fill=\"#ffffff\">50.87 us   (in-process Pure ISO C11)</text>\n"
        "  <rect x=\"1108\" y=\"462\" width=\"552\" height=\"76\" rx=\"8\" fill=\"#141212\" stroke=\"#343030\" stroke-width=\"1\"/>\n"
        "  <text x=\"1132\" y=\"494\" class=\"mono\" font-size=\"13\" fill=\"#b8b4b4\">XDG state &amp; compositor profile reader</text>\n"
        "  <text x=\"1132\" y=\"520\" class=\"mono\" font-size=\"18\" font-weight=\"bold\" fill=\"#ffffff\">14.36 us   (zero shell subshells)</text>\n"
        "  <rect x=\"1108\" y=\"556\" width=\"552\" height=\"76\" rx=\"8\" fill=\"#141212\" stroke=\"#343030\" stroke-width=\"1\"/>\n"
        "  <text x=\"1132\" y=\"588\" class=\"mono\" font-size=\"13\" fill=\"#b8b4b4\">Hyprland AF_UNIX .socket.sock stat probe</text>\n"
        "  <text x=\"1132\" y=\"614\" class=\"mono\" font-size=\"18\" font-weight=\"bold\" fill=\"#ffffff\">0.13 us    (native POSIX stat)</text>\n"
        "  <rect x=\"1108\" y=\"656\" width=\"552\" height=\"132\" rx=\"8\" fill=\"#181616\" stroke=\"#ffffff\" stroke-opacity=\"0.35\" stroke-width=\"1\"/>\n"
        "  <text x=\"1132\" y=\"694\" class=\"mono\" font-size=\"13\" fill=\"#c8c4c4\">TOTAL C11 TELEMETRY FRAME BUDGET</text>\n"
        "  <text x=\"1132\" y=\"736\" class=\"sans\" font-size=\"34\" font-weight=\"bold\" fill=\"#ffffff\">65.36 microseconds</text>\n"
        "  <text x=\"1132\" y=\"768\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\">Headless CLI ready:  mono-tui --snapshot  |  --json  |  --bench</text>\n"
        /* Lower caption */
        "  <rect x=\"180\" y=\"904\" width=\"1560\" height=\"96\" rx=\"10\" fill=\"#090808\" fill-opacity=\"0.84\" stroke=\"#ffffff\" stroke-opacity=\"0.20\" stroke-width=\"1\"/>\n"
        "  <text x=\"224\" y=\"946\" class=\"sans\" font-size=\"22\" font-weight=\"bold\" fill=\"#ffffff\">ZERO-DEPENDENCY POSIX TERMIOS CONTROL DECK</text>\n"
        "  <text x=\"224\" y=\"976\" class=\"mono\" font-size=\"13\" fill=\"#a8a4a4\">Control motion physics, GLSL screen shaders, monochrome cursors, keyboard hardware profiles, and live wallpapers from one binary.</text>\n");
    write_svg_frame_foot(f);
    fclose(f);

    /* CHAPTER 04: Live Captured Wayland Session Lower-Third HUD */
    if (snprintf(path, sizeof(path), "%s/ch04.svg", dir) >= (int)sizeof(path)) return 1;
    f = fopen(path, "w");
    if (!f) return 1;
    write_svg_frame_head(f, "04 / 07  .  LIVE WAYLAND DESKTOP SESSION");
    fprintf(f,
        "  <rect x=\"84\" y=\"858\" width=\"1752\" height=\"142\" rx=\"12\" fill=\"#080808\" fill-opacity=\"0.86\" stroke=\"#ffffff\" stroke-opacity=\"0.28\" stroke-width=\"1\"/>\n"
        "  <rect x=\"120\" y=\"886\" width=\"116\" height=\"26\" rx=\"5\" fill=\"#ffffff\"/>\n"
        "  <text x=\"178\" y=\"904\" text-anchor=\"middle\" class=\"mono\" font-size=\"12\" font-weight=\"bold\" fill=\"#080808\">LIVE CAPTURE</text>\n"
        "  <text x=\"254\" y=\"904\" class=\"mono\" font-size=\"13\" fill=\"#b8b4b4\" letter-spacing=\"1.5\">UNIFIED FULL-STACK MONOCHROME DESKTOP  /  STRICT R = G = B</text>\n"
        "  <text x=\"120\" y=\"946\" class=\"sans\" font-size=\"28\" font-weight=\"bold\" fill=\"#ffffff\">Every Toolkit, Browser, Terminal &amp; Application in Pure Greyscale</text>\n"
        "  <text x=\"120\" y=\"978\" class=\"mono\" font-size=\"13\" fill=\"#a8a4a4\">GTK 3/4  .  Qt 5/6  .  Kitty + Zsh + Starship  .  Rofi  .  Waybar  .  Thunar  .  5 Live GLSL ES 3.00 Screen Shaders</text>\n");
    write_svg_frame_foot(f);
    fclose(f);

    /* CHAPTER 05: Bespoke Monochrome Cursors & Physical Keyboard Studio */
    if (snprintf(path, sizeof(path), "%s/ch05.svg", dir) >= (int)sizeof(path)) return 1;
    f = fopen(path, "w");
    if (!f) return 1;
    write_svg_frame_head(f, "05 / 07  .  CURSOR SUITE &amp; KEYBOARD STUDIO");
    fprintf(f,
        /* Left Studio Panel: 4 Cursors */
        "  <rect x=\"96\" y=\"154\" width=\"840\" height=\"690\" rx=\"12\" fill=\"#080808\" fill-opacity=\"0.86\" stroke=\"#ffffff\" stroke-opacity=\"0.26\" stroke-width=\"1\"/>\n"
        "  <text x=\"136\" y=\"202\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\" letter-spacing=\"2\">PURE C11 HYPRCURSOR (.HLC) + XCURSOR COMPILER</text>\n"
        "  <text x=\"136\" y=\"244\" class=\"sans\" font-size=\"30\" font-weight=\"bold\" fill=\"#ffffff\">Bespoke Monochrome Cursor Suite</text>\n"
        /* 4 cursor showcase cards */
        "  <rect x=\"136\" y=\"284\" width=\"366\" height=\"210\" rx=\"10\" fill=\"#111010\" stroke=\"#ffffff\" stroke-opacity=\"0.4\" stroke-width=\"1\"/>\n"
        "  <polygon points=\"196,318 196,408 222,384 240,418 252,411 234,377 268,377\" fill=\"#0a0a0a\" stroke=\"#f5f5f5\" stroke-width=\"4\" stroke-linejoin=\"round\"/>\n"
        "  <text x=\"164\" y=\"454\" class=\"sans\" font-size=\"20\" font-weight=\"bold\" fill=\"#ffffff\">Monochrome-Obsidian</text>\n"
        "  <text x=\"164\" y=\"478\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\">#0a0a0a body  /  silver precision edge</text>\n"

        "  <rect x=\"530\" y=\"284\" width=\"366\" height=\"210\" rx=\"10\" fill=\"#111010\" stroke=\"#383434\" stroke-width=\"1\"/>\n"
        "  <polygon points=\"590,318 590,408 616,384 634,418 646,411 628,377 662,377\" fill=\"#f7f7f7\" stroke=\"#0a0a0a\" stroke-width=\"4\" stroke-linejoin=\"round\"/>\n"
        "  <text x=\"558\" y=\"454\" class=\"sans\" font-size=\"20\" font-weight=\"bold\" fill=\"#ffffff\">Monochrome-Ivory</text>\n"
        "  <text x=\"558\" y=\"478\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\">#f7f7f7 body  /  high-contrast obsidian stroke</text>\n"

        "  <rect x=\"136\" y=\"518\" width=\"366\" height=\"210\" rx=\"10\" fill=\"#111010\" stroke=\"#383434\" stroke-width=\"1\"/>\n"
        "  <polygon points=\"196,552 196,642 222,618 240,652 252,645 234,611 268,611\" fill=\"none\" stroke=\"#f4f4f4\" stroke-width=\"3.5\" stroke-linejoin=\"round\"/>\n"
        "  <circle cx=\"196\" cy=\"552\" r=\"4.5\" fill=\"#ffffff\"/>\n"
        "  <text x=\"164\" y=\"688\" class=\"sans\" font-size=\"20\" font-weight=\"bold\" fill=\"#ffffff\">Monochrome-Precision</text>\n"
        "  <text x=\"164\" y=\"712\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\">Hollow CAD drafting wireframe reticle</text>\n"

        "  <rect x=\"530\" y=\"518\" width=\"366\" height=\"210\" rx=\"10\" fill=\"#111010\" stroke=\"#383434\" stroke-width=\"1\"/>\n"
        "  <circle cx=\"626\" cy=\"598\" r=\"32\" fill=\"none\" stroke=\"#f4f4f4\" stroke-width=\"3.5\"/>\n"
        "  <circle cx=\"626\" cy=\"598\" r=\"13\" fill=\"#f4f4f4\"/>\n"
        "  <text x=\"558\" y=\"688\" class=\"sans\" font-size=\"20\" font-weight=\"bold\" fill=\"#ffffff\">Monochrome-Dot</text>\n"
        "  <text x=\"558\" y=\"712\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\">Concentric telemetry halo &amp; core</text>\n"
        "  <text x=\"136\" y=\"790\" class=\"mono\" font-size=\"13\" fill=\"#cfcbcb\">Multi-DPI 24 / 32 / 48 px  .  13 shapes + 40 Wayland/X11 aliases  .  8-frame watch</text>\n"

        /* Right Studio Panel: Keyboard Type Studio */
        "  <rect x=\"984\" y=\"154\" width=\"840\" height=\"690\" rx=\"12\" fill=\"#080808\" fill-opacity=\"0.86\" stroke=\"#ffffff\" stroke-opacity=\"0.26\" stroke-width=\"1\"/>\n"
        "  <text x=\"1024\" y=\"202\" class=\"mono\" font-size=\"12\" fill=\"#9e9a9a\" letter-spacing=\"2\">PHYSICAL HARDWARE TOPOLOGY &amp; XKB ENGINE</text>\n"
        "  <text x=\"1024\" y=\"244\" class=\"sans\" font-size=\"30\" font-weight=\"bold\" fill=\"#ffffff\">Keyboard Type &amp; Keymap Studio</text>\n");

    static const char *KB_CHIPS[6] = {
        "ANSI 104", "60% HHKB (Caps=Esc)", "Vim Swap", "Home-Row Ctrl", "ISO 105", "Apple Cmd"
    };
    for (int k = 0; k < 6; k++) {
        int kx = 1024 + (k % 3) * 256;
        int ky = 284 + (k / 3) * 52;
        fprintf(f,
            "  <rect x=\"%d\" y=\"%d\" width=\"242\" height=\"40\" rx=\"6\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1\"/>\n"
            "  <text x=\"%d\" y=\"%d\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" font-weight=\"bold\" fill=\"%s\">%s</text>\n",
            kx, ky,
            k == 1 ? "#ffffff" : "#141212",
            k == 1 ? "#ffffff" : "#383434",
            kx + 121, ky + 25,
            k == 1 ? "#080808" : "#cfcbcb",
            KB_CHIPS[k]);
    }

    /* Minimal geometric 60% HHKB keycap deck */
    static const char *ROW_HOME[11] = { "ESC*", "A", "R", "S", "T", "G", "M", "N", "E", "I", "ENTER" };
    int kx = 1024;
    fprintf(f, "  <rect x=\"1024\" y=\"410\" width=\"760\" height=\"230\" rx=\"10\" fill=\"#0e0d0d\" stroke=\"#2e2b2b\" stroke-width=\"1\"/>\n");
    for (int i = 0; i < 11; i++) {
        int kw = (i == 0) ? 86 : (i == 10 ? 104 : 50);
        int hi = (i == 0);
        fprintf(f,
            "  <rect x=\"%d\" y=\"482\" width=\"%d\" height=\"48\" rx=\"6\" fill=\"%s\" stroke=\"%s\" stroke-width=\"1\"/>\n"
            "  <text x=\"%d\" y=\"511\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" font-weight=\"bold\" fill=\"%s\">%s</text>\n",
            kx + 16, kw,
            hi ? "#ffffff" : "#191717",
            hi ? "#ffffff" : "#3a3636",
            kx + 16 + kw / 2,
            hi ? "#080808" : "#e8e6e6",
            ROW_HOME[i]);
        kx += kw + 6;
    }
    fprintf(f,
        "  <rect x=\"1220\" y=\"548\" width=\"360\" height=\"46\" rx=\"6\" fill=\"#ffffff\"/>\n"
        "  <text x=\"1400\" y=\"576\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" font-weight=\"bold\" fill=\"#080808\">COLEMAK-DH  /  38 HZ TACTILE  /  176 BINDS</text>\n"
        "  <text x=\"1024\" y=\"694\" class=\"mono\" font-size=\"14\" fill=\"#ffffff\">QWERTY  .  Colemak-DH  .  Dvorak  .  Workman  .  AZERTY  .  QWERTZ  .  US+ARA</text>\n"
        "  <text x=\"1024\" y=\"730\" class=\"mono\" font-size=\"13\" fill=\"#9e9a9a\">Instant live switching via Super + Shift + K with real-time Waybar layout telemetry.</text>\n"
        "  <text x=\"1024\" y=\"790\" class=\"mono\" font-size=\"13\" fill=\"#cfcbcb\">176 synced shortcuts parsed directly from binds.lua by bin/keybind-table</text>\n"
        /* Bottom bar */
        "  <rect x=\"96\" y=\"888\" width=\"1728\" height=\"104\" rx=\"10\" fill=\"#090808\" fill-opacity=\"0.84\" stroke=\"#ffffff\" stroke-opacity=\"0.20\" stroke-width=\"1\"/>\n"
        "  <text x=\"136\" y=\"932\" class=\"sans\" font-size=\"22\" font-weight=\"bold\" fill=\"#ffffff\">CRAFTED FOR HARDWARE PRECISION ACROSS ANY PC</text>\n"
        "  <text x=\"136\" y=\"964\" class=\"mono\" font-size=\"13\" fill=\"#a8a4a4\">Auto-profiles NVIDIA, AMD RADV, Intel Iris/Arc, Hybrid Laptops, HiDPI Fractional Scaling, and QEMU/KVM Virtual Machines.</text>\n");
    write_svg_frame_foot(f);
    fclose(f);

    /* CHAPTER 06: 20 / 20 Live Animated Pure C11 Wallpapers Mosaic */
    if (snprintf(path, sizeof(path), "%s/ch06.svg", dir) >= (int)sizeof(path)) return 1;
    f = fopen(path, "w");
    if (!f) return 1;
    write_svg_frame_head(f, "06 / 07  .  20 LIVE ANIMATED WALLPAPERS");
    fprintf(f,
        "  <rect x=\"420\" y=\"368\" width=\"1080\" height=\"344\" rx=\"14\" fill=\"#080808\" fill-opacity=\"0.86\" stroke=\"#ffffff\" stroke-opacity=\"0.34\" stroke-width=\"1\"/>\n"
        "  <rect x=\"770\" y=\"412\" width=\"380\" height=\"28\" rx=\"14\" fill=\"#ffffff\"/>\n"
        "  <text x=\"960\" y=\"431\" text-anchor=\"middle\" class=\"mono\" font-size=\"12\" font-weight=\"bold\" fill=\"#080808\" letter-spacing=\"2\">20 / 20 LIVE ANIMATED LOOPS</text>\n"
        "  <text x=\"960\" y=\"514\" text-anchor=\"middle\" class=\"sans\" font-size=\"52\" font-weight=\"bold\" fill=\"#ffffff\" letter-spacing=\"3\">EVERY WALLPAPER ALIVE.</text>\n"
        "  <text x=\"960\" y=\"568\" text-anchor=\"middle\" class=\"mono\" font-size=\"15\" fill=\"#dcd8d8\" letter-spacing=\"2\">SYNTHESIZED IN PARALLEL PURE ISO C11  .  ZERO STATIC STILLS</text>\n"
        "  <line x1=\"560\" y1=\"604\" x2=\"1360\" y2=\"604\" stroke=\"#ffffff\" stroke-opacity=\"0.16\" stroke-width=\"1\"/>\n"
        "  <text x=\"960\" y=\"646\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" fill=\"#a8a4a4\" letter-spacing=\"1.5\">Zero External Image Libraries  .  120 FPS Bezier Transitions  .  Zero Idle CPU via awww</text>\n");
    write_svg_frame_foot(f);
    fclose(f);

    /* CHAPTER 07: Calm Architectural Finale Card */
    if (snprintf(path, sizeof(path), "%s/ch07.svg", dir) >= (int)sizeof(path)) return 1;
    f = fopen(path, "w");
    if (!f) return 1;
    write_svg_frame_head(f, "07 / 07  .  OPEN SOURCE MIT RELEASE");
    fprintf(f,
        "  <rect x=\"370\" y=\"290\" width=\"1180\" height=\"500\" rx=\"14\" fill=\"#080808\" fill-opacity=\"0.84\" stroke=\"#ffffff\" stroke-opacity=\"0.30\" stroke-width=\"1\"/>\n"
        "  <rect x=\"946\" y=\"338\" width=\"28\" height=\"28\" transform=\"rotate(45 960 352)\" fill=\"none\" stroke=\"#ffffff\" stroke-width=\"1.8\"/>\n"
        "  <circle cx=\"960\" cy=\"352\" r=\"3.5\" fill=\"#ffffff\"/>\n"
        "  <text x=\"960\" y=\"412\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" fill=\"#a8a4a4\" letter-spacing=\"3\">RELEASE v5.3.0  .  100%% PURE ISO C11  .  MIT LICENSE</text>\n"
        "  <text x=\"960\" y=\"488\" text-anchor=\"middle\" class=\"sans\" font-size=\"58\" font-weight=\"bold\" fill=\"#ffffff\" letter-spacing=\"5\">HYPRLAND MONOCHROME</text>\n"
        "  <line x1=\"520\" y1=\"526\" x2=\"1400\" y2=\"526\" stroke=\"#ffffff\" stroke-opacity=\"0.16\" stroke-width=\"1\"/>\n"
        "  <text x=\"960\" y=\"568\" text-anchor=\"middle\" class=\"mono\" font-size=\"14\" fill=\"#d8d4d4\" letter-spacing=\"1.5\">AF_UNIX mono-ipc  .  mono-tui Control Deck  .  4 Cursors  .  20 Live Wallpapers  .  176 Shortcuts</text>\n"
        "  <rect x=\"560\" y=\"606\" width=\"800\" height=\"56\" rx=\"8\" fill=\"#171515\" stroke=\"#ffffff\" stroke-opacity=\"0.42\" stroke-width=\"1\"/>\n"
        "  <text x=\"960\" y=\"640\" text-anchor=\"middle\" class=\"mono\" font-size=\"18\" font-weight=\"bold\" fill=\"#ffffff\" letter-spacing=\"1\">github.com/esoexe/hyprland-monochrome</text>\n"
        "  <text x=\"960\" y=\"718\" text-anchor=\"middle\" class=\"mono\" font-size=\"13\" fill=\"#9e9a9a\" letter-spacing=\"2\">LIVE INTERACTIVE SIMULATOR  .  esoexe.github.io/hyprland-monochrome</text>\n"
        "  <text x=\"960\" y=\"752\" text-anchor=\"middle\" class=\"mono\" font-size=\"12\" fill=\"#787474\" letter-spacing=\"1.5\">Arch Linux  .  CachyOS  .  EndeavourOS  .  Manjaro  .  Universal Wayland Hardware</text>\n");
    write_svg_frame_foot(f);
    fclose(f);

    return 0;
}

static int render_youtube_thumb_svg(const char *svg_out) {
    FILE *f = fopen(svg_out, "w");
    if (!f) return 1;

    fprintf(f,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1920\" height=\"1080\" viewBox=\"0 0 1920 1080\">\n"
        "  <defs>\n"
        "    <linearGradient id=\"leftShade\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\">\n"
        "      <stop offset=\"0%%\" stop-color=\"#050505\" stop-opacity=\"0.95\"/>\n"
        "      <stop offset=\"58%%\" stop-color=\"#070707\" stop-opacity=\"0.82\"/>\n"
        "      <stop offset=\"100%%\" stop-color=\"#080808\" stop-opacity=\"0.38\"/>\n"
        "    </linearGradient>\n"
        "  </defs>\n"
        "  <style>\n"
        "    .sans { font-family: 'DejaVu Sans', sans-serif; }\n"
        "    .mono { font-family: 'DejaVu Sans Mono', monospace; }\n"
        "  </style>\n"
        "  <rect x=\"0\" y=\"0\" width=\"1920\" height=\"1080\" fill=\"url(#leftShade)\"/>\n"
        "  <rect x=\"28\" y=\"28\" width=\"1864\" height=\"1024\" rx=\"10\" fill=\"none\" stroke=\"#ffffff\" stroke-opacity=\"0.24\" stroke-width=\"2\"/>\n"
        /* Top pill */
        "  <rect x=\"88\" y=\"118\" width=\"476\" height=\"52\" rx=\"26\" fill=\"#ffffff\"/>\n"
        "  <text x=\"326\" y=\"152\" text-anchor=\"middle\" class=\"mono\" font-size=\"24\" font-weight=\"bold\" fill=\"#070707\" letter-spacing=\"2\">v5.3.0  /  100%% PURE C11</text>\n"
        /* Massive mobile-legible headline */
        "  <text x=\"84\" y=\"328\" class=\"sans\" font-size=\"134\" font-weight=\"bold\" fill=\"#ffffff\" letter-spacing=\"2\">HYPRLAND</text>\n"
        "  <text x=\"88\" y=\"448\" class=\"sans\" font-size=\"94\" font-weight=\"bold\" fill=\"#dcd8d8\" letter-spacing=\"4\">MONOCHROME</text>\n"
        "  <line x1=\"88\" y1=\"510\" x2=\"930\" y2=\"510\" stroke=\"#ffffff\" stroke-opacity=\"0.28\" stroke-width=\"2\"/>\n"
        /* 3 high-impact spec badges */
        "  <rect x=\"88\" y=\"556\" width=\"390\" height=\"68\" rx=\"10\" fill=\"#141212\" stroke=\"#ffffff\" stroke-opacity=\"0.45\" stroke-width=\"1.5\"/>\n"
        "  <text x=\"283\" y=\"599\" text-anchor=\"middle\" class=\"mono\" font-size=\"25\" font-weight=\"bold\" fill=\"#ffffff\">0 PYTHON  .  0 C++</text>\n"
        "  <rect x=\"502\" y=\"556\" width=\"426\" height=\"68\" rx=\"10\" fill=\"#141212\" stroke=\"#ffffff\" stroke-opacity=\"0.45\" stroke-width=\"1.5\"/>\n"
        "  <text x=\"715\" y=\"599\" text-anchor=\"middle\" class=\"mono\" font-size=\"25\" font-weight=\"bold\" fill=\"#ffffff\">AF_UNIX IPC 0.07ms</text>\n"
        "  <rect x=\"88\" y=\"648\" width=\"840\" height=\"68\" rx=\"10\" fill=\"#111010\" stroke=\"#ffffff\" stroke-opacity=\"0.28\" stroke-width=\"1.5\"/>\n"
        "  <text x=\"508\" y=\"691\" text-anchor=\"middle\" class=\"mono\" font-size=\"23\" fill=\"#e8e4e4\">mono-tui Deck  .  4 Cursors  .  20 Live Wallpapers</text>\n"
        /* Bottom left tag (keeps bottom-right clear of YouTube duration badge!) */
        "  <text x=\"88\" y=\"956\" class=\"mono\" font-size=\"22\" fill=\"#9e9a9a\" letter-spacing=\"2\">OPEN SOURCE MIT  /  ARCH  .  CACHYOS  .  WAYLAND</text>\n"

        /* Right showcase card: mono-tui + bespoke cursor icons */
        "  <rect x=\"1024\" y=\"138\" width=\"808\" height=\"748\" rx=\"16\" fill=\"#090808\" fill-opacity=\"0.92\" stroke=\"#ffffff\" stroke-opacity=\"0.42\" stroke-width=\"2\"/>\n"
        "  <text x=\"1064\" y=\"192\" class=\"mono\" font-size=\"20\" font-weight=\"bold\" fill=\"#ffffff\">mono-tui  //  Pure C11 Control Deck</text>\n"
        "  <rect x=\"1644\" y=\"162\" width=\"148\" height=\"38\" rx=\"6\" fill=\"#ffffff\"/>\n"
        "  <text x=\"1718\" y=\"187\" text-anchor=\"middle\" class=\"mono\" font-size=\"17\" font-weight=\"bold\" fill=\"#080808\">65.3 us</text>\n"
        "  <line x1=\"1056\" y1=\"218\" x2=\"1800\" y2=\"218\" stroke=\"#2f2b2b\" stroke-width=\"1.5\"/>\n"
        /* TUI Tabs */
        "  <rect x=\"1064\" y=\"240\" width=\"214\" height=\"44\" rx=\"6\" fill=\"#ffffff\"/>\n"
        "  <text x=\"1171\" y=\"269\" text-anchor=\"middle\" class=\"mono\" font-size=\"18\" font-weight=\"bold\" fill=\"#080808\">[1:TELEMETRY]</text>\n"
        "  <rect x=\"1292\" y=\"240\" width=\"164\" height=\"44\" rx=\"6\" fill=\"#151313\" stroke=\"#444040\" stroke-width=\"1\"/>\n"
        "  <text x=\"1374\" y=\"269\" text-anchor=\"middle\" class=\"mono\" font-size=\"17\" fill=\"#b8b4b4\">2:SHADERS</text>\n"
        "  <rect x=\"1470\" y=\"240\" width=\"164\" height=\"44\" rx=\"6\" fill=\"#151313\" stroke=\"#444040\" stroke-width=\"1\"/>\n"
        "  <text x=\"1552\" y=\"269\" text-anchor=\"middle\" class=\"mono\" font-size=\"17\" fill=\"#b8b4b4\">3:CURSORS</text>\n"
        "  <rect x=\"1648\" y=\"240\" width=\"144\" height=\"44\" rx=\"6\" fill=\"#151313\" stroke=\"#444040\" stroke-width=\"1\"/>\n"
        "  <text x=\"1720\" y=\"269\" text-anchor=\"middle\" class=\"mono\" font-size=\"17\" fill=\"#b8b4b4\">6:BENCH</text>\n"
        /* Telemetry bars */
        "  <text x=\"1064\" y=\"342\" class=\"mono\" font-size=\"19\" font-weight=\"bold\" fill=\"#ffffff\">CPU</text>\n"
        "  <rect x=\"1138\" y=\"322\" width=\"510\" height=\"22\" rx=\"4\" fill=\"#1c1a1a\"/>\n"
        "  <rect x=\"1138\" y=\"322\" width=\"142\" height=\"22\" rx=\"4\" fill=\"#ffffff\"/>\n"
        "  <text x=\"1672\" y=\"342\" class=\"mono\" font-size=\"19\" font-weight=\"bold\" fill=\"#ffffff\">14%%</text>\n"
        "  <text x=\"1064\" y=\"394\" class=\"mono\" font-size=\"19\" font-weight=\"bold\" fill=\"#ffffff\">RAM</text>\n"
        "  <rect x=\"1138\" y=\"374\" width=\"510\" height=\"22\" rx=\"4\" fill=\"#1c1a1a\"/>\n"
        "  <rect x=\"1138\" y=\"374\" width=\"168\" height=\"22\" rx=\"4\" fill=\"#d8d4d4\"/>\n"
        "  <text x=\"1672\" y=\"394\" class=\"mono\" font-size=\"19\" fill=\"#d8d4d4\">4.1G</text>\n"
        /* Wire box */
        "  <rect x=\"1064\" y=\"434\" width=\"728\" height=\"148\" rx=\"10\" fill=\"#111010\" stroke=\"#383434\" stroke-width=\"1.5\"/>\n"
        "  <text x=\"1092\" y=\"476\" class=\"mono\" font-size=\"18\" font-weight=\"bold\" fill=\"#ffffff\">$ mono-ipc raw \"j/version\"</text>\n"
        "  <text x=\"1092\" y=\"514\" class=\"mono\" font-size=\"17\" fill=\"#cfcbcb\">{ \"tag\": \"v0.56.0\", \"socket\": \".socket.sock\" }</text>\n"
        "  <text x=\"1092\" y=\"554\" class=\"mono\" font-size=\"17\" fill=\"#9e9a9a\">0 jq subprocesses  .  12 Pure ISO C11 binaries</text>\n"
        /* 3 Bold Cursor Icons row */
        "  <rect x=\"1064\" y=\"608\" width=\"230\" height=\"236\" rx=\"12\" fill=\"#131111\" stroke=\"#ffffff\" stroke-opacity=\"0.4\" stroke-width=\"1.5\"/>\n"
        "  <polygon points=\"1146,638 1146,748 1178,718 1200,760 1215,751 1193,709 1235,709\" fill=\"#0a0a0a\" stroke=\"#ffffff\" stroke-width=\"5\" stroke-linejoin=\"round\"/>\n"
        "  <text x=\"1179\" y=\"812\" text-anchor=\"middle\" class=\"sans\" font-size=\"22\" font-weight=\"bold\" fill=\"#ffffff\">Obsidian</text>\n"

        "  <rect x=\"1313\" y=\"608\" width=\"230\" height=\"236\" rx=\"12\" fill=\"#131111\" stroke=\"#3e3a3a\" stroke-width=\"1.5\"/>\n"
        "  <polygon points=\"1395,638 1395,748 1427,718 1449,760 1464,751 1442,709 1484,709\" fill=\"#f7f7f7\" stroke=\"#0a0a0a\" stroke-width=\"5\" stroke-linejoin=\"round\"/>\n"
        "  <text x=\"1428\" y=\"812\" text-anchor=\"middle\" class=\"sans\" font-size=\"22\" font-weight=\"bold\" fill=\"#ffffff\">Ivory</text>\n"

        "  <rect x=\"1562\" y=\"608\" width=\"230\" height=\"236\" rx=\"12\" fill=\"#131111\" stroke=\"#3e3a3a\" stroke-width=\"1.5\"/>\n"
        "  <circle cx=\"1677\" cy=\"700\" r=\"40\" fill=\"none\" stroke=\"#f5f5f5\" stroke-width=\"4.5\"/>\n"
        "  <circle cx=\"1677\" cy=\"700\" r=\"15\" fill=\"#ffffff\"/>\n"
        "  <text x=\"1677\" y=\"812\" text-anchor=\"middle\" class=\"sans\" font-size=\"22\" font-weight=\"bold\" fill=\"#ffffff\">Dot</text>\n"
        "</svg>\n");

    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    const char *audio_out = NULL;
    const char *plates_dir = NULL;
    const char *thumb_out = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
            audio_out = argv[++i];
        } else if (strcmp(argv[i], "--plates") == 0 && i + 1 < argc) {
            plates_dir = argv[++i];
        } else if (strcmp(argv[i], "--thumbnail") == 0 && i + 1 < argc) {
            thumb_out = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [--audio calm_score.wav] [--plates out_dir] [--thumbnail thumb.svg]\n", argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (!audio_out && !plates_dir && !thumb_out) {
        fprintf(stderr, "Usage: %s [--audio calm_score.wav] [--plates out_dir] [--thumbnail thumb.svg]\n", argv[0]);
        return 1;
    }

    if (audio_out) {
        if (render_calm_score(audio_out) != 0) {
            fprintf(stderr, "Failed to render calm score: %s\n", audio_out);
            return 1;
        }
        printf("synthesized 60.0s 48kHz stereo calm felt-piano & ambient hall score -> %s\n", audio_out);
    }

    if (plates_dir) {
        if (render_svg_plates(plates_dir) != 0) {
            fprintf(stderr, "Failed to render SVG plates in: %s\n", plates_dir);
            return 1;
        }
        printf("rendered 7 architectural 1920x1080 SVG plates -> %s/ch01..ch07.svg\n", plates_dir);
    }

    if (thumb_out) {
        if (render_youtube_thumb_svg(thumb_out) != 0) {
            fprintf(stderr, "Failed to render YouTube thumbnail SVG: %s\n", thumb_out);
            return 1;
        }
        printf("rendered 1920x1080 YouTube thumbnail SVG -> %s\n", thumb_out);
    }

    return 0;
}
