cmake_minimum_required(VERSION 3.20)
project(MultistreamChat VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets Svg)
find_package(nlohmann_json REQUIRED)
find_package(ZLIB REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platforms
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

set(SOURCES
    src/main.cpp
    
    # Core
    src/core/Logger.cpp
    src/core/ConfigManager.cpp
    src/core/DonationManager.cpp
    src/core/ChatManager.cpp
    src/core/EventManager.cpp
    src/core/ConnectionManager.cpp
    
    # Network
    src/network/HttpHelper.cpp
    src/network/WebSocketWrapper.cpp
    src/network/LocalWebServer.cpp
    
    # Platforms
    src/platforms/BasePlatform.cpp
    src/platforms/TwitchPlatform.cpp
    src/platforms/KickPlatform.cpp
    src/platforms/YouTubePlatform.cpp
    src/platforms/TikTokPlatform.cpp
    
    # UI
    src/ui/Sidebar.cpp
    src/ui/ChatWidget.cpp
    src/ui/ChannelManagerWidget.cpp
    src/ui/WebServerWidget.cpp
    src/ui/DonationsWidget.cpp
    src/ui/StatisticsWidget.cpp
    src/ui/StyleSettingsWidget.cpp
    src/ui/GeneralSettingsWidget.cpp
    src/ui/LogViewerWidget.cpp
    src/ui/MainWindow.cpp
    
    # Resources
    resources/resources.qrc
)

set(HEADERS
    src/models/ChatMessage.h
    src/models/Donation.h
    src/models/ChannelConfig.h
    src/models/PlatformStatus.h
    src/models/AppStyleConfig.h
    
    src/core/Logger.h
    src/core/ConfigManager.h
    src/core/DonationManager.h
    src/core/ChatManager.h
    src/core/EventManager.h
    src/core/ConnectionManager.h
    
    src/network/HttpHelper.h
    src/network/WebSocketWrapper.h
    src/network/LocalWebServer.h
    
    src/platforms/IStreamingPlatform.h
    src/platforms/BasePlatform.h
    src/platforms/TwitchPlatform.h
    src/platforms/KickPlatform.h
    src/platforms/YouTubePlatform.h
    src/platforms/TikTokPlatform.h
    
    src/ui/Sidebar.h
    src/ui/ChatWidget.h
    src/ui/ChannelManagerWidget.h
    src/ui/WebServerWidget.h
    src/ui/DonationsWidget.h
    src/ui/StatisticsWidget.h
    src/ui/StyleSettingsWidget.h
    src/ui/GeneralSettingsWidget.h
    src/ui/LogViewerWidget.h
    src/ui/MainWindow.h
)

# WIN32 prevents a console window from being created beside the GUI app.
add_executable(MultistreamChat WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(MultistreamChat PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::Svg
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
)

# Optional test console executable (only built if tests/test_platforms.cpp exists)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_platforms.cpp")
    add_executable(TestPlatforms
        tests/test_platforms.cpp
        src/core/Logger.cpp
        src/core/ConfigManager.cpp
        src/core/DonationManager.cpp
        src/core/ChatManager.cpp
        src/core/EventManager.cpp
        src/core/ConnectionManager.cpp
        src/network/HttpHelper.cpp
        src/network/WebSocketWrapper.cpp
        src/platforms/BasePlatform.cpp
        src/platforms/TwitchPlatform.cpp
        src/platforms/KickPlatform.cpp
        src/platforms/YouTubePlatform.cpp
        src/platforms/TikTokPlatform.cpp
        resources/resources.qrc
    )

    target_link_libraries(TestPlatforms PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::WebSockets
        Qt6::Svg
        ZLIB::ZLIB
        nlohmann_json::nlohmann_json
    )
endif()
