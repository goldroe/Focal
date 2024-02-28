cbuffer Image_Constants : register(b0) {
    matrix projection;
};

Texture2D image_texture : register(t0);
SamplerState texture_sampler : register(s0);

struct VS_IN {
    float2 pos : POSITION;
    float2  uv  : TEXCOORD;
};

struct VS_OUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS_OUT VS(VS_IN input) {
    VS_OUT output;
    output.pos = mul(projection, float4(input.pos, 0, 1));
    output.uv = input.uv;
    return output;
}

float4 PS(VS_OUT input) : SV_TARGET {
    /* return float4(1, 1, 1, 1); */
    return image_texture.Sample(texture_sampler, input.uv);
}
