# Add standard specific include directory
include_directories(${CMAKE_SOURCE_DIR}/include/ieee1516-2010)
include_directories(${CMAKE_BINARY_DIR}/include/ieee1516-2010)
# Add libhla for hash function
include_directories(${CMAKE_SOURCE_DIR})

##########################
# RTI1516e
##########################

set(RTI1516e_LIB_SRCS
    RTIambassadorFactory.cpp
    Exception.cpp
    LogicalTime.cpp
    HLAfloat64TimeImplementation.cpp
    HLAfloat64TimeImplementation.h
    RTIvariableLengthData.cpp
    RTIvariableLengthDataImplementation.h
    RTIambassador.cpp
    RTIambassadorImplementation.cpp
    RTIambassadorImplementation.h
    RTIambPrivateRefs.cpp
    RTIambPrivateRefs.h
    Handle.cpp
    HandleImplementation.cpp
    HandleImplementation.h
    RTIfedAmbassador.cpp
    RTIHandleFactory.cpp
    RTIHandleFactory.h
    Typedefs.cpp
   )

set(RTI1516e_LIB_INCLUDE
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/Enums.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/Exception.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/encoding/EncodingExceptions.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/FederateAmbassador.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/Handle.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/LogicalTime.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/LogicalTimeFactory.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/LogicalTimeInterval.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/time/HLAfloat64Time.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/time/HLAfloat64TimeFactory.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/time/HLAfloat64Interval.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/NullFederateAmbassador.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/RangeBounds.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/RTI1516.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/RTIambassador.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/RTIambassadorFactory.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/SpecificConfig.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/Typedefs.h
    ${CMAKE_SOURCE_DIR}/include/ieee1516-2010/RTI/VariableLengthData.h
)

add_library(RTI1516eLib STATIC ${RTI1516e_LIB_SRCS} ${RTI1516e_LIB_INCLUDE})
target_link_libraries(RTI1516eLib)
set_target_properties(RTI1516eLib PROPERTIES COMPILE_FLAGS "-DRTI_DISABLE_WARNINGS -DSTATIC_RTI")

add_library(RTI1516e ${RTI1516e_LIB_SRCS} ${RTI1516e_LIB_INCLUDE})
target_link_libraries(RTI1516e CERTI HLA FedTime1516eLib)
install(FILES RTI1516fedTime.h DESTINATION include/ieee1516-2010/RTI)
message(STATUS "libRTI variant: HLA 1516e")
set_target_properties(RTI1516e PROPERTIES OUTPUT_NAME "RTI1516e")
set_target_properties(RTI1516e PROPERTIES COMPILE_FLAGS "-DHLA13NG_LIBRTI -DRTI_DISABLE_WARNINGS -DBUILDING_RTI")
set_target_properties(RTI1516e PROPERTIES VERSION 1.0.0 SOVERSION 1)
if (MINGW)
    set_target_properties(RTI1516e PROPERTIES LINK_FLAGS "-Wl,--output-def,${LIBRARY_OUTPUT_PATH}/RTI1516eLib.def")
    install(FILES ${LIBRARY_OUTPUT_PATH}/RTI1516eLib.def DESTINATION lib)
endif (MINGW)


##########################################################
set(FEDTIME1516_SRCS
    LogicalTimeFactoryFactory.cpp
   )

add_library(FedTime1516eLib STATIC ${FEDTIME1516_SRCS})
target_link_libraries(FedTime1516eLib)
set_target_properties(FedTime1516eLib PROPERTIES COMPILE_FLAGS "-DRTI_DISABLE_WARNINGS -DSTATIC_RTI")

add_library(FedTime1516e ${FEDTIME1516_SRCS})
target_link_libraries(FedTime1516e RTI1516eLib)
set_target_properties(FedTime1516e PROPERTIES COMPILE_FLAGS "-DBUILDING_FEDTIME -DRTI_DISABLE_WARNINGS -DSTATIC_RTI")
set_target_properties(FedTime1516e PROPERTIES VERSION 1.0.0 SOVERSION 1)
if(MINGW)
    set_target_properties(FedTime1516e PROPERTIES LINK_FLAGS "-Wl,--output-def,${LIBRARY_OUTPUT_PATH}/FedTime1516eLib.def")
    install(FILES ${LIBRARY_OUTPUT_PATH}/FedTime1516eLib.def
            DESTINATION lib)
endif(MINGW)

if(COMPILE_WITH_CXX11)
    if (CMAKE_VERSION VERSION_LESS 3.1.3)
        SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11")
    else()
        set_property(TARGET RTI1516e FedTime1516e PROPERTY CXX_STANDARD 11)
    endif()
endif()

#set(TestFedTime1516e_SRCS
#  TestFedTime.cc
#)

#ADD_EXECUTABLE(TestFedTime1516 ${TestFedTime1516_SRCS})

# TODO: Should link with RTI1516 lib, instead of base RTI lib
#target_link_libraries(TestFedTime1516 FedTime1516 RTI1516)

#ADD_TEST(TestLibFedTime1516 COMMAND $<TARGET_FILE:TestFedTime1516>)


# Install rules for both libRTI156 and libFedTime1516
install(TARGETS RTI1516eLib FedTime1516eLib RTI1516e FedTime1516e
        EXPORT CERTIDepends
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# mimic install directory structure in the binary tree in order to ease the linking 
# of builtin test (i.e. testFederate)
#configure_file(${CMAKE_CURRENT_SOURCE_DIR}/RTI1516fedTime.h ${CMAKE_BINARY_DIR}/include/ieee1516-2010/RTI/RTI1516fedTime.h COPYONLY)
#install(FILES RTI1516fedTime.h DESTINATION include/ieee1516-2010/RTI)
