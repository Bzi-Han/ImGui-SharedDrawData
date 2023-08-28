#include <WinSock2.h>
#include "ImGuiSharedDrawData.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <afunix.h>

#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>

std::atomic_bool g_bufferReadLock = false;
std::vector<uint8_t> g_sharedDrawData;
bool g_work = true;
size_t g_maxPacketSize = 1 * 1024 * 1024; // 1MB
std::mutex g_renderingMutex;
std::condition_variable g_renderCompleteSignal;

void RenderService()
{
    WSADATA wsaData{};
    if (0 != ::WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        std::cout << "[-] WSAStartup failed" << std::endl;
        exit(0);
    }

    auto fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (INVALID_SOCKET == fd)
    {
        std::cout << "[-] Create socket fd failed" << std::endl;
        exit(0);
    }

    sockaddr_un info{};
    info.sun_family = AF_UNIX;
    strcpy_s(info.sun_path, "socket.RenderService");
    std::filesystem::remove("socket.RenderService");
    if (SOCKET_ERROR == ::bind(fd, reinterpret_cast<sockaddr *>(&info), sizeof(sockaddr_un)))
    {
        std::cout << "[-] Bind socket fd failed" << std::endl;
        exit(0);
    }

    if (SOCKET_ERROR == ::listen(fd, 0))
    {
        std::cout << "[-] Listen socket fd failed" << std::endl;
        exit(0);
    }

    auto dataFd = ::accept(fd, nullptr, nullptr);
    if (INVALID_SOCKET == dataFd)
    {
        std::cout << "[-] Accept returned invalid socket fd" << std::endl;
        exit(0);
    }

    uint32_t packetSize = 0, packetReaded = 0;
    while (g_work)
    {
        if (static_cast<int>(sizeof(packetSize)) > ::recv(dataFd, reinterpret_cast<char *>(&packetSize), sizeof(packetSize), 0))
        {
            std::cout << "[-] Can not read packet size: " << ::WSAGetLastError() << std::endl;
            break;
        }
        if (packetSize > g_maxPacketSize)
        {
            std::cout << "[-] Packet is too large: " << std::setprecision(2) << std::fixed << (packetSize / 1024.f / 1024.f) << std::endl;
            break;
        }

        g_bufferReadLock = true;
        if (g_sharedDrawData.size() < packetSize)
            g_sharedDrawData.resize(packetSize);
        packetReaded = 0;
        while (packetReaded < packetSize)
        {
            auto readResult = ::recv(dataFd, reinterpret_cast<char *>(g_sharedDrawData.data() + packetReaded), packetSize - packetReaded, 0);
            if (0 >= readResult)
            {
                std::cout << "[-] Client disconnect or read failed: " << readResult << std::endl;
                exit(0);
            }
            packetReaded += readResult;
        }
        g_bufferReadLock = false;

        // Wait for rendering complete
        {
            std::unique_lock<std::mutex> locker(g_renderingMutex);

            g_renderCompleteSignal.wait(locker);
        }
    }

    ::shutdown(fd, 0);
    ::WSACleanup();
}

int main()
{
    GLsizei windowWidth = 1280, windowHeight = 720;
    ImVec4 clearColor(0.45f, 0.55f, 0.60f, 1.00f);

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

    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w);

    // Start render service
    std::thread renderServiceThread(RenderService);

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

        // Rendering
        if (!g_bufferReadLock)
        {
            auto sharedData = ImGui::RenderSharedDrawData(g_sharedDrawData);
            if (nullptr != sharedData)
            {
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(sharedData);
                g_renderCompleteSignal.notify_one();
                glfwSwapBuffers(window);
            }
        }
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(imguiContext);

    glfwDestroyWindow(window);
    glfwTerminate();

    g_work = false;
    if (renderServiceThread.joinable())
        renderServiceThread.join();

    return 0;
}