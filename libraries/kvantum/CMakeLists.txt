find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Svg)
add_library(Luna_kvantum STATIC
    style/Kvantum.cpp style/Kvantum.h style/eventFiltering.cpp style/polishing.cpp
    style/rendering.cpp style/standardIcons.cpp style/viewItems.cpp
    style/shortcuthandler.cpp style/shortcuthandler.h
    style/animation/animation.cpp style/animation/animation.h
    style/blur/blurhelper.cpp style/blur/blurhelper.h
    style/drag/windowmanager.cpp style/drag/windowmanager.h
    style/themeconfig/ThemeConfig.cpp style/themeconfig/defaulttheme.qrc)
target_include_directories(Luna_kvantum PUBLIC style)
target_compile_definitions(Luna_kvantum PUBLIC NO_KF NO_X11 PRIVATE DATADIR="")
target_link_libraries(Luna_kvantum PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg)
install(FILES COPYING README.luna.md DESTINATION share/licenses/${Launcher_Name}/kvantum)
