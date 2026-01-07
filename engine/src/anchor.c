/*
 * Anchor Engine - Minimal skeleton
 * Phase 1: Window + OpenGL + Lua integration
 * Phase 2: Web build (Emscripten/WebGL)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

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

#define WINDOW_TITLE "Anchor"
#define GAME_WIDTH 480
#define GAME_HEIGHT 270
#define INITIAL_SCALE 3

// Timing configuration (matching reference Anchor)
#define FIXED_RATE (1.0 / 144.0)  // 144 Hz fixed timestep
#define MAX_UPDATES 10            // Cap on fixed steps per frame (prevents spiral of death)

// Transform stack depth
#define MAX_TRANSFORM_DEPTH 32

// Initial command queue capacity
#define INITIAL_COMMAND_CAPACITY 256

// Shape types
enum {
    SHAPE_RECTANGLE = 0,
    SHAPE_CIRCLE,
    SHAPE_SPRITE,
};

// Blend modes
enum {
    BLEND_ALPHA = 0,
    BLEND_ADDITIVE,
    BLEND_MULTIPLY,
};

// DrawCommand — stores one deferred draw call
typedef struct {
    uint8_t type;           // SHAPE_RECTANGLE, SHAPE_CIRCLE, SHAPE_SPRITE
    uint8_t blend_mode;     // BLEND_ALPHA, BLEND_ADDITIVE, BLEND_MULTIPLY
    uint8_t _pad[2];

    float transform[6];     // 2D affine matrix (2x3): [m00 m01 m02 m10 m11 m12]
    uint32_t color;         // Packed RGBA

    // Shape parameters (meaning depends on type)
    // RECTANGLE: params[0]=x, [1]=y, [2]=w, [3]=h
    // CIRCLE: params[0]=x, [1]=y, [2]=radius
    // SPRITE: params[0]=x, [1]=y, [2]=w, [3]=h, [4]=ox, [5]=oy (+ texture_id)
    float params[8];

    GLuint texture_id;      // For SPRITE
} DrawCommand;

// Layer
typedef struct {
    GLuint fbo;
    GLuint color_texture;
    int width;
    int height;

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

    // Initialize command queue
    layer->commands = (DrawCommand*)malloc(INITIAL_COMMAND_CAPACITY * sizeof(DrawCommand));
    if (!layer->commands) {
        free(layer);
        return NULL;
    }
    layer->command_count = 0;
    layer->command_capacity = INITIAL_COMMAND_CAPACITY;
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
    free(layer);
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
static DrawCommand* layer_add_command(Layer* layer) {
    // Grow if needed
    if (layer->command_count >= layer->command_capacity) {
        int new_capacity = layer->command_capacity * 2;
        DrawCommand* new_commands = (DrawCommand*)realloc(layer->commands,
            new_capacity * sizeof(DrawCommand));
        if (!new_commands) return NULL;
        layer->commands = new_commands;
        layer->command_capacity = new_capacity;
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
    cmd->type = SHAPE_RECTANGLE;
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
    cmd->type = SHAPE_CIRCLE;
    cmd->color = color;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = radius;
}

// Batch rendering
#define MAX_BATCH_VERTICES 6000  // 1000 quads * 6 vertices
#define VERTEX_FLOATS 13         // x, y, u, v, r, g, b, a, type, shape[4]

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

// Transform a point by a 2x3 matrix: [m0 m1 m2] [x]   [m0*x + m1*y + m2]
//                                    [m3 m4 m5] [y] = [m3*x + m4*y + m5]
//                                               [1]
static void transform_point(const float* m, float x, float y, float* out_x, float* out_y) {
    *out_x = m[0] * x + m[1] * y + m[2];
    *out_y = m[3] * x + m[4] * y + m[5];
}

// Unpack uint32 color to RGBA floats (0-1)
static void unpack_color(uint32_t color, float* r, float* g, float* b, float* a) {
    *r = ((color >> 24) & 0xFF) / 255.0f;
    *g = ((color >> 16) & 0xFF) / 255.0f;
    *b = ((color >> 8) & 0xFF) / 255.0f;
    *a = (color & 0xFF) / 255.0f;
}

// Add a vertex to the batch (13 floats per vertex)
static void batch_add_vertex(float x, float y, float u, float v,
                             float r, float g, float b, float a,
                             float type, float s0, float s1, float s2, float s3) {
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
    batch_vertices[i + 9] = s0;   // shape[0]
    batch_vertices[i + 10] = s1;  // shape[1]
    batch_vertices[i + 11] = s2;  // shape[2]
    batch_vertices[i + 12] = s3;  // shape[3]
    batch_vertex_count++;
}

// Add a quad (two triangles, 6 vertices) for SDF shapes
// UVs go from (0,0) to (1,1) across the quad
// Shape params are the same for all vertices
static void batch_add_sdf_quad(float x0, float y0, float x1, float y1,
                               float x2, float y2, float x3, float y3,
                               float r, float g, float b, float a,
                               float type, float s0, float s1, float s2, float s3) {
    // Quad corners with UVs:
    // 0(0,0)---1(1,0)
    // |         |
    // 3(0,1)---2(1,1)

    // Triangle 1: 0, 1, 2
    batch_add_vertex(x0, y0, 0.0f, 0.0f, r, g, b, a, type, s0, s1, s2, s3);
    batch_add_vertex(x1, y1, 1.0f, 0.0f, r, g, b, a, type, s0, s1, s2, s3);
    batch_add_vertex(x2, y2, 1.0f, 1.0f, r, g, b, a, type, s0, s1, s2, s3);
    // Triangle 2: 0, 2, 3
    batch_add_vertex(x0, y0, 0.0f, 0.0f, r, g, b, a, type, s0, s1, s2, s3);
    batch_add_vertex(x2, y2, 1.0f, 1.0f, r, g, b, a, type, s0, s1, s2, s3);
    batch_add_vertex(x3, y3, 0.0f, 1.0f, r, g, b, a, type, s0, s1, s2, s3);
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

// Main game layer
static Layer* game_layer = NULL;

// Screen blit resources
static GLuint screen_shader = 0;
static GLuint screen_vao = 0;
static GLuint screen_vbo = 0;

// Flush batch to GPU
static void batch_flush(void) {
    if (batch_vertex_count == 0) return;

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    batch_vertex_count * VERTEX_FLOATS * sizeof(float),
                    batch_vertices);
    glDrawArrays(GL_TRIANGLES, 0, batch_vertex_count);
    glBindVertexArray(0);

    batch_vertex_count = 0;
}

// Process a rectangle command (SDF-based)
static void process_rectangle(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float w = cmd->params[2];
    float h = cmd->params[3];

    // Add padding for anti-aliasing (1-2 pixels)
    float pad = 2.0f;

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

    // Compute center and half-size in world space (for SDF)
    // Note: This assumes no rotation in transform for now
    float cx, cy;
    transform_point(cmd->transform, x + w * 0.5f, y + h * 0.5f, &cx, &cy);
    float half_w = w * 0.5f;
    float half_h = h * 0.5f;

    // Unpack color
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Add SDF quad: shape = (center.x, center.y, half_w, half_h)
    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_RECT, cx, cy, half_w, half_h);
}

// Process a circle command (SDF-based)
static void process_circle(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float radius = cmd->params[2];

    // Add padding for anti-aliasing
    float pad = 2.0f;

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

    // Transform center to world space
    float cx, cy;
    transform_point(cmd->transform, x, y, &cx, &cy);

    // Unpack color
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Add SDF quad: shape = (center.x, center.y, radius, unused)
    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_CIRCLE, cx, cy, radius, 0.0f);
}

// Render all commands on a layer
static void layer_render(Layer* layer) {
    batch_vertex_count = 0;

    for (int i = 0; i < layer->command_count; i++) {
        const DrawCommand* cmd = &layer->commands[i];

        switch (cmd->type) {
            case SHAPE_RECTANGLE:
                process_rectangle(cmd);
                break;
            case SHAPE_CIRCLE:
                process_circle(cmd);
                break;
            case SHAPE_SPRITE:
                // TODO: Step 7
                break;
        }

        // Flush if batch is getting full
        if (batch_vertex_count >= MAX_BATCH_VERTICES - 6) {
            batch_flush();
        }
    }

    // Final flush
    batch_flush();
}

// Lua bindings
static int l_layer_create(lua_State* L) {
    // For now, ignore name and return game_layer
    // const char* name = luaL_checkstring(L, 1);
    (void)L;  // unused parameter warning
    lua_pushlightuserdata(L, game_layer);
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

static void register_lua_bindings(lua_State* L) {
    lua_register(L, "layer_create", l_layer_create);
    lua_register(L, "layer_rectangle", l_layer_rectangle);
    lua_register(L, "layer_circle", l_layer_circle);
    lua_register(L, "rgba", l_rgba);
    lua_register(L, "set_shape_filter", l_set_shape_filter);
}

// Main loop state (needed for emscripten)
static bool running = true;
static Uint64 perf_freq = 0;
static Uint64 last_time = 0;
static double lag = 0.0;
static Uint64 step = 0;
static double game_time = 0.0;
static Uint64 frame = 0;

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
    "\n"
    "out vec2 vPos;\n"
    "out vec2 vUV;\n"
    "out vec4 vColor;\n"
    "out float vType;\n"
    "out vec4 vShape;\n"
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
    "}\n";

static const char* fragment_shader_source =
    "in vec2 vPos;\n"
    "in vec2 vUV;\n"
    "in vec4 vColor;\n"
    "in float vType;\n"
    "in vec4 vShape;\n"
    "\n"
    "out vec4 FragColor;\n"
    "\n"
    "uniform float u_aa_width;\n"
    "\n"
    "// SDF for rectangle: shape = (center.x, center.y, half_w, half_h)\n"
    "float sdf_rect(vec2 p, vec2 center, vec2 half_size) {\n"
    "    vec2 d = abs(p - center) - half_size;\n"
    "    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);\n"
    "}\n"
    "\n"
    "// SDF for circle: shape = (center.x, center.y, radius, unused)\n"
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
    "    // In rough mode, snap position to pixel centers for chunky look\n"
    "    vec2 p = (u_aa_width > 0.0) ? vPos : floor(vPos) + 0.5;\n"
    "    \n"
    "    if (vType < 0.5) {\n"
    "        // Rectangle\n"
    "        vec2 center = vShape.xy;\n"
    "        vec2 half_size = vShape.zw;\n"
    "        d = sdf_rect(p, center, half_size);\n"
    "    } else if (vType < 1.5) {\n"
    "        // Circle\n"
    "        vec2 center = vShape.xy;\n"
    "        float radius = vShape.z;\n"
    "        if (u_aa_width == 0.0) {\n"
    "            // Rough mode: snap center and radius to pixel grid\n"
    "            center = floor(center) + 0.5;\n"
    "            radius = floor(radius + 0.5);\n"
    "            d = sdf_circle_pixel(p, center, radius);\n"
    "        } else {\n"
    "            d = sdf_circle(p, center, radius);\n"
    "        }\n"
    "    } else {\n"
    "        // Sprite (future) - for now just solid color\n"
    "        FragColor = vColor;\n"
    "        return;\n"
    "    }\n"
    "    \n"
    "    // Apply anti-aliasing (or hard edges when u_aa_width = 0)\n"
    "    float alpha;\n"
    "    if (u_aa_width > 0.0) {\n"
    "        alpha = 1.0 - smoothstep(-u_aa_width, u_aa_width, d);\n"
    "    } else {\n"
    "        alpha = 1.0 - step(0.0, d);\n"
    "    }\n"
    "    FragColor = vec4(vColor.rgb, vColor.a * alpha);\n"
    "}\n";

static const char* screen_vertex_source =
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
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
    // Layer
    if (game_layer) { layer_destroy(game_layer); game_layer = NULL; }
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

    // Accumulate lag, capped to prevent spiral of death
    lag += dt;
    if (lag > FIXED_RATE * MAX_UPDATES) {
        lag = FIXED_RATE * MAX_UPDATES;
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
    }

    // Fixed timestep loop
    bool did_update = false;
    while (lag >= FIXED_RATE) {
        did_update = true;

        // Clear commands at start of update (so they persist if no update runs)
        layer_clear_commands(game_layer);

        // Call Lua update (skip if in error state)
        if (!error_state) {
            lua_pushcfunction(L, traceback);
            int err_handler = lua_gettop(L);
            lua_getglobal(L, "update");
            if (lua_isfunction(L, -1)) {
                lua_pushnumber(L, FIXED_RATE);
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
        game_time += FIXED_RATE;
        lag -= FIXED_RATE;
    }

    // Render (once per frame, not per fixed step)
    frame++;

    // === PASS 1: Render game to layer ===
    glBindFramebuffer(GL_FRAMEBUFFER, game_layer->fbo);
    glViewport(0, 0, game_layer->width, game_layer->height);

    if (error_state) {
        glClearColor(0.3f, 0.1f, 0.1f, 1.0f);  // Dark red for error
    } else {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Black
    }
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up orthographic projection (game coordinates)
    // Maps (0,0) at top-left to (width, height) at bottom-right
    float projection[16] = {
        2.0f / game_layer->width, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / game_layer->height, 0.0f, 0.0f,
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

    // Render all commands (added by Lua during update)
    layer_render(game_layer);

    // === PASS 2: Blit layer to screen with aspect-ratio scaling ===
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Get current window size
    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);
    glViewport(0, 0, window_w, window_h);

    // Calculate scale to fit window while maintaining aspect ratio
    // Use integer scaling for pixel-perfect rendering
    float scale_x = (float)window_w / game_layer->width;
    float scale_y = (float)window_h / game_layer->height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    int int_scale = (int)scale;
    if (int_scale < 1) int_scale = 1;

    // Calculate centered position with letterboxing
    int scaled_w = game_layer->width * int_scale;
    int scaled_h = game_layer->height * int_scale;
    int offset_x = (window_w - scaled_w) / 2;
    int offset_y = (window_h - scaled_h) / 2;

    // Clear screen to black (letterbox color)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw layer texture (viewport handles positioning)
    glViewport(offset_x, offset_y, scaled_w, scaled_h);
    glUseProgram(screen_shader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, game_layer->color_texture);

    glBindVertexArray(screen_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    SDL_GL_SwapWindow(window);

    #ifdef __EMSCRIPTEN__
    if (!running) {
        emscripten_cancel_main_loop();
        shutdown();
    }
    #endif
}

int main(int argc, char* argv[]) {
    const char* script_path = (argc > 1) ? argv[1] : "main.lua";
    printf("Anchor Engine starting...\n");
    printf("Loading: %s\n", script_path);

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

    // Stride = 13 floats = 52 bytes
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

    glBindVertexArray(0);
    printf("Game VAO/VBO created (stride=%d bytes)\n", stride);

    // Create game layer
    game_layer = layer_create(GAME_WIDTH, GAME_HEIGHT);
    if (!game_layer) {
        fprintf(stderr, "Failed to create game layer\n");
        shutdown();
        return 1;
    }
    printf("Game layer created (%dx%d)\n", game_layer->width, game_layer->height);

    // Create screen shader for blitting layer
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
    if (luaL_loadfile(L, script_path) != LUA_OK) {
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
