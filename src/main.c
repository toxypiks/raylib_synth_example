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

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

float semitone_to_frequency(int semitone)
{
    return ROOT_NOOT*pow(NEXT_SEMITONE, semitone);
}

typedef struct Note {
    bool playing;
    float frequency;
    int frame_count;
} Note;

void note_update(Note *note, float amp, float *buffer, size_t size)
{
    if(!note->playing)return;
    for(int i = 0; i < size; i++)
    {
        float time = (float)note->frame_count/SAMPLE_RATE;
        buffer[i] += (float)sin(2*M_PI*time*note->frequency)*amp;
        note->frame_count += 1;
    }
}

const KeyboardKey MY_KEYS[] = {
    KEY_Y,
    KEY_S,
    KEY_X,
    KEY_D,
    KEY_C
};

Note notes[ARRAY_LEN(MY_KEYS)];

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

    SetTargetFPS(60);

    while(!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(GetColor(0x181818AA));

        int notes_playing = 0;
        for (int i = 0; i < ARRAY_LEN(notes); i++)
        {
            notes[i].playing = IsKeyDown(MY_KEYS[i]);
            if(notes[i].playing) {
                notes_playing += 1;
            }
        }

        while (IsAudioStreamProcessed(synth))
        {
            for (int i = 0; i < ARRAY_LEN(buffer); i++)
            {
                buffer[i] = 0.0f;
            }
            if (notes_playing > 0) {
                for(int i = 0; i < arrlen(notes); i++)
                {
                    note_update(&notes[i], 1.0/notes_playing, buffer, ARRAY_LEN(buffer));
                }

                for(int i = 0; i < ARRAY_LEN(buffer); i++)
                {
                    buffer[i] = CLAMP(buffer[i], -1.0f, 1.0f);
                }
            }
            UpdateAudioStream(synth, buffer, ARRAY_LEN(buffer));
        }
        EndDrawing();
    }

    CloseWindow();
    UnloadAudioStream(synth);
    CloseAudioDevice();
    return 0;
}
