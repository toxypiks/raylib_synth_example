#include <stdio.h>
#include <raylib.h>
#include <math.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE 32
#define CHANNELS 1
#define BUFFER_SIZE 1024

int main(void)
{
    InitWindow(800, 600, "synth");
    InitAudioDevice();

    float buffer[BUFFER_SIZE];
    SetAudioStreamBufferSizeDefault(ARRAY_LEN(buffer));

    AudioStream synth = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, CHANNELS);

    PlayAudioStream(synth);
    SetTargetFPS(60);
    int synth_frame_count = 0;
    float synth_freq = 440.0;

    while(!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(GetColor(0x181818AA));
        if (IsAudioStreamProcessed(synth))
            {
                for(int i = 0; i < ARRAY_LEN(buffer); ++i)
                {
                    float time = (float)synth_frame_count/SAMPLE_RATE;
                    buffer[i] = sin(2*M_PI*time*440);
                    synth_frame_count += 1;
                }
                UpdateAudioStream(synth, buffer, BUFFER_SIZE);
            }
            EndDrawing();
        }
        CloseWindow();
        UnloadAudioStream(synth);
        CloseAudioDevice();
        return 0;
}
