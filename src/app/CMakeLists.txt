# Konflikt Linux CLI Application

add_executable(konflikt-app
    main.cpp
)

target_link_libraries(konflikt-app
    PRIVATE
        konflikt
)

# Set output name to just "konflikt"
set_target_properties(konflikt-app PROPERTIES
    OUTPUT_NAME konflikt
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS konflikt-app
    RUNTIME DESTINATION bin
)
