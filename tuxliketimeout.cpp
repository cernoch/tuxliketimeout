// Copyright (c) 2018 Radomír Černoch
// 
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include <limits>
#include <string>
#include <iostream>
#include <windows.h>
#include <IntSafe.h>

#define EXIT_TIMEDOUT      (124) // job timed out
#define EXIT_CANCELED      (125) // internal error
#define EXIT_CANNOT_INVOKE (126) // error executing job
#define EXIT_ENOENT        (127) // couldn't find job to exec

/**
 * This routine appends the given argument to a command line such
 * that CommandLineToArgvW will return the argument string unchanged.
 * Arguments in a command line should be separated by spaces;
 * this function does not add these spaces.
 *
 * Source:
 * https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/
 * 
 * \param[in] Argument supplies the argument to encode.
 * \param[out] CommandLine supplies the command line to which
 *             we append the encoded argument string.
 * \param[in] Force supplies an indication of whether we should quote
 *            the argument even if it does not contain any characters
 *            that would ordinarily require quoting.
 */
void ArgvQuote (const std::wstring& Argument,
    std::wstring& CommandLine, bool Force)
{
    // Unless we're told otherwise, don't quote unless we actually
    // need to do so --- hopefully avoid problems if programs won't
    // parse quotes properly
    if (Force == false
            && Argument.empty() == false
            && Argument.find_first_of(L" \t\n\v\"") == Argument.npos) {
        CommandLine.append(Argument);
    } else {
        CommandLine.push_back(L'"');
        
        for (auto It = Argument.begin(); ; ++It) {
            unsigned NumberBackslashes = 0;
        
            while (It != Argument.end() && *It == L'\\') {
                ++It;
                ++NumberBackslashes;
            }
        
            if (It == Argument.end()) {
                // Escape all backslashes, but let the terminating
                // double quotation mark we add below be interpreted
                // as a metacharacter.
                CommandLine.append(NumberBackslashes * 2, L'\\');
                break;
            } else if (*It == L'"') {
                // Escape all backslashes and the following
                // double quotation mark.
                CommandLine.append(NumberBackslashes * 2 + 1, L'\\');
                CommandLine.push_back(*It);
            } else {
                // Backslashes aren't special here.
                CommandLine.append(NumberBackslashes, L'\\');
                CommandLine.push_back(*It);
            }
        }
    
        CommandLine.push_back(L'"');
    }
}



/**
 * Calls a clean-up method in the destructor.
 * 
 * Allocate this object to ensure that any subsequent
 * `return` calls the `CloseHandle` on the given handle.
 */
struct HandleGuard {

    HANDLE m_handle;

    HandleGuard(HANDLE handle)
        : m_handle(handle) {}

    ~HandleGuard() {
        CloseHandle(m_handle);
    }
};



int wmain(int argc, wchar_t *argv[], wchar_t *envp[]) {

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (argc < 3) {
        std::wcerr << L"Usage: " << std::wstring(argv[0]);
        std::wcerr << L" TIMEOUT PROGRAM [ARGUMENTS...]" << std::endl;
        return EXIT_CANCELED;
    }

    // Parse the TIMEOUT parameter
    DWORD time_out;
    try {
        time_out = std::stoul(std::wstring(argv[1]));
    } catch(std::invalid_argument&) {
        std::wcerr << L"The TIMEOUT must be a number in 0..4294967295." << std::endl;
        return EXIT_CANCELED;
    } catch(std::out_of_range&) {
        std::wcerr << L"The TIMEOUT must be a number in 0..4294967295." << std::endl;
        return EXIT_CANCELED;
    }

    std::wstring command_line;
    for (int argi = 2; argi < argc; argi++) {
        ArgvQuote(std::wstring(argv[argi]), command_line, false);
        if (argi < argc) {
            command_line += L" ";
        }
    }

    // Do not use/modify/... command_line after this line:
    LPWSTR command_line_buf = const_cast<wchar_t*>(command_line.c_str());

    // Start the child process. 
    if (!CreateProcessW(
        NULL,           // No module name (use command line)
        command_line_buf, // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi )           // Pointer to PROCESS_INFORMATION structure
    ) {
        switch (GetLastError()) {
            
            case ERROR_FILE_NOT_FOUND:
                std::wcerr << L"Command '" << std::wstring(argv[2]);
                std::wcerr << "' not found." << std::endl;
                return EXIT_ENOENT;

            default:
                std::wcerr << L"CreateProcess failed. (ERROR ";
                std::wcerr << GetLastError() << L")" << std::endl;
                return EXIT_CANNOT_INVOKE;
        }
    }

    // Close all handles on any path
    HandleGuard hProcessGuard(pi.hProcess);
    HandleGuard hThreadGuard(pi.hThread);

    // Wait until child process exits.
    switch (WaitForSingleObject(pi.hProcess, time_out)) {
        
        case WAIT_FAILED:
            std::wcerr << L"WaitForSingleObject failed. (ERROR ";
            std::wcerr << GetLastError() << L")" << std::endl;
            return EXIT_CANCELED;
        
        case WAIT_TIMEOUT:
            if (!TerminateProcess(pi.hProcess, 0)) {
                std::wcerr << L"TerminateProcess failed. (ERROR ";
                std::wcerr << GetLastError() << L")" << std::endl;
                return EXIT_CANCELED;
            }
            if (!WaitForSingleObject(pi.hThread, 0)) {
                std::wcerr << L"WaitForSingleObject failed. (ERROR ";
                std::wcerr << GetLastError() << L")" << std::endl;
                return EXIT_CANCELED;
            }
            return EXIT_TIMEDOUT;

        case WAIT_OBJECT_0:
            DWORD error_code;
            if (GetExitCodeProcess(pi.hProcess, &error_code)) {
                return error_code;
            } else {
                std::wcerr << L"GetExitCodeProcess failed. (ERROR ";
                std::wcerr << GetLastError() << L")" << std::endl;
                return EXIT_CANCELED;
            }

        default:
            std::wcerr << L"WaitForSingleObject returned an unexpected ";
            std::wcerr << L" value (" << GetLastError() << L")." << std::endl;
            return EXIT_CANCELED;
    }
}
