#pragma once
inline constexpr char HudShader[] = R"(
cbuffer HudConstants : register(b0) { float2 viewport; float2 padding; };
Texture2D atlas : register(t0); SamplerState atlasSampler : register(s0);
struct Input { float2 position:POSITION; float2 uv:TEXCOORD0; float4 color:COLOR0; };
struct Pixel { float4 position:SV_POSITION; float2 uv:TEXCOORD0; float4 color:COLOR0; };
Pixel HudVS(Input input)
{
    Pixel output;
    output.position=float4(input.position.x/viewport.x*2-1,1-input.position.y/viewport.y*2,0,1);
    output.uv=input.uv;output.color=input.color;return output;
}
float4 HudPS(Pixel input):SV_TARGET
{
    return float4(input.color.rgb,input.color.a*atlas.Sample(atlasSampler,input.uv).a);
}
)";
