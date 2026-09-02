/**
 * @file Host.hpp
 * @brief La bibliotheque en service, et tout ce qu'elle a fabrique.
 *
 * MEMBRE, jamais classe de base. C'est la seule difference avec la classe de base qu il remplace,
 * et c est ce qui supprime le piege : un destructeur de base ne peut pas atteindre
 * un virtuel de la classe derivee, donc le menage n'avait jamais lieu et
 * la musique continuait apres l'arret du jeu. Un membre, lui, est detruit
 * par le compilateur, et ~Host peut appeler son propre code puisqu'a cet
 * instant Host EST le type le plus derive de lui-meme.
 *
 * IL POSSEDE CE QU'IL FABRIQUE. Le jeu ne detruit rien : il demande, il
 * garde des pointeurs, et il les redemande quand la generation change.
 * C'est ce qui fait disparaitre la derniere regle d'ordre - "detruis les
 * tiens avant que je lache le module" - qui etait une regle qu'on pouvait
 * oublier.
 *
 * Deux contrats suivis pour soi : image et son. Le jeu peut donc etre
 * sonore et aveugle, ou visible et muet, et l'hote peut prendre sa fenetre
 * chez sfml et ses sons chez raylib.
 */

#ifndef HOST_HPP_
#define HOST_HPP_

#include "Cadence.hpp"

#include "IAudioModule.hpp"
#include "IGraphic2Module.hpp"
#include "IModuleRegistry.hpp"

#include <string>
#include <vector>

class Host {

    public:
        Host(IModuleRegistry &modules, std::string assets, double stepsPerSecond)
            : _modules(modules), _assets(std::move(assets)), _cadence(stepsPerSecond) {}

        /* Le compilateur s'en charge, et rien n'est virtuel : ce destructeur
         * fait vraiment le menage, la ou celui de la classe de base qu il remplace ne le pouvait
         * pas. */
        ~Host() { drop(); dropAudio(); }

        Host(const Host &) = delete;
        Host &operator=(const Host &) = delete;

        /* ---- suivre l'hote -------------------------------------------- */

        /**
         * @brief A appeler en tete de chaque event() et update().
         *
         * Suit les DEUX contrats, chacun pour soi : une bascule audio se
         * voit meme quand l'image n'a pas bouge.
         */
        void follow(const char *title, bool cursor) {
            followAudio();

            IModule *current = _modules.Current(IGraphic2Module::contract);

            if (_module && !_module->mustClose() && _module == current)
                return;

            /* Deja refuse par celui-la : on ne retente pas soixante fois par
             * seconde, mais pas jamais non plus. */
            if (!_module && current == _refused && ++_patience < RETRY)
                return;
            if (!_module && current == _refused)
                _patience = 0;

            drop();
            if (!current)
                return;
            if (!take(current, title, cursor))
                _refused = current;
        }

        /* ---- ce que le jeu lit ---------------------------------------- */

        graphic::IWindow2 *window() const { return _window; }
        graphic::IKeyboard *keyboard() const { return _keyboard; }
        graphic::IMouse *mouse() const { return _mouse; }
        graphic::IText *text() const { return _text; }
        graphic::IFont *font() const { return _font; }
        const std::string &assets() const { return _assets; }
        Cadence &cadence() const { return _cadence; }

        bool visible() const { return _window != nullptr; }
        bool audible() const { return _audio != nullptr; }

        /**
         * @brief Le joueur a-t-il demande la fermeture de SA fenetre ?
         *
         * isOpen() est le seul signal commun aux quatre vendors : sfml
         * detruit la fenetre sur-le-champ, les deux sdl la marquent et la
         * laissent a l'ecran. Lire l'etat efface la difference.
         */
        bool closing() const { return _window && !_window->isOpen(); }

        /**
         * @brief Change a chaque bascule de vendor.
         *
         * LE SEUL SIGNAL dont le jeu a besoin. Il compare, et refabrique
         * s'il a change - sans savoir ce qui s'est passe ni quand.
         */
        unsigned generation() const { return _generation; }

        /* ---- ce que le jeu demande ------------------------------------ *
         *
         * Chaque fabrique retient ce qu'elle rend. Le jeu garde le pointeur
         * pour dessiner, mais ne le detruit jamais : c'est Host qui le fera,
         * dans le bon ordre et pendant que le contexte du vendor vit encore. */

        graphic::ITexture *texture(const std::string &file) {
            if (!_graphic)
                return nullptr;

            graphic::ITexture *made = _graphic->createTexture(_assets + "/" + file);

            if (made)
                _textures.push_back(made);
            return made;
        }

        graphic::ISprite *sprite(graphic::ITexture *from) {
            if (!_graphic || !from)
                return nullptr;

            graphic::ISprite *made = _graphic->createSprite(from);

            if (made)
                _sprites.push_back(made);
            return made;
        }

        graphic::IPolygon *polygon(const std::vector<Vector2f> &points) {
            if (!_graphic)
                return nullptr;

            graphic::IPolygon *made = _graphic->createPolygon(points);

            if (made)
                _polygons.push_back(made);
            return made;
        }

        audio::ISoundBuffer *buffer(const std::string &file) {
            if (!_audio)
                return nullptr;

            audio::ISoundBuffer *made = _audio->createSoundBuffer(_assets + "/" + file);

            if (made)
                _buffers.push_back(made);
            return made;
        }

        audio::ISound *sound(audio::ISoundBuffer *from) {
            if (!_audio || !from)
                return nullptr;

            audio::ISound *made = _audio->createSound(from);

            if (made)
                _sounds.push_back(made);
            return made;
        }

        /**
         * @brief Rend tout ce que le jeu a fabrique, garde la bibliotheque.
         *
         * Pour un jeu dont les formes dependent de la taille de fenetre :
         * snake refait ses disques quand la case change, et sans ceci les
         * anciens s'empileraient jusqu'a la prochaine bascule de vendor.
         *
         * La generation avance, donc meme un jeu qui oublierait de
         * refabriquer tout de suite le fera au tick suivant.
         */
        void discard() {
            if (_graphic) {
                for (graphic::ISprite *made : _sprites)   _graphic->deleteSprite(made);
                for (graphic::IPolygon *made : _polygons) _graphic->deletePolygon(made);
                for (graphic::ITexture *made : _textures) _graphic->deleteTexture(made);
            }
            _sprites.clear();
            _polygons.clear();
            _textures.clear();
            _generation++;
        }

        audio::IMusic *music(const std::string &file) {
            if (!_audio)
                return nullptr;

            audio::IMusic *made = _audio->createMusic(_assets + "/" + file);

            if (made)
                _musics.push_back(made);
            return made;
        }

    private:
        static constexpr int32_t WIDTH = 700;
        static constexpr int32_t HEIGHT = 480;
        static constexpr int RETRY = 60;   ///< ticks avant de retenter un refus

        /* ---- l'image --------------------------------------------------- */

        /**
         * @brief Prend ce module et ouvre SA fenetre.
         *
         * Le jeu ouvre la sienne, il n'emprunte pas celle de l'hote : deux
         * choses qui dessinent dans une meme fenetre se superposent. Si le
         * vendor n'en donne pas de seconde - raylib garde son etat dans une
         * globale unique - le jeu ne peut pas tourner derriere lui.
         *
         * @return false si le vendor refuse
         */
        bool take(IModule *module, const char *title, bool cursor) {
            /* Le membre n'est pose qu'une fois la garde passee. L'ecrire
             * avant et refuser ensuite laissait un pointeur sur un module
             * condamne, que plus rien ne nettoyait : Reconcile() fermait la
             * dll au meme tick, et le follow() suivant lisait dedans. */
            if (module->mustClose())
                return false;

            _module = module;
            _module->acquire();   //detenteur AVANT toute demande
            _graphic = static_cast<IGraphic2Module *>(_module);

            _window = _graphic->createWindow(WIDTH, HEIGHT, title);
            if (!_window) {
                drop();
                return false;
            }
            _window->setFrameLimit(60);
            _window->setMouseVisibility(cursor);

            _font = _graphic->createFont(_assets + "/font.ttf");
            _text = _graphic->createText("", _font);
            _keyboard = _graphic->createKeyboard(_window);
            _mouse = _graphic->createMouse(_window);

            /* PRENDRE PERIME AUSSI, pas seulement rendre.
             *
             * Ne l'incrementer qu'au drop() donnait un ecran noir a chaque
             * bascule : le jeu refabriquait pendant le trou - donc sur du
             * vide, tout a nullptr - notait cette generation-la comme faite,
             * et ne voyait plus jamais passer l'arrivee du nouveau vendor.
             *
             * Les deux bouts du remplacement periment ce que le jeu tient :
             * l'ancien parce qu'il est detruit, le nouveau parce qu'il est
             * autre. */
            _generation++;

            /* Ouvrir une fenetre et charger des textures prend parfois une
             * demi-seconde : ce n'est pas du temps de jeu. */
            _cadence.resync();
            return true;
        }

        /** @brief Rend tout au vendor, dans l'ordre inverse, puis le relache. */
        void drop() {
            if (!_module)
                return;

            /* LA GENERATION CHANGE DES QU'ON DETRUIT, pas seulement quand on
             * reprend. Sans ca, debrancher un vendor sans le remplacer
             * laissait le jeu sur des pointeurs morts : il ne voyait aucune
             * raison de refabriquer, et lisait dans ce qu'on venait de
             * rendre. */
            _generation++;
            if (!_graphic) {
                _module->release();
                _module = nullptr;
                return;
            }

            /* Les sprites AVANT les textures, les deux avant la fenetre :
             * ils sont batis dessus. */
            for (graphic::ISprite *made : _sprites)   _graphic->deleteSprite(made);
            for (graphic::IPolygon *made : _polygons) _graphic->deletePolygon(made);
            for (graphic::ITexture *made : _textures) _graphic->deleteTexture(made);
            _sprites.clear();
            _polygons.clear();
            _textures.clear();

            if (_mouse)    _graphic->deleteMouse(_mouse);
            if (_keyboard) _graphic->deleteKeyboard(_keyboard);
            if (_text)     _graphic->deleteText(_text);
            if (_font)     _graphic->deleteFont(_font);
            if (_window)   _graphic->deleteWindow(_window);

            /* Le relachement APRES les destructions, jamais avant : entre
             * les deux, sa dll pourrait se fermer. */
            _module->release();

            _mouse = nullptr;
            _keyboard = nullptr;
            _text = nullptr;
            _font = nullptr;
            _window = nullptr;
            _graphic = nullptr;
            _module = nullptr;
        }

        /* ---- le son, l'autre contrat ----------------------------------- *
         *
         * Meme protocole, et deliberement le meme dans le detail. Ce qui
         * change est ce qui n'y est PAS : aucune fenetre a ouvrir, donc
         * aucun refus possible, donc pas de patience a tenir. */

        void followAudio() {
            IModule *current = _modules.Current(IAudioModule::contract);

            if (_audioModule && !_audioModule->mustClose() && _audioModule == current)
                return;

            dropAudio();
            if (!current || current->mustClose())
                return;

            _audioModule = current;
            _audioModule->acquire();
            _audio = static_cast<IAudioModule *>(_audioModule);
            _generation++;
            _cadence.resync();   //decoder un ogg n'est pas du temps de jeu
        }

        void dropAudio() {
            if (!_audio)
                return;

            _generation++;   //meme raison : detruire, c'est perimer

            /* Les sons AVANT les tampons : ils sont batis dessus. */
            for (audio::IMusic *made : _musics)       { made->stop(); _audio->deleteMusic(made); }
            for (audio::ISound *made : _sounds)       _audio->deleteSound(made);
            for (audio::ISoundBuffer *made : _buffers) _audio->deleteSoundBuffer(made);
            _musics.clear();
            _sounds.clear();
            _buffers.clear();

            _audioModule->release();
            _audio = nullptr;
            _audioModule = nullptr;
        }

        IModuleRegistry &_modules;
        std::string _assets;
        mutable Cadence _cadence;

        IModule *_module = nullptr;             ///< le module tenu, verrou pris
        IGraphic2Module *_graphic = nullptr;    ///< le meme, vu comme fabrique
        graphic::IWindow2 *_window = nullptr;   ///< la NOTRE
        graphic::IFont *_font = nullptr;
        graphic::IText *_text = nullptr;
        graphic::IKeyboard *_keyboard = nullptr;
        graphic::IMouse *_mouse = nullptr;

        IModule *_audioModule = nullptr;
        IAudioModule *_audio = nullptr;

        std::vector<graphic::ITexture *> _textures;
        std::vector<graphic::ISprite *> _sprites;
        std::vector<graphic::IPolygon *> _polygons;
        std::vector<audio::ISoundBuffer *> _buffers;
        std::vector<audio::ISound *> _sounds;
        std::vector<audio::IMusic *> _musics;

        /* Le module qui nous a refuse une fenetre. Compare, jamais
         * dereference. */
        IModule *_refused = nullptr;
        int _patience = 0;

        unsigned _generation = 0;
};

#endif /* !HOST_HPP_ */
