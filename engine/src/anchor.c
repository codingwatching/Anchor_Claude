/*
 * Anchor Engine
 * Phase 1: Window + OpenGL + Lua integration
 * Phase 2: Web build (Emscripten/WebGL)
 * Phase 3: Rendering (layers, shapes, sprites, transforms, blend modes)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <direct.h>  // _chdir
#else
#include <unistd.h>  // chdir
#endif

#include <SDL.h>

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/html5.h>
    #include <GLES3/gl3.h>
#else
    #include <glad/gl.h>
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define WINDOW_TITLE "Anchor"
#define GAME_WIDTH 480
#define GAME_HEIGHT 270
#define INITIAL_SCALE 3

// Timing configuration
#define PHYSICS_RATE (1.0 / 120.0)  // 120 Hz physics/input timestep
#define RENDER_RATE  (1.0 / 60.0)   // 60 Hz render cap (for chunky pixel movement)
#define MAX_UPDATES 10              // Cap on fixed steps per frame (prevents spiral of death)

// VSync snapping - snap delta times within 0.2ms of common refresh rates
// This prevents accumulator drift from timer jitter
#define VSYNC_SNAP_TOLERANCE 0.0002

// Forward declaration for timing resync (defined with main loop state)
static void timing_resync(void);

// Transform stack depth
#define MAX_TRANSFORM_DEPTH 32

// Command queue capacity (fixed size, allocated once)
// 16384 commands × ~64 bytes = ~1MB per layer
#define MAX_COMMAND_CAPACITY 16384

// Command types
enum {
    COMMAND_RECTANGLE = 0,
    COMMAND_CIRCLE,
    COMMAND_SPRITE,
    COMMAND_APPLY_SHADER,       // Post-process layer through a shader
    COMMAND_SET_UNIFORM_FLOAT,  // Set float uniform on shader
    COMMAND_SET_UNIFORM_VEC2,   // Set vec2 uniform on shader
    COMMAND_SET_UNIFORM_VEC4,   // Set vec4 uniform on shader
    COMMAND_SET_UNIFORM_INT,    // Set int uniform on shader
};

// Blend modes
enum {
    BLEND_ALPHA = 0,
    BLEND_ADDITIVE,
};

// DrawCommand — stores one deferred draw call
// Explicitly padded to 64 bytes for consistent memory layout across platforms
//
// Coordinate conventions:
//   RECTANGLE: x,y is top-left corner, w,h extend right and down (matches SDL/LÖVE)
//   CIRCLE: x,y is center, radius extends outward
//   SPRITE: x,y is center (texture drawn centered at that point)
typedef struct {
    uint8_t type;           // COMMAND_RECTANGLE, COMMAND_CIRCLE, COMMAND_SPRITE, COMMAND_APPLY_SHADER, COMMAND_SET_UNIFORM_*
    uint8_t blend_mode;     // BLEND_ALPHA, BLEND_ADDITIVE, BLEND_MULTIPLY
    uint8_t _pad[2];        // Padding to align next field to 4 bytes

    float transform[6];     // 2D affine matrix (2x3): [m00 m01 m02 m10 m11 m12] (24 bytes)
    uint32_t color;         // Packed RGBA for multiply/tint; For SET_UNIFORM_*: uniform location (4 bytes)

    // Shape parameters (meaning depends on type)
    // RECTANGLE: params[0]=x, [1]=y, [2]=w, [3]=h
    // CIRCLE: params[0]=x, [1]=y, [2]=radius
    // SPRITE: params[0]=x, [1]=y, [2]=w, [3]=h, [4]=ox, [5]=oy (+ texture_id)
    // SET_UNIFORM_FLOAT: params[0]=value
    // SET_UNIFORM_VEC2: params[0]=x, [1]=y
    // SET_UNIFORM_VEC4: params[0]=x, [1]=y, [2]=z, [3]=w
    // SET_UNIFORM_INT: params[0]=value (as float, cast to int)
    float params[6];        // 24 bytes (reduced from 8 to fit 64-byte target)

    GLuint texture_id;      // For SPRITE: texture handle; For APPLY_SHADER: shader handle (4 bytes)
    uint32_t flash_color;   // Packed RGB for additive flash (uses only RGB, alpha ignored)
    // Total: 4 + 24 + 4 + 24 + 4 + 4 = 64 bytes
} DrawCommand;

// Verify DrawCommand is exactly 64 bytes (compile-time check)
#ifdef _MSC_VER
    static_assert(sizeof(DrawCommand) == 64, "DrawCommand must be 64 bytes");
#else
    _Static_assert(sizeof(DrawCommand) == 64, "DrawCommand must be 64 bytes");
#endif

// Layer
typedef struct {
    GLuint fbo;
    GLuint color_texture;
    int width;
    int height;

    // Effect ping-pong buffers (created on first use)
    GLuint effect_fbo;
    GLuint effect_texture;
    bool textures_swapped;  // Which buffer is current result

    // Transform stack (mat3 stored as 9 floats: row-major)
    // Each mat3: [m00 m01 m02 m10 m11 m12 m20 m21 m22]
    // Represents 2D affine transform (2x3 used, bottom row is 0,0,1)
    float transform_stack[MAX_TRANSFORM_DEPTH * 9];
    int transform_depth;

    // Command queue (deferred rendering)
    DrawCommand* commands;
    int command_count;
    int command_capacity;

    // Current state
    uint8_t current_blend;
} Layer;

// Texture
typedef struct {
    GLuint id;
    int width;
    int height;
} Texture;

// Load a texture from file using stb_image
static Texture* texture_load(const char* path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(0);  // Don't flip - we handle Y in our coordinate system
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);  // Force RGBA
    if (!data) {
        fprintf(stderr, "Failed to load texture: %s\n", path);
        return NULL;
    }

    Texture* tex = (Texture*)malloc(sizeof(Texture));
    if (!tex) {
        stbi_image_free(data);
        return NULL;
    }

    tex->width = width;
    tex->height = height;

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    printf("Loaded texture: %s (%dx%d)\n", path, width, height);
    return tex;
}

static void texture_destroy(Texture* tex) {
    if (!tex) return;
    if (tex->id) glDeleteTextures(1, &tex->id);
    free(tex);
}

// Create a layer with FBO at specified resolution
static Layer* layer_create(int width, int height) {
    Layer* layer = (Layer*)calloc(1, sizeof(Layer));
    if (!layer) return NULL;

    layer->width = width;
    layer->height = height;

    // Initialize transform stack with identity matrix at depth 0
    layer->transform_depth = 0;
    float* m = layer->transform_stack;
    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f;  // row 0
    m[3] = 0.0f; m[4] = 1.0f; m[5] = 0.0f;  // row 1
    m[6] = 0.0f; m[7] = 0.0f; m[8] = 1.0f;  // row 2

    // Initialize command queue (fixed size, never grows)
    layer->commands = (DrawCommand*)malloc(MAX_COMMAND_CAPACITY * sizeof(DrawCommand));
    if (!layer->commands) {
        free(layer);
        return NULL;
    }
    layer->command_count = 0;
    layer->command_capacity = MAX_COMMAND_CAPACITY;
    layer->current_blend = BLEND_ALPHA;

    // Create FBO
    glGenFramebuffers(1, &layer->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, layer->fbo);

    // Create color texture
    glGenTextures(1, &layer->color_texture);
    glBindTexture(GL_TEXTURE_2D, layer->color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, layer->color_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Layer FBO not complete\n");
        glDeleteTextures(1, &layer->color_texture);
        glDeleteFramebuffers(1, &layer->fbo);
        free(layer);
        return NULL;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return layer;
}

static void layer_destroy(Layer* layer) {
    if (!layer) return;
    if (layer->commands) free(layer->commands);
    if (layer->color_texture) glDeleteTextures(1, &layer->color_texture);
    if (layer->fbo) glDeleteFramebuffers(1, &layer->fbo);
    // Effect ping-pong buffers
    if (layer->effect_texture) glDeleteTextures(1, &layer->effect_texture);
    if (layer->effect_fbo) glDeleteFramebuffers(1, &layer->effect_fbo);
    free(layer);
}

// Ensure effect buffer exists (lazy creation)
static void layer_ensure_effect_buffer(Layer* layer) {
    if (layer->effect_fbo != 0) return;  // Already created

    // Create effect texture
    glGenTextures(1, &layer->effect_texture);
    glBindTexture(GL_TEXTURE_2D, layer->effect_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, layer->width, layer->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create effect FBO
    glGenFramebuffers(1, &layer->effect_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, layer->effect_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, layer->effect_texture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Effect framebuffer incomplete: 0x%x\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Get the current result texture (accounts for ping-pong state)
static GLuint layer_get_texture(Layer* layer) {
    return layer->textures_swapped ? layer->effect_texture : layer->color_texture;
}

// Reset effect state for start of frame (call after layer_flush, before new frame)
static void layer_reset_effects(Layer* layer) {
    layer->textures_swapped = false;
}

// Get pointer to current transform (mat3 at current depth)
static float* layer_get_transform(Layer* layer) {
    return &layer->transform_stack[layer->transform_depth * 9];
}

// Copy current transform to a 2x3 array (for DrawCommand)
static void layer_copy_transform(Layer* layer, float* dest) {
    float* src = layer_get_transform(layer);
    // Copy first two rows (6 floats) - third row is always [0, 0, 1]
    dest[0] = src[0]; dest[1] = src[1]; dest[2] = src[2];
    dest[3] = src[3]; dest[4] = src[4]; dest[5] = src[5];
}

// Add a command to the layer's queue (returns pointer to the new command)
// Returns NULL if queue is full (MAX_COMMAND_CAPACITY reached)
static DrawCommand* layer_add_command(Layer* layer) {
    if (layer->command_count >= layer->command_capacity) {
        // Fixed size queue - don't grow, just drop the command
        // This should never happen in normal use (16384 commands per frame is huge)
        static bool warned = false;
        if (!warned) {
            fprintf(stderr, "Error: Command queue full (%d commands). Dropping draw calls.\n",
                    layer->command_capacity);
            warned = true;
        }
        return NULL;
    }

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->blend_mode = layer->current_blend;
    layer_copy_transform(layer, cmd->transform);
    return cmd;
}

// Clear all commands (call at frame end after rendering)
static void layer_clear_commands(Layer* layer) {
    layer->command_count = 0;
}

// Record a rectangle command
static void layer_add_rectangle(Layer* layer, float x, float y, float w, float h, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_RECTANGLE;
    cmd->color = color;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = w;
    cmd->params[3] = h;
}

// Record a circle command
static void layer_add_circle(Layer* layer, float x, float y, float radius, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_CIRCLE;
    cmd->color = color;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = radius;
}

// Record a sprite/image command (centered at x, y)
// color = multiply/tint color (RGBA), flash_color = additive flash color (RGB, alpha ignored)
static void layer_add_image(Layer* layer, Texture* tex, float x, float y, uint32_t color, uint32_t flash_color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_SPRITE;
    cmd->color = color;
    cmd->flash_color = flash_color;
    cmd->texture_id = tex->id;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = (float)tex->width;
    cmd->params[3] = (float)tex->height;
}

// Set the current blend mode for subsequent commands
static void layer_set_blend_mode(Layer* layer, uint8_t mode) {
    layer->current_blend = mode;
}

// Batch rendering
#define MAX_BATCH_VERTICES 6000  // 1000 quads * 6 vertices
#define VERTEX_FLOATS 16         // x, y, u, v, r, g, b, a, type, shape[4], addR, addG, addB

// Shape types for uber-shader
#define SHAPE_TYPE_RECT   0.0f
#define SHAPE_TYPE_CIRCLE 1.0f
#define SHAPE_TYPE_SPRITE 2.0f

// Shape filter mode (smooth = anti-aliased, rough = hard pixel edges)
enum {
    FILTER_SMOOTH = 0,
    FILTER_ROUGH,
};
static int shape_filter_mode = FILTER_SMOOTH;

static float batch_vertices[MAX_BATCH_VERTICES * VERTEX_FLOATS];
static int batch_vertex_count = 0;
static GLuint current_batch_texture = 0;  // Currently bound texture for batching

// Transform a point by a 2x3 matrix: [m0 m1 m2] [x]   [m0*x + m1*y + m2]
//                                    [m3 m4 m5] [y] = [m3*x + m4*y + m5]
//                                               [1]
static void transform_point(const float* m, float x, float y, float* out_x, float* out_y) {
    *out_x = m[0] * x + m[1] * y + m[2];
    *out_y = m[3] * x + m[4] * y + m[5];
}

// Multiply two 3x3 matrices: C = A * B (row-major order)
// For 2D affine transforms, bottom row is always [0, 0, 1]
static void mat3_multiply(const float* A, const float* B, float* C) {
    // Row 0
    C[0] = A[0]*B[0] + A[1]*B[3];  // + A[2]*0
    C[1] = A[0]*B[1] + A[1]*B[4];  // + A[2]*0
    C[2] = A[0]*B[2] + A[1]*B[5] + A[2];  // *1
    // Row 1
    C[3] = A[3]*B[0] + A[4]*B[3];
    C[4] = A[3]*B[1] + A[4]*B[4];
    C[5] = A[3]*B[2] + A[4]*B[5] + A[5];
    // Row 2 - always [0, 0, 1]
    C[6] = 0.0f;
    C[7] = 0.0f;
    C[8] = 1.0f;
}

// Push a transform onto the layer's stack
// Builds TRS matrix (Translate * Rotate * Scale) and multiplies with current
// Returns false if stack overflow (caller should error)
static bool layer_push(Layer* layer, float x, float y, float r, float sx, float sy) {
    if (layer->transform_depth >= MAX_TRANSFORM_DEPTH - 1) {
        return false;  // Stack overflow
    }

    // Build TRS matrix: result of Translate(x,y) * Rotate(r) * Scale(sx,sy)
    // [sx*cos  -sy*sin  x]
    // [sx*sin   sy*cos  y]
    // [   0        0    1]
    float c = cosf(r);
    float s = sinf(r);
    float m[9] = {
        sx * c, -sy * s, x,
        sx * s,  sy * c, y,
        0.0f,    0.0f,   1.0f
    };

    // Get parent transform
    float* parent = layer_get_transform(layer);

    // Increment depth
    layer->transform_depth++;

    // Get new current transform slot
    float* current = layer_get_transform(layer);

    // Multiply: current = parent * m
    mat3_multiply(parent, m, current);
    return true;
}

// Pop a transform from the layer's stack
static void layer_pop(Layer* layer) {
    if (layer->transform_depth > 0) {
        layer->transform_depth--;
    } else {
        fprintf(stderr, "Warning: transform stack underflow\n");
    }
}

// Unpack uint32 color to RGBA floats (0-1)
static void unpack_color(uint32_t color, float* r, float* g, float* b, float* a) {
    *r = ((color >> 24) & 0xFF) / 255.0f;
    *g = ((color >> 16) & 0xFF) / 255.0f;
    *b = ((color >> 8) & 0xFF) / 255.0f;
    *a = (color & 0xFF) / 255.0f;
}

// Add a vertex to the batch (16 floats per vertex)
static void batch_add_vertex(float x, float y, float u, float v,
                             float r, float g, float b, float a,
                             float type, float s0, float s1, float s2, float s3,
                             float addR, float addG, float addB) {
    if (batch_vertex_count >= MAX_BATCH_VERTICES) return;
    int i = batch_vertex_count * VERTEX_FLOATS;
    batch_vertices[i + 0] = x;
    batch_vertices[i + 1] = y;
    batch_vertices[i + 2] = u;
    batch_vertices[i + 3] = v;
    batch_vertices[i + 4] = r;
    batch_vertices[i + 5] = g;
    batch_vertices[i + 6] = b;
    batch_vertices[i + 7] = a;
    batch_vertices[i + 8] = type;
    batch_vertices[i + 9] = s0;    // shape[0]
    batch_vertices[i + 10] = s1;   // shape[1]
    batch_vertices[i + 11] = s2;   // shape[2]
    batch_vertices[i + 12] = s3;   // shape[3]
    batch_vertices[i + 13] = addR; // additive color R (flash)
    batch_vertices[i + 14] = addG; // additive color G (flash)
    batch_vertices[i + 15] = addB; // additive color B (flash)
    batch_vertex_count++;
}

// Add a quad (two triangles, 6 vertices) for SDF shapes
// UVs go from (0,0) to (1,1) across the quad
// Shape params are the same for all vertices
// addR/G/B is additive color (flash effect)
static void batch_add_sdf_quad(float x0, float y0, float x1, float y1,
                               float x2, float y2, float x3, float y3,
                               float r, float g, float b, float a,
                               float type, float s0, float s1, float s2, float s3,
                               float addR, float addG, float addB) {
    // Quad corners with UVs:
    // 0(0,0)---1(1,0)
    // |         |
    // 3(0,1)---2(1,1)

    // Triangle 1: 0, 1, 2
    batch_add_vertex(x0, y0, 0.0f, 0.0f, r, g, b, a, type, s0, s1, s2, s3, addR, addG, addB);
    batch_add_vertex(x1, y1, 1.0f, 0.0f, r, g, b, a, type, s0, s1, s2, s3, addR, addG, addB);
    batch_add_vertex(x2, y2, 1.0f, 1.0f, r, g, b, a, type, s0, s1, s2, s3, addR, addG, addB);
    // Triangle 2: 0, 2, 3
    batch_add_vertex(x0, y0, 0.0f, 0.0f, r, g, b, a, type, s0, s1, s2, s3, addR, addG, addB);
    batch_add_vertex(x2, y2, 1.0f, 1.0f, r, g, b, a, type, s0, s1, s2, s3, addR, addG, addB);
    batch_add_vertex(x3, y3, 0.0f, 1.0f, r, g, b, a, type, s0, s1, s2, s3, addR, addG, addB);
}

static SDL_Window* window = NULL;
static SDL_GLContext gl_context = NULL;
static lua_State* L = NULL;
static bool error_state = false;
static char error_message[4096] = {0};

// Rendering state
static GLuint shader_program = 0;
static GLuint vao = 0;
static GLuint vbo = 0;

// Layer registry
#define MAX_LAYERS 16
static Layer* layer_registry[MAX_LAYERS];
static char* layer_names[MAX_LAYERS];
static int layer_count = 0;

// Screen blit resources
static GLuint screen_shader = 0;
static GLuint screen_vao = 0;
static GLuint screen_vbo = 0;

// Manual layer compositing queue
typedef struct {
    Layer* layer;
    float x, y;  // Offset in game coordinates
} LayerDrawCommand;

#define MAX_LAYER_DRAWS 64
static LayerDrawCommand layer_draw_queue[MAX_LAYER_DRAWS];
static int layer_draw_count = 0;

// Input state - Keyboard
static bool keys_current[SDL_NUM_SCANCODES] = {0};
static bool keys_previous[SDL_NUM_SCANCODES] = {0};

// Input state - Mouse
#define MAX_MOUSE_BUTTONS 5
static bool mouse_buttons_current[MAX_MOUSE_BUTTONS] = {0};
static bool mouse_buttons_previous[MAX_MOUSE_BUTTONS] = {0};
static int mouse_x = 0, mouse_y = 0;           // Window coordinates
static int mouse_dx = 0, mouse_dy = 0;         // Delta this frame
static int mouse_wheel_x = 0, mouse_wheel_y = 0; // Wheel delta this frame

// Input state - Gamepad
static SDL_GameController* gamepad = NULL;
static bool gamepad_buttons_current[SDL_CONTROLLER_BUTTON_MAX] = {0};
static bool gamepad_buttons_previous[SDL_CONTROLLER_BUTTON_MAX] = {0};
static float gamepad_axes[SDL_CONTROLLER_AXIS_MAX] = {0};
static float gamepad_axes_previous[SDL_CONTROLLER_AXIS_MAX] = {0};
static float gamepad_deadzone = 0.2f;  // Default deadzone for axis→button conversion

// Input type detection - track last input device used
typedef enum {
    INPUT_TYPE_KEYBOARD,
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_GAMEPAD,
} InputType;

static InputType last_input_type = INPUT_TYPE_KEYBOARD;

static const char* input_type_to_string(InputType type) {
    switch (type) {
        case INPUT_TYPE_KEYBOARD: return "keyboard";
        case INPUT_TYPE_MOUSE: return "mouse";
        case INPUT_TYPE_GAMEPAD: return "gamepad";
        default: return "keyboard";
    }
}

// Rebinding capture mode
static bool capture_mode = false;
static char captured_control[64] = {0};  // Stores captured control string like "key:space"

static void input_start_capture(void) {
    capture_mode = true;
    captured_control[0] = '\0';
}

static const char* input_get_captured(void) {
    if (captured_control[0] != '\0') {
        return captured_control;
    }
    return NULL;
}

static void input_stop_capture(void) {
    capture_mode = false;
    captured_control[0] = '\0';
}

// Convert gamepad button name to SDL button enum
static SDL_GameControllerButton gamepad_button_from_name(const char* name) {
    if (!name) return SDL_CONTROLLER_BUTTON_INVALID;
    if (strcmp(name, "a") == 0) return SDL_CONTROLLER_BUTTON_A;
    if (strcmp(name, "b") == 0) return SDL_CONTROLLER_BUTTON_B;
    if (strcmp(name, "x") == 0) return SDL_CONTROLLER_BUTTON_X;
    if (strcmp(name, "y") == 0) return SDL_CONTROLLER_BUTTON_Y;
    if (strcmp(name, "back") == 0) return SDL_CONTROLLER_BUTTON_BACK;
    if (strcmp(name, "guide") == 0) return SDL_CONTROLLER_BUTTON_GUIDE;
    if (strcmp(name, "start") == 0) return SDL_CONTROLLER_BUTTON_START;
    if (strcmp(name, "leftstick") == 0) return SDL_CONTROLLER_BUTTON_LEFTSTICK;
    if (strcmp(name, "rightstick") == 0) return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    if (strcmp(name, "leftshoulder") == 0 || strcmp(name, "lb") == 0) return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    if (strcmp(name, "rightshoulder") == 0 || strcmp(name, "rb") == 0) return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    if (strcmp(name, "dpup") == 0) return SDL_CONTROLLER_BUTTON_DPAD_UP;
    if (strcmp(name, "dpdown") == 0) return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    if (strcmp(name, "dpleft") == 0) return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    if (strcmp(name, "dpright") == 0) return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    return SDL_CONTROLLER_BUTTON_INVALID;
}

// Convert gamepad axis name to SDL axis enum
// Returns axis index in lower bits, sign in bit 8 (0x100 = positive, 0 = negative)
static int gamepad_axis_from_name(const char* name) {
    if (!name) return -1;
    int len = strlen(name);
    if (len < 2) return -1;

    // Check for +/- suffix
    int sign = 0;  // 0 = full axis (no sign), 1 = positive, -1 = negative
    char axis_name[32];
    strncpy(axis_name, name, sizeof(axis_name) - 1);
    axis_name[sizeof(axis_name) - 1] = '\0';

    if (axis_name[len - 1] == '+') {
        sign = 1;
        axis_name[len - 1] = '\0';
    } else if (axis_name[len - 1] == '-') {
        sign = -1;
        axis_name[len - 1] = '\0';
    }

    SDL_GameControllerAxis axis = SDL_CONTROLLER_AXIS_INVALID;
    if (strcmp(axis_name, "leftx") == 0) axis = SDL_CONTROLLER_AXIS_LEFTX;
    else if (strcmp(axis_name, "lefty") == 0) axis = SDL_CONTROLLER_AXIS_LEFTY;
    else if (strcmp(axis_name, "rightx") == 0) axis = SDL_CONTROLLER_AXIS_RIGHTX;
    else if (strcmp(axis_name, "righty") == 0) axis = SDL_CONTROLLER_AXIS_RIGHTY;
    else if (strcmp(axis_name, "triggerleft") == 0 || strcmp(axis_name, "lt") == 0) axis = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
    else if (strcmp(axis_name, "triggerright") == 0 || strcmp(axis_name, "rt") == 0) axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;

    if (axis == SDL_CONTROLLER_AXIS_INVALID) return -1;

    // Encode axis and sign: axis in lower byte, sign in upper byte
    return axis | ((sign & 0xFF) << 8);
}

// Convert key name string to SDL scancode
static SDL_Scancode key_name_to_scancode(const char* name) {
    if (!name) return SDL_SCANCODE_UNKNOWN;

    // Single character keys (a-z, 0-9)
    if (strlen(name) == 1) {
        char c = name[0];
        if (c >= 'a' && c <= 'z') return SDL_SCANCODE_A + (c - 'a');
        if (c >= 'A' && c <= 'Z') return SDL_SCANCODE_A + (c - 'A');
        if (c >= '0' && c <= '9') return SDL_SCANCODE_0 + (c - '0');
    }

    // Named keys
    if (strcmp(name, "space") == 0) return SDL_SCANCODE_SPACE;
    if (strcmp(name, "enter") == 0 || strcmp(name, "return") == 0) return SDL_SCANCODE_RETURN;
    if (strcmp(name, "escape") == 0 || strcmp(name, "esc") == 0) return SDL_SCANCODE_ESCAPE;
    if (strcmp(name, "backspace") == 0) return SDL_SCANCODE_BACKSPACE;
    if (strcmp(name, "tab") == 0) return SDL_SCANCODE_TAB;
    if (strcmp(name, "capslock") == 0) return SDL_SCANCODE_CAPSLOCK;

    // Arrow keys
    if (strcmp(name, "left") == 0) return SDL_SCANCODE_LEFT;
    if (strcmp(name, "right") == 0) return SDL_SCANCODE_RIGHT;
    if (strcmp(name, "up") == 0) return SDL_SCANCODE_UP;
    if (strcmp(name, "down") == 0) return SDL_SCANCODE_DOWN;

    // Modifier keys
    if (strcmp(name, "lshift") == 0) return SDL_SCANCODE_LSHIFT;
    if (strcmp(name, "rshift") == 0) return SDL_SCANCODE_RSHIFT;
    if (strcmp(name, "shift") == 0) return SDL_SCANCODE_LSHIFT;  // Default to left
    if (strcmp(name, "lctrl") == 0) return SDL_SCANCODE_LCTRL;
    if (strcmp(name, "rctrl") == 0) return SDL_SCANCODE_RCTRL;
    if (strcmp(name, "ctrl") == 0) return SDL_SCANCODE_LCTRL;
    if (strcmp(name, "lalt") == 0) return SDL_SCANCODE_LALT;
    if (strcmp(name, "ralt") == 0) return SDL_SCANCODE_RALT;
    if (strcmp(name, "alt") == 0) return SDL_SCANCODE_LALT;

    // Function keys
    if (strcmp(name, "f1") == 0) return SDL_SCANCODE_F1;
    if (strcmp(name, "f2") == 0) return SDL_SCANCODE_F2;
    if (strcmp(name, "f3") == 0) return SDL_SCANCODE_F3;
    if (strcmp(name, "f4") == 0) return SDL_SCANCODE_F4;
    if (strcmp(name, "f5") == 0) return SDL_SCANCODE_F5;
    if (strcmp(name, "f6") == 0) return SDL_SCANCODE_F6;
    if (strcmp(name, "f7") == 0) return SDL_SCANCODE_F7;
    if (strcmp(name, "f8") == 0) return SDL_SCANCODE_F8;
    if (strcmp(name, "f9") == 0) return SDL_SCANCODE_F9;
    if (strcmp(name, "f10") == 0) return SDL_SCANCODE_F10;
    if (strcmp(name, "f11") == 0) return SDL_SCANCODE_F11;
    if (strcmp(name, "f12") == 0) return SDL_SCANCODE_F12;

    // Navigation keys
    if (strcmp(name, "insert") == 0) return SDL_SCANCODE_INSERT;
    if (strcmp(name, "delete") == 0) return SDL_SCANCODE_DELETE;
    if (strcmp(name, "home") == 0) return SDL_SCANCODE_HOME;
    if (strcmp(name, "end") == 0) return SDL_SCANCODE_END;
    if (strcmp(name, "pageup") == 0) return SDL_SCANCODE_PAGEUP;
    if (strcmp(name, "pagedown") == 0) return SDL_SCANCODE_PAGEDOWN;

    // Punctuation and symbols
    if (strcmp(name, "minus") == 0 || strcmp(name, "-") == 0) return SDL_SCANCODE_MINUS;
    if (strcmp(name, "equals") == 0 || strcmp(name, "=") == 0) return SDL_SCANCODE_EQUALS;
    if (strcmp(name, "leftbracket") == 0 || strcmp(name, "[") == 0) return SDL_SCANCODE_LEFTBRACKET;
    if (strcmp(name, "rightbracket") == 0 || strcmp(name, "]") == 0) return SDL_SCANCODE_RIGHTBRACKET;
    if (strcmp(name, "backslash") == 0 || strcmp(name, "\\") == 0) return SDL_SCANCODE_BACKSLASH;
    if (strcmp(name, "semicolon") == 0 || strcmp(name, ";") == 0) return SDL_SCANCODE_SEMICOLON;
    if (strcmp(name, "apostrophe") == 0 || strcmp(name, "'") == 0) return SDL_SCANCODE_APOSTROPHE;
    if (strcmp(name, "grave") == 0 || strcmp(name, "`") == 0) return SDL_SCANCODE_GRAVE;
    if (strcmp(name, "comma") == 0 || strcmp(name, ",") == 0) return SDL_SCANCODE_COMMA;
    if (strcmp(name, "period") == 0 || strcmp(name, ".") == 0) return SDL_SCANCODE_PERIOD;
    if (strcmp(name, "slash") == 0 || strcmp(name, "/") == 0) return SDL_SCANCODE_SLASH;

    // Numpad
    if (strcmp(name, "kp0") == 0) return SDL_SCANCODE_KP_0;
    if (strcmp(name, "kp1") == 0) return SDL_SCANCODE_KP_1;
    if (strcmp(name, "kp2") == 0) return SDL_SCANCODE_KP_2;
    if (strcmp(name, "kp3") == 0) return SDL_SCANCODE_KP_3;
    if (strcmp(name, "kp4") == 0) return SDL_SCANCODE_KP_4;
    if (strcmp(name, "kp5") == 0) return SDL_SCANCODE_KP_5;
    if (strcmp(name, "kp6") == 0) return SDL_SCANCODE_KP_6;
    if (strcmp(name, "kp7") == 0) return SDL_SCANCODE_KP_7;
    if (strcmp(name, "kp8") == 0) return SDL_SCANCODE_KP_8;
    if (strcmp(name, "kp9") == 0) return SDL_SCANCODE_KP_9;
    if (strcmp(name, "kpenter") == 0) return SDL_SCANCODE_KP_ENTER;
    if (strcmp(name, "kpplus") == 0) return SDL_SCANCODE_KP_PLUS;
    if (strcmp(name, "kpminus") == 0) return SDL_SCANCODE_KP_MINUS;
    if (strcmp(name, "kpmultiply") == 0) return SDL_SCANCODE_KP_MULTIPLY;
    if (strcmp(name, "kpdivide") == 0) return SDL_SCANCODE_KP_DIVIDE;
    if (strcmp(name, "kpperiod") == 0) return SDL_SCANCODE_KP_PERIOD;

    return SDL_SCANCODE_UNKNOWN;
}

// Convert scancode to key name string (reverse of key_name_to_scancode)
// Returns NULL if unknown
static const char* scancode_to_key_name(SDL_Scancode sc) {
    // Letters
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
        static char letter[2] = {0};
        letter[0] = 'a' + (sc - SDL_SCANCODE_A);
        return letter;
    }
    // Numbers
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) {
        static char digit[2] = {0};
        digit[0] = '1' + (sc - SDL_SCANCODE_1);
        return digit;
    }
    if (sc == SDL_SCANCODE_0) return "0";

    // Named keys
    switch (sc) {
        case SDL_SCANCODE_SPACE: return "space";
        case SDL_SCANCODE_RETURN: return "enter";
        case SDL_SCANCODE_ESCAPE: return "escape";
        case SDL_SCANCODE_BACKSPACE: return "backspace";
        case SDL_SCANCODE_TAB: return "tab";
        case SDL_SCANCODE_LEFT: return "left";
        case SDL_SCANCODE_RIGHT: return "right";
        case SDL_SCANCODE_UP: return "up";
        case SDL_SCANCODE_DOWN: return "down";
        case SDL_SCANCODE_LSHIFT: return "lshift";
        case SDL_SCANCODE_RSHIFT: return "rshift";
        case SDL_SCANCODE_LCTRL: return "lctrl";
        case SDL_SCANCODE_RCTRL: return "rctrl";
        case SDL_SCANCODE_LALT: return "lalt";
        case SDL_SCANCODE_RALT: return "ralt";
        case SDL_SCANCODE_F1: return "f1";
        case SDL_SCANCODE_F2: return "f2";
        case SDL_SCANCODE_F3: return "f3";
        case SDL_SCANCODE_F4: return "f4";
        case SDL_SCANCODE_F5: return "f5";
        case SDL_SCANCODE_F6: return "f6";
        case SDL_SCANCODE_F7: return "f7";
        case SDL_SCANCODE_F8: return "f8";
        case SDL_SCANCODE_F9: return "f9";
        case SDL_SCANCODE_F10: return "f10";
        case SDL_SCANCODE_F11: return "f11";
        case SDL_SCANCODE_F12: return "f12";
        case SDL_SCANCODE_INSERT: return "insert";
        case SDL_SCANCODE_DELETE: return "delete";
        case SDL_SCANCODE_HOME: return "home";
        case SDL_SCANCODE_END: return "end";
        case SDL_SCANCODE_PAGEUP: return "pageup";
        case SDL_SCANCODE_PAGEDOWN: return "pagedown";
        default: return NULL;
    }
}

// Convert gamepad button enum to name string
static const char* gamepad_button_to_name(SDL_GameControllerButton btn) {
    switch (btn) {
        case SDL_CONTROLLER_BUTTON_A: return "a";
        case SDL_CONTROLLER_BUTTON_B: return "b";
        case SDL_CONTROLLER_BUTTON_X: return "x";
        case SDL_CONTROLLER_BUTTON_Y: return "y";
        case SDL_CONTROLLER_BUTTON_BACK: return "back";
        case SDL_CONTROLLER_BUTTON_GUIDE: return "guide";
        case SDL_CONTROLLER_BUTTON_START: return "start";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "leftstick";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "rightstick";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "leftshoulder";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "rightshoulder";
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return "dpup";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "dpdown";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "dpleft";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "dpright";
        default: return NULL;
    }
}

// Convert gamepad axis enum to name string
static const char* gamepad_axis_to_name(SDL_GameControllerAxis axis) {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: return "leftx";
        case SDL_CONTROLLER_AXIS_LEFTY: return "lefty";
        case SDL_CONTROLLER_AXIS_RIGHTX: return "rightx";
        case SDL_CONTROLLER_AXIS_RIGHTY: return "righty";
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return "triggerleft";
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "triggerright";
        default: return NULL;
    }
}

// Copy current input state to previous (called at end of physics frame)
static void input_post_update(void) {
    memcpy(keys_previous, keys_current, sizeof(keys_previous));
    memcpy(mouse_buttons_previous, mouse_buttons_current, sizeof(mouse_buttons_previous));
    memcpy(gamepad_buttons_previous, gamepad_buttons_current, sizeof(gamepad_buttons_previous));
    memcpy(gamepad_axes_previous, gamepad_axes, sizeof(gamepad_axes_previous));
    // Reset per-frame deltas
    mouse_dx = 0;
    mouse_dy = 0;
    mouse_wheel_x = 0;
    mouse_wheel_y = 0;
}

// Update gamepad state (call once per frame before input processing)
static void gamepad_update(void) {
    if (!gamepad) return;

    // Update buttons
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
        bool was_down = gamepad_buttons_current[i];
        gamepad_buttons_current[i] = SDL_GameControllerGetButton(gamepad, (SDL_GameControllerButton)i);
        // Detect button press for input type tracking and capture
        if (gamepad_buttons_current[i] && !was_down) {
            last_input_type = INPUT_TYPE_GAMEPAD;
            // Capture mode: capture the button
            if (capture_mode && captured_control[0] == '\0') {
                const char* btn_name = gamepad_button_to_name((SDL_GameControllerButton)i);
                if (btn_name) {
                    snprintf(captured_control, sizeof(captured_control), "button:%s", btn_name);
                }
            }
        }
    }

    // Update axes (normalize from -32768..32767 to -1..1)
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
        float prev = gamepad_axes[i];
        Sint16 raw = SDL_GameControllerGetAxis(gamepad, (SDL_GameControllerAxis)i);
        gamepad_axes[i] = raw / 32767.0f;
        // Clamp to -1..1 (raw -32768 would give slightly more than -1)
        if (gamepad_axes[i] < -1.0f) gamepad_axes[i] = -1.0f;
        // Detect significant axis movement for input type tracking and capture
        if (fabsf(gamepad_axes[i]) > 0.5f && fabsf(prev) <= 0.5f) {
            last_input_type = INPUT_TYPE_GAMEPAD;
            // Capture mode: capture the axis with direction
            if (capture_mode && captured_control[0] == '\0') {
                const char* axis_name = gamepad_axis_to_name((SDL_GameControllerAxis)i);
                if (axis_name) {
                    char sign = gamepad_axes[i] > 0 ? '+' : '-';
                    snprintf(captured_control, sizeof(captured_control), "axis:%s%c", axis_name, sign);
                }
            }
        }
    }
}

// Convert window mouse coordinates to game coordinates
// Returns false if mouse is outside the game area (in letterbox)
static bool mouse_to_game_coords(int win_x, int win_y, float* game_x, float* game_y) {
    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);

    // Calculate scale (same logic as render)
    float scale_x = (float)window_w / GAME_WIDTH;
    float scale_y = (float)window_h / GAME_HEIGHT;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int int_scale = (int)scale;
    if (int_scale < 1) int_scale = 1;

    // Calculate letterbox offset
    int scaled_w = GAME_WIDTH * int_scale;
    int scaled_h = GAME_HEIGHT * int_scale;
    int offset_x = (window_w - scaled_w) / 2;
    int offset_y = (window_h - scaled_h) / 2;

    // Convert to game coordinates
    float gx = (float)(win_x - offset_x) / int_scale;
    float gy = (float)(win_y - offset_y) / int_scale;

    *game_x = gx;
    *game_y = gy;

    // Check if inside game area
    return (gx >= 0 && gx < GAME_WIDTH && gy >= 0 && gy < GAME_HEIGHT);
}

// Action binding system
typedef enum {
    CONTROL_KEY,
    CONTROL_MOUSE_BUTTON,
    CONTROL_GAMEPAD_BUTTON,  // For Step 6
    CONTROL_GAMEPAD_AXIS,    // For Step 6
} ControlType;

typedef struct {
    ControlType type;
    int code;       // Scancode for keys, button number for mouse/gamepad
    int sign;       // For axes: +1 or -1 (positive or negative direction)
} Control;

#define MAX_CONTROLS_PER_ACTION 8
#define MAX_ACTIONS 128
#define MAX_ACTION_NAME 32

typedef struct {
    char name[MAX_ACTION_NAME];
    Control controls[MAX_CONTROLS_PER_ACTION];
    int control_count;
} Action;

static Action actions[MAX_ACTIONS];
static int action_count = 0;

// Chord: multiple actions that must all be held simultaneously
#define MAX_ACTIONS_PER_CHORD 4
#define MAX_CHORDS 32

typedef struct {
    char name[MAX_ACTION_NAME];
    char action_names[MAX_ACTIONS_PER_CHORD][MAX_ACTION_NAME];
    int action_count;
    bool was_down;  // For edge detection
} Chord;

static Chord chords[MAX_CHORDS];
static int chord_count = 0;

// Sequence: series of actions that must be pressed in order within time windows
#define MAX_SEQUENCE_STEPS 8
#define MAX_SEQUENCES 32

typedef struct {
    char name[MAX_ACTION_NAME];
    char action_names[MAX_SEQUENCE_STEPS][MAX_ACTION_NAME];
    float delays[MAX_SEQUENCE_STEPS];  // Time window after each step (delay[i] = time allowed after step i)
    int step_count;
    int current_step;       // Which step we're waiting for (0 = first action)
    float last_press_time;  // When last step was completed
    bool just_fired;        // True for one frame when sequence completes
    bool was_fired;         // For is_released edge detection
} Sequence;

static Sequence sequences[MAX_SEQUENCES];
static int sequence_count = 0;

// Hold: action that triggers after being held for a duration
#define MAX_HOLDS 32

typedef struct {
    char name[MAX_ACTION_NAME];
    char source_action[MAX_ACTION_NAME];
    float required_duration;
    float current_duration;     // How long source has been held
    bool triggered;             // True once duration is reached (stays true while held)
    bool just_triggered;        // True for one frame when duration is first reached
    bool was_triggered;         // For is_released edge detection
} Hold;

static Hold holds[MAX_HOLDS];
static int hold_count = 0;

// Find or create an action by name
static Action* action_get_or_create(const char* name) {
    // Find existing
    for (int i = 0; i < action_count; i++) {
        if (strcmp(actions[i].name, name) == 0) {
            return &actions[i];
        }
    }
    // Create new
    if (action_count >= MAX_ACTIONS) {
        printf("Warning: Max actions reached\n");
        return NULL;
    }
    Action* action = &actions[action_count++];
    strncpy(action->name, name, MAX_ACTION_NAME - 1);
    action->name[MAX_ACTION_NAME - 1] = '\0';
    action->control_count = 0;
    return action;
}

// Find action by name (returns NULL if not found)
static Action* action_find(const char* name) {
    for (int i = 0; i < action_count; i++) {
        if (strcmp(actions[i].name, name) == 0) {
            return &actions[i];
        }
    }
    return NULL;
}

// Parse control string like 'key:space', 'mouse:1', 'button:a', 'axis:leftx+'
// Returns true on success, fills out control struct
static bool parse_control_string(const char* str, Control* ctrl) {
    if (!str || !ctrl) return false;

    // Find the colon separator
    const char* colon = strchr(str, ':');
    if (!colon) return false;

    // Get type prefix
    size_t type_len = colon - str;
    const char* value = colon + 1;

    if (type_len == 3 && strncmp(str, "key", 3) == 0) {
        ctrl->type = CONTROL_KEY;
        ctrl->code = key_name_to_scancode(value);
        ctrl->sign = 0;
        return ctrl->code != SDL_SCANCODE_UNKNOWN;
    }
    else if (type_len == 5 && strncmp(str, "mouse", 5) == 0) {
        ctrl->type = CONTROL_MOUSE_BUTTON;
        ctrl->code = atoi(value);  // 1, 2, 3, etc.
        ctrl->sign = 0;
        return ctrl->code >= 1 && ctrl->code <= MAX_MOUSE_BUTTONS;
    }
    else if (type_len == 6 && strncmp(str, "button", 6) == 0) {
        ctrl->type = CONTROL_GAMEPAD_BUTTON;
        ctrl->code = gamepad_button_from_name(value);
        ctrl->sign = 0;
        return ctrl->code != SDL_CONTROLLER_BUTTON_INVALID;
    }
    else if (type_len == 4 && strncmp(str, "axis", 4) == 0) {
        ctrl->type = CONTROL_GAMEPAD_AXIS;
        int axis_info = gamepad_axis_from_name(value);
        if (axis_info < 0) return false;
        ctrl->code = axis_info & 0xFF;        // Axis index
        ctrl->sign = (axis_info >> 8) & 0xFF; // Sign (0=full, 1=positive, -1=negative as 0xFF)
        if (ctrl->sign == 0xFF) ctrl->sign = -1;  // Fix sign extension
        return true;
    }

    return false;
}

// Bind a control to an action
static bool input_bind_control(const char* action_name, const char* control_str) {
    Action* action = action_get_or_create(action_name);
    if (!action) return false;

    Control ctrl;
    if (!parse_control_string(control_str, &ctrl)) {
        printf("Warning: Invalid control string '%s'\n", control_str);
        return false;
    }

    // Check if already bound
    for (int i = 0; i < action->control_count; i++) {
        if (action->controls[i].type == ctrl.type &&
            action->controls[i].code == ctrl.code &&
            action->controls[i].sign == ctrl.sign) {
            return true;  // Already bound
        }
    }

    // Add new control
    if (action->control_count >= MAX_CONTROLS_PER_ACTION) {
        printf("Warning: Max controls per action reached for '%s'\n", action_name);
        return false;
    }

    action->controls[action->control_count++] = ctrl;
    return true;
}

// Helper: check if gamepad axis exceeds deadzone in specified direction
static bool axis_is_active(int axis, int sign) {
    if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) return false;
    float value = gamepad_axes[axis];
    if (sign > 0) return value > gamepad_deadzone;   // Positive direction
    if (sign < 0) return value < -gamepad_deadzone;  // Negative direction
    return fabsf(value) > gamepad_deadzone;          // Either direction
}

// Check if a single control is currently down
static bool control_is_down(const Control* ctrl) {
    switch (ctrl->type) {
        case CONTROL_KEY:
            return keys_current[ctrl->code];
        case CONTROL_MOUSE_BUTTON:
            if (ctrl->code >= 1 && ctrl->code <= MAX_MOUSE_BUTTONS) {
                return mouse_buttons_current[ctrl->code - 1];
            }
            return false;
        case CONTROL_GAMEPAD_BUTTON:
            if (ctrl->code >= 0 && ctrl->code < SDL_CONTROLLER_BUTTON_MAX) {
                return gamepad_buttons_current[ctrl->code];
            }
            return false;
        case CONTROL_GAMEPAD_AXIS:
            return axis_is_active(ctrl->code, ctrl->sign);
        default:
            return false;
    }
}

// Helper: check if axis WAS active in previous frame
static bool axis_was_active(int axis, int sign) {
    if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) return false;
    float value = gamepad_axes_previous[axis];
    if (sign > 0) return value > gamepad_deadzone;
    if (sign < 0) return value < -gamepad_deadzone;
    return fabsf(value) > gamepad_deadzone;
}

// Check if a single control was just pressed
static bool control_is_pressed(const Control* ctrl) {
    switch (ctrl->type) {
        case CONTROL_KEY:
            return keys_current[ctrl->code] && !keys_previous[ctrl->code];
        case CONTROL_MOUSE_BUTTON:
            if (ctrl->code >= 1 && ctrl->code <= MAX_MOUSE_BUTTONS) {
                int idx = ctrl->code - 1;
                return mouse_buttons_current[idx] && !mouse_buttons_previous[idx];
            }
            return false;
        case CONTROL_GAMEPAD_BUTTON:
            if (ctrl->code >= 0 && ctrl->code < SDL_CONTROLLER_BUTTON_MAX) {
                return gamepad_buttons_current[ctrl->code] && !gamepad_buttons_previous[ctrl->code];
            }
            return false;
        case CONTROL_GAMEPAD_AXIS:
            return axis_is_active(ctrl->code, ctrl->sign) && !axis_was_active(ctrl->code, ctrl->sign);
        default:
            return false;
    }
}

// Check if a single control was just released
static bool control_is_released(const Control* ctrl) {
    switch (ctrl->type) {
        case CONTROL_KEY:
            return !keys_current[ctrl->code] && keys_previous[ctrl->code];
        case CONTROL_MOUSE_BUTTON:
            if (ctrl->code >= 1 && ctrl->code <= MAX_MOUSE_BUTTONS) {
                int idx = ctrl->code - 1;
                return !mouse_buttons_current[idx] && mouse_buttons_previous[idx];
            }
            return false;
        case CONTROL_GAMEPAD_BUTTON:
            if (ctrl->code >= 0 && ctrl->code < SDL_CONTROLLER_BUTTON_MAX) {
                return !gamepad_buttons_current[ctrl->code] && gamepad_buttons_previous[ctrl->code];
            }
            return false;
        case CONTROL_GAMEPAD_AXIS:
            return !axis_is_active(ctrl->code, ctrl->sign) && axis_was_active(ctrl->code, ctrl->sign);
        default:
            return false;
    }
}

// Query action state - is_down returns true if ANY bound control is down
static bool action_is_down(const char* name) {
    Action* action = action_find(name);
    if (!action) return false;

    for (int i = 0; i < action->control_count; i++) {
        if (control_is_down(&action->controls[i])) {
            return true;
        }
    }
    return false;
}

// Query action state - is_pressed returns true if ANY bound control was just pressed
static bool action_is_pressed(const char* name) {
    Action* action = action_find(name);
    if (!action) return false;

    for (int i = 0; i < action->control_count; i++) {
        if (control_is_pressed(&action->controls[i])) {
            return true;
        }
    }
    return false;
}

// Query action state - is_released returns true if ANY bound control was just released
static bool action_is_released(const char* name) {
    Action* action = action_find(name);
    if (!action) return false;

    for (int i = 0; i < action->control_count; i++) {
        if (control_is_released(&action->controls[i])) {
            return true;
        }
    }
    return false;
}

// Chord functions
static Chord* chord_find(const char* name) {
    for (int i = 0; i < chord_count; i++) {
        if (strcmp(chords[i].name, name) == 0) {
            return &chords[i];
        }
    }
    return NULL;
}

// Check if chord is currently down (all actions held)
static bool chord_is_down(Chord* chord) {
    if (!chord || chord->action_count == 0) return false;
    for (int i = 0; i < chord->action_count; i++) {
        if (!action_is_down(chord->action_names[i])) {
            return false;
        }
    }
    return true;
}

// Check if chord was just pressed (is down now, wasn't before)
static bool chord_is_pressed(Chord* chord) {
    if (!chord) return false;
    bool down_now = chord_is_down(chord);
    return down_now && !chord->was_down;
}

// Check if chord was just released (was down, isn't now)
static bool chord_is_released(Chord* chord) {
    if (!chord) return false;
    bool down_now = chord_is_down(chord);
    return !down_now && chord->was_down;
}

// Update chord edge detection state (call at end of frame)
static void chords_post_update(void) {
    for (int i = 0; i < chord_count; i++) {
        chords[i].was_down = chord_is_down(&chords[i]);
    }
}

// Bind a chord (multiple actions that must all be held)
// action_names is an array of action name strings, count is the number of actions
static bool input_bind_chord_internal(const char* name, const char** action_names, int count) {
    if (count <= 0 || count > MAX_ACTIONS_PER_CHORD) {
        printf("Warning: Chord must have 1-%d actions\n", MAX_ACTIONS_PER_CHORD);
        return false;
    }

    // Check if chord already exists
    Chord* chord = chord_find(name);
    if (chord) {
        printf("Warning: Chord '%s' already exists\n", name);
        return false;
    }

    if (chord_count >= MAX_CHORDS) {
        printf("Warning: Max chords reached\n");
        return false;
    }

    chord = &chords[chord_count++];
    strncpy(chord->name, name, MAX_ACTION_NAME - 1);
    chord->name[MAX_ACTION_NAME - 1] = '\0';
    chord->action_count = count;
    chord->was_down = false;

    for (int i = 0; i < count; i++) {
        strncpy(chord->action_names[i], action_names[i], MAX_ACTION_NAME - 1);
        chord->action_names[i][MAX_ACTION_NAME - 1] = '\0';
    }

    return true;
}

// Sequence functions
static Sequence* sequence_find(const char* name) {
    for (int i = 0; i < sequence_count; i++) {
        if (strcmp(sequences[i].name, name) == 0) {
            return &sequences[i];
        }
    }
    return NULL;
}

// Sequences are momentary - is_down returns true only the frame it fires
static bool sequence_is_down(Sequence* seq) {
    return seq && seq->just_fired;
}

static bool sequence_is_pressed(Sequence* seq) {
    return seq && seq->just_fired;
}

static bool sequence_is_released(Sequence* seq) {
    return seq && seq->was_fired && !seq->just_fired;
}

// Check if an action press advances any sequence (called when any action is pressed)
static void sequences_on_action_pressed(const char* action_name, float current_time) {
    for (int i = 0; i < sequence_count; i++) {
        Sequence* seq = &sequences[i];
        if (seq->step_count == 0) continue;

        // Check if this action matches what we're waiting for
        if (strcmp(action_name, seq->action_names[seq->current_step]) == 0) {
            if (seq->current_step == 0) {
                // First step - always accept, start the sequence
                seq->current_step = 1;
                seq->last_press_time = current_time;
            }
            else {
                // Check if we're within the time window
                float elapsed = current_time - seq->last_press_time;
                if (elapsed <= seq->delays[seq->current_step - 1]) {
                    // Within time window - advance
                    seq->current_step++;
                    seq->last_press_time = current_time;

                    // Check if sequence completed
                    if (seq->current_step >= seq->step_count) {
                        seq->just_fired = true;
                        seq->current_step = 0;  // Reset for next time
                    }
                }
                else {
                    // Timeout - reset and check if this action starts the sequence
                    seq->current_step = 0;
                    if (strcmp(action_name, seq->action_names[0]) == 0) {
                        seq->current_step = 1;
                        seq->last_press_time = current_time;
                    }
                }
            }
        }
    }
}

// Update sequences each frame (clear just_fired, check timeouts)
static void sequences_update(float current_time) {
    for (int i = 0; i < sequence_count; i++) {
        Sequence* seq = &sequences[i];

        // Update edge detection
        seq->was_fired = seq->just_fired;
        seq->just_fired = false;

        // Check for timeout on in-progress sequences
        if (seq->current_step > 0) {
            float elapsed = current_time - seq->last_press_time;
            if (elapsed > seq->delays[seq->current_step - 1]) {
                seq->current_step = 0;  // Reset
            }
        }
    }
}

// Bind a sequence - action_names and delays alternate: {action1, delay1, action2, delay2, action3}
// The arrays should have step_count actions and (step_count - 1) delays
static bool input_bind_sequence_internal(const char* name, const char** action_names, const float* delays, int step_count) {
    if (step_count < 2 || step_count > MAX_SEQUENCE_STEPS) {
        printf("Warning: Sequence must have 2-%d steps\n", MAX_SEQUENCE_STEPS);
        return false;
    }

    // Check if sequence already exists
    Sequence* seq = sequence_find(name);
    if (seq) {
        printf("Warning: Sequence '%s' already exists\n", name);
        return false;
    }

    if (sequence_count >= MAX_SEQUENCES) {
        printf("Warning: Max sequences reached\n");
        return false;
    }

    seq = &sequences[sequence_count++];
    strncpy(seq->name, name, MAX_ACTION_NAME - 1);
    seq->name[MAX_ACTION_NAME - 1] = '\0';
    seq->step_count = step_count;
    seq->current_step = 0;
    seq->last_press_time = 0;
    seq->just_fired = false;
    seq->was_fired = false;

    for (int i = 0; i < step_count; i++) {
        strncpy(seq->action_names[i], action_names[i], MAX_ACTION_NAME - 1);
        seq->action_names[i][MAX_ACTION_NAME - 1] = '\0';
    }

    for (int i = 0; i < step_count - 1; i++) {
        seq->delays[i] = delays[i];
    }

    return true;
}

// Check all actions for pressed state and notify sequences
static void sequences_check_actions(float current_time) {
    for (int i = 0; i < action_count; i++) {
        if (action_is_pressed(actions[i].name)) {
            sequences_on_action_pressed(actions[i].name, current_time);
        }
    }
}

// Hold functions
static Hold* hold_find(const char* name) {
    for (int i = 0; i < hold_count; i++) {
        if (strcmp(holds[i].name, name) == 0) {
            return &holds[i];
        }
    }
    return NULL;
}

static bool hold_is_down(Hold* hold) {
    return hold && hold->triggered;
}

static bool hold_is_pressed(Hold* hold) {
    return hold && hold->just_triggered;
}

static bool hold_is_released(Hold* hold) {
    return hold && hold->was_triggered && !hold->triggered;
}

// Get how long the source action has been held (useful for charge-up effects)
static float hold_get_duration(Hold* hold) {
    return hold ? hold->current_duration : 0.0f;
}

// Update holds each frame
static void holds_update(float dt) {
    for (int i = 0; i < hold_count; i++) {
        Hold* hold = &holds[i];

        // Update edge detection
        hold->was_triggered = hold->triggered;
        hold->just_triggered = false;

        // Check if source action is held
        if (action_is_down(hold->source_action)) {
            hold->current_duration += dt;

            // Check if we just reached the threshold
            if (!hold->triggered && hold->current_duration >= hold->required_duration) {
                hold->triggered = true;
                hold->just_triggered = true;
            }
        }
        else {
            // Source released - reset
            hold->current_duration = 0.0f;
            hold->triggered = false;
        }
    }
}

// Bind a hold
static bool input_bind_hold_internal(const char* name, float duration, const char* source_action) {
    if (duration <= 0) {
        printf("Warning: Hold duration must be positive\n");
        return false;
    }

    // Check if hold already exists
    Hold* hold = hold_find(name);
    if (hold) {
        printf("Warning: Hold '%s' already exists\n", name);
        return false;
    }

    if (hold_count >= MAX_HOLDS) {
        printf("Warning: Max holds reached\n");
        return false;
    }

    hold = &holds[hold_count++];
    strncpy(hold->name, name, MAX_ACTION_NAME - 1);
    hold->name[MAX_ACTION_NAME - 1] = '\0';
    strncpy(hold->source_action, source_action, MAX_ACTION_NAME - 1);
    hold->source_action[MAX_ACTION_NAME - 1] = '\0';
    hold->required_duration = duration;
    hold->current_duration = 0.0f;
    hold->triggered = false;
    hold->just_triggered = false;
    hold->was_triggered = false;

    return true;
}

// Get hold duration by name (for Lua binding)
static float input_get_hold_duration(const char* name) {
    Hold* hold = hold_find(name);
    return hold ? hold->current_duration : 0.0f;
}

// Unified query functions that check actions, chords, sequences, and holds
static bool input_is_down(const char* name) {
    // Check actions first
    if (action_is_down(name)) return true;
    // Check chords
    Chord* chord = chord_find(name);
    if (chord) return chord_is_down(chord);
    // Check sequences
    Sequence* seq = sequence_find(name);
    if (seq) return sequence_is_down(seq);
    // Check holds
    Hold* hold = hold_find(name);
    if (hold) return hold_is_down(hold);
    return false;
}

static bool input_is_pressed(const char* name) {
    // Check actions first
    if (action_is_pressed(name)) return true;
    // Check chords
    Chord* chord = chord_find(name);
    if (chord) return chord_is_pressed(chord);
    // Check sequences
    Sequence* seq = sequence_find(name);
    if (seq) return sequence_is_pressed(seq);
    // Check holds
    Hold* hold = hold_find(name);
    if (hold) return hold_is_pressed(hold);
    return false;
}

static bool input_is_released(const char* name) {
    // Check actions first
    if (action_is_released(name)) return true;
    // Check chords
    Chord* chord = chord_find(name);
    if (chord) return chord_is_released(chord);
    // Check sequences
    Sequence* seq = sequence_find(name);
    if (seq) return sequence_is_released(seq);
    // Check holds
    Hold* hold = hold_find(name);
    if (hold) return hold_is_released(hold);
    return false;
}

// Check if any bound action was just pressed this frame
static bool input_any_pressed(void) {
    for (int i = 0; i < action_count; i++) {
        if (action_is_pressed(actions[i].name)) {
            return true;
        }
    }
    return false;
}

// Get the name of the first action that was just pressed this frame (or NULL)
static const char* input_get_pressed_action(void) {
    for (int i = 0; i < action_count; i++) {
        if (action_is_pressed(actions[i].name)) {
            return actions[i].name;
        }
    }
    return NULL;
}

// Unbind a specific control from an action
static bool input_unbind_control(const char* action_name, const char* control_str) {
    Action* action = action_find(action_name);
    if (!action) return false;

    Control ctrl;
    if (!parse_control_string(control_str, &ctrl)) {
        return false;
    }

    // Find and remove the control
    for (int i = 0; i < action->control_count; i++) {
        if (action->controls[i].type == ctrl.type &&
            action->controls[i].code == ctrl.code &&
            action->controls[i].sign == ctrl.sign) {
            // Shift remaining controls down
            for (int j = i; j < action->control_count - 1; j++) {
                action->controls[j] = action->controls[j + 1];
            }
            action->control_count--;
            return true;
        }
    }
    return false;
}

// Unbind all controls from an action
static void input_unbind_all_controls(const char* action_name) {
    Action* action = action_find(action_name);
    if (action) {
        action->control_count = 0;
    }
}

// Bind all standard inputs to actions with matching names
// Keys become actions like 'a', 'space', 'left', etc.
// Mouse buttons become 'mouse_1', 'mouse_2', etc.
static void input_bind_all_defaults(void) {
    // Letters a-z
    for (char c = 'a'; c <= 'z'; c++) {
        char action_name[2] = {c, '\0'};
        char control_str[8];
        snprintf(control_str, sizeof(control_str), "key:%c", c);
        input_bind_control(action_name, control_str);
    }

    // Numbers 0-9
    for (char c = '0'; c <= '9'; c++) {
        char action_name[2] = {c, '\0'};
        char control_str[8];
        snprintf(control_str, sizeof(control_str), "key:%c", c);
        input_bind_control(action_name, control_str);
    }

    // Named keys
    static const char* named_keys[] = {
        "space", "enter", "escape", "backspace", "tab",
        "left", "right", "up", "down",
        "lshift", "rshift", "lctrl", "rctrl", "lalt", "ralt",
        "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "f10", "f11", "f12",
        "insert", "delete", "home", "end", "pageup", "pagedown",
        NULL
    };
    for (int i = 0; named_keys[i] != NULL; i++) {
        char control_str[32];
        snprintf(control_str, sizeof(control_str), "key:%s", named_keys[i]);
        input_bind_control(named_keys[i], control_str);
    }

    // Mouse buttons
    for (int i = 1; i <= MAX_MOUSE_BUTTONS; i++) {
        char action_name[16];
        char control_str[16];
        snprintf(action_name, sizeof(action_name), "mouse_%d", i);
        snprintf(control_str, sizeof(control_str), "mouse:%d", i);
        input_bind_control(action_name, control_str);
    }

    // Gamepad buttons
    static const char* gamepad_buttons[] = {
        "a", "b", "x", "y",
        "back", "guide", "start",
        "leftstick", "rightstick",
        "leftshoulder", "rightshoulder",
        "dpup", "dpdown", "dpleft", "dpright",
        NULL
    };
    for (int i = 0; gamepad_buttons[i] != NULL; i++) {
        char action_name[32];
        char control_str[32];
        snprintf(action_name, sizeof(action_name), "button_%s", gamepad_buttons[i]);
        snprintf(control_str, sizeof(control_str), "button:%s", gamepad_buttons[i]);
        input_bind_control(action_name, control_str);
    }
}

// Get axis value from two actions (negative and positive)
// Returns -1 to 1 for digital inputs
// Will return analog values when gamepad support is added (Step 6)
static float input_get_axis(const char* negative, const char* positive) {
    float value = 0.0f;
    if (action_is_down(negative)) value -= 1.0f;
    if (action_is_down(positive)) value += 1.0f;
    return value;
}

// Get 2D vector from four actions, normalized to prevent faster diagonal movement
static void input_get_vector(const char* left, const char* right, const char* up, const char* down, float* out_x, float* out_y) {
    float x = input_get_axis(left, right);
    float y = input_get_axis(up, down);

    // Normalize if length > 1 (diagonal movement)
    float len_sq = x * x + y * y;
    if (len_sq > 1.0f) {
        float len = sqrtf(len_sq);
        x /= len;
        y /= len;
    }

    *out_x = x;
    *out_y = y;
}

// Queue a layer to be drawn to screen at given offset
static void layer_queue_draw(Layer* layer, float x, float y) {
    if (layer_draw_count >= MAX_LAYER_DRAWS) return;
    layer_draw_queue[layer_draw_count].layer = layer;
    layer_draw_queue[layer_draw_count].x = x;
    layer_draw_queue[layer_draw_count].y = y;
    layer_draw_count++;
}

// Queue a shader application command (deferred - actual work done at frame end)
static void layer_apply_shader(Layer* layer, GLuint shader) {
    if (!shader) return;
    if (layer->command_count >= MAX_COMMAND_CAPACITY) return;

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->type = COMMAND_APPLY_SHADER;
    cmd->texture_id = shader;  // Reuse texture_id field for shader handle
}

// Queue uniform setting commands (deferred - applied when processing commands)
static void layer_shader_set_float(Layer* layer, GLuint shader, const char* name, float value) {
    if (!shader || layer->command_count >= MAX_COMMAND_CAPACITY) return;

    GLint loc = glGetUniformLocation(shader, name);
    if (loc == -1) return;  // Uniform not found

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->type = COMMAND_SET_UNIFORM_FLOAT;
    cmd->texture_id = shader;
    cmd->color = (uint32_t)loc;  // Store uniform location
    cmd->params[0] = value;
}

static void layer_shader_set_vec2(Layer* layer, GLuint shader, const char* name, float x, float y) {
    if (!shader || layer->command_count >= MAX_COMMAND_CAPACITY) return;

    GLint loc = glGetUniformLocation(shader, name);
    if (loc == -1) return;

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->type = COMMAND_SET_UNIFORM_VEC2;
    cmd->texture_id = shader;
    cmd->color = (uint32_t)loc;
    cmd->params[0] = x;
    cmd->params[1] = y;
}

static void layer_shader_set_vec4(Layer* layer, GLuint shader, const char* name, float x, float y, float z, float w) {
    if (!shader || layer->command_count >= MAX_COMMAND_CAPACITY) return;

    GLint loc = glGetUniformLocation(shader, name);
    if (loc == -1) return;

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->type = COMMAND_SET_UNIFORM_VEC4;
    cmd->texture_id = shader;
    cmd->color = (uint32_t)loc;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = z;
    cmd->params[3] = w;
}

static void layer_shader_set_int(Layer* layer, GLuint shader, const char* name, int value) {
    if (!shader || layer->command_count >= MAX_COMMAND_CAPACITY) return;

    GLint loc = glGetUniformLocation(shader, name);
    if (loc == -1) return;

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->type = COMMAND_SET_UNIFORM_INT;
    cmd->texture_id = shader;
    cmd->color = (uint32_t)loc;
    cmd->params[0] = (float)value;  // Store as float, cast back when processing
}

// Execute shader application (ping-pong): read from current buffer, apply shader, write to alternate
// Called during command processing when COMMAND_APPLY_SHADER is encountered
static void execute_apply_shader(Layer* layer, GLuint shader) {
    // Ensure effect buffer exists
    layer_ensure_effect_buffer(layer);

    // Determine source and destination based on current state
    GLuint src_tex, dst_fbo;
    if (layer->textures_swapped) {
        src_tex = layer->effect_texture;
        dst_fbo = layer->fbo;
    } else {
        src_tex = layer->color_texture;
        dst_fbo = layer->effect_fbo;
    }

    // Bind destination FBO
    glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);
    glViewport(0, 0, layer->width, layer->height);

    // Clear destination
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Disable blending (replace, don't blend)
    glDisable(GL_BLEND);

    // Use the effect shader
    glUseProgram(shader);

    // Set standard uniforms
    GLint tex_loc = glGetUniformLocation(shader, "u_texture");
    if (tex_loc != -1) glUniform1i(tex_loc, 0);

    // Bind source texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src_tex);

    // Draw fullscreen quad
    glBindVertexArray(screen_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Re-enable blending
    glEnable(GL_BLEND);

    // Toggle state - now the "current" buffer is the destination
    layer->textures_swapped = !layer->textures_swapped;
}

// Flush batch to GPU
static void batch_flush(void) {
    if (batch_vertex_count == 0) return;

    // Bind texture if we have one (for sprites)
    if (current_batch_texture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_batch_texture);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    batch_vertex_count * VERTEX_FLOATS * sizeof(float),
                    batch_vertices);
    glDrawArrays(GL_TRIANGLES, 0, batch_vertex_count);
    glBindVertexArray(0);

    batch_vertex_count = 0;
}

// Process a rectangle command (SDF-based, UV-space approach)
// The SDF is computed in local quad space using UV coordinates.
// This handles rotation correctly because UV interpolation implicitly
// provides the inverse rotation.
static void process_rectangle(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float w = cmd->params[2];
    float h = cmd->params[3];

    // Add padding for anti-aliasing (1-2 pixels)
    float pad = 2.0f;

    // Quad size in local space (including padding)
    float quad_w = w + 2.0f * pad;
    float quad_h = h + 2.0f * pad;

    // Rectangle corners with padding (local coordinates)
    // 0---1
    // |   |
    // 3---2
    float lx0 = x - pad, ly0 = y - pad;
    float lx1 = x + w + pad, ly1 = y - pad;
    float lx2 = x + w + pad, ly2 = y + h + pad;
    float lx3 = x - pad, ly3 = y + h + pad;

    // Transform to world coordinates
    float wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3;
    transform_point(cmd->transform, lx0, ly0, &wx0, &wy0);
    transform_point(cmd->transform, lx1, ly1, &wx1, &wy1);
    transform_point(cmd->transform, lx2, ly2, &wx2, &wy2);
    transform_point(cmd->transform, lx3, ly3, &wx3, &wy3);

    // Rectangle half-size in local space
    float half_w = w * 0.5f;
    float half_h = h * 0.5f;

    // Unpack color
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Add SDF quad: shape = (quad_w, quad_h, half_w, half_h)
    // Shader computes local_p = vUV * quad_size, center = quad_size * 0.5
    // No flash for shapes (additive = 0)
    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_RECT, quad_w, quad_h, half_w, half_h,
                       0.0f, 0.0f, 0.0f);
}

// Process a circle command (SDF-based, UV-space approach)
// Same UV-space approach as rectangles for rotation support.
static void process_circle(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float radius = cmd->params[2];

    // Add padding for anti-aliasing
    float pad = 2.0f;

    // Quad size in local space (square, including padding)
    float quad_size = (radius + pad) * 2.0f;

    // Circle bounding box with padding (local coordinates)
    float lx0 = x - radius - pad, ly0 = y - radius - pad;
    float lx1 = x + radius + pad, ly1 = y - radius - pad;
    float lx2 = x + radius + pad, ly2 = y + radius + pad;
    float lx3 = x - radius - pad, ly3 = y + radius + pad;

    // Transform to world coordinates
    float wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3;
    transform_point(cmd->transform, lx0, ly0, &wx0, &wy0);
    transform_point(cmd->transform, lx1, ly1, &wx1, &wy1);
    transform_point(cmd->transform, lx2, ly2, &wx2, &wy2);
    transform_point(cmd->transform, lx3, ly3, &wx3, &wy3);

    // Unpack color
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Add SDF quad: shape = (quad_size, quad_size, radius, unused)
    // Shader computes local_p = vUV * quad_size, center = quad_size * 0.5
    // No flash for shapes (additive = 0)
    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_CIRCLE, quad_size, quad_size, radius, 0.0f,
                       0.0f, 0.0f, 0.0f);
}

// Forward declaration of batch_flush (needed for process_sprite)
static void batch_flush(void);

// Process a sprite command (texture sampling)
// Image is centered at (x, y) in local coordinates
static void process_sprite(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float w = cmd->params[2];
    float h = cmd->params[3];

    // Flush batch if texture changes
    if (current_batch_texture != cmd->texture_id && batch_vertex_count > 0) {
        batch_flush();
    }
    current_batch_texture = cmd->texture_id;

    // Image is centered at (x, y), so compute corners
    float half_w = w * 0.5f;
    float half_h = h * 0.5f;

    // Local corners (centered at x, y)
    float lx0 = x - half_w, ly0 = y - half_h;
    float lx1 = x + half_w, ly1 = y - half_h;
    float lx2 = x + half_w, ly2 = y + half_h;
    float lx3 = x - half_w, ly3 = y + half_h;

    // Transform to world coordinates
    float wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3;
    transform_point(cmd->transform, lx0, ly0, &wx0, &wy0);
    transform_point(cmd->transform, lx1, ly1, &wx1, &wy1);
    transform_point(cmd->transform, lx2, ly2, &wx2, &wy2);
    transform_point(cmd->transform, lx3, ly3, &wx3, &wy3);

    // Unpack color (used for tinting)
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Unpack flash color (additive, alpha ignored)
    float addR, addG, addB, addA;
    unpack_color(cmd->flash_color, &addR, &addG, &addB, &addA);
    (void)addA;  // Alpha not used for additive color

    // Add sprite quad with UVs (0,0) to (1,1)
    // shape params unused for sprites, but we still use the same vertex format
    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_SPRITE, 0.0f, 0.0f, 0.0f, 0.0f,
                       addR, addG, addB);
}

// Apply GL blend state based on blend mode
static void apply_blend_mode(uint8_t mode) {
    switch (mode) {
        case BLEND_ALPHA:
            // Standard alpha blending: result = src * src.a + dst * (1 - src.a)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BLEND_ADDITIVE:
            // Additive blending: result = src * src.a + dst (good for glows, particles)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
    }
}

// Render all commands on a layer
// Note: Caller must have set up projection matrix and bound initial FBO before calling
static void layer_render(Layer* layer) {
    batch_vertex_count = 0;
    current_batch_texture = 0;
    uint8_t current_blend = BLEND_ALPHA;  // Start with default
    apply_blend_mode(current_blend);

    for (int i = 0; i < layer->command_count; i++) {
        const DrawCommand* cmd = &layer->commands[i];

        // Handle uniform setting commands
        // These need to temporarily switch programs, so flush batch first and restore after
        if (cmd->type == COMMAND_SET_UNIFORM_FLOAT ||
            cmd->type == COMMAND_SET_UNIFORM_VEC2 ||
            cmd->type == COMMAND_SET_UNIFORM_VEC4 ||
            cmd->type == COMMAND_SET_UNIFORM_INT) {
            // Flush any pending draws before switching programs
            batch_flush();
            current_batch_texture = 0;

            glUseProgram(cmd->texture_id);
            switch (cmd->type) {
                case COMMAND_SET_UNIFORM_FLOAT:
                    glUniform1f((GLint)cmd->color, cmd->params[0]);
                    break;
                case COMMAND_SET_UNIFORM_VEC2:
                    glUniform2f((GLint)cmd->color, cmd->params[0], cmd->params[1]);
                    break;
                case COMMAND_SET_UNIFORM_VEC4:
                    glUniform4f((GLint)cmd->color, cmd->params[0], cmd->params[1], cmd->params[2], cmd->params[3]);
                    break;
                case COMMAND_SET_UNIFORM_INT:
                    glUniform1i((GLint)cmd->color, (int)cmd->params[0]);
                    break;
            }

            // Restore drawing shader for subsequent draw commands
            glUseProgram(shader_program);
            continue;
        }

        // Handle shader application command
        if (cmd->type == COMMAND_APPLY_SHADER) {
            // Flush pending draw commands before shader application
            batch_flush();
            current_batch_texture = 0;

            // Execute the shader (ping-pong to alternate buffer)
            execute_apply_shader(layer, cmd->texture_id);

            // After ping-pong, bind the NEW current FBO for subsequent draws
            // (execute_apply_shader toggled textures_swapped, so current is now the destination)
            GLuint current_fbo = layer->textures_swapped ? layer->effect_fbo : layer->fbo;
            glBindFramebuffer(GL_FRAMEBUFFER, current_fbo);
            glViewport(0, 0, layer->width, layer->height);
            // DON'T clear - the ping-pong output is our background for subsequent draws

            // Restore drawing shader state
            glUseProgram(shader_program);
            apply_blend_mode(current_blend);

            continue;
        }

        // Check for blend mode change (draw commands only)
        if (cmd->blend_mode != current_blend && batch_vertex_count > 0) {
            batch_flush();
            current_blend = cmd->blend_mode;
            apply_blend_mode(current_blend);
        } else if (cmd->blend_mode != current_blend) {
            current_blend = cmd->blend_mode;
            apply_blend_mode(current_blend);
        }

        switch (cmd->type) {
            case COMMAND_RECTANGLE:
                // SDF shapes use no texture - flush if we were drawing sprites
                if (current_batch_texture != 0 && batch_vertex_count > 0) {
                    batch_flush();
                    current_batch_texture = 0;
                }
                process_rectangle(cmd);
                break;
            case COMMAND_CIRCLE:
                // SDF shapes use no texture - flush if we were drawing sprites
                if (current_batch_texture != 0 && batch_vertex_count > 0) {
                    batch_flush();
                    current_batch_texture = 0;
                }
                process_circle(cmd);
                break;
            case COMMAND_SPRITE:
                process_sprite(cmd);
                break;
        }

        // Flush if batch is getting full
        if (batch_vertex_count >= MAX_BATCH_VERTICES - 6) {
            batch_flush();
        }
    }

    // Final flush
    batch_flush();

    // Reset to default blend mode for screen blit
    apply_blend_mode(BLEND_ALPHA);
}

// Find or create a named layer
static Layer* layer_get_or_create(const char* name) {
    // Check if layer already exists
    for (int i = 0; i < layer_count; i++) {
        if (strcmp(layer_names[i], name) == 0) {
            return layer_registry[i];
        }
    }

    // Create new layer
    if (layer_count >= MAX_LAYERS) {
        fprintf(stderr, "Error: Maximum number of layers (%d) reached\n", MAX_LAYERS);
        return NULL;
    }

    Layer* layer = layer_create(GAME_WIDTH, GAME_HEIGHT);
    if (!layer) {
        fprintf(stderr, "Error: Failed to create layer '%s'\n", name);
        return NULL;
    }

    // Store in registry
    layer_registry[layer_count] = layer;
    layer_names[layer_count] = strdup(name);
    layer_count++;

    printf("Created layer: %s\n", name);
    return layer;
}

// Forward declarations for effect shader functions (defined after shader sources)
static GLuint effect_shader_load_file(const char* path);
static GLuint effect_shader_load_string(const char* frag_source);
static void effect_shader_destroy(GLuint shader);
static void shader_set_float(GLuint shader, const char* name, float value);
static void shader_set_vec2(GLuint shader, const char* name, float x, float y);
static void shader_set_vec4(GLuint shader, const char* name, float x, float y, float z, float w);
static void shader_set_int(GLuint shader, const char* name, int value);
static void shader_set_texture(GLuint shader, const char* name, GLuint texture, int unit);

// Lua bindings
static int l_layer_create(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    Layer* layer = layer_get_or_create(name);
    if (!layer) {
        return luaL_error(L, "Failed to create layer: %s", name);
    }
    lua_pushlightuserdata(L, layer);
    return 1;
}

static int l_layer_rectangle(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 6);
    layer_add_rectangle(layer, x, y, w, h, color);
    return 0;
}

static int l_layer_circle(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float radius = (float)luaL_checknumber(L, 4);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 5);
    layer_add_circle(layer, x, y, radius, color);
    return 0;
}

static int l_rgba(lua_State* L) {
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    int a = (int)luaL_optinteger(L, 4, 255);
    uint32_t color = ((r & 0xFF) << 24) | ((g & 0xFF) << 16) | ((b & 0xFF) << 8) | (a & 0xFF);
    lua_pushinteger(L, color);
    return 1;
}

static int l_set_shape_filter(lua_State* L) {
    const char* mode = luaL_checkstring(L, 1);
    if (strcmp(mode, "smooth") == 0) {
        shape_filter_mode = FILTER_SMOOTH;
    } else if (strcmp(mode, "rough") == 0) {
        shape_filter_mode = FILTER_ROUGH;
    } else {
        return luaL_error(L, "Invalid filter mode: %s (use 'smooth' or 'rough')", mode);
    }
    return 0;
}

static int l_timing_resync(lua_State* L) {
    (void)L;  // Unused
    timing_resync();
    return 0;
}

static int l_layer_push(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_optnumber(L, 2, 0.0);
    float y = (float)luaL_optnumber(L, 3, 0.0);
    float r = (float)luaL_optnumber(L, 4, 0.0);
    float sx = (float)luaL_optnumber(L, 5, 1.0);
    float sy = (float)luaL_optnumber(L, 6, 1.0);
    if (!layer_push(layer, x, y, r, sx, sy)) {
        return luaL_error(L, "Transform stack overflow (max depth: %d)", MAX_TRANSFORM_DEPTH);
    }
    return 0;
}

static int l_layer_pop(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    layer_pop(layer);
    return 0;
}

static int l_texture_load(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    Texture* tex = texture_load(path);
    if (!tex) {
        return luaL_error(L, "Failed to load texture: %s", path);
    }
    lua_pushlightuserdata(L, tex);
    return 1;
}

static int l_texture_get_width(lua_State* L) {
    Texture* tex = (Texture*)lua_touserdata(L, 1);
    lua_pushinteger(L, tex->width);
    return 1;
}

static int l_texture_get_height(lua_State* L) {
    Texture* tex = (Texture*)lua_touserdata(L, 1);
    lua_pushinteger(L, tex->height);
    return 1;
}

static int l_layer_draw_texture(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    Texture* tex = (Texture*)lua_touserdata(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    uint32_t color = (uint32_t)luaL_optinteger(L, 5, 0xFFFFFFFF);  // Default white (no tint)
    uint32_t flash = (uint32_t)luaL_optinteger(L, 6, 0x00000000);  // Default black (no flash)
    layer_add_image(layer, tex, x, y, color, flash);
    return 0;
}

static int l_layer_set_blend_mode(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    const char* mode = luaL_checkstring(L, 2);
    if (strcmp(mode, "alpha") == 0) {
        layer_set_blend_mode(layer, BLEND_ALPHA);
    } else if (strcmp(mode, "additive") == 0) {
        layer_set_blend_mode(layer, BLEND_ADDITIVE);
    } else {
        return luaL_error(L, "Invalid blend mode: %s (use 'alpha' or 'additive')", mode);
    }
    return 0;
}

// Effect shader Lua bindings
static int l_shader_load_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    GLuint shader = effect_shader_load_file(path);
    if (!shader) {
        return luaL_error(L, "Failed to load effect shader: %s", path);
    }
    lua_pushinteger(L, (lua_Integer)shader);
    return 1;
}

static int l_shader_load_string(lua_State* L) {
    const char* source = luaL_checkstring(L, 1);
    GLuint shader = effect_shader_load_string(source);
    if (!shader) {
        return luaL_error(L, "Failed to compile effect shader from string");
    }
    lua_pushinteger(L, (lua_Integer)shader);
    return 1;
}

static int l_shader_destroy(lua_State* L) {
    GLuint shader = (GLuint)luaL_checkinteger(L, 1);
    effect_shader_destroy(shader);
    return 0;
}

// Deferred uniform setting Lua bindings (queued to layer's command list)
static int l_layer_shader_set_float(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    GLuint shader = (GLuint)luaL_checkinteger(L, 2);
    const char* name = luaL_checkstring(L, 3);
    float value = (float)luaL_checknumber(L, 4);
    layer_shader_set_float(layer, shader, name, value);
    return 0;
}

static int l_layer_shader_set_vec2(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    GLuint shader = (GLuint)luaL_checkinteger(L, 2);
    const char* name = luaL_checkstring(L, 3);
    float x = (float)luaL_checknumber(L, 4);
    float y = (float)luaL_checknumber(L, 5);
    layer_shader_set_vec2(layer, shader, name, x, y);
    return 0;
}

static int l_layer_shader_set_vec4(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    GLuint shader = (GLuint)luaL_checkinteger(L, 2);
    const char* name = luaL_checkstring(L, 3);
    float x = (float)luaL_checknumber(L, 4);
    float y = (float)luaL_checknumber(L, 5);
    float z = (float)luaL_checknumber(L, 6);
    float w = (float)luaL_checknumber(L, 7);
    layer_shader_set_vec4(layer, shader, name, x, y, z, w);
    return 0;
}

static int l_layer_shader_set_int(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    GLuint shader = (GLuint)luaL_checkinteger(L, 2);
    const char* name = luaL_checkstring(L, 3);
    int value = (int)luaL_checkinteger(L, 4);
    layer_shader_set_int(layer, shader, name, value);
    return 0;
}

// Layer effect Lua bindings
static int l_layer_apply_shader(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    GLuint shader = (GLuint)luaL_checkinteger(L, 2);
    layer_apply_shader(layer, shader);
    return 0;
}

static int l_layer_draw(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (lua_gettop(L) >= 2) ? (float)luaL_checknumber(L, 2) : 0.0f;
    float y = (lua_gettop(L) >= 3) ? (float)luaL_checknumber(L, 3) : 0.0f;
    layer_queue_draw(layer, x, y);
    return 0;
}

static int l_layer_get_texture(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    GLuint tex = layer_get_texture(layer);
    lua_pushinteger(L, (lua_Integer)tex);
    return 1;
}

static int l_layer_reset_effects(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    layer_reset_effects(layer);
    return 0;
}

// Input Lua bindings
static int l_key_is_down(lua_State* L) {
    const char* key_name = luaL_checkstring(L, 1);
    SDL_Scancode scancode = key_name_to_scancode(key_name);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, keys_current[scancode]);
    }
    return 1;
}

static int l_key_is_pressed(lua_State* L) {
    const char* key_name = luaL_checkstring(L, 1);
    SDL_Scancode scancode = key_name_to_scancode(key_name);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, keys_current[scancode] && !keys_previous[scancode]);
    }
    return 1;
}

static int l_key_is_released(lua_State* L) {
    const char* key_name = luaL_checkstring(L, 1);
    SDL_Scancode scancode = key_name_to_scancode(key_name);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, !keys_current[scancode] && keys_previous[scancode]);
    }
    return 1;
}

// Mouse Lua bindings
static int l_mouse_position(lua_State* L) {
    float gx, gy;
    mouse_to_game_coords(mouse_x, mouse_y, &gx, &gy);
    lua_pushnumber(L, gx);
    lua_pushnumber(L, gy);
    return 2;
}

static int l_mouse_delta(lua_State* L) {
    // Delta is in window pixels, scale to game pixels
    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);
    float scale_x = (float)window_w / GAME_WIDTH;
    float scale_y = (float)window_h / GAME_HEIGHT;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int int_scale = (int)scale;
    if (int_scale < 1) int_scale = 1;

    lua_pushnumber(L, (float)mouse_dx / int_scale);
    lua_pushnumber(L, (float)mouse_dy / int_scale);
    return 2;
}

static int l_mouse_set_visible(lua_State* L) {
    bool visible = lua_toboolean(L, 1);
    SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
    return 0;
}

static int l_mouse_set_grabbed(lua_State* L) {
    bool grabbed = lua_toboolean(L, 1);
    SDL_SetRelativeMouseMode(grabbed ? SDL_TRUE : SDL_FALSE);
    return 0;
}

static int l_mouse_is_down(lua_State* L) {
    int button = (int)luaL_checkinteger(L, 1);
    if (button < 1 || button > MAX_MOUSE_BUTTONS) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, mouse_buttons_current[button - 1]);
    }
    return 1;
}

static int l_mouse_is_pressed(lua_State* L) {
    int button = (int)luaL_checkinteger(L, 1);
    if (button < 1 || button > MAX_MOUSE_BUTTONS) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, mouse_buttons_current[button - 1] && !mouse_buttons_previous[button - 1]);
    }
    return 1;
}

static int l_mouse_is_released(lua_State* L) {
    int button = (int)luaL_checkinteger(L, 1);
    if (button < 1 || button > MAX_MOUSE_BUTTONS) {
        lua_pushboolean(L, 0);
    } else {
        lua_pushboolean(L, !mouse_buttons_current[button - 1] && mouse_buttons_previous[button - 1]);
    }
    return 1;
}

static int l_mouse_wheel(lua_State* L) {
    lua_pushinteger(L, mouse_wheel_x);
    lua_pushinteger(L, mouse_wheel_y);
    return 2;
}

// Action binding Lua bindings
static int l_input_bind(lua_State* L) {
    const char* action = luaL_checkstring(L, 1);
    const char* control = luaL_checkstring(L, 2);
    lua_pushboolean(L, input_bind_control(action, control));
    return 1;
}

static int l_is_down(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, input_is_down(name));  // Checks both actions and chords
    return 1;
}

static int l_is_pressed(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, input_is_pressed(name));  // Checks both actions and chords
    return 1;
}

static int l_is_released(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, input_is_released(name));  // Checks both actions and chords
    return 1;
}

static int l_input_any_pressed(lua_State* L) {
    lua_pushboolean(L, input_any_pressed());
    return 1;
}

static int l_input_get_pressed_action(lua_State* L) {
    const char* action = input_get_pressed_action();
    if (action) {
        lua_pushstring(L, action);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// input_bind_chord(name, {action1, action2, ...})
static int l_input_bind_chord(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    const char* action_names[MAX_ACTIONS_PER_CHORD];
    int count = 0;

    lua_pushnil(L);
    while (lua_next(L, 2) != 0 && count < MAX_ACTIONS_PER_CHORD) {
        if (lua_isstring(L, -1)) {
            action_names[count++] = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
    }

    lua_pushboolean(L, input_bind_chord_internal(name, action_names, count));
    return 1;
}

// input_bind_sequence(name, {action1, delay1, action2, delay2, action3, ...})
// Table alternates: string action, number delay, string action, number delay, string action
static int l_input_bind_sequence(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    const char* action_names[MAX_SEQUENCE_STEPS];
    float delays[MAX_SEQUENCE_STEPS];
    int action_count = 0;
    int delay_count = 0;

    // Iterate through the table by index (array order matters)
    int len = (int)lua_rawlen(L, 2);
    for (int i = 1; i <= len && action_count < MAX_SEQUENCE_STEPS; i++) {
        lua_rawgeti(L, 2, i);
        int t = lua_type(L, -1);
        // Must check type exactly - lua_isstring returns true for numbers too!
        if (t == LUA_TSTRING) {
            action_names[action_count++] = lua_tostring(L, -1);
        }
        else if (t == LUA_TNUMBER && delay_count < MAX_SEQUENCE_STEPS - 1) {
            delays[delay_count++] = (float)lua_tonumber(L, -1);
        }
        lua_pop(L, 1);
    }

    // Validate: should have alternating action, delay, action, delay, action...
    // So action_count should be delay_count + 1
    if (action_count < 2 || action_count != delay_count + 1) {
        printf("Warning: Sequence format should be {action, delay, action, delay, action, ...}\n");
        lua_pushboolean(L, false);
        return 1;
    }

    lua_pushboolean(L, input_bind_sequence_internal(name, action_names, delays, action_count));
    return 1;
}

// input_bind_hold(name, duration, source_action)
static int l_input_bind_hold(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float duration = (float)luaL_checknumber(L, 2);
    const char* source_action = luaL_checkstring(L, 3);
    lua_pushboolean(L, input_bind_hold_internal(name, duration, source_action));
    return 1;
}

// input_get_hold_duration(name) - returns how long the source action has been held
static int l_input_get_hold_duration(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushnumber(L, input_get_hold_duration(name));
    return 1;
}

// input_get_last_type() - returns 'keyboard', 'mouse', or 'gamepad'
static int l_input_get_last_type(lua_State* L) {
    lua_pushstring(L, input_type_to_string(last_input_type));
    return 1;
}

// Capture mode for rebinding
static int l_input_start_capture(lua_State* L) {
    (void)L;
    input_start_capture();
    return 0;
}

static int l_input_get_captured(lua_State* L) {
    const char* captured = input_get_captured();
    if (captured) {
        lua_pushstring(L, captured);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int l_input_stop_capture(lua_State* L) {
    (void)L;
    input_stop_capture();
    return 0;
}

static int l_input_unbind(lua_State* L) {
    const char* action = luaL_checkstring(L, 1);
    const char* control = luaL_checkstring(L, 2);
    lua_pushboolean(L, input_unbind_control(action, control));
    return 1;
}

static int l_input_unbind_all(lua_State* L) {
    const char* action = luaL_checkstring(L, 1);
    input_unbind_all_controls(action);
    return 0;
}

static int l_input_bind_all(lua_State* L) {
    (void)L;  // Unused
    input_bind_all_defaults();
    return 0;
}

static int l_input_get_axis(lua_State* L) {
    const char* negative = luaL_checkstring(L, 1);
    const char* positive = luaL_checkstring(L, 2);
    lua_pushnumber(L, input_get_axis(negative, positive));
    return 1;
}

static int l_input_get_vector(lua_State* L) {
    const char* left = luaL_checkstring(L, 1);
    const char* right = luaL_checkstring(L, 2);
    const char* up = luaL_checkstring(L, 3);
    const char* down = luaL_checkstring(L, 4);
    float x, y;
    input_get_vector(left, right, up, down, &x, &y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

static int l_gamepad_is_connected(lua_State* L) {
    lua_pushboolean(L, gamepad != NULL);
    return 1;
}

static int l_gamepad_get_axis(lua_State* L) {
    const char* axis_name = luaL_checkstring(L, 1);
    int axis_info = gamepad_axis_from_name(axis_name);
    if (axis_info < 0) {
        lua_pushnumber(L, 0);
        return 1;
    }
    int axis = axis_info & 0xFF;
    if (axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX) {
        lua_pushnumber(L, gamepad_axes[axis]);
    } else {
        lua_pushnumber(L, 0);
    }
    return 1;
}

static int l_input_set_deadzone(lua_State* L) {
    float dz = (float)luaL_checknumber(L, 1);
    if (dz < 0) dz = 0;
    if (dz > 1) dz = 1;
    gamepad_deadzone = dz;
    return 0;
}

static void register_lua_bindings(lua_State* L) {
    lua_register(L, "layer_create", l_layer_create);
    lua_register(L, "layer_rectangle", l_layer_rectangle);
    lua_register(L, "layer_circle", l_layer_circle);
    lua_register(L, "layer_push", l_layer_push);
    lua_register(L, "layer_pop", l_layer_pop);
    lua_register(L, "layer_draw_texture", l_layer_draw_texture);
    lua_register(L, "layer_set_blend_mode", l_layer_set_blend_mode);
    lua_register(L, "texture_load", l_texture_load);
    lua_register(L, "texture_get_width", l_texture_get_width);
    lua_register(L, "texture_get_height", l_texture_get_height);
    lua_register(L, "rgba", l_rgba);
    lua_register(L, "set_shape_filter", l_set_shape_filter);
    lua_register(L, "timing_resync", l_timing_resync);
    // Effect shaders
    lua_register(L, "shader_load_file", l_shader_load_file);
    lua_register(L, "shader_load_string", l_shader_load_string);
    lua_register(L, "shader_destroy", l_shader_destroy);
    // Layer shader uniforms (deferred)
    lua_register(L, "layer_shader_set_float", l_layer_shader_set_float);
    lua_register(L, "layer_shader_set_vec2", l_layer_shader_set_vec2);
    lua_register(L, "layer_shader_set_vec4", l_layer_shader_set_vec4);
    lua_register(L, "layer_shader_set_int", l_layer_shader_set_int);
    // Layer effects
    lua_register(L, "layer_apply_shader", l_layer_apply_shader);
    lua_register(L, "layer_draw", l_layer_draw);
    lua_register(L, "layer_get_texture", l_layer_get_texture);
    lua_register(L, "layer_reset_effects", l_layer_reset_effects);
    // Input - Keyboard
    lua_register(L, "key_is_down", l_key_is_down);
    lua_register(L, "key_is_pressed", l_key_is_pressed);
    lua_register(L, "key_is_released", l_key_is_released);
    // Input - Mouse
    lua_register(L, "mouse_position", l_mouse_position);
    lua_register(L, "mouse_delta", l_mouse_delta);
    lua_register(L, "mouse_set_visible", l_mouse_set_visible);
    lua_register(L, "mouse_set_grabbed", l_mouse_set_grabbed);
    lua_register(L, "mouse_is_down", l_mouse_is_down);
    lua_register(L, "mouse_is_pressed", l_mouse_is_pressed);
    lua_register(L, "mouse_is_released", l_mouse_is_released);
    lua_register(L, "mouse_wheel", l_mouse_wheel);
    // Input - Action binding
    lua_register(L, "input_bind", l_input_bind);
    lua_register(L, "input_bind_chord", l_input_bind_chord);
    lua_register(L, "input_bind_sequence", l_input_bind_sequence);
    lua_register(L, "input_bind_hold", l_input_bind_hold);
    lua_register(L, "input_get_hold_duration", l_input_get_hold_duration);
    lua_register(L, "input_get_last_type", l_input_get_last_type);
    lua_register(L, "input_start_capture", l_input_start_capture);
    lua_register(L, "input_get_captured", l_input_get_captured);
    lua_register(L, "input_stop_capture", l_input_stop_capture);
    lua_register(L, "input_unbind", l_input_unbind);
    lua_register(L, "input_unbind_all", l_input_unbind_all);
    lua_register(L, "input_bind_all", l_input_bind_all);
    lua_register(L, "input_get_axis", l_input_get_axis);
    lua_register(L, "input_get_vector", l_input_get_vector);
    lua_register(L, "input_set_deadzone", l_input_set_deadzone);
    lua_register(L, "is_down", l_is_down);
    lua_register(L, "is_pressed", l_is_pressed);
    lua_register(L, "is_released", l_is_released);
    lua_register(L, "input_any_pressed", l_input_any_pressed);
    lua_register(L, "input_get_pressed_action", l_input_get_pressed_action);
    // Input - Gamepad
    lua_register(L, "gamepad_is_connected", l_gamepad_is_connected);
    lua_register(L, "gamepad_get_axis", l_gamepad_get_axis);
}

// Main loop state (needed for emscripten)
static bool running = true;
static Uint64 perf_freq = 0;
static Uint64 last_time = 0;
static double physics_lag = 0.0;
static double render_lag = 0.0;
static Uint64 step = 0;
static double game_time = 0.0;
static Uint64 frame = 0;

// VSync snap frequencies (computed at init based on display refresh rate)
static double snap_frequencies[8];
static int snap_frequency_count = 0;

// Reset timing accumulators (call on focus gain, scene transitions, etc.)
// This prevents accumulated lag from causing catch-up updates
static void timing_resync(void) {
    physics_lag = 0.0;
    render_lag = 0.0;
    last_time = SDL_GetPerformanceCounter();
}

// Shader headers - prepended to all shaders based on platform
#ifdef __EMSCRIPTEN__
    #define SHADER_HEADER_VERT "#version 300 es\n"
    #define SHADER_HEADER_FRAG "#version 300 es\nprecision mediump float;\n"
#else
    #define SHADER_HEADER_VERT "#version 330 core\n"
    #define SHADER_HEADER_FRAG "#version 330 core\n"
#endif

// Shader sources (no version line - header prepended at compile time)
static const char* vertex_shader_source =
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aUV;\n"
    "layout (location = 2) in vec4 aColor;\n"
    "layout (location = 3) in float aType;\n"
    "layout (location = 4) in vec4 aShape;\n"
    "layout (location = 5) in vec3 aAddColor;\n"
    "\n"
    "out vec2 vPos;\n"
    "out vec2 vUV;\n"
    "out vec4 vColor;\n"
    "out float vType;\n"
    "out vec4 vShape;\n"
    "out vec3 vAddColor;\n"
    "\n"
    "uniform mat4 projection;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
    "    vPos = aPos;\n"
    "    vUV = aUV;\n"
    "    vColor = aColor;\n"
    "    vType = aType;\n"
    "    vShape = aShape;\n"
    "    vAddColor = aAddColor;\n"
    "}\n";

static const char* fragment_shader_source =
    "in vec2 vPos;\n"
    "in vec2 vUV;\n"
    "in vec4 vColor;\n"
    "in float vType;\n"
    "in vec4 vShape;\n"
    "in vec3 vAddColor;\n"
    "\n"
    "out vec4 FragColor;\n"
    "\n"
    "uniform float u_aa_width;\n"
    "uniform sampler2D u_texture;\n"
    "\n"
    "// SDF for rectangle in local space\n"
    "float sdf_rect(vec2 p, vec2 center, vec2 half_size) {\n"
    "    vec2 d = abs(p - center) - half_size;\n"
    "    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);\n"
    "}\n"
    "\n"
    "// SDF for circle in local space\n"
    "float sdf_circle(vec2 p, vec2 center, float radius) {\n"
    "    return length(p - center) - radius;\n"
    "}\n"
    "\n"
    "// SDF for 'pixel-style' circle with cardinal bumps (superellipse, n < 2)\n"
    "float sdf_circle_pixel(vec2 p, vec2 center, float radius) {\n"
    "    vec2 d = abs(p - center);\n"
    "    float n = 1.95;\n"
    "    float dist = pow(pow(d.x, n) + pow(d.y, n), 1.0/n);\n"
    "    return dist - radius;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    float d;\n"
    "    \n"
    "    // UV-space SDF approach:\n"
    "    // vShape.xy = quad size in local space\n"
    "    // vUV * quad_size = position in local quad space\n"
    "    // center = quad_size * 0.5 (shape is always centered in quad)\n"
    "    // This handles rotation correctly because UV interpolation\n"
    "    // implicitly provides the inverse rotation.\n"
    "    \n"
    "    if (vType < 0.5) {\n"
    "        // Rectangle: shape = (quad_w, quad_h, half_w, half_h)\n"
    "        vec2 quad_size = vShape.xy;\n"
    "        vec2 local_p = vUV * quad_size;\n"
    "        vec2 center = quad_size * 0.5;\n"
    "        vec2 half_size = vShape.zw;\n"
    "        \n"
    "        // In rough mode, snap to local pixel grid\n"
    "        if (u_aa_width == 0.0) {\n"
    "            local_p = floor(local_p) + 0.5;\n"
    "        }\n"
    "        \n"
    "        d = sdf_rect(local_p, center, half_size);\n"
    "    } else if (vType < 1.5) {\n"
    "        // Circle: shape = (quad_size, quad_size, radius, unused)\n"
    "        float quad_size = vShape.x;\n"
    "        vec2 local_p = vUV * quad_size;\n"
    "        vec2 center = vec2(quad_size * 0.5);\n"
    "        float radius = vShape.z;\n"
    "        // Snap radius for consistent shape\n"
    "        if (u_aa_width == 0.0) {\n"
    "            radius = floor(radius + 0.5);\n"
    "        }\n"
    "        d = sdf_circle(local_p, center, radius);\n"
    "    } else {\n"
    "        // Sprite: sample texture at texel centers for pixel-perfect rendering\n"
    "        // vColor is multiply (tint), vAddColor is additive (flash)\n"
    "        ivec2 texSize = textureSize(u_texture, 0);\n"
    "        vec2 snappedUV = (floor(vUV * vec2(texSize)) + 0.5) / vec2(texSize);\n"
    "        vec4 texColor = texture(u_texture, snappedUV);\n"
    "        FragColor = vec4(texColor.rgb * vColor.rgb + vAddColor, texColor.a * vColor.a);\n"
    "        return;\n"
    "    }\n"
    "    \n"
    "    // Apply anti-aliasing (or hard edges when u_aa_width = 0)\n"
    "    // vColor is multiply (tint), vAddColor is additive (flash)\n"
    "    float alpha;\n"
    "    if (u_aa_width > 0.0) {\n"
    "        alpha = 1.0 - smoothstep(-u_aa_width, u_aa_width, d);\n"
    "    } else {\n"
    "        alpha = 1.0 - step(0.0, d);\n"
    "    }\n"
    "    FragColor = vec4(vColor.rgb + vAddColor, vColor.a * alpha);\n"
    "}\n";

static const char* screen_vertex_source =
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "uniform vec2 u_offset;\n"  // Offset in NDC (-1 to 1 range)
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos + u_offset, 0.0, 1.0);\n"
    "    TexCoord = aTexCoord;\n"
    "}\n";

static const char* screen_fragment_source =
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D screenTexture;\n"
    "void main() {\n"
    "    FragColor = texture(screenTexture, TexCoord);\n"
    "}\n";

// Compile a shader and return its ID (0 on failure)
// Automatically prepends platform-specific header
static GLuint compile_shader(GLenum type, const char* source) {
    const char* header = (type == GL_VERTEX_SHADER) ? SHADER_HEADER_VERT : SHADER_HEADER_FRAG;

    // Concatenate header + source
    size_t header_len = strlen(header);
    size_t source_len = strlen(source);
    char* full_source = (char*)malloc(header_len + source_len + 1);
    memcpy(full_source, header, header_len);
    memcpy(full_source + header_len, source, source_len + 1);

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, (const char**)&full_source, NULL);
    glCompileShader(shader);

    free(full_source);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "Shader compilation failed: %s\n", info_log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Link shaders into a program and return its ID (0 on failure)
static GLuint create_shader_program(const char* vert_src, const char* frag_src) {
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src);
    if (!vert) return 0;

    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!frag) {
        glDeleteShader(vert);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    // Shaders can be deleted after linking
    glDeleteShader(vert);
    glDeleteShader(frag);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "Shader program linking failed: %s\n", info_log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// Effect shader loading
// Read entire file into malloc'd string (caller must free)
static char* read_file_to_string(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    size_t read_size = fread(buffer, 1, size, f);
    buffer[read_size] = '\0';
    fclose(f);
    return buffer;
}

// Create an effect shader program from fragment source (uses screen_vertex_source)
static GLuint effect_shader_load_string(const char* frag_source) {
    return create_shader_program(screen_vertex_source, frag_source);
}

// Create an effect shader program from a fragment shader file
static GLuint effect_shader_load_file(const char* path) {
    char* source = read_file_to_string(path);
    if (!source) return 0;
    GLuint shader = effect_shader_load_string(source);
    free(source);
    if (shader) {
        printf("Loaded effect shader: %s\n", path);
    }
    return shader;
}

// Destroy an effect shader program
static void effect_shader_destroy(GLuint shader) {
    if (shader) {
        glDeleteProgram(shader);
    }
}

// Uniform setters
static void shader_set_float(GLuint shader, const char* name, float value) {
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform1f(loc, value);
}

static void shader_set_vec2(GLuint shader, const char* name, float x, float y) {
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform2f(loc, x, y);
}

static void shader_set_vec4(GLuint shader, const char* name, float x, float y, float z, float w) {
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform4f(loc, x, y, z, w);
}

static void shader_set_int(GLuint shader, const char* name, int value) {
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform1i(loc, value);
}

static void shader_set_texture(GLuint shader, const char* name, GLuint texture, int unit) {
    glUseProgram(shader);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform1i(loc, unit);
}

// Error handler that adds stack trace
static int traceback(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg, 1);
    return 1;
}

static void shutdown(void) {
    // Game rendering resources
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (shader_program) { glDeleteProgram(shader_program); shader_program = 0; }
    // Layers
    for (int i = 0; i < layer_count; i++) {
        layer_destroy(layer_registry[i]);
        free(layer_names[i]);
        layer_registry[i] = NULL;
        layer_names[i] = NULL;
    }
    layer_count = 0;
    // Screen blit resources
    if (screen_vbo) { glDeleteBuffers(1, &screen_vbo); screen_vbo = 0; }
    if (screen_vao) { glDeleteVertexArrays(1, &screen_vao); screen_vao = 0; }
    if (screen_shader) { glDeleteProgram(screen_shader); screen_shader = 0; }
    // Other resources
    if (L) { lua_close(L); L = NULL; }
    if (gl_context) { SDL_GL_DeleteContext(gl_context); gl_context = NULL; }
    if (window) { SDL_DestroyWindow(window); window = NULL; }
    SDL_Quit();
}

// One frame of the main loop
static void main_loop_iteration(void) {
    Uint64 current_time = SDL_GetPerformanceCounter();
    double dt = (double)(current_time - last_time) / (double)perf_freq;
    last_time = current_time;

    // Clamp delta time to handle anomalies (pauses, debugger, sleep resume)
    if (dt > PHYSICS_RATE * MAX_UPDATES) {
        dt = PHYSICS_RATE;
    }
    if (dt < 0) {
        dt = 0;
    }

    // VSync snapping: if dt is close to a known refresh rate, snap to it exactly
    // This prevents accumulator drift from timer jitter
    for (int i = 0; i < snap_frequency_count; i++) {
        double diff = dt - snap_frequencies[i];
        if (diff < 0) diff = -diff;
        if (diff < VSYNC_SNAP_TOLERANCE) {
            dt = snap_frequencies[i];
            break;
        }
    }

    // Accumulate physics lag, capped to prevent spiral of death
    physics_lag += dt;
    if (physics_lag > PHYSICS_RATE * MAX_UPDATES) {
        physics_lag = PHYSICS_RATE * MAX_UPDATES;
    }

    // Accumulate render lag, capped to prevent unbounded growth
    render_lag += dt;
    if (render_lag > RENDER_RATE * 2) {
        render_lag = RENDER_RATE * 2;
    }

    // Process events every frame (not tied to fixed timestep)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
        // Track keyboard state
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            last_input_type = INPUT_TYPE_KEYBOARD;
            SDL_Scancode sc = event.key.keysym.scancode;
            if (sc < SDL_NUM_SCANCODES) {
                keys_current[sc] = true;
            }
            // Capture mode: capture the key
            if (capture_mode && captured_control[0] == '\0') {
                const char* key_name = scancode_to_key_name(sc);
                if (key_name) {
                    snprintf(captured_control, sizeof(captured_control), "key:%s", key_name);
                }
            }
            // Built-in key handling (skip if in capture mode)
            if (!capture_mode) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }
            #ifndef __EMSCRIPTEN__
            // Fullscreen toggle only on desktop
            if (event.key.keysym.sym == SDLK_F11 ||
                (event.key.keysym.sym == SDLK_RETURN && (event.key.keysym.mod & KMOD_ALT))) {
                Uint32 flags = SDL_GetWindowFlags(window);
                SDL_SetWindowFullscreen(window, (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            #endif
        }
        if (event.type == SDL_KEYUP) {
            SDL_Scancode sc = event.key.keysym.scancode;
            if (sc < SDL_NUM_SCANCODES) {
                keys_current[sc] = false;
            }
        }
        // Track mouse motion
        if (event.type == SDL_MOUSEMOTION) {
            last_input_type = INPUT_TYPE_MOUSE;
            mouse_x = event.motion.x;
            mouse_y = event.motion.y;
            mouse_dx += event.motion.xrel;
            mouse_dy += event.motion.yrel;
        }
        // Track mouse buttons
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            last_input_type = INPUT_TYPE_MOUSE;
            int btn = event.button.button - 1;  // SDL buttons are 1-indexed
            if (btn >= 0 && btn < MAX_MOUSE_BUTTONS) {
                mouse_buttons_current[btn] = true;
            }
            // Capture mode: capture the mouse button
            if (capture_mode && captured_control[0] == '\0') {
                snprintf(captured_control, sizeof(captured_control), "mouse:%d", event.button.button);
            }
        }
        if (event.type == SDL_MOUSEBUTTONUP) {
            int btn = event.button.button - 1;
            if (btn >= 0 && btn < MAX_MOUSE_BUTTONS) {
                mouse_buttons_current[btn] = false;
            }
        }
        // Track mouse wheel
        if (event.type == SDL_MOUSEWHEEL) {
            mouse_wheel_x += event.wheel.x;
            mouse_wheel_y += event.wheel.y;
        }
        // Handle window focus events - resync timing to prevent catch-up stutter
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                timing_resync();
            }
        }
        // Gamepad hotplug handling
        if (event.type == SDL_CONTROLLERDEVICEADDED) {
            if (!gamepad) {
                gamepad = SDL_GameControllerOpen(event.cdevice.which);
                if (gamepad) {
                    printf("Gamepad connected: %s\n", SDL_GameControllerName(gamepad));
                }
            }
        }
        if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (gamepad && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad))) {
                printf("Gamepad disconnected\n");
                SDL_GameControllerClose(gamepad);
                gamepad = NULL;
                // Clear gamepad state
                memset(gamepad_buttons_current, 0, sizeof(gamepad_buttons_current));
                memset(gamepad_buttons_previous, 0, sizeof(gamepad_buttons_previous));
                memset(gamepad_axes, 0, sizeof(gamepad_axes));
                memset(gamepad_axes_previous, 0, sizeof(gamepad_axes_previous));
            }
        }
    }

    // Update gamepad state (poll axes and buttons)
    gamepad_update();

    // Fixed timestep physics/input loop (120Hz)
    while (physics_lag >= PHYSICS_RATE) {
        // Clear commands on all layers at start of update
        for (int i = 0; i < layer_count; i++) {
            layer_clear_commands(layer_registry[i]);
        }

        // Update sequences (clear just_fired from last frame, check timeouts)
        sequences_update((float)game_time);

        // Check which actions were pressed and notify sequences
        sequences_check_actions((float)game_time);

        // Update holds (track how long actions have been held)
        holds_update((float)PHYSICS_RATE);

        // Call Lua update (skip if in error state)
        if (!error_state) {
            lua_pushcfunction(L, traceback);
            int err_handler = lua_gettop(L);
            lua_getglobal(L, "update");
            if (lua_isfunction(L, -1)) {
                lua_pushnumber(L, PHYSICS_RATE);
                if (lua_pcall(L, 1, 0, err_handler) != LUA_OK) {
                    snprintf(error_message, sizeof(error_message), "%s", lua_tostring(L, -1));
                    fprintf(stderr, "ERROR: %s\n", error_message);
                    lua_pop(L, 2);  // error + traceback
                    error_state = true;
                } else {
                    lua_pop(L, 1);  // traceback
                }
            } else {
                lua_pop(L, 2);  // nil + traceback
            }
        }

        step++;
        game_time += PHYSICS_RATE;
        physics_lag -= PHYSICS_RATE;

        // Copy current input state to previous for next frame's edge detection
        input_post_update();
        chords_post_update();
    }

    // Render at 60Hz (for chunky pixel movement on high-refresh monitors)
    if (render_lag >= RENDER_RATE) {
        render_lag -= RENDER_RATE;
        frame++;

        // Set up orthographic projection (game coordinates)
        // Maps (0,0) at top-left to (width, height) at bottom-right
        float projection[16] = {
            2.0f / GAME_WIDTH, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / GAME_HEIGHT, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };

        glUseProgram(shader_program);
        GLint proj_loc = glGetUniformLocation(shader_program, "projection");
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);

        // Set AA width based on filter mode (0 = rough/hard edges, 1 = smooth)
        GLint aa_loc = glGetUniformLocation(shader_program, "u_aa_width");
        float aa_width = (shape_filter_mode == FILTER_SMOOTH) ? 1.0f : 0.0f;
        glUniform1f(aa_loc, aa_width);

        // === PASS 1: Render each layer to its FBO ===
        glBindTexture(GL_TEXTURE_2D, 0);  // Unbind to avoid feedback loop

        for (int i = 0; i < layer_count; i++) {
            Layer* layer = layer_registry[i];
            glBindFramebuffer(GL_FRAMEBUFFER, layer->fbo);
            glViewport(0, 0, layer->width, layer->height);

            if (error_state) {
                glClearColor(0.3f, 0.1f, 0.1f, 1.0f);  // Dark red for error
            } else {
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent black
            }
            glClear(GL_COLOR_BUFFER_BIT);

            layer_render(layer);
        }

        // === PASS 2: Composite all layers to screen ===
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Get current window size
        int window_w, window_h;
        SDL_GetWindowSize(window, &window_w, &window_h);

        // Calculate scale to fit window while maintaining aspect ratio
        // Use integer scaling for pixel-perfect rendering
        float scale_x = (float)window_w / GAME_WIDTH;
        float scale_y = (float)window_h / GAME_HEIGHT;
        float scale = (scale_x < scale_y) ? scale_x : scale_y;
        int int_scale = (int)scale;
        if (int_scale < 1) int_scale = 1;

        // Calculate centered position with letterboxing
        int scaled_w = GAME_WIDTH * int_scale;
        int scaled_h = GAME_HEIGHT * int_scale;
        int offset_x = (window_w - scaled_w) / 2;
        int offset_y = (window_h - scaled_h) / 2;

        // Clear screen to black (letterbox color)
        glViewport(0, 0, window_w, window_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Set viewport for game area
        glViewport(offset_x, offset_y, scaled_w, scaled_h);
        glUseProgram(screen_shader);

        // Get offset uniform location
        GLint offset_loc = glGetUniformLocation(screen_shader, "u_offset");

        if (layer_draw_count > 0) {
            // Manual compositing: use layer_draw queue
            for (int i = 0; i < layer_draw_count; i++) {
                LayerDrawCommand* cmd = &layer_draw_queue[i];
                Layer* layer = cmd->layer;

                // Convert game coordinates to NDC offset
                // Game coords: (0,0) top-left, positive Y down
                // NDC: (-1,-1) bottom-left, positive Y up
                // Offset in NDC = (game_offset / game_size) * 2
                float ndc_x = (cmd->x / GAME_WIDTH) * 2.0f;
                float ndc_y = -(cmd->y / GAME_HEIGHT) * 2.0f;  // Flip Y
                glUniform2f(offset_loc, ndc_x, ndc_y);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer));

                glBindVertexArray(screen_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }

            // Reset all layers' effect state
            for (int i = 0; i < layer_count; i++) {
                layer_reset_effects(layer_registry[i]);
            }

            // Clear the draw queue for next frame
            layer_draw_count = 0;
        } else {
            // Automatic compositing: blit each layer in order (first created = bottom)
            glUniform2f(offset_loc, 0.0f, 0.0f);  // No offset for automatic

            for (int i = 0; i < layer_count; i++) {
                Layer* layer = layer_registry[i];
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer));

                glBindVertexArray(screen_vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                // Reset effect state for next frame
                layer_reset_effects(layer);
            }
        }

        SDL_GL_SwapWindow(window);
    }

    #ifdef __EMSCRIPTEN__
    if (!running) {
        emscripten_cancel_main_loop();
        shutdown();
    }
    #endif
}

int main(int argc, char* argv[]) {
    printf("Anchor Engine starting...\n");

    // Change working directory to game folder (passed as argument, like LÖVE)
    if (argc > 1) {
        const char* game_folder = argv[1];
        #ifdef _WIN32
        _chdir(game_folder);
        #else
        chdir(game_folder);
        #endif
        printf("Game folder: %s\n", game_folder);
    }

    printf("Loading: main.lua\n");

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    #ifdef __EMSCRIPTEN__
    // Request WebGL 2.0 (OpenGL ES 3.0)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    #else
    // Request OpenGL 3.3 Core Profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    #endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        GAME_WIDTH * INITIAL_SCALE, GAME_HEIGHT * INITIAL_SCALE,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        shutdown();
        return 1;
    }

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        shutdown();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);  // VSync

    #ifndef __EMSCRIPTEN__
    // Load OpenGL functions (desktop only - Emscripten provides them)
    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (version == 0) {
        fprintf(stderr, "gladLoadGL failed\n");
        shutdown();
        return 1;
    }
    printf("OpenGL %d.%d loaded\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    #else
    printf("WebGL 2.0 (OpenGL ES 3.0) context created\n");
    #endif
    printf("Renderer: %s\n", glGetString(GL_RENDERER));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create shader program
    shader_program = create_shader_program(vertex_shader_source, fragment_shader_source);
    if (!shader_program) {
        fprintf(stderr, "Failed to create shader program\n");
        shutdown();
        return 1;
    }
    printf("Shader program created\n");

    // Set up VAO and VBO for dynamic quad rendering
    // Vertex format: x, y, u, v, r, g, b, a, type, shape[4] (13 floats per vertex)
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // Allocate space for batch rendering
    glBufferData(GL_ARRAY_BUFFER, MAX_BATCH_VERTICES * VERTEX_FLOATS * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    // Stride = 16 floats = 64 bytes
    int stride = VERTEX_FLOATS * sizeof(float);

    // Position attribute (location 0): 2 floats at offset 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // UV attribute (location 1): 2 floats at offset 2
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Color attribute (location 2): 4 floats at offset 4
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Type attribute (location 3): 1 float at offset 8
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // Shape attribute (location 4): 4 floats at offset 9
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(4);

    // AddColor attribute (location 5): 3 floats at offset 13 (additive flash color)
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, stride, (void*)(13 * sizeof(float)));
    glEnableVertexAttribArray(5);

    glBindVertexArray(0);
    printf("Game VAO/VBO created (stride=%d bytes)\n", stride);

    // Create screen shader for blitting layers
    screen_shader = create_shader_program(screen_vertex_source, screen_fragment_source);
    if (!screen_shader) {
        fprintf(stderr, "Failed to create screen shader\n");
        shutdown();
        return 1;
    }
    printf("Screen shader created\n");

    // Set up screen quad VAO/VBO (fullscreen quad in NDC, viewport handles positioning)
    // Vertex format: x, y, u, v (4 floats per vertex)
    float screen_vertices[] = {
        // pos         // tex
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &screen_vao);
    glGenBuffers(1, &screen_vbo);

    glBindVertexArray(screen_vao);
    glBindBuffer(GL_ARRAY_BUFFER, screen_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screen_vertices), screen_vertices, GL_STATIC_DRAW);

    // Position attribute (location 0): 2 floats
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // TexCoord attribute (location 1): 2 floats
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    printf("Screen VAO/VBO created\n");

    // Initialize Lua
    L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "luaL_newstate failed\n");
        shutdown();
        return 1;
    }
    luaL_openlibs(L);
    register_lua_bindings(L);

    // Initialize gamepad (check for already-connected controllers)
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            gamepad = SDL_GameControllerOpen(i);
            if (gamepad) {
                printf("Gamepad found at startup: %s\n", SDL_GameControllerName(gamepad));
                break;  // Only use first gamepad
            }
        }
    }

    // Load and run script with traceback
    lua_pushcfunction(L, traceback);
    int err_handler = lua_gettop(L);
    if (luaL_loadfile(L, "main.lua") != LUA_OK) {
        snprintf(error_message, sizeof(error_message), "%s", lua_tostring(L, -1));
        fprintf(stderr, "ERROR: %s\n", error_message);
        lua_pop(L, 2);  // error + traceback
        error_state = true;
    } else if (lua_pcall(L, 0, 0, err_handler) != LUA_OK) {
        snprintf(error_message, sizeof(error_message), "%s", lua_tostring(L, -1));
        fprintf(stderr, "ERROR: %s\n", error_message);
        lua_pop(L, 2);  // error + traceback
        error_state = true;
    } else {
        lua_pop(L, 1);  // traceback
    }

    printf("Initialization complete. Press ESC to exit, F11 for fullscreen.\n");

    // Initialize timing state
    perf_freq = SDL_GetPerformanceFrequency();
    last_time = SDL_GetPerformanceCounter();

    // Initialize vsync snap frequencies based on display refresh rate
    {
        int display_hz = 60;  // Default fallback
        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.refresh_rate > 0) {
            display_hz = mode.refresh_rate;
        }
        printf("Display refresh rate: %dHz\n", display_hz);

        // Compute snap frequencies for this refresh rate and its multiples
        // (handles 1x, 2x, 3x... of the base frame time for missed frames)
        double base_frametime = 1.0 / (double)display_hz;
        snap_frequency_count = 0;
        for (int i = 1; i <= 8 && snap_frequency_count < 8; i++) {
            snap_frequencies[snap_frequency_count++] = base_frametime * i;
        }
    }

    #ifdef __EMSCRIPTEN__
    // Use browser's requestAnimationFrame
    // 0 = use RAF, 1 = simulate infinite loop (blocking)
    emscripten_set_main_loop(main_loop_iteration, 0, 1);
    #else
    // Desktop: traditional blocking loop
    while (running) {
        main_loop_iteration();
    }

    printf("Shutting down...\n");
    shutdown();
    #endif

    return 0;
}
