#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include <ctime>
#include "qrcodegen.hpp"

using namespace qrcodegen;




HWND hInput, hGenerate, hSave;
HFONT hFont;
QrCode currentQr = QrCode::encodeText("", QrCode::Ecc::LOW);
bool qrGenerated = false;



void SaveQrToFile(const QrCode& qr, const std::string& filename, int scale) {
    int size = qr.getSize();
    int imgSize = size * scale;
    int channels = 3;
    std::vector<unsigned char> img(imgSize * imgSize * channels);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            bool dot = qr.getModule(x, y);
            unsigned char color = dot ? 255 : 0;
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    int idx = ((y * scale + dy) * imgSize + (x * scale + dx)) * channels;
                    img[idx + 0] = color;
                    img[idx + 1] = color;
                    img[idx + 2] = color;
                }
            }
        }
    }

    if (filename.ends_with(".png"))
        stbi_write_png(filename.c_str(), imgSize, imgSize, channels, img.data(), imgSize * channels);
    else if (filename.ends_with(".jpg"))
        stbi_write_jpg(filename.c_str(), imgSize, imgSize, channels, img.data(), 90);
}

void DrawQrToDC(HDC hdc, RECT clientRect, const QrCode& qr) {
    int size = qr.getSize();
    int availWidth = clientRect.right - clientRect.left;
    int availHeight = clientRect.bottom - clientRect.top - 100;
    int scale = std::min(availWidth, availHeight) / size;
    if (scale < 1) scale = 1;

    int qrPixelSize = size * scale;
    int offsetX = (availWidth - qrPixelSize) / 2;
    int offsetY = 100 + (availHeight - qrPixelSize) / 2;

    HBRUSH bg = CreateSolidBrush(RGB(20, 20, 20));
    FillRect(hdc, &clientRect, bg);
    DeleteObject(bg);

    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            RECT r = { offsetX + x * scale, offsetY + y * scale,
                       offsetX + (x + 1) * scale, offsetY + (y + 1) * scale };
            FillRect(hdc, &r, qr.getModule(x, y) ? white : black);
        }
    }

    DeleteObject(white);
    DeleteObject(black);
}

LRESULT HandleCtlColor(UINT msg, HDC hdc, HWND hwndCtl) {
    SetBkColor(hdc, RGB(42, 42, 42));
    SetTextColor(hdc, RGB(255, 255, 255));
    static HBRUSH hbrDark = CreateSolidBrush(RGB(42, 42, 42));
    return (LRESULT)hbrDark;
}

void GenerateQr(HWND hwnd) {
    char text[512];
    GetWindowTextA(hInput, text, 512);
    if (strlen(text) == 0) {
        MessageBoxA(hwnd, "Please enter text or a link first.", "Info", MB_OK | MB_ICONINFORMATION);
        return;
    }
    currentQr = QrCode::encodeText(text, QrCode::Ecc::LOW);
    qrGenerated = true;
    InvalidateRect(hwnd, NULL, TRUE);
}

std::string GetTimestampFilename() {
    time_t now = time(nullptr);
    tm t;
    localtime_s(&t, &now);
    char buf[64];
    strftime(buf, sizeof(buf), "QRCode_%Y%m%d_%H%M%S.png", &t);
    return buf;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hFont = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");

        CreateWindowA("static", "Enter link:", WS_VISIBLE | WS_CHILD,
            20, 25, 90, 25, hwnd, NULL, NULL, NULL);

        hInput = CreateWindowA("edit", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            120, 25, 360, 25, hwnd, (HMENU)100, NULL, NULL);

        hGenerate = CreateWindowA("button", "Generate", WS_VISIBLE | WS_CHILD,
            500, 25, 100, 25, hwnd, (HMENU)1, NULL, NULL);

        hSave = CreateWindowA("button", "Save QR", WS_VISIBLE | WS_CHILD,
            500, 65, 100, 25, hwnd, (HMENU)2, NULL, NULL);

        SendMessageA(hInput, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageA(hGenerate, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageA(hSave, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
        return HandleCtlColor(msg, (HDC)wParam, (HWND)lParam);

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) GenerateQr(hwnd);
        else if (LOWORD(wParam) == 2 && qrGenerated) {
            OPENFILENAMEA ofn = { sizeof(ofn) };
            char filename[MAX_PATH];
            std::string suggested = GetTimestampFilename();
            strcpy_s(filename, suggested.c_str());
            ofn.lpstrFilter = "PNG Files\0*.png\0JPEG Files\0*.jpg\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileNameA(&ofn)) {
                SaveQrToFile(currentQr, filename, 8);
                MessageBoxA(hwnd, "QR code saved successfully!", "Saved", MB_OK);
            }
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            GenerateQr(hwnd);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(20, 20, 20));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        if (qrGenerated)
            DrawQrToDC(hdc, rc, currentQr);
        EndPaint(hwnd, &ps);
        break;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_DESTROY:
        DeleteObject(hFont);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSW wc = { 0 };
    wc.hbrBackground = CreateSolidBrush(RGB(20, 20, 20));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"QrWinApp";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"QrWinApp", L"QR CODE generator",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 500,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
