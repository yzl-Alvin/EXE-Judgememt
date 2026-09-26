/*
    Windows 简易评测机 (绿色便携版)
    - 编译器: 使用本文件夹内 MinGW32\bin\g++.exe, 无需配置 PATH
    - 源代码: 固定为 code.cpp
    - 输入题号: 评测 data\题号\ 下所有测试点, 每次运行只判一题
    - Dev-C++ 编译设置见代码末尾说明
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <windows.h>
#include <psapi.h>          // GetProcessMemoryInfo 需要它, 链接时加 -lpsapi

using namespace std;

// ================== 可按需修改的配置 ==================
const string MINGW_DIR = "MinGW32";   // 编译器文件夹名, 与 checker.exe 同级
// =====================================================

enum JudgeResult { AC, WA, TLE, MLE, RE, CE };

// ---------- 结果转字符串 ----------
string resultName(JudgeResult r) {
    switch (r) {
        case AC:  return "AC";
        case WA:  return "WA";
        case TLE: return "TLE";
        case MLE: return "MLE";
        case RE:  return "RE";
        case CE:  return "CE";
    }
    return "??";
}

// ---------- 去掉行首行尾空白 (比较输出时用) ----------
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// ---------- 拿到 checker.exe 自己所在的文件夹 ----------
string getExeDir() {
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);   // 取本程序完整路径
    string path = buf;
    size_t pos = path.find_last_of("\\/");     // 找最后一个斜杠
    return path.substr(0, pos);                // 截掉文件名, 剩下就是文件夹
}

// ---------- 编译 code.cpp -> code.exe ----------
JudgeResult compileUserCode(const string& sourceFile, const string& exeFile) {
    string compiler = getExeDir() + "\\" + MINGW_DIR + "\\bin\\g++.exe";

    string command = "\"" + compiler + "\" \"" + sourceFile + "\" -o \"" + exeFile
                   + "\" -O2 -std=c++11 -static-libgcc -static-libstdc++"
                   + " > compile_error.log 2>&1";

    DeleteFileA(exeFile.c_str());

    // ★ 关键修改: 整条命令外面再包一层引号
    // cmd 会吞掉最外层这对新引号, 里面完整引好的命令原样保留
    int ret = system(("\"" + command + "\"").c_str());

    if (ret != 0) {
        cout << "=== 编译失败 (CE), 错误信息如下 ===" << endl;
        ifstream log("compile_error.log");
        string line;
        while (getline(log, line)) cout << line << endl;
        cout << "====================================" << endl;
        return CE;
    }
    return AC;
}


// ---------- 运行一个测试点, 顺便统计用时 ----------
JudgeResult runTestCase(const string& exeFile, const string& inputFile,
                        const string& outputFile, int timeLimit,
                        int memoryLimit, int& usedTime)
{
    usedTime = 0;

    // 可继承的安全属性, 让子进程能用我们打开的文件句柄
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    // 1. 打开输入文件 (作为子进程的 stdin)
    HANDLE hInput = CreateFileA(inputFile.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hInput == INVALID_HANDLE_VALUE) return RE;

    // 2. 创建输出文件 (作为子进程的 stdout 和 stderr)
    HANDLE hOutput = CreateFileA(outputFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                 &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOutput == INVALID_HANDLE_VALUE) {
        CloseHandle(hInput);
        return RE;
    }

    // 3. 配置子进程的标准输入/输出/错误
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = hInput;
    si.hStdOutput = hOutput;
    si.hStdError  = hOutput;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // 4. 创建子进程运行 code.exe
    string cmdLine = "\"" + exeFile + "\"";
    vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');   // CreateProcess 可能修改命令行, 必须传可写缓冲区

    BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hInput);      // 句柄已传给子进程, 本进程的可以关了
    CloseHandle(hOutput);
    if (!ok) return RE;

    // 5. 监控循环: 超时 / 超内存 / 正常退出
    DWORD startTime = GetTickCount();
    bool isTLE = false, isMLE = false;

    while (true) {
        DWORD code;
        GetExitCodeProcess(pi.hProcess, &code);
        if (code != STILL_ACTIVE) break;    // 程序自己跑完了

        if (GetTickCount() - startTime > (DWORD)timeLimit) {   // 超时
            isTLE = true;
            TerminateProcess(pi.hProcess, 1);
            break;
        }

        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc))
            && pmc.WorkingSetSize > (SIZE_T)memoryLimit) {     // 超内存
            isMLE = true;
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        Sleep(10);   // 每 10ms 查一次, 降低 CPU 占用
    }

    usedTime = (int)(GetTickCount() - startTime);

    DWORD finalCode = 0;
    GetExitCodeProcess(pi.hProcess, &finalCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (isTLE) return TLE;
    if (isMLE) return MLE;
    if (finalCode != 0) return RE;   // 非零退出码 = 运行时错误 (含崩溃)
    return AC;
}

// ---------- 比对输出: 以标准答案为准逐行比, 忽略行尾空白 ----------
bool compareOutput(const string& userFile, const string& stdFile) {
    ifstream userOut(userFile.c_str());
    ifstream stdOut(stdFile.c_str());
    if (!userOut.is_open() || !stdOut.is_open()) return false;

    string uLine, sLine;
    while (getline(stdOut, sLine)) {                 // 先读标准答案的一行
        if (!getline(userOut, uLine)) return false;  // 用户输出行数不够 -> WA
        if (trim(uLine) != trim(sLine)) return false;
    }
    // 标准答案读完后, 用户输出只允许剩空行
    while (getline(userOut, uLine))
        if (!trim(uLine).empty()) return false;
    return true;
}

// ---------- 从 "3.in" 提取数字 3, 不是纯数字编号返回 -1 ----------
int extractCaseNumber(const string& filename) {
    size_t dot = filename.find('.');
    if (dot == string::npos) return -1;
    string num = filename.substr(0, dot);
    if (num.empty()) return -1;
    if (num.find_first_not_of("0123456789") != string::npos) return -1;
    return stoi(num);
}

// ---------- 扫描 data\题号\ 下所有 *.in, 返回排序后的编号 ----------
vector<int> getTestCases(const string& problemDir) {
    vector<int> cases;
    string pattern = problemDir + "\\*.in";

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return cases;   // 目录不存在 / 无数据

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;  // 跳过子目录
        int n = extractCaseNumber(fd.cFileName);
        if (n >= 0) cases.push_back(n);
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    sort(cases.begin(), cases.end());   // 按编号从小到大评
    return cases;
}

// ---------- 评测一道题, 返回 (通过数, 总测试点数) ----------
pair<int,int> judgeProblem(const string& pid, int timeLimit, int memoryLimit) {
    string sourceFile = "code.cpp";              // ★ 源码固定, 不跟题号走
    string exeFile    = "code.exe";              // ★ exe 同理固定
    string problemDir = "data\\" + pid;          // ★ 只有数据目录跟题号有关

    cout << "\n---------- 题目 " << pid << " ----------" << endl;

    // 1. 编译
    cout << "正在编译 code.cpp ..." << endl;
    if (compileUserCode(sourceFile, exeFile) == CE) {
        cout << ">>> 结果: CE (编译错误)" << endl;
        return make_pair(0, 0);
    }
    cout << "编译成功!" << endl;

    // 2. 扫描该题的测试点 (有几个 .in 就评几个)
    vector<int> cases = getTestCases(problemDir);
    if (cases.empty()) {
        cout << "[警告] " << problemDir << " 下没有找到任何 .in 文件" << endl;
        return make_pair(0, 0);
    }
    cout << "共发现 " << cases.size() << " 个测试点" << endl;

    // 3. 逐个评测
    int acCount = 0;
    for (size_t i = 0; i < cases.size(); i++) {
        int tc = cases[i];
        string inputFile = problemDir + "\\" + to_string(tc) + ".in";
        string stdFile   = problemDir + "\\" + to_string(tc) + ".out";
        string outFile   = problemDir + "\\" + to_string(tc) + ".myout"; // 用户输出

        int usedTime = 0;
        JudgeResult r = runTestCase(exeFile, inputFile, outFile,
                                    timeLimit, memoryLimit, usedTime);

        cout << "  测试点 #" << tc << ": ";
        if (r == AC && compareOutput(outFile, stdFile)) {
            cout << "AC";
            acCount++;
        } else {
            if (r == AC) r = WA;    // 运行正常但答案不对
            cout << resultName(r);
        }
        cout << " (" << usedTime << "ms)" << endl;
    }

    // 4. 本题小结
    int score = (int)(100.0 * acCount / cases.size());
    cout << ">>> 题目 " << pid << ": 通过 " << acCount << "/" << cases.size()
         << "  得分 " << score << endl;

    return make_pair(acCount, (int)cases.size());
}

// ---------- 主函数: 问一次题号, 判一题, 结束 ----------
int main() {
    const int TIME_LIMIT   = 1000;               // 时间限制 1s
    const int MEMORY_LIMIT = 128 * 1024 * 1024;  // 内存限制 128MB

    // 把工作目录固定到 checker.exe 所在文件夹,
    // 这样无论从哪里启动, code.cpp / data\ 等相对路径都有效
    SetCurrentDirectoryA(getExeDir().c_str());

    cout << "=== Windows 简易评测机 (绿色便携版) ===" << endl;
    cout << "源代码: code.cpp    数据目录: data\\题号\\" << endl;

    // 启动时先确认自带编译器存在, 不存在就给明确提示
    string compiler = getExeDir() + "\\" + MINGW_DIR + "\\bin\\g++.exe";
    if (GetFileAttributesA(compiler.c_str()) == INVALID_FILE_ATTRIBUTES) {
        cout << "\n[错误] 找不到编译器: " << compiler << endl;
        cout << "请确认 " << MINGW_DIR << "\\bin\\g++.exe 和 checker.exe 在同一文件夹内" << endl;
        system("pause");
        return 1;
    }

    // 输入题号, 只判这一题
    cout << "\n输入题号: ";
    string pid;
    cin >> pid;

    judgeProblem(pid, TIME_LIMIT, MEMORY_LIMIT);

    system("pause");
    return 0;
}
