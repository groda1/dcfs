#include "core.h"
#include "log.h"

#include "vulkan_spirv.h"

/* minimal SPIR-V reader: just enough of the type system to name the vertex
   input variables. spec: https://registry.khronos.org/SPIR-V/ */

#define SPIRV_MAGIC   0x07230203u
#define SPIRV_MAX_IDS 4096

#define OP_TYPE_INT     21
#define OP_TYPE_FLOAT   22
#define OP_TYPE_VECTOR  23
#define OP_TYPE_POINTER 32
#define OP_VARIABLE     59
#define OP_DECORATE     71

#define DECORATION_BUILTIN  11
#define DECORATION_LOCATION 30

#define STORAGE_CLASS_INPUT 1

#define NO_LOCATION U32_MAX

typedef enum
{
    ID_UNKNOWN = 0,
    ID_TYPE_FLOAT,
    ID_TYPE_INT,
    ID_TYPE_VECTOR,
    ID_TYPE_POINTER,
} id_kind_t;

typedef struct
{
    u8  kind;
    u32 a; /* float/int: bit width; vector: component type id; pointer: pointee type id */
    u32 b; /* int: signedness; vector: component count; pointer: storage class */
} id_info_t;

/* only used during pipeline creation at init; not reentrant */
static id_info_t s_ids[SPIRV_MAX_IDS];
static u32       s_locations[SPIRV_MAX_IDS];
static bool      s_builtin[SPIRV_MAX_IDS];

static bool input_format(const id_info_t *type, u32 location, vertex_format_t *format_out);

bool VulkanSpirv_ReflectVertexInputs(shader_code_t shader, spirv_vertex_input_t *inputs_out,
                                     u32 max_inputs, u32 *input_count_out)
{
    const u32 *words = (const u32 *)shader.code;
    u64 word_count = shader.size / sizeof(u32);

    if (words == NULL || word_count < 5 || words[0] != SPIRV_MAGIC)
    {
        Log(ERROR, "not a SPIR-V blob");
        return false;
    }

    u32 bound = words[3];
    if (bound > SPIRV_MAX_IDS)
    {
        Log(ERROR, "SPIR-V id bound %u exceeds reflection limit %u", bound, SPIRV_MAX_IDS);
        return false;
    }

    MemoryZeroArray(s_ids);
    MemoryZeroArray(s_builtin);
    for (u32 i = 0; i < SPIRV_MAX_IDS; i++)
        s_locations[i] = NO_LOCATION;

    u32 input_count = 0;

    for (u64 at = 5; at < word_count;)
    {
        u32 opcode = words[at] & 0xFFFFu;
        u32 length = words[at] >> 16;

        if (length == 0 || at + length > word_count)
        {
            Log(ERROR, "malformed SPIR-V instruction stream");
            return false;
        }

        const u32 *operands = &words[at + 1];

        switch (opcode)
        {
        case OP_DECORATE:
            if (length >= 4 && operands[0] < bound)
            {
                if (operands[1] == DECORATION_LOCATION)
                    s_locations[operands[0]] = operands[2];
            }
            if (length >= 3 && operands[0] < bound && operands[1] == DECORATION_BUILTIN)
                s_builtin[operands[0]] = true;
            break;

        case OP_TYPE_FLOAT:
            if (length >= 3 && operands[0] < bound)
                s_ids[operands[0]] = (id_info_t){.kind = ID_TYPE_FLOAT, .a = operands[1]};
            break;

        case OP_TYPE_INT:
            if (length >= 4 && operands[0] < bound)
                s_ids[operands[0]] = (id_info_t){.kind = ID_TYPE_INT, .a = operands[1],
                                                 .b = operands[2]};
            break;

        case OP_TYPE_VECTOR:
            if (length >= 4 && operands[0] < bound)
                s_ids[operands[0]] = (id_info_t){.kind = ID_TYPE_VECTOR, .a = operands[1],
                                                 .b = operands[2]};
            break;

        case OP_TYPE_POINTER:
            if (length >= 4 && operands[0] < bound)
                s_ids[operands[0]] = (id_info_t){.kind = ID_TYPE_POINTER, .a = operands[2],
                                                 .b = operands[1]};
            break;

        case OP_VARIABLE:
            if (length >= 4 && operands[0] < bound && operands[1] < bound &&
                operands[2] == STORAGE_CLASS_INPUT)
            {
                u32 variable_id = operands[1];
                u32 location = s_locations[variable_id];

                if (s_builtin[variable_id] || location == NO_LOCATION)
                    break; /* gl_VertexIndex and friends */

                const id_info_t *pointer = &s_ids[operands[0]];
                if (pointer->kind != ID_TYPE_POINTER || pointer->a >= bound)
                {
                    Log(ERROR, "SPIR-V input variable %u has no pointer type", variable_id);
                    return false;
                }

                vertex_format_t format;
                if (!input_format(&s_ids[pointer->a], location, &format))
                    return false;

                if (input_count >= max_inputs)
                {
                    Log(ERROR, "vertex shader has more than %u inputs", max_inputs);
                    return false;
                }

                inputs_out[input_count++] = (spirv_vertex_input_t){
                    .location = location,
                    .format = format,
                };
            }
            break;

        default:
            break;
        }

        at += length;
    }

    *input_count_out = input_count;
    return true;
}

static bool input_format(const id_info_t *type, u32 location, vertex_format_t *format_out)
{
    switch (type->kind)
    {
    case ID_TYPE_FLOAT:
        if (type->a == 32)
        {
            *format_out = VERTEX_FORMAT_F32;
            return true;
        }
        break;

    case ID_TYPE_INT:
        if (type->a == 32 && type->b == 0) /* unsigned */
        {
            *format_out = VERTEX_FORMAT_U32;
            return true;
        }
        break;

    case ID_TYPE_VECTOR:
    {
        const id_info_t *component = &s_ids[type->a];
        if (component->kind == ID_TYPE_FLOAT && component->a == 32 &&
            type->b >= 2 && type->b <= 4)
        {
            *format_out = (vertex_format_t)(VERTEX_FORMAT_F32 + type->b - 1);
            return true;
        }
        break;
    }

    default:
        break;
    }

    Log(ERROR, "vertex shader input location %u has a type the engine cannot feed "
        "(only f32 scalars/vectors and u32 are supported)", location);
    return false;
}
