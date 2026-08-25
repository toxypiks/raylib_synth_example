#include <stdio.h>
#include <raylib.h>
#include <math.h>
#include "stb_ds.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE 32
#define CHANNELS 1
#define ROOT_NOOT 440.0
#define NEXT_SEMITONE 1.0594630943592953f

float semitone_to_frequency(int semitone)
{
    return ROOT_NOOT*pow(NEXT_SEMITONE, semitone);
}

typedef struct Note {
    float frequency;
    int frame_count;
} Note;

int main(void)
{
    InitWindow(800, 600, "synth");
    InitAudioDevice();

    float buffer[1024];
    SetAudioStreamBufferSizeDefault(ARRAY_LEN(buffer));

    AudioStream synth = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, CHANNELS);

    PlayAudioStream(synth);
    SetTargetFPS(60);
    int synth_frame_count = 0;
    float synth_freq = 0.0f;

    float* dynamic_array = NULL;

    while(!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(GetColor(0x181818AA));
        if(IsKeyDown(KEY_Z)) {
            synth_freq = semitone_to_frequency(0);
        }
        else {
            synth_freq = 0.0f;
        }

        if(IsKeyDown(KEY_S)) {
            synth_freq = semitone_to_frequency(1);
        } else {
            synth_freq = 0.0f;
        }

        if(IsKeyDown(KEY_X)) {
            synth_freq = semitone_to_frequency(2);
        } else {
            synth_freq = 0.0f;
        }

        if (IsAudioStreamProcessed(synth))
            {
                for(int i = 0; i < ARRAY_LEN(buffer); ++i)
                {
                    float time = (float)synth_frame_count/SAMPLE_RATE;
                    buffer[i] = sin(2*M_PI*time*synth_freq);
                    synth_frame_count += 1;
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
