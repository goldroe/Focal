cbuffer Grid_Constants : register(b0) {
    float4 c1;
    float4 c2;
    float2 cp;
    float size;
};

float4 VS(float2 in_pos : POSITION) : SV_POSITION {
    return float4(in_pos, 0, 1);
}

float4 PS(float4 pos : SV_POSITION) : SV_TARGET {
    float2 p = cp - pos;
    float2 tile = floor(p / size);
    float c = fmod(tile.x + tile.y, 2.0);
    float4 color;
    if (c == 0.0) {
        color = c1;
    } else {
        color = c2;
    }
    return color;
}
