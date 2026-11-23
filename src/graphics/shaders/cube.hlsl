cbuffer Transform : register(b0)
{
    row_major float4x4 transformMatrix;
	float4 topTint;
	float4 fogGeometry;
	float4 fogColor;
};

Texture2D sideTexture : register(t0);
Texture2D topTexture : register(t1);
Texture2D bottomTexture : register(t2);
SamplerState textureSampler : register(s0);

struct Output
{
    float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	nointerpolation uint faceType : TEXCOORD1;
	float2 horizontalOffset : TEXCOORD2;
};

Output VS(float3 position : POSITION, float2 uv : TEXCOORD, uint faceType : TEXCOORD1,
    float3 instancePosition : TEXCOORD2)
{
    Output output;
    const float3 worldPosition = position * fogGeometry.z + instancePosition;
    output.position = mul(float4(worldPosition, 1.0f), transformMatrix);
	output.uv = uv;
	output.faceType = faceType;
	output.horizontalOffset = worldPosition.xz + fogGeometry.xy;
    return output;
}

float4 PS(Output input) : SV_TARGET
{
	const uint face = input.faceType;

	float4 color;
	if (face == 4)
		color = topTexture.Sample(textureSampler, input.uv) * topTint;
	else if (face == 5)
		color = bottomTexture.Sample(textureSampler, input.uv);
	else
		color = sideTexture.Sample(textureSampler, input.uv);

	const float fog = fogGeometry.w > 0.0f
		? smoothstep(fogGeometry.w * 0.75f, fogGeometry.w, length(input.horizontalOffset))
		: 0.0f;
	return lerp(color, fogColor, fog);
}
