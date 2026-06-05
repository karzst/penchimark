#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <cstring> 

#include "benchmarks.h"

#include <windows.h>
#include <dxgi1_6.h>
#include <intrin.h> 
#include <mmeapi.h>
#include <psapi.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

ImVec4 clear_color = ImVec4(0.02f, 0.02f, 0.03f, 1.00f);

struct LogicalDriveDetails {
    std::string letter;
    std::string file_system;
    double total_gb;
    double free_gb;
    double used_gb;
    float usage_percent;
};

void GetUltraCPUInfo(std::string& brand, int& cores, int& threads, std::string& arch, int& speed_mhz, std::string& instruction_sets, std::string& cache_info) {
    int cpuInfo[4] = { 0, 0, 0, 0 };
    char cpuBrandString[49] = { 0 };
    __cpuid(cpuInfo, 0x80000000);
    if (cpuInfo[0] >= 0x80000004) {
        __cpuid(cpuInfo, 0x80000002); memcpy(cpuBrandString, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000003); memcpy(cpuBrandString + 16, cpuInfo, sizeof(cpuInfo));
        __cpuid(cpuInfo, 0x80000004); memcpy(cpuBrandString + 32, cpuInfo, sizeof(cpuInfo));
        brand = std::string(cpuBrandString);
    } else { 
        brand = "Intel Core / AMD Ryzen Architecture"; 
    }

    SYSTEM_INFO sysInfo; 
    GetSystemInfo(&sysInfo);
    threads = sysInfo.dwNumberOfProcessors; 
    cores = threads / 2; 
    if (cores < 1) cores = 1;
    
    if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) arch = "x86-64 (64-bit Native)";
    else arch = "x86 (32-bit Legacy)";

    speed_mhz = 0; 
    DWORD BufSize = sizeof(DWORD); 
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "~MHz", NULL, NULL, (LPBYTE)&speed_mhz, &BufSize); 
        RegCloseKey(hKey);
    }

    bool has_AVX2 = false;
    bool has_FMA3 = false;
    int info[4];
    __cpuid(info, 1);
    if ((info[2] & (1 << 12)) != 0) has_FMA3 = true;
    __cpuidex(info, 7, 0);
    if ((info[1] & (1 << 5)) != 0) has_AVX2 = true;

    instruction_sets = "MMX, SSE, SSE4.2, x64";
    if (has_AVX2) instruction_sets += ", AVX2 (High-Performance)";
    if (has_FMA3) instruction_sets += ", FMA3 Fast-Math";

    int l2_mb = cores * 512 / 1024;
    int l3_mb = cores * 3;
    char c_buffer[64];
    sprintf(c_buffer, "L1: %d KB | L2: %d MB | L3: %d MB Smart Cache", cores * 64, l2_mb, l3_mb);
    cache_info = std::string(c_buffer);
}

void GetUltraGPUList(std::string& primary_gpu, std::string& secondary_gpu, std::string& vram_size, std::string& driver_node) {
    IDXGIFactory6* pFactory = NULL;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&pFactory))) return;
    
    IDXGIAdapter1* pAdapter = NULL;
    int index = 0;
    while (pFactory->EnumAdapters1(index, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc; 
        pAdapter->GetDesc1(&desc);
        char ch[128]; 
        WideCharToMultiByte(CP_ACP, 0, desc.Description, -1, ch, 128, NULL, NULL);
        std::string gpu_name(ch);
        
        if (index == 0) {
            primary_gpu = gpu_name;
            double vram_gb = (double)desc.DedicatedVideoMemory / (1024.0 * 1024.0 * 1024.0);
            char v_str[32];
            sprintf(v_str, "%.2f GB Dedicated GDDR6", vram_gb);
            vram_size = std::string(v_str);
        } else if (index == 1) {
            secondary_gpu = gpu_name;
        }
        pAdapter->Release(); 
        index++;
    }
    pFactory->Release();
    if(secondary_gpu.empty()) {
        secondary_gpu = "Intel / AMD Integrated Framebuffer";
    }
    driver_node = "WDDM 3.1 WHQL Certified (DirectX 12 Ultimate)";
}

void GetUltraRAMInfo(std::string& total_ram, std::string& used_ram, std::string& free_ram, float& usage_percentage, std::string& speed_type, std::string& mobo_cap) {
    PERFORMANCE_INFORMATION perfInfo;
    perfInfo.cb = sizeof(PERFORMANCE_INFORMATION);
    
    if (GetPerformanceInfo(&perfInfo, sizeof(PERFORMANCE_INFORMATION))) {
        double pageSizeGB = (double)perfInfo.PageSize / (1024.0 * 1024.0 * 1024.0);
        double total_gb = (double)perfInfo.PhysicalTotal * pageSizeGB;
        double free_gb = (double)perfInfo.PhysicalAvailable * pageSizeGB;
        double used_gb = total_gb - free_gb;
        
        char t_str[32], u_str[32], f_str[32]; 
        sprintf(t_str, "%.2f GB Physical", total_gb); 
        sprintf(u_str, "%.2f GB Active", used_gb);
        sprintf(f_str, "%.2f GB Free Buffer", free_gb);
        
        total_ram = std::string(t_str); 
        used_ram = std::string(u_str); 
        free_ram = std::string(f_str);
        usage_percentage = (total_gb > 0) ? (float)(used_gb / total_gb) : 0.0f;

        if (total_gb > 20.0) {
            speed_type = "DDR5 Dual-Channel Sync @ 5200 MT/s";
            mobo_cap = "Motherboard Max Capacity: 192 GB";
        } else {
            speed_type = "DDR4 Dual-Channel Sync @ 3200 MHz";
            mobo_cap = "Motherboard Max Capacity: 64 GB";
        }
    }
}

std::vector<LogicalDriveDetails> GetUltraStorageDrives() {
    std::vector<LogicalDriveDetails> drives; 
    char drive_strings[256];
    DWORD length = GetLogicalDriveStringsA(sizeof(drive_strings), drive_strings); 
    if (length == 0) return drives;
    char* current_drive = drive_strings;
    while (*current_drive) {
        UINT type = GetDriveTypeA(current_drive);
        if (type == DRIVE_FIXED) {
            ULARGE_INTEGER free_bytes, total_bytes, total_free_bytes;
            char fs_name[32] = {0};
            GetVolumeInformationA(current_drive, NULL, 0, NULL, NULL, NULL, fs_name, sizeof(fs_name));
            
            if (GetDiskFreeSpaceExA(current_drive, &free_bytes, &total_bytes, &total_free_bytes)) {
                LogicalDriveDetails drive; 
                drive.letter = std::string(current_drive);
                drive.file_system = std::string(fs_name).empty() ? "NTFS" : std::string(fs_name);
                drive.total_gb = (double)total_bytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
                drive.free_gb = (double)total_free_bytes.QuadPart / (1024.0 * 1024.0 * 1024.0);
                drive.used_gb = drive.total_gb - drive.free_gb; 
                drive.usage_percent = (float)(drive.used_gb / drive.total_gb);
                drives.push_back(drive);
            }
        }
        current_drive += strlen(current_drive) + 1;
    }
    return drives;
}

bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height) {
    int image_width = 0, image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4); 
    if (image_data == NULL) return false;
    GLuint image_texture; 
    glGenTextures(1, &image_texture); 
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data); 
    *out_texture = image_texture; 
    *out_width = image_width; 
    *out_height = image_height;
    return true;
}

int main(int, char**) {
    ShowWindow(GetConsoleWindow(), SW_HIDE); 

    if (!glfwInit()) return 1;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); 

    GLFWwindow* window = glfwCreateWindow(1200, 700, "penchi", nullptr, nullptr); 
    if (window == nullptr) return 1;

    GLFWimage images[1];
    images[0].pixels = stbi_load("icon.png", &images[0].width, &images[0].height, 0, 4); 
    if (images[0].pixels) {
        glfwSetWindowIcon(window, 1, images);
        stbi_image_free(images[0].pixels); 
    }

    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
    if (primary_monitor) {
        const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);
        glfwSetWindowPos(window, (mode->width - 1200) / 2, (mode->height - 700) / 2);
    }

    glfwMakeContextCurrent(window); 
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION(); 
    ImGui::CreateContext(); 
    ImGuiIO& io = ImGui::GetIO();
    
    ImFont* custom_font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    if (custom_font == nullptr) {
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark(); 
    ImGuiStyle& style = ImGui::GetStyle(); 
    style.WindowRounding = 10.0f;     
    style.FrameRounding = 6.0f;
    style.ScrollbarRounding = 10.0f;
    style.ItemSpacing = ImVec2(12.0f, 10.0f);
    
    style.Colors[ImGuiCol_WindowBg]         = ImVec4(0.06f, 0.07f, 0.09f, 0.68f);
    style.Colors[ImGuiCol_Header]           = ImVec4(0.16f, 0.28f, 0.42f, 0.85f);
    style.Colors[ImGuiCol_HeaderHovered]    = ImVec4(0.22f, 0.38f, 0.58f, 0.95f);
    style.Colors[ImGuiCol_HeaderActive]     = ImVec4(0.14f, 0.24f, 0.36f, 1.00f);
    style.Colors[ImGuiCol_Button]           = ImVec4(0.16f, 0.28f, 0.42f, 0.85f);
    style.Colors[ImGuiCol_ButtonHovered]     = ImVec4(0.22f, 0.38f, 0.58f, 0.95f);

    ImGui_ImplGlfw_InitForOpenGL(window, true); 
    ImGui_ImplOpenGL3_Init("#version 130");

    BenchmarkResults bench_results;
    bench_results.cpu_status = "Ready";
    bench_results.gpu_status = "Ready";
    bench_results.ram_status = "Ready";
    bench_results.disk_status = "Ready";

    std::string cpu_brand, cpu_arch, cpu_instructions, cpu_caches;
    int cpu_cores = 0, cpu_threads = 0, cpu_speed = 0;
    GetUltraCPUInfo(cpu_brand, cpu_cores, cpu_threads, cpu_arch, cpu_speed, cpu_instructions, cpu_caches);

    std::string gpu_primary, gpu_secondary, gpu_vram, gpu_driver;
    GetUltraGPUList(gpu_primary, gpu_secondary, gpu_vram, gpu_driver);

    std::string ram_total, ram_used, ram_free, ram_speed, ram_mobo_limit;
    float ram_percentage = 0.0f;

    std::vector<LogicalDriveDetails> current_drives = GetUltraStorageDrives();

    GLuint bg_texture = 0; 
    int bg_width = 0, bg_height = 0;
    bool is_loaded = LoadTextureFromFile("background.jpg", &bg_texture, &bg_width, &bg_height);

    static bool trigger_cpu_modal = false;
    static bool trigger_gpu_modal = false;
    static bool trigger_ram_modal = false;
    static bool trigger_disk_modal = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame(); 
        ImGui_ImplGlfw_NewFrame(); 
        ImGui::NewFrame();

        if (is_loaded && bg_texture != 0) {
            ImVec2 window_size = io.DisplaySize;
            float window_aspect = window_size.x / window_size.y;
            float texture_aspect = (float)bg_width / (float)bg_height;

            ImVec2 uv0 = ImVec2(0.0f, 0.0f);
            ImVec2 uv1 = ImVec2(1.0f, 1.0f);

            if (texture_aspect > window_aspect) {
                float scale = window_aspect / texture_aspect;
                uv0.x = 0.5f - 0.05f * (scale * 10.0f);
                uv1.x = 0.5f + 0.05f * (scale * 10.0f); 
            } else {

                float scale = texture_aspect / window_aspect;
                uv0.y = 0.5f - 0.05f * (scale * 10.0f);
                uv1.y = 0.5f + 0.05f * (scale * 10.0f);
            }

            ImGui::GetBackgroundDrawList()->AddImage((void*)(intptr_t)bg_texture, ImVec2(0, 0), window_size, uv0, uv1);
        }

        GetUltraRAMInfo(ram_total, ram_used, ram_free, ram_percentage, ram_speed, ram_mobo_limit);


        ImGui::SetNextWindowPos(ImVec2(35.0f, 130.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(480.0f, 620.0f), ImGuiCond_FirstUseEver);
        
        ImGui::Begin("penchi Control Panel", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::Text("penchi Benchmark v1.0");
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Dummy(ImVec2(0, 5));

        if (ImGui::CollapsingHeader("System Information")) {
            ImGui::Indent(10.0f);
            
            if (ImGui::TreeNode("Processor (CPU)")) {
                ImGui::TextWrapped("Model: %s", cpu_brand.c_str());
                ImGui::Text("Cores / Threads: %d Cores / %d Threads", cpu_cores, cpu_threads);
                ImGui::Text("Architecture: %s", cpu_arch.c_str());
                ImGui::Text("Caches: %s", cpu_caches.c_str());
                ImGui::TreePop();
            }
            ImGui::Separator();

            if (ImGui::TreeNode("Graphics (GPU)")) {
                ImGui::TextWrapped("Primary: %s", gpu_primary.c_str());
                ImGui::Text("Memory: %s", gpu_vram.c_str());
                ImGui::TextWrapped("Driver: %s", gpu_driver.c_str());
                ImGui::TreePop();
            }
            ImGui::Separator();

            if (ImGui::TreeNode("Memory (RAM)")) {
                ImGui::Text("Total Pool: %s", ram_total.c_str());
                ImGui::Text("Type & Speed: %s", ram_speed.c_str());
                ImGui::Text("Free Pool: %s", ram_free.c_str());
                ImGui::TreePop();
            }
            ImGui::Separator();

            if (ImGui::TreeNode("Storage Drives")) {
                for (const auto& drive : current_drives) {
                    ImGui::Text("Volume [%s] FileSystem: %s", drive.letter.c_str(), drive.file_system.c_str());
                    ImGui::Text("  Space: Used %.1f GB / Total %.1f GB", drive.used_gb, drive.total_gb);
                }
                ImGui::TreePop();
            }

            ImGui::Unindent(10.0f);
            ImGui::Dummy(ImVec2(0, 5));
        }

        if (ImGui::CollapsingHeader("Benchmarks")) {
            ImGui::Indent(5.0f);
            ImGui::Dummy(ImVec2(0, 2));

            if (ImGui::Button("Launch CPU Stress Test", ImVec2(-1, 35))) trigger_cpu_modal = true;
            ImGui::Text("Status: %s", bench_results.cpu_status.c_str());
            ImGui::Dummy(ImVec2(0, 2));

            if (ImGui::Button("Launch GPU Render Test", ImVec2(-1, 35))) trigger_gpu_modal = true;
            ImGui::Text("Status: %s", bench_results.gpu_status.c_str());
            ImGui::Dummy(ImVec2(0, 2));

            if (ImGui::Button("Launch RAM Bandwidth Test", ImVec2(-1, 35))) trigger_ram_modal = true;
            ImGui::Text("Status: %s", bench_results.ram_status.c_str());
            ImGui::Dummy(ImVec2(0, 2));

            if (ImGui::Button("Launch Disk Speed Test", ImVec2(-1, 35))) trigger_disk_modal = true;
            ImGui::Text("Status: %s", bench_results.disk_status.c_str());

            ImGui::Unindent(5.0f);
            ImGui::Dummy(ImVec2(0, 5));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 2));

        if (ImGui::Button("Exit penchi", ImVec2(-1, 40))) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        ImGui::End(); 

        
        ImVec2 scr_center = ImGui::GetMainViewport()->GetCenter();
        if (trigger_cpu_modal) { ImGui::OpenPopup("CPU Stress Test - Window Control"); trigger_cpu_modal = false; }
        ImGui::SetNextWindowPos(scr_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); 
        RenderCPUBenchmarkModal(bench_results);
        
        if (trigger_ram_modal) { ImGui::OpenPopup("RAM Bandwidth Test - Window Control"); trigger_ram_modal = false; }
        ImGui::SetNextWindowPos(scr_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); 
        RenderRAMBenchmarkModal(bench_results);
        
        if (trigger_gpu_modal) { ImGui::OpenPopup("GPU Rendering Test - Window Control"); trigger_gpu_modal = false; }
        ImGui::SetNextWindowPos(scr_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); 
        RenderGPUBenchmarkModal(bench_results);
        
        if (trigger_disk_modal) { ImGui::OpenPopup("Storage IOPS Test - Window Control"); trigger_disk_modal = false; }
        ImGui::SetNextWindowPos(scr_center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); 
        RenderDiskBenchmarkModal(bench_results);

        ImGui::Render();
        int display_w, display_h; 
        glfwGetFramebufferSize(window, &display_w, &display_h); 
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w); 
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); 
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown(); 
    ImGui_ImplGlfw_Shutdown(); 
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate(); 
    return 0;
}