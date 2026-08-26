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

int main(void)
{
    InitWindow(800, 600, "synth");
    InitAudioDevice();

    float buffer[1024];
    SetAudioStreamBufferSizeDefault(ARRAY_LEN(buffer));

    AudioStream synth = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, CHANNELS);

    PlayAudioStream(synth);

    SetTargetFPS(60);

    int frame_count = 0;

    while(!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(GetColor(0x181818FF));

        int notes_playing = 0;
        for (int i = 0; i < ARRAY_LEN(notes); i++)
        {
            notes[i] = IsKeyDown(MY_KEYS[i]);
            if (notes[i]) {
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
                for(int i = 0; i < ARRAY_LEN(notes); i++)
                {
                    if(notes[i]) {
                        note_update(frame_count, semitone_to_frequency(i), 1.0/notes_playing, buffer, ARRAY_LEN(buffer));
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
        EndDrawing();
    }

    CloseWindow();
    UnloadAudioStream(synth);
    CloseAudioDevice();
    return 0;
}
