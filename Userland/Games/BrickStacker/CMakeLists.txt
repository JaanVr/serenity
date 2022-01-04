serenity_component(
        BrickStacker
        RECOMMENDED
        TARGETS BrickStacker
)

set(SOURCES
        main.cpp
        Game.cpp
        )

serenity_app(BrickStacker ICON app-brickstacker)
target_link_libraries(BrickStacker LibGUI LibMain)
