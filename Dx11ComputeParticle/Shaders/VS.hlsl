struct VS_IN {
  float4 Center : Center;
  float4 Color : Color;
  float4 Position : Position;
};

struct PS_IN {
  float4 Position : SV_Position;
  float4 Color : Color;
};

PS_IN main(VS_IN input) {
  PS_IN output;
  output.Position = input.Center + input.Position;
  output.Color = input.Color;
  return output;
}
