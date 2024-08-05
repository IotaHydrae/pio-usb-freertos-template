add_executable(${ProjectName}
    main.c
)

target_link_options(${ProjectName} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${ProjectName} PRIVATE -Wall -Wextra)

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${ProjectName} 
    pico_stdlib
    pico_multicore
    FreeRTOS-Kernel-Heap4
    pico_pio_usb
    )

pico_enable_stdio_usb(${ProjectName} 1)
pico_enable_stdio_uart(${ProjectName} 1)

pico_add_extra_outputs(${ProjectName})
