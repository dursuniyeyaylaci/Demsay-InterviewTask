cmake_minimum_required(VERSION 3.16)

project(uart_led_control C)

add_library(led_app_core
  Core/Src/led_app.c
  Core/Src/ring_buffer.c
)

target_include_directories(led_app_core PUBLIC Core/Inc)
target_compile_features(led_app_core PUBLIC c_std_11)

add_executable(led_app_tests tests/test_led_app.c)
target_link_libraries(led_app_tests PRIVATE led_app_core)

if(MSVC)
  target_compile_options(led_app_core PRIVATE /W4 /WX)
  target_compile_options(led_app_tests PRIVATE /W4 /WX)
else()
  target_compile_options(led_app_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_compile_options(led_app_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME led_app_tests COMMAND led_app_tests)
