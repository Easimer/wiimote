#include <stdio.h>
#include <assert.h>

#include <SDL2/SDL.h>
#include "glad/glad.h"

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>

#include <motion_input.h>

typedef struct wnd {
    SDL_Window *hWindow;
    void *hGlCtx;
} wnd_t;

static int open_window(wnd_t *wnd) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    char const *pszGlslVersion = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags eWindowFlags = (SDL_WindowFlags)(
            SDL_WINDOW_OPENGL |
            SDL_WINDOW_RESIZABLE |
            SDL_WINDOW_ALLOW_HIGHDPI
    );

    SDL_Window *hWindow = SDL_CreateWindow(
            "wiimote",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1600, 900,
            eWindowFlags
    );

    if(hWindow == NULL) {
        return 0;
    }

    void *hGlCtx = SDL_GL_CreateContext(hWindow);

    if(hGlCtx == NULL) {
        SDL_DestroyWindow(hWindow);
        return 0;
    }

    SDL_GL_MakeCurrent(hWindow, hGlCtx);
    SDL_GL_SetSwapInterval(1);

    if (gladLoadGL() == 0) {
        printf("gladLoadGL failed\n");
        SDL_GL_DeleteContext(hGlCtx);
        SDL_DestroyWindow(hWindow);
        return 0;
    }

    wnd->hWindow = hWindow;
    wnd->hGlCtx = hGlCtx;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(hWindow, hGlCtx);
    ImGui_ImplOpenGL3_Init(pszGlslVersion);

    return 1;
}

static void destroy_window(wnd_t *wnd) {
    assert(wnd != NULL);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(wnd->hGlCtx);
    SDL_DestroyWindow(wnd->hWindow);
    SDL_Quit();
}

#define PAST_DATA_COUNT (1024)

typedef struct input_state {
    bool buttons[(int)MB_MAX];

    float acc[3];

    float old_x[PAST_DATA_COUNT];
    float old_y[PAST_DATA_COUNT];
    float old_z[PAST_DATA_COUNT];
    int old_i;
} input_state_t;

static int display_input_state(input_state_t *state) {
    for(int i = 0; i < MB_MAX; i++) {
        char buf[32];
        snprintf(buf, 31, "%d", i);
        ImGui::Checkbox(buf, &state->buttons[i]);
    }

    ImGui::InputFloat3("Acc.", state->acc);

    ImGui::PlotLines("Accel. X", state->old_x, PAST_DATA_COUNT);
    ImGui::PlotLines("Accel. Y", state->old_y, PAST_DATA_COUNT);
    ImGui::PlotLines("Accel. Z", state->old_z, PAST_DATA_COUNT);
    return 0;
}

static void mutate(input_state_t *state, motion_event_t const &ev) {
    if(ev.kind == MI_EV_BUTTON) {
        state->buttons[(int)ev.btn.btn] = ev.btn.released ? false : true;
    } else if(ev.kind == MI_EV_ACCEL) {
        state->acc[0] = ev.accel.x;
        state->acc[1] = ev.accel.y;
        state->acc[2] = ev.accel.z;

        state->old_x[state->old_i] = ev.accel.x;
        state->old_y[state->old_i] = ev.accel.y;
        state->old_z[state->old_i] = ev.accel.z;
        state->old_i = (state->old_i + 1) % PAST_DATA_COUNT;
    }
}

int main(int argc, char **argv) {
    wnd_t wnd;
    motion_input_config_t cfg;
    input_state_t inp;

    memset(&inp, 0, sizeof(inp));

    if(!open_window(&wnd)) {
        printf("open_window() failed\n");
        return 1;
    }

    if(motion_init(&cfg) != 0) {
        printf("motion_init() failed\n");
        return 1;
    }

    bool bExit = false;
    auto &io = ImGui::GetIO();

    while(!bExit) {
        motion_event_t ev;
        int already_have_accel = 0;
        int cancel_motion_poll = 0;
        while(motion_poll(&ev) && !cancel_motion_poll) {
            switch(ev.kind) {
                case MI_EV_BUTTON:
                {
                    mutate(&inp, ev);
                    break;
                }
                case MI_EV_ACCEL:
                {
                    if(!already_have_accel) {
                        mutate(&inp, ev);
                        already_have_accel = 1;
                    } else {
                        cancel_motion_poll = 1;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        SDL_Event sev;
        while(SDL_PollEvent(&sev)) {
            ImGui_ImplSDL2_ProcessEvent(&sev);
            if(sev.type == SDL_QUIT) {
                bExit = true;
            }

            if(!io.WantCaptureKeyboard) {
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(wnd.hWindow);
        ImGui::NewFrame();

        if(display_input_state(&inp) != 0) {
            bExit = true;
        }

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(wnd.hWindow);
    }

    if(motion_shutdown() != 0) {
        printf("motion_shutdown() failed\n");
        return 1;
    }

    destroy_window(&wnd);
    return 0;
}
