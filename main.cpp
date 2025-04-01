#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "RtMidi.h"

#pragma comment(lib, "winmm.lib")

#define ID_COMBOBOX 1001
#define ID_BUTTON_START 1002

HWND hComboBox, hButton;
HWAVEOUT hWaveOut = nullptr;
WAVEHDR waveHeader = {};
std::vector<short> audioBuffer;
RtMidiIn* midiin = nullptr;
const int SAMPLE_RATE = 44100;
const int BUFFER_SIZE = 4096;
const double A4_FREQUENCY = 440.0;

void midiCallback(double, std::vector<unsigned char>* message, void*) noexcept {
    if (message->size() < 3) return;

    unsigned char status = message->at(0);
    unsigned char note = message->at(1);
    unsigned char velocity = message->at(2);

    bool isNoteOn = (status & 0xF0) == 0x90 && velocity > 0;
    if (isNoteOn) {
        double freq = A4_FREQUENCY * pow(2.0, (note - 69) / 12.0);
        audioBuffer.resize(BUFFER_SIZE);
        for (int i = 0; i < BUFFER_SIZE; ++i) {
            double t = static_cast<double>(i) / SAMPLE_RATE;
            audioBuffer[i] = static_cast<short>(32767 * sin(2 * 3.14159265358979323846 * freq * t));
        }

        waveHeader.lpData = reinterpret_cast<LPSTR>(audioBuffer.data());
        waveHeader.dwBufferLength = BUFFER_SIZE * sizeof(short);
        waveHeader.dwFlags = 0;

        waveOutPrepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &waveHeader, sizeof(WAVEHDR));
    }
    else {
        waveOutReset(hWaveOut);
        waveOutUnprepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR));
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        hComboBox = CreateWindowW(L"COMBOBOX", nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            20, 20, 250, 200, hWnd, (HMENU)ID_COMBOBOX, nullptr, nullptr);

        hButton = CreateWindowW(L"BUTTON", L"開始",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            290, 20, 80, 25, hWnd, (HMENU)ID_BUTTON_START, nullptr, nullptr);

        try {
            midiin = new RtMidiIn();
            unsigned int ports = midiin->getPortCount();
            for (unsigned int i = 0; i < ports; ++i) {
                std::string name = midiin->getPortName(i);
                std::wstring wname(name.begin(), name.end());
                SendMessageW(hComboBox, CB_ADDSTRING, 0, (LPARAM)wname.c_str());
            }
            if (ports > 0) SendMessageW(hComboBox, CB_SETCURSEL, 0, 0);
        }
        catch (...) {
            MessageBoxW(hWnd, L"RtMidiの初期化に失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
            PostQuitMessage(1);
        }
        break;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BUTTON_START) {
            int index = (int)SendMessageW(hComboBox, CB_GETCURSEL, 0, 0);
            if (index == CB_ERR) {
                MessageBoxW(hWnd, L"ポートを選択してください。", L"エラー", MB_OK | MB_ICONERROR);
                return 0;
            }

            // オーディオ初期化
            WAVEFORMATEX wf = {};
            wf.wFormatTag = WAVE_FORMAT_PCM;
            wf.nChannels = 1;
            wf.nSamplesPerSec = SAMPLE_RATE;
            wf.wBitsPerSample = 16;
            wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
            wf.nAvgBytesPerSec = SAMPLE_RATE * wf.nBlockAlign;

            if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
                MessageBoxW(hWnd, L"オーディオ出力の初期化に失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
                return 0;
            }

            try {
                midiin->openPort(index);
                midiin->setCallback(&midiCallback);
                midiin->ignoreTypes(false, false, false);
                MessageBoxW(hWnd, L"MIDI接続成功。鍵盤を押してください！", L"OK", MB_OK);
            }
            catch (...) {
                MessageBoxW(hWnd, L"MIDIポートのオープンに失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
            }
        }
        break;

    case WM_DESTROY:
        if (hWaveOut) waveOutClose(hWaveOut);
        if (midiin) {
            midiin->closePort();
            delete midiin;
        }

        PostQuitMessage(0);
        ExitProcess(0);  // ← 追加：強制終了で .exe が残るのを防ぐ
        break;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MidiSynthWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    HWND hWnd = CreateWindowW(L"MidiSynthWindowClass", L"MIDIシンセサイザー",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 120,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
