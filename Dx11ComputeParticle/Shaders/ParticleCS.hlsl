cbuffer c0 {
  uint particleCount;
  uint lifetime;
};

struct Particle {
  uint age;
  float3 position;
  float3 velocity;
};

RWStructuredBuffer<Particle> Particles : register(u0);

void main_(uint3 threadID) {
  if (Particles[threadID.x].age > lifetime) {
    Particles[threadID.x].position = (float3)0;
    return;
  }
  Particles[threadID.x].position += Particles[threadID.x].velocity;
  Particles[threadID.x].age += 1;
}

[numthreads(64, 1, 1)] void main(uint3 threadID : SV_DispatchThreadID) {
  if (threadID.x < particleCount) {
    main_(threadID);
  }
}
