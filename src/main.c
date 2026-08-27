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
    return ROOT_NOOT*pow(NEXT_SEMITONE, semitone);
}

void note_update(int frame_count, float frequency, float amp, float *buffer, size_t size)
{
    for(int i = 0; i < size; i++)
    {
        float time = (float)(frame_count+i)/SAMPLE_RATE;
        buffer[i] += (float)sin(2*M_PI*time*frequency)*amp;
    }
}

const KeyboardKey MY_KEYS[] = {
    KEY_Z,
    KEY_S,
    KEY_X,
    KEY_D,
    KEY_C,
    KEY_V,
    KEY_G,
    KEY_B,
    KEY_H,
    KEY_N,
    KEY_J,
    KEY_M,
    KEY_COMMA
};

bool notes[ARRAY_LEN(MY_KEYS)];

typedef int Quant;

typedef struct Event {
    Quant timestamp;
    bool start;
    int semitone;
} Event;

int main(void)
{
    InitWindow(800, 600, "synth");
    InitAudioDevice();
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    float buffer[1024];
    SetAudioStreamBufferSizeDefault(ARRAY_LEN(buffer));

    AudioStream synth = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, CHANNELS);

    PlayAudioStream(synth);

    Sound beat = LoadSound("plant-bomb.wav");

    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    int frame_count = 0;
    float beat_time = 0.0f;
    bool recording = false;

    typedef enum STATE {
        REPLAY,
        WAITING_UNTIL_END_OF_BAR,
        RECORD,
    } STATE;

    STATE current_state = REPLAY;

    Event *events = NULL;

    while(!WindowShouldClose()) {
        int quant = (int)(beat_time/QUANT_SECS);

        double beat_time_prev = beat_time;
        beat_time += GetFrameTime();

        if (fmod(beat_time, BEAT_SECS) < fmod(beat_time_prev, BEAT_SECS)) {
            PlaySound(beat);
        }

        if (current_state == WAITING_UNTIL_END_OF_BAR) {
            if (fmod(beat_time, BAR_SECS) < fmod(beat_time_prev, BAR_SECS)) {
                current_state = RECORD;
                beat_time = 0;
                arrsetlen(events, 0);
            }
        }

        if (IsKeyPressed(KEY_SPACE)) {
            switch (current_state)
            {
                case REPLAY:
                    current_state = WAITING_UNTIL_END_OF_BAR;
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

        int notes_playing = 0;
        for (int i = 0; i < ARRAY_LEN(notes); i++)
        {
            if (IsKeyDown(MY_KEYS[i])) {
                if (!notes[i]) {
                    notes[i] = true;
                    if (current_state == RECORD) {
                        arrput(events, ((Event){
                            .timestamp = quant,
                            .start = true,
                            .semitone = i
                        }));
                    }
                }
            } else {
                if (notes[i]) {
                    notes[i] = false;
                    if (current_state == RECORD) {
                        arrput(events, ((Event){
                            .timestamp = quant,
                            .start = false,
                            .semitone = i
                        }));
                    }
                }
            }
            if (notes[i]) {notes_playing += 1;}
        }

        while (IsAudioStreamProcessed(synth))
        {
            for (int i = 0; i < ARRAY_LEN(buffer); i++)
            {
                buffer[i] = 0.0f;
            }
            if (notes_playing > 0) {
                for(int i = 0; i < ARRAY_LEN(notes); i++)
                {
                    if(notes[i]) {
                        note_update(frame_count, semitone_to_frequency(i), 1.0f /notes_playing, buffer, ARRAY_LEN(buffer));
                    }
                }

                for(int i = 0; i < ARRAY_LEN(buffer); i++)
                {
                    buffer[i] = CLAMP(buffer[i], -1.0f, 1.0f);
                }
            }
            frame_count += ARRAY_LEN(buffer);
            UpdateAudioStream(synth, buffer, ARRAY_LEN(buffer));
        }
        BeginDrawing();
        ClearBackground(GetColor(0x181818FF));
        Vector2 center = {GetScreenWidth() - 75.0f, 75.0f};
        float radius = 25.0f;
        Color color = RED;
        switch (current_state)
        {
            case REPLAY:
                DrawRing(center, radius*0.8, radius, 0.0f, 360.f, 100, WHITE);
                break;
            case WAITING_UNTIL_END_OF_BAR:
                DrawCircleV(center, radius, BLUE);
                break;
            case RECORD:
                DrawCircleV(center, radius, RED);
                break;
        }
        float beat_length = (float)GetScreenWidth()/BAR_BEATS;
        for (int i = 1; i < BAR_BEATS; ++i) {
            DrawLineV((Vector2){i*beat_length, 0},(Vector2){i*beat_length, (float)GetScreenHeight()}, GRAY);
        }
        double x = fmod(beat_time, BAR_SECS)/BAR_SECS*GetScreenWidth();
        DrawLineV((Vector2){(float)x, 0}, (Vector2){(float)x, (float)GetScreenHeight()}, WHITE);
        EndDrawing();
    }

    CloseWindow();
    UnloadAudioStream(synth);
    CloseAudioDevice();
    return 0;
}
