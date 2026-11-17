#pragma once
#include <array>
#include <algorithm>

namespace rmx
{
enum Program
{
    ambience, roomA1, hallC1, plateA1, hallB3, chorus, echo, nonlin2, reverse1,
    reverse2, freeze, roomA0, roomB1, hallA1, plateB1, delay, imageP1, nonlin1, programCount
};
enum Shape { diffuse, ensemble, repeats, gate, reverse, sustain, straight, pan };
struct ProgramSpec
{
    const char* name;
    Shape shape;
    int maxPre, maxB;
    float maxDecay;
    bool low, high;
    float size, early, modulation, damping;
};
inline constexpr std::array<ProgramSpec, programCount> programs {{
    {"AMBIENCE",diffuse,200,200,9.9f,true,true,.99f,.036f,1.3f,.04f},
    {"ROOM A1",diffuse,300,300,9.9f,false,true,.42f,.55f,.45f,.27f},
    {"HALL C1",diffuse,200,200,9.9f,true,true,1.34f,.68f,2.1f,.11f},
    {"PLATE A1",diffuse,300,300,9.9f,true,true,.91f,.09f,1.7f,.075f},
    {"HALL B3",diffuse,300,300,9.9f,true,true,1.58f,.25f,2.7f,.18f},
    {"CHORUS",ensemble,50,50,9.9f,false,true,1,0,0,0},
    {"ECHO",repeats,1200,400,9.9f,true,true,1,0,0,0},
    {"NONLIN 2",gate,300,300,9.9f,false,true,1,0,0,0},
    {"REVRS 1",reverse,300,300,9.9f,false,false,1,0,0,0},
    {"REVRS 2",reverse,300,300,9.9f,false,true,1.2f,0,0,0},
    {"FREEZE",sustain,0,0,.2f,false,false,.76f,.18f,1.3f,0},
    {"ROOM A0",diffuse,300,300,9.9f,false,true,.42f,.55f,.45f,.06f},
    {"ROOM B1",diffuse,300,300,.7f,false,true,.28f,.38f,.1f,.16f},
    {"HALL A1",diffuse,300,300,9.9f,false,true,1.18f,.53f,.55f,.19f},
    {"PLATE B1",diffuse,300,300,9.9f,true,true,.68f,.10f,.95f,.12f},
    {"DELAY",straight,1630,1630,9.9f,false,false,1,0,0,0},
    {"IMAGE P1",pan,300,300,9.9f,false,true,1,0,0,0},
    {"NONLIN 1",gate,300,300,9.9f,false,false,.8f,0,0,0},
}};
struct Settings
{
    int program = ambience;
    std::array<float,2> pre {0,0}, decay {2,0}, low {0,0}, high {0,0};
    float input = 0, output = 0, wet = 1;
    bool power = true;
};
struct Preset
{
    const char* name;
    int program;
    float pre, decay, low, high;
    float preB = 0, decayB = 0;
};
inline constexpr std::array<Preset,25> presets {{
    {"Lead Vocal Plate",plateA1,48,1.9f,-3,-4},
    {"Intimate Vocal Ambience",ambience,24,1.2f,-2,-5},
    {"Arena Vocal Hall",hallC1,67,2.8f,-4,-4},
    {"Backing Vocal Halo",plateB1,19,2.3f,-3,-6},
    {"Vocal Slap",echo,114,.8f,-2,-5,143,.4f},
    {"Five Voice Backing Stack",chorus,14,2.4f,0,-6},
    {"Hard Rock Lead Plate",plateA1,32,1.5f,-4,-4},
    {"Arena Solo Hall",hallB3,61,2.6f,-2,-5},
    {"Metal Lead Throw",echo,375,3.4f,-3,-5,188,2.4f},
    {"Clean Guitar Shimmer",chorus,20,3.3f,0,-2},
    {"Tight Rhythm Room",roomB1,7,.4f,0,-4},
    {"Wide Acoustic Room",roomA0,14,.8f,0,-6},
    {"Bass Texture Double",chorus,7,1.2f,0,-7},
    {"Big Gated Snare",nonlin2,17,4.4f,0,-2},
    {"Tight Snare Gate",nonlin1,4,2.7f,0,0},
    {"Snare Plate Snap",plateB1,23,.8f,-4,-3},
    {"Tom Bloom",nonlin2,12,3.6f,0,-4},
    {"Kick and Snare Chamber",roomA1,12,.8f,0,-7},
    {"Drum Room Push",roomA1,3,1.2f,0,-6},
    {"Parallel Drum Ambience",ambience,0,.8f,-2,-5},
    {"Dark Drum Hall",hallB3,23,1.8f,-3,-8},
    {"Reverse Snare Rise",reverse2,0,5.2f,0,-3},
    {"Cross-Pan Drum Splash",imageP1,8,5.4f,0,-4},
    {"Sparse Reverse Guitar",reverse1,14,7.2f,0,0},
    {"Long Vocal Wash",hallA1,71,4.2f,0,-6},
}};
}
