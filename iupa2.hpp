// This code was written to win a bet with a friend, use it at your own risk

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <cctype>
#include <map>
#include <memory>

#ifdef _WIN32
#include <windows.h>

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#endif

namespace iupa2 {

  namespace {
    static const char VERTICAL_BORDER = '|';
    static const char HORIZONTAL_BORDER = '-';
    static const char EOL = '\n';
    static const char EOW = '\0'; // End of window
    static const char SPACE = ' ';

    inline std::string& rtrim(std::string &s) {
      s.erase(std::find_if(s.rbegin(), s.rend(),
        std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
      return s;
    }

    // Helper templates to enable std::function<> to static routine bindings
    template <typename Unique, typename Ret, typename... Args>
    struct std_fn_helper {
    public:
      typedef std::function<Ret(Args...)> function_type;

      template<typename T>
      static void bind(T&& std_fun) {
        instance().std_fun_internal = std::forward<T>(std_fun);
      }

      // The entire point of this helper is here: to get a pointer to this
      // static function which will call the internal std::function<>
      static Ret invoke(Args... args) {
        return instance().std_fun_internal(args...);
      }
      using pointer_type = decltype(&std_fn_helper::invoke);

      static pointer_type ptr() {
        return &invoke;
      }

    private:
      std_fn_helper() = default;

      // Singleton for this instance (assumes this template instance is unique)
      static std_fn_helper& instance() {
        static std_fn_helper inst_;
        return inst_;
      }

      function_type std_fun_internal;
    };

    template <typename Unique, typename Ret, typename... Args>
    auto std_fn_to_static_fn_helper(const std::function<Ret(Args...)>& std_fun) {
      std_fn_helper<Unique, Ret, Args...>::bind(std_fun);
      return std_fn_helper<Unique, Ret, Args...>::ptr();
    }

    // Guaranteed to be unique by [expr.prim.lambda]/3
#define std_fn_to_static_fn(fn, fptr) do { \
      auto ll = [](){}; \
      fptr = std_fn_to_static_fn_helper<decltype(ll)>(fn); \
    } while(0)

  }

  enum ElementType {
    Unknown,
    Button,
    EditBox,
    Label
  };

  struct Container {};

  struct Element : public Container {
    Container *parent = nullptr;
    std::vector<std::shared_ptr<Element>> sub_elements_;
    std::string text_;
    std::string id_;
    ElementType type_ = Unknown;
    int left_ = 0, top_ = 0;
    int bottom_ = 0, right_ = 0;
  };

  class Window : public Container {
  public:
    Window(const char *ascii) :
      ascii_(ascii)
    {
      build_window();
    }

    void build_window() {

      if (!ascii_) 
        return;

      auto skip_ws = [&](char& ch, int& i) {
        while (ascii_[i] == SPACE)
          ++i;
        ch = ascii_[i];
      };

      int width = 0, height = 0;
      int win_x = 0;
      bool inside_height = false,
        inside_width = false;
      
      for (int i = 0;; ++i) {
        char ch = ascii_[i];
        if (ch == EOW)
          break;
         

        if (!inside_height || !inside_width)
          skip_ws(ch, i);
        else
          ++win_x;

        switch (ch) {
          case SPACE: {
            continue;
          } break;

          case EOL: {
            ++height;
            win_x = 0;

            for (auto& elptr : open_height_elements_) {
              ++(elptr->bottom_);
            }
          } break;

          case HORIZONTAL_BORDER: {
            if (!inside_height) {
              // Window border, calculate width
              while (ascii_[i++] == HORIZONTAL_BORDER)
                ++width;
              --i; // For next iteration increment
              win_x = width;
              inside_height = true;
              break;
            } else {

              // New element. Calculate dimensions and find the father
              auto new_element = std::make_shared<Element>();
              new_element->left_ = win_x;
              new_element->top_ = height;
              new_element->bottom_ = height;

              int element_width = 0;
              while (ascii_[i] == HORIZONTAL_BORDER) {
                ++element_width;
                ++i;
                ++win_x;
              }
              --i; // Decrement for next iteration
              --win_x;
              new_element->right_ = new_element->left_ + element_width;

              // Before finding the father: detect if this is a closing element
              std::vector<decltype(open_height_elements_)::iterator> to_delete;
              for (decltype(open_height_elements_)::iterator it = open_height_elements_.begin();
                   it != open_height_elements_.end(); ++it) 
              {
                auto& elptr = *it;
                if (elptr->left_ == new_element->left_ &&
                    elptr->right_ == new_element->right_) {
                  new_element.reset();
                  ++elptr->bottom_; // Also count the border
                  to_delete.push_back(it);                  
                  break;
                }
              }
              if (to_delete.empty()) {
                // This might as well be the window's own closing border
                if (new_element->right_ - new_element->left_ == width) {
                  new_element.reset();
                  inside_height = false;
                  inside_width = false;
                  open_height_elements_.clear();
                  open_width_elements_.clear();
                  break; // Closing window
                }
              } else {
                for (auto& el : to_delete)
                  open_height_elements_.erase(el);
                // Do not delete the elements these point to - the elements live in their parents
              }              

              if (new_element == nullptr)
                break; // An element was closed and removed from open height elements, nothing more to do

              if (!open_width_elements_.empty()) {

                Element *parent_candidate = nullptr;
                for (auto elptr : open_width_elements_) {
                  if (elptr->left_ < new_element->left_ && elptr->right_ > new_element->right_) {
                    if (!parent_candidate || elptr->left_ > parent_candidate->left_)
                      parent_candidate = elptr.get(); // Better candidate
                  }
                }

                if (parent_candidate)
                  new_element->parent = parent_candidate;
                else
                  throw std::runtime_error("Could not find a parent for element");

              } else {
                new_element->parent = this;
                elements_.emplace_back(new_element);
              }

              open_height_elements_.emplace_back(new_element);
            }

          } break;

          case VERTICAL_BORDER: {

            if (!inside_width && inside_height) {
              inside_width = true;
              break; // Window border
            } else {

              if (win_x == width - 1) {
                // Reached right window border
                inside_width = false;
                open_width_elements_.clear();
                break;
              }

              // Element. Find inner opening element or closing element
              for (auto& elptr : open_height_elements_) {
                if (elptr->left_ == win_x) {
                  open_width_elements_.push_back(elptr);
                  break;
                } else if (elptr->right_ - 1 == win_x) {
                  open_width_elements_.erase(std::remove(open_width_elements_.begin(),
                    open_width_elements_.end(), elptr), open_width_elements_.end());
                  break;
                }
              }
            }

          } break;

          default: {
            // Not a border character. Grab the string.
            
            int j = i;
            std::string str;
            while (ascii_[j] != HORIZONTAL_BORDER && ascii_[j] != VERTICAL_BORDER) {
              str += ascii_[j];
              ++j;
            }

            // Check if this is a name string or a label

            auto new_label = [](int left, int top, int bottom, int right, std::string text, std::string id = std::string()) {
              // New label control here
              auto new_element = std::make_shared<Element>();
              new_element->type_ = Label;
              new_element->left_ = left;
              new_element->top_ = top;
              new_element->bottom_ = bottom;
              new_element->right_ = right;
              new_element->text_ = rtrim(text);
              if (!id.empty())
                new_element->id_ = id;
              return new_element;
            };

            // A label can have no border
            bool is_id_label = (str.find("label_") != std::string::npos);
            if (open_width_elements_.size() > 0 || is_id_label) {
              decltype(open_width_elements_)::value_type candidate = nullptr;
              for (auto elptr : open_width_elements_) {
                if (candidate == nullptr || elptr->left_ > candidate->left_)
                  candidate = elptr;
              }
              if (candidate != nullptr || is_id_label) {
                if (str.find("btn_") == 0) {
                  candidate->type_ = Button;
                  candidate->id_ = rtrim(str.substr(sizeof("btn_") - 1));
                }
                else if (str.find("editbox_") == 0) {
                  candidate->type_ = EditBox;
                  candidate->id_ = rtrim(str.substr(sizeof("editbox_") - 1));
                }
                else if (str.find("label_") == 0) {
                  elements_.emplace_back(new_label(win_x, height, height + 1, 
                                                   (int)(win_x + str.size()), std::string(), 
                                                   rtrim(str.substr(sizeof("label_") - 1))));
                }
                else {
                  // New label control here (without id)
                  candidate->sub_elements_.emplace_back(new_label(win_x, height, height + 1, (int)(win_x + str.size()), str));
                }
              }
            } else {
              // New label control here (without id)
              elements_.emplace_back(new_label(win_x, height, height + 1, (int)(win_x + str.size()), str));
            }

            i += (int)str.size() - 1;
            win_x += (int)str.size() - 1;

          } break;
        }

      }

      width_ = width;
      height_ = height;

      create_os_window(); // Proceed to OS-specific window initialization
    }

    Window& onClickButton(const std::string& id, std::function<void(Window&)> callback) {
      auto it = controls_map_.find(id);
      if (it == controls_map_.end())
        return *this;

      buttons_click_handlers_.emplace(it->second, callback);
      return *this;
    }

    Window& setText(const std::string& id, const std::string& text) {
      auto it = controls_map_.find(id);
      if (it == controls_map_.end())
        return *this;

#ifdef _WIN32
      SendMessage(it->second, WM_SETTEXT, 0, (LPARAM)text.c_str());
#else
#error "Not implemented"
#endif   
      return *this;
    }

    enum WindowPosition {
      CENTER,
      CUSTOM
    };
    Window& setWindowPosition(WindowPosition pos, int x = 0, int y = 0)
    {
#ifdef _WIN32
        if (pos == CUSTOM) {
          SetWindowPos(window_hwnd_, (HWND)-1,
            /* X */ x,
            /* Y */ y,
            /* cx */ 0,
            /* cy */ 0,
            /* uFlags */ SWP_NOSIZE | SWP_NOZORDER
          );
        }
        else if (pos == CENTER) {
          HWND hwndOwner = GetParent(window_hwnd_);
          if (hwndOwner == NULL)
            hwndOwner = GetDesktopWindow();

          RECT rcOwner, rcDlg, rc;
          GetWindowRect(hwndOwner, &rcOwner);
          GetWindowRect(window_hwnd_, &rcDlg);
          CopyRect(&rc, &rcOwner);

          OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
          OffsetRect(&rc, -rc.left, -rc.top);
          OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);

          // The new position is the sum of half the remaining space and the owner's 
          // original position. 

          SetWindowPos(window_hwnd_,
            HWND_TOP,
            rcOwner.left + (rc.right / 2),
            rcOwner.top + (rc.bottom / 2),
            0, 0, // Ignores size arguments
            SWP_NOSIZE);

        }
#else
#error "Not implemented"
#endif
      return *this;
    }

    // Apply an offset to the window
    Window& addWindowOffset(int off_x = 0, int off_y = 0,
                      int off_width = 0, int off_height = 0)
    {
#ifdef _WIN32
      RECT rect;
      GetWindowRect(window_hwnd_, &rect);

      SetWindowPos(window_hwnd_, (HWND)-1,
        /* X */ rect.left + off_x,
        /* Y */ rect.top + off_y,
        /* cx */ rect.right - rect.left + off_width,
        /* cy */ rect.bottom - rect.top + off_height,
        /* uFlags */ SWP_NOZORDER
      );
#else
#error "Not implemented"
#endif
      return *this;
    }

    Window& addOffset(const std::string& id, int off_x = 0, int off_y = 0, 
                                             int off_width = 0, int off_height = 0) 
    {
      auto it = controls_map_.find(id);
      if (it == controls_map_.end())
        return *this;

#ifdef _WIN32
      RECT rect;
      GetWindowRect(it->second, &rect);
      MapWindowPoints(HWND_DESKTOP, window_hwnd_, (LPPOINT)&rect, 2);

      SetWindowPos (it->second, (HWND)-1,
        /* X */ rect.left + off_x,
        /* Y */ rect.top + off_y,
        /* cx */ rect.right - rect.left + off_width,
        /* cy */ rect.bottom - rect.top + off_height,
        /* uFlags */ SWP_NOZORDER
      );
#else
#error "Not implemented"
#endif
      return *this;
    }

    std::string getText(const std::string& id) {
      auto it = controls_map_.find(id);
      if (it == controls_map_.end())
        return std::string();

#ifdef _WIN32
      char text[512];
      GetWindowText(it->second, text, 512);

      return text;
#else
#error "Not implemented"
#endif
    }

    void show() {

#ifdef _WIN32

      ShowWindow(window_hwnd_, SW_SHOW);
      UpdateWindow(window_hwnd_);

      MSG Msg;
      while (GetMessage(&Msg, NULL, 0, 0) > 0)
      {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
      }
#else
#error "Not implemented"
#endif
    }

    void exit() {
#ifdef _WIN32
      PostQuitMessage(0);
#else
#error "Not implemented"
#endif
    }

  private:

#ifdef _WIN32
    HWND window_hwnd_;
    std::multimap<std::string /* id */, HWND> controls_map_;
    std::multimap<HWND, std::function<void(Window&)>> buttons_click_handlers_;

    std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> wndProc;

    // The Window Procedure
    LRESULT wndProcInternal(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
      switch (message)
      {
      case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
      case WM_DESTROY:
        PostQuitMessage(0);
        break;
      case WM_CTLCOLORSTATIC: {
        SetBkMode((HDC)wParam, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      } break;
      case WM_COMMAND: {

        auto it = buttons_click_handlers_.find((HWND)lParam);
        if (it == buttons_click_handlers_.end())
          break;
        
        it->second(*this);

      } break;
      default:
        return DefWindowProc(hWnd, message, wParam, lParam);
      }
      return 0;
    }

#else
#error "Not implemented"
#endif

    void create_os_window() {
#ifdef _WIN32
      WNDCLASSEX wc; 
      wc.cbSize = sizeof(WNDCLASSEX);
      wc.style = 0;

      using namespace std::placeholders;
      wndProc = std::bind(&Window::wndProcInternal, this, _1, _2, _3, _4);
      // Convert a std::function pointing to the wndProc to a static function
      // pointer and assign it to lpfnWndProc
      std_fn_to_static_fn(wndProc, wc.lpfnWndProc);

      wc.cbClsExtra = 0;
      wc.cbWndExtra = 0;
      wc.hInstance = NULL;
      wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
      wc.hCursor = LoadCursor(NULL, IDC_ARROW);
      wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
      wc.lpszMenuName = NULL;
      wc.lpszClassName = "iupa2Class";
      wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

      if (!RegisterClassEx(&wc))
      {
        throw std::runtime_error("Window Registration Failed");
      }

      // Create the window
      auto dwStyle = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU);
      window_hwnd_ = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "iupa2Class",
        title_.c_str(),
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        chars_to_pixels(X, width_), chars_to_pixels(Y, height_),
        NULL, NULL, NULL, NULL);

      if (window_hwnd_ == NULL)
      {
        throw std::runtime_error("Window Creation Failed");
      }

      // Get default font. Assumes Vista or higher
      HFONT default_font_handle;
      NONCLIENTMETRICS ncm;
      ncm.cbSize = sizeof(ncm);
      SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
      default_font_handle = CreateFontIndirect(&(ncm.lfMessageFont));

      SendMessage(window_hwnd_, WM_SETFONT, (WPARAM)default_font_handle, MAKELPARAM(FALSE, 0));

      // Create the controls
      {
        for (auto element : elements_) {

          switch (element->type_) {
            case Label: {

              HWND e_hwnd = CreateWindow("static", element->text_.c_str(), WS_CHILD | WS_VISIBLE,
                chars_to_pixels(X, element->left_), chars_to_pixels(Y, element->top_) - chars_to_pixels(Y, element->bottom_ - element->top_),
                chars_to_pixels(X, element->right_ - element->left_), chars_to_pixels(Y, element->bottom_ - element->top_),
                window_hwnd_, NULL, (HINSTANCE)GetModuleHandle(NULL), NULL);

              SendMessage(e_hwnd, WM_SETFONT, (WPARAM)default_font_handle, MAKELPARAM(FALSE, 0));

              controls_map_.emplace(element->id_, e_hwnd);

            } break;
            case EditBox: {

              HWND e_hwnd = CreateWindow("edit", element->text_.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER,
                chars_to_pixels(X, element->left_), chars_to_pixels(Y, element->top_),
                chars_to_pixels(X, element->right_ - element->left_), ncm.iCaptionHeight,
                window_hwnd_, NULL, (HINSTANCE)GetModuleHandle(NULL), NULL);

              SendMessage(e_hwnd, WM_SETFONT, (WPARAM)default_font_handle, MAKELPARAM(FALSE, 0));

              controls_map_.emplace(element->id_, e_hwnd);

            } break;
            case Button: {

              HWND e_hwnd = CreateWindow("button", element->id_.c_str(), WS_CHILD | WS_VISIBLE,
                chars_to_pixels(X, element->left_), chars_to_pixels(Y, element->top_),
                chars_to_pixels(X, element->right_ - element->left_), chars_to_pixels(Y, element->bottom_ - element->top_ - 2),
                window_hwnd_, NULL, (HINSTANCE)GetModuleHandle(NULL), NULL);

              SendMessage(e_hwnd, WM_SETFONT, (WPARAM)default_font_handle, MAKELPARAM(FALSE, 0));

              controls_map_.emplace(element->id_, e_hwnd);

            } break;
          }

        }
      }
#else
#error "Not implemented yet"
#endif
    }

    enum Orientation {X, Y};
    inline int chars_to_pixels(Orientation o, int value) {
      // Empirical calculations ~ 10px per point horizontal
      //                          25px per point vertical
      return (o == X) ? (value * 10) : (value * 25);
    }

    const char *ascii_;
    int width_ = 0, height_ = 0;
    std::string title_ = "iupa2";
    std::vector<std::shared_ptr<Element>> elements_;
    std::vector<std::shared_ptr<Element>> open_height_elements_;
    std::vector<std::shared_ptr<Element>> open_width_elements_;
  };


}
