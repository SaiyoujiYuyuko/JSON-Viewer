// Text association helper. See ../docs/BATCH_DEFAULTS.md for scope and notices.
// RegistryContext is included here to reuse the pinned upstream context builder
// without modifying that source file. No services, ACLs or policies are changed.
#include "ucl/RegistryContext.cpp"
#include <shobjidl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdexcept>

using namespace UserChoiceLatestHash;
static const wchar_t* extensions[] = {L".txt",L".log",L".ini",L".cfg",L".conf",L".config",L".json",L".jsonc",L".xml",L".yaml",L".yml",L".toml",L".md",L".markdown",L".csv",L".tsv",L".properties",L".lst",L".nfo",L".sql",L".srt",L".ass"};

static void output(const std::wstring& text) {
    auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0, written = 0;
    if (GetConsoleMode(handle, &mode)) WriteConsoleW(handle, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
    else {
        const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string bytes(count, 0);
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), bytes.data(), count, nullptr, nullptr);
        WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    }
}
static void checked(LSTATUS status) {
    if (status != ERROR_SUCCESS) throw std::runtime_error("Registry operation failed: " + std::to_string(status));
}
static bool allowed(const std::wstring& extension) {
    for (auto candidate : extensions) if (extension == candidate) return true;
    return false;
}
static std::wstring path(const std::wstring& ext) {
    return L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext + L"\\UserChoiceLatest";
}
static std::wstring current(const std::wstring& ext) {
    IApplicationAssociationRegistration* registration = nullptr;
    const auto result = CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&registration));
    if (FAILED(result)) throw std::runtime_error("Cannot query Windows effective association.");
    LPWSTR value = nullptr;
    const auto queried = registration->QueryCurrentDefault(ext.c_str(), AT_FILEEXTENSION, AL_EFFECTIVE, &value);
    std::wstring progId = SUCCEEDED(queried) && value ? value : L"";
    CoTaskMemFree(value);
    registration->Release();
    return progId;
}
static std::wstring executable(const std::wstring& ext) {
    wchar_t value[32768];
    DWORD size = 32768;
    return SUCCEEDED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, ext.c_str(), L"open", value, &size)) ? value : L"";
}
static void writeString(const std::wstring& keyName, const wchar_t* name, const std::wstring& value) {
    HKEY key = nullptr;
    checked(RegCreateKeyExW(HKEY_CURRENT_USER, keyName.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr));
    auto status = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    checked(status);
}
static void notifyShell() { SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr); }
static bool preflight() {
    // Match hashes written by Windows before trusting this reverse-engineered
    // algorithm on another Windows build. Computation is entirely local.
    WorkingSeeds seeds; LoadProvidedSeeds(&seeds);
    int matched = 0;
    for (auto ext : extensions) {
        std::wstring existing;
        if (!QueryRegString(HKEY_CURRENT_USER, path(ext), L"Hash", &existing)) continue;
        AssocContext ctx;
        if (!VerifyCurrentAssociation(ext, seeds, &ctx) || ctx.registry_hash != ctx.computed_primary) return false;
        ++matched;
    }
    return matched > 0;
}
static void writeLatest(const std::wstring& ext, const std::wstring& progId) {
    // Parent hash updates do not change the ProgId child key's last-write time.
    const auto key = path(ext);
    writeString(key + L"\\ProgId", L"ProgId", progId);
    std::wstring oldHash;
    if (!QueryRegString(HKEY_CURRENT_USER, key, L"Hash", &oldHash)) writeString(key, L"Hash", L"");
    WorkingSeeds seeds; LoadProvidedSeeds(&seeds);
    AssocContext ctx;
    if (!VerifyCurrentAssociation(ext, seeds, &ctx)) throw std::runtime_error("Cannot compute the latest association hash.");
    writeString(key, L"Hash", ctx.computed_primary);
}
static void removeLatest(const std::wstring& ext) {
    // Only the exact UserChoiceLatest tree under one allowlisted extension.
    // Failure (including UCPD denial) is reported; ACLs are never rewritten.
    const auto status = RegDeleteTreeW(HKEY_CURRENT_USER, path(ext).c_str());
    if (status != ERROR_FILE_NOT_FOUND) checked(status);
}
int wmain(int argc, wchar_t** argv) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        const std::wstring action = argc > 1 ? argv[1] : L"";
        if (action == L"self-test") {
            WorkingSeeds seeds; LoadProvidedSeeds(&seeds);
            std::wstring hash;
            // Public upstream README test vector; not a local machine snapshot.
            const auto sample = L"copyright (c) microsoft. all rights reserved {3822b7ca-c2f4-4889-b8cc-4ce39a8fb81c}.pdd01dcb06ea49f32a0b4deb148-0249-44c4-a8d3-5409e822c599msedgepdfs-1-5-21-673349297-2269585490-1023937497-500";
            if (!ComputeHash(sample, seeds, false, &hash, nullptr) || hash != L"JOBZ2dl4dKM=" || allowed(L".exe") || allowed(L"http"))
                throw std::runtime_error("Hash sample or association scope check failed.");
            output(L"SELF_TEST_OK\n"); return 0;
        }
        if (action == L"preflight") {
            const auto ok = preflight();
            output(ok ? L"MATCH\n" : L"UNSUPPORTED_OR_NO_NATIVE_ANCHOR\n");
            return ok ? 0 : 2;
        }
        if (argc < 3 || !allowed(argv[2])) throw std::runtime_error("Expected query/set-latest/clear-latest and one supported text extension.");
        const std::wstring ext = argv[2];
        if (action == L"query") {
            output(L"ProgId\t" + current(ext) + L"\nExecutable\t" + executable(ext) + L"\n");
            return 0;
        }
        if (action == L"clear-legacy" && argc == 4) {
            const auto key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext + L"\\UserChoice";
            std::wstring previous;
            if (!QueryRegString(HKEY_CURRENT_USER, key, L"ProgId", &previous)) return 0;
            if (previous != argv[3]) throw std::runtime_error("The legacy association changed; refusing to remove it.");
            checked(RegDeleteKeyW(HKEY_CURRENT_USER, key.c_str()));
            notifyShell(); return 0;
        }
        if (argc != 4 || !preflight()) throw std::runtime_error("Native hash preflight failed; no settings changed.");
        const std::wstring progId = argv[3];
        if (progId.empty() || progId.find_first_of(L"\r\n") != std::wstring::npos) throw std::runtime_error("Invalid ProgID.");
        std::wstring previous;
        bool hadPrevious = QueryRegString(HKEY_CURRENT_USER, path(ext) + L"\\ProgId", L"ProgId", &previous);
        if (!hadPrevious) hadPrevious = QueryRegString(HKEY_CURRENT_USER, path(ext), L"ProgId", &previous);
        if (action == L"clear-latest") {
            if (!hadPrevious) return 0;
            if (previous != progId) throw std::runtime_error("Association changed since installation; refusing to remove it.");
            removeLatest(ext); notifyShell();
            return 0;
        }
        if (action != L"set-latest") throw std::runtime_error("Unknown action.");
        HKEY handler = nullptr;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, progId.c_str(), 0, KEY_READ, &handler) != ERROR_SUCCESS)
            throw std::runtime_error("The target ProgID is not registered.");
        RegCloseKey(handler);
        try {
            writeLatest(ext, progId);
            notifyShell();
            WorkingSeeds seeds; LoadProvidedSeeds(&seeds);
            AssocContext verified;
            if (!VerifyCurrentAssociation(ext, seeds, &verified) || verified.registry_hash != verified.computed_primary)
                throw std::runtime_error("Latest association hash verification failed.");
        } catch (...) {
            // Restore the previous latest selection with a fresh valid timestamp.
            try { if (hadPrevious) writeLatest(ext, previous); else removeLatest(ext); notifyShell(); }
            catch (...) { output(L"ROLLBACK_FAILED: use the saved backup and Windows default-app settings.\n"); }
            throw;
        }
        output(L"VERIFIED\n");
        return 0;
    } catch (const std::exception& e) {
        std::string error = e.what();
        output(L"ERROR: " + std::wstring(error.begin(), error.end()) + L"\n");
        return 1;
    }
}
