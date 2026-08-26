#include <stdio.h>
#include <raylib.h>
#include <math.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE 32
#define CHANNELS 1
#define ROOT_NOOT 440.0
#define NEXT_SEMITONE 1.0594630943592953f

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

float semitone_to_frequency(int semitone)
{
    return ROOT_NOOT*pow(NEXT_SEMITONE, semitone);
}

typedef struct Note {
    float frequency;
    int frame_count;
} Note;

void note_update(Note *note, float amp, float *buffer, size_t size)
{
    for(int i = 0; i < size; i++)
    {
        float time = (float)note->frame_count/SAMPLE_RATE;
        buffer[i] += (float)sin(2*M_PI*time*note->frequency)*amp;
        note->frame_count += 1;
    }
}

static inline Note note(float semitone) {
    Note n;
    n.frequency = semitone_to_frequency(semitone);
    n.frame_count = 0;
    return n;
}

int main(void)
{
    InitWindow(800, 600, "synth");
    InitAudioDevice();

    float buffer[1024];
    SetAudioStreamBufferSizeDefault(ARRAY_LEN(buffer));

    AudioStream synth = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, CHANNELS);

    PlayAudioStream(synth);
    Note* notes = NULL;

    arrpush(notes, note(0));
    arrpush(notes, note(4));
    arrpush(notes, note(7));

    SetTargetFPS(60);

    while(!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(GetColor(0x181818AA));

        while (IsAudioStreamProcessed(synth))
        {
            for(int i = 0; i < ARRAY_LEN(buffer); i++)
            {
                buffer[i] = 0.0f;
            }

            for(int i = 0; i < arrlen(notes); i++)
            {
                note_update(&notes[i], 1.0/arrlen(notes), buffer, ARRAY_LEN(buffer));
            }

            for(int i = 0; i < ARRAY_LEN(buffer); i++)
            {
                buffer[i] = CLAMP(buffer[i], -1.0f, 1.0f);
            }
            UpdateAudioStream(synth, buffer, ARRAY_LEN(buffer));
        }
        EndDrawing();
    }

    arrfree(notes);
    CloseWindow();
    UnloadAudioStream(synth);
    CloseAudioDevice();
    return 0;
}
