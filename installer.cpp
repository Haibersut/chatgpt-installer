#define _CRT_SECURE_NO_WARNINGS
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <cstdio>
#include <filesystem>
#include <map>
#include <windows.h>
#include "curl/curl.h"
#include "spdlog/spdlog.h"

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

std::map<std::string, std::string> url_map = {
    {"miniconda", "https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe"},
    {"git", "https://git-scm.com/download/win"},
    {"pip-official", "https://pypi.org/simple/"},
    {"pip-mirror", "https://pypi.tuna.tsinghua.edu.cn/simple"},
    {"project-official", "https://github.com/lss233/chatgpt-mirai-qq-bot.git"},
    {"project-mirror", "https://gitee.com/mattgideon/chatgpt-mirai-qq-bot"}
};

std::string get_url(const std::string& key, int mirror) {
    std::string official_key = key + "-official";
    std::string mirror_key = key + "-mirror";
    if (mirror == 1) {
        return url_map[official_key];
    }
    else {
        return url_map[mirror_key];
    }
}

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
    if (!pipe) {
        throw std::runtime_error("_popen() failed!");
    }
    try {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
    }
    catch (const std::exception& e) {
        spdlog::error("执行命令时发生错误: {}", e.what());
    }
    return result;
}

int progress_func(void* ptr, double TotalToDownload, double NowDownloaded, double TotalToUpload, double NowUploaded)
{
    if (TotalToDownload <= 0.0) return 0;
    int totalDotz = 40;
    double fractiondownloaded = NowDownloaded / TotalToDownload;
    int dotz = round(fractiondownloaded * totalDotz);

    std::cout << std::fixed;
    std::cout.precision(0);
    std::cout << "\r" << "下载进度: " << fractiondownloaded * 100 << "% [";
    int i = 0;
    for (; i < dotz; i++) {
        std::cout << "=";
    }
    for (; i < totalDotz; i++) {
        std::cout << " ";
    }
    std::cout << "]";
    std::cout.flush();
    return 0;
}

void down_miniconda() {
    CURL* curl;
    FILE* fp;
    CURLcode res;
    const char* url = url_map["miniconda"].c_str();
    char outfilename[FILENAME_MAX] = "Miniconda3-latest-Windows-x86_64.exe";
    curl = curl_easy_init();
    if (curl) {
        fp = fopen(outfilename, "wb");
        if (!fp) {
            spdlog::error("无法打开文件: {}", outfilename);
            return;
        }

        // 不知道为什么需要禁用SSL校验
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, false);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_func);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            spdlog::error("下载失败: {}", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
        fclose(fp);
    }
    spdlog::info("\n安装MINICONDA");
    system("Miniconda3-latest-Windows-x86_64.exe /InstallationType=JustMe /AddToPath=1 /RegisterPython=1 /S /D=%UserProfile%\\Miniconda3");
    spdlog::warn("MINICONDA安装完成，需要重新启动程序以使新的环境变量生效。");
    spdlog::info("按任意键退出...");
    system("pause>nul");
    std::exit(0);
}

void handle_env() {
    std::string check_conda = exec("conda env list | findstr chatgpt");
    if (check_conda.find("chatgpt") != std::string::npos)
    {
        spdlog::info("CHATGPT环境已存在");
    }
    else {
        std::string check_miniconda = exec("conda --version");
        if (check_miniconda.find("conda") != std::string::npos)
        {
            spdlog::info("MINICONDA已存在，不需要安装");
            spdlog::info("初始化CHATGPT环境");
            system("conda create -n chatgpt python=3.10 -y");
        }
        else
        {
            down_miniconda();
            spdlog::info("初始化CHATGPT环境");
            system("conda create -n chatgpt python=3.10 -y");
        }
    }
}

void handle_project(int mirror) {
    std::string check_git = exec("git --version");

    if (check_git.find("git") != std::string::npos) {
        if (std::filesystem::exists("chatgpt-mirai-qq-bot"))
        {
            spdlog::info("检查项目版本");
            system("cd chatgpt-mirai-qq-bot && git pull");
        }
        else {
            spdlog::info("开始下载chatgpt-mirai-qq-bot");
            std::string command = "git clone " + get_url("project", mirror);
            spdlog::info("当前项目源是：{}", get_url("pip", mirror));
            system(command.c_str());
        }
    }
    else {
        spdlog::error("GIT不存在，请先下载并安装GIT");
        spdlog::info("你可以在此处下载适合windows平台的GIT，{}", url_map["git"]);
    }
}

int handle_source(int choose) {
    if (choose == 1)
    {
        spdlog::info("请选择项目源：1. 官方源 2. 镜像源");
        spdlog::info("一般项目源选择官方源即可，如果下不下来可以选择镜像源");
    }
    else
    {
        spdlog::info("请选择PIP下载源：1. 官方源 2. 镜像源");
        spdlog::info("一般项目源选择镜像源，如果需要更新的话则选择官方源");
    }

    int mirror;
    std::cin >> mirror;
    while (mirror != 1 && mirror != 2) {
        spdlog::error("无效的选项，请重新输入：1. 官方源 2. 镜像源");
        std::cin >> mirror;
    }
    if (mirror == 2) {
        spdlog::info("已选择镜像源");
    }
    else {
        spdlog::info("已选择官方源");
    }
    return mirror;
}

void clear_screen() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen = { 0, 0 };
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    GetConsoleScreenBufferInfo(hConsole, &csbi);
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten);
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hConsole, coordScreen);
}

int main() {
    SetConsoleTitleA("CHATGPT QQ机器人安装程序");

    int mirror1 = handle_source(1);
    int mirror2 = handle_source(2);

    handle_env();
    handle_project(mirror1);

    // spdlog::info("当前路径是：{}", std::filesystem::current_path().string());
    spdlog::info("开始安装依赖");
    std::string command = "conda activate chatgpt && cd chatgpt-mirai-qq-bot && pip install -r requirements.txt -i " + get_url("pip", mirror2);
    spdlog::info("当前PIP镜像源是：{}", get_url("pip", mirror2));
    system(command.c_str());

    spdlog::info("启动程序");
    system("conda activate chatgpt && cd chatgpt-mirai-qq-bot && python bot.py");


    spdlog::info("按任意键退出...");
    system("pause>nul");

    return 0;
}