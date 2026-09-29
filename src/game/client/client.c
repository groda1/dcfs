#include "HandmadeMath.h"
#include "array_queue.h"
#include "core.h"
#include "core_math.h"
#include "log.h"

#include "client.h"
#include "engine_types.h"
#include "sight.h"
#include "los.h"
#include "level.h"
#include "memory_arena.h"
#include "mesh.h"
#include "model.h"
#include "platform.h"
#include "render_types.h"
#include "renderer.h"
#include "frog.h"
#include "rules.h"

#define FLOOR_TILE_COLOR_A          V4(0.30f, 0.42f, 0.28f, 1.0f)
#define FLOOR_TILE_COLOR_B          V4(0.23f, 0.35f, 0.22f, 1.0f)
#define WALL_COLOR                  V4(0.4f, 0.4f, 0.2f, 1.0f)
#define WALL_HEIGHT                 2.0f

#define RENDER_WIDTH                (640 * 1)
#define RENDER_HEIGHT               (360 * 1)

#define REMEMBERED_BRIGHTNESS              0.25f
#define REMEMBERED_DESATURATION            0.8f

#define PLAYER_SCALE                1.0f
#define PLAYER_MOVE_SPEED           7.0f
#define PLAYER_BUMP_SPEED           5.0f
#define PLAYER_BUMP_DISTANCE        0.05f
#define PLAYER_WALK_ANIM_SPEED      1.71f

#define CAMERA_BOT_CLAMP            -2.0f
#define CAMERA_TOP_CLAMP            5.0f
#define CAMERA_RIGHT_CLAMP          7.0f
#define CAMERA_LEFT_CLAMP           7.0f

#define CAMERA_DEFAULT_PITCH_DEG    65.0f
#define CAMERA_DEFAULT_DISTANCE     12.0f
#define CAMERA_DEFAULT_FOV_DEG      60.0f

#define CAMERA_CLOSEUP_DISTANCE     4.0f
#define CAMERA_CLOSEUP_EYE_HEIGHT   2.5f
#define CAMERA_CLOSEUP_LOOK_HEIGHT  1.0f
#define CAMERA_CLOSEUP_FOV_DEG      35.0f

#define CAMERA_OVERVIEW_RADIUS      10.0f

#define CAMERA_FOLLOW_SPEED         8.0f
#define CAMERA_FLY_SPEED            2.5f
#define CAMERA_NEAR                 0.1f
#define CAMERA_FAR                  100.0f

#define GAME_SLOWMOTION_FACTOR      1.0f

#define OUTBOX_CAPACITY             128

typedef struct
{
    sbo_push_constant_t sbo;
} tile_push_constant_t;

typedef struct
{
    mat4 transform;
    vec4 color;
} tile_instance_t;
StaticAssert(sizeof(tile_instance_t) == 80, "quad_instance_t must match the shader's std430 stride");

typedef struct
{
    sbo_push_constant_t sbo;
    f32 transition;
    f32 pad;
} sight_mask_push_constant_t;

typedef struct
{
    vec2 position;
    vec2 size;
    texture_handle_t color_texture;
    texture_handle_t mask_texture;
    f32 remembered_brightness;
    f32 remembered_desaturation;
} sight_post_instance_t;
StaticAssert(sizeof(sight_post_instance_t) == 32, "sight_post_instance_t must match the shader's std430 stride");

typedef enum
{
    PLAYER_ANIM_NONE,
    PLAYER_ANIM_MOVE,
    PLAYER_ANIM_BUMP,
} player_anim_t;

typedef enum
{
    CAMERA_MODE_DEFAULT,
    CAMERA_MODE_CLOSEUP,
    CAMERA_MODE_OVERVIEW,
} camera_mode_t;

typedef struct
{
    vec3 eye;
    vec3 look;
    vec3 up;
    mat4 proj;
} camera_rig_t;

typedef struct
{
    arena_t *arena;

    mesh_handle_t cube_mesh;
    mesh_handle_t quad_mesh;
    mesh_handle_t screen_quad_mesh;

    texture_handle_t scene_texture;
    texture_handle_t sight_mask_texture;
    renderpass_handle_t scene_pass;
    renderpass_handle_t sight_mask_pass;
    model_handle_t player_model;
    model_instance_handle_t player_model_instance;
    model_animation_handle_t player_walk_l_anim;
    model_animation_handle_t player_walk_r_anim;
    model_animation_handle_t player_attack_anim;
    model_animation_handle_t player_bump_anim;

    pipeline_handle_t floor_pipeline;
    buffer_object_handle_t floor_sbo;
    pipeline_handle_t wall_pipeline;
    buffer_object_handle_t wall_sbo;

    pipeline_handle_t player_pipeline;

    pipeline_handle_t sight_mask_pipeline;
    buffer_object_handle_t sight_mask_sbo;

    pipeline_handle_t sight_post_pipeline;
    buffer_object_handle_t sight_post_sbo;

    buffer_object_handle_t vp_uniform;

    i32 player_pos_x;
    i32 player_pos_y;
    i32 player_target_pos_x;
    i32 player_target_pos_y;

    player_anim_t player_anim;
    f32 player_anim_progress;

    vec3 player_gfx_pos;
    vec3 player_gfx_target_pos;

    f32  player_gfx_old_rot;
    f32  player_gfx_rot;
    f32  player_gfx_target_rot;

    vec3 camera_target;     /* smoothed follow point */
    camera_mode_t camera_mode;
    f32 camera_blend;       /* 0 -> 1 progress from the snapshot to the rig */

    camera_rig_t camera_cur;
    camera_rig_t camera_from;

    u32 step_count;

    arena_t *run_arena;
    level_t level;
    los_t los;
    bool run_active;
    bool los_dirty;

    i32 server_player_x;
    i32 server_player_y;

    bool move_pending;
    u32 pending_sequence;
    u32 next_sequence;

    bool paused;

    array_queue_t outbox;
} client_t;

static client_t g_client;

static void update_player(f32 delta_time);
static void update_player_move(f32 delta_time);
static void update_player_bump(f32 delta_time);
static void update_camera(f32 delta_time);
static camera_rig_t camera_rig_for(camera_mode_t mode, f32 aspect);
static void camera_set_mode(camera_mode_t mode);
static void draw_grid(void);
static void draw_player(void);
static void draw_sight_post(void);
static f32  sight_transition(void);
static vec3 tile_center(i32 x, i32 y);
static vec3 player_center(i32 x, i32 y);
static f32  wrap_angle_deg(f32 angle);
static bool player_attempt_move(i32 dx, i32 dy);
static void handle_level_init(const event_level_init_t *init);
static void handle_tile_reveal(const event_tile_reveal_t *reveal);
static void handle_player_moved(const event_player_moved_t *moved);
static void handle_run_start(void);
static void handle_turn_done(const event_turn_done_t *done);
static void snap_player_to(i32 x, i32 y);
static void send(const command_t *command);

bool Client_Init(void)
{
    g_client.arena = MemoryArena_Create("client-arena");
    g_client.run_arena = MemoryArena_Create("client-run-arena");
    g_client.outbox = ArrayQueue_Create(g_client.arena, sizeof(command_t), OUTBOX_CAPACITY);
    Sight_Init(WALL_HEIGHT);
    LoS_Init(&g_client.los, g_client.arena, LOS_RADIUS);

    g_client.cube_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_NORMALED_CUBE);
    g_client.quad_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_NORMALED_QUAD);
    g_client.screen_quad_mesh = MeshManager_GetPredefinedMesh(PREDEFINED_MESH_TEXTURED_QUAD);

    sampler_handle_t sampler = Renderer_CreateSampler();
    if (sampler == SAMPLER_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create sampler");
        goto error;
    }

    g_client.scene_texture = Renderer_CreateRenderTexture(RENDER_WIDTH, RENDER_HEIGHT,
                                                          RENDER_TEXTURE_FORMAT_SRGB, sampler);
    g_client.sight_mask_texture = Renderer_CreateRenderTexture(RENDER_WIDTH, RENDER_HEIGHT,
                                                             RENDER_TEXTURE_FORMAT_UNORM, sampler);
    if (g_client.scene_texture == TEXTURE_HANDLE_INVALID ||
        g_client.sight_mask_texture == TEXTURE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create render textures");
        goto error;
    }

    renderpass_config_t sight_mask_pass_config = {
        .order = 0,
        .target_count = 1,
        .targets = {
            { .texture = g_client.sight_mask_texture, .load = RENDER_TARGET_CLEAR },
        },
    };
    renderpass_config_t scene_pass_config = {
        .order = 1,
        .target_count = 2,
        .targets = {
            { .texture = g_client.scene_texture, .load = RENDER_TARGET_CLEAR },
            { .texture = g_client.sight_mask_texture, .load = RENDER_TARGET_LOAD },
        },
    };
    g_client.sight_mask_pass = Renderer_CreateRenderPass(&sight_mask_pass_config);
    g_client.scene_pass = Renderer_CreateRenderPass(&scene_pass_config);
    if (g_client.scene_pass == RENDERPASS_HANDLE_INVALID ||
        g_client.sight_mask_pass == RENDERPASS_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create render passes");
        goto error;
    }

    g_client.vp_uniform = Renderer_CreateUniformBuffer(sizeof(view_projection_t));
    if (g_client.vp_uniform == BUFFER_OBJECT_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create view projection uniform");
        goto error;
    }

    pipeline_config_t floor_pipeline_config = {
        .name = "grid-floor",
        .vertex_shader = Renderer_LoadShader("shaders/floor.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/floor.frag.spv"),
        .push_constant_size = sizeof(tile_push_constant_t),
        .vertex_layout = &VERTEX_LAYOUT_NORMAL,
        .uniform_binding_count = 1,
        .uniform_bindings = {
            {
                .binding = 0,
                .buffer_object = g_client.vp_uniform,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
    };
    g_client.floor_sbo = Renderer_CreateStorageBuffer(KB(64));

    g_client.floor_pipeline = Renderer_AddPipeline(g_client.scene_pass, &floor_pipeline_config);
    if (g_client.floor_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create floor pipeline");
        goto error;
    }

    pipeline_config_t wall_pipeline_config = {
        .name = "grid-wall",
        .vertex_shader = Renderer_LoadShader("shaders/wall.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/wall.frag.spv"),
        .push_constant_size = sizeof(tile_push_constant_t),
        .vertex_layout = &VERTEX_LAYOUT_NORMAL,
        .uniform_binding_count = 1,
        .uniform_bindings = {
            {
                .binding = 0,
                .buffer_object = g_client.vp_uniform,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
    };
    g_client.wall_sbo = Renderer_CreateStorageBuffer(KB(64));

    g_client.wall_pipeline = Renderer_AddPipeline(g_client.scene_pass, &wall_pipeline_config);
    if (g_client.wall_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create wall pipeline");
        goto error;
    }


    pipeline_config_t player_pipeline_config = {
        .name = "player",
        .vertex_shader = Renderer_LoadShader("shaders/frog_player.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/frog_player.frag.spv"),
        .push_constant_size = sizeof(model_push_constant_t),
        .vertex_layout = &VERTEX_LAYOUT_NORMAL_MATERIAL,
        .vertex_streams = 2,
        .color_output_count = 2,
        .uniform_binding_count = 1,
        .uniform_bindings = {
            {
                .binding = 0,
                .buffer_object = g_client.vp_uniform,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
    };

    g_client.player_pipeline = Renderer_AddPipeline(g_client.scene_pass, &player_pipeline_config);
    if (g_client.player_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create player pipeline");
        goto error;
    }

    pipeline_config_t sight_mask_pipeline_config = {
        .name = "sight-mask",
        .vertex_shader = Renderer_LoadShader("shaders/sight_mask.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/sight_mask.frag.spv"),
        .push_constant_size = sizeof(sight_mask_push_constant_t),
        .vertex_layout = &VERTEX_LAYOUT_NORMAL,
        .uniform_binding_count = 1,
        .uniform_bindings = {
            {
                .binding = 0,
                .buffer_object = g_client.vp_uniform,
                .stage = UNIFORM_STAGE_VERTEX,
            },
        },
    };
    g_client.sight_mask_sbo = Renderer_CreateStorageBuffer(KB(256));

    g_client.sight_mask_pipeline = Renderer_AddPipeline(g_client.sight_mask_pass, &sight_mask_pipeline_config);
    if (g_client.sight_mask_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create sight mask pipeline");
        goto error;
    }

    pipeline_config_t sight_post_pipeline_config = {
        .name = "sight-post",
        .vertex_shader = Renderer_LoadShader("shaders/sight_post.vert.spv"),
        .fragment_shader = Renderer_LoadShader("shaders/sight_post.frag.spv"),
        .push_constant_size = sizeof(tile_push_constant_t),
        .vertex_layout = &VERTEX_LAYOUT_TEXTURED,
        .disable_depth_test = true,
    };
    g_client.sight_post_sbo = Renderer_CreateStorageBuffer(sizeof(sight_post_instance_t));

    g_client.sight_post_pipeline = Renderer_AddPipeline(SWAPCHAIN_PASS_HANDLE, &sight_post_pipeline_config);
    if (g_client.sight_post_pipeline == PIPELINE_HANDLE_INVALID)
    {
        Log(ERROR, "failed to create sight post pipeline");
        goto error;
    }

    g_client.player_model = Frog_LoadModel("resources/models/player.frog");
    if (g_client.player_model == MODEL_INVALID_HANDLE)
    {
        Log(ERROR, "failed to load player model");
        goto error;
    }

    g_client.player_model_instance = ModelInstance_New(g_client.player_model);
    if (g_client.player_model_instance == MODEL_INSTANCE_INVALID_HANDLE)
    {
        Log(ERROR, "failed to create player model instance");
        goto error;
    }

    if (!ModelInstance_SetIdleAnimation(g_client.player_model_instance, "idle"))
    {
        Log(ERROR, "failed to set idle animation");
        goto error;
    }

    g_client.player_walk_r_anim = Model_GetAnimation(g_client.player_model, "walk_r");
    g_client.player_walk_l_anim = Model_GetAnimation(g_client.player_model, "walk_l");
    g_client.player_attack_anim = Model_GetAnimation(g_client.player_model, "attack");
    g_client.player_bump_anim   = Model_GetAnimation(g_client.player_model, "bump");
    if (g_client.player_walk_r_anim == MODEL_ANIMATION_INVALID_HANDLE ||
        g_client.player_walk_l_anim == MODEL_ANIMATION_INVALID_HANDLE ||
        g_client.player_attack_anim == MODEL_ANIMATION_INVALID_HANDLE ||
        g_client.player_bump_anim == MODEL_ANIMATION_INVALID_HANDLE)
    {
        Log(ERROR, "failed load player animation");
        goto error;
    }

    g_client.camera_mode = CAMERA_MODE_DEFAULT;

    return true;

error:
    MemoryArena_Destroy(g_client.run_arena);
    MemoryArena_Destroy(g_client.arena);
    return false;
}

void Client_Destroy(void)
{
    MemoryArena_Print(g_client.arena);
    MemoryArena_Destroy(g_client.run_arena);
    MemoryArena_Destroy(g_client.arena);
}

void Client_HandleKeyDown(key_code_t key)
{
    client_t *client = &g_client;

    if (!client->run_active)
        return;

    if (key == KEY_V)
    {
        camera_set_mode(client->camera_mode == CAMERA_MODE_CLOSEUP
                            ? CAMERA_MODE_DEFAULT
                            : CAMERA_MODE_CLOSEUP);
        return;
    }

    if (key == KEY_O)
    {
        camera_set_mode(client->camera_mode == CAMERA_MODE_OVERVIEW
                            ? CAMERA_MODE_DEFAULT
                            : CAMERA_MODE_OVERVIEW);
        return;
    }
    if (key == KEY_L)
    {
        ModelInstance_PlayAnimation(client->player_model_instance, client->player_attack_anim);
        return;
    }

    if (key == KEY_F12)
    {
        Log(DEBUG, "toggle pause");
        client->paused = !client->paused;
    }

    if (client->player_anim == PLAYER_ANIM_NONE && !client->move_pending)
    {
        switch (key)
        {
            case KEY_D:
            case KEY_RIGHT:
            {
                player_attempt_move(1, 0);
                break;
            }
            case KEY_A:
            case KEY_LEFT:
            {
                player_attempt_move(-1, 0);
                break;
            }
            case KEY_W:
            case KEY_UP:
            {
                player_attempt_move(0, 1);
                break;
            }
            case KEY_S:
            case KEY_DOWN:
            {
                player_attempt_move(0, -1);
                break;
            }
            case KEY_KP_7:
            case KEY_HOME:
            {
                player_attempt_move(-1, 1);
                break;
            }
            case KEY_KP_1:
            case KEY_END:
            {
                player_attempt_move(-1, -1);
                break;
            }
            case KEY_KP_9:
            case KEY_PGUP:
            {
                player_attempt_move(1, 1);
                break;
            }
            case KEY_KP_3:
            case KEY_PGDN:
            {
                player_attempt_move(1, -1);
                break;
            }

            default:
                break;
        }
    }
}

void Client_Update(f32 delta_time)
{
    if (!g_client.run_active)
        return;
    if (g_client.paused)
        goto draw;

    delta_time *= GAME_SLOWMOTION_FACTOR;

    if (g_client.los_dirty)
    {
        LoS_Compute(&g_client.los, &g_client.level, g_client.player_target_pos_x, g_client.player_target_pos_y);
        g_client.los_dirty = false;
    }

    update_player(delta_time);
    update_camera(delta_time);

draw:
    draw_grid();
    draw_player();
    draw_sight_post();
}

void Client_HandleEvent(const event_t *event)
{
    Protocol_LogEvent("client", event);

    switch (event->type)
    {
        case RUN_START:
            handle_run_start();
            break;
        case LEVEL_INIT:
            handle_level_init(&event->level_init);
            break;
        case TILE_REVEAL:
            handle_tile_reveal(&event->tile_reveal);
            break;
        case PLAYER_MOVED:
            handle_player_moved(&event->player_moved);
            break;
        case TURN_DONE:
            handle_turn_done(&event->turn_done);
            break;
    }
}

bool Client_PollCommand(command_t *command)
{
    return ArrayQueue_Pop(&g_client.outbox, command);
}

static void update_player(f32 delta_time)
{
    ModelInstance_Update(g_client.player_model_instance, delta_time);

    switch (g_client.player_anim)
    {
        case PLAYER_ANIM_MOVE:
            update_player_move(delta_time);
            break;
        case PLAYER_ANIM_BUMP:
            update_player_bump(delta_time);
            break;
        case PLAYER_ANIM_NONE:
            break;
    }
}

static void update_player_move(f32 delta_time)
{
    client_t *client = &g_client;

    client->player_anim_progress += delta_time * PLAYER_MOVE_SPEED;

    f32 t = client->player_anim_progress;
    if (t > 1.0f)
        t = 1.0f;

    vec3 old = player_center(client->player_pos_x, client->player_pos_y);
    client->player_gfx_pos = lerp(old, t, client->player_gfx_target_pos);
    client->player_gfx_rot = lerp(client->player_gfx_old_rot, smoothstep(t), client->player_gfx_target_rot);

    if (client->player_anim_progress >= 1.0f)
    {
        client->player_pos_x = client->player_target_pos_x;
        client->player_pos_y = client->player_target_pos_y;
        client->player_anim = PLAYER_ANIM_NONE;
        Log(DEBUG, "move complete %d,%d", client->player_pos_x, client->player_pos_y);
    }
}

static void update_player_bump(f32 delta_time)
{
    client_t *client = &g_client;

    client->player_anim_progress += delta_time * PLAYER_BUMP_SPEED;

    f32 t = client->player_anim_progress;
    if (t > 1.0f)
        t = 1.0f;

    vec3 old = player_center(client->player_pos_x, client->player_pos_y);
    client->player_gfx_pos = lerp(old, smootherstep(outbackstep(t)) * PLAYER_BUMP_DISTANCE,
                                      client->player_gfx_target_pos);
    client->player_gfx_rot = lerp(client->player_gfx_old_rot, smootherstep(outbackstep(t)), client->player_gfx_target_rot);

    if (client->player_anim_progress >= 1.0f)
    {
        client->player_gfx_pos = old;
        client->player_anim = PLAYER_ANIM_NONE;
    }
}

static camera_rig_t camera_rig_for(camera_mode_t mode, f32 aspect)
{
    client_t *client = &g_client;

    camera_rig_t rig;

    switch (mode)
    {
        case CAMERA_MODE_CLOSEUP:
        {
            /* parked due south of the player (+Z), whichever way it faces */
            rig.eye = HMM_AddV3(client->player_gfx_pos,
                                V3(0.0f,
                                   CAMERA_CLOSEUP_EYE_HEIGHT,
                                   CAMERA_CLOSEUP_DISTANCE));
            rig.look = HMM_AddV3(client->player_gfx_pos,
                                 V3(0.0f, CAMERA_CLOSEUP_LOOK_HEIGHT, 0.0f));
            rig.up = V3(0.0f, 1.0f, 0.0f);
            rig.proj = HMM_Perspective_RH_ZO(HMM_AngleDeg(CAMERA_CLOSEUP_FOV_DEG),
                                             aspect, CAMERA_NEAR, CAMERA_FAR);
            break;
        }
        case CAMERA_MODE_OVERVIEW:
        {
            /* a fixed window around the player, not the whole map */
            vec3 center = V3(client->player_gfx_pos.X, 0.0f, client->player_gfx_pos.Z);

            f32 half_w = CAMERA_OVERVIEW_RADIUS;
            f32 half_h = CAMERA_OVERVIEW_RADIUS;
            if (aspect >= 1.0f)
                half_w = half_h * aspect;
            else
                half_h = half_w / aspect;

            f32 height = half_h / HMM_TanF(HMM_AngleDeg(CAMERA_DEFAULT_FOV_DEG) * 0.5f);

            rig.eye = HMM_AddV3(center, V3(0.0f, height, 0.0f));
            rig.look = center;
            rig.up = V3(0.0f, 0.0f, -1.0f);
            rig.proj = HMM_Orthographic_RH_ZO(-half_w, half_w, -half_h, half_h,
                                              CAMERA_NEAR, CAMERA_FAR);
            break;
        }
        case CAMERA_MODE_DEFAULT:
        default:
        {
            f32 pitch = HMM_AngleDeg(CAMERA_DEFAULT_PITCH_DEG);

            rig.eye = HMM_AddV3(client->camera_target,
                                V3(0.0f,
                                   HMM_SinF(pitch) * CAMERA_DEFAULT_DISTANCE,
                                   HMM_CosF(pitch) * CAMERA_DEFAULT_DISTANCE));
            rig.look = client->camera_target;
            rig.up = V3(0.0f, 1.0f, 0.0f);
            rig.proj = HMM_Perspective_RH_ZO(HMM_AngleDeg(CAMERA_DEFAULT_FOV_DEG),
                                             aspect, CAMERA_NEAR, CAMERA_FAR);
            break;
        }
    }

    return rig;
}

static void camera_set_mode(camera_mode_t mode)
{
    client_t *client = &g_client;

    if (client->camera_mode == mode)
        return;

    client->camera_from = client->camera_cur;
    client->camera_blend = 0.0f;
    client->camera_mode = mode;
}

static void update_camera(f32 delta_time)
{
    client_t *client = &g_client;

    f32 follow = CAMERA_FOLLOW_SPEED * delta_time;
    if (follow > 1.0f)
        follow = 1.0f;

    vec3 new_target = {
        .X = Clamp(CAMERA_LEFT_CLAMP, client->player_gfx_pos.X, client->level.width - CAMERA_RIGHT_CLAMP),
        .Y = client->player_gfx_pos.Y,
        .Z = Clamp(-client->level.height + CAMERA_TOP_CLAMP, client->player_gfx_pos.Z, CAMERA_BOT_CLAMP),
    };

    /* the follow point keeps tracking while in close-up, so toggling back
       resumes where the follow camera would have been instead of snapping */
    client->camera_target = lerp(client->camera_target, follow, new_target);

    f32 step = CAMERA_FLY_SPEED * delta_time;
    client->camera_blend = Min(client->camera_blend + step, 1.0f);

    camera_rig_t rig = camera_rig_for(client->camera_mode,
                                      (f32)RENDER_WIDTH / (f32)RENDER_HEIGHT);

    f32 t = smoothstep(client->camera_blend);
    client->camera_cur.eye  = lerp(client->camera_from.eye, t, rig.eye);
    client->camera_cur.look = lerp(client->camera_from.look, t, rig.look);
    client->camera_cur.up   = lerp(client->camera_from.up, t, rig.up);
    client->camera_cur.proj = HMM_AddM4(HMM_MulM4F(client->camera_from.proj, 1.0f - t),
                                      HMM_MulM4F(rig.proj, t));

    view_projection_t vp = {
        .view = HMM_LookAt_RH(client->camera_cur.eye,
                              client->camera_cur.look,
                              client->camera_cur.up),
        .proj = client->camera_cur.proj,
    };
    Renderer_SetBufferObject(g_client.vp_uniform, &vp, sizeof(vp));
}

static void draw_grid(void)
{
    tile_push_constant_t push_constant = {};
    sight_mask_push_constant_t sight_mask_push_constant = {
        .transition = sight_transition(),
    };

    Renderer_ClearBufferObject(g_client.floor_sbo);
    Renderer_ClearBufferObject(g_client.wall_sbo);
    Renderer_ClearBufferObject(g_client.sight_mask_sbo);

    mat4 floor_scale = HMM_Scale(V3(1.0f, 1.0f, 1.0f));
    mat4 floor_rotation = HMM_Rotate_RH(HMM_AngleDeg(-90), V3(1.0f, 0.0f, 0.0f));
    u64  floor_instance_count = 0;

    mat4 wall_scale = HMM_Scale(V3(1.0f, WALL_HEIGHT, 1.0f));
    u64  wall_instance_count = 0;

    sight_mask_instance_t mask_instances[SIGHT_MAX_TILE_INSTANCES];
    u64  mask_instance_count = 0;

    // TODO: this can be heavily optimized
    i32 start_x =   ClampBot(0, g_client.player_pos_x - 20);
    i32 end_x =     ClampTop(g_client.player_pos_x + 20, (i32)g_client.level.width);
    i32 start_y =   ClampBot(0, g_client.player_pos_y - 12);
    i32 end_y =     ClampTop(g_client.player_pos_y + 12, (i32)g_client.level.height);

    for (i32 y = start_y; y < end_y; y++)
    {
        for (i32 x = start_x; x < end_x; x++)
        {
            const tile_t *tile = Level_GetTile(&g_client.level, x, y);
            if (!(tile->flags & FLAG_REVEALED) || tile->type == TILE_EMPTY)
                continue;

            vec3 floor_center = tile_center(x, y);

            u32 mask_count = Sight_WriteInstances(x, y, floor_center, mask_instances);
            Renderer_PushBufferObject(g_client.sight_mask_sbo, mask_instances, mask_count * sizeof(sight_mask_instance_t));
            mask_instance_count += mask_count;

            if (tile->type == TILE_WALL)
            {
                vec3 wall_center = floor_center;
                wall_center.Y = WALL_HEIGHT / 2.0f;
                tile_instance_t instance = {
                    .transform = HMM_MulM4(
                                    HMM_Translate(wall_center),
                                    wall_scale),
                    .color = WALL_COLOR,
                };
                Renderer_PushBufferObject(g_client.wall_sbo, &instance, sizeof(instance));
                wall_instance_count++;
            }
            else
            {
                tile_instance_t instance = {
                    .transform = HMM_MulM4(
                                    HMM_Translate(floor_center),
                                    HMM_MulM4(
                                        floor_rotation,
                                        floor_scale)),
                    .color = ((x + y) & 1) ? FLOOR_TILE_COLOR_A : FLOOR_TILE_COLOR_B,
                };
                Renderer_PushBufferObject(g_client.floor_sbo, &instance, sizeof(instance));
                floor_instance_count++;
            }
        }
    }

    Renderer_DrawMeshInstanced(g_client.scene_pass,
        g_client.floor_pipeline,
        &push_constant,
        g_client.floor_sbo,
        floor_instance_count, g_client.quad_mesh);

    Renderer_DrawMeshInstanced(g_client.scene_pass,
        g_client.wall_pipeline,
        &push_constant,
        g_client.wall_sbo,
        wall_instance_count, g_client.cube_mesh);

    Renderer_DrawMeshInstanced(g_client.sight_mask_pass,
        g_client.sight_mask_pipeline,
        &sight_mask_push_constant,
        g_client.sight_mask_sbo,
        mask_instance_count, g_client.quad_mesh);
}

static f32 sight_transition(void)
{
    if (g_client.player_anim != PLAYER_ANIM_MOVE)
        return 1.0f;

    return Min(g_client.player_anim_progress, 1.0f);
}

static void draw_player(void)
{
    mat4 transform = HMM_MulM4(
                        HMM_Translate( g_client.player_gfx_pos),
                        HMM_MulM4(
                            HMM_Rotate_RH(HMM_AngleDeg(g_client.player_gfx_rot), V3(0.0f, 1.0f, 0.0f)),
                            HMM_Scale(V3(PLAYER_SCALE, PLAYER_SCALE, PLAYER_SCALE))
                        )
                    );

    ModelInstance_Draw(g_client.player_model_instance, g_client.scene_pass,
                       g_client.player_pipeline, transform);
}

static void draw_sight_post(void)
{
    window_extent_t extent = Renderer_GetWindowExtent();

    u32 scale = Min(extent.width / RENDER_WIDTH, extent.height / RENDER_HEIGHT);
    if (scale < 1)
        scale = 1;

    sight_post_instance_t instance = {
        .position = V2(0.0f, 0.0f),
        .size = V2(2.0f * (f32)(RENDER_WIDTH * scale) / (f32)extent.width,
                   2.0f * (f32)(RENDER_HEIGHT * scale) / (f32)extent.height),
        .color_texture = g_client.scene_texture,
        .mask_texture = g_client.sight_mask_texture,
        .remembered_brightness = REMEMBERED_BRIGHTNESS,
        .remembered_desaturation = REMEMBERED_DESATURATION,
    };

    Renderer_ClearBufferObject(g_client.sight_post_sbo);
    Renderer_PushBufferObject(g_client.sight_post_sbo, &instance, sizeof(instance));

    tile_push_constant_t push_constant = {};
    Renderer_DrawMeshInstanced(SWAPCHAIN_PASS_HANDLE,
        g_client.sight_post_pipeline,
        &push_constant,
        g_client.sight_post_sbo,
        1, g_client.screen_quad_mesh);
}


static vec3 tile_center(i32 x, i32 y)
{
    return V3((f32)x + 0.5f, 0.0f, (f32)-y + 0.5f);
}

static vec3 player_center(i32 x, i32 y)
{
    return V3((f32)x + 0.5f, 0.0f, (f32)-y + 0.5f);
}

static inline f32 wrap_angle_deg(f32 angle)
{
    while (angle > 180.0f)
        angle -= 360.0f;
    while (angle < -180.0f)
        angle += 360.0f;
    return angle;
}

static bool player_attempt_move(i32 dx, i32 dy)
{
    client_t *client = &g_client;

    Assert(client->player_anim == PLAYER_ANIM_NONE);

    i32 new_x = client->player_pos_x + dx;
    i32 new_y = client->player_pos_y + dy;

    client->player_anim_progress = 0.0f;
    client->player_gfx_target_pos = player_center(new_x, new_y);

    f32 rotation = client->player_gfx_rot;

    if (new_x > client->player_pos_x && new_y > client->player_pos_y ) // northeast
        rotation = 135.0f;
    else if (new_x < client->player_pos_x && new_y > client->player_pos_y) // northwest
        rotation = -135.0f;
    else if (new_x > client->player_pos_x && new_y < client->player_pos_y ) // southeast
        rotation = 45.0f;
    else if (new_x < client->player_pos_x && new_y < client->player_pos_y ) // southwest
        rotation = -45.0f;
    else if (new_x > client->player_pos_x) // east
        rotation = 90.0f;
    else if (new_x < client->player_pos_x) // west
        rotation = -90.0f;
    else if (new_y > client->player_pos_y) // north
        rotation = 180.0f;
    else if (new_y < client->player_pos_y) // south
        rotation = 0.0f;

    client->player_gfx_old_rot = wrap_angle_deg(client->player_gfx_rot);
    client->player_gfx_target_rot = client->player_gfx_old_rot
                                + wrap_angle_deg(rotation - client->player_gfx_old_rot);

    bool is_blocked = !Level_InBounds(&client->level, new_x, new_y) ||
                      ((Level_GetTile(&client->level, new_x, new_y)->flags & FLAG_REVEALED) &&
                       !Rules_IsWalkable(&client->level, new_x, new_y));

    if (!is_blocked)
    {
        command_t move = {
            .type = COMMAND_MOVE,
            .sequence = client->next_sequence++,
            .move = {
                .dx = (i8)dx,
                .dy = (i8)dy,
            },
        };
        send(&move);
        client->move_pending = true;
        client->pending_sequence = move.sequence;

        client->player_target_pos_x = new_x;
        client->player_target_pos_y = new_y;
        client->los_dirty = true;
        client->player_anim = PLAYER_ANIM_MOVE;
        client->step_count++;

        model_animation_handle_t walk_anim;
        if (client->step_count % 2)
            walk_anim = client->player_walk_l_anim;
        else
            walk_anim = client->player_walk_r_anim;

        ModelInstance_PlayAnimationS(client->player_model_instance, walk_anim, PLAYER_WALK_ANIM_SPEED);
        return true;
    }

    // Bump
    client->player_anim = PLAYER_ANIM_BUMP;
    ModelInstance_PlayAnimation(client->player_model_instance, client->player_bump_anim);
    return false;
}

static void handle_level_init(const event_level_init_t *init)
{
    client_t *client = &g_client;

    Level_Init(&client->level, client->run_arena, init->level_width, init->level_height);
    Sight_Reset(&client->level);

    client->move_pending = false;
    client->los_dirty = true;
}

static void handle_tile_reveal(const event_tile_reveal_t *reveal)
{
    client_t *client = &g_client;

    if (!Level_InBounds(&client->level, reveal->x, reveal->y))
    {
        Log(WARNING, "client: tile reveal out of bounds %u,%u", reveal->x, reveal->y);
        return;
    }

    tile_t *tile = Level_GetTile(&client->level, reveal->x, reveal->y);
    tile->type = reveal->tile;
    tile->flags |= FLAG_REVEALED;

    client->los_dirty = true;
}

static void handle_player_moved(const event_player_moved_t *moved)
{
    client_t *client = &g_client;

    client->server_player_x = moved->x;
    client->server_player_y = moved->y;

    if (!client->move_pending)
        snap_player_to(moved->x, moved->y);
}

static void handle_run_start(void)
{
    client_t *client = &g_client;

    client->player_gfx_rot = 0.0f;
    client->player_gfx_target_rot = client->player_gfx_rot;
    client->camera_target = client->player_gfx_pos;
    client->camera_blend = 1.0f;

    client->run_active = true;
}

static void handle_turn_done(const event_turn_done_t *done)
{
    client_t *client = &g_client;

    if (!client->move_pending || done->sequence != client->pending_sequence)
        return;

    client->move_pending = false;

    if (client->server_player_x == client->player_target_pos_x
        && client->server_player_y == client->player_target_pos_y)
        return;

    Log(ERROR, "client: misprediction, snapping to %d,%d",
        client->server_player_x, client->server_player_y);
    snap_player_to(client->server_player_x, client->server_player_y);
}

static void snap_player_to(i32 x, i32 y)
{
    client_t *client = &g_client;

    client->player_pos_x = x;
    client->player_pos_y = y;
    client->player_target_pos_x = x;
    client->player_target_pos_y = y;
    client->player_anim = PLAYER_ANIM_NONE;
    client->player_gfx_pos = player_center(x, y);
    client->player_gfx_target_pos = client->player_gfx_pos;
    client->los_dirty = true;
}

static void send(const command_t *command)
{
    if (!ArrayQueue_Push(&g_client.outbox, command))
    {
        Log(ERROR, "client: outbox full, dropped command type=%u", command->type);
        Assert(false);
    }
}
