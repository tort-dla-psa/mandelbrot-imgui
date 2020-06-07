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

struct task{
    double img_x_min, img_x_max, img_y_min, img_y_max;
    unsigned lines, threads;
    bool operator==(const task &rhs)const{
        return img_x_max == rhs.img_x_max &&
            img_x_min == rhs.img_x_min &&
            img_y_max == rhs.img_y_max &&
            img_y_min == rhs.img_y_min;
    }
};

struct image{
    std::vector<GLubyte> pixels;
    int line_no;
    int x, y;
    int w, h;
};

using task_q = moodycamel::BlockingReaderWriterQueue<task>;
using img_q = moodycamel::ReaderWriterQueue<image>;
task_q tasks(1);
img_q imgs(1);

std::atomic_bool run;

void fract_func(){
    const int img_w = 1920;
    const int img_h = 1080;
    task prev_t;
    run = true;
    while(run){
        task t;
        tasks.wait_dequeue(t);
        if(t == prev_t){
            continue;
        }
        std::cout<<"x_min:"<<t.img_x_min<<std::endl;
        std::cout<<"x_max:"<<t.img_x_max<<std::endl;
        std::cout<<"y_min:"<<t.img_y_min<<std::endl;
        std::cout<<"y_max:"<<t.img_y_max<<std::endl;
        auto mand = [](auto x, auto y, const size_t &lim){
            const std::complex<double> point(x, y);
            std::complex<double> z(0, 0);
            size_t cnt = 0;
            while(std::abs(z) < 2 && cnt <= lim) {
                z = z * z + point;
                cnt++;
            }
            if (cnt < lim) return 255;
            else return 50;
        };
        auto calc_line = [&mand](auto y, auto img_w, auto img_h, auto line_h,
            auto img_x_min, auto img_x_max, auto img_y_min, auto img_y_max)
        {
            image img;
            img.x = 0;
            img.y = y;
            img.w = img_w;
            img.h = line_h;
            auto &pixels = img.pixels;
            double map_x = (img_x_max - img_x_min)/img_w;
            double map_y = (img_y_max - img_y_min)/img_h;
            for(size_t j = y; j<y+line_h; j++){
                for(size_t i = 0; i<img_w; i++){
                    double x_map = i*map_x + img_x_min;
                    double y_map = j*map_y + img_y_min;
                    auto clr = mand(x_map, y_map, 42);
                    pixels.emplace_back(clr);
                    pixels.emplace_back(0);
                    pixels.emplace_back(0);
                }
            }
            return img;
        };

        auto line_h = img_h/t.lines;
        for(size_t l = 0; l<t.lines; l += 2){
            auto img_line = calc_line(l*line_h, img_w, img_h, line_h,
                    t.img_x_min, t.img_x_max, t.img_y_min, t.img_y_max);
            img_line.line_no = l;
            imgs.emplace(std::move(img_line));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        for(size_t l = 1; l<t.lines; l += 2){
            auto img_line = calc_line(l*line_h, img_w, img_h, line_h,
                    t.img_x_min, t.img_x_max, t.img_y_min, t.img_y_max);
            img_line.line_no = l;
            imgs.emplace(std::move(img_line));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        prev_t = t;
    }
}

int main(int, char**) {
    double x_zoom = 0;
    double y_zoom = 0;
    double zoom_fact = 1.0;
    bool zoom = false;
    task t;
    t.img_x_max = 1.5;
    t.img_y_max = 1.5;
    t.img_x_min = -1.5;
    t.img_y_min = -1.5;
    t.lines = 10;
    t.threads = 1;
    tasks.enqueue(t);
    std::thread thr(fract_func);
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

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    struct texture{
        GLuint id;
        int x, y, w, h;
    };
    GLuint id;
    GLubyte raw[1920*1080*3]{128};
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //scale linearly when image bigger than texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //scale linearly when image smalled than texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1920, 1080, 0, GL_RGB, GL_UNSIGNED_BYTE, &raw);
    glBindTexture(GL_TEXTURE_2D, 0);
    std::vector<texture> lines;
    while (!done) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
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

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::InputInt("Lines:", (int*)&t.lines);
        ImGui::InputDouble("X1:", &t.img_x_min);
        ImGui::InputDouble("Y1:", &t.img_y_min);
        ImGui::InputDouble("X2:", &t.img_x_max);
        ImGui::InputDouble("Y2:", &t.img_y_max);
        ImGui::InputDouble("X zoom:", &x_zoom);
        ImGui::InputDouble("Y zoom:", &y_zoom);
        ImGui::InputDouble("Zoom:", &zoom_fact);
        ImGui::Checkbox("Auto zoom:", &zoom);
        ImGui::End();
        image i;
        if(io.MouseClicked[0]){
            x_zoom = io.MouseClickedPos[0].x;//TODO:demap
            y_zoom = io.MouseClickedPos[0].y;
        }

        auto size = ImGui::GetIO().DisplaySize;
        lines.resize(t.lines);
        auto drawList = ImGui::GetBackgroundDrawList();
        if(imgs.try_dequeue(i)){
            do{
                auto &line = lines.at(i.line_no);
                line.x = i.x;
                line.y = i.y;
                line.w = i.w;
                line.h = i.h;
                glBindTexture(GL_TEXTURE_2D, id);
                glTexSubImage2D(GL_TEXTURE_2D, 0, i.x, i.y,
                        i.w, i.h, GL_RGB8, GL_UNSIGNED_BYTE,
                        i.pixels.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }while(imgs.try_dequeue(i));
            std::cout<<"got imgs, sending task"<<std::endl;
            t.img_x_min = x_zoom - (x_zoom - t.img_x_min)/zoom_fact;
            t.img_x_max = x_zoom + (t.img_x_max - x_zoom)/zoom_fact;
            t.img_y_min = y_zoom - (y_zoom - t.img_y_min)/zoom_fact;
            t.img_y_max = y_zoom + (t.img_y_max - y_zoom)/zoom_fact;
            tasks.enqueue(t);
        }
        drawList->AddImage((void*)(intptr_t)id, ImVec2(0,0), ImVec2(1920, 1080));
        /*
        for(auto &line:lines){
            drawList->AddImage((void*)(intptr_t)line.id, ImVec2(line.x, line.y), ImVec2(line.w, line.h));
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize()*2.0f, ImVec2(line.x, line.y), IM_COL32_WHITE,
                "Line");
            std::cout<<"id:"<<line.id<<" x:"<<line.x<<" y:"<<line.y<<" w:"<<line.w<<" h:"<<line.h<<std::endl;
        }*/


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
    run = false;
    if(thr.joinable()){
        thr.join();
    }

    return 0;
}
