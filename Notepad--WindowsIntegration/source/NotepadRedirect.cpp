// Portable Notepad-- launcher for Windows Image File Execution Options.
// Build with MSVC: cl /nologo /std:c++17 /EHsc /MT /utf-8 NotepadRedirect.cpp
//   /link /SUBSYSTEM:WINDOWS shell32.lib user32.lib
// This program does not write the registry or execute commands through a shell.
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cwctype>

static std::wstring lower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

// Quote one argument according to Windows CommandLineToArgvW/CRT rules.
static std::wstring quote(const std::wstring& s) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t c : s) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        result += c;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}

struct Request { std::wstring filename, error; };

static Request parse(const std::vector<std::wstring>& args) {
    Request r;
    if (args.size() < 2 || args[0] != L"--ifeo") {
        r.error = L"This launcher must be called by the optional Notepad replacement setting.";
        return r;
    }
    const auto slash = args[1].find_last_of(L"\\/");
    if (lower(args[1].substr(slash == std::wstring::npos ? 0 : slash + 1)) != L"notepad.exe") {
        r.error = L"Expected the original notepad.exe argument.";
        return r;
    }
    for (size_t i = 2; i < args.size(); ++i) {
        const auto option = lower(args[i]);
        if (r.filename.empty() && (option == L"/a" || option == L"/w")) continue;
        if (r.filename.empty() && !option.empty() && option[0] == L'/') {
            r.error = L"This replacement supports opening files, not Notepad printing or other slash switches. "
                      L"Open Notepad-- directly for those operations, or restore Notepad.";
            return r;
        }
        if (!r.filename.empty()) r.filename += L' ';
        r.filename += args[i];
    }
    return r;
}

#ifdef REDIRECT_TEST
#include <iostream>
int wmain() {
    int checks = 0;
    auto check = [&checks](bool ok) { if (!ok) ExitProcess(1); ++checks; };
    check(parse({L"--ifeo", L"C:\\Windows\\notepad.exe"}).filename.empty());
    check(parse({L"--ifeo", L"C:\\Windows\\notepad.exe"}).error.empty());
    check(parse({L"--ifeo", L"NOTEPAD.EXE", L"C:\\中文 文件\\a&b.txt"}).filename == L"C:\\中文 文件\\a&b.txt");
    check(parse({L"--ifeo", L"notepad.exe", L"/A", L"a.txt"}).filename == L"a.txt");
    check(parse({L"--ifeo", L"notepad.exe", L"/W", L"a.txt"}).filename == L"a.txt");
    check(parse({L"--ifeo", L"notepad.exe", L"a", L"b.txt"}).filename == L"a b.txt");
    check(!parse({L"--ifeo", L"notepad.exe", L"/p", L"a.txt"}).error.empty());
    check(!parse({L"--ifeo", L"other.exe", L"a.txt"}).error.empty());
    check(!parse({}).error.empty());
    for (const std::wstring s : {L"", L"a.txt", L"C:\\中文 文件\\a&b.txt", L"a\"b", L"C:\\folder\\", L"\\\""}) {
        const std::wstring cmd = L"app.exe " + quote(s);
        int count = 0;
        auto argv = CommandLineToArgvW(cmd.c_str(), &count);
        check(argv && count == 2 && argv[1] == s);
        LocalFree(argv);
    }
    std::wcout << checks << L" launcher checks passed.\n";
    return 0;
}
#else
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    LocalFree(argv);
    const auto request = parse(args);
    if (!request.error.empty()) {
        MessageBoxW(nullptr, request.error.c_str(), L"Notepad-- replacement", MB_OK | MB_ICONERROR);
        return 2;
    }
    std::vector<wchar_t> ownPath(32768);
    const auto length = GetModuleFileNameW(nullptr, ownPath.data(), static_cast<DWORD>(ownPath.size()));
    if (!length || length >= ownPath.size()) return 3;
    const std::wstring own(ownPath.data(), length);
    const auto target = own.substr(0, own.find_last_of(L"\\/") + 1) + L"Notepad--.exe";
    const auto attributes = GetFileAttributesW(target.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxW(nullptr, L"Place NotepadRedirect.exe beside Notepad--.exe. Restore the replacement setting before moving the editor.",
                    L"Notepad-- replacement", MB_OK | MB_ICONERROR);
        return 4;
    }
    std::wstring cmd = quote(target);
    if (!request.filename.empty()) cmd += L" " + quote(request.filename);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    // Inherit the caller's current directory so relative filenames still work.
    if (!CreateProcessW(target.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        const auto message = L"Cannot start Notepad--. Windows error: " + std::to_wstring(GetLastError());
        MessageBoxW(nullptr, message.c_str(), L"Notepad-- replacement", MB_OK | MB_ICONERROR);
        return 5;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    CloseHandle(process.hProcess);
    return 0;
}
#endif
