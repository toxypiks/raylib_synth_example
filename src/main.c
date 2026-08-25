#include <stdio.h>
#include <raylib.h>
#include <math.h>

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE 32
#define CHANNELS 1

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
    float next_semitone = pow(2.0, 1.0/12.0);
    float synth_freq = 440.0*next_semitone;

    while(!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(GetColor(0x181818AA));
        if (IsAudioStreamProcessed(synth))
            {
                for(int i = 0; i < ARRAY_LEN(buffer); ++i)
                {
                    float time = (float)synth_frame_count/SAMPLE_RATE;
                    buffer[i] = sin(2*M_PI*time*synth_freq*pow((next_semitone*next_semitone), floorf(GetTime())));
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
