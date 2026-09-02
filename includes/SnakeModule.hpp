/**
 * @file SnakeModule.hpp
 * @brief Le module que la borne trouve dans la dll.
 */

#ifndef SNAKEMODULE_HPP_
#define SNAKEMODULE_HPP_

#include "IAppModule.hpp"
#include "SnakeApp.hpp"

class SnakeModule : public IAppModule {

    public:
        const char *type() const override { return IAppModule::contract; }
        const char *name() const override { return "snake"; }

        IApp *createApp(IModuleRegistry &modules) override {
            return new SnakeApp(modules, ASSETS_DIR);
        }

        /* La dll qui a alloue est celle qui libere : deux allocateurs
         * differents de part et d'autre d'un dlopen, et delete depuis
         * l'hote irait chercher le mauvais. */
        void deleteApp(IApp *app) override { delete app; }
};

#endif /* !SNAKEMODULE_HPP_ */
