#include <Windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <TlHelp32.h>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

namespace weapon_names
{
    const std::unordered_map<uint16_t, std::string> weaponMap = {
        {1, "Desert Eagle"},
        {2, "Dual Berettas"},
        {3, "Five-SeveN"},
        {4, "Glock-18"},
        {30, "Tec-9"},
        {32, "P2000"},
        {36, "P250"},
        {61, "USP-S"},
        {63, "CZ75-Auto"},
        {64, "R8 Revolver"},
        {7, "AK-47"},
        {8, "AUG"},
        {10, "FAMAS"},
        {13, "Galil AR"},
        {16, "M4A4"},
        {39, "SG 553"},
        {60, "M4A1-S"},
        {9, "AWP"},
        {11, "G3SG1"},
        {38, "SCAR-20"},
        {40, "SSG 08"},
        {17, "MAC-10"},
        {19, "P90"},
        {23, "MP5-SD"},
        {24, "UMP-45"},
        {26, "PP-Bizon"},
        {33, "MP7"},
        {34, "MP9"},
        {14, "M249"},
        {28, "Negev"},
        {25, "XM1014"},
        {27, "MAG-7"},
        {29, "Sawed-Off"},
        {35, "Nova"},
        {43, "Flashbang"},
        {44, "HE Grenade"},
        {45, "Smoke Grenade"},
        {46, "Molotov"},
        {47, "Decoy Grenade"},
        {48, "Incendiary Grenade"},
        {68, "Tactical Awareness Grenade"},
        {81, "Fire Bomb"},
        {82, "Snowball"},
        {84, "Breach Charge"},
        {31, "Zeus x27"},
        {42, "Knife"},
        {49, "C4 Explosive"},
        {50, "Kevlar Vest"},
        {51, "Kevlar + Helmet"},
        {52, "Defuse Kit"},
        {54, "Rescue Kit"},
        {55, "Medi-Shot"},
        {57, "Medi-Shot"},
        {59, "Knife"},
        {80, "Ballistic Shield"},
        {500, "Bayonet"},
        {503, "Karambit"},
        {505, "Flip Knife"},
        {506, "Gut Knife"},
        {507, "M9 Bayonet"},
        {508, "Huntsman Knife"},
        {509, "Falchion Knife"},
        {512, "Bowie Knife"},
        {514, "Butterfly Knife"},
        {515, "Shadow Daggers"},
        {516, "Paracord Knife"},
        {517, "Survival Knife"},
        {518, "Ursus Knife"},
        {519, "Navaja Knife"},
        {520, "Nomad Knife"},
        {521, "Stiletto Knife"},
        {522, "Talon Knife"},
        {523, "Classic Knife"},
        {525, "Skeleton Knife"},
        {526, "Kukri Knife"},
        {5027, "Bloodhound Gloves"},
        {5028, "Sport Gloves"},
        {5029, "Driver Gloves"},
        {5030, "Specialist Gloves"},
        {5031, "Moto Gloves"},
        {5032, "Hand Wraps"},
        {5033, "Hydra Gloves"},
        {5034, "Broken Fang Gloves"},
    };

    inline std::string getWeaponName(uint16_t itemDefIndex)
    {
        auto it = weaponMap.find(itemDefIndex);
        if (it != weaponMap.end())
            return it->second;
        return "Unknown";
    }

    inline bool isKnife(uint16_t itemDefIndex)
    {
        return (itemDefIndex == 42 || itemDefIndex == 59 ||
            (itemDefIndex >= 500 && itemDefIndex <= 526));
    }
}

struct Vector2 { float x, y; };
struct Vector3 { float x, y, z; };
struct Matrix4x4 { float m[4][4]; };

namespace Offsets {
    constexpr DWORD64 dwEntityList = 0x254EE60;
    constexpr DWORD64 dwViewMatrix = 0x23A9340;
    constexpr DWORD64 dwLocalPlayerPawn = 0x23A4238;
    constexpr DWORD64 dwViewAngles = 0x23B9C78;
    constexpr DWORD64 m_hPlayerPawn = 0x914;
    constexpr DWORD64 m_iHealth = 0x34C;
    constexpr DWORD64 m_vOldOrigin = 0x13B8;
    constexpr DWORD64 m_iTeamNum = 0x3E7;
    constexpr DWORD64 m_modelState = 0x140;
    constexpr DWORD64 m_pGameSceneNode = 0x330;
    constexpr DWORD64 m_iszPlayerName = 0x6F4;
    constexpr DWORD64 m_pBoneArray = 0x1C0;
    constexpr DWORD64 m_lifeState = 0x354;
    constexpr DWORD64 m_bDormant = 0x103;
    constexpr DWORD64 m_pWeaponServices = 0x1208;
    constexpr DWORD64 m_hActiveWeapon = 0x60;
    constexpr DWORD64 m_AttributeManager = 0x11A8;
    constexpr DWORD64 m_Item = 0x50;
    constexpr DWORD64 m_iItemDefinitionIndex = 0x1BA;
    constexpr DWORD64 m_iClip1 = 0x1700;
}

enum BoneIndices : int {
    PELVIS = 1, SPINE1 = 3, SPINE2 = 4, NECK = 6, HEAD = 7,
    SHOULDER_L = 9, ELBOW_L = 10, HAND_L = 11,
    SHOULDER_R = 13, ELBOW_R = 14, HAND_R = 15,
    HIP_L = 17, KNEE_L = 18, FOOT_HEEL_L = 19,
    HIP_R = 20, KNEE_R = 21, FOOT_HEEL_R = 22,
    CHEST = 23, FOOT_TOES_L_T = 74, FOOT_TOES_R_T = 77,
    BONE_MAX = 128
};

struct BoneLink { int a, b; };
const BoneLink skeleton_links[] = {
    { HEAD, NECK }, { NECK, SPINE2 }, { SPINE2, SPINE1 }, { SPINE1, PELVIS },
    { SHOULDER_L, ELBOW_L }, { ELBOW_L, HAND_L },
    { SHOULDER_R, ELBOW_R }, { ELBOW_R, HAND_R },
    { HIP_L, KNEE_L }, { KNEE_L, FOOT_HEEL_L },
    { HIP_R, KNEE_R }, { KNEE_R, FOOT_HEEL_R },
    { NECK, SHOULDER_L }, { NECK, SHOULDER_R },
    { PELVIS, HIP_L }, { PELVIS, HIP_R },
};

struct CachedPlayer {
    Vector3 origin{};
    int health = 0;
    int team = 0;
    Vector3 bones[BONE_MAX]{};
    wchar_t name[128] = {};
    std::string weaponName;
    int clipAmmo = 0;
    int maxClipAmmo = 0;
    uint16_t weaponDefIndex = 0;
    float viewYaw = 0.0f;
};

struct FrameData {
    std::vector<CachedPlayer> enemies;
    std::vector<CachedPlayer> teammates;
};

FrameData g_FrameBuffers[2];
std::atomic<int> g_ActiveBufferIdx{ 0 };
std::atomic<bool> g_Running{ true };

bool bAimbot = false;
float aimSmooth = 8.0f;
float aimFov = 15.0f;
bool bAimbotFovCircle = true;
float g_fovCircleColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
int aimTarget = 0;
bool bUseAimKey = false;
int aimKey = VK_LBUTTON;
bool bWaitingForKey = false;
bool bAimbotTeam = false;

bool bWatermark = true;

bool bEnableEnemy = true;
bool bEnemyEspBox = true;
float g_enemyBoxColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
bool bEnemyHealthBar = true;
float g_enemyHealthHighColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
float g_enemyHealthLowColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
bool bEnemySkeleton = true;
float g_enemySkeletonColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
bool bEnemyDrawLines = false;
float g_enemyLineColor[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
bool bEnemyName = true;
float g_enemyNameBgColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
float g_enemyNameTextColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float g_teamNameBgColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
float g_teamNameTextColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

bool bEnemyWeapon = true;
float g_enemyWeaponColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float g_enemyWeaponBgColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

bool bEnemyAmmoBar = true;
float g_enemyAmmoHighColor[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
float g_enemyAmmoLowColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

bool bEnableTeam = true;
bool bTeamEspBox = true;
float g_teamBoxColor[4] = { 0.0f, 0.8f, 0.0f, 1.0f };
bool bTeamHealthBar = true;
float g_teamHealthHighColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
float g_teamHealthLowColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
bool bTeamSkeleton = true;
float g_teamSkeletonColor[4] = { 0.0f, 0.8f, 0.0f, 1.0f };
bool bTeamDrawLines = false;
float g_teamLineColor[4] = { 0.0f, 0.8f, 0.0f, 1.0f };
bool bTeamName = true;
bool bTeamWeapon = true;
float g_teamWeaponColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float g_teamWeaponBgColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

bool bTeamAmmoBar = true;
float g_teamAmmoHighColor[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
float g_teamAmmoLowColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

bool bRadarEnabled = true;
float g_radarBgColor[4] = { 1.0f, 1.0f, 1.0f, 0.6f };
float g_radarEnemyColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
float g_radarEnemyArrowColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float g_radarCenterColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
float g_radarTeamColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
float radarCenterX = 0.078f;
float radarCenterY = 0.138f;
float radarRadius = 0.130f;
float radarScale = 1.0f;
bool bRadarShowCenter = true;

int g_menuToggleKey = VK_INSERT;
bool bWaitingForMenuKey = false;

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND g_hWnd = nullptr;

bool g_bShowWindow = true;
bool g_bGameFocused = false;
bool g_bInsertPressed = false;
bool g_bOverlayClickable = false;
bool g_bInGame = false;
//сабнись на тгк если ты тут был
uintptr_t g_clientBase = 0;
HANDLE g_hProcess = nullptr;
uintptr_t g_localPawn = 0;
int g_localTeam = 0;

int g_screenWidth = GetSystemMetrics(SM_CXSCREEN);
int g_screenHeight = GetSystemMetrics(SM_CYSCREEN);

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

const char* GetKeyName(int vk) {
    switch (vk) {
    case VK_LBUTTON: return "Left Mouse";
    case VK_RBUTTON: return "Right Mouse";
    case VK_MBUTTON: return "Middle Mouse";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    case VK_BACK: return "Backspace";
    case VK_TAB: return "Tab";
    case VK_CLEAR: return "Clear";
    case VK_RETURN: return "Enter";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_PAUSE: return "Pause";
    case VK_CAPITAL: return "Caps Lock";
    case VK_ESCAPE: return "Esc";
    case VK_SPACE: return "Space";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_END: return "End";
    case VK_HOME: return "Home";
    case VK_LEFT: return "Left Arrow";
    case VK_UP: return "Up Arrow";
    case VK_RIGHT: return "Right Arrow";
    case VK_DOWN: return "Down Arrow";
    case VK_SELECT: return "Select";
    case VK_PRINT: return "Print";
    case VK_EXECUTE: return "Execute";
    case VK_SNAPSHOT: return "Print Screen";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HELP: return "Help";
    case VK_LWIN: return "Left Windows";
    case VK_RWIN: return "Right Windows";
    case VK_APPS: return "Applications";
    case VK_SLEEP: return "Sleep";
    case VK_NUMPAD0: return "Numpad 0";
    case VK_NUMPAD1: return "Numpad 1";
    case VK_NUMPAD2: return "Numpad 2";
    case VK_NUMPAD3: return "Numpad 3";
    case VK_NUMPAD4: return "Numpad 4";
    case VK_NUMPAD5: return "Numpad 5";
    case VK_NUMPAD6: return "Numpad 6";
    case VK_NUMPAD7: return "Numpad 7";
    case VK_NUMPAD8: return "Numpad 8";
    case VK_NUMPAD9: return "Numpad 9";
    case VK_MULTIPLY: return "Numpad *";
    case VK_ADD: return "Numpad +";
    case VK_SEPARATOR: return "Separator";
    case VK_SUBTRACT: return "Numpad -";
    case VK_DECIMAL: return "Numpad .";
    case VK_DIVIDE: return "Numpad /";
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    case VK_NUMLOCK: return "Num Lock";
    case VK_SCROLL: return "Scroll Lock";
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_LCONTROL: return "Left Ctrl";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_LMENU: return "Left Alt";
    case VK_RMENU: return "Right Alt";
    }
    static char keyName[32] = "";
    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    if ((vk >= 0x30 && vk <= 0x39) || (vk >= 0x41 && vk <= 0x5A)) {
        bool shift = GetAsyncKeyState(VK_SHIFT) & 0x8000;
        bool capsLock = GetKeyState(VK_CAPITAL) & 1;
        bool isLetter = (vk >= 0x41 && vk <= 0x5A);
        if (isLetter) {
            if ((shift && !capsLock) || (!shift && capsLock)) {
                keyName[0] = (char)vk;
            }
            else {
                keyName[0] = (char)(vk + 32);
            }
            keyName[1] = '\0';
            return keyName;
        }
        else {
            keyName[0] = (char)vk;
            keyName[1] = '\0';
            return keyName;
        }
    }
    LONG lParamValue = (scanCode << 16);
    if (GetKeyNameTextA(lParamValue, keyName, sizeof(keyName)) > 0) {
        return keyName;
    }
    return "Unknown";
}
//сабнись на тгк если ты тут был
template<typename T>
T Read(HANDLE process, uintptr_t address) {
    T value{};
    if (address < 0x10000 || address > 0x7FFFFFFFFFFF) return value;
    ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), nullptr);
    return value;
}

template<typename T>
void Write(HANDLE process, uintptr_t address, T value) {
    if (address < 0x10000 || address > 0x7FFFFFFFFFFF) return;
    WriteProcessMemory(process, reinterpret_cast<LPVOID>(address), &value, sizeof(T), nullptr);
}

bool IsValidPtr(DWORD64 ptr) {
    return ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF;
}

DWORD GetProcessIdByName(const wchar_t* processName) {
    PROCESSENTRY32W entry{ sizeof(entry) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    DWORD pid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (!_wcsicmp(entry.szExeFile, processName)) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* moduleName) {
    MODULEENTRY32W entry{ sizeof(entry) };
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    uintptr_t result = 0;
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (!_wcsicmp(entry.szModule, moduleName)) {
                result = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

uintptr_t GetEntityByIndex(HANDLE h, uintptr_t entityList, int index) {
    uintptr_t chunk = Read<uintptr_t>(h, entityList + 8 * (index >> 9) + 16);
    if (!IsValidPtr(chunk)) return 0;
    return Read<uintptr_t>(h, chunk + 0x70 * (index & 0x1FF));
}

Vector3 GetBonePosition(HANDLE h, uintptr_t pawn, int boneIndex) {
    uintptr_t gameSceneNode = Read<uintptr_t>(h, pawn + Offsets::m_pGameSceneNode);
    if (!IsValidPtr(gameSceneNode)) return {};
    uintptr_t boneArray = Read<uintptr_t>(h, gameSceneNode + Offsets::m_modelState + 0x80);
    if (!IsValidPtr(boneArray)) return {};
    return Read<Vector3>(h, boneArray + (boneIndex * 0x20));
}
//сабнись на тгк если ты тут был
std::wstring ReadPlayerName(HANDLE h, uintptr_t controller) {
    if (!IsValidPtr(controller)) return L"";
    char buffer[128] = { 0 };
    uintptr_t nameAddr = controller + Offsets::m_iszPlayerName;
    if (ReadProcessMemory(h, reinterpret_cast<LPCVOID>(nameAddr), buffer, sizeof(buffer) - 1, nullptr)) {
        buffer[127] = '\0';
        size_t len = 0;
        while (len < 128 && buffer[len] != '\0') len++;
        if (len == 0) return L"";
        while (len > 0 && (buffer[len - 1] < 32 || buffer[len - 1] > 126)) {
            buffer[len - 1] = '\0';
            len--;
        }
        if (len == 0) return L"";
        std::wstring wname;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer, (int)len, nullptr, 0);
        if (wlen > 0) {
            wname.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, buffer, (int)len, &wname[0], wlen);
            while (!wname.empty() && (wname.back() == L' ' || wname.back() == L'\t' || wname.back() == L'\n' || wname.back() == L'\r')) {
                wname.pop_back();
            }
        }
        return wname;
    }
    return L"";
}

int GetPlayerClipAmmo(HANDLE h, uintptr_t pawn, uintptr_t entityList) {
    if (!IsValidPtr(pawn)) return 0;

    uintptr_t weaponServices = Read<uintptr_t>(h, pawn + Offsets::m_pWeaponServices);
    if (!IsValidPtr(weaponServices)) return 0;

    uint32_t activeWeaponHandle = Read<uint32_t>(h, weaponServices + Offsets::m_hActiveWeapon);
    if (!activeWeaponHandle || activeWeaponHandle == 0xFFFFFFFF) return 0;

    uint32_t weaponIndex = activeWeaponHandle & 0x7FFF;
    uintptr_t weaponEntity = GetEntityByIndex(h, entityList, weaponIndex);
    if (!IsValidPtr(weaponEntity)) return 0;

    int clipAmmo = Read<int>(h, weaponEntity + Offsets::m_iClip1);
    return clipAmmo;
}

int GetMaxClipAmmo(const std::string& weaponName) {
    if (weaponName == "AK-47" || weaponName == "M4A4" || weaponName == "SG 553" ||
        weaponName == "AUG" || weaponName == "FAMAS" || weaponName == "Galil AR" ||
        weaponName == "UMP-45" || weaponName == "MP7" || weaponName == "MP9" ||
        weaponName == "MP5-SD" || weaponName == "MAC-10") {
        return 30;
    }
    else if (weaponName == "M4A1-S") {
        return 20;
    }
    else if (weaponName == "AWP" || weaponName == "SSG 08") {
        return 5;
    }
    else if (weaponName == "G3SG1" || weaponName == "SCAR-20") {
        return 20;
    }
    else if (weaponName == "P90") {
        return 50;
    }
    else if (weaponName == "PP-Bizon") {
        return 64;
    }
    else if (weaponName == "M249") {
        return 100;
    }
    else if (weaponName == "Negev") {
        return 150;
    }
    else if (weaponName == "XM1014" || weaponName == "Nova") {
        return 8;
    }
    else if (weaponName == "Sawed-Off") {
        return 7;
    }
    else if (weaponName == "MAG-7") {
        return 5;
    }
    else if (weaponName == "Desert Eagle" || weaponName == "R8 Revolver") {
        return 8;
    }
    else if (weaponName == "Glock-18") {
        return 20;
    }
    else if (weaponName == "Five-SeveN" || weaponName == "Tec-9") {
        return 20;
    }
    else if (weaponName == "P250") {
        return 13;
    }
    else if (weaponName == "P2000" || weaponName == "USP-S") {
        return 12;
    }
    else if (weaponName == "CZ75-Auto") {
        return 12;
    }
    else if (weaponName == "Dual Berettas") {
        return 30;
    }
    else if (weaponName == "Zeus x27") {
        return 1;
    }
    else if (weaponName.find("Knife") != std::string::npos ||
        weaponName.find("Grenade") != std::string::npos ||
        weaponName == "C4 Explosive" ||
        weaponName.find("Gloves") != std::string::npos) {
        return 0;
    }
    return 0;
}                //сабнись на тгк если ты тут был

uint16_t ReadWeaponDefIndex(HANDLE h, uintptr_t pawn, uintptr_t entityList) {
    if (!IsValidPtr(pawn)) return 0;
    uintptr_t weaponServices = Read<uintptr_t>(h, pawn + Offsets::m_pWeaponServices);
    if (!IsValidPtr(weaponServices)) return 0;
    uint32_t activeWeaponHandle = Read<uint32_t>(h, weaponServices + Offsets::m_hActiveWeapon);
    if (!activeWeaponHandle || activeWeaponHandle == 0xFFFFFFFF) return 0;
    uint32_t weaponIndex = activeWeaponHandle & 0x7FFF;
    uintptr_t weaponEntity = GetEntityByIndex(h, entityList, weaponIndex);
    if (!IsValidPtr(weaponEntity)) return 0;
    uintptr_t attributeManager = weaponEntity + Offsets::m_AttributeManager;
    if (!IsValidPtr(attributeManager)) return 0;
    uintptr_t item = attributeManager + Offsets::m_Item;
    if (!IsValidPtr(item)) return 0;
    return Read<uint16_t>(h, item + Offsets::m_iItemDefinitionIndex);
}

std::string ReadWeaponName(HANDLE h, uintptr_t pawn, uintptr_t entityList) {
    uint16_t index = ReadWeaponDefIndex(h, pawn, entityList);
    return weapon_names::getWeaponName(index);
}

Vector3 GetAimTarget(const CachedPlayer& player, int targetType) {
    switch (targetType) {
    case 0: return player.bones[HEAD];
    case 1: return player.bones[CHEST];
    case 2: return player.bones[PELVIS];
    default: return player.bones[HEAD];
    }
}

bool IsValidPlayer(HANDLE hProcess, uintptr_t pawn, uintptr_t localPawn, int localTeam) {
    if (!IsValidPtr(pawn) || pawn == localPawn) return false;
    uint8_t lifeState = Read<uint8_t>(hProcess, pawn + Offsets::m_lifeState);
    if (lifeState != 0) return false;
    int health = Read<int>(hProcess, pawn + Offsets::m_iHealth);
    if (health < 1 || health > 100) return false;
    int team = Read<int>(hProcess, pawn + Offsets::m_iTeamNum);
    if (team != 2 && team != 3) return false;
    bool dormant = Read<bool>(hProcess, pawn + Offsets::m_bDormant);
    if (dormant) return false;
    Vector3 origin = Read<Vector3>(hProcess, pawn + Offsets::m_vOldOrigin);
    if (std::abs(origin.x) > 10000.0f || std::abs(origin.y) > 10000.0f || std::abs(origin.z) > 10000.0f)
        return false;
    return true;
}

bool IsInGame() {
    if (!g_hProcess || !g_clientBase || !g_localPawn) return false;
    int health = Read<int>(g_hProcess, g_localPawn + Offsets::m_iHealth);
    if (health > 0 && health <= 100) return true;
    int team = Read<int>(g_hProcess, g_localPawn + Offsets::m_iTeamNum);
    if (team == 2 || team == 3) return true;
    return false;
}

bool IsLocalWeaponSuitableForAimbot(HANDLE h, uintptr_t localPawn, uintptr_t entityList) {
    uint16_t defIndex = ReadWeaponDefIndex(h, localPawn, entityList);
    if (weapon_names::isKnife(defIndex))
        return false;

    int clipAmmo = GetPlayerClipAmmo(h, localPawn, entityList);
    std::string weaponName = weapon_names::getWeaponName(defIndex);
    int maxClip = GetMaxClipAmmo(weaponName);

    if (maxClip > 0 && clipAmmo <= 0)
        return false;

    if (weaponName.find("Grenade") != std::string::npos ||
        weaponName == "C4 Explosive" ||
        weaponName == "Zeus x27" ||
        weaponName == "Medi-Shot" ||
        weaponName.find("Gloves") != std::string::npos ||
        weaponName == "Kevlar Vest" ||
        weaponName == "Kevlar + Helmet" ||
        weaponName == "Defuse Kit" ||
        weaponName == "Rescue Kit" ||
        weaponName == "Ballistic Shield" ||
        weaponName == "Breach Charge" ||
        weaponName == "Snowball" ||
        weaponName == "Fire Bomb" ||
        weaponName == "Tactical Awareness Grenade") {
        return false;
    }

    return true;
}
//сабнись на тгк если ты тут был
ImColor GetHealthColor(float healthRatio, bool isTeammate) {
    float* highColor = isTeammate ? g_teamHealthHighColor : g_enemyHealthHighColor;
    float* lowColor = isTeammate ? g_teamHealthLowColor : g_enemyHealthLowColor;
    float r = lowColor[0] + (highColor[0] - lowColor[0]) * healthRatio;
    float g = lowColor[1] + (highColor[1] - lowColor[1]) * healthRatio;
    float b = lowColor[2] + (highColor[2] - lowColor[2]) * healthRatio;
    float a = lowColor[3] + (highColor[3] - lowColor[3]) * healthRatio;
    return ImColor(r, g, b, a);
}

ImColor GetAmmoColor(float ammoRatio, bool isTeammate) {
    float* highColor = isTeammate ? g_teamAmmoHighColor : g_enemyAmmoHighColor;
    float* lowColor = isTeammate ? g_teamAmmoLowColor : g_enemyAmmoLowColor;
    float r = lowColor[0] + (highColor[0] - lowColor[0]) * ammoRatio;
    float g = lowColor[1] + (highColor[1] - lowColor[1]) * ammoRatio;
    float b = lowColor[2] + (highColor[2] - lowColor[2]) * ammoRatio;
    float a = lowColor[3] + (highColor[3] - lowColor[3]) * ammoRatio;
    return ImColor(r, g, b, a);
}

void DrawRadar() {
    if (!bRadarEnabled || !g_bInGame) return;

    int readIdx = g_ActiveBufferIdx.load(std::memory_order_acquire);
    const auto& enemies = g_FrameBuffers[readIdx].enemies;
    const auto& teammates = g_FrameBuffers[readIdx].teammates;

    if (enemies.empty() && teammates.empty()) return;

    Vector3 viewAngles = Read<Vector3>(g_hProcess, g_clientBase + Offsets::dwViewAngles);
    float playerYaw = viewAngles.y;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    float centerX = g_screenWidth * radarCenterX;
    float centerY = g_screenHeight * radarCenterY;
    float radius = g_screenHeight * radarRadius;

    ImColor bgColor = ImColor(0.15f, 0.15f, 0.15f, 1.0f);
    draw->AddCircleFilled(ImVec2(centerX, centerY), radius, bgColor, 64);
    draw->AddCircle(ImVec2(centerX, centerY), radius, IM_COL32(100, 100, 100, 200), 64, 2.0f);

    Vector3 localPos = {};
    if (g_localPawn) {
        localPos = Read<Vector3>(g_hProcess, g_localPawn + Offsets::m_vOldOrigin);
    }

    float rotationRad = (90.0f - playerYaw) * 3.14159265f / 180.0f;
    float cosRot = cosf(rotationRad);
    float sinRot = sinf(rotationRad);

    for (const auto& enemy : enemies) {
        if (enemy.health <= 0) continue;

        float deltaX = enemy.origin.x - localPos.x;
        float deltaY = enemy.origin.y - localPos.y;

        float rotatedX = deltaX * cosRot - deltaY * sinRot;
        float rotatedY = deltaX * sinRot + deltaY * cosRot;

        float scaleFactor = radius / (2000.0f * radarScale);

        float radarX = centerX + rotatedX * scaleFactor;
        float radarY = centerY - rotatedY * scaleFactor;

        float distFromCenter = sqrtf(
            (radarX - centerX) * (radarX - centerX) +
            (radarY - centerY) * (radarY - centerY)
        );

        if (distFromCenter <= radius) {
            ImColor dotColor = ImColor(g_radarEnemyColor[0], g_radarEnemyColor[1], g_radarEnemyColor[2], g_radarEnemyColor[3]);
            draw->AddCircleFilled(ImVec2(radarX, radarY), 4.0f, dotColor);
        }
    }
    //сабнись на тгк если ты тут был
    for (const auto& teammate : teammates) {
        if (teammate.health <= 0) continue;

        float deltaX = teammate.origin.x - localPos.x;
        float deltaY = teammate.origin.y - localPos.y;

        float rotatedX = deltaX * cosRot - deltaY * sinRot;
        float rotatedY = deltaX * sinRot + deltaY * cosRot;

        float scaleFactor = radius / (2000.0f * radarScale);

        float radarX = centerX + rotatedX * scaleFactor;
        float radarY = centerY - rotatedY * scaleFactor;

        float distFromCenter = sqrtf(
            (radarX - centerX) * (radarX - centerX) +
            (radarY - centerY) * (radarY - centerY)
        );

        if (distFromCenter <= radius) {
            ImColor dotColor = ImColor(g_radarTeamColor[0], g_radarTeamColor[1], g_radarTeamColor[2], g_radarTeamColor[3]);
            draw->AddCircleFilled(ImVec2(radarX, radarY), 4.0f, dotColor);
        }
    }

    if (bRadarShowCenter) {
        ImColor centerColor = ImColor(g_radarCenterColor[0], g_radarCenterColor[1], g_radarCenterColor[2], g_radarCenterColor[3]);
        draw->AddCircleFilled(ImVec2(centerX, centerY), 5.0f, centerColor);

        float arrowLen = 6.0f;
        float arrowOffset = 5.0f + 1.0f;
        draw->AddTriangleFilled(
            ImVec2(centerX, centerY - arrowOffset - arrowLen),
            ImVec2(centerX - 2.5f, centerY - arrowOffset),
            ImVec2(centerX + 2.5f, centerY - arrowOffset),
            IM_COL32(255, 255, 255, 255)
        );
    }
}
void AimbotThread() {
    while (g_Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!bAimbot || !g_hProcess || !g_clientBase || !g_localPawn) continue;
        if (!IsInGame()) continue;
        if (bUseAimKey) {
            if (!(GetAsyncKeyState(aimKey) & 0x8000)) continue;
        }

        uintptr_t entityList = Read<uintptr_t>(g_hProcess, g_clientBase + Offsets::dwEntityList);
        if (!IsValidPtr(entityList)) continue;

        if (!IsLocalWeaponSuitableForAimbot(g_hProcess, g_localPawn, entityList))
            continue;

        uintptr_t localGameSceneNode = Read<uintptr_t>(g_hProcess, g_localPawn + Offsets::m_pGameSceneNode);
        Vector3 localCameraPos{};
        if (IsValidPtr(localGameSceneNode)) {
            uintptr_t localBoneArray = Read<uintptr_t>(g_hProcess, localGameSceneNode + Offsets::m_modelState + 0x80);
            if (IsValidPtr(localBoneArray)) {
                localCameraPos = Read<Vector3>(g_hProcess, localBoneArray + (HEAD * 0x20));
            }
        }
        if (localCameraPos.x == 0 && localCameraPos.y == 0 && localCameraPos.z == 0) {
            localCameraPos = Read<Vector3>(g_hProcess, g_localPawn + Offsets::m_vOldOrigin);
        }

        uintptr_t viewAnglesAddr = g_clientBase + Offsets::dwViewAngles;
        Vector3 currentAngles = Read<Vector3>(g_hProcess, viewAnglesAddr);

        int readIdx = g_ActiveBufferIdx.load(std::memory_order_acquire);
        const auto& enemies = g_FrameBuffers[readIdx].enemies;
        const auto& teammates = g_FrameBuffers[readIdx].teammates;

        float bestFov = aimFov;
        Vector3 bestTarget{};

        for (const auto& enemy : enemies) {
            if (enemy.health <= 0) continue;
            Vector3 targetPos = GetAimTarget(enemy, aimTarget);
            if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) continue;
            Vector3 delta = { targetPos.x - localCameraPos.x, targetPos.y - localCameraPos.y, targetPos.z - localCameraPos.z };
            float dist = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
            if (dist < 5.0f) continue;
            float yaw = atan2f(delta.y, delta.x) * 180.0f / 3.1415926535f;
            float pitch = -asinf(delta.z / dist) * 180.0f / 3.1415926535f;
            while (yaw > 180.0f) yaw -= 360.0f;
            while (yaw < -180.0f) yaw += 360.0f;
            while (pitch > 89.0f) pitch = 89.0f;
            while (pitch < -89.0f) pitch = -89.0f;
            float deltaYaw = fabsf(yaw - currentAngles.y);
            if (deltaYaw > 180.0f) deltaYaw = 360.0f - deltaYaw;
            float deltaPitch = fabsf(pitch - currentAngles.x);
            float fov = sqrtf(deltaYaw * deltaYaw + deltaPitch * deltaPitch);
            if (fov < bestFov) {
                bestFov = fov;
                bestTarget = targetPos;
            }
        }
        //сабнись на тгк если ты тут был
        if (bAimbotTeam) {
            for (const auto& teammate : teammates) {
                if (teammate.health <= 0) continue;
                Vector3 targetPos = GetAimTarget(teammate, aimTarget);
                if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) continue;
                Vector3 delta = { targetPos.x - localCameraPos.x, targetPos.y - localCameraPos.y, targetPos.z - localCameraPos.z };
                float dist = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                if (dist < 5.0f) continue;
                float yaw = atan2f(delta.y, delta.x) * 180.0f / 3.1415926535f;
                float pitch = -asinf(delta.z / dist) * 180.0f / 3.1415926535f;
                while (yaw > 180.0f) yaw -= 360.0f;
                while (yaw < -180.0f) yaw += 360.0f;
                while (pitch > 89.0f) pitch = 89.0f;
                while (pitch < -89.0f) pitch = -89.0f;
                float deltaYaw = fabsf(yaw - currentAngles.y);
                if (deltaYaw > 180.0f) deltaYaw = 360.0f - deltaYaw;
                float deltaPitch = fabsf(pitch - currentAngles.x);
                float fov = sqrtf(deltaYaw * deltaYaw + deltaPitch * deltaPitch);
                if (fov < bestFov) {
                    bestFov = fov;
                    bestTarget = targetPos;
                }
            }
        }

        if (bestFov < aimFov && bestTarget.x != 0) {
            Vector3 delta = { bestTarget.x - localCameraPos.x, bestTarget.y - localCameraPos.y, bestTarget.z - localCameraPos.z };
            float dist = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
            float targetYaw = atan2f(delta.y, delta.x) * 180.0f / 3.1415926535f;
            float targetPitch = -asinf(delta.z / dist) * 180.0f / 3.1415926535f;
            while (targetYaw > 180.0f) targetYaw -= 360.0f;
            while (targetYaw < -180.0f) targetYaw += 360.0f;
            float deltaYaw = targetYaw - currentAngles.y;
            if (deltaYaw > 180.0f) deltaYaw -= 360.0f;
            if (deltaYaw < -180.0f) deltaYaw += 360.0f;
            float deltaPitch = targetPitch - currentAngles.x;
            Vector3 newAngles;
            newAngles.x = currentAngles.x + deltaPitch / aimSmooth;
            newAngles.y = currentAngles.y + deltaYaw / aimSmooth;
            newAngles.z = 0.0f;
            while (newAngles.y > 180.0f) newAngles.y -= 360.0f;
            while (newAngles.y < -180.0f) newAngles.y += 360.0f;
            if (newAngles.x > 89.0f) newAngles.x = 89.0f;
            if (newAngles.x < -89.0f) newAngles.x = -89.0f;
            Write<Vector3>(g_hProcess, viewAnglesAddr, newAngles);
        }
    }
}
//сабнись на тгк если ты тут был
void MemoryReadLoop() {
    while (g_Running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(8));

        if (!g_hProcess || !g_clientBase) continue;

        uintptr_t entityList = Read<uintptr_t>(g_hProcess, g_clientBase + Offsets::dwEntityList);
        if (!IsValidPtr(entityList)) continue;

        uintptr_t localPawn = Read<uintptr_t>(g_hProcess, g_clientBase + Offsets::dwLocalPlayerPawn);
        if (IsValidPtr(localPawn)) {
            g_localPawn = localPawn;
            g_localTeam = Read<int>(g_hProcess, localPawn + Offsets::m_iTeamNum);
        }
        //сабнись на тгк если ты тут был
        std::vector<CachedPlayer> newEnemies, newTeammates;
        newEnemies.reserve(64);
        newTeammates.reserve(64);

        for (int i = 0; i < 64; i++) {
            uintptr_t controller = GetEntityByIndex(g_hProcess, entityList, i);
            if (!IsValidPtr(controller)) continue;

            DWORD pawnHandle = Read<DWORD>(g_hProcess, controller + Offsets::m_hPlayerPawn) & 0x7FFF;
            if (!pawnHandle) continue;

            uintptr_t pawn = GetEntityByIndex(g_hProcess, entityList, pawnHandle);
            if (!IsValidPlayer(g_hProcess, pawn, localPawn, g_localTeam))
                continue;

            CachedPlayer p;
            p.health = Read<int>(g_hProcess, pawn + Offsets::m_iHealth);
            p.team = Read<int>(g_hProcess, pawn + Offsets::m_iTeamNum);
            p.origin = Read<Vector3>(g_hProcess, pawn + Offsets::m_vOldOrigin);

            uintptr_t viewAnglesAddr = g_clientBase + Offsets::dwViewAngles;
            Vector3 viewAngles = Read<Vector3>(g_hProcess, viewAnglesAddr);
            p.viewYaw = viewAngles.y;
            p.weaponDefIndex = ReadWeaponDefIndex(g_hProcess, pawn, entityList);
            p.weaponName = weapon_names::getWeaponName(p.weaponDefIndex);
            p.clipAmmo = GetPlayerClipAmmo(g_hProcess, pawn, entityList);
            p.maxClipAmmo = GetMaxClipAmmo(p.weaponName);

            uintptr_t gameSceneNode = Read<uintptr_t>(g_hProcess, pawn + Offsets::m_pGameSceneNode);
            if (IsValidPtr(gameSceneNode)) {
                uintptr_t boneArray = Read<uintptr_t>(g_hProcess, gameSceneNode + Offsets::m_modelState + 0x80);
                if (IsValidPtr(boneArray)) {
                    for (int b = 0; b < BONE_MAX; ++b) {
                        p.bones[b] = Read<Vector3>(g_hProcess, boneArray + (b * 0x20));
                    }
                }
            }

            std::wstring name = ReadPlayerName(g_hProcess, controller);
            wcscpy_s(p.name, name.c_str());

            if (p.team == g_localTeam)
                newTeammates.push_back(std::move(p));
            else
                newEnemies.push_back(std::move(p));
        }

        int writeIdx = 1 - g_ActiveBufferIdx.load(std::memory_order_relaxed);
        g_FrameBuffers[writeIdx].enemies = std::move(newEnemies);
        g_FrameBuffers[writeIdx].teammates = std::move(newTeammates);
        g_ActiveBufferIdx.store(writeIdx, std::memory_order_release);
    }
}

bool WorldToScreen(const Vector3& pos, Vector2& screen, const Matrix4x4& m) {
    float w = m.m[3][0] * pos.x + m.m[3][1] * pos.y + m.m[3][2] * pos.z + m.m[3][3];
    if (w < 0.01f) return false;
    float invW = 1.f / w;
    screen.x = (g_screenWidth * 0.5f) * (1.f + (m.m[0][0] * pos.x + m.m[0][1] * pos.y + m.m[0][2] * pos.z + m.m[0][3]) * invW);
    screen.y = (g_screenHeight * 0.5f) * (1.f - (m.m[1][0] * pos.x + m.m[1][1] * pos.y + m.m[1][2] * pos.z + m.m[1][3]) * invW);
    return true;
}

inline ImColor ToImColor(const float* col) {
    return ImColor(col[0], col[1], col[2], col[3]);
}
//сабнись на тгк если ты тут был
void DrawFovCircle() {
    if (!bAimbot || !bAimbotFovCircle || aimFov <= 0.f || !g_bInGame) return;
    int readIdx = g_ActiveBufferIdx.load(std::memory_order_acquire);
    const auto& enemies = g_FrameBuffers[readIdx].enemies;
    const auto& teammates = g_FrameBuffers[readIdx].teammates;
    if (enemies.empty() && teammates.empty()) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 center(g_screenWidth * 0.5f, g_screenHeight * 0.5f);
    float radius = (aimFov / 90.f) * g_screenWidth * 0.5f;
    ImColor fovColor = ToImColor(g_fovCircleColor);
    draw->AddCircle(ImVec2(center.x + 1.0f, center.y + 1.0f), radius, IM_COL32(0, 0, 0, 200), 164, 1.0f);
    draw->AddCircle(center, radius, fovColor, 164, 1.0f);
}

void DrawSkeleton(const CachedPlayer& player, const Matrix4x4& matrix, bool isTeammate) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImColor color = isTeammate ? ToImColor(g_teamSkeletonColor) : ToImColor(g_enemySkeletonColor);
    for (const auto& link : skeleton_links) {
        if (link.a >= BONE_MAX || link.b >= BONE_MAX) continue;
        Vector3 posA = player.bones[link.a];
        Vector3 posB = player.bones[link.b];
        if ((posA.x == 0.0f && posA.y == 0.0f && posA.z == 0.0f) ||
            (posB.x == 0.0f && posB.y == 0.0f && posB.z == 0.0f))
            continue;
        if (std::isnan(posA.x) || std::isnan(posA.y) || std::isnan(posA.z) ||
            std::isnan(posB.x) || std::isnan(posB.y) || std::isnan(posB.z))
            continue;
        if (std::abs(posA.x) > 50000.0f || std::abs(posA.y) > 50000.0f || std::abs(posA.z) > 50000.0f ||
            std::abs(posB.x) > 50000.0f || std::abs(posB.y) > 50000.0f || std::abs(posB.z) > 50000.0f)
            continue;
        float dx = posA.x - posB.x;
        float dy = posA.y - posB.y;
        float dz = posA.z - posB.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist > 120.0f || dist < 0.1f) continue;
        Vector2 screenA, screenB;
        if (!WorldToScreen(posA, screenA, matrix) || !WorldToScreen(posB, screenB, matrix)) continue;
        float screenDist = sqrtf((screenA.x - screenB.x) * (screenA.x - screenB.x) +
            (screenA.y - screenB.y) * (screenA.y - screenB.y));
        if (screenDist > 800.0f) continue;
        if (isTeammate) {
            draw->AddLine(ImVec2(screenA.x, screenA.y), ImVec2(screenB.x, screenB.y), IM_COL32(0, 0, 0, 255), 1.0f);
            draw->AddLine(ImVec2(screenA.x, screenA.y), ImVec2(screenB.x, screenB.y), color, 1.0f);
        }
        else {
            draw->AddLine(ImVec2(screenA.x - 1, screenA.y - 1), ImVec2(screenB.x - 1, screenB.y - 1), IM_COL32(0, 0, 0, 200), 1.0f);
            draw->AddLine(ImVec2(screenA.x + 1, screenA.y + 1), ImVec2(screenB.x + 1, screenB.y + 1), IM_COL32(0, 0, 0, 200), 1.0f);
            draw->AddLine(ImVec2(screenA.x, screenA.y), ImVec2(screenB.x, screenB.y), color, 1.0f);
        }
    }
}

void DrawLineToPlayer(const CachedPlayer& player, const Matrix4x4& matrix, bool isTeammate) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImColor lineColor = isTeammate ? ToImColor(g_teamLineColor) : ToImColor(g_enemyLineColor);
    Vector3 head = { player.origin.x, player.origin.y, player.origin.z + 72.f };
    Vector2 screenFeet, screenHead;
    if (!WorldToScreen(player.origin, screenFeet, matrix) || !WorldToScreen(head, screenHead, matrix)) return;
    float height = screenFeet.y - screenHead.y;
    if (height <= 0.f) return;
    float boxCenterX = screenHead.x;
    float boxBottom = screenFeet.y;
    float screenBottomCenterX = g_screenWidth * 0.5f;
    float screenBottomY = (float)g_screenHeight;
    if (isTeammate) {
        draw->AddLine(ImVec2(screenBottomCenterX - 1, screenBottomY), ImVec2(boxCenterX - 1, boxBottom), IM_COL32(0, 0, 0, 255), 1.0f);
        draw->AddLine(ImVec2(screenBottomCenterX + 1, screenBottomY), ImVec2(boxCenterX + 1, boxBottom), IM_COL32(0, 0, 0, 255), 1.0f);
        draw->AddLine(ImVec2(screenBottomCenterX, screenBottomY - 1), ImVec2(boxCenterX, boxBottom - 1), IM_COL32(0, 0, 0, 255), 1.0f);
        draw->AddLine(ImVec2(screenBottomCenterX, screenBottomY + 1), ImVec2(boxCenterX, boxBottom + 1), IM_COL32(0, 0, 0, 255), 1.0f);
        draw->AddLine(ImVec2(screenBottomCenterX, screenBottomY), ImVec2(boxCenterX, boxBottom), lineColor, 1.0f);
    }
    else {
        draw->AddLine(ImVec2(screenBottomCenterX - 1, screenBottomY), ImVec2(boxCenterX - 1, boxBottom), IM_COL32(0, 0, 0, 200), 1.0f);
        draw->AddLine(ImVec2(screenBottomCenterX + 1, screenBottomY), ImVec2(boxCenterX + 1, boxBottom), IM_COL32(0, 0, 0, 200), 1.0f);
        draw->AddLine(ImVec2(screenBottomCenterX, screenBottomY - 1), ImVec2(boxCenterX, boxBottom - 1), IM_COL32(0, 0, 0, 200), 1.0f);
        draw->AddLine(ImVec2(screenBottomCenterX, screenBottomY + 1), ImVec2(boxCenterX, boxBottom + 1), IM_COL32(0, 0, 0, 200), 1.0f);
        draw->AddLine(ImVec2(screenBottomCenterX, screenBottomY), ImVec2(boxCenterX, boxBottom), lineColor, 1.0f);
    }
}
//сабнись на тгк если ты тут был
void DrawAmmoBar(const CachedPlayer& player, float boxLeft, float boxRight, float boxBottom, float height, bool isTeammate) {
    if (player.maxClipAmmo <= 0) return;
    if (player.clipAmmo < 0) return;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    float barHeight = 1.5f;
    float barWidth = boxRight - boxLeft;
    float barY = boxBottom + 1.0f;

    float ammoRatio = static_cast<float>(player.clipAmmo) / static_cast<float>(player.maxClipAmmo);
    if (ammoRatio < 0.0f) ammoRatio = 0.0f;
    if (ammoRatio > 1.0f) ammoRatio = 1.0f;

    ImColor ammoColor = GetAmmoColor(ammoRatio, isTeammate);

    ImVec2 bgMin = ImVec2(boxLeft, barY);
    ImVec2 bgMax = ImVec2(boxRight, barY + barHeight);
    draw->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 200));

    if (ammoRatio > 0.001f) {
        float fillWidth = barWidth * ammoRatio;
        ImVec2 fillMin = ImVec2(boxLeft, barY);
        ImVec2 fillMax = ImVec2(boxLeft + fillWidth, barY + barHeight);
        draw->AddRectFilled(fillMin, fillMax, ammoColor);
    }
}
//сабнись на тгк если ты тут был
void DrawWeaponName(const CachedPlayer& player, float boxLeft, float boxRight, float boxBottom, float height, bool isTeammate) {
    if (!player.weaponName.empty()) {
        const char* weaponStr = player.weaponName.c_str();

        float baseHeight = 16.0f;
        float scaleFactor = height / 120.0f;
        float fontSize = baseHeight * scaleFactor;
        if (fontSize < 10.0f) fontSize = 10.0f;
        if (fontSize > 22.0f) fontSize = 22.0f;

        ImGuiIO& io = ImGui::GetIO();
        ImFont* currentFont = io.Fonts->Fonts[0];
        ImVec2 textSize = currentFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, weaponStr);

        float boxCenterX = boxLeft + (boxRight - boxLeft) * 0.5f;
        float textX = boxCenterX - textSize.x * 0.5f;

        float textY = boxBottom + 4.0f;
        if ((isTeammate && bTeamAmmoBar) || (!isTeammate && bEnemyAmmoBar)) {
            textY += 3.0f;
        }

        float paddingX = 3.0f;
        float paddingY = 1.0f;
        ImVec2 rectMin = ImVec2(textX - paddingX, textY - paddingY);
        ImVec2 rectMax = ImVec2(textX + textSize.x + paddingX, textY + textSize.y + paddingY);

        ImColor bgColorIm = isTeammate ? ToImColor(g_teamWeaponBgColor) : ToImColor(g_enemyWeaponBgColor);
        ImU32 bgColor = IM_COL32(
            (int)(bgColorIm.Value.x * 255),
            (int)(bgColorIm.Value.y * 255),
            (int)(bgColorIm.Value.z * 255),
            120
        );
        float rounding = 2.0f;
        ImGui::GetBackgroundDrawList()->AddRectFilled(rectMin, rectMax, bgColor, rounding);

        ImColor textColorIm = isTeammate ? ToImColor(g_teamWeaponColor) : ToImColor(g_enemyWeaponColor);
        ImU32 textColor = IM_COL32(
            (int)(textColorIm.Value.x * 255),
            (int)(textColorIm.Value.y * 255),
            (int)(textColorIm.Value.z * 255),
            (int)(textColorIm.Value.w * 255)
        );
        ImGui::GetBackgroundDrawList()->AddText(currentFont, fontSize, ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, 230), weaponStr);
        ImGui::GetBackgroundDrawList()->AddText(currentFont, fontSize, ImVec2(textX, textY), textColor, weaponStr);
    }
}
//сабнись на тгк если ты тут был
void DrawEspBox(const CachedPlayer& player, const Matrix4x4& matrix, bool drawBox, bool drawHealth, bool drawName, bool drawWeapon, bool isTeammate) {
    Vector3 head = { player.origin.x, player.origin.y, player.origin.z + 72.f };
    Vector2 screenFeet, screenHead;
    if (!WorldToScreen(player.origin, screenFeet, matrix) || !WorldToScreen(head, screenHead, matrix)) return;

    float height = screenFeet.y - screenHead.y;
    if (height <= 0.f) return;
    float width = height * 0.5f;

    float boxLeft = screenHead.x - width * 0.5f;
    float boxRight = screenHead.x + width * 0.5f;
    float boxTop = screenHead.y;
    float boxBottom = screenFeet.y;

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImColor boxColor = isTeammate ? ToImColor(g_teamBoxColor) : ToImColor(g_enemyBoxColor);

    if (drawBox) {
        if (isTeammate) {
            draw->AddRect(ImVec2(boxLeft - 2, boxTop - 2), ImVec2(boxRight + 2, boxBottom + 2), IM_COL32(60, 60, 60, 255), 0.f, 0, 1.0f);
        }
        else {
            draw->AddRect(ImVec2(boxLeft - 1, boxTop - 1), ImVec2(boxRight + 1, boxBottom + 1), IM_COL32(0, 0, 0, 255), 0.f, 0, 1.0f);
            draw->AddRect(ImVec2(boxLeft + 1, boxTop + 1), ImVec2(boxRight - 1, boxBottom - 1), IM_COL32(0, 0, 0, 255), 0.f, 0, 1.0f);
        }
        draw->AddRect(ImVec2(boxLeft, boxTop), ImVec2(boxRight, boxBottom), boxColor, 0.f, 0, 1.5f);
    }

    if (drawHealth) {
        float barWidth = 1.5f;
        float barGap = 1.0f;
        float barLeft = boxLeft - barGap - barWidth;
        float barRight = boxLeft - barGap;
        draw->AddRectFilled(ImVec2(barLeft - 1, boxTop - 1), ImVec2(barRight + 1, boxBottom + 1), IM_COL32(0, 0, 0, 200));
        float healthRatio = static_cast<float>(player.health) * 0.01f;
        float barHeight = height * healthRatio;
        float barTop = boxBottom - barHeight;
        ImColor healthColor = GetHealthColor(healthRatio, isTeammate);
        draw->AddRectFilled(ImVec2(barLeft, barTop), ImVec2(barRight, boxBottom), healthColor);
    }

    if ((isTeammate && bTeamAmmoBar) || (!isTeammate && bEnemyAmmoBar)) {
        DrawAmmoBar(player, boxLeft, boxRight, boxBottom, height, isTeammate);
    }
    //сабнись на тгк если ты тут был
    if (drawName && player.name[0] != L'\0') {
        char nameStr[128] = { 0 };
        size_t converted = 0;
        wcstombs_s(&converted, nameStr, sizeof(nameStr), player.name, _TRUNCATE);
        nameStr[127] = '\0';
        size_t realLen = strnlen(nameStr, sizeof(nameStr));
        if (realLen == 0) return;
        while (realLen > 0 && (nameStr[realLen - 1] < 32 || nameStr[realLen - 1] > 126)) {
            nameStr[realLen - 1] = '\0';
            realLen--;
        }
        if (realLen == 0) return;

        float baseHeight = 16.0f;
        float scaleFactor = height / 120.0f;
        float fontSize = baseHeight * scaleFactor;
        if (fontSize < 10.0f) fontSize = 10.0f;
        if (fontSize > 22.0f) fontSize = 22.0f;

        ImGuiIO& io = ImGui::GetIO();
        ImFont* currentFont = io.Fonts->Fonts[0];
        ImVec2 textSize = currentFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, nameStr);

        float boxCenterX = boxLeft + (boxRight - boxLeft) * 0.5f;
        float textX = boxCenterX - textSize.x * 0.5f;
        float textY = boxTop - textSize.y - 4.0f;

        float paddingX = 3.0f;
        float paddingY = 1.0f;
        ImVec2 rectMin = ImVec2(textX - paddingX, textY - paddingY);
        ImVec2 rectMax = ImVec2(textX + textSize.x + paddingX, textY + textSize.y + paddingY);

        ImColor bgColorIm = isTeammate ? ToImColor(g_teamNameBgColor) : ToImColor(g_enemyNameBgColor);
        ImU32 bgColor = IM_COL32(
            (int)(bgColorIm.Value.x * 255),
            (int)(bgColorIm.Value.y * 255),
            (int)(bgColorIm.Value.z * 255),
            120
        );
        float rounding = 2.0f;
        draw->AddRectFilled(rectMin, rectMax, bgColor, rounding);

        ImColor textColorIm = isTeammate ? ToImColor(g_teamNameTextColor) : ToImColor(g_enemyNameTextColor);
        ImU32 textColor = IM_COL32(
            (int)(textColorIm.Value.x * 255),
            (int)(textColorIm.Value.y * 255),
            (int)(textColorIm.Value.z * 255),
            (int)(textColorIm.Value.w * 255)
        );
        draw->AddText(currentFont, fontSize, ImVec2(textX + 1, textY + 1), IM_COL32(0, 0, 0, 230), nameStr);
        draw->AddText(currentFont, fontSize, ImVec2(textX, textY), textColor, nameStr);
    }

    if (drawWeapon) {
        DrawWeaponName(player, boxLeft, boxRight, boxBottom, height, isTeammate);
    }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))) && pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
//сабнись на тгк если ты тут был
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL featureLevel;

    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, featureLevels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext)))
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
//сабнись на тгк если ты тут был
bool IsCS2Running() {
    return FindWindowA(nullptr, "Counter-Strike 2") != nullptr;
}

bool IsCS2Focused() {
    HWND cs2 = FindWindowA(nullptr, "Counter-Strike 2");
    return cs2 && GetForegroundWindow() == cs2;
}

void SetOverlayClickable(bool clickable) {
    if (g_bOverlayClickable == clickable) return;
    g_bOverlayClickable = clickable;
    LONG_PTR exStyle = GetWindowLongPtr(g_hWnd, GWL_EXSTYLE);
    if (clickable) {
        exStyle &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);
    }
    else {
        exStyle |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
    }
    SetWindowLongPtr(g_hWnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(g_hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    if (!clickable) {
        SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);
        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(g_hWnd, &margins);
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            RECT rect;
            GetClientRect(g_hWnd, &rect);
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, rect.right - rect.left, rect.bottom - rect.top, DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDPIAware();
    while (!IsCS2Running()) Sleep(100);
    //сабнись на тгк если ты тут был
    DWORD pid = GetProcessIdByName(L"cs2.exe");
    if (!pid) return 1;

    g_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (!g_hProcess) return 1;

    g_clientBase = GetModuleBaseAddress(pid, L"client.dll");
    if (!g_clientBase) return 1;

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, L"Overlay", nullptr };
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        wc.lpszClassName, L"CS2 Overlay", WS_POPUP, 0, 0, g_screenWidth, g_screenHeight, nullptr, nullptr, wc.hInstance, nullptr);

    if (!g_hWnd) return 1;

    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(g_hWnd, &margins);
    SetLayeredWindowAttributes(g_hWnd, 0, 255, LWA_ALPHA);

    if (!CreateDeviceD3D(g_hWnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2((float)g_screenWidth, (float)g_screenHeight);

    ImFont* watermarkFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 20.0f);
    ImFont* menuFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 16.0f);

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.ItemSpacing = ImVec2(8, 8);
    style.FramePadding = ImVec2(6, 4);
    style.ScrollbarSize = 12.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.95f);

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    SetOverlayClickable(true);

    std::thread readThread(MemoryReadLoop);
    std::thread aimThread(AimbotThread);

    bool done = false;

    std::vector<CachedPlayer> enemies;
    std::vector<CachedPlayer> teammates;

    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done || !IsCS2Running()) break;

        g_bGameFocused = IsCS2Focused();
        g_bInGame = IsInGame();

        if (bWaitingForKey && g_bGameFocused) {
            for (int key = 1; key < 255; key++) {
                if (GetAsyncKeyState(key) & 0x8000) {
                    if (key != VK_ESCAPE && key != VK_LWIN && key != VK_RWIN) {
                        aimKey = key;
                        bWaitingForKey = false;
                        break;
                    }
                }
            }
        }
        //сабнись на тгк если ты тут был
        if (bWaitingForMenuKey && g_bGameFocused) {
            for (int key = 1; key < 255; key++) {
                if (GetAsyncKeyState(key) & 0x8000) {
                    if (key != VK_ESCAPE && key != VK_LWIN && key != VK_RWIN) {
                        g_menuToggleKey = key;
                        bWaitingForMenuKey = false;
                        break;
                    }
                }
            }
        }

        if (g_bGameFocused && (GetAsyncKeyState(g_menuToggleKey) & 0x8000)) {
            if (!g_bInsertPressed) {
                g_bInsertPressed = true;
                g_bShowWindow = !g_bShowWindow;
                SetOverlayClickable(g_bShowWindow);
            }
        }
        else {
            g_bInsertPressed = false;
        }

        if (g_bGameFocused) {
            ShowWindow(g_hWnd, SW_SHOW);
            HWND cs2Wnd = FindWindowA(nullptr, "Counter-Strike 2");
            if (cs2Wnd) {
                RECT clientRect;
                if (GetClientRect(cs2Wnd, &clientRect)) {
                    POINT topLeft = { 0, 0 };
                    ClientToScreen(cs2Wnd, &topLeft);
                    g_screenWidth = clientRect.right - clientRect.left;
                    g_screenHeight = clientRect.bottom - clientRect.top;
                    SetWindowPos(g_hWnd, HWND_TOPMOST, topLeft.x, topLeft.y, g_screenWidth, g_screenHeight, SWP_NOACTIVATE | SWP_SHOWWINDOW);
                }
            }
        }
        else {
            ShowWindow(g_hWnd, SW_HIDE);
        }

        RECT rect;
        GetClientRect(g_hWnd, &rect);
        io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        //сабнись на тгк если ты тут был
        if (g_bGameFocused && g_clientBase) {
            if (bWatermark && g_bInGame) {
                ImGui::PushFont(watermarkFont);
                const char* watermarkText = "t.me/Freevexora";
                ImVec2 textSize = ImGui::CalcTextSize(watermarkText);
                ImVec2 textPos = ImVec2(io.DisplaySize.x * 0.5f - textSize.x * 0.5f, io.DisplaySize.y - 200.0f - textSize.y * 0.5f);
                ImGui::GetBackgroundDrawList()->AddText(ImVec2(textPos.x + 2, textPos.y + 2), IM_COL32(0, 0, 0, 100), watermarkText);
                ImGui::GetBackgroundDrawList()->AddText(textPos, IM_COL32(255, 255, 255, 255), watermarkText);
                ImGui::PopFont();
            }

            if (g_bInGame) {
                DrawFovCircle();
                DrawRadar();
            }

            if (g_bInGame) {
                Matrix4x4 matrix = Read<Matrix4x4>(g_hProcess, g_clientBase + Offsets::dwViewMatrix);
                int readIdx = g_ActiveBufferIdx.load(std::memory_order_acquire);
                enemies = g_FrameBuffers[readIdx].enemies;
                teammates = g_FrameBuffers[readIdx].teammates;

                if (bEnableEnemy) {
                    for (const auto& player : enemies) {
                        if (bEnemySkeleton) DrawSkeleton(player, matrix, false);
                        if (bEnemyDrawLines) DrawLineToPlayer(player, matrix, false);
                        DrawEspBox(player, matrix, bEnemyEspBox, bEnemyHealthBar, bEnemyName, bEnemyWeapon, false);
                    }
                }
                //сабнись на тгк если ты тут был
                if (bEnableTeam) {
                    for (const auto& player : teammates) {
                        if (bTeamSkeleton) DrawSkeleton(player, matrix, true);
                        if (bTeamDrawLines) DrawLineToPlayer(player, matrix, true);
                        DrawEspBox(player, matrix, bTeamEspBox, bTeamHealthBar, bTeamName, bTeamWeapon, true);
                    }
                }
            }

            if (g_bShowWindow) {
                ImGui::PushFont(menuFont);
                ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(580, 560), ImGuiCond_FirstUseEver);
                ImGui::Begin("t.me/Freevexora", &g_bShowWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
                //сабнись на тгк если ты тут был
                if (ImGui::BeginTabBar("MyTabBar")) {
                    if (ImGui::BeginTabItem("Visuals")) {
                        ImGui::Checkbox("Enable enemy", &bEnableEnemy);
                        if (bEnableEnemy) {
                            ImGui::Checkbox("Enemy | ESP Box", &bEnemyEspBox);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyBoxColor", g_enemyBoxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::Checkbox("Enemy | Health Bar", &bEnemyHealthBar);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyHPHigh", g_enemyHealthHighColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyHPLow", g_enemyHealthLowColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                            ImGui::Checkbox("Enemy | Ammo Bar", &bEnemyAmmoBar);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyAmmoHigh", g_enemyAmmoHighColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyAmmoLow", g_enemyAmmoLowColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                            ImGui::Checkbox("Enemy | Weapon", &bEnemyWeapon);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyWeaponTextColor", g_enemyWeaponColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyWeaponBgColor", g_enemyWeaponBgColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                            ImGui::Checkbox("Enemy | Skeleton", &bEnemySkeleton);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemySkeletonColor", g_enemySkeletonColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::Checkbox("Enemy | Draw Lines", &bEnemyDrawLines);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyLineColor", g_enemyLineColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::Checkbox("Enemy | Name", &bEnemyName);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyNameBgColor", g_enemyNameBgColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##EnemyNameTextColor", g_enemyNameTextColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        }

                        ImGui::Separator();

                        ImGui::Checkbox("Enable Team", &bEnableTeam);
                        if (bEnableTeam) {
                            ImGui::Checkbox("Team | ESP Box", &bTeamEspBox);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamBoxColor", g_teamBoxColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::Checkbox("Team | Health Bar", &bTeamHealthBar);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamHPHigh", g_teamHealthHighColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamHPLow", g_teamHealthLowColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                            ImGui::Checkbox("Team | Ammo Bar", &bTeamAmmoBar);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamAmmoHigh", g_teamAmmoHighColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamAmmoLow", g_teamAmmoLowColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                            ImGui::Checkbox("Team | Weapon", &bTeamWeapon);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamWeaponTextColor", g_teamWeaponColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamWeaponBgColor", g_teamWeaponBgColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

                            ImGui::Checkbox("Team | Skeleton", &bTeamSkeleton);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamSkeletonColor", g_teamSkeletonColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::Checkbox("Team | Draw Lines", &bTeamDrawLines);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamLineColor", g_teamLineColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::Checkbox("Team | Name", &bTeamName);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamNameBgColor", g_teamNameBgColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##TeamNameTextColor", g_teamNameTextColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                        }
                        ImGui::Separator();

                        ImGui::Checkbox("Overlay radar", &bRadarEnabled);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Aimbot")) {
                        ImGui::Checkbox("Enable Aimbot", &bAimbot);
                        if (bAimbot) {
                            ImGui::Separator();
                            ImGui::Checkbox("Aim at Team", &bAimbotTeam);
                            ImGui::Checkbox("DrawFov", &bAimbotFovCircle);
                            ImGui::SameLine();
                            ImGui::ColorEdit4("##FovCircleColor", g_fovCircleColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                            ImGui::Checkbox("Use Aim Key", &bUseAimKey);
                        }
                        if (bUseAimKey) {
                            ImGui::Text("Aim Key: %s", bWaitingForKey ? "Press any key..." : GetKeyName(aimKey));
                            if (bWaitingForKey) {
                                if (ImGui::Button("Cancel", ImVec2(-1, 25))) {
                                    bWaitingForKey = false;
                                }
                            }
                            else {
                                if (ImGui::Button("Set Key", ImVec2(-1, 25))) {
                                    bWaitingForKey = true;
                                }
                            }
                        }
                        const char* targetTypes[] = { "Head", "Body", "Legs" };
                        if (!bAimbot) ImGui::Separator();
                        ImGui::Combo("Target", &aimTarget, targetTypes, IM_ARRAYSIZE(targetTypes));
                        ImGui::SliderFloat("Smooth", &aimSmooth, 1.0f, 20.0f);
                        ImGui::SliderFloat("FOV", &aimFov, 1.0f, 90.0f);
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Settings")) {
                        ImGui::Checkbox("Watermark", &bWatermark);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show watermark text.");
                        ImGui::Separator();
                        ImGui::Text("Menu Toggle Key: %s", bWaitingForMenuKey ? "Press any key..." : GetKeyName(g_menuToggleKey));
                        if (bWaitingForMenuKey) {
                            if (ImGui::Button("Cancel", ImVec2(-1, 25))) {
                                bWaitingForMenuKey = false;
                            }
                        }
                        else {
                            if (ImGui::Button("Set Menu Key", ImVec2(-1, 25))) {
                                bWaitingForMenuKey = true;
                            }
                        }
                        ImGui::Separator();
                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.10f, 0.10f, 1.0f));
                        if (ImGui::Button("Unload", ImVec2(-1, 35)))
                            done = true;
                        ImGui::PopStyleColor(3);
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
                ImGui::End();
                ImGui::PopFont();
            }
        }

        ImGui::Render();
        if (g_bGameFocused) {
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clear_color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_pSwapChain->Present(0, 0);
        }
        else {
            Sleep(100);
        }
    }

    g_Running = false;
    if (readThread.joinable()) readThread.join();
    if (aimThread.joinable()) aimThread.join();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(g_hWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (g_hProcess) CloseHandle(g_hProcess);

    return 0;
}