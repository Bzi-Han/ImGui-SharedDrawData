#include <WinSock2.h>
#include "ImGuiSharedDrawData.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <WS2tcpip.h>

#include <iostream>
#include <thread>

SOCKET g_dataFd = INVALID_SOCKET;

void ConnectToRenderService()
{
    WSADATA wsaData{};
    if (0 != ::WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        std::cout << "[-] WSAStartup failed" << std::endl;
        exit(0);
    }

    g_dataFd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == g_dataFd)
    {
        std::cout << "[-] Create socket fd failed" << std::endl;
        exit(0);
    }

    sockaddr_in info{};
    info.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &info.sin_addr);
    info.sin_port = htons(16888);
    if (0 != ::connect(g_dataFd, reinterpret_cast<sockaddr *>(&info), sizeof(info)))
    {
        std::cout << "[-] Connect render service failed" << std::endl;
        exit(0);
    }
}

void SendToRender(const std::vector<uint8_t> &sharedData)
{
    if (sharedData.empty())
        return;

    uint32_t packetSize = static_cast<uint32_t>(sharedData.size());
    send(g_dataFd, reinterpret_cast<char *>(&packetSize), sizeof(packetSize), 0);
    send(g_dataFd, reinterpret_cast<const char *>(sharedData.data()), static_cast<int>(sharedData.size()), 0);
}

int main()
{
    GLsizei windowWidth = 1280, windowHeight = 720;
    bool state = true, showDemoWindow = true, showAnotherWindow = true;
    ImVec4 clearColor(0.45f, 0.55f, 0.60f, 1.00f);

    // Connect to render service
    ConnectToRenderService();

    // Initialize glfw
    glfwSetErrorCallback(
        [](int error, const char *description)
        {
            std::cout << "[-] GLFW Error " << error << ": " << description << std::endl;
        });

    if (!glfwInit())
    {
        std::cout << "[-] GLFW init failed" << std::endl;
        return 1;
    }

    const char *glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize imgui
    IMGUI_CHECKVERSION();

    auto imguiContext = ImGui::CreateContext();
    if (nullptr == imguiContext)
    {
        std::cout << "[-] ImGui create context failed" << std::endl;
        return 1;
    }

    auto &imguiIO = ImGui::GetIO();

    imguiIO.IniFilename = nullptr;
    imguiIO.MouseDrawCursor = true;

    ImFontConfig fontConfig;
    fontConfig.SizePixels = 22.f;
    imguiIO.Fonts->AddFontDefault(&fontConfig);

    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(3.f);

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
    {
        std::cout << "[-] ImGui init GLFW failed" << std::endl;
        return 1;
    }
    if (!ImGui_ImplOpenGL3_Init(glslVersion))
    {
        std::cout << "[-] ImGui init OpenGL3 failed" << std::endl;
        return 1;
    }

    // Send shared font data
    auto sharedFontData = ImGui::GetSharedFontData();
    SendToRender(sharedFontData);

    // Render data
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (showDemoWindow)
            ImGui::ShowDemoWindow(&showDemoWindow);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!", &state); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");        // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &showDemoWindow); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &showAnotherWindow);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *)&clearColor); // Edit 3 floats representing a color

            if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (showAnotherWindow)
        {
            ImGui::Begin("Another Window", &showAnotherWindow); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                showAnotherWindow = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        const auto &sharedData = ImGui::GetSharedDrawData();
        if (!sharedData.empty())
        {
            SendToRender(sharedData);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(imguiContext);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}