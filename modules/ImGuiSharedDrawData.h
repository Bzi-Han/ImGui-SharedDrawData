#ifndef IMGUI_SHARED_DRAWDATA_H // !IMGUI_SHARED_DRAWDATA_H
#define IMGUI_SHARED_DRAWDATA_H

#include <imgui/imgui.h>

#include <stdint.h>

#include <string>
#include <vector>
#include <unordered_set>

namespace ImGui
{
    std::vector<uint8_t> GetSharedFontData()
    {
        auto &imguiIO = ImGui::GetIO();

        std::vector<uint8_t> sharedFontData;
        uint8_t *pixelData = nullptr;
        int width = 0, height = 0;

        auto WriteData = [&](void *data, size_t size)
        {
            auto begin = reinterpret_cast<uint8_t *>(data);
            auto end = begin + size;
            sharedFontData.insert(sharedFontData.end(), begin, end);
        };

        imguiIO.Fonts->GetTexDataAsAlpha8(&pixelData, &width, &height);
        if (1 > width || 1 > height)
            return sharedFontData;

        // Write width
        WriteData(&width, sizeof(width));
        // Write height
        WriteData(&height, sizeof(height));
        // Write data
        WriteData(pixelData, width * height);

        return sharedFontData;
    }

    void SetSharedFontData(const std::vector<uint8_t> &data)
    {
        auto &imguiIO = ImGui::GetIO();

        if (data.empty() || 8 > data.size())
            return;
        if (!imguiIO.Fonts->IsBuilt())
        {
            if (!imguiIO.Fonts->Build())
                return;
        }

        size_t readIndex = 0;
        int originDataSize = imguiIO.Fonts->TexWidth * imguiIO.Fonts->TexHeight;

        // Read width
        memcpy(&imguiIO.Fonts->TexWidth, data.data() + readIndex, sizeof(imguiIO.Fonts->TexWidth));
        readIndex += sizeof(imguiIO.Fonts->TexWidth);
        // Read height
        memcpy(&imguiIO.Fonts->TexHeight, data.data() + readIndex, sizeof(imguiIO.Fonts->TexHeight));
        readIndex += sizeof(imguiIO.Fonts->TexHeight);
        // Read data
        int newDataSize = imguiIO.Fonts->TexWidth * imguiIO.Fonts->TexHeight;
        if (originDataSize < newDataSize)
        {
            IM_FREE(imguiIO.Fonts->TexPixelsAlpha8);
            imguiIO.Fonts->TexPixelsAlpha8 = reinterpret_cast<uint8_t *>(IM_ALLOC(newDataSize));
        }
        memcpy(imguiIO.Fonts->TexPixelsAlpha8, data.data() + readIndex, newDataSize);

        if (nullptr != imguiIO.Fonts->TexPixelsRGBA32)
        {
            IM_FREE(imguiIO.Fonts->TexPixelsRGBA32);
            imguiIO.Fonts->TexPixelsRGBA32 = nullptr;
        }
    }

    const std::vector<uint8_t> &GetSharedDrawData()
    {
        static std::vector<uint8_t> sharedDrawData;
        auto WriteData = [](void *data, size_t size)
        {
            auto begin = reinterpret_cast<uint8_t *>(data);
            auto end = begin + size;
            sharedDrawData.insert(sharedDrawData.end(), begin, end);
        };

        auto drawData = ImGui::GetDrawData();

        sharedDrawData.clear();
        // Write cmd list count
        WriteData(&drawData->CmdListsCount, sizeof(drawData->CmdListsCount));
        // Write display pos
        WriteData(&drawData->DisplayPos, sizeof(drawData->DisplayPos));
        // Write frame buffer scale
        WriteData(&drawData->FramebufferScale, sizeof(drawData->FramebufferScale));

        for (const auto &cmdList : drawData->CmdLists)
        {
            // Vertex buffer
            {
                // Write vertexes count
                WriteData(&cmdList->VtxBuffer.Size, sizeof(cmdList->VtxBuffer.Size));
                // Write vertexes
                WriteData(cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            }

            // Index buffer
            {
                // Write indexes count
                WriteData(&cmdList->IdxBuffer.Size, sizeof(cmdList->IdxBuffer.Size));
                // Write indexes
                WriteData(cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            }

            // Cmd buffer
            {
                // Write cmds count
                WriteData(&cmdList->CmdBuffer.Size, sizeof(cmdList->CmdBuffer.Size));
                // Write cmds
                auto sharedCmds = reinterpret_cast<ImDrawCmd *>(sharedDrawData.data() + sharedDrawData.size());
                WriteData(cmdList->CmdBuffer.Data, cmdList->CmdBuffer.Size * sizeof(ImDrawCmd));
                for (int i = 0; i < cmdList->CmdBuffer.Size; ++i)
                {
                    sharedCmds[i].UserCallback = nullptr;
                    sharedCmds[i].UserCallbackData = nullptr;
                }
            }
        }

        return sharedDrawData;
    }

    ImDrawData *RenderSharedDrawData(const std::vector<uint8_t> &data)
    {
        size_t readIndex = 0;

        if (data.empty())
            return ImGui::GetDrawData();

        // Read cmd lists count
        int cmdListsCount = *reinterpret_cast<int *>(const_cast<uint8_t *>(data.data()));
        readIndex += sizeof(cmdListsCount);
        if (1 > cmdListsCount)
            return ImGui::GetDrawData();

        // Prepare draw data
        static bool showWindow = true;
        auto drawData = ImGui::GetDrawData();
        // Clean slice data
        if (nullptr != drawData && 0 < drawData->CmdListsCount)
        {
            for (auto &cmdList : drawData->CmdLists)
            {
                cmdList->VtxBuffer.Data = nullptr;
                cmdList->IdxBuffer.Data = nullptr;
                cmdList->CmdBuffer.Data = nullptr;
            }
        }
        // Make cmd lists
        static std::vector<std::string> windowNames;
        if (cmdListsCount > static_cast<int>(windowNames.size()))
        {
            for (int i = static_cast<int>(windowNames.size()); i < cmdListsCount; ++i)
                windowNames.emplace_back("Window" + std::to_string(i));
        }
        do
        {
            ImGui::NewFrame();
            for (int i = 0; i < cmdListsCount; ++i)
            {
                ImGui::Begin(windowNames[i].data(), &showWindow);
                ImGui::End();
            }
            ImGui::Render();

            drawData = ImGui::GetDrawData();
        } while (0 == drawData->CmdListsCount);

        // Read display pos
        memcpy(&drawData->DisplayPos, data.data() + readIndex, sizeof(drawData->DisplayPos));
        readIndex += sizeof(drawData->DisplayPos);
        // Read frame buffer scale
        memcpy(&drawData->FramebufferScale, data.data() + readIndex, sizeof(drawData->FramebufferScale));
        readIndex += sizeof(drawData->FramebufferScale);

        for (auto &cmdList : drawData->CmdLists)
        {
            // Slice vertex buffer
            cmdList->VtxBuffer.clear();
            cmdList->VtxBuffer.Size = *reinterpret_cast<decltype(cmdList->VtxBuffer.Size) *>(const_cast<uint8_t *>(data.data()) + readIndex);
            readIndex += sizeof(cmdList->VtxBuffer.Size);
            cmdList->VtxBuffer.Data = reinterpret_cast<decltype(cmdList->VtxBuffer.Data)>(const_cast<uint8_t *>(data.data()) + readIndex);
            readIndex += cmdList->VtxBuffer.Size * sizeof(ImDrawVert);

            // Slice index buffer
            cmdList->IdxBuffer.clear();
            cmdList->IdxBuffer.Size = *reinterpret_cast<decltype(cmdList->IdxBuffer.Size) *>(const_cast<uint8_t *>(data.data()) + readIndex);
            readIndex += sizeof(cmdList->IdxBuffer.Size);
            cmdList->IdxBuffer.Data = reinterpret_cast<decltype(cmdList->IdxBuffer.Data)>(const_cast<uint8_t *>(data.data()) + readIndex);
            readIndex += cmdList->IdxBuffer.Size * sizeof(ImDrawIdx);

            // Slice cmd buffer
            cmdList->CmdBuffer.clear();
            cmdList->CmdBuffer.Size = *reinterpret_cast<decltype(cmdList->CmdBuffer.Size) *>(const_cast<uint8_t *>(data.data()) + readIndex);
            readIndex += sizeof(cmdList->CmdBuffer.Size);
            cmdList->CmdBuffer.Data = reinterpret_cast<decltype(cmdList->CmdBuffer.Data)>(const_cast<uint8_t *>(data.data()) + readIndex);
            readIndex += cmdList->CmdBuffer.Size * sizeof(ImDrawCmd);
        }

        return drawData;
    }
}

#endif //! IMGUI_SHARED_DRAWDATA_H