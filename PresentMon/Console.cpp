/*
Copyright 2017-2019 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "PresentMon.hpp"

void SetConsoleText(const char *text)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    enum { MAX_BUFFER = 16384 };
    char buffer[16384];
    int bufferSize = 0;
    auto write = [&](char ch) {
        if (bufferSize < MAX_BUFFER) {
            buffer[bufferSize++] = ch;
        }
    };

    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        return;
    }

    int oldBufferSize = int(csbi.dwSize.X * csbi.dwSize.Y);
    if (oldBufferSize > MAX_BUFFER) {
        oldBufferSize = MAX_BUFFER;
    }

    int x = 0;
    while (*text) {
        int repeat = 1;
        char ch = *text;
        if (ch == '\t') {
            ch = ' ';
            repeat = 4;
        }
        else if (ch == '\n') {
            ch = ' ';
            repeat = csbi.dwSize.X - x;
        }
        for (int i = 0; i < repeat; ++i) {
            write(ch);
            if (++x >= csbi.dwSize.X) {
                x = 0;
            }
        }
        text++;
    }

    for (int i = bufferSize; i < oldBufferSize; ++i)
    {
        write(' ');
    }

    COORD origin = { 0,0 };
    DWORD dwCharsWritten;
    WriteConsoleOutputCharacterA(
        hConsole,
        buffer,
        bufferSize,
        origin,
        &dwCharsWritten);

    SetConsoleCursorPosition(hConsole, origin);
}

void UpdateConsole(uint32_t processId, ProcessInfo const& processInfo, std::string* display)
{
    auto const& args = GetCommandLineArgs();

    // Don't display non-target or empty processes
    if (!processInfo.mTargetProcess ||
        processInfo.mModuleName.empty() ||
        processInfo.mChainMap.empty()) {
        return;
    }

    char str[256] = {};
    _snprintf_s(str, _TRUNCATE, "\n%s[%d]:\n", processInfo.mModuleName.c_str(), processId);
    *display += str;

    for (auto& chain : processInfo.mChainMap)
    {
        double fps = chain.second.ComputeFps();

        _snprintf_s(str, _TRUNCATE, "\t%016llX (%s): SyncInterval %d | Flags %d | %.2lf ms/frame (%.1lf fps, ",
            chain.first,
            RuntimeToString(chain.second.mRuntime),
            chain.second.mLastSyncInterval,
            chain.second.mLastFlags,
            1000.0/fps,
            fps);
        *display += str;

        if (args.mVerbosity > Verbosity::Simple) {
            _snprintf_s(str, _TRUNCATE, "%.1lf displayed fps, ", chain.second.ComputeDisplayedFps());
            *display += str;
        }

        _snprintf_s(str, _TRUNCATE, "%.2lf ms CPU", chain.second.ComputeCpuFrameTime() * 1000.0);
        *display += str;

        if (args.mVerbosity > Verbosity::Simple) {
            _snprintf_s(str, _TRUNCATE, ", %.2lf ms latency) (%s",
                1000.0 * chain.second.ComputeLatency(),
                PresentModeToString(chain.second.mLastPresentMode));
            *display += str;

            if (chain.second.mLastPresentMode == PresentMode::Hardware_Composed_Independent_Flip) {
                _snprintf_s(str, _TRUNCATE, ": Plane %d", chain.second.mLastPlane);
                *display += str;
            }

            if ((chain.second.mLastPresentMode == PresentMode::Hardware_Composed_Independent_Flip ||
                 chain.second.mLastPresentMode == PresentMode::Hardware_Independent_Flip) &&
                args.mVerbosity >= Verbosity::Verbose &&
                chain.second.mDwmNotified) {
                _snprintf_s(str, _TRUNCATE, ", DWM notified");
                *display += str;
            }

            if (args.mVerbosity >= Verbosity::Verbose &&
                chain.second.mHasBeenBatched) {
                _snprintf_s(str, _TRUNCATE, ", batched");
                *display += str;
            }
        }

        _snprintf_s(str, _TRUNCATE, ")\n");
        *display += str;
    }
}
