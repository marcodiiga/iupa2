#include <iostream>
#include <string>

#include "iupa2.hpp"


int main()
{
  std::string user_name;

  iupa2::Window(

    "   -----------------------------------------------  \n"
    "   |                     --------------------    |  \n"
    "   |  Enter your name    |editbox_edit1     |    |  \n"
    "   |                     --------------------    |  \n"
    "   |        -----------        ------------      |  \n"
    "   |        |  btn_ok |        |btn_cancel|      |  \n"
    "   |        -----------        ------------      |  \n"
    "   -----------------------------------------------  \n"

  ).onClickButton("ok", [&user_name](iupa2::Window& win) {

    user_name = win.getText("edit1");

    std::cout << "Hi " << user_name << std::endl;

  }).onClickButton("cancel", [](iupa2::Window& win) {

    win.exit();

  }).setWindowPosition(iupa2::Window::CENTER).show();

  std::cout << "Hi " << user_name << std::endl;

  return 0;
}

