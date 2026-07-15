#include "utils.h"

#include <flutter_windows.h>
#include <io.h>
#include <shellapi.h>
#include <stdio.h>
#include <windows.h>

#include <iostream>
#include <string>

void CreateAndAttachConsole() {
  if (::AllocConsole()) {
    FILE *unused;
    if (freopen_s(&unused, "CONOUT$", "w", stdout)) {
      _dup2(_fileno(stdout), 1);
    }
    if (freopen_s(&unused, "CONOUT$", "w", stderr)) {
      _dup2(_fileno(stdout), 2);
    }
    std::ios::sync_with_stdio();
    FlutterDesktopResyncOutputStreams();
  }
}

bool IsRunningAsAdministrator() {
  BOOL is_admin = FALSE;
  PSID admin_group = nullptr;
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  if (::AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                 &admin_group)) {
    ::CheckTokenMembership(nullptr, admin_group, &is_admin);
    ::FreeSid(admin_group);
  }
  return is_admin != FALSE;
}

static std::wstring QuoteArgument(const wchar_t* argument) {
  std::wstring quoted = L"\"";
  size_t slashes = 0;
  for (const wchar_t* ch = argument; ; ch++) {
    if (*ch == L'\\') {
      slashes++;
      continue;
    }
    if (*ch == L'"' || *ch == L'\0') {
      quoted.append(slashes * 2, L'\\');
      slashes = 0;
      if (*ch == L'"') {
        quoted.append(L"\\\"");
        continue;
      }
      break;
    }
    quoted.append(slashes, L'\\');
    slashes = 0;
    quoted.push_back(*ch);
  }
  quoted.push_back(L'"');
  return quoted;
}

bool RelaunchAsAdministrator() {
  wchar_t exe_path[MAX_PATH];
  DWORD path_length = ::GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
  if (path_length == 0 || path_length >= MAX_PATH) {
    return false;
  }

  int argc = 0;
  wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return false;
  }

  std::wstring parameters;
  for (int i = 1; i < argc; i++) {
    if (!parameters.empty()) {
      parameters.push_back(L' ');
    }
    parameters.append(QuoteArgument(argv[i]));
  }
  ::LocalFree(argv);

  HINSTANCE result = ::ShellExecuteW(
      nullptr, L"runas", exe_path,
      parameters.empty() ? nullptr : parameters.c_str(), nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(result) > 32;
}

std::vector<std::string> GetCommandLineArguments() {
  // Convert the UTF-16 command line arguments to UTF-8 for the Engine to use.
  int argc;
  wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
  if (argv == nullptr) {
    return std::vector<std::string>();
  }

  std::vector<std::string> command_line_arguments;

  // Skip the first argument as it's the binary name.
  for (int i = 1; i < argc; i++) {
    command_line_arguments.push_back(Utf8FromUtf16(argv[i]));
  }

  ::LocalFree(argv);

  return command_line_arguments;
}

std::string Utf8FromUtf16(const wchar_t* utf16_string) {
  if (utf16_string == nullptr) {
    return std::string();
  }
  // First, find the length of the string with a safe upper bound (CWE-126).
  // UNICODE_STRING_MAX_CHARS (32767) is the maximum length of a UNICODE_STRING.
  int input_length = static_cast<int>(wcsnlen(utf16_string, UNICODE_STRING_MAX_CHARS));
  // Now use that bounded length to determine the required buffer size.
  // When an explicit length is passed, WideCharToMultiByte does not include
  // the null terminator in its returned size.
  int target_length = ::WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, utf16_string,
      input_length, nullptr, 0, nullptr, nullptr);
  std::string utf8_string;
  if (target_length == 0 || static_cast<size_t>(target_length) > utf8_string.max_size()) {
    return utf8_string;
  }
  utf8_string.resize(target_length);
  int converted_length = ::WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, utf16_string,
      input_length, utf8_string.data(), target_length, nullptr, nullptr);
  if (converted_length == 0) {
    return std::string();
  }
  return utf8_string;
}
