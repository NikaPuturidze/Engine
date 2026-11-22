cbuffer Transform : register(b0)
{
    row_major float4x4 transformMatrix;
};

struct Output
{
    float4 position : SV_POSITION;
};

Output VS(float3 position : POSITION)
{
    Output output;
    output.position = mul(float4(position, 1.0f), transformMatrix);
    return output;
}

float4 PS(Output input, uint triangleId : SV_PrimitiveID) : SV_TARGET
{
    const float3 faceColors[6] = {
        float3(0.95f, 0.35f, 0.25f), // Front: red
        float3(0.25f, 0.55f, 0.90f), // Back: blue
        float3(0.95f, 0.75f, 0.20f), // Left: yellow
        float3(0.30f, 0.75f, 0.45f), // Right: green
        float3(0.90f, 0.90f, 0.90f), // Top: white
        float3(0.60f, 0.35f, 0.80f), // Bottom: purple
    };
    return float4(faceColors[triangleId / 2], 1.0f);
}
