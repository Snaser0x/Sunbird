cbuffer QuadConstants : register(b0)
{
    float2 ScreenSize;
    float2 Padding;
};

Texture2D QuadTexture : register(t0);
SamplerState QuadSampler : register(s0);

struct VSInput
{
    float2 Position : POSITION;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;

    // Pixels (top-left origin, y down) -> clip space (center origin, y up).
    float2 clip = (input.Position / ScreenSize) * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.Position = float4(clip, 0.0, 1.0);
    output.UV = input.UV;
    output.Color = input.Color;

    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    return QuadTexture.Sample(QuadSampler, input.UV) * input.Color;
}