#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "3ds.h"
#include "cpu.h"
#include "emulator.h"
#include "pica/renderer_gl.h"

const char usage[] =
    R"(ctremu [options] [romfile]
-h -- print help
-l -- enable info logging
-v -- disable vsync
-sN -- upscale by N
)";

SDL_Window* g_window;

SDL_JoystickID g_gamepad_id;
SDL_Gamepad* g_gamepad;

bool g_pending_reset;

char* oldcwd;

#define FREECAM_SPEED 5.0
#define FREECAM_ROTATE_SPEED 0.02

#ifdef GLDEBUGCTX
void glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
                   GLsizei length, const char* message, const void* userParam) {
    printfln("[GLDEBUG]%d %d %d %d %s", source, type, id, severity, message);
}
#endif

void read_args(int argc, char** argv) {
    char c;
    while ((c = getopt(argc, argv, "hlvs:")) != (char) -1) {
        switch (c) {
            case 'l':
                g_infologs = true;
                break;
            case 's': {
                int scale = atoi(optarg);
                if (scale <= 0) eprintf("invalid scale factor");
                else ctremu.videoscale = scale;
                break;
            }
            case 'v': {
                ctremu.vsync = false;
                break;
            }
            case '?':
            case 'h':
            default:
                eprintf(usage);
                exit(0);
        }
    }
    argc -= optind;
    argv += optind;
    if (argc >= 1) {
        if (argv[0][0] == '/') {
            emulator_set_rom(argv[0]);
        } else {
            char* path;
            asprintf(&path, "%s/%s", oldcwd, argv[0]);
            emulator_set_rom(path);
            free(path);
        }
    }
}

void file_callback(void*, char** files, int n) {
    if (files && files[0]) {
        emulator_set_rom(files[0]);
        g_pending_reset = true;
    }
}

void load_rom_dialog() {
    SDL_DialogFileFilter filetypes = {
        .name = "3DS Executables",
        .pattern = "3ds;cci;cxi;app;elf",
    };

    ctremu.pause = true;
    SDL_ShowOpenFileDialog((SDL_DialogFileCallback) file_callback, nullptr,
                           g_window, &filetypes, 1, nullptr, false);
}

void hotkey_press(SDL_Keycode key) {
    switch (key) {
        case SDLK_F5:
            ctremu.pause = !ctremu.pause;
            break;
        case SDLK_TAB:
            ctremu.uncap = !ctremu.uncap;
            break;
        case SDLK_F1:
            g_pending_reset = true;
            break;
        case SDLK_F2:
            load_rom_dialog();
            break;
        case SDLK_F4:
            g_cpulog = !g_cpulog;
            break;
        case SDLK_F7:
            ctremu.freecam_enable = !ctremu.freecam_enable;
            glm_mat4_identity(ctremu.freecam_mtx);
            renderer_gl_update_freecam(&ctremu.system.gpu.gl);
            break;
        default:
            break;
    }
}

void update_input(E3DS* s, SDL_Gamepad* controller, int view_w, int view_h) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    PadState btn = {};
    int cx = 0;
    int cy = 0;

    if (!ctremu.freecam_enable) {
        btn.a = keys[SDL_SCANCODE_L];
        btn.b = keys[SDL_SCANCODE_K];
        btn.x = keys[SDL_SCANCODE_O];
        btn.y = keys[SDL_SCANCODE_I];
        btn.l = keys[SDL_SCANCODE_Q];
        btn.r = keys[SDL_SCANCODE_P];
        btn.start = keys[SDL_SCANCODE_RETURN];
        btn.select = keys[SDL_SCANCODE_RSHIFT];
        btn.up = keys[SDL_SCANCODE_UP];
        btn.down = keys[SDL_SCANCODE_DOWN];
        btn.left = keys[SDL_SCANCODE_LEFT];
        btn.right = keys[SDL_SCANCODE_RIGHT];

        cx = (keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]) * INT16_MAX;
        cy = (keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S]) * INT16_MAX;
    } else {
        float speed = FREECAM_SPEED;
        if (keys[SDL_SCANCODE_LSHIFT]) speed /= 20;
        if (keys[SDL_SCANCODE_RSHIFT]) speed *= 20;

        mat4 m;

        vec3 t = {};
        mat4 r = GLM_MAT4_IDENTITY_INIT;

        if (keys[SDL_SCANCODE_E]) {
            t[1] = -speed;
        }
        if (keys[SDL_SCANCODE_Q]) {
            t[1] = speed;
        }
        if (keys[SDL_SCANCODE_DOWN]) {
            glm_rotate_make(r, FREECAM_ROTATE_SPEED, GLM_XUP);
        }
        if (keys[SDL_SCANCODE_UP]) {
            glm_rotate_make(r, -FREECAM_ROTATE_SPEED, GLM_XUP);
        }
        if (keys[SDL_SCANCODE_A]) {
            t[0] = speed;
        }
        if (keys[SDL_SCANCODE_D]) {
            t[0] = -speed;
        }
        if (keys[SDL_SCANCODE_LEFT]) {
            glm_rotate_make(r, -FREECAM_ROTATE_SPEED, GLM_YUP);
        }
        if (keys[SDL_SCANCODE_RIGHT]) {
            glm_rotate_make(r, FREECAM_ROTATE_SPEED, GLM_YUP);
        }
        if (keys[SDL_SCANCODE_W]) {
            t[2] = speed;
        }
        if (keys[SDL_SCANCODE_S]) {
            t[2] = -speed;
        }

        glm_translate_make(m, t);
        glm_mat4_mul(m, ctremu.freecam_mtx, ctremu.freecam_mtx);
        glm_mat4_mul(r, ctremu.freecam_mtx, ctremu.freecam_mtx);

        renderer_gl_update_freecam(&ctremu.system.gpu.gl);
    }

    if (controller) {
        btn.a |= SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_EAST);
        btn.b |= SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_SOUTH);
        btn.x |= SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_NORTH);
        btn.y |= SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_WEST);
        btn.start |= SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_START);
        btn.select |= SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_BACK);
        btn.left |=
            SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        btn.right |=
            SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        btn.up |= SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_DPAD_UP);
        btn.down |=
            SDL_GetGamepadButton(controller, SDL_GAMEPAD_BUTTON_DPAD_DOWN);

        int x = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTX);
        if (abs(x) > abs(cx)) cx = x;
        int y = -SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFTY);
        if (abs(y) > abs(cy)) cy = y;

        int tl = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        if (tl > INT16_MAX / 10) btn.l = 1;
        int tr = SDL_GetGamepadAxis(controller, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        if (tr > INT16_MAX / 10) btn.r = 1;
    }

    btn.cup = cy > INT16_MAX / 2;
    btn.cdown = cy < INT16_MIN / 2;
    btn.cleft = cx < INT16_MIN / 2;
    btn.cright = cx > INT16_MAX / 2;

    hid_update_pad(s, btn.w, cx, cy);

    float xf, yf;
    bool pressed =
        SDL_GetMouseState(&xf, &yf) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT);
    if (controller) {
        if (SDL_GetGamepadButton(controller,
                                 SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
            pressed = true;
        }
    }
    int x = xf, y = yf;

    if (pressed) {
        x -= view_w * (SCREEN_WIDTH_TOP - SCREEN_WIDTH_BOT) /
             (2 * SCREEN_WIDTH_TOP);
        x = x * SCREEN_WIDTH_TOP / view_w;
        y -= view_h / 2;
        y = y * 2 * SCREEN_HEIGHT / view_h;
        if (x < 0 || x >= SCREEN_WIDTH_BOT || y < 0 || y >= SCREEN_HEIGHT) {
            hid_update_touch(s, 0, 0, false);
        } else {
            hid_update_touch(s, x, y, true);
        }
    } else {
        hid_update_touch(s, 0, 0, false);
    }
}

int main(int argc, char** argv) {
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "Tanuki3DS");

    oldcwd = realpath(".", nullptr);

#ifdef NOPORTABLE
    char* prefpath = SDL_GetPrefPath("", "Tanuki3DS");
    chdir(prefpath);
    SDL_free(prefpath);
    int logfd =
        open("ctremu.log", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
    dup2(logfd, STDOUT_FILENO);
    close(logfd);
#endif

    emulator_init();

    read_args(argc, argv);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef GLDEBUGCTX
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    g_window =
        SDL_CreateWindow("Tanuki3DS", SCREEN_WIDTH_TOP * ctremu.videoscale,
                         2 * SCREEN_HEIGHT * ctremu.videoscale,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GLContext glcontext = SDL_GL_CreateContext(g_window);
    if (!glcontext) {
        SDL_Quit();
        lerror("could not create gl context");
        return 1;
    }
    glewInit();

#ifdef GLDEBUGCTX
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr,
                          GL_TRUE);
#endif

    if (!ctremu.romfile) {
        load_rom_dialog();
    } else {
        g_pending_reset = true;
    }

    if (ctremu.vsync) {
        if (!SDL_GL_SetSwapInterval(-1)) SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }

    Uint64 prev_time = SDL_GetTicksNS();
    Uint64 prev_fps_update = prev_time;
    Uint64 prev_fps_frame = 0;
    const Uint64 frame_ticks = SDL_NS_PER_SECOND / FPS;
    Uint64 frame = 0;

    ctremu.running = true;
    while (ctremu.running) {
        Uint64 cur_time;
        Uint64 elapsed;

        if (g_pending_reset) {
            g_pending_reset = false;
            if (emulator_reset()) {
                ctremu.pause = false;
            } else {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Tanuki3DS",
                                         "ROM loading failed", g_window);
                ctremu.pause = true;
            }
            SDL_RaiseWindow(g_window);
        }

        if (!ctremu.pause) {
            renderer_gl_setup_gpu(&ctremu.system.gpu.gl);

            do {
                e3ds_run_frame(&ctremu.system);
                frame++;

                cur_time = SDL_GetTicksNS();
                elapsed = cur_time - prev_time;
            } while (ctremu.uncap && elapsed < frame_ticks);
        }

        int w, h;
        SDL_GetWindowSizeInPixels(g_window, &w, &h);

        render_gl_main(&ctremu.system.gpu.gl, w, h);

        SDL_GL_SwapWindow(g_window);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    ctremu.running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    hotkey_press(e.key.key);
                    break;
                case SDL_EVENT_GAMEPAD_ADDED:
                    if (!g_gamepad) {
                        g_gamepad_id = e.gdevice.which;
                        g_gamepad = SDL_OpenGamepad(g_gamepad_id);
                    }
                    break;
                case SDL_EVENT_GAMEPAD_REMOVED:
                    if (g_gamepad && e.gdevice.which == g_gamepad_id) {
                        g_gamepad = nullptr;
                    }
                    break;
                case SDL_EVENT_DROP_FILE:
                    emulator_set_rom(e.drop.data);
                    g_pending_reset = true;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    const float aspect =
                        (float) SCREEN_WIDTH_TOP / (2 * SCREEN_HEIGHT);
                    SDL_SetWindowAspectRatio(g_window, aspect, aspect);
                    break;
            }
        }

        if (!ctremu.pause) update_input(&ctremu.system, g_gamepad, w, h);

        if (!(ctremu.uncap || ctremu.vsync)) {
            cur_time = SDL_GetTicksNS();
            elapsed = cur_time - prev_time;
            Sint64 wait = frame_ticks - elapsed;
            if (wait > 0) {
                SDL_DelayPrecise(wait);
            }
        }
        cur_time = SDL_GetTicksNS();
        elapsed = cur_time - prev_fps_update;
        if (!ctremu.pause && elapsed >= SDL_NS_PER_SECOND / 2) {
            double fps =
                (double) SDL_NS_PER_SECOND * (frame - prev_fps_frame) / elapsed;

            char* wintitle;
            asprintf(&wintitle, "Tanuki3DS | %s | %.2lf FPS",
                     ctremu.romfilenodir, fps);
            SDL_SetWindowTitle(g_window, wintitle);
            free(wintitle);
            prev_fps_update = cur_time;
            prev_fps_frame = frame;
        }
        prev_time = cur_time;
    }

    SDL_GL_DestroyContext(glcontext);
    SDL_DestroyWindow(g_window);
    SDL_CloseGamepad(g_gamepad);

    SDL_Quit();

    emulator_quit();

    free(oldcwd);

    return 0;
}
