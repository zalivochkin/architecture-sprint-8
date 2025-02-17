cmake_minimum_required(VERSION 3.11)

project(api_service CXX)
set(CMAKE_CXX_STANDARD 20)

# boost.beast будет использовать std::string_view вместо boost::string_view
add_definitions( -DBOOST_BEAST_USE_STD_STRING_VIEW )

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

find_package(Boost COMPONENTS program_options REQUIRED)
find_package(Boost COMPONENTS thread REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)

add_executable(api_service src/api_service.cpp)

target_link_libraries(api_service Boost::boost Boost::program_options Boost::thread jwt-cpp::jwt-cpp curl)
