#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_buffer_reference : require

struct instance_data {
    mat4 transform;
    uint boxes;
    uint flags;
    float surface[9];
    float edge[16];
    float known_surface[9];
    float known_edge[16];
};

layout(std430, buffer_reference) readonly buffer InstanceData {
    instance_data instances[];
};

layout(push_constant) uniform pushConstants {
    InstanceData instance_data;
    float tear_depth;
    float edge_softness;
    float transition;
    float pad;
} pc;

layout(location = 0) in vec3 worldPosition;
layout(location = 1) flat in uint instanceIndex;
layout(location = 2) in vec2 local;

layout(location = 0) out vec4 outMask;

const uint SELF_BIT = 4u;
const uint NEIGHBOUR_BITS = 0x1EFu;
const uint BOX_NONE = 0u;
const uint BOX_LIT = 1u;
const uint BOX_PENDING = 2u;
const uint REVEALING_SHIFT = 9u;
const uint KNOWN_SHIFT = 18u;
const uint HIDDEN_BIT = 20u;
const uint KNOWN_FULL = 1u;
const uint KNOWN_FRONT = 2u;
const vec3 VOID_NOISE_OFFSET = vec3(37.1, 11.3, 23.7);
const float MAX_TEAR_REACH = 0.45;
const float NO_NEIGHBOUR = 2.0;
const float MIN_TEAR = 0.02;

instance_data inst;

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float value_noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    return mix(mix(mix(hash(i + vec3(0, 0, 0)), hash(i + vec3(1, 0, 0)), f.x),
                   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
                   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float tear_noise(vec3 p) {
    return 0.5 * value_noise(p * 2.0)
         + 0.3 * value_noise(p * 5.0)
         + 0.2 * value_noise(p * 11.0);
}

float box_distance(int dx, int dy) {
    vec2 lo = vec2(dx, dy);
    vec2 hi = lo + 1.0;
    return length(max(max(lo - local, local - hi), 0.0));
}

float neighbour_distance(uint bits) {
    float distance_to_set = NO_NEIGHBOUR;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            uint bit = uint((dy + 1) * 3 + (dx + 1));
            if (bit == SELF_BIT || (bits & (1u << bit)) == 0u)
                continue;

            distance_to_set = min(distance_to_set, box_distance(dx, dy));
        }
    }

    return distance_to_set;
}

float torn(float distance_to_set, float threshold, float softness) {
    if (distance_to_set >= NO_NEIGHBOUR || threshold <= MIN_TEAR)
        return 0.0;

    return softness > 0.0
         ? 1.0 - smoothstep(threshold, threshold + softness, distance_to_set)
         : step(distance_to_set, threshold);
}

float along(float a, float b, float c, float u) {
    u = clamp(u, 0.0, 1.0) * 2.0;
    return u < 1.0 ? mix(a, b, u) : mix(b, c, u - 1.0);
}

float surface_sample(int i, bool known) {
    return known ? inst.known_surface[i] : inst.surface[i];
}

float surface_front(bool known) {
    vec2 g = clamp(local, 0.0, 1.0) * 2.0;
    ivec2 c = min(ivec2(g), ivec2(1));
    vec2 f = g - vec2(c);
    int i = c.y * 3 + c.x;
    return mix(mix(surface_sample(i, known), surface_sample(i + 1, known), f.x),
               mix(surface_sample(i + 3, known), surface_sample(i + 4, known), f.x), f.y);
}

float edge_sample(int i, bool known) {
    return known ? inst.known_edge[i] : inst.edge[i];
}

float edge_front(int dx, int dy, bool known) {
    if (dy == -1 && dx == 0)
        return along(edge_sample(0, known), edge_sample(1, known), edge_sample(2, known), local.x);
    if (dy == 0 && dx == 1)
        return along(edge_sample(3, known), edge_sample(4, known), edge_sample(5, known), local.y);
    if (dy == 1 && dx == 0)
        return along(edge_sample(6, known), edge_sample(7, known), edge_sample(8, known), local.x);
    if (dy == 0 && dx == -1)
        return along(edge_sample(9, known), edge_sample(10, known), edge_sample(11, known), local.y);
    if (dy == -1)
        return edge_sample(dx == -1 ? 12 : 13, known);
    return edge_sample(dx == -1 ? 14 : 15, known);
}

float distance_to_light() {
    uint self_state = (inst.boxes >> (2u * SELF_BIT)) & 3u;
    if (self_state == BOX_LIT)
        return -1.0;

    float distance_to_set = self_state == BOX_PENDING ? surface_front(false) : NO_NEIGHBOUR;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            uint bit = uint((dy + 1) * 3 + (dx + 1));
            uint state = (inst.boxes >> (2u * bit)) & 3u;
            if (bit == SELF_BIT || state == BOX_NONE)
                continue;

            float d = box_distance(dx, dy);
            if (state == BOX_PENDING)
                d += max(edge_front(dx, dy, false), 0.0);
            distance_to_set = min(distance_to_set, d);
        }
    }

    return distance_to_set;
}

float distance_to_void(uint void_bits, uint revealing_bits, float threshold) {
    float distance_to_set = neighbour_distance(void_bits);

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            uint bit = uint((dy + 1) * 3 + (dx + 1));
            if (bit == SELF_BIT || (revealing_bits & (1u << bit)) == 0u)
                continue;

            float d = box_distance(dx, dy) + max(threshold - edge_front(dx, dy, true), 0.0);
            distance_to_set = min(distance_to_set, d);
        }
    }

    return distance_to_set;
}

void main() {
    inst = pc.instance_data.instances[instanceIndex];

    float noise = tear_noise(worldPosition);
    float softness = min(pc.edge_softness, MAX_TEAR_REACH);
    float depth = min(pc.tear_depth, MAX_TEAR_REACH - softness);
    float threshold = depth * (0.2 + 0.8 * noise);
    float t = clamp(pc.transition, 0.0, 1.0);

    float d = distance_to_light();
    float lit = d <= 0.0 ? 1.0 : torn(d, threshold, softness);

    uint void_bits = inst.flags & NEIGHBOUR_BITS;
    uint revealing_bits = (inst.flags >> REVEALING_SHIFT) & NEIGHBOUR_BITS;
    uint known_mode = (inst.flags >> KNOWN_SHIFT) & 3u;

    float known;
    if (known_mode == KNOWN_FRONT) {
        float front = surface_front(true);
        known = max(front <= 0.0 ? 1.0 : torn(front, threshold, softness), lit);
    } else {
        known = (known_mode == KNOWN_FULL || t >= 1.0) ? 1.0 : step(noise, t);
    }

    float void_threshold = depth * (0.2 + 0.8 * tear_noise(worldPosition + VOID_NOISE_OFFSET));
    uint revealing_voids = known_mode == KNOWN_FULL ? revealing_bits : 0u;
    float void_torn = torn(distance_to_void(void_bits, revealing_voids, threshold),
                           void_threshold, softness);

    known = min(known, 1.0 - void_torn);
    if ((inst.flags & (1u << HIDDEN_BIT)) != 0u)
        known = 0.0;

    outMask = vec4(lit, known, 0.0, 1.0);
}
