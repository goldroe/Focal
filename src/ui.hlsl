cbuffer UI_Constants {
    matrix projection;
};

SamplerState font_sampler;
Texture2D font_texture;

struct VS_IN {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD;
    float4 color : COLOR;
};

struct VS_OUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

VS_OUT VS(VS_IN input) {
    VS_OUT output;
    output.pos = mul(projection, float4(input.pos, 0, 1));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PS(VS_OUT input) : SV_TARGET {
    return font_texture.Sample(font_sampler, input.uv).r * input.color;
}
