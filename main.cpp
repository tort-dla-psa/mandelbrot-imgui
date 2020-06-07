#include <chrono>
#include <iostream>
#include "submodules/imgui/imgui.h"
#include "submodules/imgui/examples/imgui_impl_sdl.h"
#include "submodules/imgui/examples/imgui_impl_opengl3.h"
#include "submodules/rwqueue/readerwriterqueue.h"
#include <SDL.h>
#include <tuple>
#include <vector>
#include <complex>
#include <thread>
#include <mutex>
#include <atomic>
#include <GL/glew.h>
#include "mand_generator.h"

using mand_gen_ = mand_generator<double>;

int main(int, char**) {
    mand_gen_::task_q tasks(1);
    mand_gen_::img_q imgs(1);
    mand_gen_ gen(tasks, imgs);
    gen.run();

    const unsigned w = 1080, h = 1920;
    double x_zoom = 0;
    double y_zoom = 0;
    double zoom_fact = 1.0;
    bool first_zoom = true, zoom = false;
    mand_gen_::task t;
    t.x_max = 1.5;
    t.y_max = 1.5;
    t.x_min = -1.5;
    t.y_min = -1.5;
    t.lines = 10;
    t.threads = 4;
    t.w = w;
    t.h = h;
    tasks.enqueue(t);
    // Setup SDL
    // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
    bool err = glewInit() != GLEW_OK;
    if (err) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    GLuint id;
    GLubyte raw[w*h*3];
    std::fill(std::begin(raw), std::end(raw), 128);
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //scale linearly when image bigger than texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //scale linearly when image smalled than texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, &raw);
    glBindTexture(GL_TEXTURE_2D, 0);

    ImVec2 drag_1, drag_2;
    bool drag_beg;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();
        auto drawList = ImGui::GetBackgroundDrawList();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::InputInt("Lines:", (int*)&t.lines);
        ImGui::InputInt("Threads:", (int*)&t.threads);
        ImGui::InputDouble("X1:", &t.x_min);
        ImGui::InputDouble("Y1:", &t.y_min);
        ImGui::InputDouble("X2:", &t.x_max);
        ImGui::InputDouble("Y2:", &t.y_max);
        ImGui::End();
        mand_gen_::image i;
        if(ImGui::IsMouseClicked(0)){
            drag_1 = ImGui::GetMousePos();
        }
        if(ImGui::GetMouseDragDelta(0).x > .0f &&
            ImGui::GetMouseDragDelta(0).y > .0f)
        {
            drag_beg = true;
        }
        if(ImGui::IsMouseReleased(0)){
            drag_2 = ImGui::GetMousePos();
            if(drag_beg){
                double map_x = (t.x_max - t.x_min)/ImGui::GetWindowWidth();
                double map_y = (t.y_max - t.y_min)/ImGui::GetWindowHeight();
                auto x_beg = std::min(drag_1.x, drag_2.x);
                auto x_end = std::max(drag_1.x, drag_2.x);
                auto y_beg = std::min(drag_1.y, drag_2.y);
                auto y_end = std::max(drag_1.y, drag_2.y);
                auto x_min_bak = t.x_min;
                auto y_min_bak = t.y_min;
                t.x_min = x_beg*map_x + x_min_bak;
                t.x_max = x_end*map_x + x_min_bak;
                t.y_min = y_beg*map_y + y_min_bak;
                t.y_max = y_end*map_y + y_min_bak;
            }
            drag_beg = false;
        }

        auto size = ImGui::GetIO().DisplaySize;
        if(imgs.try_dequeue(i)){
            do{
                glBindTexture(GL_TEXTURE_2D, id);
                glTexSubImage2D(GL_TEXTURE_2D, 0, i.x, i.y,
                        i.w, i.h, GL_RGB, GL_UNSIGNED_BYTE,
                        i.pixels.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }while(imgs.try_dequeue(i));
        }
        /*
        if(zoom){
            t.x_min = x_zoom - (x_zoom - t.x_min)/zoom_fact;
            t.x_max = x_zoom + (t.x_max - x_zoom)/zoom_fact;
            t.y_min = y_zoom - (y_zoom - t.y_min)/zoom_fact;
            t.y_max = y_zoom + (t.y_max - y_zoom)/zoom_fact;
        }*/
        tasks.enqueue(t);
        drawList->AddImage((void*)(intptr_t)id, ImVec2(0,0), ImVec2(1920, 1080));
        if(drag_beg){
            auto x_beg = std::min(drag_1.x, ImGui::GetMousePos().x);
            auto x_end = std::max(drag_1.x, ImGui::GetMousePos().x);
            auto y_beg = std::min(drag_1.y, ImGui::GetMousePos().y);
            auto y_end = std::max(drag_1.y, ImGui::GetMousePos().y);
            drawList->AddRect({x_beg, y_beg}, {x_end, y_end}, ImColor{255,255,255});
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    gen.stop();

    return 0;
}
