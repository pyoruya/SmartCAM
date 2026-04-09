# SmartCAMVCam: IMFMediaSource COM component for MFVirtualCamera
add_library(SmartCAMVCam SHARED
    src/media_source.cpp
    src/media_source.h
    src/class_factory.cpp
    src/class_factory.h
    src/vcam_guids.h
)

target_link_libraries(SmartCAMVCam PRIVATE
    mf
    mfplat
    mfuuid
    mfsensorgroup
    ole32
    uuid
)

# Generate .def file for COM exports
target_sources(SmartCAMVCam PRIVATE src/SmartCAMVCam.def)
