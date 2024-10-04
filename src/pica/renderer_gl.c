#include "renderer_gl.h"

#include "../3ds.h"
#include "hostshaders/hostshaders.h"

// #define WIREFRAME

#define BOT_GAP ((1 - (float) SCREEN_WIDTH_BOT / (float) SCREEN_WIDTH) / 2)

float quads[] = {
    0,           0,   0, 0, //
    0,           0.5, 0, 1, //
    1,           0,   1, 0, //
    1,           0.5, 1, 1, //
    BOT_GAP,     0.5, 0, 0, //
    BOT_GAP,     1,   0, 1, //
    1 - BOT_GAP, 0.5, 1, 0, //
    1 - BOT_GAP, 1,   1, 1, //
};

GLuint make_shader(const char* vert, const char* frag) {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vert, NULL);
    glCompileShader(vertexShader);
    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infolog[512];
        glGetShaderInfoLog(vertexShader, 512, NULL, infolog);
        printf("Error compiling vertex shader: %s\n", infolog);
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &frag, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infolog[512];
        glGetShaderInfoLog(fragmentShader, 512, NULL, infolog);
        printf("Error compiling fragment shader: %s\n", infolog);
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infolog[512];
        glGetProgramInfoLog(program, 512, NULL, infolog);
        printf("Error linking program: %s\n", infolog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void bind_gpu(GLState* state) {
    glUseProgram(state->gpu.program);
    glBindVertexArray(state->gpu.vao);

#ifdef WIREFRAME
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void renderer_gl_setup(GLState* state) {

    state->main.program = make_shader(mainvertsource, mainfragsource);
    glUniform1i(glGetUniformLocation(state->main.program, "tex"), 0);

    glGenVertexArrays(1, &state->main.vao);
    glBindVertexArray(state->main.vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quads, quads, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*) (2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    state->gpu.program = make_shader(gpuvertsource, gpufragsource);

    glGenVertexArrays(1, &state->gpu.vao);
    glBindVertexArray(state->gpu.vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*) offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*) offsetof(Vertex, color));
    glEnableVertexAttribArray(1);

    glEnable(GL_DEPTH_TEST);

    for (int i = 0; i < 2; i++) {
        glGenFramebuffers(1, &state->fbs[i].fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, state->fbs[i].fbo);

        glGenTextures(1, &state->fbs[i].tex_colorbuf);
        glBindTexture(GL_TEXTURE_2D, state->fbs[i].tex_colorbuf);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, state->fbs[i].tex_colorbuf, 0);

        glGenTextures(1, &state->fbs[i].tex_depthbuf);
        glBindTexture(GL_TEXTURE_2D, state->fbs[i].tex_depthbuf);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, SCREEN_WIDTH,
                     SCREEN_HEIGHT, 0, GL_DEPTH24_STENCIL8,
                     GL_UNSIGNED_INT_24_8, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D, state->fbs[i].tex_depthbuf, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            printfln("uh oh");
    }
}

void render_gl_main(GLState* state) {
    glUseProgram(state->main.program);
    glBindVertexArray(state->main.vao);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifdef WIREFRAME
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    glViewport(0, 0, SCREEN_WIDTH, 2 * SCREEN_HEIGHT);

    glBindTexture(GL_TEXTURE_2D, state->fbs[state->fb_top].tex_colorbuf);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindTexture(GL_TEXTURE_2D, state->fbs[state->fb_bot].tex_colorbuf);
    glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);

    bind_gpu(state);
}