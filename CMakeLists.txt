add_library(pico_hid_ups INTERFACE)

target_sources(pico_hid_ups INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/pico_hid_ups.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.cpp)

target_include_directories(pico_hid_ups INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(pico_hid_ups INTERFACE
    pico_stdlib
    pico_unique_id
    tinyusb_device
    tinyusb_board)
