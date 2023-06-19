project(sdk_samples)
cmake_minimum_required(VERSION 3.10)

find_package(OpenCV REQUIRED)
find_package(Iconv)

add_executable(sdk_sample_raw_frames
    sdk_sample_raw_frames.cpp
        )

target_include_directories(sdk_sample_raw_frames
    PRIVATE
    /usr/local/Meerkat/include
)

target_link_libraries(sdk_sample_raw_frames
    PRIVATE
    /usr/local/Meerkat/lib/libsightx_sdk.so

    ${OpenCV_LIBRARIES}
)

add_executable(sdk_sample_single_frame
    sdk_sample_single_frame.cpp
        )

target_include_directories(sdk_sample_single_frame
    PRIVATE
    /usr/local/Meerkat/include
)

target_link_libraries(sdk_sample_single_frame
    PRIVATE
    /usr/local/Meerkat/lib/libsightx_sdk.so

    ${OpenCV_LIBRARIES}
)



