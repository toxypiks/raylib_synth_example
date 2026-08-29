#include <stdio.h>
#include <stdbool.h>
#include <raylib.h>
#include <math.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

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

float note_update(int frame_count, float frequency)
{
    float time = (float)frame_count/ SAMPLE_RATE;
    return (float)sin(2 * M_PI * time * frequency);
}

const KeyboardKey MY_KEYS[] = {
    KEY_Z, KEY_S, KEY_X, KEY_D, KEY_C, KEY_V, KEY_G,
    KEY_B, KEY_H, KEY_N, KEY_J, KEY_M, KEY_COMMA
};

typedef struct Note {
    bool playing;
    int frame_stamp;
} Note;

Note notes_replay[ARRAY_LEN(MY_KEYS)];
Note notes_monitor[ARRAY_LEN(MY_KEYS)];

void note_play(Note *note, int frame_count)
{
    note->playing = true;
    note->frame_stamp = frame_count;
}

void note_stop(Note *note)
{
    note->playing = false;
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
                                note_play(&notes_replay[events[i].semitone], frame_count);
                            }
                            else {
                                note_stop(&notes_replay[events[i].semitone]);
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
                        note_stop(&notes_replay[i]);
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
                    note_play(&notes_monitor[i], frame_count);
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
                    note_stop(&notes_monitor[i]);
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

                if (notes_playing > 0) {
                    float amplitude = 1.0f / notes_playing;

                    for (int note_idx = 0; note_idx < ARRAY_LEN(notes_monitor); ++note_idx) {
                        if (notes_monitor[note_idx].playing) {
                            buffer[sample_idx] += note_update(frame_count, semitone_to_frequency(note_idx)) * amplitude;
                        }
                    }
                    for (int note_idx = 0; note_idx < ARRAY_LEN(notes_replay); ++note_idx) {
                        if (notes_replay[note_idx].playing) {
                            buffer[sample_idx] += note_update(frame_count, semitone_to_frequency(note_idx)) * amplitude;
                        }
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
