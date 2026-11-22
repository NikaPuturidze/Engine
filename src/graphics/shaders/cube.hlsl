cbuffer Transform : register(b0)
{
    row_major float4x4 transformMatrix;
	float4 topTint;
};

Texture2D sideTexture : register(t0);
Texture2D topTexture : register(t1);
Texture2D bottomTexture : register(t2);
SamplerState textureSampler : register(s0);

struct Output
{
    float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

Output VS(float3 position : POSITION, float2 uv : TEXCOORD)
{
    Output output;
    output.position = mul(float4(position, 1.0f), transformMatrix);
	output.uv = uv;
    return output;
}

float4 PS(Output input, uint triangleId : SV_PrimitiveID) : SV_TARGET
{
	const uint face = triangleId / 2;

	if (face == 4)
		return topTexture.Sample(textureSampler, input.uv) * topTint;

	if (face == 5)
		return bottomTexture.Sample(textureSampler, input.uv);

	return sideTexture.Sample(textureSampler, input.uv);
}
