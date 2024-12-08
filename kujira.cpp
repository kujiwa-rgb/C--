#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <thread>
#include <chrono>
#include <iomanip> // For std::put_time
#include <ctime>   // For std::time_t
#include <sstream> // For std::ostringstream

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

void hideCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(consoleHandle, &cursorInfo);
    cursorInfo.bVisible = false; // Set the cursor visibility to false
    SetConsoleCursorInfo(consoleHandle, &cursorInfo);
}

// Function to check if the console size has changed
bool isConsoleResized(HANDLE hConsole, COORD &lastSize) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        // Explicitly cast to SHORT to avoid narrowing conversion
        SHORT width = static_cast<SHORT>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
        SHORT height = static_cast<SHORT>(csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
        
        COORD currentSize = {width, height};

        if (currentSize.X != lastSize.X || currentSize.Y != lastSize.Y) {
            lastSize = currentSize;
            return true; // Size changed
        }
    }
    return false;
}

void clearLine(int yPos) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = {0, static_cast<SHORT>(yPos)}; // Explicitly cast to SHORT
    DWORD written;
    FillConsoleOutputCharacter(hConsole, ' ', 80, coord, &written); // Clear the line
    FillConsoleOutputAttribute(hConsole, FOREGROUND_BLUE, 80, coord, &written); // Reset attributes
}

void displayClock(int consoleHeight) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD lastSize = {0, 0}; // Initialize last size to prevent false positive on first check
    
    hideCursor(); // Hide the cursor to prevent flickering

    while (true) {
        // Get the console size
        GetConsoleScreenBufferInfo(hConsole, &csbi);
        int consoleWidth = csbi.srWindow.Right - csbi.srWindow.Left + 1; // Width of the console

        // Get current time
        std::time_t now = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);
        
        // Format time as HH:MM:SS
        std::ostringstream oss;
        oss << std::put_time(localTime, "%H:%M:%S");
        std::string currentTimeStr = oss.str();

        // Calculate position for the clock (right-hand side of the console)
        int clockPosition = consoleWidth - static_cast<int>(currentTimeStr.length()) - 10; // 10 for "Current Time: "
        if (clockPosition < 0) clockPosition = 0; // Ensure we don't go out of bounds

        // Set cursor position and write time directly to the console
        COORD coord = {static_cast<SHORT>(clockPosition), static_cast<SHORT>(consoleHeight - 1)};
        SetConsoleCursorPosition(hConsole, coord);

        DWORD written;
        std::string output = "Time: " + currentTimeStr;
        WriteConsoleOutputCharacter(hConsole, output.c_str(), static_cast<DWORD>(output.length()), coord, &written);

        // Sleep for 1 second
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void getIPAddress() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    struct hostent* host = gethostbyname(hostname);
    if (host == nullptr) {
        std::cout << "Unable to get IP Address." << std::endl;
    } else {
        std::cout << "IP Address: " << inet_ntoa(*(struct in_addr*)host->h_addr) << std::endl;
    }

    WSACleanup();
}

void getMACAddress() {
    IP_ADAPTER_INFO adapterInfo[16];  // Buffer to store adapter information
    DWORD bufferSize = sizeof(adapterInfo);

    DWORD status = GetAdaptersInfo(adapterInfo, &bufferSize);
    if (status == ERROR_SUCCESS) {
        PIP_ADAPTER_INFO adapter = adapterInfo;
        while (adapter) {
            // Check if the adapter has an IP address assigned
            if (adapter->IpAddressList.IpAddress.String[0] != '0') {
                std::cout << "MAC Address: ";
                for (UINT i = 0; i < adapter->AddressLength; i++) {
                    if (i > 0) std::cout << "-";
                    printf("%02X", adapter->Address[i]);
                }
                std::cout << std::endl;
                break;  // Stop after finding the first active adapter with an IP address
            }
            adapter = adapter->Next;
        }
    } else {
        std::cout << "Unable to get MAC Address." << std::endl;
    }
}

int main() {
    // Set up the console appearance
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD bufferSize;
    bufferSize.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    bufferSize.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    SetConsoleScreenBufferSize(hConsole, bufferSize);
    system("color 17");

    // Display initial message
    std::cout << "Kujira is running... Welcome v0.0.1" << std::endl;
    std::cout << "Press Ctrl + I to display your IP and MAC Address." << std::endl;
    std::cout << "Press 'Q' to quit." << std::endl;

    // Start the clock in a separate thread
    int consoleHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    std::thread clockThread(displayClock, consoleHeight);

    while (true) {
        // Check if Ctrl + I is pressed
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState('I') & 0x8000)) {
            std::cout << "\n--- Network Information ---" << std::endl;
            getIPAddress();
            getMACAddress();
            std::cout << "---------------------------" << std::endl;
        }

        // Wait for user to press 'Q' to quit
        if (GetAsyncKeyState('Q') & 0x8000) {
            break;
        }

        Sleep(100); // Sleep to prevent high CPU usage
    }

    return 0;
}
