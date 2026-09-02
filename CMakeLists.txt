#----------- general -----------

cmake_minimum_required(VERSION 3.24)
project(snake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#----------- project -----------

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(CMakeUtils REQUIRED)

# Un jeu ne linke AUCUN vendor : il ne connait que les contrats et trouve
# une bibliotheque graphique au runtime, par le registre que la borne lui
# passe. C'est ce qui lui permet de tourner derriere sfml comme derriere
# raylib sans etre recompile.
find_package(System REQUIRED)
find_package(imodule REQUIRED)
find_package(icore REQUIRED)
find_package(igraphic REQUIRED)

# Host suit DEUX contrats, image et son : il faut donc les deux en-tetes,
# meme si snake ne joue encore rien.
find_package(iaudio REQUIRED)

# SHARED, et pas gouverne par BUILD_SHARED_LIBS : la raison d'etre d'un jeu
# d'arcade est d'etre charge a chaud. Si un drapeau global le transformait
# en .a, la borne ne le verrait simplement jamais.
add_library(${PROJECT_NAME} SHARED sources/snake.cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC icore igraphic iaudio)

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/includes
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# snake.dylib, pas libsnake.dylib : c'est le nom de fichier qui devient la
# cle de chargement dans la borne.
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
