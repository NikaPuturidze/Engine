cbuffer Transform : register(b0)
{
    row_major float4x4 transformMatrix;
	float4 topTint;
	float4 fogGeometry;
	float4 fogColor;
	float4 crackingBlock;
};

Texture2D sideTexture : register(t0);
Texture2D topTexture : register(t1);
Texture2D bottomTexture : register(t2);
Texture2D crackingTexture : register(t3);
Texture2D sideOverlayTexture : register(t4);
SamplerState textureSampler : register(s0);

struct Output
{
    float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	nointerpolation uint faceType : TEXCOORD1;
	float2 horizontalOffset : TEXCOORD2;
	nointerpolation float3 blockCenter : TEXCOORD3;
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
	output.blockCenter = instancePosition;
    return output;
}

float4 PS(Output input) : SV_TARGET
{
	const uint face = input.faceType;

	float4 color;
	if (face == 4)
		color = topTexture.Sample(textureSampler, input.uv) * float4(topTint.rgb, 1.0f);
	else if (face == 5)
		color = bottomTexture.Sample(textureSampler, input.uv);
	else
		color = sideTexture.Sample(textureSampler, input.uv);
	if (face < 4 && topTint.w > 0.0f)
	{
		const float4 overlay = sideOverlayTexture.Sample(textureSampler, input.uv);
		color.rgb = lerp(color.rgb, overlay.rgb * topTint.rgb, overlay.a);
	}
	clip(color.a - 0.1f);
	if (crackingBlock.w > 0.0f && all(abs(input.blockCenter - crackingBlock.xyz) < 0.01f))
	{
		const float4 crack = crackingTexture.Sample(textureSampler, input.uv);
		color.rgb *= lerp(float3(1, 1, 1), crack.rgb, crack.a);
	}
	// Overworld directional shade: north/south 0.8, west/east 0.6, up 1, down 0.5.
	const float faceShade = face == 4 ? 1.0f : (face == 5 ? 0.5f : (face < 2 ? 0.8f : 0.6f));
	color.rgb *= faceShade;

	const float fog = fogGeometry.w > 0.0f
		? smoothstep(fogGeometry.w * 0.75f, fogGeometry.w, length(input.horizontalOffset))
		: 0.0f;
	return lerp(color, fogColor, fog);
}
