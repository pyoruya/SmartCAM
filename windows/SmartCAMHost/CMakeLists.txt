add_executable(SmartCAMHost WIN32
    src/main.cpp
    src/tray.cpp
    src/tray.h
    src/adb_manager.cpp
    src/adb_manager.h
    src/stream_receiver.cpp
    src/stream_receiver.h
)

target_link_libraries(SmartCAMHost PRIVATE
    mf
    mfplat
    mfuuid
    mfreadwrite
    ole32
    shell32
    user32
)

# Copy adb.exe to output directory if present
if(EXISTS "${CMAKE_SOURCE_DIR}/../third_party/adb/adb.exe")
    add_custom_command(TARGET SmartCAMHost POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../third_party/adb/adb.exe"
            "$<TARGET_FILE_DIR:SmartCAMHost>/adb.exe"
    )
endif()
