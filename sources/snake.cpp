/**
 * @file snake.cpp
 * @brief Le seul symbole que la borne cherche.
 */

#include "SnakeModule.hpp"

/**
 * @brief Tout ce que cette dll fournit.
 *
 * IModule ** termine par nullptr, et pas un std::vector : un vecteur ne
 * traverse pas un dlopen sans supposer la meme ABI de bibliotheque standard
 * des deux cotes. La conversion vers IModule * se fait ICI, ou le type
 * complet est connu.
 */
extern "C" IModule **getModules() {
    static SnakeModule snake;
    static IModule *list[] = { &snake, nullptr };

    return list;
}
