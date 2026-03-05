# xuji103 - 控制台应用程序

set(APP_SOURCES
    main.cpp
    Config.cpp
    DataPrinter.cpp
)

set(APP_HEADERS
    Config.h
    DataPrinter.h
)

add_executable(xuji103 ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(xuji103
    PRIVATE
        xuji_iec103
        Qt5::Core
)

target_compile_features(xuji103 PUBLIC cxx_std_14)

# 安装
install(TARGETS xuji103 RUNTIME DESTINATION bin)
