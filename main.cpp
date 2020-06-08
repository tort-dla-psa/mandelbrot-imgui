#include <chrono>
#include <iostream>
#include "submodules/imgui/imgui.h"
#include "submodules/imgui/examples/imgui_impl_sdl.h"
#include "submodules/imgui/examples/imgui_impl_opengl3.h"
#include "submodules/rwqueue/readerwriterqueue.h"
#include <SDL.h>
#include <string>
#include <tuple>
#include <vector>
#include <complex>
#include <thread>
#include <mutex>
#include <atomic>
#include <GL/glew.h>
#include "mand_generator.h"

using T = double;
using mand_gen_ = mand_generator<T>;

auto make_task(T x_min, T x_max, T y_min, T y_max,
        unsigned threads, unsigned lines, unsigned limit,
        unsigned w, unsigned h,
        ImVec4 bel, ImVec4 not_bel)
{
    mand_gen_::task t;
    t.belongs = mand_gen_::color{bel.x, bel.y, bel.z};
    t.not_belongs = mand_gen_::color{not_bel.x, not_bel.y, not_bel.z};
    t.w = w;
    t.h = h;
    t.x_min = x_min;
    t.x_max = x_max;
    t.y_min = y_min;
    t.y_max = y_max;
    t.threads = threads;
    t.lines = lines;
    t.iters = limit;
    return t;
}

int main(int, char**) {
    mand_gen_::task_q tasks(1);
    mand_gen_::img_q imgs(1);
    mand_gen_ gen(tasks, imgs);
    gen.run();

    const unsigned w = 1920, h = 1080;
    ImVec4 def_belongs = ImVec4(0.f, 0.f, 0.f, 1.00f);
    ImVec4 def_not_belongs = ImVec4(0.5f, 0.2f, 0.3f, 1.00f);
    T def_x_max = 1.5;
    T def_y_max = 1.5;
    T def_x_min = -1.5;
    T def_y_min = -1.5;
    unsigned def_lines = 50;
    unsigned def_threads = 4;
    unsigned def_iters = 42;
    auto t = make_task(def_x_min, def_x_max, def_y_min, def_y_max,
            def_threads, def_lines, def_iters,
            w, h, def_belongs, def_not_belongs);
    tasks.enqueue(t);
    auto reset = [&def_belongs, &def_not_belongs,
        &def_x_max, &def_y_max, &def_x_min, &def_y_min,
        &w, &h, &t,
        &def_lines, &def_threads, &def_iters]
    {
        t = make_task(def_x_min, def_x_max, def_y_min, def_y_max,
                def_threads, def_lines, def_iters,
                w, h, def_belongs, def_not_belongs);
    };

    auto belongs = def_belongs;
    auto not_belongs = def_not_belongs;
    auto x_max=def_x_max;
    auto y_max=def_y_max;
    auto x_min=def_x_min;
    auto y_min=def_y_min;
    auto lines=def_lines;
    auto threads=def_threads;
    auto iters=def_iters;
    auto update = [&belongs, &not_belongs,
        &x_max, &y_max, &x_min, &y_min,
        &w, &h, &t,
        &lines, &threads, &iters]
    {
        t = make_task(x_min, x_max, y_min, y_max,
                threads, lines, iters,
                w, h, belongs, not_belongs);
    };

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
    bool iter_incr = true;

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

        ImGui::Begin("Parameters");
        ImGui::ColorEdit3("Belongs", (float*)&belongs); ImGui::SameLine();
        if(ImGui::Button("Reset")){ belongs = def_belongs; update(); }
        ImGui::ColorEdit3("Not belongs", (float*)&not_belongs); ImGui::SameLine();
        if(ImGui::Button("Reset")){ not_belongs = def_not_belongs; update(); }
        ImGui::InputInt("Lines:", (int*)&lines); ImGui::SameLine();
        if(ImGui::Button("Reset")){ x_min = def_x_min; update(); }
        ImGui::InputInt("Threads:", (int*)&threads); ImGui::SameLine();
        if(ImGui::Button("Reset")){ x_min = def_x_min; update(); }
        ImGui::InputInt("Iters:", (int*)&iters); ImGui::SameLine();
        if(ImGui::Button("Reset")){ x_min = def_x_min; update(); }
        ImGui::InputDouble("X1:", &x_min); ImGui::SameLine();
        if(ImGui::Button("Reset")){ x_min = def_x_min; update(); }
        ImGui::InputDouble("Y1:", &y_min); ImGui::SameLine();
        if(ImGui::Button("Reset")){ y_min = def_y_min; update(); }
        ImGui::InputDouble("X2:", &x_max); ImGui::SameLine();
        if(ImGui::Button("Reset")){ x_max = def_x_max; update(); }
        ImGui::InputDouble("Y2:", &y_max); ImGui::SameLine();
        if(ImGui::Button("Reset")){ y_max = def_y_max; update(); }
        if(ImGui::Button("Reset all")){ reset(); } ImGui::SameLine();
        if(ImGui::Button("Update")){ update(); }
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
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
        if(drag_beg){
            drag_2 = ImGui::GetMousePos();
        }
        double x_beg = std::min(drag_1.x, drag_2.x);
        double x_end = std::max(drag_1.x, drag_2.x);
        double y_beg = std::min(drag_1.y, drag_2.y);
        double y_end = std::max(drag_1.y, drag_2.y);
        double x_min_bak = x_min;
        double x_max_bak = x_max;
        double y_min_bak = y_min;
        double y_max_bak = y_max;
        double x1_rel = x_beg/w;
        double x2_rel = x_end/w;
        double y1_rel = y_beg/h;
        double y2_rel = y_end/h;
        double new_x_min = (x_max_bak - x_min_bak)*x1_rel + x_min_bak;
        double new_x_max = (x_max_bak - x_min_bak)*x2_rel + x_min_bak;
        double new_y_min = (y_max_bak - y_min_bak)*y1_rel + y_min_bak;
        double new_y_max = (y_max_bak - y_min_bak)*y2_rel + y_min_bak;
        if(ImGui::IsMouseReleased(0)){
            drag_2 = ImGui::GetMousePos();
            if(drag_beg){
                x_min = new_x_min;
                x_max = new_x_max;
                y_min = new_y_min;
                y_max = new_y_max;
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
        tasks.enqueue(t);
        drawList->AddImage((void*)(intptr_t)id, ImVec2(0,0), ImVec2(1920, 1080));
        if(drag_beg){
            auto str = [](double data){ return std::to_string(data).substr(0, 4); };
            auto x_beg = std::min(drag_1.x, ImGui::GetMousePos().x);
            auto x_end = std::max(drag_1.x, ImGui::GetMousePos().x);
            auto y_beg = std::min(drag_1.y, ImGui::GetMousePos().y);
            auto y_end = std::max(drag_1.y, ImGui::GetMousePos().y);
            drawList->AddRect({x_beg, y_beg}, {x_end, y_end}, ImColor{255,255,255});
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(not_belongs.x, not_belongs.y, not_belongs.z, not_belongs.w);
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
