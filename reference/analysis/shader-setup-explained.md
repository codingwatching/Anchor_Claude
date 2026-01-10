# Shader Setup Explained

This document explains the OpenGL shader code added to `anchor.c`.

---

## Overview

The rendering pipeline has four parts:
1. **Shader sources** — GLSL code that runs on the GPU
2. **Shader compilation** — turning text into GPU programs
3. **VAO/VBO setup** — describing how vertex data is laid out
4. **Rendering** — uploading vertices and drawing

---

## 1. Shader Sources

### Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
out vec4 vertexColor;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
}
```

**What each line does:**

| Line | Purpose |
|------|---------|
| `#version 330 core` | Use GLSL 3.30, core profile (no deprecated features) |
| `layout (location = 0) in vec2 aPos` | Input attribute 0 is a 2D position |
| `layout (location = 1) in vec4 aColor` | Input attribute 1 is an RGBA color |
| `out vec4 vertexColor` | Pass color to fragment shader |
| `uniform mat4 projection` | 4x4 matrix set by C code (same for all vertices) |
| `gl_Position = projection * vec4(aPos, 0.0, 1.0)` | Transform 2D pos to clip space |
| `vertexColor = aColor` | Forward color unchanged |

**The transformation:**
- `aPos` is in game coordinates (e.g., 240, 135)
- `vec4(aPos, 0.0, 1.0)` makes it a 4D homogeneous coordinate (z=0, w=1)
- `projection *` transforms to clip space (-1 to +1 range)
- GPU then maps clip space to screen pixels

### Fragment Shader

```glsl
#version 330 core
in vec4 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vertexColor;
}
```

**What each line does:**

| Line | Purpose |
|------|---------|
| `in vec4 vertexColor` | Receive interpolated color from vertex shader |
| `out vec4 FragColor` | Output to framebuffer |
| `FragColor = vertexColor` | Just output the color |

**Interpolation:** If triangle vertices have different colors, the GPU automatically interpolates between them for each pixel. We're using solid colors, so all three vertices have the same color.

---

## 2. Shader Compilation

### compile_shader()

```c
static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

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
```

| Step | What happens |
|------|--------------|
| `glCreateShader(type)` | Allocate a shader object (type = `GL_VERTEX_SHADER` or `GL_FRAGMENT_SHADER`) |
| `glShaderSource(...)` | Upload source code text to the shader object |
| `glCompileShader(...)` | Compile GLSL to GPU bytecode |
| `glGetShaderiv(..., GL_COMPILE_STATUS, ...)` | Check if compilation succeeded |
| `glGetShaderInfoLog(...)` | Get error messages if it failed |

**Why return 0 on failure:** OpenGL uses 0 as "no object", so returning 0 signals failure while any positive number is a valid shader ID.

### create_shader_program()

```c
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
```

| Step | What happens |
|------|--------------|
| Compile both shaders | Get shader IDs (or fail early) |
| `glCreateProgram()` | Allocate a program object |
| `glAttachShader(...)` | Connect shaders to program |
| `glLinkProgram(...)` | Link into final GPU program (resolves `in`/`out` connections) |
| `glDeleteShader(...)` | Shaders can be deleted after linking — program keeps its own copy |
| Check `GL_LINK_STATUS` | Linking can fail if vertex outputs don't match fragment inputs |

**Why link?** Vertex and fragment shaders are separate compilation units. Linking:
- Connects vertex `out` variables to fragment `in` variables
- Validates that types match
- Creates the final executable GPU program

---

## 3. VAO/VBO Setup

```c
glGenVertexArrays(1, &vao);
glGenBuffers(1, &vbo);

glBindVertexArray(vao);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, 6 * 6 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

// Position attribute (location 0): 2 floats
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

// Color attribute (location 1): 4 floats
glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
glEnableVertexAttribArray(1);

glBindVertexArray(0);
```

### What's a VAO?

**Vertex Array Object** — stores the *description* of vertex data layout. It remembers:
- Which VBO(s) to read from
- How to interpret the bytes (what's position, what's color, etc.)
- Which attributes are enabled

Think of it as a "preset" for vertex format. Bind it, and OpenGL knows how to read your vertices.

### What's a VBO?

**Vertex Buffer Object** — the actual vertex data stored in GPU memory. Just raw bytes.

### The vertex format

Our vertices are interleaved: `x, y, r, g, b, a` (6 floats per vertex)

```
Vertex 0: [x][y][r][g][b][a]
Vertex 1: [x][y][r][g][b][a]
...
```

**Stride** = 6 floats = 24 bytes (distance from one vertex to the next)

### glVertexAttribPointer explained

```c
glVertexAttribPointer(
    0,                    // attribute location (matches shader)
    2,                    // number of components (vec2 = 2 floats)
    GL_FLOAT,             // data type
    GL_FALSE,             // normalize? (no, already floats)
    6 * sizeof(float),    // stride (bytes between vertices)
    (void*)0              // offset (position starts at byte 0)
);
```

For color:
```c
glVertexAttribPointer(
    1,                          // location 1
    4,                          // vec4 = 4 floats
    GL_FLOAT,
    GL_FALSE,
    6 * sizeof(float),          // same stride
    (void*)(2 * sizeof(float))  // offset: skip x,y (2 floats = 8 bytes)
);
```

### GL_DYNAMIC_DRAW

Tells OpenGL we'll update this buffer frequently. Helps it choose optimal memory location. Alternatives:
- `GL_STATIC_DRAW` — upload once, never change
- `GL_STREAM_DRAW` — upload once, use once, discard

---

## 4. Rendering

### The Projection Matrix

```c
float projection[16] = {
    2.0f / GAME_WIDTH, 0.0f, 0.0f, 0.0f,
    0.0f, -2.0f / GAME_HEIGHT, 0.0f, 0.0f,
    0.0f, 0.0f, -1.0f, 0.0f,
    -1.0f, 1.0f, 0.0f, 1.0f
};
```

This is an orthographic projection matrix that transforms game coordinates to clip space.

**What it does:**
- Input: game coordinates (0,0) to (480, 270)
- Output: clip space (-1,-1) to (+1,+1)

**The math:**

For a point (x, y):
```
clip_x = x * (2/width) - 1
clip_y = y * (-2/height) + 1
```

- `2/width` scales x from [0, width] to [0, 2]
- `-1` shifts to [-1, 1]
- Negative Y scale flips the axis (screen Y goes down, clip Y goes up)

**Matrix layout:** OpenGL uses column-major order, so:
```
[0]  [4]  [8]  [12]     [sx   0   0  tx]
[1]  [5]  [9]  [13]  =  [ 0  sy   0  ty]
[2]  [6]  [10] [14]     [ 0   0  sz  tz]
[3]  [7]  [11] [15]     [ 0   0   0   1]
```

### Drawing

```c
glUseProgram(shader_program);
GLint proj_loc = glGetUniformLocation(shader_program, "projection");
glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);
```

| Call | Purpose |
|------|---------|
| `glUseProgram(...)` | Activate our shader for subsequent draw calls |
| `glGetUniformLocation(...)` | Find where "projection" uniform lives in the shader |
| `glUniformMatrix4fv(...)` | Upload the 4x4 matrix (fv = float vector, 1 = one matrix, GL_FALSE = don't transpose) |

```c
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
```

A quad needs 2 triangles (OpenGL only draws triangles). We repeat 2 vertices — an index buffer would avoid this, but for simplicity we just duplicate.

```
(cx-size, cy-size) -------- (cx+size, cy-size)
         |    \                     |
         |      \    Triangle 1     |
         |        \                 |
         |          \               |
         |   Triangle 2  \          |
         |                 \        |
(cx-size, cy+size) -------- (cx+size, cy+size)
```

```c
glBindVertexArray(vao);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
glDrawArrays(GL_TRIANGLES, 0, 6);
glBindVertexArray(0);
```

| Call | Purpose |
|------|---------|
| `glBindVertexArray(vao)` | Use our vertex format preset |
| `glBindBuffer(...)` | Target our VBO for the upload |
| `glBufferSubData(...)` | Upload vertex data (sub = update existing buffer, not reallocate) |
| `glDrawArrays(GL_TRIANGLES, 0, 6)` | Draw 6 vertices as triangles (2 triangles) |
| `glBindVertexArray(0)` | Unbind (optional, but clean) |

---

## Why This Structure?

This is a minimal but complete rendering setup. For the layer system later:

1. **Batch rendering** — instead of 6 vertices for one quad, we'll have thousands of vertices for many quads, all uploaded at once
2. **Texture support** — add texture coordinates to vertex format, sample in fragment shader
3. **Multiple shaders** — different programs for different effects (textured, colored, post-processing)

The current code draws one hardcoded quad. The layer system will collect draw commands, batch them by texture/shader, and draw efficiently.
