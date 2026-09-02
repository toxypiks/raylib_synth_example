#include <stdio.h>
#include <stdbool.h>
#include <raylib.h>
#include <math.h>
#include <complex.h>
#include <string.h>
#include <assert.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LERP(a, b, t) ((a) + (t) * ((b) - (a)))

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE 32
#define CHANNELS 1
#define ROOT_NOOT 440.0
#define NEXT_SEMITONE 1.0594630943592953f

#define BPM 120
#define BEAT_SECS (60.0f / BPM)
#define BAR_BEATS 4
#define BAR_SECS (BAR_BEATS * BEAT_SECS)
#define BAR_QUANT 32
#define QUANT_SECS (BAR_SECS / BAR_QUANT)

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// fft settings
#define N (1<<11) // 2048 samples

float in_raw[N];
float in_win[N];
float complex out_raw[N];
float out_log[N];
float out_smooth[N];

void fft_push(float frame) {
    memmove(in_raw, in_raw + 1, (N - 1) * sizeof(in_raw[0]));
    in_raw[N - 1] = frame;
}

void fft(float in[], size_t stride, float complex out[], size_t n) {
    assert(n > 0);
    if (n == 1) {
        out[0] = in[0];
        return;
    }
    fft(in, stride * 2, out, n / 2);
    fft(in + stride, stride * 2, out + n / 2, n / 2);

    for (size_t k = 0; k < n / 2; ++k) {
        float t = (float)k / n;
        float complex v = cexp(-2 * I * PI * t) * out[k + n / 2];
        float complex e = out[k];
        out[k]       = e + v;
        out[k + n / 2] = e - v;
    }
}

static inline float amp(float complex z) {
    float a = crealf(z);
    float b = cimagf(z);
    return logf(a * a + b * b + 1e-6f);
}

size_t fft_analyze(float dt) {
    // hann window
    for (size_t i = 0; i < N; ++i) {
        float t = (float)i / (N - 1);
        float hann = 0.5f - 0.5f * cosf(2 * PI * t);
        in_win[i] = in_raw[i] * hann;
    }

    fft(in_win, 1, out_raw, N);

    // logarithmic scaling
    float step = 1.06f;
    float lowf = 1.0f;
    size_t m = 0;
    float max_amp = 1.0f;

    for (float f = lowf; (size_t)f < N / 2; f = ceilf(f * step)) {
        float f1 = ceilf(f * step);
        float a = 0.0f;
        for (size_t q = (size_t)f; q < N / 2 && q < (size_t)f1; ++q) {
            float b = amp(out_raw[q]);
            if (b > a) a = b;
        }
        if (max_amp < a) max_amp = a;
        out_log[m++] = a;
    }

    // normalize and smooth out
    for (size_t i = 0; i < m; ++i) {
        out_log[i] /= max_amp;
        float smoothness = 12.0f;
        out_smooth[i] += (out_log[i] - out_smooth[i]) * smoothness * dt;
    }

    return m;
}

void fft_render(int w, int h, size_t m) {
    float cell_width = (float)w / m;

    for (size_t i = 0; i < m; ++i) {
        float val = CLAMP(out_smooth[i], 0.0f, 1.0f);
        float hue = (float)i / m * 360.0f;
        Color color = ColorFromHSV(hue, 0.75f, 1.0f);

        float bar_height = h * 0.4f * val; // bar height 40% of window height

        Rectangle bar = {
            .x = i * cell_width,
            .y = h - bar_height,
            .width = cell_width - 1.0f,
            .height = bar_height
        };

        DrawRectangleRec(bar, color);
    }
}

// synthesizer engine

double semitone_to_frequency(double semitone)
{
    return ROOT_NOOT * pow(NEXT_SEMITONE, semitone);
}

const KeyboardKey MY_KEYS[] = {
    KEY_Z, KEY_S, KEY_X, KEY_D, KEY_C, KEY_V, KEY_G,
    KEY_B, KEY_H, KEY_N, KEY_J, KEY_M, KEY_COMMA
};

typedef struct Instrument Instrument;

// function pointer type definition
typedef double (*InstrumentRunFunc)(Instrument *this, double x);

struct Instrument {
    InstrumentRunFunc run;
    double p;
    double tremolo_frequency;
    Instrument *applied_to;
};

typedef struct Note {
    bool playing;
    int frame_stamp;
    Instrument instrument;
} Note;

// wave forms and modulation

double instrument_sine_run(Instrument *this, double x) {
    return sin(x * 2.0 * M_PI);
}

double instrument_square_run(Instrument *this, double x) {
    double x_frac = fmod(x, 1.0);
    if (x_frac < 0.0) x_frac += 1.0;
    double duty = (this->p > 0.0) ? this->p : 0.5;
    return (x_frac < duty) ? 0.3 : -0.3;
}

double instrument_sawtooth_run(Instrument *this, double x) {
    double x_frac = fmod(x, 1.0);
    if (x_frac < 0.0) x_frac += 1.0;
    double p = (this->p > 0.001 && this->p < 0.999) ? this->p : 0.5;

    if (x_frac <= p) {
        return LERP(-1.0f, 1.0f, x_frac / p);
    }
    return LERP(1.0f, -1.0f, (x_frac - p) / (1.0f - p));
}

double instrument_tremolo_run(Instrument *this, double x) {
    if (!this->applied_to || this->applied_to == this) return 0.0;

    double volume = (sin(x * 2.0 * M_PI/this->tremolo_frequency) + 1.0) * 0.5;
    return this->applied_to->run(this->applied_to, x) * volume;
}

Instrument my_sine = {
    .run = instrument_sine_run
};

// example instruments
Instrument my_square = {
    .run = instrument_square_run,
    .p = 0.5
};

Instrument my_tremolo = {
    .run = instrument_tremolo_run,
    .tremolo_frequency = 50.0, // Hz Tremolo
    .applied_to = &my_sine
};

typedef struct NoteReleased {
    int note_frame_stamp;
    int release_frame_stamp;
    float frequency;
    float start_volume;
    Instrument instrument;
} NoteReleased;

Note notes_replay[ARRAY_LEN(MY_KEYS)];
Note notes_monitor[ARRAY_LEN(MY_KEYS)];
NoteReleased *notes_released = NULL;

const int ATTACK_FRAME = 220;   // ~5ms Attack
const int RELEASE_FRAME = 2200; // ~50ms Release

float note_released_update(NoteReleased *note_released, int frame_count)
{
    // fade out based on time when released
    float release_progress = (float)(frame_count - note_released->release_frame_stamp) / RELEASE_FRAME;
    float volume = (1.0f - CLAMP(release_progress, 0.0f, 1.0f)) * note_released->start_volume;

    float local_time = (float)(frame_count - note_released->note_frame_stamp) / SAMPLE_RATE;

    if (note_released->instrument.run) {
        return note_released->instrument.run(&note_released->instrument, local_time * note_released->frequency) * volume;
    }
    return 0.0f;
}

bool note_released_done(NoteReleased *note_released, int frame_count)
{
    return frame_count - note_released->release_frame_stamp >= RELEASE_FRAME;
}

void note_release(Note *note, float frequency, int frame_count)
{
    if (note->playing) {
        note->playing = false;
        int age = frame_count - note->frame_stamp;
        float current_vol = MIN((float)age / ATTACK_FRAME, 1.0f);

        NoteReleased note_released = {
            .note_frame_stamp = note->frame_stamp,
            .release_frame_stamp = frame_count,
            .frequency = frequency,
            .start_volume = current_vol,
            .instrument = note->instrument
        };
        arrput(notes_released, note_released);
    }
}

float note_update(Note *note, int frame_count, float frequency)
{
    int age = frame_count - note->frame_stamp;
    float volume = MIN((float)age / ATTACK_FRAME, 1.0f);

    float local_time = (float)age / SAMPLE_RATE;

    if (note->instrument.run) {
        return note->instrument.run(&note->instrument, local_time * frequency) * volume;
    }
    return 0.0f;
}

void note_press(Note *note, int frame_count, Instrument instrument)
{
    note->playing = true;
    note->frame_stamp = frame_count;
    note->instrument = instrument;
}

float soft_clip(float x) {
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x - (x * x * x) / 3.0f;
}

typedef int Quant;

typedef struct Event {
    Quant timestamp;
    bool start;
    int semitone;
} Event;

typedef enum STATE {
    REPLAY,
    WAITING_UNTIL_END_OF_BAR,
    RECORD,
} STATE;

int main(void) {
    InitWindow(800, 600, "synth");
    InitAudioDevice();
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);

    float buffer[1024*2];
    SetAudioStreamBufferSizeDefault(ARRAY_LEN(buffer));

    AudioStream synth = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, CHANNELS);
    PlayAudioStream(synth);

    Sound beat = LoadSound("plant-bomb.wav");

    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    int frame_count = 0;
    float beat_time = 0.0f;
    STATE current_state = REPLAY;

    Event *events = NULL;

    // initialize instrument with pointer to function
    /*Instrument instrument_current = {
        .run = instrument_sawtooth_run,
        .p = 0.5
    };
    */
    Instrument instrument_current = my_sine;

    while (!WindowShouldClose()) {
        int quant = (int)(beat_time / QUANT_SECS);

        double beat_time_prev = beat_time;
        beat_time += GetFrameTime();

        if (fmod(beat_time, BEAT_SECS) < fmod(beat_time_prev, BEAT_SECS)) {
            if (current_state == RECORD) {
                PlaySound(beat);
            }
        }

        switch (current_state)
        {
            case REPLAY:
                if (arrlen(events) > 0) {
                    int last_timestamp = events[arrlen(events) - 1].timestamp;
                    int total_quants = ((last_timestamp + BAR_QUANT) / BAR_QUANT) * BAR_QUANT;
                    int quant_we_have_to_play = quant % total_quants;

                    for (int i = 0; i < arrlen(events); ++i) {
                        if (events[i].timestamp == quant_we_have_to_play) {
                            if (events[i].start) {
                                note_press(&notes_replay[events[i].semitone], frame_count, instrument_current);
                            }
                            else {
                                note_release(&notes_replay[events[i].semitone], semitone_to_frequency(events[i].semitone), frame_count);
                            }
                        }
                    }
                }
                break;

            case WAITING_UNTIL_END_OF_BAR:
                if (fmod(beat_time, BAR_SECS) < fmod(beat_time_prev, BAR_SECS)) {
                    current_state = RECORD;
                    beat_time = 0.0f;
                    quant = 0;
                    for (int i = 0; i < ARRAY_LEN(notes_monitor); ++i) {
                        if (notes_monitor[i].playing) {
                            arrput(events, ((Event){
                                .timestamp = quant,
                                .start = true,
                                .semitone = i,
                            }));
                        }
                    }
                }
                break;

            case RECORD:
                break;
        }

        if (IsKeyPressed(KEY_SPACE)) {
            switch (current_state)
            {
                case REPLAY:
                    current_state = WAITING_UNTIL_END_OF_BAR;
                    arrsetlen(events, 0);
                    for (int i = 0; i < ARRAY_LEN(notes_replay); ++i) {
                        note_release(&notes_replay[i], semitone_to_frequency(i), frame_count);
                    }
                    break;
                case RECORD:
                    current_state = REPLAY;
                    break;
                case WAITING_UNTIL_END_OF_BAR:
                    current_state = REPLAY;
                    break;
            }
        }

        for (int i = 0; i < ARRAY_LEN(notes_monitor); i++)
        {
            if (IsKeyDown(MY_KEYS[i])) {
                if (!notes_monitor[i].playing) {
                    note_press(&notes_monitor[i], frame_count, instrument_current);
                    if (current_state == RECORD) {
                        arrput(events, ((Event){
                            .timestamp = quant,
                            .start = true,
                            .semitone = i,
                        }));
                    }
                }
            } else {
                if (notes_monitor[i].playing) {
                    note_release(&notes_monitor[i], semitone_to_frequency(i), frame_count);
                    if (current_state == RECORD) {
                        arrput(events, ((Event){
                            .timestamp = quant,
                            .start = false,
                            .semitone = i,
                        }));
                    }
                }
            }
        }

        // fill audio buffer
        while (IsAudioStreamProcessed(synth)) {
             for (int sample_idx = 0; sample_idx < ARRAY_LEN(buffer); ++sample_idx)
            {
                float mixed_sample = 0.0f;

                // active notes
                for (int note_idx = 0; note_idx < ARRAY_LEN(notes_monitor); ++note_idx) {
                    if (notes_monitor[note_idx].playing) {
                        mixed_sample += note_update(&notes_monitor[note_idx], frame_count, semitone_to_frequency(note_idx));
                    }
                }

                for (int note_idx = 0; note_idx < ARRAY_LEN(notes_replay); ++note_idx) {
                    if (notes_replay[note_idx].playing) {
                        mixed_sample += note_update(&notes_replay[note_idx], frame_count, semitone_to_frequency(note_idx));
                    }
                }

                // released notes
                for (int i = arrlen(notes_released) - 1; i >= 0; --i) {
                    mixed_sample += note_released_update(&notes_released[i], frame_count);
                    if (note_released_done(&notes_released[i], frame_count)) {
                        notes_released[i] = arrlast(notes_released);
                        arrpop(notes_released);
                    }
                }

                mixed_sample *= 0.25f;
                float final_sample = soft_clip(mixed_sample);
                buffer[sample_idx] = final_sample;

                fft_push(final_sample);

                frame_count += 1;
            }
            UpdateAudioStream(synth, buffer, ARRAY_LEN(buffer));
        }

        size_t m = fft_analyze(GetFrameTime());

        BeginDrawing();
        ClearBackground(GetColor(0x181818FF));
         Vector2 center = {GetScreenWidth() - 75.0f, 75.0f};
            float radius = 25.0f;

            switch (current_state)
            {
                case REPLAY:
                    DrawRing(center, radius * 0.8f, radius, 0.0f, 360.0f, 100, WHITE);
                    break;
                case WAITING_UNTIL_END_OF_BAR:
                    DrawCircleV(center, radius, BLUE);
                    break;
                case RECORD:
                    DrawCircleV(center, radius, RED);
                    break;
            }

            float beat_length = (float)GetScreenWidth() / BAR_BEATS;
            for (int i = 1; i < BAR_BEATS; ++i) {
                DrawLineV((Vector2){i * beat_length, 0}, (Vector2){i * beat_length, (float)GetScreenHeight()}, GRAY);
            }

            double x = fmod(beat_time, BAR_SECS) / BAR_SECS * GetScreenWidth();
            DrawLineV((Vector2){(float)x, 0}, (Vector2){(float)x, (float)GetScreenHeight()}, WHITE);

            fft_render(GetScreenWidth(), GetScreenHeight(), m);

        EndDrawing();
    }

    UnloadSound(beat);
    UnloadAudioStream(synth);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
