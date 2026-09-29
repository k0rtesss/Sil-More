#!/usr/bin/env python3
"""Check production footstep DSP/cache with an offline SDL mixer; render a preview."""
from pathlib import Path
import os
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-standard"
OUT = ROOT / "scripts/output/footstep-acoustics"

HARNESS = r'''
#include "sdl-sound.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static float rms(const float* x, int n) {
    double energy = 0;
    for (int i = 0; i < n; i++) energy += x[i] * x[i];
    return (float)sqrt(energy / n);
}
static void test_filter(void) {
    float signal[4800], copy[4800];
    int length;
    for (int i=0;i<4800;i++) signal[i]=.4f*sinf(6.2831853f*200*i/48000);
    memcpy(copy,signal,sizeof(signal));
    float* low=sound_footstep_filter(signal,4800,1,48000,CAVE_ATMOSPHERE_HUSHED,&length);
    assert(low && length>4800 && !memcmp(copy,signal,sizeof(signal)));
    float low_rms=rms(low+100,4700); SDL_free(low);
    for(int i=0;i<4800;i++) signal[i]=.4f*sinf(6.2831853f*6000*i/48000);
    float* high=sound_footstep_filter(signal,4800,1,48000,CAVE_ATMOSPHERE_HUSHED,&length);
    assert(high && rms(high+100,4700)<low_rms*.08f); SDL_free(high);
    for(int channels=1;channels<=2;channels++) {
        float impulse[960]={0}; impulse[0]=1;
        float* echo=sound_footstep_filter(impulse,480,channels,48000,CAVE_ATMOSPHERE_ECHOING,&length);
        assert(echo && length>480 && echo[0]>.8f && echo[0]<.9f);
        double tail=0;
        for(int f=0;f<length;f++) for(int c=0;c<channels;c++) {
            float v=echo[f*channels+c]; assert(isfinite(v) && fabsf(v)<=.950001f);
            if(c==1) assert(v==0); /* no accidental cross-channel corruption */
            if(f>480) tail+=v*v;
        }
        assert(tail>.001); SDL_free(echo);
    }
    assert(!sound_footstep_filter(signal,4800,1,48000,CAVE_ATMOSPHERE_NONE,&length));
    assert(!sound_footstep_filter(signal,4800,3,48000,CAVE_ATMOSPHERE_HUSHED,&length));
    assert(!sound_footstep_filter(signal,48000*5,1,48000,CAVE_ATMOSPHERE_HUSHED,&length));
    assert(!sound_footstep_filter(NULL,4800,1,48000,CAVE_ATMOSPHERE_HUSHED,&length));
    puts("PCM: high-frequency muffling, reverb tail, mono/stereo isolation, headroom and input bounds PASS.");
}
static void word(FILE* out,unsigned v,int n) {
    for(int i=0;i<n;i++) fputc((v>>(8*i))&255,out);
}
static void preview(const char* path,MIX_Audio** versions) {
    const int frames=48000*2;
    float* samples=SDL_calloc(frames,sizeof(float)); assert(samples);
    FILE* out=fopen(path,"wb"); assert(out);
    unsigned bytes=frames*2*3;
    fwrite("RIFF",1,4,out);word(out,bytes+36,4);fwrite("WAVEfmt ",1,8,out);
    word(out,16,4);word(out,1,2);word(out,1,2);word(out,48000,4);
    word(out,96000,4);word(out,2,2);word(out,16,2);fwrite("data",1,4,out);word(out,bytes,4);
    MIX_Track* track=MIX_CreateTrack(sound_state.mixer); assert(track);
    for(int part=0;part<3;part++) {
        memset(samples,0,frames*sizeof(float));
        assert(MIX_SetTrackAudio(track,versions[part]));
        assert(MIX_PlayTrack(track,0));
        assert(MIX_Generate(sound_state.mixer,samples,frames*sizeof(float))>0);
        assert(!MIX_TrackPlaying(track));
        assert(rms(samples,frames)>0);
        for(int i=0;i<frames;i++) {
            float v=fmaxf(-1,fminf(1,samples[i]));
            word(out,(unsigned)(short)(v*32767),2);
        }
    }
    MIX_DestroyTrack(track);fclose(out);SDL_free(samples);
}
int main(int argc,char** argv) {
    assert(argc==4);
    test_filter();
    assert(MIX_Init());
    SDL_AudioSpec spec={.format=SDL_AUDIO_F32,.channels=1,.freq=48000};
    sound_state.mixer=MIX_CreateMixer(&spec); assert(sound_state.mixer);
    MIX_Audio* dry=MIX_LoadAudio(sound_state.mixer,argv[1],true); assert(dry);
    sound_origin origin={.atmosphere=CAVE_ATMOSPHERE_NONE};
    assert(sdl_sound_footstep_variant(dry,argv[1],0,false,&origin,true)==dry);
    origin.atmosphere=CAVE_ATMOSPHERE_HUSHED;
    assert(sdl_sound_footstep_variant(dry,argv[1],0,false,&origin,false)==dry);
    MIX_Audio* hush=sdl_sound_footstep_variant(dry,argv[1],0,false,&origin,true);
    assert(hush && hush!=dry);
    assert(sdl_sound_footstep_variant(dry,"missing",0,false,&origin,false)==hush);
    origin.atmosphere=CAVE_ATMOSPHERE_ECHOING;
    MIX_Audio* echo=sdl_sound_footstep_variant(dry,argv[1],0,false,&origin,true);
    assert(echo && echo!=dry && echo!=hush);
    assert(sdl_sound_footstep_variant(dry,"missing",0,false,&origin,false)==echo);
    origin.atmosphere=CAVE_ATMOSPHERE_DRAUGHTY;
    assert(sdl_sound_footstep_variant(dry,argv[1],0,false,&origin,true)==dry);
    MIX_Audio* splash=MIX_LoadAudio(sound_state.mixer,argv[2],true); assert(splash);
    for(int effect=CAVE_ATMOSPHERE_HUSHED;effect<=CAVE_ATMOSPHERE_ECHOING;effect++) {
        origin.atmosphere=(cave_atmosphere_kind)effect;
        MIX_Audio* filtered=sdl_sound_footstep_variant(splash,argv[2],0,true,&origin,true);
        assert(filtered && filtered!=splash && filtered!=hush && filtered!=echo);
        assert(sdl_sound_footstep_variant(splash,"missing",0,true,&origin,false)==filtered);
    }
    origin.atmosphere=CAVE_ATMOSPHERE_HUSHED;
    assert(sdl_sound_footstep_variant(dry,"missing",1,false,&origin,true)==dry);
    assert(sdl_sound_footstep_variant(dry,argv[1],1,false,&origin,true)==dry);
    /* Exercise the real event playback helpers through an offline SFX track,
     * including the no-decoding timer path and the water-footstep branch. */
    g_sound_config.enabled=true;
    sound_state.enable_walk=true; sound_state.volume_walk=1;
    sound_state.bank.sound_counts[MSG_WALK]=1;
    sound_state.bank.sound_counts[MSG_LANDING]=1;
    sound_state.bank.sound_audio[MSG_WALK][0]=dry;
    sound_state.bank.sound_audio[MSG_LANDING][0]=dry;
    SDL_strlcpy(sound_state.bank.sound_files[MSG_WALK][0],argv[1],SDL_SOUND_NAME_LEN);
    SDL_strlcpy(sound_state.bank.sound_files[MSG_LANDING][0],argv[1],SDL_SOUND_NAME_LEN);
    sound_state.bank.water_walk_count=1; sound_state.bank.water_walk_audio[0]=splash;
    SDL_strlcpy(sound_state.bank.water_walk_files[0],argv[2],SDL_SOUND_NAME_LEN);
    MIX_Track* sfx=MIX_CreateTrack(sound_state.mixer); assert(sfx);
    sound_state.sfx_tracks[0]=sfx;
    assert(sdl_sound_play_sample_locked(MSG_WALK,0,true,false,1,&origin));
    assert(MIX_GetTrackAudio(sfx)==hush); MIX_StopTrack(sfx,0);
    origin.atmosphere=CAVE_ATMOSPHERE_ECHOING;
    assert(sdl_sound_play_sample_locked(MSG_WALK,0,true,false,1,&origin));
    assert(MIX_GetTrackAudio(sfx)==echo); MIX_StopTrack(sfx,0);
    assert(sdl_sound_play_sample_locked(MSG_LANDING,0,true,false,1,&origin));
    assert(MIX_GetTrackAudio(sfx)==dry); MIX_StopTrack(sfx,0);
    assert(sdl_sound_play_water_walk_sample_locked(0,true,false,1,&origin));
    assert(MIX_GetTrackAudio(sfx)==sound_state.bank.footstep_effect_audio[1][1][0]);
    MIX_StopTrack(sfx,0); g_sound_config.enabled=false;
    assert(!sdl_sound_play_sample_locked(MSG_WALK,0,true,false,1,&origin));
    MIX_DestroyTrack(sfx); sound_state.sfx_tracks[0]=NULL;
    sound_state.bank.sound_audio[MSG_WALK][0]=NULL;
    sound_state.bank.sound_audio[MSG_LANDING][0]=NULL;
    sound_state.bank.water_walk_audio[0]=NULL;
    puts("Event routing: captured Hushed/Echoing effects, delayed steps, water steps, ordinary landing and mute setting PASS.");
    MIX_Audio* versions[]={dry,hush,echo}; preview(argv[3],versions);
    puts("SDL mixer: actual stone/water recordings, separate caches, timer-safe reuse, dry fallback and offline playback PASS.");
    sdl_sound_destroy_cached_audio();
    for(int e=0;e<2;e++)for(int w=0;w<2;w++) {
        assert(!sound_state.bank.footstep_effect_audio[e][w][0]);
        assert(!sound_state.bank.footstep_effect_attempted[e][w][0]);
    }
    MIX_DestroyAudio(dry);MIX_DestroyAudio(splash);MIX_DestroyMixer(sound_state.mixer);MIX_Quit();
    return 0;
}
'''


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / "check.c"
    source.write_text(HARNESS, encoding="utf-8")
    env = os.environ.copy()
    env["PATH"] = os.pathsep.join([
        *(str(BUILD / "_deps" / name) for name in ("SDL", "SDL_mixer", "SDL_image", "SDL_ttf")),
        "C:/msys64/mingw64/bin", "C:/msys64/usr/bin", env["PATH"]])
    objects = shlex.split((BUILD / "CMakeFiles/sil-more.dir/objects1.rsp").read_text())
    objects = [p for p in objects if not p.endswith((
        "/src/main.c.obj", "/src/sdl-sound.c.obj", "/src/sound-footsteps.c.obj"))]
    response = OUT / "objects.rsp"
    response.write_text("\n".join('"' + p + '"' for p in objects), encoding="utf-8")
    exe = OUT / "check.exe"
    subprocess.run([
        "C:/msys64/mingw64/bin/cc.exe", "-DUSE_SDL", "-std=c17", "-O1",
        "-ffunction-sections", "-fdata-sections", "@CMakeFiles/sil-more.dir/includes_C.rsp",
        str(source), str(ROOT / "src/sound-footsteps.c"),
        "@" + str(response), "@CMakeFiles/sil-more.dir/linkLibs.rsp",
        "-o", str(exe)], cwd=BUILD, env=env, check=True)
    result = subprocess.run([str(exe),
        str(ROOT / "lib/xtra/sound/walk/Stone_Walk_5.ogg"),
        str(ROOT / "lib/xtra/sound/water_walking/water_step_01.ogg"),
        str(OUT / "footsteps-normal-hushed-echoing.wav")],
        cwd=OUT, env=env, text=True, capture_output=True, timeout=30)
    (OUT / "validation.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    if result.returncode:
        print(result.stderr)
        result.check_returncode()


if __name__ == "__main__":
    main()
