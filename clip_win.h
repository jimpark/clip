// Clip Library
// Copyright (c) 2015 David Capello

#include <cassert>
#include <vector>
#include <windows.h>

namespace clip {

class lock::impl {
public:

  impl(void* hwnd) {
    bool opened = false;

    for (int i=0; i<5; ++i) {
      if (OpenClipboard((HWND)hwnd)) {
        opened = true;
        break;
      }
      Sleep(100);
    }

    if (!opened)
      throw std::runtime_error("Cannot open clipboard");
  }

  ~impl() {
    CloseClipboard();
  }

  bool clear() {
    return (EmptyClipboard() ? true: false);
  }

  bool is_convertible(format f) const {
    if (f == text_format()) {
      return
        (IsClipboardFormatAvailable(CF_TEXT) ||
         IsClipboardFormatAvailable(CF_UNICODETEXT) ||
         IsClipboardFormatAvailable(CF_OEMTEXT));
    }
    else if (IsClipboardFormatAvailable(f))
      return true;
    else
      return false;
  }

  bool set_data(format f, const char* buf, size_t len) {
    bool result = false;

    if (f == text_format()) {
      if (len > 0) {
        int reqsize = MultiByteToWideChar(CP_UTF8, 0, buf, len, NULL, 0);
        if (reqsize > 0) {
          ++reqsize;

          HGLOBAL hglobal = GlobalAlloc(GMEM_MOVEABLE |
                                        GMEM_ZEROINIT, sizeof(WCHAR)*reqsize);
          LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
          MultiByteToWideChar(CP_UTF8, 0, buf, len, lpstr, reqsize);
          GlobalUnlock(hglobal);

          SetClipboardData(CF_UNICODETEXT, hglobal);
        }
      }
      result = true;
    }
    else {
      HGLOBAL hglobal = GlobalAlloc(GHND, len+sizeof(size_t));
      if (hglobal) {
        size_t* dst = (size_t*)GlobalLock(hglobal);
        if (dst) {
          *dst = len;
          memcpy(dst+1, buf, len);
          GlobalUnlock(hglobal);
          SetClipboardData(f, hglobal);
          GlobalFree(hglobal);
          result = true;
        }
      }
    }

    return result;
  }

  bool get_data(format f, char* buf, size_t len) const {
    assert(buf);

    if (!buf || !is_convertible(f))
      return false;

    bool result = false;

    if (f == text_format()) {
      if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HGLOBAL hglobal = GetClipboardData(CF_UNICODETEXT);
        if (hglobal) {
          LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
          if (lpstr) {
            size_t reqsize =
              WideCharToMultiByte(CP_UTF8, 0, lpstr, wcslen(lpstr),
                                  NULL, 0, NULL, NULL) + 1;

            assert(reqsize <= len);
            if (reqsize <= len) {
              WideCharToMultiByte(CP_UTF8, 0, lpstr, wcslen(lpstr),
                                  buf, reqsize, NULL, NULL);
              result = true;
            }
            GlobalUnlock(hglobal);
          }
        }
      }
      else if (IsClipboardFormatAvailable(CF_TEXT)) {
        HGLOBAL hglobal = GetClipboardData(CF_TEXT);
        if (hglobal) {
          LPSTR lpstr = static_cast<LPSTR>(GlobalLock(hglobal));
          if (lpstr) {
            // TODO check length
            memcpy(buf, lpstr, len);
            result = true;
            GlobalUnlock(hglobal);
          }
        }
      }
    }
    else {
      if (IsClipboardFormatAvailable(f)) {
        HGLOBAL hglobal = GetClipboardData(f);
        if (hglobal) {
          const size_t* ptr = (const size_t*)GlobalLock(hglobal);
          if (ptr) {
            size_t reqsize = *ptr;
            assert(reqsize <= len);
            if (reqsize <= len) {
              memcpy(buf, ptr+1, reqsize);
              result = true;
            }
            GlobalUnlock(hglobal);
          }
        }
      }
    }

    return result;
  }

  size_t get_data_length(format f) const {
    size_t len = 0;

    if (f == text_format()) {
      if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HGLOBAL hglobal = GetClipboardData(CF_UNICODETEXT);
        if (hglobal) {
          LPWSTR lpstr = static_cast<LPWSTR>(GlobalLock(hglobal));
          if (lpstr) {
            len =
              WideCharToMultiByte(CP_UTF8, 0, lpstr, wcslen(lpstr),
                                  NULL, 0, NULL, NULL) + 1;
            GlobalUnlock(hglobal);
          }
        }
      }
      else if (IsClipboardFormatAvailable(CF_TEXT)) {
        HGLOBAL hglobal = GetClipboardData(CF_TEXT);
        if (hglobal) {
          LPSTR lpstr = (LPSTR)GlobalLock(hglobal);
          if (lpstr) {
            len = strlen(lpstr) + 1;
            GlobalUnlock(hglobal);
          }
        }
      }
    }
    // TODO check if it's a registered custom format
    else if (f != empty_format()) {
      if (IsClipboardFormatAvailable(f)) {
        HGLOBAL hglobal = GetClipboardData(f);
        if (hglobal) {
          const size_t* ptr = (const size_t*)GlobalLock(hglobal);
          if (ptr) {
            len = *ptr;
            GlobalUnlock(hglobal);
          }
        }
      }
    }

    return len;
  }

};

format register_format(const std::string& name) {
  int reqsize = 1+MultiByteToWideChar(CP_UTF8, 0,
                                      name.c_str(), name.size(), NULL, 0);
  std::vector<WCHAR> buf(reqsize);
  MultiByteToWideChar(CP_UTF8, 0, name.c_str(), name.size(),
                      &buf[0], reqsize);

  return (format)RegisterClipboardFormatW(&buf[0]);
}

} // namespace clip