# select the 3D backend for sokol_gfx.h and sokol_app.h
# the sokol_config() macro is in the top-level CMakeLists.txt file!
sokol_config()

add_subdirectory(dbgui)
add_subdirectory(cdbgui)
add_subdirectory(microui)
add_subdirectory(sokol)
add_subdirectory(stb)
add_subdirectory(basisu)
