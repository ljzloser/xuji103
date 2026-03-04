# libiec103 - 南网以太网103协议库

set(IEC103_SOURCES
    src/apci/FrameCodec.cpp
    src/apci/SeqManager.cpp
    src/asdu/ASDU.cpp
    src/asdu/DigitalParser.cpp
    src/asdu/GIParser.cpp
    src/asdu/DataParser.cpp
    src/transport/TcpTransport.cpp
    src/transport/Reconnector.cpp
    src/master/IEC103Master.cpp
    src/utils/Logger.cpp
)

set(IEC103_HEADERS
    include/iec103/IEC103Master.h
    include/iec103/types/Types.h
    include/iec103/types/Constants.h
    include/iec103/types/TimeStamp.h
    include/iec103/types/DataPoint.h
    include/iec103/apci/APCI.h
    include/iec103/apci/Frame.h
    include/iec103/apci/SeqManager.h
    include/iec103/asdu/ASDU.h
    include/iec103/asdu/ASDU1.h
    include/iec103/asdu/ASDU7_8.h
    include/iec103/asdu/ASDU10_21.h
    include/iec103/asdu/ASDU42.h
    include/iec103/callback/IDataHandler.h
    include/iec103/transport/TcpTransport.h
)

add_library(iec103 STATIC ${IEC103_SOURCES} ${IEC103_HEADERS})

target_include_directories(iec103
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(iec103
    PUBLIC
        Qt5::Core
        Qt5::Network
)

target_compile_features(iec103 PUBLIC cxx_std_14)
