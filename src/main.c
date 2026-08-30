#include <stdio.h>
#include <stdbool.h>
#include <raylib.h>
#include <math.h>

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

float semitone_to_frequency(int semitone)
{
    return ROOT_NOOT * pow(NEXT_SEMITONE, semitone);
}

const KeyboardKey MY_KEYS[] = {
    KEY_Z, KEY_S, KEY_X, KEY_D, KEY_C, KEY_V, KEY_G,
    KEY_B, KEY_H, KEY_N, KEY_J, KEY_M, KEY_COMMA
};

typedef struct Instrument {
    void *instrument_data; // generic pointer
    float (*instrument)(float x, void *data); // pointer to function
} Instrument;

typedef struct Note {
    bool playing;
    int frame_stamp;
    Instrument instrument;
} Note;

float function_sine(float x, void *data)
{
    return sinf(x* 2.0f * (float)M_PI);
}

Instrument instrument_sine = {
    .instrument_data = NULL,
    .instrument = function_sine
};

float function_square(float x, void *data)
{
    // p tetermines the size of the wave below zero
    float *p = (float*)data;
    float x_frac = fmodf(x, 1.0f); // values between 0.0f and 1.0f
    if ( x_frac < *p) {return 1.0f;}
    return -1.0f;
}

static float square_p = 0.5f;

Instrument instrument_square = {
    .instrument_data = &square_p,
    .instrument = function_square
};

float function_sawtooth(float x, void *data)
{
    float *p = (float*)data;
    float x_frac = fmodf(x, 1.0f);

    // from -1.0f to 1.0f
    if (x_frac <= *p) {
        return LERP(-1.0f, 1.0f, x_frac / (*p));
    }

    // from 1.0f to -1.0f
    return LERP(1.0f, -1.0f, (x_frac - *p) / (1.0f - *p));
}

// call instrument function with instrument data as parameter
float instrument_run(Instrument *instrument, float x)
{
    return instrument->instrument(x, instrument->instrument_data);
}

typedef struct NoteReleased {
    int frame_stamp;
    float frequency;
    float volume;
    Instrument instrument;
} NoteReleased;

Note notes_replay[ARRAY_LEN(MY_KEYS)];
Note notes_monitor[ARRAY_LEN(MY_KEYS)];
NoteReleased *notes_released = NULL;

const int RELEASE_FRAME = 10000;

float note_released_update(NoteReleased *note_released, int frame_count)
{
    float volume = (1 - MIN((float)(frame_count - note_released->frame_stamp)/RELEASE_FRAME, 1.0f))*note_released->volume;
    float time = (float)frame_count/ SAMPLE_RATE;
    return (float)sin(2 * M_PI * time * note_released->frequency)*volume;
}

bool note_released_done(NoteReleased *note_released, int frame_count)
{
    return frame_count - note_released->frame_stamp >= RELEASE_FRAME;
}

const int ATTACK_FRAME = 10000;

float note_update(Note *note, int frame_count, float frequency)
{
    // calc for how long the note has been playing: frame_count - note->frame_stamp
    // the longer the note has been played the louder it gets with limit 1.0
    float volume = MIN((float)(frame_count - note->frame_stamp)/ATTACK_FRAME, 1.0f);
    float time = (float)frame_count/ SAMPLE_RATE;
    return (float)sin(2 * M_PI * time * frequency)*volume;
}

void note_press(Note *note, int frame_count)
{
    note->playing = true;
    note->frame_stamp = frame_count;
}

void note_release(Note *note, float frequency, int frame_count)
{
    if (note->playing) {
        note->playing = false;
        float volume = MIN((float)(frame_count - note->frame_stamp)/ATTACK_FRAME, 1.0f);
        NoteReleased note_released = {.frame_stamp = frame_count, .frequency = frequency, .volume = volume};
        arrput(notes_released, note_released);
    }
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

int main(void)
{
    InitWindow(800, 600, "synth");
    InitAudioDevice();
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);

    float buffer[1024];
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

    while (!WindowShouldClose()) {
        int quant = (int)(beat_time / QUANT_SECS);

        double beat_time_prev = beat_time;
        beat_time += GetFrameTime();

        // metronome click
        if (fmod(beat_time, BEAT_SECS) < fmod(beat_time_prev, BEAT_SECS)) {
            PlaySound(beat);
        }

        // state machine
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
                                note_press(&notes_replay[events[i].semitone], frame_count);
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
                                .semitone = i
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
                    for (int i = 0; i < arrlen(events); ++i) {
                        printf("Event %d: Timestamp=%d, Start=%s, Semitone=%d\n",
                            i,
                            events[i].timestamp,
                            events[i].start ? "true" : "false",
                            events[i].semitone);
                    }
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
                    note_press(&notes_monitor[i], frame_count);
                    if (current_state == RECORD) {
                        arrput(events, ((Event){
                            .timestamp = quant,
                            .start = true,
                            .semitone = i
                        }));
                    }
                }
            } else {
                if (notes_monitor[i].playing) {
                    note_release(&notes_monitor[i], semitone_to_frequency(i),frame_count);
                    if (current_state == RECORD) {
                        arrput(events, ((Event){
                            .timestamp = quant,
                            .start = false,
                            .semitone = i
                        }));
                    }
                }
            }
        }

        while (IsAudioStreamProcessed(synth))
        {
            for (int sample_idx = 0; sample_idx < ARRAY_LEN(buffer); ++sample_idx)
            {
                buffer[sample_idx] = 0.0f;
                int notes_playing = 0;

                for (int note_idx = 0; note_idx < ARRAY_LEN(notes_monitor); ++note_idx) {
                    if (notes_monitor[note_idx].playing) {
                        notes_playing += 1;
                    }
                }

                for (int note_idx = 0; note_idx < ARRAY_LEN(notes_replay); ++note_idx) {
                    if (notes_replay[note_idx].playing) {
                        notes_playing += 1;
                    }
                }

                // O(1) delete element out of array when order of elements dont matter
                for (int i = arrlen(notes_released) - 1; i >= 0; --i) {
                    if (note_released_done(&notes_released[i], frame_count)) {
                        notes_released[i] = arrlast(notes_released); // copy last element to index of released note
                        arrpop(notes_released);                      // delete last element -> arraylen - 1
                    } else {
                        notes_playing += arrlen(notes_released);
                    }
                }

                if (notes_playing > 0) {
                    float amplitude = 1.0f / notes_playing;

                    for (int note_idx = 0; note_idx < ARRAY_LEN(notes_monitor); ++note_idx) {
                        if (notes_monitor[note_idx].playing) {
                            buffer[sample_idx] += note_update(&notes_monitor[note_idx], frame_count, semitone_to_frequency(note_idx)) * amplitude;
                        }
                    }
                    for (int note_idx = 0; note_idx < ARRAY_LEN(notes_replay); ++note_idx) {
                        if (notes_replay[note_idx].playing) {
                            buffer[sample_idx] += note_update(&notes_replay[note_idx], frame_count, semitone_to_frequency(note_idx)) * amplitude;
                        }
                    }
                    for (int note_idx = 0; note_idx < arrlen(notes_released); ++note_idx) {
                        buffer[sample_idx] += note_released_update(&notes_released[note_idx], frame_count) * amplitude;
                    }
                }
                buffer[sample_idx] = CLAMP(buffer[sample_idx], -1.0f, 1.0f);
                frame_count += 1;
            }
            UpdateAudioStream(synth, buffer, ARRAY_LEN(buffer));
        }


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
        EndDrawing();
    }

    UnloadSound(beat);
    UnloadAudioStream(synth);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
