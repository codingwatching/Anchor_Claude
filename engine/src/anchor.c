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
#define INITIAL_SCALE 2

// Timing configuration (matching reference Anchor)
#define FIXED_RATE (1.0 / 144.0)  // 144 Hz fixed timestep
#define MAX_UPDATES 10            // Cap on fixed steps per frame (prevents spiral of death)

static SDL_Window* window = NULL;
static SDL_GLContext gl_context = NULL;
static lua_State* L = NULL;
static bool error_state = false;
static char error_message[4096] = {0};

// Rendering state
static GLuint shader_program = 0;
static GLuint vao = 0;
static GLuint vbo = 0;

// Framebuffer for integer scaling
static GLuint fbo = 0;
static GLuint fbo_texture = 0;
static GLuint screen_shader = 0;
static GLuint screen_vao = 0;
static GLuint screen_vbo = 0;

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
    "layout (location = 1) in vec4 aColor;\n"
    "out vec4 vertexColor;\n"
    "uniform mat4 projection;\n"
    "void main() {\n"
    "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
    "    vertexColor = aColor;\n"
    "}\n";

static const char* fragment_shader_source =
    "in vec4 vertexColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = vertexColor;\n"
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
    // Framebuffer resources
    if (screen_vbo) { glDeleteBuffers(1, &screen_vbo); screen_vbo = 0; }
    if (screen_vao) { glDeleteVertexArrays(1, &screen_vao); screen_vao = 0; }
    if (screen_shader) { glDeleteProgram(screen_shader); screen_shader = 0; }
    if (fbo_texture) { glDeleteTextures(1, &fbo_texture); fbo_texture = 0; }
    if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
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

    // Fixed timestep loop
    while (lag >= FIXED_RATE) {
        // Process events inside fixed loop (input tied to simulation steps)
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

    // === PASS 1: Render game to framebuffer ===
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, GAME_WIDTH, GAME_HEIGHT);

    if (error_state) {
        glClearColor(0.3f, 0.1f, 0.1f, 1.0f);  // Dark red for error
    } else {
        glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up orthographic projection (game coordinates)
    // Maps (0,0) at top-left to (GAME_WIDTH, GAME_HEIGHT) at bottom-right
    float projection[16] = {
        2.0f / GAME_WIDTH, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / GAME_HEIGHT, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };

    glUseProgram(shader_program);
    GLint proj_loc = glGetUniformLocation(shader_program, "projection");
    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);

    // Draw a test quad in the center
    float cx = GAME_WIDTH / 2.0f;
    float cy = GAME_HEIGHT / 2.0f;
    float size = 50.0f;
    float r = 1.0f, g = 0.5f, b = 0.2f, a = 1.0f;  // Orange

    // Two triangles forming a quad (6 vertices)
    float vertices[] = {
        // Triangle 1
        cx - size, cy - size, r, g, b, a,
        cx + size, cy - size, r, g, b, a,
        cx + size, cy + size, r, g, b, a,
        // Triangle 2
        cx - size, cy - size, r, g, b, a,
        cx + size, cy + size, r, g, b, a,
        cx - size, cy + size, r, g, b, a,
    };

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // TODO: Layer system will draw here (error_message when in error_state)

    // === PASS 2: Blit framebuffer to screen with aspect-ratio scaling ===
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Get current window size
    int window_w, window_h;
    SDL_GetWindowSize(window, &window_w, &window_h);
    glViewport(0, 0, window_w, window_h);

    // Calculate scale to fit window while maintaining aspect ratio
    float scale_x = (float)window_w / GAME_WIDTH;
    float scale_y = (float)window_h / GAME_HEIGHT;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    // Calculate centered position with letterboxing
    int scaled_w = (int)(GAME_WIDTH * scale);
    int scaled_h = (int)(GAME_HEIGHT * scale);
    int offset_x = (window_w - scaled_w) / 2;
    int offset_y = (window_h - scaled_h) / 2;

    // Clear screen to black (letterbox color)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw framebuffer texture (viewport handles positioning)
    glViewport(offset_x, offset_y, scaled_w, scaled_h);
    glUseProgram(screen_shader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_texture);

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
    // Vertex format: x, y, r, g, b, a (6 floats per vertex, 6 vertices per quad)
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // Allocate space for one quad (6 vertices * 6 floats)
    glBufferData(GL_ARRAY_BUFFER, 6 * 6 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    // Position attribute (location 0): 2 floats
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (location 1): 4 floats
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    printf("Game VAO/VBO created\n");

    // Create framebuffer for integer scaling
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create texture to render to
    glGenTextures(1, &fbo_texture);
    glBindTexture(GL_TEXTURE_2D, fbo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GAME_WIDTH, GAME_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  // Crisp pixels
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer not complete\n");
        shutdown();
        return 1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    printf("Framebuffer created (%dx%d)\n", GAME_WIDTH, GAME_HEIGHT);

    // Create screen shader for blitting framebuffer
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
