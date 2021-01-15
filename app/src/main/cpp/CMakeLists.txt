cmake_minimum_required(VERSION 3.4.1)

#设置so库路径
set(my_lib_path ${CMAKE_SOURCE_DIR}/../jniLibs)
#设置include目录
include_directories(${my_lib_path}/include)

#将第三方库作为动态库引用
add_library(avcodec-56
        SHARED
        IMPORTED)

#指定第三方库的绝对路径
set_target_properties(avcodec-56
        PROPERTIES IMPORTED_LOCATION
        ${my_lib_path}/${ANDROID_ABI}/libavcodec-56.so)

#将第三方库作为动态库引用
add_library(avdevice-56
        SHARED
        IMPORTED)

#指定第三方库的绝对路径
set_target_properties(avdevice-56
        PROPERTIES IMPORTED_LOCATION
        ${my_lib_path}/${ANDROID_ABI}/libavdevice-56.so)

#将第三方库作为动态库引用
add_library(avfilter-5
        SHARED
        IMPORTED)

#指定第三方库的绝对路径
set_target_properties(avfilter-5
        PROPERTIES IMPORTED_LOCATION
        ${my_lib_path}/${ANDROID_ABI}/libavfilter-5.so)

#将第三方库作为动态库引用
add_library(avformat-56
        SHARED
        IMPORTED)

#指定第三方库的绝对路径
set_target_properties(avformat-56
        PROPERTIES IMPORTED_LOCATION
        ${my_lib_path}/${ANDROID_ABI}/libavformat-56.so)

#将第三方库作为动态库引用
add_library(avutil-54
        SHARED
        IMPORTED)

#指定第三方库的绝对路径
set_target_properties(avutil-54
        PROPERTIES IMPORTED_LOCATION
        ${my_lib_path}/${ANDROID_ABI}/libavutil-54.so)

#将第三方库作为动态库引用
add_library(postproc-53
        SHARED
        IMPORTED)

#指定第三方库的绝对路径
set_target_properties(postproc-53
        PROPERTIES IMPORTED_LOCATION
        ${my_lib_path}/${ANDROID_ABI}/libpostproc-53.so)

#将第三方库作为动态库引用
add_library(swresample-1
        SHARED
        IMPORTED)

#指定第三方库的绝对路径
set_target_properties(swresample-1
        PROPERTIES IMPORTED_LOCATION
        ${my_lib_path}/${ANDROID_ABI}/libswresample-1.so)

#将第三方库作为动态库引用
add_library(swscale-3
        SHARED
        IMPORTED)

#指定第三方库的绝对路径
set_target_properties(swscale-3
        PROPERTIES IMPORTED_LOCATION
        ${my_lib_path}/${ANDROID_ABI}/libswscale-3.so)


add_library(
        native-lib

        SHARED

        native-lib.cpp
        _opensl_helper.cpp)

find_library(
        log-lib

        log)

target_link_libraries(
        native-lib
        avcodec-56
        avdevice-56
        avfilter-5
        avformat-56
        avutil-54
        postproc-53
        swresample-1
        swscale-3

        android
        OpenSLES
        ${log-lib})