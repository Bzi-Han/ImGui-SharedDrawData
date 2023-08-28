# ImGui-SharedDrawData

#### 介绍

通过把顶点数据与指令数据打包成字节流来实现[Dear ImGui](https://github.com/ocornut/imgui.git)的远程绘制。

#### 使用

将`modules`文件夹中的`ImGuiSharedDrawData.h`复制到你的项目中或以`git submodule`的方式引入你的项目中即可。

例子请看：

1. Server：[src/canvas.cc](https://github.com/Bzi-Han/ImGui-SharedDrawData/blob/main/src/canvas.cc)
2. Client：[src/render.cc](https://github.com/Bzi-Han/ImGui-SharedDrawData/blob/main/src/render.cc)

[screenshot.webm](https://github.com/Bzi-Han/ImGui-SharedDrawData/assets/75075077/6069ebfa-cc19-484c-8879-185b6b878dca)
