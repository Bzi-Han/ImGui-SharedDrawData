#include <WinSock2.h>
#include "ImGuiSharedDrawData.h"

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>

enum class RenderState
{
    SetFont,
    Rendering,
    ReadData,
};

std::atomic<RenderState> g_renderState = RenderState::ReadData;
std::vector<uint8_t> g_sharedFontData;
std::vector<uint8_t> g_sharedDrawData, g_sharedDrawDataBack;

bool g_work = true;
size_t g_maxPacketSize = 1 * 1024 * 1024; // 1MB
SOCKET g_dataFd = INVALID_SOCKET;

int ReadData(void *buffer, size_t readSize)
{
    size_t packetReaded = 0;

    while (packetReaded < readSize)
    {
        auto readResult = ::recv(g_dataFd, reinterpret_cast<char *>(buffer) + packetReaded, static_cast<int>(readSize - packetReaded), 0);
        if (0 >= readResult)
            return readResult;
        packetReaded += readResult;
    }

    return static_cast<int>(packetReaded);
}
void WriteData(void *data, size_t size)
{
    ::send(g_dataFd, reinterpret_cast<char *>(data), static_cast<int>(size), 0);
}

void RenderService()
{
    WSADATA wsaData{};
    if (0 != ::WSAStartup(MAKEWORD(2, 2), &wsaData))
    {
        std::cout << "[-] WSAStartup failed" << std::endl;
        exit(0);
    }

    auto fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == fd)
    {
        std::cout << "[-] Create socket fd failed" << std::endl;
        exit(0);
    }

    sockaddr_in info{};
    info.sin_family = AF_INET;
    info.sin_addr.S_un.S_addr = ADDR_ANY;
    info.sin_port = htons(16888);
    if (SOCKET_ERROR == ::bind(fd, reinterpret_cast<sockaddr *>(&info), sizeof(info)))
    {
        std::cout << "[-] Bind socket fd failed" << std::endl;
        exit(0);
    }

    if (SOCKET_ERROR == ::listen(fd, 1))
    {
        std::cout << "[-] Listen socket fd failed" << std::endl;
        exit(0);
    }

    while (g_work)
    {
        g_dataFd = INVALID_SOCKET;
        g_sharedFontData.clear();
        g_dataFd = ::accept(fd, nullptr, nullptr);
        if (INVALID_SOCKET == g_dataFd)
        {
            std::cout << "[-] Accept returned invalid socket fd" << std::endl;
            exit(0);
        }

        uint32_t packetSize = 0, packetReaded = 0;
        while (g_work)
        {
            if (static_cast<int>(sizeof(packetSize)) > ReadData(&packetSize, sizeof(packetSize)))
            {
                std::cout << "[-] Can not read packet size: " << ::WSAGetLastError() << std::endl;
                break;
            }
            if (packetSize > g_maxPacketSize)
            {
                std::cout << "[-] Packet is too large: " << std::setprecision(2) << std::fixed << (packetSize / 1024.f / 1024.f) << std::endl;
                break;
            }

            if (g_sharedDrawDataBack.size() < packetSize)
                g_sharedDrawDataBack.resize(packetSize);
            auto readResult = ReadData(g_sharedDrawDataBack.data(), packetSize);
            if (0 >= readResult)
            {
                std::cout << "[-] Client disconnect or read failed, readResult:" << readResult << std::endl;
                break;
            }

            if (RenderState::Rendering == g_renderState)
                continue;
            if (g_sharedFontData.empty()) // NOTE: First packet is font data
            {
                g_sharedFontData.swap(g_sharedDrawDataBack);
                g_renderState = RenderState::SetFont;
            }
            else
            {
                g_sharedDrawData.swap(g_sharedDrawDataBack);
                g_renderState = RenderState::Rendering;
            }
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

        switch (g_renderState)
        {
        case RenderState::SetFont:
        {
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui::SetSharedFontData(g_sharedFontData);
            ImGui_ImplOpenGL3_CreateFontsTexture();
            break;
        }
        case RenderState::Rendering:
        {
            auto sharedData = ImGui::RenderSharedDrawData(g_sharedDrawData);
            if (nullptr != sharedData)
            {
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(sharedData);
                glfwSwapBuffers(window);
            }
            g_renderState = RenderState::ReadData;

            break;
        }
        default:
            break;
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