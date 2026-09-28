#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_buffer_reference : require

layout(std430, buffer_reference) readonly buffer InstanceData {
    uint unused[];
};

layout(push_constant) uniform pushConstants {
    InstanceData instance_data;
    float tear_depth;
    float edge_softness;
    float transition;
    float origin_x;
    float origin_z;
    float radial_sweep_radius;
} pc;

layout(location = 0) in vec3 worldPosition;
layout(location = 1) flat in ivec2 tile;
layout(location = 2) flat in uint visibility;
layout(location = 3) flat in uint previous;

layout(location = 0) out vec4 outMask;

const uint SELF_BIT = 4u;
const uint NEIGHBOUR_BITS = 0x1EFu;
const uint PREVIOUSLY_KNOWN_BIT = 9u;
const uint VOID_SHIFT = 10u;
const vec3 VOID_NOISE_OFFSET = vec3(37.1, 11.3, 23.7);
const float MAX_TEAR_REACH = 0.45;
const float SWEEP_REACH = 1.0;
const float NO_NEIGHBOUR = 2.0;
const float MIN_TEAR = 0.02;

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

float neighbour_distance(uint bits, vec2 local) {
    float distance_to_set = NO_NEIGHBOUR;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            uint bit = uint((dy + 1) * 3 + (dx + 1));
            if (bit == SELF_BIT || (bits & (1u << bit)) == 0u)
                continue;

            vec2 lo = vec2(dx, dy);
            vec2 hi = lo + 1.0;
            vec2 outside = max(max(lo - local, local - hi), 0.0);
            distance_to_set = min(distance_to_set, length(outside));
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

float radial(float player_distance, float front, float jitter, float softness) {
    float d = player_distance + jitter;
    return softness > 0.0
         ? 1.0 - smoothstep(front, front + softness, d)
         : step(d, front);
}

float front_threshold(float threshold, float progress) {
    return clamp(threshold + progress * SWEEP_REACH - 1.0, 0.0, threshold);
}

void main() {
    vec2 local = vec2(worldPosition.x - float(tile.x), float(1 - tile.y) - worldPosition.z);

    float noise = tear_noise(worldPosition);
    float softness = min(pc.edge_softness, MAX_TEAR_REACH);
    float depth = min(pc.tear_depth, MAX_TEAR_REACH - softness);
    float threshold = depth * (0.2 + 0.8 * noise);
    float t = clamp(pc.transition, 0.0, 1.0);
    float player_distance = length(worldPosition.xz - vec2(pc.origin_x, pc.origin_z));

    uint now = visibility & 0x1FFu;
    uint before = previous & 0x1FFu;
    uint void_now = (visibility >> VOID_SHIFT) & NEIGHBOUR_BITS;
    uint void_before = (previous >> VOID_SHIFT) & NEIGHBOUR_BITS;
    bool visible_now = (now & (1u << SELF_BIT)) != 0u;
    bool visible_before = (before & (1u << SELF_BIT)) != 0u;
    bool known_before = (previous & (1u << PREVIOUSLY_KNOWN_BIT)) != 0u;

    float lit;
    if (t >= 1.0 || (visible_now && visible_before)) {
        lit = visible_now ? 1.0 : torn(neighbour_distance(now, local), threshold, softness);
    } else if (visible_now) {
        float d = neighbour_distance(before, local);
        lit = d < NO_NEIGHBOUR
            ? torn(d, threshold + t * SWEEP_REACH, softness)
            : radial(player_distance, t * pc.radial_sweep_radius, threshold, softness);
    } else if (visible_before) {
        float d = neighbour_distance(now, local);
        lit = d < NO_NEIGHBOUR
            ? torn(d, threshold + (1.0 - t) * SWEEP_REACH, softness)
            : radial(player_distance, (1.0 - t) * pc.radial_sweep_radius, threshold, softness);
    } else {
        uint kept = now & before & NEIGHBOUR_BITS;
        uint lost = before & ~now & NEIGHBOUR_BITS;
        uint gained = now & ~before & NEIGHBOUR_BITS;

        float lost_threshold = front_threshold(threshold, 1.0 - t);
        float gained_threshold = front_threshold(threshold, t);
        float softness_scale = softness / max(threshold, 1e-5);

        lit = max(torn(neighbour_distance(kept, local), threshold, softness),
              max(torn(neighbour_distance(lost, local), lost_threshold,
                       softness_scale * lost_threshold),
                  torn(neighbour_distance(gained, local), gained_threshold,
                       softness_scale * gained_threshold)));
    }

    float known = (known_before || t >= 1.0) ? 1.0
                : visible_now ? lit
                : step(noise, t);

    float void_threshold = depth * (0.2 + 0.8 * tear_noise(worldPosition + VOID_NOISE_OFFSET));
    float void_torn = max(
        torn(neighbour_distance(void_now, local), void_threshold, softness),
        known_before ? torn(neighbour_distance(void_before & ~void_now, local),
                            void_threshold * (1.0 - t), softness * (1.0 - t))
                     : 0.0);

    known = min(known, 1.0 - void_torn);

    outMask = vec4(lit, known, 0.0, 1.0);
}
