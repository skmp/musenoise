// vim: set ts=4 sw=4 expandtab: 
/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#include <SDL.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <assert.h>

#include <arpa/inet.h> 
#include <netinet/in.h> 

#define PORT     5001 
#define MAXLINE 4096 

float centers[4] = { 10, 1.75, 72, 5.5 };
float volumes[4] = { 2, 5, 0.5, 3 };


int sockfd; 
    unsigned char buffer[MAXLINE]; 
    char *hello = "Hello from server"; 
    struct sockaddr_in servaddr, cliaddr; 

int HalfSquareWavePeriod;

// NOTE: MAP_ANONYMOUS is not defined on Mac OS X and some other UNIX systems.
// On the vast majority of those systems, one can use MAP_ANON instead.
// Huge thanks to Adam Rosenfield for investigating this, and suggesting this
// workaround:
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct sdl_offscreen_buffer
{
    // NOTE(casey): Pixels are alwasy 32-bits wide, Memory Order BB GG RR XX
    SDL_Texture *Texture;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct sdl_window_dimension
{
    int Width;
    int Height;
};

global_variable sdl_offscreen_buffer GlobalBackbuffer;

float osc_config[4][4];


#define MAX_CONTROLLERS 4
SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
SDL_Haptic *RumbleHandles[MAX_CONTROLLERS];

internal void
SDLInitAudio(int32 SamplesPerSecond, int32 BufferSize)
{
    SDL_AudioSpec AudioSettings = {0};

    AudioSettings.freq = SamplesPerSecond;
    AudioSettings.format = AUDIO_S16LSB;
    AudioSettings.channels = 2;
    AudioSettings.samples = BufferSize;

    SDL_OpenAudio(&AudioSettings, 0);

    if (AudioSettings.format != AUDIO_S16LSB)
    {
        printf("Oops! We didn't get AUDIO_S16LSB as our sample format!\n");
        SDL_CloseAudio();
    }

}

sdl_window_dimension
SDLGetWindowDimension(SDL_Window *Window)
{
    sdl_window_dimension Result;

    SDL_GetWindowSize(Window, &Result.Width, &Result.Height);

    return(Result);
}


internal void
RenderWeirdGradient(sdl_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{    
    uint8 *Row = (uint8 *)Buffer->Memory;
    for(int Y = 0;
        Y < Buffer->Height;
        ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = 0;
            X < Buffer->Width;
            ++X)
        {
            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);
            
            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void
SDLResizeTexture(sdl_offscreen_buffer *Buffer, SDL_Renderer *Renderer, int Width, int Height)
{
    int BytesPerPixel = 4;
    if (Buffer->Memory)
    {
        munmap(Buffer->Memory,
               Buffer->Width * Buffer->Height * BytesPerPixel);
    }
    if (Buffer->Texture)
    {
        SDL_DestroyTexture(Buffer->Texture);
    }
    Buffer->Texture = SDL_CreateTexture(Renderer,
                                        SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        Width,
                                        Height);
    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->Pitch = Width * BytesPerPixel;
    Buffer->Memory = mmap(0,
                          Width * Height * BytesPerPixel,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0);
}

internal void
SDLUpdateWindow(SDL_Window *Window, SDL_Renderer *Renderer, sdl_offscreen_buffer *Buffer)
{
    SDL_UpdateTexture(Buffer->Texture,
                      0,
                      Buffer->Memory,
                      Buffer->Pitch);

    SDL_RenderCopy(Renderer,
                   Buffer->Texture,
                   0,
                   0);

    SDL_RenderPresent(Renderer);
}


bool HandleEvent(SDL_Event *Event)
{
    bool ShouldQuit = false;
 
    switch(Event->type)
    {
        case SDL_QUIT:
        {
            printf("SDL_QUIT\n");
            ShouldQuit = true;
        } break;
        
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            SDL_Keycode KeyCode = Event->key.keysym.sym;
            bool IsDown = (Event->key.state == SDL_PRESSED);
            bool WasDown = false;
            if (Event->key.state == SDL_RELEASED)
            {
                WasDown = true;
            }
            else if (Event->key.repeat != 0)
            {
                WasDown = true;
            }
            
            // NOTE: In the windows version, we used "if (IsDown != WasDown)"
            // to detect key repeats. SDL has the 'repeat' value, though,
            // which we'll use.
            #define updn(u,d,v) else if(KeyCode == SDLK_##u) { v *= 1.2; } else if(KeyCode == SDLK_##d) { v /= 1.2; }
            if (Event->key.repeat == 0)
            {
                if (KeyCode == 0) { }
                updn(q,a,centers[0])
                updn(w,s,centers[1])
                updn(e,d,centers[2])
                updn(r,f,centers[3])
                updn(t,g,volumes[0])
                updn(y,h,volumes[1])
                updn(u,j,volumes[2])
                updn(i,k,volumes[3])
                else if(KeyCode == SDLK_UP)
                {
                    HalfSquareWavePeriod++;
                }
                else if(KeyCode == SDLK_LEFT)
                {
                    HalfSquareWavePeriod--;
                }
                else if(KeyCode == SDLK_DOWN)
                {
                }
                else if(KeyCode == SDLK_RIGHT)
                {
                }
                else if(KeyCode == SDLK_ESCAPE)
                {
                    printf("ESCAPE: ");
                    if(IsDown)
                    {
                        printf("IsDown ");
                    }
                    if(WasDown)
                    {
                        printf("WasDown");
                    }
                    printf("\n");
                }
                else if(KeyCode == SDLK_SPACE)
                {
                }
            }

            bool AltKeyWasDown = (Event->key.keysym.mod & KMOD_ALT);
            if (KeyCode == SDLK_F4 && AltKeyWasDown)
            {
                ShouldQuit = true;
            }

            printf("c0: %.3f c1: %.3f c2:%.3f c3: %.3f v0: %.3f v1: %.3f v2: %.3f v3: %.3f\n",
                centers[0], centers[1], centers[2], centers[3],
                volumes[0], volumes[1], volumes[2], volumes[3]);
        } break;

        case SDL_WINDOWEVENT:
        {
            switch(Event->window.event)
            {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                {
                    SDL_Window *Window = SDL_GetWindowFromID(Event->window.windowID);
                    SDL_Renderer *Renderer = SDL_GetRenderer(Window);
                    printf("SDL_WINDOWEVENT_SIZE_CHANGED (%d, %d)\n", Event->window.data1, Event->window.data2);
                } break;

                case SDL_WINDOWEVENT_FOCUS_GAINED:
                {
                    printf("SDL_WINDOWEVENT_FOCUS_GAINED\n");
                } break;

                case SDL_WINDOWEVENT_EXPOSED:
                {
                    SDL_Window *Window = SDL_GetWindowFromID(Event->window.windowID);
                    SDL_Renderer *Renderer = SDL_GetRenderer(Window);
                    SDLUpdateWindow(Window, Renderer, &GlobalBackbuffer);
                } break;
            }
        } break;
    }
    
    return(ShouldQuit);
}

internal void
SDLOpenGameControllers()
{
    int MaxJoysticks = SDL_NumJoysticks();
    int ControllerIndex = 0;
    for(int JoystickIndex=0; JoystickIndex < MaxJoysticks; ++JoystickIndex)
    {
        if (!SDL_IsGameController(JoystickIndex))
        {
            continue;
        }
        if (ControllerIndex >= MAX_CONTROLLERS)
        {
            break;
        }
        ControllerHandles[ControllerIndex] = SDL_GameControllerOpen(JoystickIndex);
        SDL_Joystick *JoystickHandle = SDL_GameControllerGetJoystick(ControllerHandles[ControllerIndex]);
        RumbleHandles[ControllerIndex] = SDL_HapticOpenFromJoystick(JoystickHandle);
        if (SDL_HapticRumbleInit(RumbleHandles[ControllerIndex]) != 0)
        {
            SDL_HapticClose(RumbleHandles[ControllerIndex]);
            RumbleHandles[ControllerIndex] = 0;
        }

        ControllerIndex++;
    }
}

internal void
SDLCloseGameControllers()
{
    for(int ControllerIndex = 0; ControllerIndex < MAX_CONTROLLERS; ++ControllerIndex)
    {
        if (ControllerHandles[ControllerIndex])
        {
            if (RumbleHandles[ControllerIndex])
                SDL_HapticClose(RumbleHandles[ControllerIndex]);
            SDL_GameControllerClose(ControllerHandles[ControllerIndex]);
        }
    }
}

void update_oscilator(char* name, float* args)
{
    //a, d, t, g
    int channel = -1;

    if (name[15] == 'a') channel = 0;
    if (name[15] == 'd') channel = 1;
    if (name[15] == 't') channel = 2;
    if (name[15] == 'g') channel = 3;

    if (channel != -1)
    {
        //printf("UOSC: %d %f %f %f %f\n", channel, args[0], args[1], args[2], args[3]);

        osc_config[channel][0] /= 2;
        osc_config[channel][1] /= 2;
        osc_config[channel][2] /= 2;
        osc_config[channel][3] /= 2;

        osc_config[channel][0] += args[0]/2;
        osc_config[channel][1] += args[1]/2;
        osc_config[channel][2] += args[2]/2;
        osc_config[channel][3] += args[3]/2;
    }
}

int main(int argc, char *argv[])
{
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
      
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(PORT); 
      
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO);
    // Initialise our Game Controllers:
    SDLOpenGameControllers();
    // Create our window.
    SDL_Window *Window = SDL_CreateWindow("GIMMEH BRAINZ SIGNALZ",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          640,
                                          480,
                                          SDL_WINDOW_RESIZABLE);
    if(Window)
    {
        // Create a "Renderer" for our window.
        SDL_Renderer *Renderer = SDL_CreateRenderer(Window,
                                                    -1,
                                                    SDL_RENDERER_PRESENTVSYNC);
        if (Renderer)
        {
            bool Running = true;
            sdl_window_dimension Dimension = SDLGetWindowDimension(Window);
            SDLResizeTexture(&GlobalBackbuffer, Renderer, Dimension.Width, Dimension.Height);
            int XOffset = 0;
            int YOffset = 0;


            // NOTE: Sound test
            int SamplesPerSecond = 48000;
            int ToneHz = 256;
            int16 ToneVolume = 3000;
            uint32 RunningSampleIndex = 0;
            int SquareWavePeriod = SamplesPerSecond / ToneHz;
            HalfSquareWavePeriod = SquareWavePeriod / 2;
            int BytesPerSample = sizeof(int16) * 2;
            // Open our audio device:
            SDLInitAudio(48000, SamplesPerSecond * BytesPerSample / 60);
            bool SoundIsPlaying = false;

            while(Running)
            {
                SDL_Event Event;
                while(SDL_PollEvent(&Event))
                {
                    if (HandleEvent(&Event))
                    {
                        Running = false;
                    }
                }
                
                // Poll our controllers for input.
                for (int ControllerIndex = 0;
                     ControllerIndex < MAX_CONTROLLERS;
                     ++ControllerIndex)
                {
                    if(ControllerHandles[ControllerIndex] != 0 && SDL_GameControllerGetAttached(ControllerHandles[ControllerIndex]))
                    {
                        // NOTE: We have a controller with index ControllerIndex.
                        bool Up = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_UP);
                        bool Down = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                        bool Left = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                        bool Right = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                        bool Start = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_START);
                        bool Back = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_BACK);
                        bool LeftShoulder = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                        bool RightShoulder = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                        bool AButton = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_A);
                        bool BButton = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_B);
                        bool XButton = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_X);
                        bool YButton = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_Y);

                        int16 StickX = SDL_GameControllerGetAxis(ControllerHandles[ControllerIndex], SDL_CONTROLLER_AXIS_LEFTX);
                        int16 StickY = SDL_GameControllerGetAxis(ControllerHandles[ControllerIndex], SDL_CONTROLLER_AXIS_LEFTY);
                        
                        if (AButton)
                        {
                            YOffset += 2;
                        }
                        if (BButton)
                        {
                            if (RumbleHandles[ControllerIndex])
                            {
                                SDL_HapticRumblePlay(RumbleHandles[ControllerIndex], 0.5f, 2000);
                            }
                        }
                    }
                    else
                    {
                        // TODO: This controller is not plugged in.
                    }
                }
                
                
                RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

                for(;;)
                {
                    int len, n; 
                    n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
                                MSG_DONTWAIT, ( struct sockaddr *) &cliaddr, 
                                &len);

                    if (n >0)
                    {
                    buffer[n] = '\0'; 

                    unsigned char* in = buffer + 16;
                    unsigned char* end = buffer + n;
                    //printf("%d:: ", n);
                    for (;;)
                    {
                        unsigned char* bundle = in;

                        if (bundle+4 == end)
                            break;

                        unsigned int len = bundle[0] * 256 * 256 * 256 + bundle[1] * 256 * 256 + bundle[2] * 256 + bundle[3];

                        if (len == 0)
                        {
                            break;
                        }
                        else
                        {
                            char * name = bundle + 4;
                            int namelen = strlen(name);
                            if (namelen&3)
                                namelen += 4 - (namelen&3);

                            if (strstr(name, "/muse/elements/"))
                            {
                                char* type = name + namelen;

                                int typelen = strlen(type);
                                if (typelen&3)
                                    typelen += 4 - (typelen&3);

                                unsigned char* data = type + typelen;


                                //printf("%d: %s, ", namelen, name);
                                //printf("%d: %s\n", typelen, type);

                                char* type_args = type+ 1;

                                int count = strlen(type_args);
                                assert(count < 16);
                                float args[16];

                                for (int i = 0; i < count; i++)
                                {
                                    char t = type_args[i];

                                    unsigned int payload = data[0] * 256 * 256 * 256 + data[1] * 256 * 256 + data[2] * 256 + data[3];

                                    args[i] = (float&)payload;
                                    //printf("%d: %c: %f\n", i, t, (float&)payload);
                                    data+= 4;
                                }

                                if (count == 4)
                                {
                                    update_oscilator(name, args);
                                }
                            }
                            //puts(bundle + 4);
                            in += len;
                        }
                    }
                    //printf("Client : %d %s\n", n, buffer);

                    //fwrite(buffer, 1, n, stdout);


                    //write(0, buffer, n);
                    }
                    else
                    {
                        break;
                    } 
                }

                // Sound output test
                int TargetQueueBytes = SamplesPerSecond * BytesPerSample/10;
                int BytesToWrite = TargetQueueBytes - SDL_GetQueuedAudioSize(1);
                if (BytesToWrite)
                {
                    void *SoundBuffer = malloc(BytesToWrite);
                    int16 *SampleOut = (int16 *)SoundBuffer;
                    int SampleCount = BytesToWrite/BytesPerSample;
                    for(int SampleIndex = 0;
                        SampleIndex < SampleCount;
                        ++SampleIndex)
                    {
                        int32 SampleValue = 0;

                        //a , d, g, t
                        // a:8-12, d: 0.5 - 3, g 32-100, t: 4-7
                        // 
                        
                        for (int ch=0; ch<4; ch++)
                        {
                            for (int fg = 0; fg < 4; fg++)
                            {
                                auto period = HalfSquareWavePeriod/(centers[fg] * (8.5f+fg)/10);

                                auto tone = osc_config[ch][fg] * ToneVolume * volumes[ch];

                                if (period == 0) { period = 1; tone = 0; }
                                SampleValue+= sin(RunningSampleIndex / period) * tone;// ? tone : -tone;
                            }
                        }
                        
                        SampleValue /= 32;

                        if (SampleValue > 32000)
                            SampleValue = 32000;
                        else if (SampleValue < -32000)
                            SampleValue = -32000;

                        RunningSampleIndex++;
                        *SampleOut++ = SampleValue;
                        *SampleOut++ = SampleValue;
                    }
                    SDL_QueueAudio(1, SoundBuffer, BytesToWrite);
                    free(SoundBuffer);
                }

                if(!SoundIsPlaying)
                {
                    SDL_PauseAudio(0);
                    SoundIsPlaying = true;
                }

                SDLUpdateWindow(Window, Renderer, &GlobalBackbuffer);

                ++XOffset;
                
            }
        }
        else
        {
            // TODO(casey): Logging
        }
    }
    else
    {
        // TODO(casey): Logging
    }
    
    SDLCloseGameControllers();
    SDL_Quit();
    return(0);
}
