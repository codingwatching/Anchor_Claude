/*
 * Anchor Engine - Single-file C implementation
 * SDL2 + OpenGL 3.3 (WebGL 2.0) + Lua 5.4 + Box2D 3.x
 *
 * FILE STRUCTURE (search for section banners):
 *
 * - Includes, constants, core structs (DrawCommand, Layer)
 * - Physics foundation (tags, events, PCG32 random)
 * - Resources (Texture, Font, Sound, Music)
 * - Layer system (FBO, transforms, command queue, batching)
 * - Input system (keyboard, mouse, gamepad, actions, chords, sequences, holds)
 * - Rendering pipeline (shader execution, command processing)
 * - Lua bindings: Rendering, Physics, Random, Input
 * - Lua registration (register_lua_bindings)
 * - Shader sources and compilation
 * - Main loop, initialization, shutdown
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <direct.h>  // _chdir
#include <windows.h>  // RegisterHotKey, PeekMessage for global hotkeys
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

#define STB_PERLIN_IMPLEMENTATION
#include <stb_perlin.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#include <ft2build.h>
#include <freetype/freetype.h>

#define MA_ENABLE_VORBIS
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <box2d.h>

// Miniz for zip archive support (single exe distribution)
#ifndef __EMSCRIPTEN__
#define MINIZ_IMPL
#include <miniz.h>
#endif

// ============================================================================
// CONFIGURATION & CONSTANTS
// ============================================================================

// Default configuration (can be changed via Lua before engine_init)
static char window_title[256] = "Anchor";
static int game_width = 480;
static int game_height = 270;
static float initial_scale = 3.0f;
static bool vsync_enabled = true;
static bool start_fullscreen = false;
static bool window_resizable = true;
static bool headless_mode = false;  // Headless mode: no window, no rendering, max speed
static bool render_mode = false;    // Render mode: window + rendering, deterministic timing, frame capture
static GLuint capture_fbo = 0;
static GLuint capture_texture = 0;
static unsigned char* capture_buffer = NULL;
static int capture_frame_number = 0;
static char capture_output_dir[512] = "";
static FILE* record_pipe = NULL;  // Live recording: ffmpeg pipe for raw frame data
static double time_scale = 1.0;  // Time scale multiplier (0 = hitstop, 1 = normal)

// CLI arguments (--key=value pairs stored for Lua access)
#define MAX_CLI_ARGS 32
#define MAX_CLI_KEY 64
#define MAX_CLI_VALUE 256
static struct { char key[MAX_CLI_KEY]; char value[MAX_CLI_VALUE]; } cli_args[MAX_CLI_ARGS];
static int cli_arg_count = 0;
static bool running = true;  // Main loop flag (file scope so engine_quit can access it)
// filter_mode is defined later in the font section

// Timing configuration
#define PHYSICS_RATE (1.0 / 120.0)  // 120 Hz physics/input timestep
#define RENDER_RATE  (1.0 / 60.0)   // 60 Hz render cap (for chunky pixel movement)
#define MAX_UPDATES 10              // Cap on fixed steps per frame (prevents spiral of death)

// VSync snapping - snap delta times within 0.2ms of common refresh rates
// This prevents accumulator drift from timer jitter
#define VSYNC_SNAP_TOLERANCE 0.0002

// Mathematical constants
#define PI 3.14159265358979323846

// Forward declarations
static void timing_resync(void);
static GLuint create_shader_program(const char* vert_src, const char* frag_src);
// Shader sources (defined in SHADERS section, needed by engine_init)
extern const char* vertex_shader_source;
extern const char* fragment_shader_source;
extern const char* screen_vertex_source;
extern const char* screen_fragment_source;

// ============================================================================
// ZIP ARCHIVE SUPPORT (Desktop only - single exe distribution)
// Detects zip data appended to executable and reads assets from it
// ============================================================================

#ifndef __EMSCRIPTEN__
static mz_zip_archive zip_archive;
static bool zip_initialized = false;
static unsigned char* zip_data = NULL;
static size_t zip_data_size = 0;

// Initialize zip archive from executable if present
// Returns true if zip was found and initialized, false otherwise
static bool zip_init(const char* exe_path) {
    FILE* f = fopen(exe_path, "rb");
    if (!f) return false;

    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    if (file_size < 22) {  // Minimum size for zip end-of-central-dir header
        fclose(f);
        return false;
    }

    // Read entire file
    fseek(f, 0, SEEK_SET);
    unsigned char* full_file = (unsigned char*)malloc(file_size);
    if (!full_file) {
        fclose(f);
        return false;
    }
    if (fread(full_file, 1, file_size, f) != (size_t)file_size) {
        free(full_file);
        fclose(f);
        return false;
    }
    fclose(f);

    // Search backwards for EOCD signature (PK\x05\x06)
    // EOCD can have a comment (up to 65535 bytes), so search from end
    long search_start = file_size - 22;
    long search_end = file_size - 65535 - 22;
    if (search_end < 0) search_end = 0;

    long eocd_pos = -1;
    for (long i = search_start; i >= search_end; i--) {
        if (full_file[i] == 0x50 && full_file[i+1] == 0x4b &&
            full_file[i+2] == 0x05 && full_file[i+3] == 0x06) {
            eocd_pos = i;
            break;
        }
    }

    if (eocd_pos < 0) {
        free(full_file);
        return false;
    }

    // Found EOCD - read central directory offset and size
    // EOCD structure: signature(4) + disk stuff(4) + cd_entries(4) + cd_size(4) + cd_offset(4) + comment_len(2)
    uint32_t cd_size = *(uint32_t*)(full_file + eocd_pos + 12);
    uint32_t cd_offset = *(uint32_t*)(full_file + eocd_pos + 16);

    // Central directory ends right before EOCD, so CD starts at (eocd_pos - cd_size)
    long cd_start_absolute = eocd_pos - cd_size;
    if (cd_start_absolute < 0) {
        free(full_file);
        return false;
    }

    // The cd_offset is relative to zip start, so: zip_start = cd_start_absolute - cd_offset
    long zip_start = cd_start_absolute - cd_offset;
    if (zip_start < 0) {
        free(full_file);
        return false;
    }

    // Verify zip start looks like a local file header (PK\x03\x04)
    if (full_file[zip_start] != 0x50 || full_file[zip_start+1] != 0x4b ||
        full_file[zip_start+2] != 0x03 || full_file[zip_start+3] != 0x04) {
        free(full_file);
        return false;
    }

    // Extract just the zip portion
    zip_data_size = file_size - zip_start;
    zip_data = (unsigned char*)malloc(zip_data_size);
    if (!zip_data) {
        free(full_file);
        return false;
    }
    memcpy(zip_data, full_file + zip_start, zip_data_size);
    free(full_file);

    // Initialize miniz with the zip data
    memset(&zip_archive, 0, sizeof(zip_archive));
    if (!mz_zip_reader_init_mem(&zip_archive, zip_data, zip_data_size, 0)) {
        free(zip_data);
        zip_data = NULL;
        return false;
    }

    zip_initialized = true;
    printf("Loaded %d files from embedded zip (%zu bytes)\n",
           (int)mz_zip_reader_get_num_files(&zip_archive), zip_data_size);
    return true;
}

// Read a file from zip archive or disk
// Returns malloc'd buffer (caller must free), sets *out_size
// Returns NULL if file not found
static void* zip_read_file(const char* path, size_t* out_size) {
    // Try zip first if initialized
    if (zip_initialized) {
        // Try original path first
        void* data = mz_zip_reader_extract_file_to_heap(&zip_archive, path, out_size, 0);
        if (data) return data;

        // PowerShell's Compress-Archive uses backslashes on Windows
        // Try with opposite separator if original path failed
        char alt_path[512];
        size_t len = strlen(path);
        if (len < sizeof(alt_path)) {
            strcpy(alt_path, path);
            for (char* p = alt_path; *p; p++) {
                if (*p == '/') *p = '\\';
                else if (*p == '\\') *p = '/';
            }
            data = mz_zip_reader_extract_file_to_heap(&zip_archive, alt_path, out_size, 0);
            if (data) return data;
        }
    }

    // Fall back to disk
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_size = size;
    return data;
}

// Cleanup zip archive
static void zip_shutdown(void) {
    if (zip_initialized) {
        mz_zip_reader_end(&zip_archive);
        zip_initialized = false;
    }
    if (zip_data) {
        free(zip_data);
        zip_data = NULL;
    }
}
#else
// Emscripten - use regular file loading (files are preloaded via --preload-file)
static bool zip_initialized = false;
static bool zip_init(const char* exe_path) { (void)exe_path; return false; }
static void* zip_read_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_size = size;
    return data;
}
static void zip_shutdown(void) {}
#endif

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
    COMMAND_GLYPH,              // Font glyph with custom UVs (uses flash_color for packed UVs)
    COMMAND_SPRITESHEET_FRAME,  // Spritesheet frame with custom UVs and flash support
    COMMAND_LINE,               // Line segment / capsule
    COMMAND_TRIANGLE,           // Triangle (3 vertices)
    COMMAND_POLYGON,            // Polygon (up to 8 vertices)
    COMMAND_ROUNDED_RECTANGLE,  // Rounded rectangle
    COMMAND_RECTANGLE_GRADIENT_H, // Horizontal gradient rectangle (left to right)
    COMMAND_RECTANGLE_GRADIENT_V, // Vertical gradient rectangle (top to bottom)
    COMMAND_APPLY_SHADER,       // Post-process layer through a shader
    COMMAND_SET_UNIFORM_FLOAT,  // Set float uniform on shader
    COMMAND_SET_UNIFORM_VEC2,   // Set vec2 uniform on shader
    COMMAND_SET_UNIFORM_VEC4,   // Set vec4 uniform on shader
    COMMAND_SET_UNIFORM_INT,    // Set int uniform on shader
    COMMAND_SET_UNIFORM_TEXTURE, // Bind a texture to a sampler uniform
    COMMAND_STENCIL_MASK,       // Start writing to stencil buffer (don't draw to color)
    COMMAND_STENCIL_TEST,       // Start testing against stencil (only draw where stencil is set)
    COMMAND_STENCIL_TEST_INVERSE, // Start testing against stencil (only draw where stencil is NOT set)
    COMMAND_STENCIL_OFF,        // Disable stencil, return to normal drawing
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

    union {
        uint32_t color;           // Packed RGBA for multiply/tint (shapes)
        uint32_t uniform_location; // Uniform location (SET_UNIFORM_* commands)
    };

    // Shape parameters (meaning depends on type)
    // RECTANGLE: params[0]=x, [1]=y, [2]=w, [3]=h, [4]=stroke (0=filled, >0=line width)
    // CIRCLE: params[0]=x, [1]=y, [2]=radius, [3]=stroke
    // LINE: params[0]=x1, [1]=y1, [2]=x2, [3]=y2, [4]=radius, [5]=stroke
    // TRIANGLE: params[0..5]=x1,y1,x2,y2,x3,y3, [6]=stroke
    // POLYGON: params[0..15]=x1,y1,...,x8,y8, [16]=vertex_count, [17]=stroke
    // SPRITE: params[0]=x, [1]=y, [2]=w, [3]=h, [4]=ox, [5]=oy (+ texture_id)
    // GLYPH: params[0]=x, [1]=y, [2]=w, [3]=h, [4]=packed(u0,v0), [5]=packed(u1,v1) (+ texture_id)
    // SET_UNIFORM_FLOAT: params[0]=value
    // SET_UNIFORM_VEC2: params[0]=x, [1]=y
    // SET_UNIFORM_VEC4: params[0]=x, [1]=y, [2]=z, [3]=w
    // SET_UNIFORM_INT: params[0]=value (as float, cast to int)
    float params[20];       // 80 bytes (expanded for polygon support)

    union {
        GLuint texture_id;  // Texture handle (SPRITE, GLYPH)
        GLuint shader_id;   // Shader handle (APPLY_SHADER, SET_UNIFORM_*)
    };
    uint32_t flash_color;   // For SPRITE: packed RGB additive flash (GLYPH uses params for UVs instead)
    // Total: 4 + 24 + 4 + 80 + 4 + 4 = 120 bytes
} DrawCommand;

// Verify DrawCommand is exactly 120 bytes (compile-time check)
#ifdef _MSC_VER
    static_assert(sizeof(DrawCommand) == 120, "DrawCommand must be 120 bytes");
#else
    _Static_assert(sizeof(DrawCommand) == 120, "DrawCommand must be 120 bytes");
#endif

// Layer
typedef struct {
    GLuint fbo;
    GLuint color_texture;
    GLuint stencil_rbo;     // Stencil renderbuffer for masking
    int width;
    int height;

    // Effect ping-pong buffers (created on first use)
    GLuint effect_fbo;
    GLuint effect_texture;
    bool textures_swapped;  // Which buffer is current result

    // Extra texture bindings for shaders (bound right before apply_shader draws)
    GLuint extra_texture;     // texture to bind to unit 1
    GLint extra_texture_loc;  // uniform location for the sampler
    bool has_extra_texture;

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

// ============================================================================
// PHYSICS & AUDIO GLOBALS
// Physics: Tag system, event buffers, world state
// Audio: miniaudio engine, sound pool
// ============================================================================

// Audio globals (declared early so Sound functions can use them)
static ma_engine audio_engine;
static bool audio_initialized = false;
static float sound_master_volume = 1.0f;
static float music_master_volume = 1.0f;
static float audio_master_pitch = 1.0f;
#ifdef __EMSCRIPTEN__
static bool audio_needs_unlock = true;  // Web requires user interaction to start audio
#endif

// Physics globals
static b2WorldId physics_world = {0};
static bool physics_initialized = false;
static bool physics_enabled = true;
static float pixels_per_meter = 64.0f;  // Default: 64 pixels = 1 meter

// Physics tag system
#define MAX_PHYSICS_TAGS 64
#define MAX_TAG_NAME 32

typedef struct {
    char name[MAX_TAG_NAME];
    uint64_t category_bit;    // Single bit identifying this tag (1, 2, 4, 8, ...)
    uint64_t collision_mask;  // Which tags this collides with (physical response)
    uint64_t sensor_mask;     // Which tags trigger sensor events
    uint64_t hit_mask;        // Which tags trigger hit events
} PhysicsTag;

static PhysicsTag physics_tags[MAX_PHYSICS_TAGS];
static int physics_tag_count = 0;

// Per-shape user data (stored in Box2D shape user data pointer)
typedef struct {
    int tag_index;
    int filter_group;  // Non-zero: shapes with same group skip collision
} ShapeUserData;

#define MAX_SHAPE_USER_DATA 4096
static ShapeUserData shape_user_data_pool[MAX_SHAPE_USER_DATA];
static int shape_user_data_count = 0;

// Find tag index by name, returns -1 if not found
static int physics_tag_find(const char* name) {
    for (int i = 0; i < physics_tag_count; i++) {
        if (strcmp(physics_tags[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Get tag by index (NULL if invalid)
static PhysicsTag* physics_tag_get(int index) {
    if (index < 0 || index >= physics_tag_count) return NULL;
    return &physics_tags[index];
}

// Get tag by name (NULL if not found)
static PhysicsTag* physics_tag_get_by_name(const char* name) {
    int index = physics_tag_find(name);
    if (index < 0) return NULL;
    return &physics_tags[index];
}

// Physics event buffers
// See also: Event processing at ~line 370, Lua query functions at ~line 4530
#define MAX_PHYSICS_EVENTS 256

// Contact begin event (two shapes started touching)
typedef struct {
    b2BodyId body_a;
    b2BodyId body_b;
    b2ShapeId shape_a;
    b2ShapeId shape_b;
    int tag_a;  // Tag index of shape_a
    int tag_b;  // Tag index of shape_b
    float point_x;      // Contact point (pixels)
    float point_y;
    float normal_x;     // Normal from A to B
    float normal_y;
} PhysicsContactBeginEvent;

// Contact end event (two shapes stopped touching)
typedef struct {
    b2BodyId body_a;
    b2BodyId body_b;
    b2ShapeId shape_a;
    b2ShapeId shape_b;
    int tag_a;
    int tag_b;
} PhysicsContactEndEvent;

// Hit event (two shapes collided with speed above threshold)
typedef struct {
    b2BodyId body_a;
    b2BodyId body_b;
    b2ShapeId shape_a;
    b2ShapeId shape_b;
    int tag_a;
    int tag_b;
    float point_x;      // Contact point (pixels)
    float point_y;
    float normal_x;     // Normal from A to B
    float normal_y;
    float approach_speed;  // Speed of approach (pixels/sec)
} PhysicsHitEvent;

// Sensor begin event (shape entered sensor)
typedef struct {
    b2BodyId sensor_body;
    b2BodyId visitor_body;
    b2ShapeId sensor_shape;
    b2ShapeId visitor_shape;
    int sensor_tag;
    int visitor_tag;
} PhysicsSensorBeginEvent;

// Sensor end event (shape left sensor)
typedef struct {
    b2BodyId sensor_body;
    b2BodyId visitor_body;
    b2ShapeId sensor_shape;
    b2ShapeId visitor_shape;
    int sensor_tag;
    int visitor_tag;
} PhysicsSensorEndEvent;

// Event buffers
static PhysicsContactBeginEvent contact_begin_events[MAX_PHYSICS_EVENTS];
static int contact_begin_count = 0;

static PhysicsContactEndEvent contact_end_events[MAX_PHYSICS_EVENTS];
static int contact_end_count = 0;

static PhysicsHitEvent hit_events[MAX_PHYSICS_EVENTS];
static int hit_count = 0;

static PhysicsSensorBeginEvent sensor_begin_events[MAX_PHYSICS_EVENTS];
static int sensor_begin_count = 0;

static PhysicsSensorEndEvent sensor_end_events[MAX_PHYSICS_EVENTS];
static int sensor_end_count = 0;

// Random (PCG32)
// PCG32 - Permuted Congruential Generator (32-bit output, 64-bit state)
// Fast, excellent statistical quality, small state for easy replay save/restore
// See also: Lua bindings at ~line 5220
typedef struct {
    uint64_t state;      // RNG state (all values are possible)
    uint64_t increment;  // Controls which RNG sequence (stream) is selected (must be odd)
    uint64_t seed;       // Original seed (for random_get_seed)
} PCG32;

// Global RNG instance
static PCG32 global_rng = {0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL, 0};

// Seed the PCG32 generator
static void pcg32_seed(PCG32* rng, uint64_t seed) {
    rng->seed = seed;
    rng->state = 0;
    rng->increment = (seed << 1) | 1;  // Increment must be odd
    // Advance state once to mix in the seed
    rng->state = rng->state * 6364136223846793005ULL + rng->increment;
    rng->state += seed;
    rng->state = rng->state * 6364136223846793005ULL + rng->increment;
}

// Generate next 32-bit random number
static uint32_t pcg32_next(PCG32* rng) {
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + rng->increment;
    // Calculate output function (XSH RR)
    uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = (uint32_t)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
}

// Clear all event buffers (call at start of each physics step)
static void physics_clear_events(void) {
    contact_begin_count = 0;
    contact_end_count = 0;
    hit_count = 0;
    sensor_begin_count = 0;
    sensor_end_count = 0;
}

// Get tag index from shape's user data (stored during shape creation)
static int physics_get_shape_tag(b2ShapeId shape_id) {
    if (!b2Shape_IsValid(shape_id)) return -1;
    ShapeUserData* ud = (ShapeUserData*)b2Shape_GetUserData(shape_id);
    if (!ud) return -1;
    return ud->tag_index;
}

// Custom filter callback: reject collisions between shapes with same non-zero filter group
static bool physics_custom_filter(b2ShapeId shapeIdA, b2ShapeId shapeIdB, void* context) {
    ShapeUserData* ud_a = (ShapeUserData*)b2Shape_GetUserData(shapeIdA);
    ShapeUserData* ud_b = (ShapeUserData*)b2Shape_GetUserData(shapeIdB);
    if (!ud_a || !ud_b) return true;
    if (ud_a->filter_group != 0 && ud_a->filter_group == ud_b->filter_group) return false;
    return true;
}

// Process physics events after b2World_Step
// Retrieves all events from Box2D and buffers them with tag info for Lua queries
static void physics_process_events(void) {
    if (!physics_initialized) return;

    // Get contact events
    b2ContactEvents contact_events = b2World_GetContactEvents(physics_world);

    // Process contact begin events
    for (int i = 0; i < contact_events.beginCount && contact_begin_count < MAX_PHYSICS_EVENTS; i++) {
        b2ContactBeginTouchEvent* e = &contact_events.beginEvents[i];
        if (!b2Shape_IsValid(e->shapeIdA) || !b2Shape_IsValid(e->shapeIdB)) continue;

        int tag_a = physics_get_shape_tag(e->shapeIdA);
        int tag_b = physics_get_shape_tag(e->shapeIdB);
        if (tag_a < 0 || tag_b < 0) continue;

        PhysicsContactBeginEvent* ev = &contact_begin_events[contact_begin_count++];
        ev->shape_a = e->shapeIdA;
        ev->shape_b = e->shapeIdB;
        ev->body_a = b2Shape_GetBody(e->shapeIdA);
        ev->body_b = b2Shape_GetBody(e->shapeIdB);
        ev->tag_a = tag_a;
        ev->tag_b = tag_b;

        // Get contact manifold for contact point and normal
        b2ContactData contact_data;
        int contact_count = b2Shape_GetContactData(e->shapeIdA, &contact_data, 1);
        if (contact_count > 0 && contact_data.manifold.pointCount > 0) {
            // Use first contact point (there can be up to 2 for polygon-polygon)
            ev->point_x = contact_data.manifold.points[0].point.x * pixels_per_meter;
            ev->point_y = contact_data.manifold.points[0].point.y * pixels_per_meter;
            ev->normal_x = contact_data.manifold.normal.x;
            ev->normal_y = contact_data.manifold.normal.y;
        } else {
            // Fallback: no contact data available
            ev->point_x = 0;
            ev->point_y = 0;
            ev->normal_x = 0;
            ev->normal_y = 0;
        }
    }

    // Process contact end events
    for (int i = 0; i < contact_events.endCount && contact_end_count < MAX_PHYSICS_EVENTS; i++) {
        b2ContactEndTouchEvent* e = &contact_events.endEvents[i];
        // Note: shapes may have been destroyed, but we still record the event
        int tag_a = b2Shape_IsValid(e->shapeIdA) ? physics_get_shape_tag(e->shapeIdA) : -1;
        int tag_b = b2Shape_IsValid(e->shapeIdB) ? physics_get_shape_tag(e->shapeIdB) : -1;

        PhysicsContactEndEvent* ev = &contact_end_events[contact_end_count++];
        ev->shape_a = e->shapeIdA;
        ev->shape_b = e->shapeIdB;
        ev->body_a = b2Shape_IsValid(e->shapeIdA) ? b2Shape_GetBody(e->shapeIdA) : (b2BodyId){0};
        ev->body_b = b2Shape_IsValid(e->shapeIdB) ? b2Shape_GetBody(e->shapeIdB) : (b2BodyId){0};
        ev->tag_a = tag_a;
        ev->tag_b = tag_b;
    }

    // Process hit events
    for (int i = 0; i < contact_events.hitCount && hit_count < MAX_PHYSICS_EVENTS; i++) {
        b2ContactHitEvent* e = &contact_events.hitEvents[i];
        if (!b2Shape_IsValid(e->shapeIdA) || !b2Shape_IsValid(e->shapeIdB)) continue;

        int tag_a = physics_get_shape_tag(e->shapeIdA);
        int tag_b = physics_get_shape_tag(e->shapeIdB);
        if (tag_a < 0 || tag_b < 0) continue;

        PhysicsHitEvent* ev = &hit_events[hit_count++];
        ev->shape_a = e->shapeIdA;
        ev->shape_b = e->shapeIdB;
        ev->body_a = b2Shape_GetBody(e->shapeIdA);
        ev->body_b = b2Shape_GetBody(e->shapeIdB);
        ev->tag_a = tag_a;
        ev->tag_b = tag_b;
        // Convert from meters to pixels
        ev->point_x = e->point.x * pixels_per_meter;
        ev->point_y = e->point.y * pixels_per_meter;
        ev->normal_x = e->normal.x;
        ev->normal_y = e->normal.y;
        ev->approach_speed = e->approachSpeed * pixels_per_meter;
    }

    // Get sensor events
    b2SensorEvents sensor_events = b2World_GetSensorEvents(physics_world);

    // Process sensor begin events
    for (int i = 0; i < sensor_events.beginCount && sensor_begin_count < MAX_PHYSICS_EVENTS; i++) {
        b2SensorBeginTouchEvent* e = &sensor_events.beginEvents[i];
        if (!b2Shape_IsValid(e->sensorShapeId) || !b2Shape_IsValid(e->visitorShapeId)) continue;

        int sensor_tag = physics_get_shape_tag(e->sensorShapeId);
        int visitor_tag = physics_get_shape_tag(e->visitorShapeId);
        if (sensor_tag < 0 || visitor_tag < 0) continue;

        PhysicsSensorBeginEvent* ev = &sensor_begin_events[sensor_begin_count++];
        ev->sensor_shape = e->sensorShapeId;
        ev->visitor_shape = e->visitorShapeId;
        ev->sensor_body = b2Shape_GetBody(e->sensorShapeId);
        ev->visitor_body = b2Shape_GetBody(e->visitorShapeId);
        ev->sensor_tag = sensor_tag;
        ev->visitor_tag = visitor_tag;
    }

    // Process sensor end events
    for (int i = 0; i < sensor_events.endCount && sensor_end_count < MAX_PHYSICS_EVENTS; i++) {
        b2SensorEndTouchEvent* e = &sensor_events.endEvents[i];
        // Note: shapes may have been destroyed
        int sensor_tag = b2Shape_IsValid(e->sensorShapeId) ? physics_get_shape_tag(e->sensorShapeId) : -1;
        int visitor_tag = b2Shape_IsValid(e->visitorShapeId) ? physics_get_shape_tag(e->visitorShapeId) : -1;

        PhysicsSensorEndEvent* ev = &sensor_end_events[sensor_end_count++];
        ev->sensor_shape = e->sensorShapeId;
        ev->visitor_shape = e->visitorShapeId;
        ev->sensor_body = b2Shape_IsValid(e->sensorShapeId) ? b2Shape_GetBody(e->sensorShapeId) : (b2BodyId){0};
        ev->visitor_body = b2Shape_IsValid(e->visitorShapeId) ? b2Shape_GetBody(e->visitorShapeId) : (b2BodyId){0};
        ev->sensor_tag = sensor_tag;
        ev->visitor_tag = visitor_tag;
    }
}

// ============================================================================
// RESOURCES: TEXTURE, FONT, AUDIO
// Loading, management, and playback of game assets
// ============================================================================

// Texture
typedef struct {
    GLuint id;
    int width;
    int height;
} Texture;

// Load a texture from file using stb_image (supports zip archive)
static Texture* texture_load(const char* path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(0);  // Don't flip - we handle Y in our coordinate system

    // Load file data from zip or disk
    size_t file_size;
    unsigned char* file_data = (unsigned char*)zip_read_file(path, &file_size);
    if (!file_data) {
        fprintf(stderr, "Failed to load texture: %s\n", path);
        return NULL;
    }

    // Decode image from memory (needed for width/height even in headless)
    unsigned char* data = stbi_load_from_memory(file_data, (int)file_size, &width, &height, &channels, 4);
    free(file_data);
    if (!data) {
        fprintf(stderr, "Failed to decode texture: %s\n", path);
        return NULL;
    }

    Texture* tex = (Texture*)malloc(sizeof(Texture));
    if (!tex) {
        stbi_image_free(data);
        return NULL;
    }

    tex->width = width;
    tex->height = height;

    if (headless_mode) {
        // Headless: keep dimensions but skip GL texture upload
        tex->id = 0;
        stbi_image_free(data);
        return tex;
    }

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

// Create a texture from raw RGBA pixel data (4 bytes per pixel)
static Texture* texture_create_from_rgba(int width, int height, const unsigned char* data) {
    Texture* tex = (Texture*)malloc(sizeof(Texture));
    if (!tex) return NULL;

    tex->width = width;
    tex->height = height;

    if (headless_mode) {
        tex->id = 0;
        return tex;
    }

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

static void texture_destroy(Texture* tex) {
    if (!tex) return;
    if (tex->id) glDeleteTextures(1, &tex->id);
    free(tex);
}

// Spritesheet - texture with frame grid for animations
typedef struct {
    Texture* texture;
    int frame_width;
    int frame_height;
    int padding;
    int frames_per_row;
    int total_frames;
} Spritesheet;

// Load a spritesheet from file with frame dimensions
static Spritesheet* spritesheet_load(const char* path, int frame_width, int frame_height, int padding) {
    Texture* tex = texture_load(path);
    if (!tex) return NULL;

    Spritesheet* sheet = (Spritesheet*)malloc(sizeof(Spritesheet));
    if (!sheet) {
        texture_destroy(tex);
        return NULL;
    }

    sheet->texture = tex;
    sheet->frame_width = frame_width;
    sheet->frame_height = frame_height;
    sheet->padding = padding;

    // Calculate frame layout (left-to-right, top-to-bottom)
    int cell_width = frame_width + padding;
    int cell_height = frame_height + padding;
    sheet->frames_per_row = tex->width / cell_width;
    int rows = tex->height / cell_height;
    sheet->total_frames = sheet->frames_per_row * rows;

    printf("Loaded spritesheet: %s (%dx%d frames, %d total)\n",
           path, sheet->frames_per_row, rows, sheet->total_frames);
    return sheet;
}

static void spritesheet_destroy(Spritesheet* sheet) {
    if (!sheet) return;
    texture_destroy(sheet->texture);
    free(sheet);
}

// Font - TTF font with baked glyph atlas
// Global filter mode (smooth = anti-aliased, rough = hard pixel edges)
// Affects shapes and fonts - must be declared before font code
enum {
    FILTER_SMOOTH = 0,
    FILTER_ROUGH,
};
static int filter_mode = FILTER_ROUGH;  // Default to pixel-perfect

#define MAX_FONT_NAME 64
#define FONT_ATLAS_SIZE 512
#define FONT_FIRST_CHAR 32
#define FONT_NUM_CHARS 96

typedef struct {
    float x0, y0, x1, y1;  // Bounding box in pixels (relative to baseline)
    float u0, v0, u1, v1;  // UV coordinates in atlas
    float advance;          // Horizontal advance
} GlyphInfo;

typedef struct {
    char name[MAX_FONT_NAME];
    GLuint atlas_texture;
    int atlas_width;
    int atlas_height;
    float size;             // Font size in pixels
    float ascent;           // Distance from baseline to top
    float descent;          // Distance from baseline to bottom (negative)
    float line_height;      // Recommended line spacing
    GlyphInfo glyphs[FONT_NUM_CHARS];  // ASCII 32-127
    int filter;             // Filter mode font was loaded with (FILTER_SMOOTH or FILTER_ROUGH)
} Font;

// Global FreeType library
static FT_Library ft_library = NULL;

#define MAX_FONTS 16
static Font* font_registry[MAX_FONTS];
static int font_count = 0;

// UTF-8 decoding helper - returns codepoint and advances pointer
static uint32_t utf8_decode(const char** str) {
    const unsigned char* s = (const unsigned char*)*str;
    uint32_t codepoint;
    int bytes;

    if (s[0] < 0x80) {
        codepoint = s[0];
        bytes = 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        codepoint = s[0] & 0x1F;
        bytes = 2;
    } else if ((s[0] & 0xF0) == 0xE0) {
        codepoint = s[0] & 0x0F;
        bytes = 3;
    } else if ((s[0] & 0xF8) == 0xF0) {
        codepoint = s[0] & 0x07;
        bytes = 4;
    } else {
        // Invalid UTF-8, skip byte
        *str += 1;
        return 0xFFFD;  // Replacement character
    }

    for (int i = 1; i < bytes; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *str += 1;
            return 0xFFFD;
        }
        codepoint = (codepoint << 6) | (s[i] & 0x3F);
    }

    *str += bytes;
    return codepoint;
}

// Load a font from TTF file using FreeType (supports zip archive)
// Uses global filter_mode: FILTER_ROUGH = 1-bit mono, FILTER_SMOOTH = 8-bit grayscale AA
static Font* font_load(const char* name, const char* path, float size) {
    // Check if font already exists
    for (int i = 0; i < font_count; i++) {
        if (strcmp(font_registry[i]->name, name) == 0) {
            fprintf(stderr, "Font '%s' already loaded\n", name);
            return font_registry[i];
        }
    }

    if (font_count >= MAX_FONTS) {
        fprintf(stderr, "Maximum number of fonts (%d) reached\n", MAX_FONTS);
        return NULL;
    }

    // Initialize FreeType if needed
    if (!ft_library) {
        if (FT_Init_FreeType(&ft_library)) {
            fprintf(stderr, "Failed to initialize FreeType\n");
            return NULL;
        }
    }

    // Load font file from zip or disk
    size_t font_data_size;
    unsigned char* font_data = (unsigned char*)zip_read_file(path, &font_data_size);
    if (!font_data) {
        fprintf(stderr, "Failed to load font file: %s\n", path);
        return NULL;
    }

    // Load font face from memory
    FT_Face face;
    if (FT_New_Memory_Face(ft_library, font_data, (FT_Long)font_data_size, 0, &face)) {
        fprintf(stderr, "Failed to load font: %s\n", path);
        free(font_data);
        return NULL;
    }

    // Set pixel size
    int pixel_size = (int)(size + 0.5f);
    FT_Set_Pixel_Sizes(face, 0, pixel_size);

    // Create font struct
    Font* font = (Font*)malloc(sizeof(Font));
    if (!font) {
        FT_Done_Face(face);
        free(font_data);
        return NULL;
    }
    memset(font, 0, sizeof(Font));
    strncpy(font->name, name, MAX_FONT_NAME - 1);
    font->size = size;
    font->filter = filter_mode;  // Store filter mode font was loaded with

    // Get font metrics (in 26.6 fixed point, convert to pixels)
    font->ascent = face->size->metrics.ascender / 64.0f;
    font->descent = face->size->metrics.descender / 64.0f;
    font->line_height = face->size->metrics.height / 64.0f;

    font->atlas_width = FONT_ATLAS_SIZE;
    font->atlas_height = FONT_ATLAS_SIZE;

    // Create atlas bitmap (RGBA)
    unsigned char* rgba_bitmap = (unsigned char*)calloc(FONT_ATLAS_SIZE * FONT_ATLAS_SIZE * 4, 1);
    if (!rgba_bitmap) {
        free(font);
        FT_Done_Face(face);
        free(font_data);
        return NULL;
    }

    // Determine FreeType load flags based on filter mode
    FT_Int32 load_flags = FT_LOAD_RENDER;
    if (filter_mode == FILTER_ROUGH) {
        load_flags |= FT_LOAD_TARGET_MONO;  // 1-bit monochrome
    }
    // FILTER_SMOOTH uses default grayscale rendering (8-bit)

    // Pack glyphs into atlas
    int pen_x = 1;  // Start with 1px padding
    int pen_y = 1;
    int row_height = 0;

    for (int i = 0; i < FONT_NUM_CHARS; i++) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, FONT_FIRST_CHAR + i);

        if (FT_Load_Glyph(face, glyph_index, load_flags)) {
            continue;  // Skip failed glyphs
        }

        FT_GlyphSlot slot = face->glyph;
        FT_Bitmap* bmp = &slot->bitmap;

        int glyph_w = bmp->width;
        int glyph_h = bmp->rows;

        // Check if we need to move to next row
        if (pen_x + glyph_w + 1 >= FONT_ATLAS_SIZE) {
            pen_x = 1;
            pen_y += row_height + 1;
            row_height = 0;
        }

        // Check if atlas is full
        if (pen_y + glyph_h + 1 >= FONT_ATLAS_SIZE) {
            fprintf(stderr, "Warning: Font atlas full, some glyphs skipped\n");
            break;
        }

        // Copy glyph bitmap to atlas
        for (int y = 0; y < glyph_h; y++) {
            for (int x = 0; x < glyph_w; x++) {
                unsigned char alpha;

                if (filter_mode == FILTER_ROUGH) {
                    // Monochrome: 1-bit packed format
                    int byte_idx = y * bmp->pitch + (x >> 3);
                    int bit_idx = 7 - (x & 7);
                    unsigned char pixel = (bmp->buffer[byte_idx] >> bit_idx) & 1;
                    alpha = pixel ? 255 : 0;
                } else {
                    // Grayscale: 8-bit per pixel
                    alpha = bmp->buffer[y * bmp->pitch + x];
                }

                int atlas_x = pen_x + x;
                int atlas_y = pen_y + y;
                int atlas_idx = (atlas_y * FONT_ATLAS_SIZE + atlas_x) * 4;

                rgba_bitmap[atlas_idx + 0] = 255;    // R = white
                rgba_bitmap[atlas_idx + 1] = 255;    // G = white
                rgba_bitmap[atlas_idx + 2] = 255;    // B = white
                rgba_bitmap[atlas_idx + 3] = alpha;  // A = coverage
            }
        }

        // Store glyph info
        GlyphInfo* g = &font->glyphs[i];
        g->x0 = (float)slot->bitmap_left;
        g->y0 = -(float)slot->bitmap_top;  // FreeType uses upward Y, we use downward
        g->x1 = g->x0 + glyph_w;
        g->y1 = g->y0 + glyph_h;
        g->u0 = (float)pen_x / FONT_ATLAS_SIZE;
        g->v0 = (float)pen_y / FONT_ATLAS_SIZE;
        g->u1 = (float)(pen_x + glyph_w) / FONT_ATLAS_SIZE;
        g->v1 = (float)(pen_y + glyph_h) / FONT_ATLAS_SIZE;
        g->advance = slot->advance.x / 64.0f;  // 26.6 to pixels

        // Advance pen
        pen_x += glyph_w + 1;
        if (glyph_h > row_height) row_height = glyph_h;
    }

    FT_Done_Face(face);
    free(font_data);  // Font data no longer needed after face processing

    if (headless_mode) {
        // Headless: glyph metrics are loaded, skip GL atlas upload
        font->atlas_texture = 0;
        free(rgba_bitmap);
    } else {
        // Create OpenGL texture from RGBA atlas
        // Use appropriate filtering based on mode
        GLint tex_filter = (filter_mode == FILTER_ROUGH) ? GL_NEAREST : GL_LINEAR;
        glGenTextures(1, &font->atlas_texture);
        glBindTexture(GL_TEXTURE_2D, font->atlas_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FONT_ATLAS_SIZE, FONT_ATLAS_SIZE, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba_bitmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        free(rgba_bitmap);
    }

    // Register font
    font_registry[font_count++] = font;
    printf("Loaded font: %s (%.1fpx, %s) atlas=%dx%d\n", name, size,
           filter_mode == FILTER_ROUGH ? "rough" : "smooth",
           FONT_ATLAS_SIZE, FONT_ATLAS_SIZE);
    return font;
}

static void font_unload(const char* name) {
    for (int i = 0; i < font_count; i++) {
        if (strcmp(font_registry[i]->name, name) == 0) {
            Font* font = font_registry[i];
            if (font->atlas_texture) glDeleteTextures(1, &font->atlas_texture);
            free(font);
            // Shift remaining fonts
            for (int j = i; j < font_count - 1; j++) {
                font_registry[j] = font_registry[j + 1];
            }
            font_count--;
            printf("Unloaded font: %s\n", name);
            return;
        }
    }
    fprintf(stderr, "Font not found: %s\n", name);
}

static Font* font_get(const char* name) {
    for (int i = 0; i < font_count; i++) {
        if (strcmp(font_registry[i]->name, name) == 0) {
            return font_registry[i];
        }
    }
    return NULL;
}

// Font metrics functions
static float font_get_height(const char* font_name) {
    Font* font = font_get(font_name);
    if (!font) return 0.0f;
    return font->line_height;
}

static float font_get_text_width(const char* font_name, const char* text) {
    Font* font = font_get(font_name);
    if (!font || !text) return 0.0f;

    float width = 0.0f;
    while (*text) {
        uint32_t codepoint = utf8_decode(&text);
        if (codepoint >= FONT_FIRST_CHAR && codepoint < FONT_FIRST_CHAR + FONT_NUM_CHARS) {
            width += font->glyphs[codepoint - FONT_FIRST_CHAR].advance;
        }
    }
    return width;
}

static float font_get_char_width(const char* font_name, uint32_t codepoint) {
    Font* font = font_get(font_name);
    if (!font) return 0.0f;
    if (codepoint >= FONT_FIRST_CHAR && codepoint < FONT_FIRST_CHAR + FONT_NUM_CHARS) {
        return font->glyphs[codepoint - FONT_FIRST_CHAR].advance;
    }
    return 0.0f;
}

// Sound - stores audio data for fire-and-forget playback (supports zip archive)
// Each play creates a new ma_sound instance that self-destructs when done
#define MAX_SOUND_PATH 256

typedef struct {
    char path[MAX_SOUND_PATH];  // For debug logging
    void* data;                 // Raw audio file data (WAV, OGG, etc.)
    size_t data_size;           // Size of audio data
} Sound;

static Sound* sound_load(const char* path) {
    Sound* sound = (Sound*)malloc(sizeof(Sound));
    if (!sound) return NULL;

    strncpy(sound->path, path, MAX_SOUND_PATH - 1);
    sound->path[MAX_SOUND_PATH - 1] = '\0';

    if (headless_mode) {
        // Headless: return valid pointer but skip audio data loading
        sound->data = NULL;
        sound->data_size = 0;
        return sound;
    }

    // Load audio data from zip or disk
    sound->data = zip_read_file(path, &sound->data_size);
    if (!sound->data) {
        fprintf(stderr, "Failed to load sound file: %s\n", path);
        free(sound);
        return NULL;
    }

    // Verify the file can be decoded by attempting to init a sound
    if (audio_initialized) {
        ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, 2, audio_engine.sampleRate);
        ma_decoder decoder;
        ma_result result = ma_decoder_init_memory(sound->data, sound->data_size, &decoder_config, &decoder);
        if (result != MA_SUCCESS) {
            fprintf(stderr, "Failed to decode sound: %s (error %d)\n", path, result);
            free(sound->data);
            free(sound);
            return NULL;
        }
        ma_decoder_uninit(&decoder);
    }

    printf("Loaded sound: %s (%zu bytes)\n", path, sound->data_size);
    return sound;
}

static void sound_destroy(Sound* sound) {
    if (sound) {
        if (sound->data) free(sound->data);
        free(sound);
    }
}

// Sound instance pool for fire-and-forget playback
// Cleaned up from main thread to avoid threading issues
#define MAX_PLAYING_SOUNDS 512

typedef struct {
    ma_sound sound;
    ma_decoder decoder;  // Decoder for memory-based sounds
    bool in_use;
    uint32_t generation; // Incremented each allocation, for stale handle detection
    float user_pitch;    // Per-instance pitch (before master pitch multiplier)
    float user_volume;   // Per-instance volume (before master volume multiplier)
} PlayingSound;

static PlayingSound playing_sounds[MAX_PLAYING_SOUNDS];
static bool playing_sounds_initialized = false;

// Handle encoding: pack slot index (9 bits, 0-511) + generation (23 bits) into int
static int sound_handle_encode(int slot, uint32_t generation) {
    return (int)(((generation & 0x7FFFFF) << 9) | (slot & 0x1FF));
}

static bool sound_handle_decode(int handle, int* out_slot) {
    int slot = handle & 0x1FF;
    uint32_t expected_gen = (uint32_t)((handle >> 9) & 0x7FFFFF);
    if (slot < 0 || slot >= MAX_PLAYING_SOUNDS) return false;
    if (!playing_sounds[slot].in_use) return false;
    if ((playing_sounds[slot].generation & 0x7FFFFF) != expected_gen) return false;
    *out_slot = slot;
    return true;
}

// Clean up finished sounds (call from main thread each frame)
static void sound_cleanup_finished(void) {
    if (!audio_initialized) return;

    for (int i = 0; i < MAX_PLAYING_SOUNDS; i++) {
        if (playing_sounds[i].in_use) {
            if (!ma_sound_is_playing(&playing_sounds[i].sound)) {
                ma_sound_uninit(&playing_sounds[i].sound);
                ma_decoder_uninit(&playing_sounds[i].decoder);
                playing_sounds[i].in_use = false;
            }
        }
    }
}

// Clean up all playing sounds (call on shutdown)
static void sound_cleanup_all(void) {
    for (int i = 0; i < MAX_PLAYING_SOUNDS; i++) {
        if (playing_sounds[i].in_use) {
            ma_sound_stop(&playing_sounds[i].sound);
            ma_sound_uninit(&playing_sounds[i].sound);
            ma_decoder_uninit(&playing_sounds[i].decoder);
            playing_sounds[i].in_use = false;
        }
    }
}

// Convert linear volume (0-1) to perceptual volume using power curve
static float linear_to_perceptual(float linear) {
    return linear * linear;
}

// Play a sound with volume and pitch, returns slot index or -1
static int sound_play(Sound* sound, float volume, float pitch) {
    if (!audio_initialized || !sound || headless_mode) return -1;

    // Find a free slot
    int slot = -1;
    for (int i = 0; i < MAX_PLAYING_SOUNDS; i++) {
        if (!playing_sounds[i].in_use) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        // No free slots - try to reclaim finished sounds
        sound_cleanup_finished();
        for (int i = 0; i < MAX_PLAYING_SOUNDS; i++) {
            if (!playing_sounds[i].in_use) {
                slot = i;
                break;
            }
        }
    }

    if (slot == -1) {
        fprintf(stderr, "No free sound slots available\n");
        return -1;
    }

    // Initialize decoder from memory
    ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, 2, audio_engine.sampleRate);
    ma_result result = ma_decoder_init_memory(sound->data, sound->data_size, &decoder_config, &playing_sounds[slot].decoder);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to decode sound: %s (error %d)\n", sound->path, result);
        return -1;
    }

    // Initialize sound from decoder
    result = ma_sound_init_from_data_source(&audio_engine, &playing_sounds[slot].decoder, 0, NULL, &playing_sounds[slot].sound);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to play sound: %s (error %d)\n", sound->path, result);
        ma_decoder_uninit(&playing_sounds[slot].decoder);
        return -1;
    }

    // Store user values for later modification
    playing_sounds[slot].user_pitch = pitch;
    playing_sounds[slot].user_volume = volume;

    // Apply volume: per-play volume * master volume (perceptual scaling)
    ma_sound_set_volume(&playing_sounds[slot].sound, linear_to_perceptual(volume * sound_master_volume));

    // Apply pitch: per-play pitch * master pitch
    ma_sound_set_pitch(&playing_sounds[slot].sound, pitch * audio_master_pitch);

    playing_sounds[slot].generation++;
    playing_sounds[slot].in_use = true;
    ma_sound_start(&playing_sounds[slot].sound);
    return slot;
}

// Set pitch of a playing sound by handle
static void sound_handle_set_pitch(int handle, float pitch) {
    int slot;
    if (!sound_handle_decode(handle, &slot)) return;
    playing_sounds[slot].user_pitch = pitch;
    ma_sound_set_pitch(&playing_sounds[slot].sound, pitch * audio_master_pitch);
}

// Set volume of a playing sound by handle
static void sound_handle_set_volume(int handle, float volume) {
    int slot;
    if (!sound_handle_decode(handle, &slot)) return;
    playing_sounds[slot].user_volume = volume;
    ma_sound_set_volume(&playing_sounds[slot].sound, linear_to_perceptual(volume * sound_master_volume));
}

// Stop a playing sound by handle
static void sound_handle_stop(int handle) {
    int slot;
    if (!sound_handle_decode(handle, &slot)) return;
    ma_sound_stop(&playing_sounds[slot].sound);
    ma_sound_uninit(&playing_sounds[slot].sound);
    ma_decoder_uninit(&playing_sounds[slot].decoder);
    playing_sounds[slot].in_use = false;
}

// Set looping on a playing sound by handle
static void sound_handle_set_looping(int handle, bool looping) {
    int slot;
    if (!sound_handle_decode(handle, &slot)) return;
    ma_sound_set_looping(&playing_sounds[slot].sound, looping);
}

// Music - streaming tracks with two channels for crossfade support (supports zip archive)
typedef struct {
    ma_sound sound;
    ma_decoder decoder;     // Decoder for memory-based music
    void* data;             // Raw audio file data
    size_t data_size;       // Size of audio data
    bool initialized;
} Music;

#define MUSIC_CHANNELS 2
typedef struct {
    Music* music;           // Currently playing music on this channel
    float volume;           // Per-channel volume multiplier (0-1)
} MusicChannel;

static MusicChannel music_channels[MUSIC_CHANNELS] = {{NULL, 1.0f}, {NULL, 1.0f}};

static Music* music_load(const char* path) {
    if (headless_mode) {
        // Headless: return valid dummy pointer, skip audio decoding
        Music* music = (Music*)malloc(sizeof(Music));
        if (!music) return NULL;
        memset(music, 0, sizeof(Music));
        return music;
    }
    if (!audio_initialized) return NULL;

    Music* music = (Music*)malloc(sizeof(Music));
    if (!music) return NULL;
    memset(music, 0, sizeof(Music));

    // Load audio data from zip or disk
    music->data = zip_read_file(path, &music->data_size);
    if (!music->data) {
        fprintf(stderr, "Failed to load music file: %s\n", path);
        free(music);
        return NULL;
    }

    // Initialize decoder from memory
    ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, 2, audio_engine.sampleRate);
    ma_result result = ma_decoder_init_memory(music->data, music->data_size, &decoder_config, &music->decoder);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to decode music: %s (error %d)\n", path, result);
        free(music->data);
        free(music);
        return NULL;
    }

    // Initialize sound from decoder (no streaming flag - decoder handles it)
    result = ma_sound_init_from_data_source(&audio_engine, &music->decoder, 0, NULL, &music->sound);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Failed to load music: %s (error %d)\n", path, result);
        ma_decoder_uninit(&music->decoder);
        free(music->data);
        free(music);
        return NULL;
    }

    music->initialized = true;
    printf("Loaded music: %s (%zu bytes)\n", path, music->data_size);
    return music;
}

static void music_destroy(Music* music) {
    if (!music) return;
    if (music->initialized) {
        ma_sound_uninit(&music->sound);
        ma_decoder_uninit(&music->decoder);
    }
    if (music->data) free(music->data);
    free(music);
}

static void music_play(Music* music, bool loop, int channel) {
    if (!audio_initialized || !music || !music->initialized || headless_mode) return;
    if (channel < 0 || channel >= MUSIC_CHANNELS) channel = 0;

    MusicChannel* ch = &music_channels[channel];

    // Stop current music on this channel if different
    if (ch->music && ch->music != music && ch->music->initialized) {
        ma_sound_stop(&ch->music->sound);
    }

    ch->music = music;
    ma_sound_set_looping(&music->sound, loop);
    ma_sound_set_volume(&music->sound, linear_to_perceptual(music_master_volume * ch->volume));
    ma_sound_seek_to_pcm_frame(&music->sound, 0);  // Restart from beginning
    ma_sound_start(&music->sound);
}

static void music_stop(int channel) {
    if (channel < 0) {
        // Stop all channels
        for (int i = 0; i < MUSIC_CHANNELS; i++) {
            if (music_channels[i].music && music_channels[i].music->initialized) {
                ma_sound_stop(&music_channels[i].music->sound);
            }
            music_channels[i].music = NULL;
        }
    } else if (channel < MUSIC_CHANNELS) {
        MusicChannel* ch = &music_channels[channel];
        if (ch->music && ch->music->initialized) {
            // Check if another channel is using the same Music
            bool in_use_elsewhere = false;
            for (int i = 0; i < MUSIC_CHANNELS; i++) {
                if (i != channel && music_channels[i].music == ch->music) {
                    in_use_elsewhere = true;
                    break;
                }
            }
            // Only stop the sound if no other channel needs it
            if (!in_use_elsewhere) {
                ma_sound_stop(&ch->music->sound);
            }
        }
        ch->music = NULL;
    }
}

static void music_set_volume(float volume, int channel) {
    if (channel < 0) {
        // Set master volume
        music_master_volume = volume;
        for (int i = 0; i < MUSIC_CHANNELS; i++) {
            if (music_channels[i].music && music_channels[i].music->initialized) {
                ma_sound_set_volume(&music_channels[i].music->sound,
                    linear_to_perceptual(music_master_volume * music_channels[i].volume));
            }
        }
    } else if (channel < MUSIC_CHANNELS) {
        // Set per-channel volume
        MusicChannel* ch = &music_channels[channel];
        ch->volume = volume;
        if (ch->music && ch->music->initialized) {
            ma_sound_set_volume(&ch->music->sound,
                linear_to_perceptual(music_master_volume * ch->volume));
        }
    }
}

static bool music_is_playing(int channel) {
    if (channel < 0 || channel >= MUSIC_CHANNELS) return false;
    MusicChannel* ch = &music_channels[channel];
    if (!ch->music || !ch->music->initialized) return false;
    return ma_sound_is_playing(&ch->music->sound);
}

static bool music_at_end(int channel) {
    if (channel < 0 || channel >= MUSIC_CHANNELS) return false;
    MusicChannel* ch = &music_channels[channel];
    if (!ch->music || !ch->music->initialized) return false;
    return ma_sound_at_end(&ch->music->sound);
}

static float music_get_position(int channel) {
    if (channel < 0 || channel >= MUSIC_CHANNELS) return 0.0f;
    MusicChannel* ch = &music_channels[channel];
    if (!ch->music || !ch->music->initialized) return 0.0f;
    float cursor;
    ma_sound_get_cursor_in_seconds(&ch->music->sound, &cursor);
    return cursor;
}

static float music_get_duration(int channel) {
    if (channel < 0 || channel >= MUSIC_CHANNELS) return 0.0f;
    MusicChannel* ch = &music_channels[channel];
    if (!ch->music || !ch->music->initialized) return 0.0f;
    float length;
    ma_sound_get_length_in_seconds(&ch->music->sound, &length);
    return length;
}

static float music_get_volume(int channel) {
    if (channel < 0 || channel >= MUSIC_CHANNELS) return 1.0f;
    return music_channels[channel].volume;
}

// Master pitch (slow-mo) - affects all currently playing audio
static void audio_set_master_pitch(float pitch) {
    audio_master_pitch = pitch;

    // Update all playing sounds
    for (int i = 0; i < MAX_PLAYING_SOUNDS; i++) {
        if (playing_sounds[i].in_use) {
            ma_sound_set_pitch(&playing_sounds[i].sound, playing_sounds[i].user_pitch * pitch);
        }
    }

    // Update music on all channels
    for (int i = 0; i < MUSIC_CHANNELS; i++) {
        if (music_channels[i].music && music_channels[i].music->initialized) {
            ma_sound_set_pitch(&music_channels[i].music->sound, pitch);
        }
    }
}

// Web audio context unlock (browsers require user interaction before audio plays)
#ifdef __EMSCRIPTEN__
static void audio_try_unlock(void) {
    if (audio_needs_unlock && audio_initialized) {
        ma_engine_start(&audio_engine);
        audio_needs_unlock = false;
        printf("Audio context unlocked\n");
    }
}
#endif

// ============================================================================
// LAYER SYSTEM
// FBO management, transform stack, command queue, drawing primitives
// ============================================================================

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

    if (headless_mode) {
        // Headless: no command buffer, no FBO — all draw calls become no-ops
        layer->commands = NULL;
        layer->command_count = 0;
        layer->command_capacity = 0;
        layer->current_blend = BLEND_ALPHA;
        return layer;
    }

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

    // Attach color texture to FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, layer->color_texture, 0);

    // Create stencil renderbuffer (using depth-stencil for wider compatibility)
    glGenRenderbuffers(1, &layer->stencil_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, layer->stencil_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, layer->stencil_rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Layer FBO not complete\n");
        glDeleteRenderbuffers(1, &layer->stencil_rbo);
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
    if (layer->stencil_rbo) glDeleteRenderbuffers(1, &layer->stencil_rbo);
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
        if (!headless_mode) {
            static bool warned = false;
            if (!warned) {
                fprintf(stderr, "Error: Command queue full (%d commands). Dropping draw calls.\n",
                        layer->command_capacity);
                warned = true;
            }
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

// Record a rectangle command (stroke=0 filled, stroke>0 outline)
static void layer_add_rectangle(Layer* layer, float x, float y, float w, float h, float stroke, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_RECTANGLE;
    cmd->color = color;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = w;
    cmd->params[3] = h;
    cmd->params[4] = stroke;
}

// Record a horizontal gradient rectangle command (left color1 to right color2)
static void layer_add_rectangle_gradient_h(Layer* layer, float x, float y, float w, float h, uint32_t color1, uint32_t color2) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_RECTANGLE_GRADIENT_H;
    cmd->color = color1;
    cmd->flash_color = color2;  // Store second color in flash_color (unused for gradients)
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = w;
    cmd->params[3] = h;
}

// Record a vertical gradient rectangle command (top color1 to bottom color2)
static void layer_add_rectangle_gradient_v(Layer* layer, float x, float y, float w, float h, uint32_t color1, uint32_t color2) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_RECTANGLE_GRADIENT_V;
    cmd->color = color1;
    cmd->flash_color = color2;  // Store second color in flash_color (unused for gradients)
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = w;
    cmd->params[3] = h;
}

// Record a circle command (stroke=0 filled, stroke>0 outline)
static void layer_add_circle(Layer* layer, float x, float y, float radius, float stroke, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_CIRCLE;
    cmd->color = color;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = radius;
    cmd->params[3] = stroke;
}

// Record a line/capsule command (stroke=0 filled capsule, stroke>0 outline)
static void layer_add_line(Layer* layer, float x1, float y1, float x2, float y2, float radius, float stroke, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_LINE;
    cmd->color = color;
    cmd->params[0] = x1;
    cmd->params[1] = y1;
    cmd->params[2] = x2;
    cmd->params[3] = y2;
    cmd->params[4] = radius;
    cmd->params[5] = stroke;
}

// Record a triangle command (stroke=0 filled, stroke>0 outline)
static void layer_add_triangle(Layer* layer, float x1, float y1, float x2, float y2, float x3, float y3, float stroke, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_TRIANGLE;
    cmd->color = color;
    cmd->params[0] = x1;
    cmd->params[1] = y1;
    cmd->params[2] = x2;
    cmd->params[3] = y2;
    cmd->params[4] = x3;
    cmd->params[5] = y3;
    cmd->params[6] = stroke;
}

// Record a polygon command (up to 8 vertices, stroke=0 filled, stroke>0 outline)
// vertices is array of [x1, y1, x2, y2, ...], vertex_count is number of vertices (not floats)
static void layer_add_polygon(Layer* layer, const float* vertices, int vertex_count, float stroke, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_POLYGON;
    cmd->color = color;
    // Clamp vertex count to 8 max
    if (vertex_count > 8) vertex_count = 8;
    if (vertex_count < 3) return; // Need at least 3 vertices
    // Copy vertex coordinates
    for (int i = 0; i < vertex_count * 2; i++) {
        cmd->params[i] = vertices[i];
    }
    cmd->params[16] = (float)vertex_count;
    cmd->params[17] = stroke;
}

// Record a rounded rectangle command (stroke=0 filled, stroke>0 outline)
static void layer_add_rounded_rectangle(Layer* layer, float x, float y, float w, float h, float radius, float stroke, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_ROUNDED_RECTANGLE;
    cmd->color = color;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = w;
    cmd->params[3] = h;
    cmd->params[4] = radius;
    cmd->params[5] = stroke;
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

// Pack two UV coordinates (0.0-1.0) into a float via bit reinterpretation (16 bits each)
static float pack_uv_pair(float u, float v) {
    uint16_t ui = (uint16_t)(u * 65535.0f);
    uint16_t vi = (uint16_t)(v * 65535.0f);
    uint32_t packed = ((uint32_t)ui) | ((uint32_t)vi << 16);
    float result;
    memcpy(&result, &packed, sizeof(float));
    return result;
}

// Unpack two UV coordinates from a float
static void unpack_uv_pair(float packed_float, float* u, float* v) {
    uint32_t packed;
    memcpy(&packed, &packed_float, sizeof(uint32_t));
    *u = (packed & 0xFFFF) / 65535.0f;
    *v = ((packed >> 16) & 0xFFFF) / 65535.0f;
}

// Forward declarations for transform stack (defined at ~line 1375)
static bool layer_push(Layer* layer, float x, float y, float r, float sx, float sy);
static void layer_pop(Layer* layer);

// Record a glyph command (top-left positioned, with custom UVs from font atlas)
// x, y is top-left corner of glyph; w, h is glyph size; UVs are atlas coordinates
static void layer_add_glyph(Layer* layer, GLuint atlas_texture, float x, float y, float w, float h,
                            float u0, float v0, float u1, float v1, uint32_t color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_GLYPH;
    cmd->color = color;
    cmd->texture_id = atlas_texture;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = w;
    cmd->params[3] = h;
    cmd->params[4] = pack_uv_pair(u0, v0);  // 16-bit precision per component
    cmd->params[5] = pack_uv_pair(u1, v1);
}

// Record a spritesheet frame command (centered at x, y, with custom UVs and flash support)
static void layer_add_spritesheet_frame(Layer* layer, GLuint texture_id, float x, float y, float w, float h,
                                        float u0, float v0, float u1, float v1,
                                        uint32_t color, uint32_t flash_color) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_SPRITESHEET_FRAME;
    cmd->color = color;
    cmd->flash_color = flash_color;
    cmd->texture_id = texture_id;
    cmd->params[0] = x;
    cmd->params[1] = y;
    cmd->params[2] = w;
    cmd->params[3] = h;
    cmd->params[4] = pack_uv_pair(u0, v0);
    cmd->params[5] = pack_uv_pair(u1, v1);
}

// Queue stencil mask command - subsequent draws write to stencil buffer only
static void layer_stencil_mask(Layer* layer) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_STENCIL_MASK;
}

// Queue stencil test command - subsequent draws only appear where stencil is set
static void layer_stencil_test(Layer* layer) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_STENCIL_TEST;
}

// Queue stencil test inverse command - subsequent draws only appear where stencil is NOT set
static void layer_stencil_test_inverse(Layer* layer) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_STENCIL_TEST_INVERSE;
}

// Queue stencil off command - disable stencil, return to normal drawing
static void layer_stencil_off(Layer* layer) {
    DrawCommand* cmd = layer_add_command(layer);
    if (!cmd) return;
    cmd->type = COMMAND_STENCIL_OFF;
}

// Draw a single glyph with transform (for per-character effects in YueScript)
// x, y is baseline position; r, sx, sy are rotation/scale applied at that point
static void layer_draw_glyph(Layer* layer, const char* font_name, uint32_t codepoint,
                             float x, float y, float r, float sx, float sy, uint32_t color) {
    Font* font = font_get(font_name);
    if (!font) return;
    if (codepoint < FONT_FIRST_CHAR || codepoint >= FONT_FIRST_CHAR + FONT_NUM_CHARS) return;

    GlyphInfo* g = &font->glyphs[codepoint - FONT_FIRST_CHAR];
    float glyph_w = g->x1 - g->x0;
    float glyph_h = g->y1 - g->y0;

    // Position: x is baseline x + glyph offset, y is baseline y + glyph offset
    float gx = x + g->x0;
    float gy = y + g->y0;

    // Apply transform at the glyph's center for rotation/scale
    float cx = gx + glyph_w * 0.5f;
    float cy = gy + glyph_h * 0.5f;

    layer_push(layer, cx, cy, r, sx, sy);
    layer_add_glyph(layer, font->atlas_texture,
                    gx - cx, gy - cy, glyph_w, glyph_h,
                    g->u0, g->v0, g->u1, g->v1, color);
    layer_pop(layer);
}

// Draw text string at position (simple API - no per-character effects)
// x, y is top-left of text block
static void layer_draw_text(Layer* layer, const char* text, const char* font_name,
                            float x, float y, uint32_t color) {
    Font* font = font_get(font_name);
    if (!font || !text) return;

    float cursor_x = x;
    float baseline_y = y + font->ascent;  // Convert top-left to baseline

    while (*text) {
        uint32_t codepoint = utf8_decode(&text);
        if (codepoint >= FONT_FIRST_CHAR && codepoint < FONT_FIRST_CHAR + FONT_NUM_CHARS) {
            GlyphInfo* g = &font->glyphs[codepoint - FONT_FIRST_CHAR];
            float glyph_w = g->x1 - g->x0;
            float glyph_h = g->y1 - g->y0;
            float gx = cursor_x + g->x0;
            float gy = baseline_y + g->y0;

            if (glyph_w > 0 && glyph_h > 0) {  // Skip space characters with no bitmap
                layer_add_glyph(layer, font->atlas_texture,
                                gx, gy, glyph_w, glyph_h,
                                g->u0, g->v0, g->u1, g->v1, color);
            }
            cursor_x += g->advance;
        }
    }
}

// Set the current blend mode for subsequent commands
static void layer_set_blend_mode(Layer* layer, uint8_t mode) {
    layer->current_blend = mode;
}

// ============================================================================
// BATCH RENDERING
// Vertex batching, matrix math, SDF shape rendering
// ============================================================================

// Batch rendering
#define MAX_BATCH_VERTICES 6000  // 1000 quads * 6 vertices
#define VERTEX_FLOATS 32         // x, y, u, v, r, g, b, a, type, shape[20], addR, addG, addB

// Shape types for uber-shader
// Filled vs outline is determined by stroke param (0 = filled, >0 = outline)
#define SHAPE_TYPE_RECT     0.0f
#define SHAPE_TYPE_CIRCLE   1.0f
#define SHAPE_TYPE_SPRITE   2.0f
#define SHAPE_TYPE_LINE     3.0f  // Line segment / capsule
#define SHAPE_TYPE_TRIANGLE 4.0f  // Triangle (3 vertices)
#define SHAPE_TYPE_POLYGON  5.0f  // Polygon (up to 8 vertices)
#define SHAPE_TYPE_ROUNDED_RECT 6.0f  // Rounded rectangle

static float batch_vertices[MAX_BATCH_VERTICES * VERTEX_FLOATS];
static int batch_vertex_count = 0;
static GLuint current_batch_texture = 0;  // Currently bound texture for batching
static int draw_calls = 0;  // Draw call counter (reset each render frame)

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

// Add a vertex to the batch (32 floats per vertex)
// shape is an array of 20 floats for shape parameters
static void batch_add_vertex(float x, float y, float u, float v,
                             float r, float g, float b, float a,
                             float type, const float* shape,
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
    // Copy 20 shape params
    for (int j = 0; j < 20; j++) {
        batch_vertices[i + 9 + j] = shape[j];
    }
    batch_vertices[i + 29] = addR; // additive color R (flash)
    batch_vertices[i + 30] = addG; // additive color G (flash)
    batch_vertices[i + 31] = addB; // additive color B (flash)
    batch_vertex_count++;
}

// Add a quad (two triangles, 6 vertices) for SDF shapes
// UVs go from (0,0) to (1,1) across the quad
// Shape params are the same for all vertices (20 floats)
// addR/G/B is additive color (flash effect)
static void batch_add_sdf_quad(float x0, float y0, float x1, float y1,
                               float x2, float y2, float x3, float y3,
                               float r, float g, float b, float a,
                               float type, const float* shape,
                               float addR, float addG, float addB) {
    // Quad corners with UVs:
    // 0(0,0)---1(1,0)
    // |         |
    // 3(0,1)---2(1,1)

    // Triangle 1: 0, 1, 2
    batch_add_vertex(x0, y0, 0.0f, 0.0f, r, g, b, a, type, shape, addR, addG, addB);
    batch_add_vertex(x1, y1, 1.0f, 0.0f, r, g, b, a, type, shape, addR, addG, addB);
    batch_add_vertex(x2, y2, 1.0f, 1.0f, r, g, b, a, type, shape, addR, addG, addB);
    // Triangle 2: 0, 2, 3
    batch_add_vertex(x0, y0, 0.0f, 0.0f, r, g, b, a, type, shape, addR, addG, addB);
    batch_add_vertex(x2, y2, 1.0f, 1.0f, r, g, b, a, type, shape, addR, addG, addB);
    batch_add_vertex(x3, y3, 0.0f, 1.0f, r, g, b, a, type, shape, addR, addG, addB);
}

// Add a quad with per-corner colors (for gradient rectangles)
// Colors: c0 = top-left, c1 = top-right, c2 = bottom-right, c3 = bottom-left
static void batch_add_sdf_quad_gradient(float x0, float y0, float x1, float y1,
                                        float x2, float y2, float x3, float y3,
                                        float r0, float g0, float b0, float a0,
                                        float r1, float g1, float b1, float a1,
                                        float r2, float g2, float b2, float a2,
                                        float r3, float g3, float b3, float a3,
                                        float type, const float* shape) {
    // Quad corners with UVs:
    // 0(0,0)---1(1,0)
    // |         |
    // 3(0,1)---2(1,1)

    // Triangle 1: 0, 1, 2
    batch_add_vertex(x0, y0, 0.0f, 0.0f, r0, g0, b0, a0, type, shape, 0.0f, 0.0f, 0.0f);
    batch_add_vertex(x1, y1, 1.0f, 0.0f, r1, g1, b1, a1, type, shape, 0.0f, 0.0f, 0.0f);
    batch_add_vertex(x2, y2, 1.0f, 1.0f, r2, g2, b2, a2, type, shape, 0.0f, 0.0f, 0.0f);
    // Triangle 2: 0, 2, 3
    batch_add_vertex(x0, y0, 0.0f, 0.0f, r0, g0, b0, a0, type, shape, 0.0f, 0.0f, 0.0f);
    batch_add_vertex(x2, y2, 1.0f, 1.0f, r2, g2, b2, a2, type, shape, 0.0f, 0.0f, 0.0f);
    batch_add_vertex(x3, y3, 0.0f, 1.0f, r3, g3, b3, a3, type, shape, 0.0f, 0.0f, 0.0f);
}

// Zero shape params for sprites/glyphs
static const float zero_shape[20] = {0};

// Add a quad with custom UV coordinates (for atlas-based glyph rendering)
static void batch_add_uv_quad(float x0, float y0, float x1, float y1,
                              float x2, float y2, float x3, float y3,
                              float u0, float v0, float u1, float v1,
                              float r, float g, float b, float a) {
    // Quad corners:
    // 0(u0,v0)---1(u1,v0)
    // |           |
    // 3(u0,v1)---2(u1,v1)

    // Triangle 1: 0, 1, 2
    batch_add_vertex(x0, y0, u0, v0, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, 0.0f, 0.0f, 0.0f);
    batch_add_vertex(x1, y1, u1, v0, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, 0.0f, 0.0f, 0.0f);
    batch_add_vertex(x2, y2, u1, v1, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, 0.0f, 0.0f, 0.0f);
    // Triangle 2: 0, 2, 3
    batch_add_vertex(x0, y0, u0, v0, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, 0.0f, 0.0f, 0.0f);
    batch_add_vertex(x2, y2, u1, v1, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, 0.0f, 0.0f, 0.0f);
    batch_add_vertex(x3, y3, u0, v1, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, 0.0f, 0.0f, 0.0f);
}

// Add a quad with custom UV coordinates and flash color (for spritesheet frames)
static void batch_add_uv_quad_flash(float x0, float y0, float x1, float y1,
                                    float x2, float y2, float x3, float y3,
                                    float u0, float v0, float u1, float v1,
                                    float r, float g, float b, float a,
                                    float addR, float addG, float addB) {
    // Triangle 1: 0, 1, 2
    batch_add_vertex(x0, y0, u0, v0, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, addR, addG, addB);
    batch_add_vertex(x1, y1, u1, v0, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, addR, addG, addB);
    batch_add_vertex(x2, y2, u1, v1, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, addR, addG, addB);
    // Triangle 2: 0, 2, 3
    batch_add_vertex(x0, y0, u0, v0, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, addR, addG, addB);
    batch_add_vertex(x2, y2, u1, v1, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, addR, addG, addB);
    batch_add_vertex(x3, y3, u0, v1, r, g, b, a, SHAPE_TYPE_SPRITE, zero_shape, addR, addG, addB);
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
#define MAX_LAYERS 32
static Layer* layer_registry[MAX_LAYERS];
static char* layer_names[MAX_LAYERS];
static int layer_count = 0;

// Texture registry (for cleanup on shutdown)
#define MAX_TEXTURES 256
static Texture* texture_registry[MAX_TEXTURES];
static int texture_count = 0;

// Effect shader registry (for cleanup on shutdown)
#define MAX_EFFECT_SHADERS 64
static GLuint effect_shader_registry[MAX_EFFECT_SHADERS];
static int effect_shader_count = 0;

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

// ============================================================================
// INPUT SYSTEM
// Keyboard, mouse, gamepad state; action bindings; chords, sequences, holds
// ============================================================================

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

// Input state - Global Hotkeys (Windows only)
#ifdef _WIN32
#define MAX_GLOBAL_HOTKEYS 16
static struct {
    int id;
    bool fired;       // Set when WM_HOTKEY received, cleared in input_post_update
    bool registered;
} global_hotkeys[MAX_GLOBAL_HOTKEYS];
static int global_hotkey_count = 0;
#endif

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
        // SDL scancodes: 1-9 are sequential, then 0 (keyboard layout order)
        if (c == '0') return SDL_SCANCODE_0;
        if (c >= '1' && c <= '9') return SDL_SCANCODE_1 + (c - '1');
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
    // Reset global hotkey fired flags
    #ifdef _WIN32
    for (int i = 0; i < global_hotkey_count; i++) {
        global_hotkeys[i].fired = false;
    }
    #endif
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
    float scale_x = (float)window_w / game_width;
    float scale_y = (float)window_h / game_height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1.0f) scale = 1.0f;

    // Calculate letterbox offset
    int scaled_w = (int)(game_width * scale);
    int scaled_h = (int)(game_height * scale);
    int offset_x = (window_w - scaled_w) / 2;
    int offset_y = (window_h - scaled_h) / 2;

    // Convert to game coordinates
    float gx = (float)(win_x - offset_x) / scale;
    float gy = (float)(win_y - offset_y) / scale;

    *game_x = gx;
    *game_y = gy;

    // Check if inside game area
    return (gx >= 0 && gx < game_width && gy >= 0 && gy < game_height);
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
} InputChord;

static InputChord chords[MAX_CHORDS];
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
static InputChord* chord_find(const char* name) {
    for (int i = 0; i < chord_count; i++) {
        if (strcmp(chords[i].name, name) == 0) {
            return &chords[i];
        }
    }
    return NULL;
}

// Check if chord is currently down (all actions held)
static bool chord_is_down(InputChord* chord) {
    if (!chord || chord->action_count == 0) return false;
    for (int i = 0; i < chord->action_count; i++) {
        if (!action_is_down(chord->action_names[i])) {
            return false;
        }
    }
    return true;
}

// Check if chord was just pressed (is down now, wasn't before)
static bool chord_is_pressed(InputChord* chord) {
    if (!chord) return false;
    bool down_now = chord_is_down(chord);
    return down_now && !chord->was_down;
}

// Check if chord was just released (was down, isn't now)
static bool chord_is_released(InputChord* chord) {
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
    InputChord* chord = chord_find(name);
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
    InputChord* chord = chord_find(name);
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
    InputChord* chord = chord_find(name);
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
    InputChord* chord = chord_find(name);
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
// Returns -1, 0, or 1 based on which actions are held
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

// ============================================================================
// RENDERING PIPELINE
// Layer draw queue, shader application, command processing
// ============================================================================

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
    cmd->shader_id = shader;
}

// Queue uniform setting commands (deferred - applied when processing commands)
static void layer_shader_set_float(Layer* layer, GLuint shader, const char* name, float value) {
    if (!shader || layer->command_count >= MAX_COMMAND_CAPACITY) return;

    GLint loc = glGetUniformLocation(shader, name);
    if (loc == -1) return;  // Uniform not found

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->type = COMMAND_SET_UNIFORM_FLOAT;
    cmd->shader_id = shader;
    cmd->uniform_location = (uint32_t)loc;
    cmd->params[0] = value;
}

static void layer_shader_set_vec2(Layer* layer, GLuint shader, const char* name, float x, float y) {
    if (!shader || layer->command_count >= MAX_COMMAND_CAPACITY) return;

    GLint loc = glGetUniformLocation(shader, name);
    if (loc == -1) return;

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->type = COMMAND_SET_UNIFORM_VEC2;
    cmd->shader_id = shader;
    cmd->uniform_location = (uint32_t)loc;
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
    cmd->shader_id = shader;
    cmd->uniform_location = (uint32_t)loc;
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
    cmd->shader_id = shader;
    cmd->uniform_location = (uint32_t)loc;
    cmd->params[0] = (float)value;  // Store as float, cast back when processing
}

// Set a texture uniform on a shader (binds to texture unit 1+)
// params[0] = texture unit index (1, 2, etc — 0 is reserved for the layer's own texture)
// texture_id = the GL texture handle
static void layer_shader_set_texture(Layer* layer, GLuint shader, const char* name, GLuint tex_id, int unit) {
    if (layer->command_count >= MAX_COMMAND_CAPACITY) return;
    GLint loc = glGetUniformLocation(shader, name);
    if (loc < 0) return;

    DrawCommand* cmd = &layer->commands[layer->command_count++];
    memset(cmd, 0, sizeof(DrawCommand));
    cmd->type = COMMAND_SET_UNIFORM_TEXTURE;
    cmd->shader_id = shader;
    cmd->uniform_location = (uint32_t)loc;
    cmd->texture_id = tex_id;
    cmd->params[0] = (float)unit;
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

    // Bind extra texture (e.g. distance field) to unit 1
    if (layer->has_extra_texture) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, layer->extra_texture);
        glUniform1i(layer->extra_texture_loc, 1);
        glActiveTexture(GL_TEXTURE0);
    }

    // Draw fullscreen quad
    glBindVertexArray(screen_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Clear extra texture state
    if (layer->has_extra_texture) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        layer->has_extra_texture = false;
    }

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
    draw_calls++;
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
    float stroke = cmd->params[4];  // 0 = filled, >0 = outline thickness

    // Add padding for anti-aliasing (1-2 pixels) + stroke width
    float pad = 2.0f + stroke;

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

    // Shape params: [quad_w, quad_h, half_w, half_h, stroke, ...]
    float shape[20] = {quad_w, quad_h, half_w, half_h, stroke};

    // Add SDF quad
    // Shader computes local_p = vUV * quad_size, center = quad_size * 0.5
    // No flash for shapes (additive = 0)
    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_RECT, shape,
                       0.0f, 0.0f, 0.0f);
}

// Process a horizontal gradient rectangle (left to right)
static void process_rectangle_gradient_h(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float w = cmd->params[2];
    float h = cmd->params[3];

    // Add padding for anti-aliasing
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

    // Unpack both colors
    float r1, g1, b1, a1, r2, g2, b2, a2;
    unpack_color(cmd->color, &r1, &g1, &b1, &a1);
    unpack_color(cmd->flash_color, &r2, &g2, &b2, &a2);

    // Shape params: [quad_w, quad_h, half_w, half_h, stroke=0, ...]
    float shape[20] = {quad_w, quad_h, half_w, half_h, 0.0f};

    // Horizontal gradient: left (0,3) = color1, right (1,2) = color2
    batch_add_sdf_quad_gradient(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                                r1, g1, b1, a1,  // top-left
                                r2, g2, b2, a2,  // top-right
                                r2, g2, b2, a2,  // bottom-right
                                r1, g1, b1, a1,  // bottom-left
                                SHAPE_TYPE_RECT, shape);
}

// Process a vertical gradient rectangle (top to bottom)
static void process_rectangle_gradient_v(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float w = cmd->params[2];
    float h = cmd->params[3];

    // Add padding for anti-aliasing
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

    // Unpack both colors
    float r1, g1, b1, a1, r2, g2, b2, a2;
    unpack_color(cmd->color, &r1, &g1, &b1, &a1);
    unpack_color(cmd->flash_color, &r2, &g2, &b2, &a2);

    // Shape params: [quad_w, quad_h, half_w, half_h, stroke=0, ...]
    float shape[20] = {quad_w, quad_h, half_w, half_h, 0.0f};

    // Vertical gradient: top (0,1) = color1, bottom (2,3) = color2
    batch_add_sdf_quad_gradient(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                                r1, g1, b1, a1,  // top-left
                                r1, g1, b1, a1,  // top-right
                                r2, g2, b2, a2,  // bottom-right
                                r2, g2, b2, a2,  // bottom-left
                                SHAPE_TYPE_RECT, shape);
}

// Process a circle command (SDF-based, UV-space approach)
// Same UV-space approach as rectangles for rotation support.
static void process_circle(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float radius = cmd->params[2];
    float stroke = cmd->params[3];  // 0 = filled, >0 = outline thickness

    // Add padding for anti-aliasing + stroke width
    float pad = 2.0f + stroke;

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

    // Shape params: [quad_size, quad_size, radius, stroke, ...]
    float shape[20] = {quad_size, quad_size, radius, stroke};

    // Add SDF quad
    // Shader computes local_p = vUV * quad_size, center = quad_size * 0.5
    // No flash for shapes (additive = 0)
    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_CIRCLE, shape,
                       0.0f, 0.0f, 0.0f);
}

// Process a line/capsule command (SDF-based)
// Line from (x1, y1) to (x2, y2) with radius (capsule thickness)
static void process_line(const DrawCommand* cmd) {
    float x1 = cmd->params[0];
    float y1 = cmd->params[1];
    float x2 = cmd->params[2];
    float y2 = cmd->params[3];
    float radius = cmd->params[4];
    float stroke = cmd->params[5];

    // Calculate line vector and length
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 0.001f) length = 0.001f; // Prevent division by zero

    // Line center
    float cx = (x1 + x2) * 0.5f;
    float cy = (y1 + y2) * 0.5f;

    // Add padding for anti-aliasing + stroke
    float pad = 2.0f + stroke;

    // Quad half-size: half-length along the line, radius perpendicular
    float half_len = length * 0.5f + radius + pad;
    float half_perp = radius + pad;

    // Unit direction of line
    float ux = dx / length;
    float uy = dy / length;

    // Perpendicular
    float px = -uy;
    float py = ux;

    // Compute 4 corners in local space (before layer transform)
    // These form an axis-aligned bounding box rotated to match line orientation
    float lx0 = cx - ux * half_len + px * half_perp;
    float ly0 = cy - uy * half_len + py * half_perp;
    float lx1 = cx + ux * half_len + px * half_perp;
    float ly1 = cy + uy * half_len + py * half_perp;
    float lx2 = cx + ux * half_len - px * half_perp;
    float ly2 = cy + uy * half_len - py * half_perp;
    float lx3 = cx - ux * half_len - px * half_perp;
    float ly3 = cy - uy * half_len - py * half_perp;

    // Transform to world coordinates
    float wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3;
    transform_point(cmd->transform, lx0, ly0, &wx0, &wy0);
    transform_point(cmd->transform, lx1, ly1, &wx1, &wy1);
    transform_point(cmd->transform, lx2, ly2, &wx2, &wy2);
    transform_point(cmd->transform, lx3, ly3, &wx3, &wy3);

    // Unpack color
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Quad dimensions for UV mapping
    float quad_w = half_len * 2.0f;
    float quad_h = half_perp * 2.0f;

    // Shape params: [quad_w, quad_h, half_length (without padding), radius, stroke]
    float shape[20] = {quad_w, quad_h, length * 0.5f, radius, stroke};

    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_LINE, shape,
                       0.0f, 0.0f, 0.0f);
}

// Process a triangle command (SDF-based)
static void process_triangle(const DrawCommand* cmd) {
    float x1 = cmd->params[0];
    float y1 = cmd->params[1];
    float x2 = cmd->params[2];
    float y2 = cmd->params[3];
    float x3 = cmd->params[4];
    float y3 = cmd->params[5];
    float stroke = cmd->params[6];

    // Find bounding box
    float min_x = fminf(fminf(x1, x2), x3);
    float max_x = fmaxf(fmaxf(x1, x2), x3);
    float min_y = fminf(fminf(y1, y2), y3);
    float max_y = fmaxf(fmaxf(y1, y2), y3);

    // Add padding for anti-aliasing + stroke
    float pad = 2.0f + stroke;

    // Quad corners with padding (local coordinates)
    float lx0 = min_x - pad, ly0 = min_y - pad;
    float lx1 = max_x + pad, ly1 = min_y - pad;
    float lx2 = max_x + pad, ly2 = max_y + pad;
    float lx3 = min_x - pad, ly3 = max_y + pad;

    // Transform to world coordinates
    float wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3;
    transform_point(cmd->transform, lx0, ly0, &wx0, &wy0);
    transform_point(cmd->transform, lx1, ly1, &wx1, &wy1);
    transform_point(cmd->transform, lx2, ly2, &wx2, &wy2);
    transform_point(cmd->transform, lx3, ly3, &wx3, &wy3);

    // Unpack color
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Quad dimensions for UV mapping
    float quad_w = (max_x - min_x) + 2.0f * pad;
    float quad_h = (max_y - min_y) + 2.0f * pad;

    // Triangle vertices relative to quad origin (min_x - pad, min_y - pad)
    float tx1 = x1 - (min_x - pad);
    float ty1 = y1 - (min_y - pad);
    float tx2 = x2 - (min_x - pad);
    float ty2 = y2 - (min_y - pad);
    float tx3 = x3 - (min_x - pad);
    float ty3 = y3 - (min_y - pad);

    // Shape params layout for shader:
    // shape0 = (quad_w, quad_h, stroke, _)
    // shape1 = (v0.x, v0.y, v1.x, v1.y)
    // shape2.xy = (v2.x, v2.y)
    float shape[20] = {quad_w, quad_h, stroke, 0, tx1, ty1, tx2, ty2, tx3, ty3};

    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_TRIANGLE, shape,
                       0.0f, 0.0f, 0.0f);
}

// Process a polygon command (SDF-based, up to 8 vertices)
static void process_polygon(const DrawCommand* cmd) {
    int vertex_count = (int)cmd->params[16];
    float stroke = cmd->params[17];

    if (vertex_count < 3) return;
    if (vertex_count > 8) vertex_count = 8;

    // Find bounding box
    float min_x = cmd->params[0], max_x = cmd->params[0];
    float min_y = cmd->params[1], max_y = cmd->params[1];
    for (int i = 1; i < vertex_count; i++) {
        float vx = cmd->params[i * 2];
        float vy = cmd->params[i * 2 + 1];
        if (vx < min_x) min_x = vx;
        if (vx > max_x) max_x = vx;
        if (vy < min_y) min_y = vy;
        if (vy > max_y) max_y = vy;
    }

    // Add padding for anti-aliasing + stroke
    float pad = 2.0f + stroke;

    // Quad corners with padding (local coordinates)
    float lx0 = min_x - pad, ly0 = min_y - pad;
    float lx1 = max_x + pad, ly1 = min_y - pad;
    float lx2 = max_x + pad, ly2 = max_y + pad;
    float lx3 = min_x - pad, ly3 = max_y + pad;

    // Transform to world coordinates
    float wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3;
    transform_point(cmd->transform, lx0, ly0, &wx0, &wy0);
    transform_point(cmd->transform, lx1, ly1, &wx1, &wy1);
    transform_point(cmd->transform, lx2, ly2, &wx2, &wy2);
    transform_point(cmd->transform, lx3, ly3, &wx3, &wy3);

    // Unpack color
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Quad dimensions for UV mapping
    float quad_w = (max_x - min_x) + 2.0f * pad;
    float quad_h = (max_y - min_y) + 2.0f * pad;

    // Shape params layout for shader:
    // shape0 = (n, stroke, quad_w, quad_h)
    // shape1 = (v0.x, v0.y, v1.x, v1.y)
    // shape2 = (v2.x, v2.y, v3.x, v3.y)
    // shape3 = (v4.x, v4.y, v5.x, v5.y)
    // shape4 = (v6.x, v6.y, v7.x, v7.y)
    float shape[20] = {0};
    shape[0] = (float)vertex_count;
    shape[1] = stroke;
    shape[2] = quad_w;
    shape[3] = quad_h;

    // Store vertices relative to quad origin (starting at shape[4])
    for (int i = 0; i < vertex_count; i++) {
        shape[4 + i * 2] = cmd->params[i * 2] - (min_x - pad);
        shape[4 + i * 2 + 1] = cmd->params[i * 2 + 1] - (min_y - pad);
    }

    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_POLYGON, shape,
                       0.0f, 0.0f, 0.0f);
}

// Process a rounded rectangle command (SDF-based)
static void process_rounded_rectangle(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float w = cmd->params[2];
    float h = cmd->params[3];
    float radius = cmd->params[4];
    float stroke = cmd->params[5];

    // Add padding for anti-aliasing (1-2 pixels) + stroke width
    float pad = 2.0f + stroke;

    // Quad size in local space (including padding)
    float quad_w = w + 2.0f * pad;
    float quad_h = h + 2.0f * pad;

    // Rectangle corners with padding (local coordinates)
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

    // Shape params: [quad_w, quad_h, half_w, half_h, radius, stroke, ...]
    float shape[20] = {quad_w, quad_h, half_w, half_h, radius, stroke};

    batch_add_sdf_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                       r, g, b, a,
                       SHAPE_TYPE_ROUNDED_RECT, shape,
                       0.0f, 0.0f, 0.0f);
}

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
                       SHAPE_TYPE_SPRITE, zero_shape,
                       addR, addG, addB);
}

// Process a glyph command (font atlas with custom UVs)
// Glyph is positioned at top-left (x, y)
static void process_glyph(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float w = cmd->params[2];
    float h = cmd->params[3];

    // Flush batch if texture changes
    if (current_batch_texture != cmd->texture_id && batch_vertex_count > 0) {
        batch_flush();
    }
    current_batch_texture = cmd->texture_id;

    // Glyph is positioned at top-left (x, y)
    float lx0 = x,     ly0 = y;
    float lx1 = x + w, ly1 = y;
    float lx2 = x + w, ly2 = y + h;
    float lx3 = x,     ly3 = y + h;

    // Transform to world coordinates
    float wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3;
    transform_point(cmd->transform, lx0, ly0, &wx0, &wy0);
    transform_point(cmd->transform, lx1, ly1, &wx1, &wy1);
    transform_point(cmd->transform, lx2, ly2, &wx2, &wy2);
    transform_point(cmd->transform, lx3, ly3, &wx3, &wy3);

    // Unpack color (used for tinting)
    float r, g, b, a;
    unpack_color(cmd->color, &r, &g, &b, &a);

    // Unpack UV coordinates from params[4] and params[5] (16-bit precision per component)
    float u0, v0, u1, v1;
    unpack_uv_pair(cmd->params[4], &u0, &v0);
    unpack_uv_pair(cmd->params[5], &u1, &v1);

    // Add glyph quad with custom UVs
    batch_add_uv_quad(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                      u0, v0, u1, v1,
                      r, g, b, a);
}

// Process a spritesheet frame command (centered, with custom UVs and flash support)
static void process_spritesheet_frame(const DrawCommand* cmd) {
    float x = cmd->params[0];
    float y = cmd->params[1];
    float w = cmd->params[2];
    float h = cmd->params[3];

    // Flush batch if texture changes
    if (current_batch_texture != cmd->texture_id && batch_vertex_count > 0) {
        batch_flush();
    }
    current_batch_texture = cmd->texture_id;

    // Frame is centered at (x, y)
    float half_w = w * 0.5f;
    float half_h = h * 0.5f;
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

    // Unpack flash color (additive)
    float addR, addG, addB, addA;
    unpack_color(cmd->flash_color, &addR, &addG, &addB, &addA);
    (void)addA;

    // Unpack UV coordinates
    float u0, v0, u1, v1;
    unpack_uv_pair(cmd->params[4], &u0, &v0);
    unpack_uv_pair(cmd->params[5], &u1, &v1);

    // Add quad with custom UVs and flash
    batch_add_uv_quad_flash(wx0, wy0, wx1, wy1, wx2, wy2, wx3, wy3,
                            u0, v0, u1, v1,
                            r, g, b, a, addR, addG, addB);
}

// Apply GL blend state based on blend mode
// Uses glBlendFuncSeparate to handle RGB and alpha channels differently:
// - RGB: standard blend (src * factor + dst * factor)
// - Alpha: preserves source alpha correctly when drawing to FBOs
//   Without this, alpha gets multiplied by itself (src.a * src.a) causing incorrect compositing
static void apply_blend_mode(uint8_t mode) {
    switch (mode) {
        case BLEND_ALPHA:
            // RGB: result = src.rgb * src.a + dst.rgb * (1 - src.a)
            // Alpha: result = src.a * 1 + dst.a * (1 - src.a) = src.a + dst.a * (1 - src.a)
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,  // RGB
                                GL_ONE, GL_ONE_MINUS_SRC_ALPHA);       // Alpha
            break;
        case BLEND_ADDITIVE:
            // RGB: result = src.rgb * src.a + dst.rgb (additive glow effect)
            // Alpha: result = src.a + dst.a (accumulate alpha)
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE,  // RGB
                                GL_ONE, GL_ONE);       // Alpha
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
            cmd->type == COMMAND_SET_UNIFORM_INT ||
            cmd->type == COMMAND_SET_UNIFORM_TEXTURE) {
            // Flush any pending draws before switching programs
            batch_flush();
            current_batch_texture = 0;

            glUseProgram(cmd->shader_id);
            switch (cmd->type) {
                case COMMAND_SET_UNIFORM_FLOAT:
                    glUniform1f((GLint)cmd->uniform_location, cmd->params[0]);
                    break;
                case COMMAND_SET_UNIFORM_VEC2:
                    glUniform2f((GLint)cmd->uniform_location, cmd->params[0], cmd->params[1]);
                    break;
                case COMMAND_SET_UNIFORM_VEC4:
                    glUniform4f((GLint)cmd->uniform_location, cmd->params[0], cmd->params[1], cmd->params[2], cmd->params[3]);
                    break;
                case COMMAND_SET_UNIFORM_INT:
                    glUniform1i((GLint)cmd->uniform_location, (int)cmd->params[0]);
                    break;
                case COMMAND_SET_UNIFORM_TEXTURE: {
                    // Store for binding during execute_apply_shader
                    layer->extra_texture = cmd->texture_id;
                    layer->extra_texture_loc = (GLint)cmd->uniform_location;
                    layer->has_extra_texture = true;
                    break;
                }
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
            execute_apply_shader(layer, cmd->shader_id);

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
            case COMMAND_LINE:
                // SDF shapes use no texture - flush if we were drawing sprites
                if (current_batch_texture != 0 && batch_vertex_count > 0) {
                    batch_flush();
                    current_batch_texture = 0;
                }
                process_line(cmd);
                break;
            case COMMAND_TRIANGLE:
                // SDF shapes use no texture - flush if we were drawing sprites
                if (current_batch_texture != 0 && batch_vertex_count > 0) {
                    batch_flush();
                    current_batch_texture = 0;
                }
                process_triangle(cmd);
                break;
            case COMMAND_POLYGON:
                // SDF shapes use no texture - flush if we were drawing sprites
                if (current_batch_texture != 0 && batch_vertex_count > 0) {
                    batch_flush();
                    current_batch_texture = 0;
                }
                process_polygon(cmd);
                break;
            case COMMAND_ROUNDED_RECTANGLE:
                // SDF shapes use no texture - flush if we were drawing sprites
                if (current_batch_texture != 0 && batch_vertex_count > 0) {
                    batch_flush();
                    current_batch_texture = 0;
                }
                process_rounded_rectangle(cmd);
                break;
            case COMMAND_RECTANGLE_GRADIENT_H:
                // SDF shapes use no texture - flush if we were drawing sprites
                if (current_batch_texture != 0 && batch_vertex_count > 0) {
                    batch_flush();
                    current_batch_texture = 0;
                }
                process_rectangle_gradient_h(cmd);
                break;
            case COMMAND_RECTANGLE_GRADIENT_V:
                // SDF shapes use no texture - flush if we were drawing sprites
                if (current_batch_texture != 0 && batch_vertex_count > 0) {
                    batch_flush();
                    current_batch_texture = 0;
                }
                process_rectangle_gradient_v(cmd);
                break;
            case COMMAND_SPRITE:
                process_sprite(cmd);
                break;
            case COMMAND_GLYPH:
                process_glyph(cmd);
                break;
            case COMMAND_SPRITESHEET_FRAME:
                process_spritesheet_frame(cmd);
                break;
            case COMMAND_STENCIL_MASK:
                // Flush pending draws before changing stencil state
                batch_flush();
                // Enable stencil, write 1 to stencil buffer, don't draw to color
                glEnable(GL_STENCIL_TEST);
                glStencilFunc(GL_ALWAYS, 1, 0xFF);
                glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                break;
            case COMMAND_STENCIL_TEST:
                // Flush pending draws before changing stencil state
                batch_flush();
                // Only draw where stencil == 1
                glStencilFunc(GL_EQUAL, 1, 0xFF);
                glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                break;
            case COMMAND_STENCIL_TEST_INVERSE:
                // Flush pending draws before changing stencil state
                batch_flush();
                // Only draw where stencil != 1 (inverse of stencil_test)
                glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
                glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                break;
            case COMMAND_STENCIL_OFF:
                // Flush pending draws before changing stencil state
                batch_flush();
                // Disable stencil, return to normal drawing
                glDisable(GL_STENCIL_TEST);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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

    Layer* layer = layer_create(game_width, game_height);
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

// Forward declarations for effect shaders (defined at ~line 6290 in SHADER SOURCES section)
static GLuint effect_shader_load_file(const char* path);
static GLuint effect_shader_load_string(const char* frag_source);
static void effect_shader_destroy(GLuint shader);
// Forward declarations for custom draw shader
static int l_set_draw_shader(lua_State* L);
static int l_get_draw_shader(lua_State* L);

// ============================================================================
// LUA BINDINGS: RENDERING
// Layer, texture, font, audio, shaders
// ============================================================================

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

// layer_rectangle(layer, x, y, w, h, color) -- filled rectangle
static int l_layer_rectangle(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 6);
    layer_add_rectangle(layer, x, y, w, h, 0.0f, color); // stroke=0 for filled
    return 0;
}

// layer_rectangle_line(layer, x, y, w, h, color, line_width?) -- rectangle outline
static int l_layer_rectangle_line(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 6);
    float line_width = (float)luaL_optnumber(L, 7, 1.0);
    layer_add_rectangle(layer, x, y, w, h, line_width, color);
    return 0;
}

// layer_rectangle_gradient_h(layer, x, y, w, h, color1, color2) -- horizontal gradient rectangle
static int l_layer_rectangle_gradient_h(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    uint32_t color1 = (uint32_t)luaL_checkinteger(L, 6);
    uint32_t color2 = (uint32_t)luaL_checkinteger(L, 7);
    layer_add_rectangle_gradient_h(layer, x, y, w, h, color1, color2);
    return 0;
}

// layer_rectangle_gradient_v(layer, x, y, w, h, color1, color2) -- vertical gradient rectangle
static int l_layer_rectangle_gradient_v(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    uint32_t color1 = (uint32_t)luaL_checkinteger(L, 6);
    uint32_t color2 = (uint32_t)luaL_checkinteger(L, 7);
    layer_add_rectangle_gradient_v(layer, x, y, w, h, color1, color2);
    return 0;
}

// layer_circle(layer, x, y, radius, color) -- filled circle
static int l_layer_circle(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float radius = (float)luaL_checknumber(L, 4);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 5);
    layer_add_circle(layer, x, y, radius, 0.0f, color); // stroke=0 for filled
    return 0;
}

// layer_circle_line(layer, x, y, radius, color, line_width?) -- circle outline
static int l_layer_circle_line(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float radius = (float)luaL_checknumber(L, 4);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 5);
    float line_width = (float)luaL_optnumber(L, 6, 1.0);
    layer_add_circle(layer, x, y, radius, line_width, color);
    return 0;
}

// layer_line(layer, x1, y1, x2, y2, width, color) -- line segment (capsule shape)
static int l_layer_line(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x1 = (float)luaL_checknumber(L, 2);
    float y1 = (float)luaL_checknumber(L, 3);
    float x2 = (float)luaL_checknumber(L, 4);
    float y2 = (float)luaL_checknumber(L, 5);
    float width = (float)luaL_checknumber(L, 6);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 7);
    // radius is half the width, stroke=0 for filled
    layer_add_line(layer, x1, y1, x2, y2, width * 0.5f, 0.0f, color);
    return 0;
}

// layer_capsule(layer, x1, y1, x2, y2, radius, color) -- filled capsule
static int l_layer_capsule(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x1 = (float)luaL_checknumber(L, 2);
    float y1 = (float)luaL_checknumber(L, 3);
    float x2 = (float)luaL_checknumber(L, 4);
    float y2 = (float)luaL_checknumber(L, 5);
    float radius = (float)luaL_checknumber(L, 6);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 7);
    layer_add_line(layer, x1, y1, x2, y2, radius, 0.0f, color);
    return 0;
}

// layer_capsule_line(layer, x1, y1, x2, y2, radius, color, line_width?) -- capsule outline
static int l_layer_capsule_line(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x1 = (float)luaL_checknumber(L, 2);
    float y1 = (float)luaL_checknumber(L, 3);
    float x2 = (float)luaL_checknumber(L, 4);
    float y2 = (float)luaL_checknumber(L, 5);
    float radius = (float)luaL_checknumber(L, 6);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 7);
    float line_width = (float)luaL_optnumber(L, 8, 1.0);
    layer_add_line(layer, x1, y1, x2, y2, radius, line_width, color);
    return 0;
}

// layer_triangle(layer, x1, y1, x2, y2, x3, y3, color) -- filled triangle
static int l_layer_triangle(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x1 = (float)luaL_checknumber(L, 2);
    float y1 = (float)luaL_checknumber(L, 3);
    float x2 = (float)luaL_checknumber(L, 4);
    float y2 = (float)luaL_checknumber(L, 5);
    float x3 = (float)luaL_checknumber(L, 6);
    float y3 = (float)luaL_checknumber(L, 7);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 8);
    layer_add_triangle(layer, x1, y1, x2, y2, x3, y3, 0.0f, color);
    return 0;
}

// layer_triangle_line(layer, x1, y1, x2, y2, x3, y3, color, line_width?) -- triangle outline
static int l_layer_triangle_line(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x1 = (float)luaL_checknumber(L, 2);
    float y1 = (float)luaL_checknumber(L, 3);
    float x2 = (float)luaL_checknumber(L, 4);
    float y2 = (float)luaL_checknumber(L, 5);
    float x3 = (float)luaL_checknumber(L, 6);
    float y3 = (float)luaL_checknumber(L, 7);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 8);
    float line_width = (float)luaL_optnumber(L, 9, 1.0);
    layer_add_triangle(layer, x1, y1, x2, y2, x3, y3, line_width, color);
    return 0;
}

// layer_polygon(layer, vertices_table, color) -- filled polygon (up to 8 vertices)
// vertices_table = {x1, y1, x2, y2, x3, y3, ...}
static int l_layer_polygon(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 3);

    // Read vertices from table
    int table_len = (int)lua_rawlen(L, 2);
    int vertex_count = table_len / 2;
    if (vertex_count < 3) {
        return luaL_error(L, "Polygon requires at least 3 vertices");
    }
    if (vertex_count > 8) vertex_count = 8;

    float vertices[16];
    for (int i = 0; i < vertex_count * 2; i++) {
        lua_rawgeti(L, 2, i + 1);
        vertices[i] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    layer_add_polygon(layer, vertices, vertex_count, 0.0f, color);
    return 0;
}

// layer_polygon_line(layer, vertices_table, color, line_width?) -- polygon outline
static int l_layer_polygon_line(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 3);
    float line_width = (float)luaL_optnumber(L, 4, 1.0);

    // Read vertices from table
    int table_len = (int)lua_rawlen(L, 2);
    int vertex_count = table_len / 2;
    if (vertex_count < 3) {
        return luaL_error(L, "Polygon requires at least 3 vertices");
    }
    if (vertex_count > 8) vertex_count = 8;

    float vertices[16];
    for (int i = 0; i < vertex_count * 2; i++) {
        lua_rawgeti(L, 2, i + 1);
        vertices[i] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    layer_add_polygon(layer, vertices, vertex_count, line_width, color);
    return 0;
}

// layer_rounded_rectangle(layer, x, y, w, h, radius, color) -- filled rounded rectangle
static int l_layer_rounded_rectangle(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    float radius = (float)luaL_checknumber(L, 6);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 7);
    layer_add_rounded_rectangle(layer, x, y, w, h, radius, 0.0f, color);
    return 0;
}

// layer_rounded_rectangle_line(layer, x, y, w, h, radius, color, line_width?) -- rounded rectangle outline
static int l_layer_rounded_rectangle_line(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4);
    float h = (float)luaL_checknumber(L, 5);
    float radius = (float)luaL_checknumber(L, 6);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 7);
    float line_width = (float)luaL_optnumber(L, 8, 1.0);
    layer_add_rounded_rectangle(layer, x, y, w, h, radius, line_width, color);
    return 0;
}

static int l_color_rgba(lua_State* L) {
    int r = (int)luaL_checkinteger(L, 1);
    int g = (int)luaL_checkinteger(L, 2);
    int b = (int)luaL_checkinteger(L, 3);
    int a = (int)luaL_optinteger(L, 4, 255);
    uint32_t color = ((r & 0xFF) << 24) | ((g & 0xFF) << 16) | ((b & 0xFF) << 8) | (a & 0xFF);
    lua_pushinteger(L, color);
    return 1;
}

static int l_set_filter_mode(lua_State* L) {
    const char* mode = luaL_checkstring(L, 1);
    if (strcmp(mode, "smooth") == 0) {
        filter_mode = FILTER_SMOOTH;
    } else if (strcmp(mode, "rough") == 0) {
        filter_mode = FILTER_ROUGH;
    } else {
        return luaL_error(L, "Invalid filter mode: %s (use 'smooth' or 'rough')", mode);
    }
    return 0;
}

static int l_get_filter_mode(lua_State* L) {
    lua_pushstring(L, filter_mode == FILTER_ROUGH ? "rough" : "smooth");
    return 1;
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
    // Register for cleanup on shutdown
    if (texture_count < MAX_TEXTURES) {
        texture_registry[texture_count++] = tex;
    }
    lua_pushlightuserdata(L, tex);
    return 1;
}

// texture_create(width, height, pixel_data_string) -> texture userdata
// pixel_data_string is a binary string of width*height*4 bytes (RGBA)
static int l_texture_create(lua_State* L) {
    int width = (int)luaL_checkinteger(L, 1);
    int height = (int)luaL_checkinteger(L, 2);
    size_t data_len;
    const char* data = luaL_checklstring(L, 3, &data_len);

    if ((int)data_len < width * height * 4) {
        return luaL_error(L, "Pixel data too short: expected %d bytes, got %d", width * height * 4, (int)data_len);
    }

    Texture* tex = texture_create_from_rgba(width, height, (const unsigned char*)data);
    if (!tex) {
        return luaL_error(L, "Failed to create texture");
    }

    // Register for cleanup on shutdown (matches l_texture_load behavior).
    // Returning lightuserdata pointing to the malloc'd Texture makes the
    // result safe to pass to texture_unload, which calls free() on it.
    if (texture_count < MAX_TEXTURES) {
        texture_registry[texture_count++] = tex;
    }
    lua_pushlightuserdata(L, tex);
    return 1;
}

static int l_texture_unload(lua_State* L) {
    Texture* tex = (Texture*)lua_touserdata(L, 1);
    if (!tex) return 0;
    // Remove from registry
    for (int i = 0; i < texture_count; i++) {
        if (texture_registry[i] == tex) {
            texture_registry[i] = texture_registry[--texture_count];
            break;
        }
    }
    texture_destroy(tex);
    return 0;
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

// Spritesheet Lua bindings
static int l_spritesheet_load(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int frame_width = (int)luaL_checkinteger(L, 2);
    int frame_height = (int)luaL_checkinteger(L, 3);
    int padding = (int)luaL_optinteger(L, 4, 0);
    Spritesheet* sheet = spritesheet_load(path, frame_width, frame_height, padding);
    if (!sheet) {
        return luaL_error(L, "Failed to load spritesheet: %s", path);
    }
    lua_pushlightuserdata(L, sheet);
    return 1;
}

static int l_spritesheet_get_frame_width(lua_State* L) {
    Spritesheet* sheet = (Spritesheet*)lua_touserdata(L, 1);
    lua_pushinteger(L, sheet->frame_width);
    return 1;
}

static int l_spritesheet_get_frame_height(lua_State* L) {
    Spritesheet* sheet = (Spritesheet*)lua_touserdata(L, 1);
    lua_pushinteger(L, sheet->frame_height);
    return 1;
}

static int l_spritesheet_get_total_frames(lua_State* L) {
    Spritesheet* sheet = (Spritesheet*)lua_touserdata(L, 1);
    lua_pushinteger(L, sheet->total_frames);
    return 1;
}

// Draw a spritesheet frame (1-based frame index in Lua)
static int l_layer_draw_spritesheet_frame(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    Spritesheet* sheet = (Spritesheet*)lua_touserdata(L, 2);
    int frame = (int)luaL_checkinteger(L, 3) - 1;  // Convert to 0-based
    float x = (float)luaL_checknumber(L, 4);
    float y = (float)luaL_checknumber(L, 5);
    uint32_t color = (uint32_t)luaL_optinteger(L, 6, 0xFFFFFFFF);
    uint32_t flash = (uint32_t)luaL_optinteger(L, 7, 0x00000000);

    // Clamp frame to valid range
    if (frame < 0) frame = 0;
    if (frame >= sheet->total_frames) frame = sheet->total_frames - 1;

    // Calculate frame position in grid (left-to-right, top-to-bottom)
    int col = frame % sheet->frames_per_row;
    int row = frame / sheet->frames_per_row;

    // Calculate UV coordinates
    int cell_width = sheet->frame_width + sheet->padding;
    int cell_height = sheet->frame_height + sheet->padding;
    float tex_width = (float)sheet->texture->width;
    float tex_height = (float)sheet->texture->height;

    float u0 = (col * cell_width) / tex_width;
    float v0 = (row * cell_height) / tex_height;
    float u1 = (col * cell_width + sheet->frame_width) / tex_width;
    float v1 = (row * cell_height + sheet->frame_height) / tex_height;

    layer_add_spritesheet_frame(layer, sheet->texture->id, x, y,
                                (float)sheet->frame_width, (float)sheet->frame_height,
                                u0, v0, u1, v1, color, flash);
    return 0;
}

// Font Lua bindings
static int l_font_load(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* path = luaL_checkstring(L, 2);
    float size = (float)luaL_checknumber(L, 3);
    Font* font = font_load(name, path, size);
    if (!font) {
        return luaL_error(L, "Failed to load font: %s", path);
    }
    return 0;
}

static int l_font_unload(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    font_unload(name);
    return 0;
}

static int l_font_get_height(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float height = font_get_height(name);
    lua_pushnumber(L, height);
    return 1;
}

static int l_font_get_text_width(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);
    float width = font_get_text_width(name, text);
    lua_pushnumber(L, width);
    return 1;
}

static int l_font_get_char_width(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    uint32_t codepoint = (uint32_t)luaL_checkinteger(L, 2);
    float width = font_get_char_width(name, codepoint);
    lua_pushnumber(L, width);
    return 1;
}

static int l_font_get_glyph_metrics(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    uint32_t codepoint = (uint32_t)luaL_checkinteger(L, 2);
    Font* font = font_get(name);
    if (!font) {
        return luaL_error(L, "Font not found: %s", name);
    }
    if (codepoint < FONT_FIRST_CHAR || codepoint >= FONT_FIRST_CHAR + FONT_NUM_CHARS) {
        return luaL_error(L, "Codepoint %d out of range (ASCII 32-127)", codepoint);
    }
    GlyphInfo* g = &font->glyphs[codepoint - FONT_FIRST_CHAR];
    lua_newtable(L);
    lua_pushnumber(L, g->x1 - g->x0);
    lua_setfield(L, -2, "width");
    lua_pushnumber(L, g->y1 - g->y0);
    lua_setfield(L, -2, "height");
    lua_pushnumber(L, g->advance);
    lua_setfield(L, -2, "advance");
    lua_pushnumber(L, g->x0);
    lua_setfield(L, -2, "bearingX");
    lua_pushnumber(L, -g->y0);  // Negate because y0 is offset from baseline going down
    lua_setfield(L, -2, "bearingY");
    return 1;
}

static int l_layer_draw_text(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    const char* text = luaL_checkstring(L, 2);
    const char* font_name = luaL_checkstring(L, 3);
    float x = (float)luaL_checknumber(L, 4);
    float y = (float)luaL_checknumber(L, 5);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 6);
    layer_draw_text(layer, text, font_name, x, y, color);
    return 0;
}

static int l_layer_draw_glyph(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    uint32_t codepoint = (uint32_t)luaL_checkinteger(L, 2);
    const char* font_name = luaL_checkstring(L, 3);
    float x = (float)luaL_checknumber(L, 4);
    float y = (float)luaL_checknumber(L, 5);
    float r = (float)luaL_optnumber(L, 6, 0.0);
    float sx = (float)luaL_optnumber(L, 7, 1.0);
    float sy = (float)luaL_optnumber(L, 8, 1.0);
    uint32_t color = (uint32_t)luaL_checkinteger(L, 9);
    layer_draw_glyph(layer, font_name, codepoint, x, y, r, sx, sy, color);
    return 0;
}

// Audio Lua bindings
static int l_sound_load(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    Sound* sound = sound_load(path);
    if (!sound) {
        return luaL_error(L, "Failed to load sound: %s", path);
    }
    lua_pushlightuserdata(L, sound);
    return 1;
}

static int l_sound_play(lua_State* L) {
    Sound* sound = (Sound*)lua_touserdata(L, 1);
    float volume = (float)luaL_optnumber(L, 2, 1.0);
    float pitch = (float)luaL_optnumber(L, 3, 1.0);
    sound_play(sound, volume, pitch);
    return 0;
}

static int l_sound_play_handle(lua_State* L) {
    Sound* sound = (Sound*)lua_touserdata(L, 1);
    float volume = (float)luaL_optnumber(L, 2, 1.0);
    float pitch = (float)luaL_optnumber(L, 3, 1.0);
    int slot = sound_play(sound, volume, pitch);
    if (slot == -1) {
        lua_pushinteger(L, -1);
    } else {
        lua_pushinteger(L, sound_handle_encode(slot, playing_sounds[slot].generation));
    }
    return 1;
}

static int l_sound_handle_set_pitch(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    float pitch = (float)luaL_checknumber(L, 2);
    sound_handle_set_pitch(handle, pitch);
    return 0;
}

static int l_sound_handle_set_volume(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    float volume = (float)luaL_checknumber(L, 2);
    sound_handle_set_volume(handle, volume);
    return 0;
}

static int l_sound_handle_stop(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    sound_handle_stop(handle);
    return 0;
}

static int l_sound_handle_set_looping(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    bool looping = lua_toboolean(L, 2);
    sound_handle_set_looping(handle, looping);
    return 0;
}

static int l_sound_set_volume(lua_State* L) {
    sound_master_volume = (float)luaL_checknumber(L, 1);
    return 0;
}

static int l_music_load(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    Music* music = music_load(path);
    if (!music) {
        return luaL_error(L, "Failed to load music: %s", path);
    }
    lua_pushlightuserdata(L, music);
    return 1;
}

static int l_music_play(lua_State* L) {
    Music* music = (Music*)lua_touserdata(L, 1);
    bool loop = lua_toboolean(L, 2);
    int channel = (int)luaL_optinteger(L, 3, 0);
    music_play(music, loop, channel);
    return 0;
}

static int l_music_stop(lua_State* L) {
    int channel = (int)luaL_optinteger(L, 1, -1);  // -1 = stop all
    music_stop(channel);
    return 0;
}

static int l_music_set_volume(lua_State* L) {
    float volume = (float)luaL_checknumber(L, 1);
    int channel = (int)luaL_optinteger(L, 2, -1);  // -1 = master volume
    music_set_volume(volume, channel);
    return 0;
}

static int l_music_is_playing(lua_State* L) {
    int channel = (int)luaL_optinteger(L, 1, 0);
    lua_pushboolean(L, music_is_playing(channel));
    return 1;
}

static int l_music_at_end(lua_State* L) {
    int channel = (int)luaL_optinteger(L, 1, 0);
    lua_pushboolean(L, music_at_end(channel));
    return 1;
}

static int l_music_get_position(lua_State* L) {
    int channel = (int)luaL_optinteger(L, 1, 0);
    lua_pushnumber(L, music_get_position(channel));
    return 1;
}

static int l_music_get_duration(lua_State* L) {
    int channel = (int)luaL_optinteger(L, 1, 0);
    lua_pushnumber(L, music_get_duration(channel));
    return 1;
}

static int l_music_get_volume(lua_State* L) {
    int channel = (int)luaL_optinteger(L, 1, 0);
    lua_pushnumber(L, music_get_volume(channel));
    return 1;
}

static int l_audio_set_master_pitch(lua_State* L) {
    float pitch = (float)luaL_checknumber(L, 1);
    audio_set_master_pitch(pitch);
    return 0;
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

// Stencil Lua bindings
static int l_layer_stencil_mask(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    layer_stencil_mask(layer);
    return 0;
}

static int l_layer_stencil_test(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    layer_stencil_test(layer);
    return 0;
}

static int l_layer_stencil_test_inverse(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    layer_stencil_test_inverse(layer);
    return 0;
}

static int l_layer_stencil_off(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    layer_stencil_off(layer);
    return 0;
}

// Effect shader Lua bindings
static int l_shader_load_file(lua_State* L) {
    if (headless_mode) {
        // Headless: return dummy shader ID (1) so Lua code doesn't get nil
        lua_pushinteger(L, 1);
        return 1;
    }
    const char* path = luaL_checkstring(L, 1);
    GLuint shader = effect_shader_load_file(path);
    if (!shader) {
        return luaL_error(L, "Failed to load effect shader: %s", path);
    }
    // Register for cleanup on shutdown
    if (effect_shader_count < MAX_EFFECT_SHADERS) {
        effect_shader_registry[effect_shader_count++] = shader;
    }
    lua_pushinteger(L, (lua_Integer)shader);
    return 1;
}

static int l_shader_load_string(lua_State* L) {
    if (headless_mode) {
        lua_pushinteger(L, 1);
        return 1;
    }
    const char* source = luaL_checkstring(L, 1);
    GLuint shader = effect_shader_load_string(source);
    if (!shader) {
        return luaL_error(L, "Failed to compile effect shader from string");
    }
    // Register for cleanup on shutdown
    if (effect_shader_count < MAX_EFFECT_SHADERS) {
        effect_shader_registry[effect_shader_count++] = shader;
    }
    lua_pushinteger(L, (lua_Integer)shader);
    return 1;
}

static int l_shader_destroy(lua_State* L) {
    GLuint shader = (GLuint)luaL_checkinteger(L, 1);
    // Remove from registry
    for (int i = 0; i < effect_shader_count; i++) {
        if (effect_shader_registry[i] == shader) {
            effect_shader_registry[i] = effect_shader_registry[--effect_shader_count];
            break;
        }
    }
    effect_shader_destroy(shader);
    return 0;
}

// Immediate shader uniform setters (applied now, for use with layer_draw_from)
static int l_shader_set_float_immediate(lua_State* L) {
    GLuint shader = (GLuint)luaL_checkinteger(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float value = (float)luaL_checknumber(L, 3);
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform1f(loc, value);
    return 0;
}

static int l_shader_set_vec2_immediate(lua_State* L) {
    GLuint shader = (GLuint)luaL_checkinteger(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform2f(loc, x, y);
    return 0;
}

static int l_shader_set_vec4_immediate(lua_State* L) {
    GLuint shader = (GLuint)luaL_checkinteger(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    float z = (float)luaL_checknumber(L, 5);
    float w = (float)luaL_checknumber(L, 6);
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform4f(loc, x, y, z, w);
    return 0;
}

static int l_shader_set_int_immediate(lua_State* L) {
    GLuint shader = (GLuint)luaL_checkinteger(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int value = (int)luaL_checkinteger(L, 3);
    glUseProgram(shader);
    GLint loc = glGetUniformLocation(shader, name);
    if (loc != -1) glUniform1i(loc, value);
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

// layer_shader_set_texture(layer, shader, name, texture_userdata, unit)
static int l_layer_shader_set_texture(lua_State* L) {
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    GLuint shader = (GLuint)luaL_checkinteger(L, 2);
    const char* name = luaL_checkstring(L, 3);
    Texture* tex = (Texture*)lua_touserdata(L, 4);
    int unit = (int)luaL_optinteger(L, 5, 1);
    if (tex) {
        layer_shader_set_texture(layer, shader, name, tex->id, unit);
    }
    return 0;
}

// Layer effect Lua bindings
static int l_layer_apply_shader(lua_State* L) {
    if (headless_mode) return 0;
    Layer* layer = (Layer*)lua_touserdata(L, 1);
    GLuint shader = (GLuint)luaL_checkinteger(L, 2);
    layer_apply_shader(layer, shader);
    return 0;
}

static int l_layer_draw(lua_State* L) {
    if (headless_mode) return 0;
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

// Clear a layer's FBO contents (transparent black)
static int l_layer_clear(lua_State* L) {
    if (headless_mode) return 0;
    Layer* layer = (Layer*)lua_touserdata(L, 1);

    // Bind the layer's current target FBO
    GLuint target_fbo = layer->textures_swapped ? layer->effect_fbo : layer->fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glViewport(0, 0, layer->width, layer->height);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    return 0;
}

// Render a layer's queued commands to its FBO (with clear)
// This is called explicitly from Lua draw() instead of automatically
static int l_layer_render(lua_State* L) {
    if (headless_mode) return 0;
    Layer* layer = (Layer*)lua_touserdata(L, 1);

    // Bind layer's FBO
    glBindFramebuffer(GL_FRAMEBUFFER, layer->fbo);
    glViewport(0, 0, layer->width, layer->height);

    // Clear color and stencil buffers
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Ensure stencil starts disabled
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Process all queued commands
    layer_render(layer);

    // Ensure stencil is disabled after rendering (in case commands left it enabled)
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Clear command queue for next frame
    layer->command_count = 0;

    return 0;
}

// Draw source layer's texture to destination layer's FBO
// Optional shader parameter - if 0/nil, uses passthrough
static int l_layer_draw_from(lua_State* L) {
    if (headless_mode) return 0;
    Layer* dst = (Layer*)lua_touserdata(L, 1);
    Layer* src = (Layer*)lua_touserdata(L, 2);
    GLuint shader = (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) ? (GLuint)luaL_checkinteger(L, 3) : 0;

    // Bind destination layer's FBO
    GLuint target_fbo = dst->textures_swapped ? dst->effect_fbo : dst->fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glViewport(0, 0, dst->width, dst->height);

    // Enable alpha blending for accumulation
    // Use glBlendFuncSeparate to preserve alpha correctly when drawing to FBOs
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,  // RGB
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);       // Alpha

    // Use shader or passthrough
    if (shader) {
        glUseProgram(shader);
        GLint tex_loc = glGetUniformLocation(shader, "u_texture");
        if (tex_loc != -1) glUniform1i(tex_loc, 0);
    } else {
        glUseProgram(screen_shader);
        GLint offset_loc = glGetUniformLocation(screen_shader, "u_offset");
        if (offset_loc != -1) glUniform2f(offset_loc, 0.0f, 0.0f);
    }

    // Bind source layer's current texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, layer_get_texture(src));

    // Draw fullscreen quad
    glBindVertexArray(screen_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Restore drawing shader
    glUseProgram(shader_program);

    return 0;
}

// ============================================================================
// LUA BINDINGS: PHYSICS
// World, bodies, shapes, events, spatial queries, raycasting
// ============================================================================

// Physics Lua bindings
static int l_physics_init(lua_State* L) {
    if (physics_initialized) {
        return 0;  // Already initialized
    }

    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = (b2Vec2){0.0f, 10.0f};  // Default gravity (10 m/s² down)
    world_def.restitutionThreshold = 0.0f;  // Allow full restitution at any speed

    physics_world = b2CreateWorld(&world_def);
    b2World_SetCustomFilterCallback(physics_world, physics_custom_filter, NULL);
    physics_initialized = true;
    shape_user_data_count = 0;
    printf("Physics initialized (Box2D)\n");
    return 0;
}

static int l_physics_set_gravity(lua_State* L) {
    if (!physics_initialized) {
        return luaL_error(L, "Physics not initialized. Call physics_init() first.");
    }

    float gx = (float)luaL_checknumber(L, 1);
    float gy = (float)luaL_checknumber(L, 2);

    // Convert from pixels/sec² to meters/sec²
    b2Vec2 gravity = {gx / pixels_per_meter, gy / pixels_per_meter};
    b2World_SetGravity(physics_world, gravity);
    return 0;
}

static int l_physics_set_meter_scale(lua_State* L) {
    float scale = (float)luaL_checknumber(L, 1);
    if (scale <= 0) {
        return luaL_error(L, "Meter scale must be positive");
    }
    pixels_per_meter = scale;
    return 0;
}

static int l_physics_set_enabled(lua_State* L) {
    physics_enabled = lua_toboolean(L, 1);
    return 0;
}

static int l_physics_register_tag(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    // Check if already registered
    if (physics_tag_find(name) >= 0) {
        return 0;  // Already exists, silently succeed
    }

    // Check capacity
    if (physics_tag_count >= MAX_PHYSICS_TAGS) {
        return luaL_error(L, "Maximum number of physics tags (%d) reached", MAX_PHYSICS_TAGS);
    }

    // Register new tag
    PhysicsTag* tag = &physics_tags[physics_tag_count];
    strncpy(tag->name, name, MAX_TAG_NAME - 1);
    tag->name[MAX_TAG_NAME - 1] = '\0';
    tag->category_bit = (uint64_t)1 << physics_tag_count;  // Assign next bit
    tag->collision_mask = 0;  // No collisions by default
    tag->sensor_mask = 0;     // No sensor events by default
    tag->hit_mask = 0;        // No hit events by default

    physics_tag_count++;
    return 0;
}

static int l_physics_enable_collision(lua_State* L) {
    const char* name_a = luaL_checkstring(L, 1);
    const char* name_b = luaL_checkstring(L, 2);

    PhysicsTag* tag_a = physics_tag_get_by_name(name_a);
    PhysicsTag* tag_b = physics_tag_get_by_name(name_b);

    if (!tag_a) return luaL_error(L, "Unknown physics tag: %s", name_a);
    if (!tag_b) return luaL_error(L, "Unknown physics tag: %s", name_b);

    // Enable collision both ways
    tag_a->collision_mask |= tag_b->category_bit;
    tag_b->collision_mask |= tag_a->category_bit;
    return 0;
}

static int l_physics_disable_collision(lua_State* L) {
    const char* name_a = luaL_checkstring(L, 1);
    const char* name_b = luaL_checkstring(L, 2);

    PhysicsTag* tag_a = physics_tag_get_by_name(name_a);
    PhysicsTag* tag_b = physics_tag_get_by_name(name_b);

    if (!tag_a) return luaL_error(L, "Unknown physics tag: %s", name_a);
    if (!tag_b) return luaL_error(L, "Unknown physics tag: %s", name_b);

    // Disable collision both ways
    tag_a->collision_mask &= ~tag_b->category_bit;
    tag_b->collision_mask &= ~tag_a->category_bit;
    return 0;
}

static int l_physics_enable_sensor(lua_State* L) {
    const char* name_a = luaL_checkstring(L, 1);
    const char* name_b = luaL_checkstring(L, 2);

    PhysicsTag* tag_a = physics_tag_get_by_name(name_a);
    PhysicsTag* tag_b = physics_tag_get_by_name(name_b);

    if (!tag_a) return luaL_error(L, "Unknown physics tag: %s", name_a);
    if (!tag_b) return luaL_error(L, "Unknown physics tag: %s", name_b);

    // Enable sensor events both ways
    tag_a->sensor_mask |= tag_b->category_bit;
    tag_b->sensor_mask |= tag_a->category_bit;
    return 0;
}

static int l_physics_enable_hit(lua_State* L) {
    const char* name_a = luaL_checkstring(L, 1);
    const char* name_b = luaL_checkstring(L, 2);

    PhysicsTag* tag_a = physics_tag_get_by_name(name_a);
    PhysicsTag* tag_b = physics_tag_get_by_name(name_b);

    if (!tag_a) return luaL_error(L, "Unknown physics tag: %s", name_a);
    if (!tag_b) return luaL_error(L, "Unknown physics tag: %s", name_b);

    // Enable hit events both ways
    tag_a->hit_mask |= tag_b->category_bit;
    tag_b->hit_mask |= tag_a->category_bit;
    return 0;
}

static int l_physics_tags_collide(lua_State* L) {
    const char* name_a = luaL_checkstring(L, 1);
    const char* name_b = luaL_checkstring(L, 2);

    PhysicsTag* tag_a = physics_tag_get_by_name(name_a);
    PhysicsTag* tag_b = physics_tag_get_by_name(name_b);

    if (!tag_a || !tag_b) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // Check if either tag's collision mask includes the other
    bool collides = (tag_a->collision_mask & tag_b->category_bit) != 0;
    lua_pushboolean(L, collides);
    return 1;
}

static int l_physics_create_body(lua_State* L) {
    if (!physics_initialized) {
        return luaL_error(L, "Physics not initialized. Call physics_init() first.");
    }

    const char* type_str = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);

    // Determine body type
    b2BodyType body_type;
    if (strcmp(type_str, "static") == 0) {
        body_type = b2_staticBody;
    } else if (strcmp(type_str, "dynamic") == 0) {
        body_type = b2_dynamicBody;
    } else if (strcmp(type_str, "kinematic") == 0) {
        body_type = b2_kinematicBody;
    } else {
        return luaL_error(L, "Invalid body type: %s (use 'static', 'dynamic', or 'kinematic')", type_str);
    }

    // Create body definition
    b2BodyDef body_def = b2DefaultBodyDef();
    body_def.type = body_type;
    body_def.position = (b2Vec2){x / pixels_per_meter, y / pixels_per_meter};

    // Create body
    b2BodyId body_id = b2CreateBody(physics_world, &body_def);

    // Return body ID as userdata
    b2BodyId* ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
    *ud = body_id;

    return 1;
}

static int l_physics_destroy_body(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id) {
        return luaL_error(L, "Invalid body");
    }

    if (b2Body_IsValid(*body_id)) {
        b2DestroyBody(*body_id);
    }
    return 0;
}

static int l_physics_get_position(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }

    b2Vec2 pos = b2Body_GetPosition(*body_id);
    lua_pushnumber(L, pos.x * pixels_per_meter);
    lua_pushnumber(L, pos.y * pixels_per_meter);
    return 2;
}

static int l_physics_get_angle(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }

    b2Rot rot = b2Body_GetRotation(*body_id);
    float angle = b2Rot_GetAngle(rot);
    lua_pushnumber(L, angle);
    return 1;
}

static int l_physics_get_body_count(lua_State* L) {
    if (!physics_initialized) {
        lua_pushinteger(L, 0);
        return 1;
    }

    b2Counters counters = b2World_GetCounters(physics_world);
    lua_pushinteger(L, counters.bodyCount);
    return 1;
}

static int l_physics_body_is_valid(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, b2Body_IsValid(*body_id));
    return 1;
}

// Helper to setup shape def from tag
static void setup_shape_def_from_tag(b2ShapeDef* def, PhysicsTag* tag, bool is_sensor) {
    def->filter.categoryBits = tag->category_bit;
    def->filter.maskBits = tag->collision_mask | tag->sensor_mask;  // Include both for filtering
    def->isSensor = is_sensor;

    // Enable events based on tag configuration
    def->enableSensorEvents = (tag->sensor_mask != 0);
    def->enableContactEvents = (tag->collision_mask != 0);
    def->enableHitEvents = (tag->hit_mask != 0);
    def->enableCustomFiltering = true;
}

// physics_add_circle(body, tag, radius, [opts])
static int l_physics_add_circle(lua_State* L) {
    if (!physics_initialized) {
        return luaL_error(L, "Physics not initialized");
    }

    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id) return luaL_error(L, "Invalid body");

    const char* tag_name = luaL_checkstring(L, 2);
    PhysicsTag* tag = physics_tag_get_by_name(tag_name);
    if (!tag) return luaL_error(L, "Unknown physics tag: %s", tag_name);

    float radius = (float)luaL_checknumber(L, 3);

    // Parse options table (4th argument, optional)
    bool is_sensor = false;
    float offset_x = 0, offset_y = 0;
    if (lua_istable(L, 4)) {
        lua_getfield(L, 4, "sensor");
        if (!lua_isnil(L, -1)) is_sensor = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 4, "offset_x");
        if (!lua_isnil(L, -1)) offset_x = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 4, "offset_y");
        if (!lua_isnil(L, -1)) offset_y = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    // Create shape def
    b2ShapeDef shape_def = b2DefaultShapeDef();
    setup_shape_def_from_tag(&shape_def, tag, is_sensor);

    // Create circle geometry (convert to meters)
    b2Circle circle = {
        .center = { offset_x / pixels_per_meter, offset_y / pixels_per_meter },
        .radius = radius / pixels_per_meter
    };

    // Create shape
    b2ShapeId shape_id = b2CreateCircleShape(*body_id, &shape_def, &circle);

    // Store tag index in shape's user data for event lookup
    ShapeUserData* sud = &shape_user_data_pool[shape_user_data_count++];
    sud->tag_index = (int)(tag - physics_tags);
    sud->filter_group = 0;
    b2Shape_SetUserData(shape_id, sud);

    // Return shape ID as userdata
    b2ShapeId* ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
    *ud = shape_id;
    return 1;
}

// physics_add_box(body, tag, width, height, [opts])
static int l_physics_add_box(lua_State* L) {
    if (!physics_initialized) {
        return luaL_error(L, "Physics not initialized");
    }

    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id) return luaL_error(L, "Invalid body");

    const char* tag_name = luaL_checkstring(L, 2);
    PhysicsTag* tag = physics_tag_get_by_name(tag_name);
    if (!tag) return luaL_error(L, "Unknown physics tag: %s", tag_name);

    float width = (float)luaL_checknumber(L, 3);
    float height = (float)luaL_checknumber(L, 4);

    // Parse options table (5th argument, optional)
    bool is_sensor = false;
    float offset_x = 0, offset_y = 0;
    float angle = 0;
    if (lua_istable(L, 5)) {
        lua_getfield(L, 5, "sensor");
        if (!lua_isnil(L, -1)) is_sensor = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 5, "offset_x");
        if (!lua_isnil(L, -1)) offset_x = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 5, "offset_y");
        if (!lua_isnil(L, -1)) offset_y = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 5, "angle");
        if (!lua_isnil(L, -1)) angle = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    // Create shape def
    b2ShapeDef shape_def = b2DefaultShapeDef();
    setup_shape_def_from_tag(&shape_def, tag, is_sensor);

    // Create box polygon (convert to meters)
    float half_w = (width / 2.0f) / pixels_per_meter;
    float half_h = (height / 2.0f) / pixels_per_meter;
    b2Vec2 center = { offset_x / pixels_per_meter, offset_y / pixels_per_meter };
    b2Rot rotation = b2MakeRot(angle);
    b2Polygon box = b2MakeOffsetBox(half_w, half_h, center, rotation);

    // Create shape
    b2ShapeId shape_id = b2CreatePolygonShape(*body_id, &shape_def, &box);

    // Store tag index in shape's user data for event lookup
    ShapeUserData* sud = &shape_user_data_pool[shape_user_data_count++];
    sud->tag_index = (int)(tag - physics_tags);
    sud->filter_group = 0;
    b2Shape_SetUserData(shape_id, sud);

    // Return shape ID as userdata
    b2ShapeId* ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
    *ud = shape_id;
    return 1;
}

// physics_add_capsule(body, tag, length, radius, [opts])
static int l_physics_add_capsule(lua_State* L) {
    if (!physics_initialized) {
        return luaL_error(L, "Physics not initialized");
    }

    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id) return luaL_error(L, "Invalid body");

    const char* tag_name = luaL_checkstring(L, 2);
    PhysicsTag* tag = physics_tag_get_by_name(tag_name);
    if (!tag) return luaL_error(L, "Unknown physics tag: %s", tag_name);

    float length = (float)luaL_checknumber(L, 3);
    float radius = (float)luaL_checknumber(L, 4);

    // Parse options table (5th argument, optional)
    bool is_sensor = false;
    float offset_x = 0, offset_y = 0;
    if (lua_istable(L, 5)) {
        lua_getfield(L, 5, "sensor");
        if (!lua_isnil(L, -1)) is_sensor = lua_toboolean(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 5, "offset_x");
        if (!lua_isnil(L, -1)) offset_x = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, 5, "offset_y");
        if (!lua_isnil(L, -1)) offset_y = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    // Create shape def
    b2ShapeDef shape_def = b2DefaultShapeDef();
    setup_shape_def_from_tag(&shape_def, tag, is_sensor);

    // Create capsule geometry (vertical, convert to meters)
    float half_len = (length / 2.0f) / pixels_per_meter;
    float rad = radius / pixels_per_meter;
    float ox = offset_x / pixels_per_meter;
    float oy = offset_y / pixels_per_meter;

    b2Capsule capsule = {
        .center1 = { ox, oy - half_len },
        .center2 = { ox, oy + half_len },
        .radius = rad
    };

    // Create shape
    b2ShapeId shape_id = b2CreateCapsuleShape(*body_id, &shape_def, &capsule);

    // Store tag index in shape's user data for event lookup
    ShapeUserData* sud = &shape_user_data_pool[shape_user_data_count++];
    sud->tag_index = (int)(tag - physics_tags);
    sud->filter_group = 0;
    b2Shape_SetUserData(shape_id, sud);

    // Return shape ID as userdata
    b2ShapeId* ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
    *ud = shape_id;
    return 1;
}

// physics_add_polygon(body, tag, vertices, [opts])
// vertices is a flat array: {x1, y1, x2, y2, ...}
static int l_physics_add_polygon(lua_State* L) {
    if (!physics_initialized) {
        return luaL_error(L, "Physics not initialized");
    }

    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id) return luaL_error(L, "Invalid body");

    const char* tag_name = luaL_checkstring(L, 2);
    PhysicsTag* tag = physics_tag_get_by_name(tag_name);
    if (!tag) return luaL_error(L, "Unknown physics tag: %s", tag_name);

    // Read vertices from table
    luaL_checktype(L, 3, LUA_TTABLE);
    int len = (int)lua_rawlen(L, 3);
    if (len < 6 || len % 2 != 0) {
        return luaL_error(L, "Polygon needs at least 3 vertices (6 numbers)");
    }

    int vertex_count = len / 2;
    if (vertex_count > B2_MAX_POLYGON_VERTICES) {
        return luaL_error(L, "Too many vertices (max %d)", B2_MAX_POLYGON_VERTICES);
    }

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];
    for (int i = 0; i < vertex_count; i++) {
        lua_rawgeti(L, 3, i * 2 + 1);
        lua_rawgeti(L, 3, i * 2 + 2);
        points[i].x = (float)lua_tonumber(L, -2) / pixels_per_meter;
        points[i].y = (float)lua_tonumber(L, -1) / pixels_per_meter;
        lua_pop(L, 2);
    }

    // Parse options table (4th argument, optional)
    bool is_sensor = false;
    if (lua_istable(L, 4)) {
        lua_getfield(L, 4, "sensor");
        if (!lua_isnil(L, -1)) is_sensor = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    // Create shape def
    b2ShapeDef shape_def = b2DefaultShapeDef();
    setup_shape_def_from_tag(&shape_def, tag, is_sensor);

    // Compute convex hull
    b2Hull hull = b2ComputeHull(points, vertex_count);
    if (hull.count == 0) {
        return luaL_error(L, "Failed to compute convex hull from vertices");
    }

    // Create polygon from hull
    b2Polygon polygon = b2MakePolygon(&hull, 0.0f);

    // Create shape
    b2ShapeId shape_id = b2CreatePolygonShape(*body_id, &shape_def, &polygon);

    // Store tag index in shape's user data for event lookup
    ShapeUserData* sud = &shape_user_data_pool[shape_user_data_count++];
    sud->tag_index = (int)(tag - physics_tags);
    sud->filter_group = 0;
    b2Shape_SetUserData(shape_id, sud);

    // Return shape ID as userdata
    b2ShapeId* ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
    *ud = shape_id;
    return 1;
}

// physics_add_chain(body, tag, vertices, is_loop)
// vertices is a flat array: {x1, y1, x2, y2, ...} (at least 4 points)
// is_loop: true for closed loop, false for open chain
// Returns chain ID as userdata
static int l_physics_add_chain(lua_State* L) {
    if (!physics_initialized) {
        return luaL_error(L, "Physics not initialized");
    }

    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id) return luaL_error(L, "Invalid body");

    const char* tag_name = luaL_checkstring(L, 2);
    PhysicsTag* tag = physics_tag_get_by_name(tag_name);
    if (!tag) return luaL_error(L, "Unknown physics tag: %s", tag_name);

    // Read vertices from table
    luaL_checktype(L, 3, LUA_TTABLE);
    int len = (int)lua_rawlen(L, 3);
    if (len < 8 || len % 2 != 0) {
        return luaL_error(L, "Chain needs at least 4 points (8 numbers)");
    }

    int point_count = len / 2;
    b2Vec2* points = (b2Vec2*)malloc(point_count * sizeof(b2Vec2));
    if (!points) return luaL_error(L, "Out of memory");

    for (int i = 0; i < point_count; i++) {
        lua_rawgeti(L, 3, i * 2 + 1);
        lua_rawgeti(L, 3, i * 2 + 2);
        points[i].x = (float)lua_tonumber(L, -2) / pixels_per_meter;
        points[i].y = (float)lua_tonumber(L, -1) / pixels_per_meter;
        lua_pop(L, 2);
    }

    bool is_loop = lua_toboolean(L, 4);

    // Create chain def
    b2ChainDef chain_def = b2DefaultChainDef();
    chain_def.points = points;
    chain_def.count = point_count;
    chain_def.isLoop = is_loop;
    chain_def.filter.categoryBits = tag->category_bit;
    chain_def.filter.maskBits = tag->collision_mask | tag->sensor_mask;
    chain_def.enableSensorEvents = (tag->sensor_mask != 0);

    // Default material
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    chain_def.materials = &material;
    chain_def.materialCount = 1;

    // Create chain
    b2ChainId chain_id = b2CreateChain(*body_id, &chain_def);
    free(points);

    // Set user data on all chain segments for event processing
    int seg_count = b2Chain_GetSegmentCount(chain_id);
    b2ShapeId* segments = (b2ShapeId*)malloc(seg_count * sizeof(b2ShapeId));
    if (segments) {
        b2Chain_GetSegments(chain_id, segments, seg_count);
        for (int i = 0; i < seg_count; i++) {
            if (shape_user_data_count < MAX_SHAPE_USER_DATA) {
                ShapeUserData* sud = &shape_user_data_pool[shape_user_data_count++];
                sud->tag_index = (int)(tag - physics_tags);
                sud->filter_group = 0;
                b2Shape_SetUserData(segments[i], sud);
            }
        }
        free(segments);
    }

    // Return chain ID as userdata
    b2ChainId* ud = (b2ChainId*)lua_newuserdata(L, sizeof(b2ChainId));
    *ud = chain_id;
    return 1;
}

static int l_physics_set_position(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float x = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float y = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    b2Rot rot = b2Body_GetRotation(*body_id);
    b2Body_SetTransform(*body_id, (b2Vec2){x, y}, rot);
    return 0;
}

static int l_physics_set_angle(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float angle = (float)luaL_checknumber(L, 2);
    b2Vec2 pos = b2Body_GetPosition(*body_id);
    b2Body_SetTransform(*body_id, pos, b2MakeRot(angle));
    return 0;
}

static int l_physics_set_transform(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float x = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float y = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    float angle = (float)luaL_checknumber(L, 4);
    b2Body_SetTransform(*body_id, (b2Vec2){x, y}, b2MakeRot(angle));
    return 0;
}

static int l_physics_get_velocity(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    b2Vec2 vel = b2Body_GetLinearVelocity(*body_id);
    lua_pushnumber(L, vel.x * pixels_per_meter);
    lua_pushnumber(L, vel.y * pixels_per_meter);
    return 2;
}

static int l_physics_get_angular_velocity(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float av = b2Body_GetAngularVelocity(*body_id);
    lua_pushnumber(L, av);
    return 1;
}

static int l_physics_set_velocity(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float vx = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float vy = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    b2Body_SetLinearVelocity(*body_id, (b2Vec2){vx, vy});
    return 0;
}

static int l_physics_set_angular_velocity(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float av = (float)luaL_checknumber(L, 2);
    b2Body_SetAngularVelocity(*body_id, av);
    return 0;
}

static int l_physics_apply_force(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float fx = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float fy = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    b2Vec2 center = b2Body_GetWorldCenterOfMass(*body_id);
    b2Body_ApplyForce(*body_id, (b2Vec2){fx, fy}, center, true);
    return 0;
}

static int l_physics_apply_force_at(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float fx = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float fy = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    float px = (float)luaL_checknumber(L, 4) / pixels_per_meter;
    float py = (float)luaL_checknumber(L, 5) / pixels_per_meter;
    b2Body_ApplyForce(*body_id, (b2Vec2){fx, fy}, (b2Vec2){px, py}, true);
    return 0;
}

static int l_physics_apply_impulse(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float ix = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float iy = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    b2Vec2 center = b2Body_GetWorldCenterOfMass(*body_id);
    b2Body_ApplyLinearImpulse(*body_id, (b2Vec2){ix, iy}, center, true);
    return 0;
}

static int l_physics_apply_impulse_at(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float ix = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float iy = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    float px = (float)luaL_checknumber(L, 4) / pixels_per_meter;
    float py = (float)luaL_checknumber(L, 5) / pixels_per_meter;
    b2Body_ApplyLinearImpulse(*body_id, (b2Vec2){ix, iy}, (b2Vec2){px, py}, true);
    return 0;
}

static int l_physics_apply_torque(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float torque = (float)luaL_checknumber(L, 2);
    b2Body_ApplyTorque(*body_id, torque, true);
    return 0;
}

static int l_physics_apply_angular_impulse(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float impulse = (float)luaL_checknumber(L, 2);
    b2Body_ApplyAngularImpulse(*body_id, impulse, true);
    return 0;
}

static int l_physics_set_linear_damping(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float damping = (float)luaL_checknumber(L, 2);
    b2Body_SetLinearDamping(*body_id, damping);
    return 0;
}

static int l_physics_set_angular_damping(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float damping = (float)luaL_checknumber(L, 2);
    b2Body_SetAngularDamping(*body_id, damping);
    return 0;
}

static int l_physics_set_gravity_scale(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float scale = (float)luaL_checknumber(L, 2);
    b2Body_SetGravityScale(*body_id, scale);
    return 0;
}

static int l_physics_set_fixed_rotation(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    bool fixed = lua_toboolean(L, 2);
    b2MotionLocks locks = b2Body_GetMotionLocks(*body_id);
    locks.angularZ = fixed;
    b2Body_SetMotionLocks(*body_id, locks);
    return 0;
}

static int l_physics_set_bullet(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    bool bullet = lua_toboolean(L, 2);
    b2Body_SetBullet(*body_id, bullet);
    return 0;
}

static int l_physics_set_user_data(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    lua_Integer id = luaL_checkinteger(L, 2);
    b2Body_SetUserData(*body_id, (void*)(intptr_t)id);
    return 0;
}

static int l_physics_get_user_data(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    void* data = b2Body_GetUserData(*body_id);
    lua_pushinteger(L, (lua_Integer)(intptr_t)data);
    return 1;
}

static int l_physics_shape_set_friction(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id)) {
        return luaL_error(L, "Invalid shape");
    }
    float friction = (float)luaL_checknumber(L, 2);
    b2Shape_SetFriction(*shape_id, friction);
    return 0;
}

static int l_physics_shape_get_friction(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id)) {
        return luaL_error(L, "Invalid shape");
    }
    float friction = b2Shape_GetFriction(*shape_id);
    lua_pushnumber(L, friction);
    return 1;
}

static int l_physics_shape_set_restitution(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id)) {
        return luaL_error(L, "Invalid shape");
    }
    float restitution = (float)luaL_checknumber(L, 2);
    b2Shape_SetRestitution(*shape_id, restitution);
    return 0;
}

static int l_physics_shape_get_restitution(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id)) {
        return luaL_error(L, "Invalid shape");
    }
    float restitution = b2Shape_GetRestitution(*shape_id);
    lua_pushnumber(L, restitution);
    return 1;
}

static int l_physics_shape_is_valid(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, b2Shape_IsValid(*shape_id));
    return 1;
}

static int l_physics_shape_get_body(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id)) {
        return luaL_error(L, "Invalid shape");
    }
    b2BodyId body_id = b2Shape_GetBody(*shape_id);
    b2BodyId* ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
    *ud = body_id;
    return 1;
}

static int l_physics_shape_set_density(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id)) {
        return luaL_error(L, "Invalid shape");
    }
    float density = (float)luaL_checknumber(L, 2);
    b2Shape_SetDensity(*shape_id, density, true);  // true = update body mass
    return 0;
}

static int l_physics_shape_get_density(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id)) {
        return luaL_error(L, "Invalid shape");
    }
    float density = b2Shape_GetDensity(*shape_id);
    lua_pushnumber(L, density);
    return 1;
}

static int l_physics_shape_destroy(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id)) {
        return 0;  // Already destroyed or invalid, silently succeed
    }
    bool update_mass = lua_isboolean(L, 2) ? lua_toboolean(L, 2) : true;
    b2DestroyShape(*shape_id, update_mass);
    return 0;
}

static int l_physics_shape_set_filter_group(lua_State* L) {
    b2ShapeId* shape_id = (b2ShapeId*)lua_touserdata(L, 1);
    if (!shape_id || !b2Shape_IsValid(*shape_id))
        return luaL_error(L, "Invalid shape");
    int group = (int)luaL_checkinteger(L, 2);
    ShapeUserData* ud = (ShapeUserData*)b2Shape_GetUserData(*shape_id);
    if (ud) ud->filter_group = group;
    return 0;
}

// Additional body queries
static int l_physics_get_body_type(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    b2BodyType type = b2Body_GetType(*body_id);
    switch (type) {
        case b2_staticBody: lua_pushstring(L, "static"); break;
        case b2_kinematicBody: lua_pushstring(L, "kinematic"); break;
        case b2_dynamicBody: lua_pushstring(L, "dynamic"); break;
        default: lua_pushstring(L, "unknown"); break;
    }
    return 1;
}

static int l_physics_get_mass(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float mass = b2Body_GetMass(*body_id);
    lua_pushnumber(L, mass);
    return 1;
}

// Set the center of mass relative to body origin (in pixels)
// This allows overriding the computed center of mass from shapes
static int l_physics_set_center_of_mass(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    float x = luaL_checknumber(L, 2);
    float y = luaL_checknumber(L, 3);

    // Get current mass data
    b2MassData massData = b2Body_GetMassData(*body_id);

    // Override center of mass (convert from pixels to meters)
    massData.center = (b2Vec2){ x / pixels_per_meter, y / pixels_per_meter };

    // Apply modified mass data
    b2Body_SetMassData(*body_id, massData);

    return 0;
}

static int l_physics_is_awake(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    bool awake = b2Body_IsAwake(*body_id);
    lua_pushboolean(L, awake);
    return 1;
}

static int l_physics_set_awake(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }
    bool awake = lua_toboolean(L, 2);
    b2Body_SetAwake(*body_id, awake);
    return 0;
}

// physics_get_shapes_geometry(body) -> table of shapes with world-space geometry
// Returns: {{type="circle", x=..., y=..., radius=...}, {type="polygon", vertices={x1,y1,x2,y2,...}}, ...}
static int l_physics_get_shapes_geometry(lua_State* L) {
    b2BodyId* body_id = (b2BodyId*)lua_touserdata(L, 1);
    if (!body_id || !b2Body_IsValid(*body_id)) {
        return luaL_error(L, "Invalid body");
    }

    int shape_count = b2Body_GetShapeCount(*body_id);
    if (shape_count <= 0) {
        lua_newtable(L);
        return 1;
    }

    b2ShapeId shapes[32];  // max 32 shapes per body
    if (shape_count > 32) shape_count = 32;
    int actual_count = b2Body_GetShapes(*body_id, shapes, shape_count);

    b2Transform xf = b2Body_GetTransform(*body_id);

    lua_newtable(L);  // result table

    for (int i = 0; i < actual_count; i++) {
        if (!b2Shape_IsValid(shapes[i])) continue;

        lua_newtable(L);  // shape entry

        b2ShapeType type = b2Shape_GetType(shapes[i]);

        // Add sensor flag
        bool is_sensor = b2Shape_IsSensor(shapes[i]);
        lua_pushboolean(L, is_sensor);
        lua_setfield(L, -2, "sensor");

        // Add tag name
        ShapeUserData* sud = (ShapeUserData*)b2Shape_GetUserData(shapes[i]);
        if (sud && sud->tag_index >= 0 && sud->tag_index < physics_tag_count) {
            lua_pushstring(L, physics_tags[sud->tag_index].name);
        } else {
            lua_pushstring(L, "unknown");
        }
        lua_setfield(L, -2, "tag");

        switch (type) {
            case b2_circleShape: {
                b2Circle circle = b2Shape_GetCircle(shapes[i]);
                b2Vec2 world_center = b2TransformPoint(xf, circle.center);

                lua_pushstring(L, "circle");
                lua_setfield(L, -2, "type");
                lua_pushnumber(L, world_center.x * pixels_per_meter);
                lua_setfield(L, -2, "x");
                lua_pushnumber(L, world_center.y * pixels_per_meter);
                lua_setfield(L, -2, "y");
                lua_pushnumber(L, circle.radius * pixels_per_meter);
                lua_setfield(L, -2, "radius");
                break;
            }
            case b2_polygonShape: {
                b2Polygon poly = b2Shape_GetPolygon(shapes[i]);

                lua_pushstring(L, "polygon");
                lua_setfield(L, -2, "type");

                // Vertices as flat array {x1, y1, x2, y2, ...}
                lua_newtable(L);
                for (int j = 0; j < poly.count; j++) {
                    b2Vec2 world_v = b2TransformPoint(xf, poly.vertices[j]);
                    lua_pushnumber(L, world_v.x * pixels_per_meter);
                    lua_rawseti(L, -2, j * 2 + 1);
                    lua_pushnumber(L, world_v.y * pixels_per_meter);
                    lua_rawseti(L, -2, j * 2 + 2);
                }
                lua_setfield(L, -2, "vertices");

                lua_pushinteger(L, poly.count);
                lua_setfield(L, -2, "count");
                lua_pushnumber(L, poly.radius * pixels_per_meter);
                lua_setfield(L, -2, "radius");
                break;
            }
            case b2_capsuleShape: {
                b2Capsule capsule = b2Shape_GetCapsule(shapes[i]);
                b2Vec2 world_c1 = b2TransformPoint(xf, capsule.center1);
                b2Vec2 world_c2 = b2TransformPoint(xf, capsule.center2);

                lua_pushstring(L, "capsule");
                lua_setfield(L, -2, "type");
                lua_pushnumber(L, world_c1.x * pixels_per_meter);
                lua_setfield(L, -2, "x1");
                lua_pushnumber(L, world_c1.y * pixels_per_meter);
                lua_setfield(L, -2, "y1");
                lua_pushnumber(L, world_c2.x * pixels_per_meter);
                lua_setfield(L, -2, "x2");
                lua_pushnumber(L, world_c2.y * pixels_per_meter);
                lua_setfield(L, -2, "y2");
                lua_pushnumber(L, capsule.radius * pixels_per_meter);
                lua_setfield(L, -2, "radius");
                break;
            }
            case b2_segmentShape: {
                b2Segment seg = b2Shape_GetSegment(shapes[i]);
                b2Vec2 world_p1 = b2TransformPoint(xf, seg.point1);
                b2Vec2 world_p2 = b2TransformPoint(xf, seg.point2);

                lua_pushstring(L, "segment");
                lua_setfield(L, -2, "type");
                lua_pushnumber(L, world_p1.x * pixels_per_meter);
                lua_setfield(L, -2, "x1");
                lua_pushnumber(L, world_p1.y * pixels_per_meter);
                lua_setfield(L, -2, "y1");
                lua_pushnumber(L, world_p2.x * pixels_per_meter);
                lua_setfield(L, -2, "x2");
                lua_pushnumber(L, world_p2.y * pixels_per_meter);
                lua_setfield(L, -2, "y2");
                break;
            }
            default:
                lua_pushstring(L, "unknown");
                lua_setfield(L, -2, "type");
                break;
        }

        lua_rawseti(L, -2, i + 1);  // result[i+1] = shape_entry
    }

    return 1;
}

static int l_physics_debug_events(lua_State* L) {
    printf("Physics Events - Contact Begin: %d, End: %d, Hit: %d | Sensor Begin: %d, End: %d\n",
           contact_begin_count, contact_end_count, hit_count,
           sensor_begin_count, sensor_end_count);
    return 0;
}

// Event query functions
// Helper to check if two tag indices match (in either order)
static bool tags_match(int event_tag_a, int event_tag_b, int query_tag_a, int query_tag_b) {
    return (event_tag_a == query_tag_a && event_tag_b == query_tag_b) ||
           (event_tag_a == query_tag_b && event_tag_b == query_tag_a);
}

// physics_get_collision_begin(tag_a, tag_b) -> array of {body_a, body_b, shape_a, shape_b, tag_a, tag_b, point_x, point_y, normal_x, normal_y}
static int l_physics_get_collision_begin(lua_State* L) {
    const char* tag_a_name = luaL_checkstring(L, 1);
    const char* tag_b_name = luaL_checkstring(L, 2);

    int tag_a = physics_tag_find(tag_a_name);
    int tag_b = physics_tag_find(tag_b_name);
    if (tag_a < 0) return luaL_error(L, "Unknown tag: %s", tag_a_name);
    if (tag_b < 0) return luaL_error(L, "Unknown tag: %s", tag_b_name);

    lua_newtable(L);
    int result_index = 1;

    for (int i = 0; i < contact_begin_count; i++) {
        PhysicsContactBeginEvent* e = &contact_begin_events[i];
        if (tags_match(e->tag_a, e->tag_b, tag_a, tag_b)) {
            lua_newtable(L);

            // body_a
            b2BodyId* body_a_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *body_a_ud = e->body_a;
            lua_setfield(L, -2, "body_a");

            // body_b
            b2BodyId* body_b_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *body_b_ud = e->body_b;
            lua_setfield(L, -2, "body_b");

            // shape_a
            b2ShapeId* shape_a_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *shape_a_ud = e->shape_a;
            lua_setfield(L, -2, "shape_a");

            // shape_b
            b2ShapeId* shape_b_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *shape_b_ud = e->shape_b;
            lua_setfield(L, -2, "shape_b");

            // tag_a (string)
            lua_pushstring(L, physics_tags[e->tag_a].name);
            lua_setfield(L, -2, "tag_a");

            // tag_b (string)
            lua_pushstring(L, physics_tags[e->tag_b].name);
            lua_setfield(L, -2, "tag_b");

            // Contact point and normal
            lua_pushnumber(L, e->point_x);
            lua_setfield(L, -2, "point_x");
            lua_pushnumber(L, e->point_y);
            lua_setfield(L, -2, "point_y");
            lua_pushnumber(L, e->normal_x);
            lua_setfield(L, -2, "normal_x");
            lua_pushnumber(L, e->normal_y);
            lua_setfield(L, -2, "normal_y");

            lua_rawseti(L, -2, result_index++);
        }
    }
    return 1;
}

// physics_get_collision_end(tag_a, tag_b) -> array of {body_a, body_b, shape_a, shape_b, tag_a, tag_b}
static int l_physics_get_collision_end(lua_State* L) {
    const char* tag_a_name = luaL_checkstring(L, 1);
    const char* tag_b_name = luaL_checkstring(L, 2);

    int tag_a = physics_tag_find(tag_a_name);
    int tag_b = physics_tag_find(tag_b_name);
    if (tag_a < 0) return luaL_error(L, "Unknown tag: %s", tag_a_name);
    if (tag_b < 0) return luaL_error(L, "Unknown tag: %s", tag_b_name);

    lua_newtable(L);
    int result_index = 1;

    for (int i = 0; i < contact_end_count; i++) {
        PhysicsContactEndEvent* e = &contact_end_events[i];
        if (tags_match(e->tag_a, e->tag_b, tag_a, tag_b)) {
            lua_newtable(L);

            // body_a (may be invalid if destroyed)
            b2BodyId* body_a_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *body_a_ud = e->body_a;
            lua_setfield(L, -2, "body_a");

            // body_b
            b2BodyId* body_b_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *body_b_ud = e->body_b;
            lua_setfield(L, -2, "body_b");

            // shape_a
            b2ShapeId* shape_a_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *shape_a_ud = e->shape_a;
            lua_setfield(L, -2, "shape_a");

            // shape_b
            b2ShapeId* shape_b_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *shape_b_ud = e->shape_b;
            lua_setfield(L, -2, "shape_b");

            // tag_a (string) - valid even if shape destroyed since tag was cached
            if (e->tag_a >= 0) {
                lua_pushstring(L, physics_tags[e->tag_a].name);
            } else {
                lua_pushnil(L);
            }
            lua_setfield(L, -2, "tag_a");

            // tag_b (string)
            if (e->tag_b >= 0) {
                lua_pushstring(L, physics_tags[e->tag_b].name);
            } else {
                lua_pushnil(L);
            }
            lua_setfield(L, -2, "tag_b");

            lua_rawseti(L, -2, result_index++);
        }
    }
    return 1;
}

// physics_get_hit(tag_a, tag_b) -> array of {body_a, body_b, shape_a, shape_b, tag_a, tag_b, point_x, point_y, normal_x, normal_y, approach_speed}
static int l_physics_get_hit(lua_State* L) {
    const char* tag_a_name = luaL_checkstring(L, 1);
    const char* tag_b_name = luaL_checkstring(L, 2);

    int tag_a = physics_tag_find(tag_a_name);
    int tag_b = physics_tag_find(tag_b_name);
    if (tag_a < 0) return luaL_error(L, "Unknown tag: %s", tag_a_name);
    if (tag_b < 0) return luaL_error(L, "Unknown tag: %s", tag_b_name);

    lua_newtable(L);
    int result_index = 1;

    for (int i = 0; i < hit_count; i++) {
        PhysicsHitEvent* e = &hit_events[i];
        if (tags_match(e->tag_a, e->tag_b, tag_a, tag_b)) {
            lua_newtable(L);

            // body_a
            b2BodyId* body_a_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *body_a_ud = e->body_a;
            lua_setfield(L, -2, "body_a");

            // body_b
            b2BodyId* body_b_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *body_b_ud = e->body_b;
            lua_setfield(L, -2, "body_b");

            // shape_a
            b2ShapeId* shape_a_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *shape_a_ud = e->shape_a;
            lua_setfield(L, -2, "shape_a");

            // shape_b
            b2ShapeId* shape_b_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *shape_b_ud = e->shape_b;
            lua_setfield(L, -2, "shape_b");

            // tag_a (string)
            lua_pushstring(L, physics_tags[e->tag_a].name);
            lua_setfield(L, -2, "tag_a");

            // tag_b (string)
            lua_pushstring(L, physics_tags[e->tag_b].name);
            lua_setfield(L, -2, "tag_b");

            // Hit-specific fields
            lua_pushnumber(L, e->point_x);
            lua_setfield(L, -2, "point_x");
            lua_pushnumber(L, e->point_y);
            lua_setfield(L, -2, "point_y");
            lua_pushnumber(L, e->normal_x);
            lua_setfield(L, -2, "normal_x");
            lua_pushnumber(L, e->normal_y);
            lua_setfield(L, -2, "normal_y");
            lua_pushnumber(L, e->approach_speed);
            lua_setfield(L, -2, "approach_speed");

            lua_rawseti(L, -2, result_index++);
        }
    }
    return 1;
}

// physics_get_sensor_begin(tag_a, tag_b) -> array of {sensor_body, visitor_body, sensor_shape, visitor_shape, sensor_tag, visitor_tag}
static int l_physics_get_sensor_begin(lua_State* L) {
    const char* tag_a_name = luaL_checkstring(L, 1);
    const char* tag_b_name = luaL_checkstring(L, 2);

    int tag_a = physics_tag_find(tag_a_name);
    int tag_b = physics_tag_find(tag_b_name);
    if (tag_a < 0) return luaL_error(L, "Unknown tag: %s", tag_a_name);
    if (tag_b < 0) return luaL_error(L, "Unknown tag: %s", tag_b_name);

    lua_newtable(L);
    int result_index = 1;

    for (int i = 0; i < sensor_begin_count; i++) {
        PhysicsSensorBeginEvent* e = &sensor_begin_events[i];
        if (tags_match(e->sensor_tag, e->visitor_tag, tag_a, tag_b)) {
            lua_newtable(L);

            // sensor_body
            b2BodyId* sensor_body_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *sensor_body_ud = e->sensor_body;
            lua_setfield(L, -2, "sensor_body");

            // visitor_body
            b2BodyId* visitor_body_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *visitor_body_ud = e->visitor_body;
            lua_setfield(L, -2, "visitor_body");

            // sensor_shape
            b2ShapeId* sensor_shape_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *sensor_shape_ud = e->sensor_shape;
            lua_setfield(L, -2, "sensor_shape");

            // visitor_shape
            b2ShapeId* visitor_shape_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *visitor_shape_ud = e->visitor_shape;
            lua_setfield(L, -2, "visitor_shape");

            // sensor_tag (string)
            lua_pushstring(L, physics_tags[e->sensor_tag].name);
            lua_setfield(L, -2, "sensor_tag");

            // visitor_tag (string)
            lua_pushstring(L, physics_tags[e->visitor_tag].name);
            lua_setfield(L, -2, "visitor_tag");

            lua_rawseti(L, -2, result_index++);
        }
    }
    return 1;
}

// physics_get_sensor_end(tag_a, tag_b) -> array of {sensor_body, visitor_body, sensor_shape, visitor_shape, sensor_tag, visitor_tag}
static int l_physics_get_sensor_end(lua_State* L) {
    const char* tag_a_name = luaL_checkstring(L, 1);
    const char* tag_b_name = luaL_checkstring(L, 2);

    int tag_a = physics_tag_find(tag_a_name);
    int tag_b = physics_tag_find(tag_b_name);
    if (tag_a < 0) return luaL_error(L, "Unknown tag: %s", tag_a_name);
    if (tag_b < 0) return luaL_error(L, "Unknown tag: %s", tag_b_name);

    lua_newtable(L);
    int result_index = 1;

    for (int i = 0; i < sensor_end_count; i++) {
        PhysicsSensorEndEvent* e = &sensor_end_events[i];
        if (tags_match(e->sensor_tag, e->visitor_tag, tag_a, tag_b)) {
            lua_newtable(L);

            // sensor_body (may be invalid if destroyed)
            b2BodyId* sensor_body_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *sensor_body_ud = e->sensor_body;
            lua_setfield(L, -2, "sensor_body");

            // visitor_body
            b2BodyId* visitor_body_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
            *visitor_body_ud = e->visitor_body;
            lua_setfield(L, -2, "visitor_body");

            // sensor_shape
            b2ShapeId* sensor_shape_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *sensor_shape_ud = e->sensor_shape;
            lua_setfield(L, -2, "sensor_shape");

            // visitor_shape
            b2ShapeId* visitor_shape_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
            *visitor_shape_ud = e->visitor_shape;
            lua_setfield(L, -2, "visitor_shape");

            // sensor_tag (string) - valid even if shape destroyed since tag was cached
            if (e->sensor_tag >= 0) {
                lua_pushstring(L, physics_tags[e->sensor_tag].name);
            } else {
                lua_pushnil(L);
            }
            lua_setfield(L, -2, "sensor_tag");

            // visitor_tag (string)
            if (e->visitor_tag >= 0) {
                lua_pushstring(L, physics_tags[e->visitor_tag].name);
            } else {
                lua_pushnil(L);
            }
            lua_setfield(L, -2, "visitor_tag");

            lua_rawseti(L, -2, result_index++);
        }
    }
    return 1;
}

// Spatial query context and callback
#define MAX_QUERY_RESULTS 256
typedef struct {
    b2BodyId bodies[MAX_QUERY_RESULTS];
    int count;
    uint64_t tag_mask;  // OR of all queried tag category bits
} QueryContext;

static bool query_overlap_callback(b2ShapeId shape_id, void* context) {
    QueryContext* ctx = (QueryContext*)context;
    if (ctx->count >= MAX_QUERY_RESULTS) return false;  // Stop query

    // Check if this shape's tag matches our query
    ShapeUserData* sud = (ShapeUserData*)b2Shape_GetUserData(shape_id);
    PhysicsTag* tag = sud ? physics_tag_get(sud->tag_index) : NULL;
    if (!tag) return true;  // Continue but skip invalid

    // Only include if shape's category matches our query mask
    if ((tag->category_bit & ctx->tag_mask) == 0) return true;  // Skip, continue

    // Get the body and check for duplicates
    b2BodyId body = b2Shape_GetBody(shape_id);
    for (int i = 0; i < ctx->count; i++) {
        if (B2_ID_EQUALS(ctx->bodies[i], body)) return true;  // Already added
    }

    ctx->bodies[ctx->count++] = body;
    return true;  // Continue query
}

// Helper: Build query filter mask from Lua tags table
static uint64_t build_query_mask_from_table(lua_State* L, int table_index) {
    uint64_t mask = 0;
    lua_pushnil(L);
    while (lua_next(L, table_index) != 0) {
        if (lua_isstring(L, -1)) {
            const char* tag_name = lua_tostring(L, -1);
            PhysicsTag* tag = physics_tag_get_by_name(tag_name);
            if (tag) {
                mask |= tag->category_bit;
            }
        }
        lua_pop(L, 1);  // Pop value, keep key for next iteration
    }
    return mask;
}

// physics_query_point(x, y, tags) -> array of bodies
static int l_physics_query_point(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1) / pixels_per_meter;
    float y = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    luaL_checktype(L, 3, LUA_TTABLE);

    uint64_t mask = build_query_mask_from_table(L, 3);
    if (mask == 0) {
        lua_newtable(L);
        return 1;  // Empty result
    }

    QueryContext ctx = {0};
    ctx.tag_mask = mask;

    // Use a small circle for point query (1 pixel radius)
    float radius = 1.0f / pixels_per_meter;
    b2Vec2 point = {x, y};
    b2ShapeProxy proxy = b2MakeProxy(&point, 1, radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = UINT64_MAX;  // Query matches everything
    filter.maskBits = mask;            // But only shapes with these categories

    b2World_OverlapShape(physics_world, &proxy, filter, query_overlap_callback, &ctx);

    // Return results
    lua_newtable(L);
    for (int i = 0; i < ctx.count; i++) {
        b2BodyId* ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
        *ud = ctx.bodies[i];
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// physics_query_circle(x, y, radius, tags) -> array of bodies
static int l_physics_query_circle(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1) / pixels_per_meter;
    float y = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float radius = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    luaL_checktype(L, 4, LUA_TTABLE);

    uint64_t mask = build_query_mask_from_table(L, 4);
    if (mask == 0) {
        lua_newtable(L);
        return 1;
    }

    QueryContext ctx = {0};
    ctx.tag_mask = mask;

    b2Vec2 center = {x, y};
    b2ShapeProxy proxy = b2MakeProxy(&center, 1, radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = UINT64_MAX;
    filter.maskBits = mask;

    b2World_OverlapShape(physics_world, &proxy, filter, query_overlap_callback, &ctx);

    lua_newtable(L);
    for (int i = 0; i < ctx.count; i++) {
        b2BodyId* ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
        *ud = ctx.bodies[i];
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// physics_query_aabb(x, y, w, h, tags) -> array of bodies
static int l_physics_query_aabb(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1) / pixels_per_meter;
    float y = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float w = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    float h = (float)luaL_checknumber(L, 4) / pixels_per_meter;
    luaL_checktype(L, 5, LUA_TTABLE);

    uint64_t mask = build_query_mask_from_table(L, 5);
    if (mask == 0) {
        lua_newtable(L);
        return 1;
    }

    QueryContext ctx = {0};
    ctx.tag_mask = mask;

    // AABB centered at x,y with half-extents w/2, h/2
    float hw = w / 2.0f;
    float hh = h / 2.0f;
    b2AABB aabb = {{x - hw, y - hh}, {x + hw, y + hh}};

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = UINT64_MAX;
    filter.maskBits = mask;

    b2World_OverlapAABB(physics_world, aabb, filter, query_overlap_callback, &ctx);

    lua_newtable(L);
    for (int i = 0; i < ctx.count; i++) {
        b2BodyId* ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
        *ud = ctx.bodies[i];
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// physics_query_box(x, y, w, h, angle, tags) -> array of bodies (rotated box)
static int l_physics_query_box(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1) / pixels_per_meter;
    float y = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float w = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    float h = (float)luaL_checknumber(L, 4) / pixels_per_meter;
    float angle = (float)luaL_checknumber(L, 5);
    luaL_checktype(L, 6, LUA_TTABLE);

    uint64_t mask = build_query_mask_from_table(L, 6);
    if (mask == 0) {
        lua_newtable(L);
        return 1;
    }

    QueryContext ctx = {0};
    ctx.tag_mask = mask;

    // Create rotated box corners
    float hw = w / 2.0f;
    float hh = h / 2.0f;
    b2Vec2 corners[4] = {
        {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
    };

    b2Vec2 position = {x, y};
    b2Rot rotation = b2MakeRot(angle);
    b2ShapeProxy proxy = b2MakeOffsetProxy(corners, 4, 0.0f, position, rotation);

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = UINT64_MAX;
    filter.maskBits = mask;

    b2World_OverlapShape(physics_world, &proxy, filter, query_overlap_callback, &ctx);

    lua_newtable(L);
    for (int i = 0; i < ctx.count; i++) {
        b2BodyId* ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
        *ud = ctx.bodies[i];
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// physics_query_capsule(x1, y1, x2, y2, radius, tags) -> array of bodies
static int l_physics_query_capsule(lua_State* L) {
    float x1 = (float)luaL_checknumber(L, 1) / pixels_per_meter;
    float y1 = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float x2 = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    float y2 = (float)luaL_checknumber(L, 4) / pixels_per_meter;
    float radius = (float)luaL_checknumber(L, 5) / pixels_per_meter;
    luaL_checktype(L, 6, LUA_TTABLE);

    uint64_t mask = build_query_mask_from_table(L, 6);
    if (mask == 0) {
        lua_newtable(L);
        return 1;
    }

    QueryContext ctx = {0};
    ctx.tag_mask = mask;

    // Capsule is two points with radius
    b2Vec2 points[2] = {{x1, y1}, {x2, y2}};
    b2ShapeProxy proxy = b2MakeProxy(points, 2, radius);

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = UINT64_MAX;
    filter.maskBits = mask;

    b2World_OverlapShape(physics_world, &proxy, filter, query_overlap_callback, &ctx);

    lua_newtable(L);
    for (int i = 0; i < ctx.count; i++) {
        b2BodyId* ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
        *ud = ctx.bodies[i];
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// physics_query_polygon(x, y, vertices, tags) -> array of bodies
// vertices is a flat array: {x1, y1, x2, y2, ...}
static int l_physics_query_polygon(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1) / pixels_per_meter;
    float y = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    luaL_checktype(L, 3, LUA_TTABLE);
    luaL_checktype(L, 4, LUA_TTABLE);

    // Get vertices from flat array {x1, y1, x2, y2, ...}
    int len = (int)lua_rawlen(L, 3);
    if (len < 6 || len % 2 != 0) {
        return luaL_error(L, "Polygon needs at least 3 vertices (6 numbers)");
    }

    int vertex_count = len / 2;
    if (vertex_count > B2_MAX_POLYGON_VERTICES) {
        return luaL_error(L, "Too many vertices (max %d)", B2_MAX_POLYGON_VERTICES);
    }

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];
    for (int i = 0; i < vertex_count; i++) {
        lua_rawgeti(L, 3, i * 2 + 1);
        lua_rawgeti(L, 3, i * 2 + 2);
        points[i].x = (float)lua_tonumber(L, -2) / pixels_per_meter;
        points[i].y = (float)lua_tonumber(L, -1) / pixels_per_meter;
        lua_pop(L, 2);
    }

    uint64_t mask = build_query_mask_from_table(L, 4);
    if (mask == 0) {
        lua_newtable(L);
        return 1;
    }

    QueryContext ctx = {0};
    ctx.tag_mask = mask;

    b2Vec2 position = {x, y};
    b2Rot rotation = b2Rot_identity;
    b2ShapeProxy proxy = b2MakeOffsetProxy(points, vertex_count, 0.0f, position, rotation);

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = UINT64_MAX;
    filter.maskBits = mask;

    b2World_OverlapShape(physics_world, &proxy, filter, query_overlap_callback, &ctx);

    lua_newtable(L);
    for (int i = 0; i < ctx.count; i++) {
        b2BodyId* ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
        *ud = ctx.bodies[i];
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// Raycast context for finding closest matching hit
typedef struct {
    b2ShapeId shape;
    b2Vec2 point;
    b2Vec2 normal;
    float fraction;
    bool hit;
    uint64_t tag_mask;
} RaycastClosestContext;

static float raycast_closest_callback(b2ShapeId shape_id, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
    RaycastClosestContext* ctx = (RaycastClosestContext*)context;

    // Check if this shape's tag matches our query
    ShapeUserData* sud = (ShapeUserData*)b2Shape_GetUserData(shape_id);
    PhysicsTag* tag = sud ? physics_tag_get(sud->tag_index) : NULL;
    if (!tag) return 1.0f;  // Continue

    if ((tag->category_bit & ctx->tag_mask) == 0) return 1.0f;  // Skip, continue

    // This hit matches - record it and clip the ray to this distance
    ctx->shape = shape_id;
    ctx->point = point;
    ctx->normal = normal;
    ctx->fraction = fraction;
    ctx->hit = true;

    return fraction;  // Clip ray to this distance (find closer matches only)
}

// Raycast context for collecting all hits
typedef struct {
    b2ShapeId shapes[MAX_QUERY_RESULTS];
    b2Vec2 points[MAX_QUERY_RESULTS];
    b2Vec2 normals[MAX_QUERY_RESULTS];
    float fractions[MAX_QUERY_RESULTS];
    int count;
    uint64_t tag_mask;
} RaycastAllContext;

static float raycast_all_callback(b2ShapeId shape_id, b2Vec2 point, b2Vec2 normal, float fraction, void* context) {
    RaycastAllContext* ctx = (RaycastAllContext*)context;
    if (ctx->count >= MAX_QUERY_RESULTS) return 0.0f;  // Stop

    // Check if this shape's tag matches our query
    ShapeUserData* sud = (ShapeUserData*)b2Shape_GetUserData(shape_id);
    PhysicsTag* tag = sud ? physics_tag_get(sud->tag_index) : NULL;
    if (!tag) return 1.0f;  // Continue

    if ((tag->category_bit & ctx->tag_mask) == 0) return 1.0f;  // Skip, continue

    ctx->shapes[ctx->count] = shape_id;
    ctx->points[ctx->count] = point;
    ctx->normals[ctx->count] = normal;
    ctx->fractions[ctx->count] = fraction;
    ctx->count++;

    return 1.0f;  // Continue to find all hits
}

// physics_raycast(x1, y1, x2, y2, tags) -> {body, shape, point_x, point_y, normal_x, normal_y, fraction} or nil
// Returns the closest hit that matches the tag filter
static int l_physics_raycast(lua_State* L) {
    float x1 = (float)luaL_checknumber(L, 1) / pixels_per_meter;
    float y1 = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float x2 = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    float y2 = (float)luaL_checknumber(L, 4) / pixels_per_meter;
    luaL_checktype(L, 5, LUA_TTABLE);

    uint64_t mask = build_query_mask_from_table(L, 5);
    if (mask == 0) {
        lua_pushnil(L);
        return 1;
    }

    RaycastClosestContext ctx = {0};
    ctx.tag_mask = mask;
    ctx.hit = false;

    b2Vec2 origin = {x1, y1};
    b2Vec2 translation = {x2 - x1, y2 - y1};

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = UINT64_MAX;
    filter.maskBits = mask;

    b2World_CastRay(physics_world, origin, translation, filter, raycast_closest_callback, &ctx);

    if (!ctx.hit) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);

    // body
    b2BodyId body = b2Shape_GetBody(ctx.shape);
    b2BodyId* body_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
    *body_ud = body;
    lua_setfield(L, -2, "body");

    // shape
    b2ShapeId* shape_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
    *shape_ud = ctx.shape;
    lua_setfield(L, -2, "shape");

    // point (convert back to pixels)
    lua_pushnumber(L, ctx.point.x * pixels_per_meter);
    lua_setfield(L, -2, "point_x");
    lua_pushnumber(L, ctx.point.y * pixels_per_meter);
    lua_setfield(L, -2, "point_y");

    // normal
    lua_pushnumber(L, ctx.normal.x);
    lua_setfield(L, -2, "normal_x");
    lua_pushnumber(L, ctx.normal.y);
    lua_setfield(L, -2, "normal_y");

    // fraction
    lua_pushnumber(L, ctx.fraction);
    lua_setfield(L, -2, "fraction");

    return 1;
}

// physics_raycast_all(x1, y1, x2, y2, tags) -> array of {body, shape, point_x, point_y, normal_x, normal_y, fraction}
static int l_physics_raycast_all(lua_State* L) {
    float x1 = (float)luaL_checknumber(L, 1) / pixels_per_meter;
    float y1 = (float)luaL_checknumber(L, 2) / pixels_per_meter;
    float x2 = (float)luaL_checknumber(L, 3) / pixels_per_meter;
    float y2 = (float)luaL_checknumber(L, 4) / pixels_per_meter;
    luaL_checktype(L, 5, LUA_TTABLE);

    uint64_t mask = build_query_mask_from_table(L, 5);
    if (mask == 0) {
        lua_newtable(L);
        return 1;
    }

    RaycastAllContext ctx = {0};
    ctx.tag_mask = mask;

    b2Vec2 origin = {x1, y1};
    b2Vec2 translation = {x2 - x1, y2 - y1};

    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = UINT64_MAX;
    filter.maskBits = mask;

    b2World_CastRay(physics_world, origin, translation, filter, raycast_all_callback, &ctx);

    lua_newtable(L);
    for (int i = 0; i < ctx.count; i++) {
        lua_newtable(L);

        // body
        b2BodyId body = b2Shape_GetBody(ctx.shapes[i]);
        b2BodyId* body_ud = (b2BodyId*)lua_newuserdata(L, sizeof(b2BodyId));
        *body_ud = body;
        lua_setfield(L, -2, "body");

        // shape
        b2ShapeId* shape_ud = (b2ShapeId*)lua_newuserdata(L, sizeof(b2ShapeId));
        *shape_ud = ctx.shapes[i];
        lua_setfield(L, -2, "shape");

        // point (convert back to pixels)
        lua_pushnumber(L, ctx.points[i].x * pixels_per_meter);
        lua_setfield(L, -2, "point_x");
        lua_pushnumber(L, ctx.points[i].y * pixels_per_meter);
        lua_setfield(L, -2, "point_y");

        // normal
        lua_pushnumber(L, ctx.normals[i].x);
        lua_setfield(L, -2, "normal_x");
        lua_pushnumber(L, ctx.normals[i].y);
        lua_setfield(L, -2, "normal_y");

        // fraction
        lua_pushnumber(L, ctx.fractions[i]);
        lua_setfield(L, -2, "fraction");

        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// ============================================================================
// LUA BINDINGS: RANDOM
// PCG32 RNG, distributions, Perlin noise
// ============================================================================

// Random Lua bindings
#define RNG_METATABLE "Anchor.RNG"

// Helper to get RNG from optional last argument, or global
static PCG32* get_rng(lua_State* L, int arg) {
    if (lua_isuserdata(L, arg)) {
        return (PCG32*)luaL_checkudata(L, arg, RNG_METATABLE);
    }
    return &global_rng;
}

// random_create(seed) - Create new RNG instance
static int l_random_create(lua_State* L) {
    lua_Integer seed = luaL_checkinteger(L, 1);
    PCG32* rng = (PCG32*)lua_newuserdata(L, sizeof(PCG32));
    pcg32_seed(rng, (uint64_t)seed);
    luaL_setmetatable(L, RNG_METATABLE);
    return 1;
}

// random_seed(seed, rng?) - Seed the RNG
static int l_random_seed(lua_State* L) {
    lua_Integer seed = luaL_checkinteger(L, 1);
    PCG32* rng = get_rng(L, 2);
    pcg32_seed(rng, (uint64_t)seed);
    return 0;
}

// random_get_seed(rng?) - Get the current seed
static int l_random_get_seed(lua_State* L) {
    PCG32* rng = get_rng(L, 1);
    lua_pushinteger(L, (lua_Integer)rng->seed);
    return 1;
}

// random_float_01(rng?) - Random float [0, 1]
static int l_random_float_01(lua_State* L) {
    PCG32* rng = get_rng(L, 1);
    uint32_t r = pcg32_next(rng);
    double result = (double)r / 4294967295.0;
    lua_pushnumber(L, result);
    return 1;
}

// random_float(min, max, rng?) - Random float [min, max]
static int l_random_float(lua_State* L) {
    double min = luaL_checknumber(L, 1);
    double max = luaL_checknumber(L, 2);
    PCG32* rng = get_rng(L, 3);
    uint32_t r = pcg32_next(rng);
    double t = (double)r / 4294967295.0;
    double result = min + t * (max - min);
    lua_pushnumber(L, result);
    return 1;
}

// random_int(min, max, rng?) - Random integer [min, max] inclusive
static int l_random_int(lua_State* L) {
    lua_Integer min = luaL_checkinteger(L, 1);
    lua_Integer max = luaL_checkinteger(L, 2);
    PCG32* rng = get_rng(L, 3);
    if (min > max) {
        lua_Integer tmp = min;
        min = max;
        max = tmp;
    }
    uint64_t range = (uint64_t)(max - min) + 1;
    uint32_t r = pcg32_next(rng);
    lua_Integer result = min + (lua_Integer)(r % range);
    lua_pushinteger(L, result);
    return 1;
}

// random_angle(rng?) - Random float [0, 2π]
static int l_random_angle(lua_State* L) {
    PCG32* rng = get_rng(L, 1);
    uint32_t r = pcg32_next(rng);
    double result = ((double)r / 4294967295.0) * 2.0 * PI;
    lua_pushnumber(L, result);
    return 1;
}

// random_sign(chance?, rng?) - Returns -1 or 1 (chance 0-100, default 50)
static int l_random_sign(lua_State* L) {
    double chance = 50.0;
    PCG32* rng = &global_rng;

    // Parse arguments: chance is optional number, rng is optional userdata
    if (lua_isnumber(L, 1)) {
        chance = lua_tonumber(L, 1);
        rng = get_rng(L, 2);
    } else if (lua_isuserdata(L, 1)) {
        rng = get_rng(L, 1);
    }

    uint32_t r = pcg32_next(rng);
    double roll = (double)r / 4294967295.0 * 100.0;
    lua_pushinteger(L, roll < chance ? 1 : -1);
    return 1;
}

// random_bool(chance?, rng?) - Returns true or false (chance 0-100, default 50)
static int l_random_bool(lua_State* L) {
    double chance = 50.0;
    PCG32* rng = &global_rng;

    // Parse arguments: chance is optional number, rng is optional userdata
    if (lua_isnumber(L, 1)) {
        chance = lua_tonumber(L, 1);
        rng = get_rng(L, 2);
    } else if (lua_isuserdata(L, 1)) {
        rng = get_rng(L, 1);
    }

    uint32_t r = pcg32_next(rng);
    double roll = (double)r / 4294967295.0 * 100.0;
    lua_pushboolean(L, roll < chance);
    return 1;
}

// random_normal(mean?, stddev?, rng?) - Gaussian distribution via Box-Muller transform
static int l_random_normal(lua_State* L) {
    double mean = 0.0;
    double stddev = 1.0;
    PCG32* rng = &global_rng;

    // Parse arguments
    int arg = 1;
    if (lua_isnumber(L, arg)) {
        mean = lua_tonumber(L, arg);
        arg++;
    }
    if (lua_isnumber(L, arg)) {
        stddev = lua_tonumber(L, arg);
        arg++;
    }
    if (lua_isuserdata(L, arg)) {
        rng = get_rng(L, arg);
    }

    // Box-Muller transform
    uint32_t r1 = pcg32_next(rng);
    uint32_t r2 = pcg32_next(rng);
    double u1 = ((double)r1 + 1.0) / 4294967297.0;  // (0, 1) exclusive to avoid log(0)
    double u2 = (double)r2 / 4294967295.0;
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
    double result = mean + z * stddev;
    lua_pushnumber(L, result);
    return 1;
}

// random_choice(array, rng?) - Pick one random element from array
static int l_random_choice(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    PCG32* rng = get_rng(L, 2);

    lua_len(L, 1);
    int len = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    if (len == 0) {
        lua_pushnil(L);
        return 1;
    }

    uint32_t r = pcg32_next(rng);
    int index = (int)(r % len) + 1;  // Lua arrays are 1-indexed
    lua_rawgeti(L, 1, index);
    return 1;
}

// random_choices(array, n, rng?) - Pick n random elements (unique indexes)
static int l_random_choices(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = (int)luaL_checkinteger(L, 2);
    PCG32* rng = get_rng(L, 3);

    lua_len(L, 1);
    int len = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    // Clamp n to array length
    if (n > len) n = len;
    if (n <= 0) {
        lua_newtable(L);
        return 1;
    }

    // Create result table
    lua_newtable(L);

    // Track used indexes with a simple array (for small n)
    // For larger selections, use Fisher-Yates on index array
    if (n <= 32 && len <= 1000) {
        // Simple rejection sampling for small n
        int used[32] = {0};
        int count = 0;
        int attempts = 0;
        while (count < n && attempts < n * 10) {
            uint32_t r = pcg32_next(rng);
            int index = (int)(r % len) + 1;
            int already_used = 0;
            for (int i = 0; i < count; i++) {
                if (used[i] == index) {
                    already_used = 1;
                    break;
                }
            }
            if (!already_used) {
                used[count] = index;
                lua_rawgeti(L, 1, index);
                lua_rawseti(L, -2, count + 1);
                count++;
            }
            attempts++;
        }
    } else {
        // For larger selections, build index array and shuffle first n
        int* indices = (int*)malloc(len * sizeof(int));
        for (int i = 0; i < len; i++) indices[i] = i + 1;

        // Partial Fisher-Yates: only shuffle first n elements
        for (int i = 0; i < n; i++) {
            uint32_t r = pcg32_next(rng);
            int j = i + (int)(r % (len - i));
            int tmp = indices[i];
            indices[i] = indices[j];
            indices[j] = tmp;

            lua_rawgeti(L, 1, indices[i]);
            lua_rawseti(L, -2, i + 1);
        }
        free(indices);
    }

    return 1;
}

// random_weighted(weights, rng?) - Returns index (1-based) based on weights
static int l_random_weighted(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    PCG32* rng = get_rng(L, 2);

    lua_len(L, 1);
    int len = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);

    if (len == 0) {
        lua_pushnil(L);
        return 1;
    }

    // Sum all weights
    double total = 0.0;
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(L, 1, i);
        total += lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    if (total <= 0.0) {
        lua_pushinteger(L, 1);
        return 1;
    }

    // Generate random value in [0, total)
    uint32_t r = pcg32_next(rng);
    double roll = ((double)r / 4294967295.0) * total;

    // Find which bucket it falls into
    double cumulative = 0.0;
    for (int i = 1; i <= len; i++) {
        lua_rawgeti(L, 1, i);
        cumulative += lua_tonumber(L, -1);
        lua_pop(L, 1);
        if (roll < cumulative) {
            lua_pushinteger(L, i);
            return 1;
        }
    }

    // Fallback to last index (handles floating point edge cases)
    lua_pushinteger(L, len);
    return 1;
}

// noise(x, y?, z?) - Perlin noise [-1, 1]
static int l_random_noise(lua_State* L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_optnumber(L, 2, 0.0);
    float z = (float)luaL_optnumber(L, 3, 0.0);

    // stb_perlin_noise3 returns [-1, 1]
    float result = stb_perlin_noise3(x, y, z, 0, 0, 0);
    lua_pushnumber(L, result);
    return 1;
}

// ============================================================================
// LUA BINDINGS: INPUT
// Keyboard, mouse, gamepad, actions, chords, sequences, holds, capture
// ============================================================================

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
    float scale_x = (float)window_w / game_width;
    float scale_y = (float)window_h / game_height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1.0f) scale = 1.0f;

    lua_pushnumber(L, (float)mouse_dx / scale);
    lua_pushnumber(L, (float)mouse_dy / scale);
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

static int l_input_is_down(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, input_is_down(name));  // Checks both actions and chords
    return 1;
}

static int l_input_is_pressed(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, input_is_pressed(name));  // Checks both actions and chords
    return 1;
}

static int l_input_is_released(lua_State* L) {
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

// ============================================================================
// ENGINE STATE GETTERS
// Expose engine state to Lua
// ============================================================================

// Forward declarations for main loop state (defined below)
static Uint64 frame;
static Uint64 step;
static double game_time;
static double fps;

static int l_engine_get_frame(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)frame);
    return 1;
}

static int l_engine_get_step(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)step);
    return 1;
}

static int l_engine_get_time(lua_State* L) {
    lua_pushnumber(L, game_time);
    return 1;
}

static int l_engine_get_dt(lua_State* L) {
    lua_pushnumber(L, PHYSICS_RATE * time_scale);
    return 1;
}

static int l_engine_get_unscaled_dt(lua_State* L) {
    lua_pushnumber(L, PHYSICS_RATE);
    return 1;
}

static int l_engine_get_time_scale(lua_State* L) {
    lua_pushnumber(L, time_scale);
    return 1;
}

static int l_engine_set_time_scale(lua_State* L) {
    time_scale = luaL_checknumber(L, 1);
    return 0;
}

static int l_engine_get_width(lua_State* L) {
    lua_pushinteger(L, game_width);
    return 1;
}

static int l_engine_get_height(lua_State* L) {
    lua_pushinteger(L, game_height);
    return 1;
}

static int l_engine_get_window_size(lua_State* L) {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}

static int l_engine_get_scale(lua_State* L) {
    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);
    float scale_x = (float)window_w / game_width;
    float scale_y = (float)window_h / game_height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (scale < 1.0f) scale = 1.0f;
    lua_pushnumber(L, scale);
    return 1;
}

static int l_engine_is_fullscreen(lua_State* L) {
    Uint32 flags = SDL_GetWindowFlags(window);
    lua_pushboolean(L, (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0);
    return 1;
}

static int l_engine_get_platform(lua_State* L) {
#ifdef __EMSCRIPTEN__
    lua_pushstring(L, "web");
#else
    lua_pushstring(L, "windows");
#endif
    return 1;
}

static int l_engine_get_fps(lua_State* L) {
    lua_pushnumber(L, fps);
    return 1;
}

static int l_engine_get_draw_calls(lua_State* L) {
    lua_pushinteger(L, draw_calls);
    return 1;
}

static int l_perf_time(lua_State* L) {
    lua_pushnumber(L, (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency());
    return 1;
}

// ============================================================================
// ENGINE CONFIGURATION (called before engine_init)
// ============================================================================

static bool engine_initialized = false;

static int l_engine_set_game_size(lua_State* L) {
    if (engine_initialized) {
        return luaL_error(L, "engine_set_game_size must be called before engine_init");
    }
    game_width = luaL_checkinteger(L, 1);
    game_height = luaL_checkinteger(L, 2);
    return 0;
}

static int l_engine_set_title(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    strncpy(window_title, title, sizeof(window_title) - 1);
    window_title[sizeof(window_title) - 1] = '\0';
    // If window already exists, update title immediately
    if (window) {
        SDL_SetWindowTitle(window, window_title);
    }
    return 0;
}

static int l_engine_set_scale(lua_State* L) {
    if (engine_initialized) {
        return luaL_error(L, "engine_set_scale must be called before engine_init");
    }
    initial_scale = (float)luaL_checknumber(L, 1);
    return 0;
}

static int l_engine_set_vsync(lua_State* L) {
    vsync_enabled = lua_toboolean(L, 1);
    // If window already exists, apply immediately
    if (window) {
        SDL_GL_SetSwapInterval(vsync_enabled ? 1 : 0);
    }
    return 0;
}

static int l_engine_set_fullscreen(lua_State* L) {
    start_fullscreen = lua_toboolean(L, 1);
    return 0;
}

static int l_engine_set_resizable(lua_State* L) {
    if (engine_initialized) {
        return luaL_error(L, "engine_set_resizable must be called before engine_init");
    }
    window_resizable = lua_toboolean(L, 1);
    return 0;
}

static int l_engine_set_headless(lua_State* L) {
    if (engine_initialized) {
        return luaL_error(L, "engine_set_headless must be called before engine_init");
    }
    headless_mode = lua_toboolean(L, 1);
    return 0;
}

static int l_engine_get_headless(lua_State* L) {
    lua_pushboolean(L, headless_mode);
    return 1;
}

static int l_engine_get_render_mode(lua_State* L) {
    lua_pushboolean(L, render_mode);
    return 1;
}

static int l_engine_render_setup(lua_State* L) {
    const char* dir = luaL_checkstring(L, 1);
    strncpy(capture_output_dir, dir, sizeof(capture_output_dir) - 1);
    capture_output_dir[sizeof(capture_output_dir) - 1] = '\0';
    capture_frame_number = 0;

    // Create capture FBO at native game resolution
    glGenFramebuffers(1, &capture_fbo);
    glGenTextures(1, &capture_texture);

    glBindTexture(GL_TEXTURE_2D, capture_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, game_width, game_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, capture_texture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return luaL_error(L, "Capture FBO incomplete: 0x%x", status);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Allocate pixel read buffer
    capture_buffer = (unsigned char*)malloc(game_width * game_height * 4);
    if (!capture_buffer) {
        return luaL_error(L, "Failed to allocate capture buffer");
    }

    printf("Render capture setup: %dx%d -> %s\n", game_width, game_height, capture_output_dir);
    return 0;
}

static int l_engine_render_save_frame(lua_State* L) {
    if (!capture_fbo || !capture_buffer) {
        return luaL_error(L, "Render capture not set up (call engine_render_setup first)");
    }

    // Composite layers to capture FBO at native resolution
    glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
    glViewport(0, 0, game_width, game_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(screen_shader);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLint offset_loc = glGetUniformLocation(screen_shader, "u_offset");

    if (layer_draw_count > 0) {
        for (int i = 0; i < layer_draw_count; i++) {
            LayerDrawCommand* cmd = &layer_draw_queue[i];
            Layer* layer = cmd->layer;

            float ndc_x = (cmd->x / game_width) * 2.0f;
            float ndc_y = -(cmd->y / game_height) * 2.0f;
            glUniform2f(offset_loc, ndc_x, ndc_y);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer));

            glBindVertexArray(screen_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    } else {
        glUniform2f(offset_loc, 0.0f, 0.0f);
        for (int i = 0; i < layer_count; i++) {
            Layer* layer = layer_registry[i];
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer));

            glBindVertexArray(screen_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    }

    // Restore blend mode
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Read pixels from capture FBO
    glReadPixels(0, 0, game_width, game_height, GL_RGBA, GL_UNSIGNED_BYTE, capture_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Flip vertically (OpenGL reads bottom-up, PNG expects top-down)
    int row_bytes = game_width * 4;
    unsigned char* temp_row = (unsigned char*)malloc(row_bytes);
    for (int y = 0; y < game_height / 2; y++) {
        unsigned char* top = capture_buffer + y * row_bytes;
        unsigned char* bot = capture_buffer + (game_height - 1 - y) * row_bytes;
        memcpy(temp_row, top, row_bytes);
        memcpy(top, bot, row_bytes);
        memcpy(bot, temp_row, row_bytes);
    }
    free(temp_row);

    // Write PNG
    char filename[600];
    snprintf(filename, sizeof(filename), "%s/frame_%06d.png", capture_output_dir, capture_frame_number);
    stbi_write_png(filename, game_width, game_height, 4, capture_buffer, game_width * 4);

    lua_pushinteger(L, capture_frame_number);
    capture_frame_number++;
    return 1;
}

// Live recording: pipe raw frames to ffmpeg for real-time encoding
static int l_engine_record_start(lua_State* L) {
    const char* output_path = luaL_checkstring(L, 1);

    // Set up capture FBO if not already done
    if (!capture_fbo) {
        glGenFramebuffers(1, &capture_fbo);
        glGenTextures(1, &capture_texture);
        glBindTexture(GL_TEXTURE_2D, capture_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, game_width, game_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, capture_texture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    if (!capture_buffer) {
        capture_buffer = (unsigned char*)malloc(game_width * game_height * 4);
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pixel_format rgba -video_size %dx%d -framerate 60 "
        "-i - -vf \"vflip,scale=1920:1080:flags=neighbor\" "
        "-c:v libx264 -preset fast -crf 18 -pix_fmt yuv420p \"%s\" 2>nul",
        game_width, game_height, output_path);

    record_pipe = _popen(cmd, "wb");
    if (!record_pipe) {
        return luaL_error(L, "Failed to open ffmpeg pipe for recording");
    }

    printf("Live recording started: %dx%d -> %s\n", game_width, game_height, output_path);
    return 0;
}

static int l_engine_record_frame(lua_State* L) {
    if (!record_pipe || !capture_fbo || !capture_buffer) {
        return luaL_error(L, "Recording not started (call engine_record_start first)");
    }

    // Composite layers to capture FBO at native resolution
    glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
    glViewport(0, 0, game_width, game_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(screen_shader);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLint offset_loc = glGetUniformLocation(screen_shader, "u_offset");

    if (layer_draw_count > 0) {
        for (int i = 0; i < layer_draw_count; i++) {
            LayerDrawCommand* cmd = &layer_draw_queue[i];
            Layer* layer = cmd->layer;
            float ndc_x = (cmd->x / game_width) * 2.0f;
            float ndc_y = -(cmd->y / game_height) * 2.0f;
            glUniform2f(offset_loc, ndc_x, ndc_y);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer));
            glBindVertexArray(screen_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    } else {
        glUniform2f(offset_loc, 0.0f, 0.0f);
        for (int i = 0; i < layer_count; i++) {
            Layer* layer = layer_registry[i];
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer));
            glBindVertexArray(screen_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    }

    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Read pixels (no vertical flip needed — ffmpeg vflip handles it)
    glReadPixels(0, 0, game_width, game_height, GL_RGBA, GL_UNSIGNED_BYTE, capture_buffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Write raw pixels to pipe
    fwrite(capture_buffer, 1, game_width * game_height * 4, record_pipe);

    return 0;
}

static int l_engine_record_stop(lua_State* L) {
    (void)L;
    if (record_pipe) {
        _pclose(record_pipe);
        record_pipe = NULL;
        printf("Live recording stopped\n");
    }
    return 0;
}

static int l_engine_get_args(lua_State* L) {
    lua_newtable(L);
    for (int i = 0; i < cli_arg_count; i++) {
        lua_pushstring(L, cli_args[i].value);
        lua_setfield(L, -2, cli_args[i].key);
    }
    return 1;
}

static int l_engine_quit(lua_State* L) {
    (void)L;
    running = false;
    return 0;
}

// engine_init: Creates window and initializes graphics
// Must be called from Lua (via framework) after configuration is set
static int l_engine_init(lua_State* L) {
    if (engine_initialized) {
        return luaL_error(L, "engine_init can only be called once");
    }

    if (headless_mode) {
        // Headless: skip all graphics initialization
        printf("Headless mode: skipping window and graphics initialization\n");
        engine_initialized = true;
        printf("Engine initialized (headless): %dx%d\n", game_width, game_height);
        return 0;
    }

    // Build window flags
    Uint32 window_flags = SDL_WINDOW_OPENGL;
    if (window_resizable) {
        window_flags |= SDL_WINDOW_RESIZABLE;
    }
    if (start_fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    window = SDL_CreateWindow(
        window_title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)(game_width * initial_scale), (int)(game_height * initial_scale),
        window_flags
    );
    if (!window) {
        return luaL_error(L, "SDL_CreateWindow failed: %s", SDL_GetError());
    }

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        return luaL_error(L, "SDL_GL_CreateContext failed: %s", SDL_GetError());
    }

    SDL_GL_SetSwapInterval((vsync_enabled && !render_mode) ? 1 : 0);

    #ifndef __EMSCRIPTEN__
    // Load OpenGL functions (desktop only - Emscripten provides them)
    int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
    if (version == 0) {
        return luaL_error(L, "gladLoadGL failed");
    }
    printf("OpenGL %d.%d loaded\n", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
    #else
    printf("WebGL 2.0 (OpenGL ES 3.0) context created\n");
    #endif
    printf("Renderer: %s\n", glGetString(GL_RENDERER));

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Create shader program
    shader_program = create_shader_program(vertex_shader_source, fragment_shader_source);
    if (!shader_program) {
        return luaL_error(L, "Failed to create shader program");
    }
    printf("Shader program created\n");

    // Set up VAO and VBO for dynamic quad rendering
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_BATCH_VERTICES * VERTEX_FLOATS * sizeof(float), NULL, GL_DYNAMIC_DRAW);

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

    // Shape attributes (locations 4-8): 5 vec4s at offsets 9, 13, 17, 21, 25
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)(13 * sizeof(float)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)(17 * sizeof(float)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, stride, (void*)(21 * sizeof(float)));
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, (void*)(25 * sizeof(float)));
    glEnableVertexAttribArray(8);

    // AddColor attribute (location 9): 3 floats at offset 29
    glVertexAttribPointer(9, 3, GL_FLOAT, GL_FALSE, stride, (void*)(29 * sizeof(float)));
    glEnableVertexAttribArray(9);

    glBindVertexArray(0);
    printf("Game VAO/VBO created (stride=%d bytes)\n", stride);

    // Create screen shader for blitting layers
    screen_shader = create_shader_program(screen_vertex_source, screen_fragment_source);
    if (!screen_shader) {
        return luaL_error(L, "Failed to create screen shader");
    }
    printf("Screen shader created\n");

    // Set up screen quad VAO/VBO
    float screen_vertices[] = {
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

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    printf("Screen VAO/VBO created\n");

    engine_initialized = true;
    printf("Engine initialized: %dx%d @ %.1fx scale\n", game_width, game_height, initial_scale);

    return 0;
}

// ============================================================================
// SYSTEM: Clipboard, Global Hotkeys, Process Execution
// Platform-specific system utilities exposed to Lua
// ============================================================================

// --- Clipboard (cross-platform via SDL) ---

static int l_clipboard_get(lua_State* L) {
    if (SDL_HasClipboardText()) {
        char* text = SDL_GetClipboardText();
        if (text && text[0] != '\0') {
            lua_pushstring(L, text);
            SDL_free(text);
            return 1;
        }
        if (text) SDL_free(text);
    }
    lua_pushnil(L);
    return 1;
}

static int l_clipboard_set(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    int result = SDL_SetClipboardText(text);
    lua_pushboolean(L, result == 0);
    return 1;
}

static int l_clipboard_has_text(lua_State* L) {
    lua_pushboolean(L, SDL_HasClipboardText());
    return 1;
}

// --- Global Hotkeys (Windows only) ---
// Uses Win32 RegisterHotKey for system-wide hotkeys that work even when unfocused.
// Must poll BEFORE SDL_PollEvent to grab WM_HOTKEY thread messages first.

#ifdef _WIN32
static void hotkey_poll_events(void) {
    MSG msg;
    while (PeekMessage(&msg, (HWND)-1, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {
        for (int i = 0; i < global_hotkey_count; i++) {
            if (global_hotkeys[i].id == (int)msg.wParam) {
                global_hotkeys[i].fired = true;
            }
        }
    }
}

static void hotkey_cleanup(void) {
    for (int i = 0; i < global_hotkey_count; i++) {
        if (global_hotkeys[i].registered) {
            UnregisterHotKey(NULL, global_hotkeys[i].id);
        }
    }
    global_hotkey_count = 0;
}

// hotkey_register(id, modifiers, vk_code) -> bool
// modifiers: 1=MOD_ALT, 2=MOD_CONTROL, 4=MOD_SHIFT, 8=MOD_WIN
static int l_hotkey_register(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    int modifiers = (int)luaL_checkinteger(L, 2);
    int vk = (int)luaL_checkinteger(L, 3);

    if (global_hotkey_count >= MAX_GLOBAL_HOTKEYS) {
        lua_pushboolean(L, 0);
        return 1;
    }

    BOOL result = RegisterHotKey(NULL, id, modifiers | MOD_NOREPEAT, vk);
    lua_pushboolean(L, result);

    if (result) {
        global_hotkeys[global_hotkey_count].id = id;
        global_hotkeys[global_hotkey_count].fired = false;
        global_hotkeys[global_hotkey_count].registered = true;
        global_hotkey_count++;
    }

    return 1;
}

static int l_hotkey_unregister(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    UnregisterHotKey(NULL, id);
    for (int i = 0; i < global_hotkey_count; i++) {
        if (global_hotkeys[i].id == id) {
            global_hotkeys[i].registered = false;
            for (int j = i; j < global_hotkey_count - 1; j++) {
                global_hotkeys[j] = global_hotkeys[j + 1];
            }
            global_hotkey_count--;
            break;
        }
    }
    return 0;
}

static int l_hotkey_is_pressed(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    for (int i = 0; i < global_hotkey_count; i++) {
        if (global_hotkeys[i].id == id) {
            lua_pushboolean(L, global_hotkeys[i].fired);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}
#endif // _WIN32

// --- Process Execution (desktop only) ---

#ifndef __EMSCRIPTEN__
// os_popen(command) -> output_string, exit_status
static int l_os_popen(lua_State* L) {
    const char* command = luaL_checkstring(L, 1);

    #ifdef _WIN32
    FILE* pipe = _popen(command, "r");
    #else
    FILE* pipe = popen(command, "r");
    #endif

    if (!pipe) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to execute command");
        return 2;
    }

    luaL_Buffer b;
    luaL_buffinit(L, &b);
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        luaL_addstring(&b, buffer);
    }

    #ifdef _WIN32
    int status = _pclose(pipe);
    #else
    int status = pclose(pipe);
    #endif

    luaL_pushresult(&b);
    lua_pushinteger(L, status);
    return 2;
}
#endif // !__EMSCRIPTEN__

// ============================================================================
// LUA REGISTRATION
// Binds all C functions to Lua global namespace
// ============================================================================

static void register_lua_bindings(lua_State* L) {
    // Create RNG metatable (for random_create instances)
    luaL_newmetatable(L, RNG_METATABLE);
    lua_pop(L, 1);

    // --- Layer & Texture ---
    lua_register(L, "layer_create", l_layer_create);
    lua_register(L, "layer_rectangle", l_layer_rectangle);
    lua_register(L, "layer_rectangle_line", l_layer_rectangle_line);
    lua_register(L, "layer_rectangle_gradient_h", l_layer_rectangle_gradient_h);
    lua_register(L, "layer_rectangle_gradient_v", l_layer_rectangle_gradient_v);
    lua_register(L, "layer_circle", l_layer_circle);
    lua_register(L, "layer_circle_line", l_layer_circle_line);
    lua_register(L, "layer_line", l_layer_line);
    lua_register(L, "layer_capsule", l_layer_capsule);
    lua_register(L, "layer_capsule_line", l_layer_capsule_line);
    lua_register(L, "layer_triangle", l_layer_triangle);
    lua_register(L, "layer_triangle_line", l_layer_triangle_line);
    lua_register(L, "layer_polygon", l_layer_polygon);
    lua_register(L, "layer_polygon_line", l_layer_polygon_line);
    lua_register(L, "layer_rounded_rectangle", l_layer_rounded_rectangle);
    lua_register(L, "layer_rounded_rectangle_line", l_layer_rounded_rectangle_line);
    lua_register(L, "layer_push", l_layer_push);
    lua_register(L, "layer_pop", l_layer_pop);
    lua_register(L, "layer_draw_texture", l_layer_draw_texture);
    lua_register(L, "layer_set_blend_mode", l_layer_set_blend_mode);
    lua_register(L, "layer_stencil_mask", l_layer_stencil_mask);
    lua_register(L, "layer_stencil_test", l_layer_stencil_test);
    lua_register(L, "layer_stencil_test_inverse", l_layer_stencil_test_inverse);
    lua_register(L, "layer_stencil_off", l_layer_stencil_off);
    lua_register(L, "texture_load", l_texture_load);
    lua_register(L, "texture_create", l_texture_create);
    lua_register(L, "texture_unload", l_texture_unload);
    lua_register(L, "texture_get_width", l_texture_get_width);
    lua_register(L, "texture_get_height", l_texture_get_height);
    // --- Spritesheet ---
    lua_register(L, "spritesheet_load", l_spritesheet_load);
    lua_register(L, "spritesheet_get_frame_width", l_spritesheet_get_frame_width);
    lua_register(L, "spritesheet_get_frame_height", l_spritesheet_get_frame_height);
    lua_register(L, "spritesheet_get_total_frames", l_spritesheet_get_total_frames);
    lua_register(L, "layer_draw_spritesheet_frame", l_layer_draw_spritesheet_frame);
    // --- Font ---
    lua_register(L, "font_load", l_font_load);
    lua_register(L, "font_unload", l_font_unload);
    lua_register(L, "font_get_height", l_font_get_height);
    lua_register(L, "font_get_text_width", l_font_get_text_width);
    lua_register(L, "font_get_char_width", l_font_get_char_width);
    lua_register(L, "font_get_glyph_metrics", l_font_get_glyph_metrics);
    lua_register(L, "layer_draw_text", l_layer_draw_text);
    lua_register(L, "layer_draw_glyph", l_layer_draw_glyph);
    // --- Audio ---
    lua_register(L, "sound_load", l_sound_load);
    lua_register(L, "sound_play", l_sound_play);
    lua_register(L, "sound_play_handle", l_sound_play_handle);
    lua_register(L, "sound_handle_set_pitch", l_sound_handle_set_pitch);
    lua_register(L, "sound_handle_set_volume", l_sound_handle_set_volume);
    lua_register(L, "sound_handle_stop", l_sound_handle_stop);
    lua_register(L, "sound_handle_set_looping", l_sound_handle_set_looping);
    lua_register(L, "sound_set_volume", l_sound_set_volume);
    lua_register(L, "music_load", l_music_load);
    lua_register(L, "music_play", l_music_play);
    lua_register(L, "music_stop", l_music_stop);
    lua_register(L, "music_set_volume", l_music_set_volume);
    lua_register(L, "music_is_playing", l_music_is_playing);
    lua_register(L, "music_at_end", l_music_at_end);
    lua_register(L, "music_get_position", l_music_get_position);
    lua_register(L, "music_get_duration", l_music_get_duration);
    lua_register(L, "music_get_volume", l_music_get_volume);
    lua_register(L, "audio_set_master_pitch", l_audio_set_master_pitch);
    lua_register(L, "rgba", l_color_rgba);
    lua_register(L, "set_filter_mode", l_set_filter_mode);
    lua_register(L, "get_filter_mode", l_get_filter_mode);
    lua_register(L, "timing_resync", l_timing_resync);
    // --- Effect Shaders ---
    lua_register(L, "set_draw_shader", l_set_draw_shader);
    lua_register(L, "get_draw_shader", l_get_draw_shader);
    lua_register(L, "shader_load_file", l_shader_load_file);
    lua_register(L, "shader_load_string", l_shader_load_string);
    lua_register(L, "shader_destroy", l_shader_destroy);
    lua_register(L, "shader_set_float_immediate", l_shader_set_float_immediate);
    lua_register(L, "shader_set_vec2_immediate", l_shader_set_vec2_immediate);
    lua_register(L, "shader_set_vec4_immediate", l_shader_set_vec4_immediate);
    lua_register(L, "shader_set_int_immediate", l_shader_set_int_immediate);
    lua_register(L, "layer_shader_set_float", l_layer_shader_set_float);
    lua_register(L, "layer_shader_set_vec2", l_layer_shader_set_vec2);
    lua_register(L, "layer_shader_set_vec4", l_layer_shader_set_vec4);
    lua_register(L, "layer_shader_set_int", l_layer_shader_set_int);
    lua_register(L, "layer_shader_set_texture", l_layer_shader_set_texture);
    lua_register(L, "layer_apply_shader", l_layer_apply_shader);
    lua_register(L, "layer_draw", l_layer_draw);
    lua_register(L, "layer_get_texture", l_layer_get_texture);
    lua_register(L, "layer_reset_effects", l_layer_reset_effects);
    lua_register(L, "layer_clear", l_layer_clear);
    lua_register(L, "layer_render", l_layer_render);
    lua_register(L, "layer_draw_from", l_layer_draw_from);
    // --- Physics: World & Bodies ---
    lua_register(L, "physics_init", l_physics_init);
    lua_register(L, "physics_set_gravity", l_physics_set_gravity);
    lua_register(L, "physics_set_meter_scale", l_physics_set_meter_scale);
    lua_register(L, "physics_set_enabled", l_physics_set_enabled);
    lua_register(L, "physics_register_tag", l_physics_register_tag);
    lua_register(L, "physics_enable_collision", l_physics_enable_collision);
    lua_register(L, "physics_disable_collision", l_physics_disable_collision);
    lua_register(L, "physics_enable_sensor", l_physics_enable_sensor);
    lua_register(L, "physics_enable_hit", l_physics_enable_hit);
    lua_register(L, "physics_tags_collide", l_physics_tags_collide);
    lua_register(L, "physics_create_body", l_physics_create_body);
    lua_register(L, "physics_destroy_body", l_physics_destroy_body);
    lua_register(L, "physics_get_position", l_physics_get_position);
    lua_register(L, "physics_get_angle", l_physics_get_angle);
    lua_register(L, "physics_get_body_count", l_physics_get_body_count);
    lua_register(L, "physics_body_is_valid", l_physics_body_is_valid);
    lua_register(L, "physics_add_circle", l_physics_add_circle);
    lua_register(L, "physics_add_box", l_physics_add_box);
    lua_register(L, "physics_add_capsule", l_physics_add_capsule);
    lua_register(L, "physics_add_polygon", l_physics_add_polygon);
    lua_register(L, "physics_add_chain", l_physics_add_chain);
    // --- Physics: Body Properties ---
    lua_register(L, "physics_set_position", l_physics_set_position);
    lua_register(L, "physics_set_angle", l_physics_set_angle);
    lua_register(L, "physics_set_transform", l_physics_set_transform);
    lua_register(L, "physics_get_velocity", l_physics_get_velocity);
    lua_register(L, "physics_get_angular_velocity", l_physics_get_angular_velocity);
    lua_register(L, "physics_set_velocity", l_physics_set_velocity);
    lua_register(L, "physics_set_angular_velocity", l_physics_set_angular_velocity);
    lua_register(L, "physics_apply_force", l_physics_apply_force);
    lua_register(L, "physics_apply_force_at", l_physics_apply_force_at);
    lua_register(L, "physics_apply_impulse", l_physics_apply_impulse);
    lua_register(L, "physics_apply_impulse_at", l_physics_apply_impulse_at);
    lua_register(L, "physics_apply_torque", l_physics_apply_torque);
    lua_register(L, "physics_apply_angular_impulse", l_physics_apply_angular_impulse);
    lua_register(L, "physics_set_linear_damping", l_physics_set_linear_damping);
    lua_register(L, "physics_set_angular_damping", l_physics_set_angular_damping);
    lua_register(L, "physics_set_gravity_scale", l_physics_set_gravity_scale);
    lua_register(L, "physics_set_fixed_rotation", l_physics_set_fixed_rotation);
    lua_register(L, "physics_set_bullet", l_physics_set_bullet);
    lua_register(L, "physics_set_user_data", l_physics_set_user_data);
    lua_register(L, "physics_get_user_data", l_physics_get_user_data);
    // --- Physics: Shape Properties ---
    lua_register(L, "physics_shape_set_friction", l_physics_shape_set_friction);
    lua_register(L, "physics_shape_get_friction", l_physics_shape_get_friction);
    lua_register(L, "physics_shape_set_restitution", l_physics_shape_set_restitution);
    lua_register(L, "physics_shape_get_restitution", l_physics_shape_get_restitution);
    lua_register(L, "physics_shape_is_valid", l_physics_shape_is_valid);
    lua_register(L, "physics_shape_get_body", l_physics_shape_get_body);
    lua_register(L, "physics_shape_set_density", l_physics_shape_set_density);
    lua_register(L, "physics_shape_get_density", l_physics_shape_get_density);
    lua_register(L, "physics_shape_destroy", l_physics_shape_destroy);
    lua_register(L, "physics_shape_set_filter_group", l_physics_shape_set_filter_group);
    // --- Physics: Queries ---
    lua_register(L, "physics_get_body_type", l_physics_get_body_type);
    lua_register(L, "physics_get_mass", l_physics_get_mass);
    lua_register(L, "physics_set_center_of_mass", l_physics_set_center_of_mass);
    lua_register(L, "physics_is_awake", l_physics_is_awake);
    lua_register(L, "physics_set_awake", l_physics_set_awake);
    lua_register(L, "physics_get_shapes_geometry", l_physics_get_shapes_geometry);
    lua_register(L, "physics_debug_events", l_physics_debug_events);
    // --- Physics: Events ---
    lua_register(L, "physics_get_collision_begin", l_physics_get_collision_begin);
    lua_register(L, "physics_get_collision_end", l_physics_get_collision_end);
    lua_register(L, "physics_get_hit", l_physics_get_hit);
    lua_register(L, "physics_get_sensor_begin", l_physics_get_sensor_begin);
    lua_register(L, "physics_get_sensor_end", l_physics_get_sensor_end);
    // --- Physics: Spatial Queries & Raycast ---
    lua_register(L, "physics_query_point", l_physics_query_point);
    lua_register(L, "physics_query_circle", l_physics_query_circle);
    lua_register(L, "physics_query_aabb", l_physics_query_aabb);
    lua_register(L, "physics_query_box", l_physics_query_box);
    lua_register(L, "physics_query_capsule", l_physics_query_capsule);
    lua_register(L, "physics_query_polygon", l_physics_query_polygon);
    lua_register(L, "physics_raycast", l_physics_raycast);
    lua_register(L, "physics_raycast_all", l_physics_raycast_all);
    // --- Random ---
    lua_register(L, "random_create", l_random_create);
    lua_register(L, "random_seed", l_random_seed);
    lua_register(L, "random_get_seed", l_random_get_seed);
    lua_register(L, "random_float_01", l_random_float_01);
    lua_register(L, "random_float", l_random_float);
    lua_register(L, "random_int", l_random_int);
    lua_register(L, "random_angle", l_random_angle);
    lua_register(L, "random_sign", l_random_sign);
    lua_register(L, "random_bool", l_random_bool);
    lua_register(L, "random_normal", l_random_normal);
    lua_register(L, "random_choice", l_random_choice);
    lua_register(L, "random_choices", l_random_choices);
    lua_register(L, "random_weighted", l_random_weighted);
    lua_register(L, "noise", l_random_noise);
    // --- Input: Keyboard ---
    lua_register(L, "key_is_down", l_key_is_down);
    lua_register(L, "key_is_pressed", l_key_is_pressed);
    lua_register(L, "key_is_released", l_key_is_released);
    // --- Input: Mouse ---
    lua_register(L, "mouse_position", l_mouse_position);
    lua_register(L, "mouse_delta", l_mouse_delta);
    lua_register(L, "mouse_set_visible", l_mouse_set_visible);
    lua_register(L, "mouse_set_grabbed", l_mouse_set_grabbed);
    lua_register(L, "mouse_is_down", l_mouse_is_down);
    lua_register(L, "mouse_is_pressed", l_mouse_is_pressed);
    lua_register(L, "mouse_is_released", l_mouse_is_released);
    lua_register(L, "mouse_wheel", l_mouse_wheel);
    // --- Input: Action Binding ---
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
    lua_register(L, "is_down", l_input_is_down);
    lua_register(L, "is_pressed", l_input_is_pressed);
    lua_register(L, "is_released", l_input_is_released);
    lua_register(L, "input_any_pressed", l_input_any_pressed);
    lua_register(L, "input_get_pressed_action", l_input_get_pressed_action);
    // --- Input: Gamepad ---
    lua_register(L, "gamepad_is_connected", l_gamepad_is_connected);
    lua_register(L, "gamepad_get_axis", l_gamepad_get_axis);
    // --- Engine State ---
    lua_register(L, "engine_get_frame", l_engine_get_frame);
    lua_register(L, "engine_get_step", l_engine_get_step);
    lua_register(L, "engine_get_time", l_engine_get_time);
    lua_register(L, "engine_get_dt", l_engine_get_dt);
    lua_register(L, "engine_get_unscaled_dt", l_engine_get_unscaled_dt);
    lua_register(L, "engine_get_time_scale", l_engine_get_time_scale);
    lua_register(L, "engine_set_time_scale", l_engine_set_time_scale);
    lua_register(L, "engine_get_width", l_engine_get_width);
    lua_register(L, "engine_get_height", l_engine_get_height);
    lua_register(L, "engine_get_window_size", l_engine_get_window_size);
    lua_register(L, "engine_get_scale", l_engine_get_scale);
    lua_register(L, "engine_is_fullscreen", l_engine_is_fullscreen);
    lua_register(L, "engine_get_platform", l_engine_get_platform);
    lua_register(L, "engine_get_fps", l_engine_get_fps);
    lua_register(L, "engine_get_draw_calls", l_engine_get_draw_calls);
    lua_register(L, "perf_time", l_perf_time);
    // --- Engine Configuration ---
    lua_register(L, "engine_set_game_size", l_engine_set_game_size);
    lua_register(L, "engine_set_title", l_engine_set_title);
    lua_register(L, "engine_set_scale", l_engine_set_scale);
    lua_register(L, "engine_set_vsync", l_engine_set_vsync);
    lua_register(L, "engine_set_fullscreen", l_engine_set_fullscreen);
    lua_register(L, "engine_set_resizable", l_engine_set_resizable);
    lua_register(L, "engine_set_headless", l_engine_set_headless);
    lua_register(L, "engine_get_headless", l_engine_get_headless);
    lua_register(L, "engine_get_render_mode", l_engine_get_render_mode);
    lua_register(L, "engine_render_setup", l_engine_render_setup);
    lua_register(L, "engine_render_save_frame", l_engine_render_save_frame);
    lua_register(L, "engine_record_start", l_engine_record_start);
    lua_register(L, "engine_record_frame", l_engine_record_frame);
    lua_register(L, "engine_record_stop", l_engine_record_stop);
    lua_register(L, "engine_get_args", l_engine_get_args);
    lua_register(L, "engine_quit", l_engine_quit);
    lua_register(L, "engine_init", l_engine_init);
    // --- System: Clipboard ---
    lua_register(L, "clipboard_get", l_clipboard_get);
    lua_register(L, "clipboard_set", l_clipboard_set);
    lua_register(L, "clipboard_has_text", l_clipboard_has_text);
    // --- System: Global Hotkeys (Windows only) ---
    #ifdef _WIN32
    lua_register(L, "hotkey_register", l_hotkey_register);
    lua_register(L, "hotkey_unregister", l_hotkey_unregister);
    lua_register(L, "hotkey_is_pressed", l_hotkey_is_pressed);
    #endif
    // --- System: Process Execution ---
    #ifndef __EMSCRIPTEN__
    lua_register(L, "os_popen", l_os_popen);
    #endif
}

// Main loop state (needed for emscripten)
// Note: 'running' is declared at file scope (near headless_mode) so engine_quit() can access it
static Uint64 perf_freq = 0;
static Uint64 last_time = 0;
static double physics_lag = 0.0;
static double render_lag = 0.0;
static Uint64 step = 0;
static double game_time = 0.0;
static Uint64 frame = 0;
static double fps = 0.0;

// VSync snap frequencies (computed at init based on display refresh rate)
static double snap_frequencies[8];
static int snap_frequency_count = 0;

// Delta time averaging (smooths out OS scheduling jitter)
#define DT_HISTORY_COUNT 4
static double dt_history[DT_HISTORY_COUNT] = {0};
static int dt_history_index = 0;
static bool dt_history_filled = false;

// Reset timing accumulators (call on focus gain, scene transitions, etc.)
// This prevents accumulated lag from causing catch-up updates
static void timing_resync(void) {
    physics_lag = 0.0;
    render_lag = 0.0;
    last_time = SDL_GetPerformanceCounter();
    // Reset dt averaging
    for (int i = 0; i < DT_HISTORY_COUNT; i++) dt_history[i] = 0;
    dt_history_index = 0;
    dt_history_filled = false;
}

// ============================================================================
// SHADER SOURCES & COMPILATION
// GLSL source strings, compile/link utilities, effect shader loading
// ============================================================================

// Shader headers - prepended to all shaders based on platform
#ifdef __EMSCRIPTEN__
    #define SHADER_HEADER_VERT "#version 300 es\n"
    #define SHADER_HEADER_FRAG "#version 300 es\nprecision mediump float;\n"
#else
    #define SHADER_HEADER_VERT "#version 330 core\n"
    #define SHADER_HEADER_FRAG "#version 330 core\n"
#endif

// Shader sources (no version line - header prepended at compile time)
const char* vertex_shader_source =
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aUV;\n"
    "layout (location = 2) in vec4 aColor;\n"
    "layout (location = 3) in float aType;\n"
    "layout (location = 4) in vec4 aShape0;\n"
    "layout (location = 5) in vec4 aShape1;\n"
    "layout (location = 6) in vec4 aShape2;\n"
    "layout (location = 7) in vec4 aShape3;\n"
    "layout (location = 8) in vec4 aShape4;\n"
    "layout (location = 9) in vec3 aAddColor;\n"
    "\n"
    "out vec2 vPos;\n"
    "out vec2 vUV;\n"
    "out vec4 vColor;\n"
    "out float vType;\n"
    "out vec4 vShape0;\n"
    "out vec4 vShape1;\n"
    "out vec4 vShape2;\n"
    "out vec4 vShape3;\n"
    "out vec4 vShape4;\n"
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
    "    vShape0 = aShape0;\n"
    "    vShape1 = aShape1;\n"
    "    vShape2 = aShape2;\n"
    "    vShape3 = aShape3;\n"
    "    vShape4 = aShape4;\n"
    "    vAddColor = aAddColor;\n"
    "}\n";

const char* fragment_shader_source =
    "in vec2 vPos;\n"
    "in vec2 vUV;\n"
    "in vec4 vColor;\n"
    "in float vType;\n"
    "in vec4 vShape0;\n"
    "in vec4 vShape1;\n"
    "in vec4 vShape2;\n"
    "in vec4 vShape3;\n"
    "in vec4 vShape4;\n"
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
    "// SDF for rounded rectangle in local space\n"
    "float sdf_rounded_rect(vec2 p, vec2 center, vec2 half_size, float radius) {\n"
    "    vec2 d = abs(p - center) - half_size + radius;\n"
    "    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radius;\n"
    "}\n"
    "\n"
    "// SDF for circle in local space\n"
    "float sdf_circle(vec2 p, vec2 center, float radius) {\n"
    "    return length(p - center) - radius;\n"
    "}\n"
    "\n"
    "// SDF for line segment / capsule (with round caps)\n"
    "float sdf_capsule(vec2 p, vec2 a, vec2 b, float radius) {\n"
    "    vec2 pa = p - a, ba = b - a;\n"
    "    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);\n"
    "    return length(pa - ba * h) - radius;\n"
    "}\n"
    "\n"
    "// SDF for triangle\n"
    "float sdf_triangle(vec2 p, vec2 p0, vec2 p1, vec2 p2) {\n"
    "    vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;\n"
    "    vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;\n"
    "    vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);\n"
    "    vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);\n"
    "    vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);\n"
    "    float s = sign(e0.x * e2.y - e0.y * e2.x);\n"
    "    vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),\n"
    "                     vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),\n"
    "                     vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));\n"
    "    return -sqrt(d.x) * sign(d.y);\n"
    "}\n"
    "\n"
    "// SDF for convex polygon (up to 8 vertices)\n"
    "float sdf_polygon(vec2 p, vec2 v[8], int n) {\n"
    "    float d = dot(p - v[0], p - v[0]);\n"
    "    float s = 1.0;\n"
    "    for (int i = 0, j = n - 1; i < n; j = i, i++) {\n"
    "        vec2 e = v[j] - v[i];\n"
    "        vec2 w = p - v[i];\n"
    "        vec2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);\n"
    "        d = min(d, dot(b, b));\n"
    "        bvec3 c = bvec3(p.y >= v[i].y, p.y < v[j].y, e.x * w.y > e.y * w.x);\n"
    "        if (all(c) || all(not(c))) s *= -1.0;\n"
    "    }\n"
    "    return s * sqrt(d);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    float d;\n"
    "    float stroke = 0.0;\n"
    "    \n"
    "    // UV-space SDF approach:\n"
    "    // vShape0.xy = quad size in local space\n"
    "    // vUV * quad_size = position in local quad space\n"
    "    // center = quad_size * 0.5 (shape is always centered in quad)\n"
    "    // This handles rotation correctly because UV interpolation\n"
    "    // implicitly provides the inverse rotation.\n"
    "    \n"
    "    if (vType < 0.5) {\n"
    "        // Rectangle: shape0 = (quad_w, quad_h, half_w, half_h), shape1.x = stroke\n"
    "        vec2 quad_size = vShape0.xy;\n"
    "        vec2 local_p = vUV * quad_size;\n"
    "        vec2 center = quad_size * 0.5;\n"
    "        vec2 half_size = vShape0.zw;\n"
    "        stroke = vShape1.x;\n"
    "        \n"
    "        // In rough mode, snap to local pixel grid\n"
    "        if (u_aa_width == 0.0) {\n"
    "            local_p = floor(local_p) + 0.5;\n"
    "        }\n"
    "        \n"
    "        d = sdf_rect(local_p, center, half_size);\n"
    "    } else if (vType < 1.5) {\n"
    "        // Circle: shape0 = (quad_size, quad_size, radius, stroke)\n"
    "        float quad_size = vShape0.x;\n"
    "        vec2 local_p = vUV * quad_size;\n"
    "        vec2 center = vec2(quad_size * 0.5);\n"
    "        float radius = vShape0.z;\n"
    "        stroke = vShape0.w;\n"
    "        // Snap radius for consistent shape\n"
    "        if (u_aa_width == 0.0) {\n"
    "            radius = floor(radius + 0.5);\n"
    "        }\n"
    "        d = sdf_circle(local_p, center, radius);\n"
    "    } else if (vType < 2.5) {\n"
    "        // Sprite: sample texture at texel centers for pixel-perfect rendering\n"
    "        // vColor is multiply (tint), vAddColor is additive (flash)\n"
    "        ivec2 texSize = textureSize(u_texture, 0);\n"
    "        vec2 snappedUV = (floor(vUV * vec2(texSize)) + 0.5) / vec2(texSize);\n"
    "        vec4 texColor = texture(u_texture, snappedUV);\n"
    "        FragColor = vec4(texColor.rgb * vColor.rgb + vAddColor, texColor.a * vColor.a);\n"
    "        return;\n"
    "    } else if (vType < 3.5) {\n"
    "        // Line/Capsule: shape0 = (quad_w, quad_h, half_len, radius), shape1.x = stroke\n"
    "        vec2 quad_size = vShape0.xy;\n"
    "        vec2 local_p = vUV * quad_size;\n"
    "        vec2 center = quad_size * 0.5;\n"
    "        float half_len = vShape0.z;\n"
    "        float radius = vShape0.w;\n"
    "        stroke = vShape1.x;\n"
    "        // Capsule endpoints are horizontal, centered in quad\n"
    "        vec2 a = center - vec2(half_len, 0.0);\n"
    "        vec2 b = center + vec2(half_len, 0.0);\n"
    "        d = sdf_capsule(local_p, a, b, radius);\n"
    "    } else if (vType < 4.5) {\n"
    "        // Triangle: shape0 = (quad_w, quad_h, stroke, _), shape1 = (v0.x, v0.y, v1.x, v1.y), shape2.xy = (v2.x, v2.y)\n"
    "        vec2 quad_size = vShape0.xy;\n"
    "        vec2 local_p = vUV * quad_size;\n"
    "        stroke = vShape0.z;\n"
    "        vec2 v0 = vShape1.xy;\n"
    "        vec2 v1 = vShape1.zw;\n"
    "        vec2 v2 = vShape2.xy;\n"
    "        d = sdf_triangle(local_p, v0, v1, v2);\n"
    "    } else if (vType < 5.5) {\n"
    "        // Polygon: shape0.x = n, shape0.y = stroke, shape0.zw = v0, shape1 = (v1, v2), shape2 = (v3, v4), shape3 = (v5, v6), shape4.xy = v7\n"
    "        int n = int(vShape0.x);\n"
    "        stroke = vShape0.y;\n"
    "        vec2 quad_size = vShape0.zw; // Use shape1.xy for quad_size\n"
    "        // Actually, for polygon we store vertices directly and compute quad from them\n"
    "        // Re-read: shape0 = (n, stroke, quad_w, quad_h), then vertices in shape1-4\n"
    "        // Let's adjust: polygon uses quad_size from shape0.zw\n"
    "        vec2 local_p = vUV * quad_size;\n"
    "        vec2 v[8];\n"
    "        v[0] = vShape1.xy;\n"
    "        v[1] = vShape1.zw;\n"
    "        v[2] = vShape2.xy;\n"
    "        v[3] = vShape2.zw;\n"
    "        v[4] = vShape3.xy;\n"
    "        v[5] = vShape3.zw;\n"
    "        v[6] = vShape4.xy;\n"
    "        v[7] = vShape4.zw;\n"
    "        d = sdf_polygon(local_p, v, n);\n"
    "    } else if (vType < 6.5) {\n"
    "        // Rounded Rectangle: shape0 = (quad_w, quad_h, half_w, half_h), shape1 = (radius, stroke, _, _)\n"
    "        vec2 quad_size = vShape0.xy;\n"
    "        vec2 local_p = vUV * quad_size;\n"
    "        vec2 center = quad_size * 0.5;\n"
    "        vec2 half_size = vShape0.zw;\n"
    "        float radius = vShape1.x;\n"
    "        stroke = vShape1.y;\n"
    "        d = sdf_rounded_rect(local_p, center, half_size, radius);\n"
    "    } else {\n"
    "        discard;\n"
    "    }\n"
    "    \n"
    "    // Apply stroke (outline) if stroke > 0\n"
    "    if (stroke > 0.0) {\n"
    "        d = abs(d) - stroke * 0.5;\n"
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

const char* screen_vertex_source =
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "uniform vec2 u_offset;\n"  // Offset in NDC (-1 to 1 range)
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos + u_offset, 0.0, 1.0);\n"
    "    TexCoord = aTexCoord;\n"
    "}\n";

const char* screen_fragment_source =
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
// Read entire file into malloc'd string (caller must free) - supports zip archive
static char* read_file_to_string(const char* path) {
    size_t size;
    char* buffer = (char*)zip_read_file(path, &size);
    if (!buffer) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }
    // Ensure null termination (realloc to add space for null if needed)
    char* result = (char*)realloc(buffer, size + 1);
    if (!result) {
        free(buffer);
        return NULL;
    }
    result[size] = '\0';
    return result;
}

// Create an effect shader program from fragment source (uses screen_vertex_source)
static GLuint effect_shader_load_string(const char* frag_source) {
    return create_shader_program(screen_vertex_source, frag_source);
}

// Replace the default draw shader with a custom fragment shader (uses the same vertex shader).
// The projection matrix and AA width are set every frame in the render loop, so they'll
// automatically apply to the new shader.
static GLuint custom_draw_shader = 0;
static int l_set_draw_shader(lua_State* L) {
    if (headless_mode) return 0;
    const char* path = luaL_checkstring(L, 1);
    char* source = read_file_to_string(path);
    if (!source) return luaL_error(L, "Failed to read draw shader: %s", path);
    GLuint shader = create_shader_program(vertex_shader_source, source);
    free(source);
    if (!shader) return luaL_error(L, "Failed to compile draw shader: %s", path);
    if (custom_draw_shader) glDeleteProgram(custom_draw_shader);
    custom_draw_shader = shader;
    shader_program = shader;
    printf("Custom draw shader loaded: %s\n", path);
    return 0;
}

// Get the current draw shader ID (so Lua can set uniforms on it via layer_shader_set_*)
static int l_get_draw_shader(lua_State* L) {
    lua_pushinteger(L, (lua_Integer)shader_program);
    return 1;
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

// ============================================================================
// MAIN LOOP & INITIALIZATION
// Engine lifecycle: startup, frame iteration, shutdown
// ============================================================================

// Error handler that adds stack trace
static int traceback(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    luaL_traceback(L, L, msg, 1);
    return 1;
}

// Custom Lua module searcher that loads from embedded zip
// Called by require() - tries module.lua and module/init.lua
static int zip_searcher(lua_State* L) {
    const char* modname = luaL_checkstring(L, 1);

    // Convert module name to path
    // Note: PowerShell's Compress-Archive creates zips with backslash paths on Windows,
    // so we try both forward slashes and backslashes
    char path[512];
    size_t len = strlen(modname);
    if (len >= sizeof(path) - 15) {
        lua_pushnil(L);
        return 1;
    }

    size_t script_size;
    char* script_data = NULL;

    // Try modname.lua with forward slashes (e.g., "anchor.lua")
    strcpy(path, modname);
    for (char* p = path; *p; p++) {
        if (*p == '.') *p = '/';
    }
    strcat(path, ".lua");
    script_data = (char*)zip_read_file(path, &script_size);

    // Try modname.lua with backslashes (e.g., "anchor.lua" -> no change for single name)
    if (!script_data) {
        strcpy(path, modname);
        for (char* p = path; *p; p++) {
            if (*p == '.') *p = '\\';
        }
        strcat(path, ".lua");
        script_data = (char*)zip_read_file(path, &script_size);
    }

    // Try modname/init.lua with forward slashes (e.g., "anchor/init.lua")
    if (!script_data) {
        strcpy(path, modname);
        for (char* p = path; *p; p++) {
            if (*p == '.') *p = '/';
        }
        strcat(path, "/init.lua");
        script_data = (char*)zip_read_file(path, &script_size);
    }

    // Try modname\init.lua with backslashes (e.g., "anchor\init.lua")
    if (!script_data) {
        strcpy(path, modname);
        for (char* p = path; *p; p++) {
            if (*p == '.') *p = '\\';
        }
        strcat(path, "\\init.lua");
        script_data = (char*)zip_read_file(path, &script_size);
    }

    if (!script_data) {
        // Not found - return nil and error message
        lua_pushnil(L);
        lua_pushfstring(L, "no file '%s.lua' or '%s/init.lua' in embedded zip", modname, modname);
        return 2;
    }

    // Found - compile and return the loader
    char chunkname[520];
    snprintf(chunkname, sizeof(chunkname), "@%s", path);
    if (luaL_loadbuffer(L, script_data, script_size, chunkname) != LUA_OK) {
        free(script_data);
        return lua_error(L);  // Propagate compile error
    }
    free(script_data);

    // Return the compiled chunk (loader function)
    lua_pushstring(L, path);  // Second return value: file path
    return 2;
}

// Register the zip searcher as the first searcher in package.searchers
static void register_zip_searcher(lua_State* L) {
    // Get package.searchers table
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "searchers");

    // Shift existing searchers down (1->2, 2->3, etc.)
    int n = (int)lua_rawlen(L, -1);
    for (int i = n; i >= 1; i--) {
        lua_rawgeti(L, -1, i);
        lua_rawseti(L, -2, i + 1);
    }

    // Insert zip_searcher at position 1
    lua_pushcfunction(L, zip_searcher);
    lua_rawseti(L, -2, 1);

    lua_pop(L, 2);  // Pop searchers and package
}

static void engine_shutdown(void) {
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
    // Textures
    for (int i = 0; i < texture_count; i++) {
        texture_destroy(texture_registry[i]);
        texture_registry[i] = NULL;
    }
    texture_count = 0;
    // Effect shaders
    for (int i = 0; i < effect_shader_count; i++) {
        effect_shader_destroy(effect_shader_registry[i]);
        effect_shader_registry[i] = 0;
    }
    effect_shader_count = 0;
    // Screen blit resources
    if (screen_vbo) { glDeleteBuffers(1, &screen_vbo); screen_vbo = 0; }
    if (screen_vao) { glDeleteVertexArrays(1, &screen_vao); screen_vao = 0; }
    if (screen_shader) { glDeleteProgram(screen_shader); screen_shader = 0; }
    // Audio
    if (audio_initialized) {
        sound_cleanup_all();
        ma_engine_uninit(&audio_engine);
        audio_initialized = false;
    }
    // Physics
    if (physics_initialized) {
        b2DestroyWorld(physics_world);
        physics_initialized = false;
        shape_user_data_count = 0;
    }
    // Global hotkeys
    #ifdef _WIN32
    hotkey_cleanup();
    #endif
    // Other resources
    if (L) { lua_close(L); L = NULL; }
    if (gl_context) { SDL_GL_DeleteContext(gl_context); gl_context = NULL; }
    if (window) { SDL_DestroyWindow(window); window = NULL; }
    SDL_Quit();
    // Zip archive cleanup
    zip_shutdown();
}

// One frame of the main loop
static void main_loop_iteration(void) {
    Uint64 current_time = SDL_GetPerformanceCounter();
    double dt = (double)(current_time - last_time) / (double)perf_freq;
    last_time = current_time;

    // Clean up finished sounds (must be done from main thread)
    sound_cleanup_finished();

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

    // Delta time averaging: smooth out OS scheduling jitter
    // A single slow frame gets spread across multiple frames instead of spiking
    dt_history[dt_history_index] = dt;
    dt_history_index = (dt_history_index + 1) % DT_HISTORY_COUNT;
    if (dt_history_index == 0) dt_history_filled = true;

    double averaged_dt;
    if (dt_history_filled) {
        // Full buffer: average all values
        averaged_dt = 0;
        for (int i = 0; i < DT_HISTORY_COUNT; i++) {
            averaged_dt += dt_history[i];
        }
        averaged_dt /= DT_HISTORY_COUNT;
    } else {
        // Buffer not full yet: average only filled entries
        averaged_dt = 0;
        for (int i = 0; i < dt_history_index; i++) {
            averaged_dt += dt_history[i];
        }
        averaged_dt /= dt_history_index > 0 ? dt_history_index : 1;
    }
    dt = averaged_dt;

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

    // Poll global hotkeys BEFORE SDL events (grab WM_HOTKEY thread messages first)
    #ifdef _WIN32
    hotkey_poll_events();
    #endif

    // Process events every frame (not tied to fixed timestep)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        }
        // Track keyboard state
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            last_input_type = INPUT_TYPE_KEYBOARD;
            #ifdef __EMSCRIPTEN__
            audio_try_unlock();
            #endif
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
            #ifdef __EMSCRIPTEN__
            audio_try_unlock();
            #endif
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
        // Touch events (for web/mobile audio unlock)
        #ifdef __EMSCRIPTEN__
        if (event.type == SDL_FINGERDOWN) {
            audio_try_unlock();
        }
        #endif
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

        // Step physics world (uses time_scale, so 0 during hitstop)
        if (physics_initialized && physics_enabled) {
            physics_clear_events();  // Clear event buffers before step
            b2World_Step(physics_world, (float)(PHYSICS_RATE * time_scale), 4);  // 4 sub-steps recommended
            physics_process_events();  // Buffer events for Lua queries
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

        // Copy current input state to previous for next frame's edge detection
        input_post_update();
        chords_post_update();
    }

    // Render at 60Hz (for chunky pixel movement on high-refresh monitors)
    if (render_lag >= RENDER_RATE) {
        render_lag -= RENDER_RATE;
        frame++;
        draw_calls = 0;  // Reset draw call counter for this frame

        // Calculate FPS from dt_history average
        if (dt_history_filled) {
            double avg_dt = 0;
            for (int i = 0; i < DT_HISTORY_COUNT; i++) avg_dt += dt_history[i];
            avg_dt /= DT_HISTORY_COUNT;
            fps = (avg_dt > 0) ? 1.0 / avg_dt : 0;
        }

        // Set up orthographic projection (game coordinates)
        // Maps (0,0) at top-left to (width, height) at bottom-right
        float projection[16] = {
            2.0f / game_width, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / game_height, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f
        };

        glUseProgram(shader_program);
        GLint proj_loc = glGetUniformLocation(shader_program, "projection");
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);

        // Set AA width based on filter mode (0 = rough/hard edges, 1 = smooth)
        GLint aa_loc = glGetUniformLocation(shader_program, "u_aa_width");
        float aa_width = (filter_mode == FILTER_SMOOTH) ? 1.0f : 0.0f;
        glUniform1f(aa_loc, aa_width);

        // === PASS 1: Call Lua draw() function ===
        // User's draw() handles: rendering layers, creating derived layers, compositing
        glBindTexture(GL_TEXTURE_2D, 0);  // Unbind to avoid feedback loop

        if (!error_state) {
            lua_getglobal(L, "draw");
            if (lua_isfunction(L, -1)) {
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    fprintf(stderr, "Lua draw() error: %s\n", err);
                    lua_pop(L, 1);
                    error_state = true;
                }
            } else {
                lua_pop(L, 1);
                // No draw() function defined - that's an error now
                fprintf(stderr, "Error: No draw() function defined in Lua\n");
                error_state = true;
            }
        }

        // === PASS 2: Composite all layers to screen ===
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Get current window size
        int window_w, window_h;
        SDL_GetWindowSize(window, &window_w, &window_h);

        // Calculate scale to fit window while maintaining aspect ratio
        // Calculate scale to fit window while maintaining aspect ratio
        float scale_x = (float)window_w / game_width;
        float scale_y = (float)window_h / game_height;
        float scale = (scale_x < scale_y) ? scale_x : scale_y;
        if (scale < 1.0f) scale = 1.0f;

        // Calculate centered position with letterboxing
        int scaled_w = (int)(game_width * scale);
        int scaled_h = (int)(game_height * scale);
        int offset_x = (window_w - scaled_w) / 2;
        int offset_y = (window_h - scaled_h) / 2;

        // Clear screen to black (letterbox color)
        glViewport(0, 0, window_w, window_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Set viewport for game area
        glViewport(offset_x, offset_y, scaled_w, scaled_h);
        glUseProgram(screen_shader);

        // Use premultiplied alpha blend for compositing layers to screen
        // FBO contents are already blended, so we don't multiply by src alpha again
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

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
                float ndc_x = (cmd->x / game_width) * 2.0f;
                float ndc_y = -(cmd->y / game_height) * 2.0f;  // Flip Y
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

        // Restore standard alpha blend for next frame's drawing
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,  // RGB
                            GL_ONE, GL_ONE_MINUS_SRC_ALPHA);       // Alpha

        SDL_GL_SwapWindow(window);
    }

    #ifdef __EMSCRIPTEN__
    if (!running) {
        emscripten_cancel_main_loop();
        engine_shutdown();
    }
    #endif
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("Anchor Engine starting...\n");

    // Try to load embedded zip from executable (for distribution)
    // This must happen before changing working directory
    if (zip_init(argv[0])) {
        printf("Running from packaged executable\n");
    }

    // Parse CLI arguments: first positional arg is game folder, --key=value are stored
    {
        const char* game_folder = NULL;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--headless") == 0) {
                headless_mode = true;
                printf("Headless mode enabled\n");
            } else if (strcmp(argv[i], "--render") == 0) {
                render_mode = true;
                printf("Render mode enabled\n");
            } else if (strncmp(argv[i], "--", 2) == 0 && cli_arg_count < MAX_CLI_ARGS) {
                // Parse --key=value or --key value
                const char* arg = argv[i] + 2;  // skip "--"
                const char* eq = strchr(arg, '=');
                if (eq) {
                    int key_len = (int)(eq - arg);
                    if (key_len >= MAX_CLI_KEY) key_len = MAX_CLI_KEY - 1;
                    strncpy(cli_args[cli_arg_count].key, arg, key_len);
                    cli_args[cli_arg_count].key[key_len] = '\0';
                    strncpy(cli_args[cli_arg_count].value, eq + 1, MAX_CLI_VALUE - 1);
                    cli_args[cli_arg_count].value[MAX_CLI_VALUE - 1] = '\0';
                } else {
                    strncpy(cli_args[cli_arg_count].key, arg, MAX_CLI_KEY - 1);
                    cli_args[cli_arg_count].key[MAX_CLI_KEY - 1] = '\0';
                    // Use "true" as default value for flags without =
                    strncpy(cli_args[cli_arg_count].value, "true", MAX_CLI_VALUE - 1);
                }
                cli_arg_count++;
            } else if (!game_folder && !zip_initialized) {
                game_folder = argv[i];
            }
        }
        if (game_folder) {
            #ifdef _WIN32
            _chdir(game_folder);
            #else
            chdir(game_folder);
            #endif
            printf("Game folder: %s\n", game_folder);
        }
    }

    printf("Loading: main.lua\n");

    // Initialize SDL (headless only needs timer, not video/audio/gamepad)
    {
        Uint32 sdl_flags = headless_mode ? 0 : (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);
        if (SDL_Init(sdl_flags) < 0) {
            fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return 1;
        }
    }

    if (!headless_mode) {
        // Set OpenGL attributes (before window creation)
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
    }

    // Initialize Lua (before window so game can configure via engine_set_* functions)
    L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "luaL_newstate failed\n");
        engine_shutdown();
        return 1;
    }
    luaL_openlibs(L);
    register_lua_bindings(L);
    register_zip_searcher(L);  // Enable require() from embedded zip

    // Initialize gamepad (check for already-connected controllers) — skip in headless
    if (!headless_mode) {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                gamepad = SDL_GameControllerOpen(i);
                if (gamepad) {
                    printf("Gamepad found at startup: %s\n", SDL_GameControllerName(gamepad));
                    break;  // Only use first gamepad
                }
            }
        }
    }

    // Initialize audio (miniaudio) — skip in headless mode
    if (!headless_mode) {
        ma_result result = ma_engine_init(NULL, &audio_engine);
        if (result != MA_SUCCESS) {
            fprintf(stderr, "Failed to initialize audio engine: %d\n", result);
            // Continue without audio - not a fatal error
        } else {
            audio_initialized = true;
            printf("Audio engine initialized\n");
        }
    }

    // Load and run main.lua (this should call engine_init via framework)
    lua_pushcfunction(L, traceback);
    int err_handler = lua_gettop(L);

    size_t script_size;
    char* script_data = (char*)zip_read_file("main.lua", &script_size);
    if (!script_data) {
        snprintf(error_message, sizeof(error_message), "Failed to read main.lua");
        fprintf(stderr, "ERROR: %s\n", error_message);
        lua_pop(L, 1);  // traceback
        error_state = true;
    } else if (luaL_loadbuffer(L, script_data, script_size, "@main.lua") != LUA_OK) {
        snprintf(error_message, sizeof(error_message), "%s", lua_tostring(L, -1));
        fprintf(stderr, "ERROR: %s\n", error_message);
        lua_pop(L, 2);  // error + traceback
        free(script_data);
        error_state = true;
    } else {
        free(script_data);
        if (lua_pcall(L, 0, 0, err_handler) != LUA_OK) {
            snprintf(error_message, sizeof(error_message), "%s", lua_tostring(L, -1));
            fprintf(stderr, "ERROR: %s\n", error_message);
            lua_pop(L, 2);  // error + traceback
            error_state = true;
        } else {
            lua_pop(L, 1);  // traceback
        }
    }

    // Check that engine_init was called by the game/framework
    if (!engine_initialized && !error_state) {
        snprintf(error_message, sizeof(error_message), "engine_init() was not called. Did you forget to require 'anchor'?");
        fprintf(stderr, "ERROR: %s\n", error_message);
        error_state = true;
    }

    printf("Initialization complete. Press ESC to exit, F11 for fullscreen.\n");

    // Initialize timing state (not needed in headless but harmless)
    perf_freq = SDL_GetPerformanceFrequency();
    last_time = SDL_GetPerformanceCounter();

    if (!headless_mode) {
        // Initialize vsync snap frequencies based on display refresh rate
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
    if (headless_mode) {
        // Headless: tight update loop — no timing, no rendering, max speed
        printf("Headless loop starting...\n");
        lua_pushcfunction(L, traceback);
        int err_handler = lua_gettop(L);
        while (running && !error_state) {
            // Clear layer commands (update code may call draw functions)
            for (int i = 0; i < layer_count; i++) {
                if (layer_registry[i]->commands) {
                    layer_registry[i]->command_count = 0;
                }
            }
            // Step physics
            if (physics_initialized && physics_enabled) {
                physics_clear_events();
                b2World_Step(physics_world, (float)(PHYSICS_RATE * time_scale), 4);
                physics_process_events();
            }
            // Call Lua update(dt)
            lua_getglobal(L, "update");
            if (lua_isfunction(L, -1)) {
                lua_pushnumber(L, PHYSICS_RATE);
                if (lua_pcall(L, 1, 0, err_handler) != LUA_OK) {
                    snprintf(error_message, sizeof(error_message), "%s", lua_tostring(L, -1));
                    fprintf(stderr, "ERROR: %s\n", error_message);
                    lua_pop(L, 1);
                    error_state = true;
                }
            } else {
                lua_pop(L, 1);
            }
            step++;
            game_time += PHYSICS_RATE;
            // Post-update input state (needed for edge detection even if no real input)
            input_post_update();
        }
        lua_pop(L, 1);  // traceback
    } else if (render_mode) {
        // Render mode: deterministic loop — 2 physics steps per render frame, no real-time timing
        printf("Render loop starting...\n");
        lua_pushcfunction(L, traceback);
        int err_handler = lua_gettop(L);
        while (running && !error_state) {
            // Drain SDL events (so window stays responsive / closable)
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                }
            }

            // 2 physics steps per render frame (120Hz physics / 60Hz render)
            for (int p = 0; p < 2 && running && !error_state; p++) {
                // Clear layer commands
                for (int i = 0; i < layer_count; i++) {
                    layer_clear_commands(layer_registry[i]);
                }

                // Step physics
                if (physics_initialized && physics_enabled) {
                    physics_clear_events();
                    b2World_Step(physics_world, (float)(PHYSICS_RATE * time_scale), 4);
                    physics_process_events();
                }

                // Call Lua update(dt)
                lua_getglobal(L, "update");
                if (lua_isfunction(L, -1)) {
                    lua_pushnumber(L, PHYSICS_RATE);
                    if (lua_pcall(L, 1, 0, err_handler) != LUA_OK) {
                        snprintf(error_message, sizeof(error_message), "%s", lua_tostring(L, -1));
                        fprintf(stderr, "ERROR: %s\n", error_message);
                        lua_pop(L, 1);
                        error_state = true;
                    }
                } else {
                    lua_pop(L, 1);
                }

                step++;
                game_time += PHYSICS_RATE;
                input_post_update();
            }

            if (!running || error_state) break;

            // Increment frame before draw so an.frame is correct
            frame++;
            draw_calls = 0;

            // Set up orthographic projection
            float projection[16] = {
                2.0f / game_width, 0.0f, 0.0f, 0.0f,
                0.0f, -2.0f / game_height, 0.0f, 0.0f,
                0.0f, 0.0f, -1.0f, 0.0f,
                -1.0f, 1.0f, 0.0f, 1.0f
            };
            glUseProgram(shader_program);
            GLint proj_loc = glGetUniformLocation(shader_program, "projection");
            glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);

            GLint aa_loc = glGetUniformLocation(shader_program, "u_aa_width");
            float aa_width = (filter_mode == FILTER_SMOOTH) ? 1.0f : 0.0f;
            glUniform1f(aa_loc, aa_width);

            // Call Lua draw() — populates layer FBOs and layer_draw_queue
            glBindTexture(GL_TEXTURE_2D, 0);
            lua_getglobal(L, "draw");
            if (lua_isfunction(L, -1)) {
                if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                    const char* err = lua_tostring(L, -1);
                    fprintf(stderr, "Lua draw() error: %s\n", err);
                    lua_pop(L, 1);
                    error_state = true;
                }
            } else {
                lua_pop(L, 1);
            }

            if (error_state) break;

            // Capture pass: composite to capture FBO at native resolution, save PNG
            if (capture_fbo && capture_buffer) {
                glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
                glViewport(0, 0, game_width, game_height);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                glUseProgram(screen_shader);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                GLint cap_offset_loc = glGetUniformLocation(screen_shader, "u_offset");

                if (layer_draw_count > 0) {
                    for (int i = 0; i < layer_draw_count; i++) {
                        LayerDrawCommand* cmd = &layer_draw_queue[i];
                        float ndc_x = (cmd->x / game_width) * 2.0f;
                        float ndc_y = -(cmd->y / game_height) * 2.0f;
                        glUniform2f(cap_offset_loc, ndc_x, ndc_y);

                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, layer_get_texture(cmd->layer));
                        glBindVertexArray(screen_vao);
                        glDrawArrays(GL_TRIANGLES, 0, 6);
                        glBindVertexArray(0);
                    }
                } else {
                    glUniform2f(cap_offset_loc, 0.0f, 0.0f);
                    for (int i = 0; i < layer_count; i++) {
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer_registry[i]));
                        glBindVertexArray(screen_vao);
                        glDrawArrays(GL_TRIANGLES, 0, 6);
                        glBindVertexArray(0);
                    }
                }

                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                // Read pixels
                glReadPixels(0, 0, game_width, game_height, GL_RGBA, GL_UNSIGNED_BYTE, capture_buffer);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                // Flip vertically (OpenGL reads bottom-up, PNG expects top-down)
                int row_bytes = game_width * 4;
                unsigned char* temp_row = (unsigned char*)malloc(row_bytes);
                for (int y = 0; y < game_height / 2; y++) {
                    unsigned char* top = capture_buffer + y * row_bytes;
                    unsigned char* bot = capture_buffer + (game_height - 1 - y) * row_bytes;
                    memcpy(temp_row, top, row_bytes);
                    memcpy(top, bot, row_bytes);
                    memcpy(bot, temp_row, row_bytes);
                }
                free(temp_row);

                // Write PNG
                char filename[600];
                snprintf(filename, sizeof(filename), "%s/frame_%06d.png", capture_output_dir, capture_frame_number);
                stbi_write_png(filename, game_width, game_height, 4, capture_buffer, game_width * 4);
                capture_frame_number++;
            }

            // Screen pass: normal compositing to window for visual feedback
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            int window_w, window_h;
            SDL_GetWindowSize(window, &window_w, &window_h);

            float scale_x = (float)window_w / game_width;
            float scale_y = (float)window_h / game_height;
            float scale = (scale_x < scale_y) ? scale_x : scale_y;
            if (scale < 1.0f) scale = 1.0f;

            int scaled_w = (int)(game_width * scale);
            int scaled_h = (int)(game_height * scale);
            int offset_x = (window_w - scaled_w) / 2;
            int offset_y = (window_h - scaled_h) / 2;

            glViewport(0, 0, window_w, window_h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glViewport(offset_x, offset_y, scaled_w, scaled_h);
            glUseProgram(screen_shader);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

            GLint offset_loc = glGetUniformLocation(screen_shader, "u_offset");

            if (layer_draw_count > 0) {
                for (int i = 0; i < layer_draw_count; i++) {
                    LayerDrawCommand* cmd = &layer_draw_queue[i];
                    Layer* layer = cmd->layer;
                    float ndc_x = (cmd->x / game_width) * 2.0f;
                    float ndc_y = -(cmd->y / game_height) * 2.0f;
                    glUniform2f(offset_loc, ndc_x, ndc_y);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer));
                    glBindVertexArray(screen_vao);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    glBindVertexArray(0);
                }

                for (int i = 0; i < layer_count; i++) {
                    layer_reset_effects(layer_registry[i]);
                }
                layer_draw_count = 0;
            } else {
                glUniform2f(offset_loc, 0.0f, 0.0f);
                for (int i = 0; i < layer_count; i++) {
                    Layer* layer = layer_registry[i];
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, layer_get_texture(layer));
                    glBindVertexArray(screen_vao);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    glBindVertexArray(0);
                    layer_reset_effects(layer);
                }
            }

            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            SDL_GL_SwapWindow(window);
        }
        lua_pop(L, 1);  // traceback

        // Clean up capture resources
        if (capture_buffer) { free(capture_buffer); capture_buffer = NULL; }
        if (capture_fbo) { glDeleteFramebuffers(1, &capture_fbo); capture_fbo = 0; }
        if (capture_texture) { glDeleteTextures(1, &capture_texture); capture_texture = 0; }
    } else {
        // Desktop: traditional blocking loop
        while (running) {
            main_loop_iteration();
        }
    }

    printf("Shutting down...\n");
    engine_shutdown();
    #endif

    return 0;
}

// stb_vorbis implementation - must be at end to avoid macro conflicts with our code
#undef STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
