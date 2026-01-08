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
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
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
        // Handle window focus events - resync timing to prevent catch-up stutter
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                timing_resync();
            }
        }
    }

    // Fixed timestep physics/input loop (120Hz)
    while (physics_lag >= PHYSICS_RATE) {
        // Clear commands on all layers at start of update
        for (int i = 0; i < layer_count; i++) {
            layer_clear_commands(layer_registry[i]);
        }

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
