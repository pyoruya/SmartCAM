# SmartCAMVCam: IMFMediaSource COM component for MFVirtualCamera
add_library(SmartCAMVCam SHARED
    src/media_source.cpp
    src/media_source.h
    src/class_factory.cpp
    src/class_factory.h
    src/vcam_guids.h
    src/dllmain.cpp
    src/SmartCAMVCam.def
)

target_include_directories(SmartCAMVCam PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/shared/src
)

target_link_libraries(SmartCAMVCam PRIVATE
    mf
    mfplat
    mfuuid
    ole32
    uuid
)
