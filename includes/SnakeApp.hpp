/**
 * @file SnakeApp.hpp
 * @brief Le jeu. Il ne connait aucun vendor, seulement des contrats.
 */

#ifndef SNAKEAPP_HPP_
#define SNAKEAPP_HPP_

#include "Host.hpp"

#include "ICore.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <string>

class SnakeApp : public IApp {

    public:
        SnakeApp(IModuleRegistry &modules, const std::string &assets)
            : _host(modules, assets, STEPS) {
            restart();
            /* Sans fenetre on demarre quand meme. La simulation ne depend
             * pas du vendor, seul l'affichage en depend - et personne ici ne
             * sait si l'absence d'ecran dure trois secondes ou toujours.
             * S'arreter serait decider a la place de l'hote. */
            /* Ni attach() ni destructeur a ecrire : _host prend ce qui est
             * en service au premier follow(), et le compilateur le detruit. */
            _host.follow(TITLE, true);
            rebuild();
        }

    protected:
        void event() override {
            using KB = graphic::IKeyboard;

            /* Avant de toucher a quoi que ce soit : la borne a pu basculer
             * de vendor dans SON event(), juste avant de nous donner le
             * notre. Notre fenetre serait alors deja detruite. */
            _host.follow(TITLE, true);
            if (_built != _host.generation())
                rebuild();

            if (!_host.window() || !_host.keyboard())
                return;
            if (!_host.window()->pollEvent())
                return;

            /* eventClose seulement chez lui : fermer la fenetre de la borne
             * ne serait pas a lui d'en decider. */
            _host.window()->eventClose();

            /* La croix ou echap ferment LE JEU, pas la borne. C'est sa
             * fenetre : personne d'autre n'a a decider ce que devient une
             * demande de fermeture qui la vise.
             *
             * stop() et pas un arret sec : la borne voit running() tomber
             * dans son update(), quitte le jeu proprement et reprend la
             * main sur son menu. */
            if (_host.closing())
                return stop();

            /* MISE EN FILE, pas ecrasement. Un tick dure un soixantieme de
             * seconde et un pas en dure douze : ^< arrive presque toujours
             * dans le meme intervalle. Garder le dernier perdrait le ^,
             * garder le premier perdrait le <. On joue les deux, un par pas. */
            if (_host.keyboard()->isKeyPressed(KB::KEY_LEFT))  turn(-1, 0);
            if (_host.keyboard()->isKeyPressed(KB::KEY_RIGHT)) turn(+1, 0);
            if (_host.keyboard()->isKeyPressed(KB::KEY_UP))    turn(0, -1);
            if (_host.keyboard()->isKeyPressed(KB::KEY_DOWN))  turn(0, +1);
            if (_host.keyboard()->isKeyPressed(KB::KEY_SPACE) && _dead)    restart();
        }

        void update() override {
            /* En premier, toujours : si la borne a condamne ma bibliotheque,
             * je lache et je reprends ailleurs avant de toucher a un objet
             * qu'elle a fabrique. */
            _host.follow(TITLE, true);
            if (_built != _host.generation())
                rebuild();

            /* Pas de _host.window() ici : le serpent avance qu'on le regarde ou
             * non. Seuls event() et display() ont besoin du vendor. */
            if (_dead)
                return void(_host.cadence().due());   //on consomme sans avancer

            /* Le temps decide, pas la machine. Cinq pas par seconde de
             * montre, que l'ecran en affiche six ou deux cent quarante. */
            for (int step = _host.cadence().due(); step > 0 && !_dead; step--)
                advance();
        }

        /** @brief UN pas : un virage, une case. Duree fixe, quoi qu'il arrive. */
        void advance() {
            /* Un virage par pas, dans l'ordre ou ils ont ete tapes. */
            if (!_turns.empty()) {
                _dx = _turns.front().x;
                _dy = _turns.front().y;
                _turns.pop_front();
            }

            const int x = _body.front().x + _dx;
            const int y = _body.front().y + _dy;

            if (x < 0 || y < 0 || x >= COLS || y >= ROWS || hits(x, y))
                return void(_dead = true);

            _body.push_front({x, y});
            if (x == _food.x && y == _food.y) {
                _score++;
                dropFood();
                /* En grandissant, la queue ne libere rien : elle reste ou
                 * elle est, et l'interpolation ne la fait pas glisser. */
                _vacated = _body.back();
            } else {
                _vacated = _body.back();
                _body.pop_back();
            }
        }

        void display() override {
            if (!_host.window() || !_cell)
                return;

            /* Chez lui, il ouvre et ferme sa frame. Invite dans celle de la
             * borne, il se contente de dessiner : la frame appartient a qui
             * possede la fenetre. */
            /* La grille suit la fenetre : elle a pu etre redimensionnee, ou
             * refabriquee par un autre vendor a une autre taille. */
            layout();

            _host.window()->beginDraw();

            /* Le mur AVANT tout le reste, et en cases pleines : le joueur
             * doit voir ou il meurt. update() tue hors de [0,COLS[ x [0,ROWS[
             * et rien ne le montrait. */
            _cell->setColor({70, 80, 110, 255});
            for (int x = -1; x <= COLS; x++) {
                draw(x, -1);
                draw(x, ROWS);
            }
            for (int y = 0; y < ROWS; y++) {
                draw(-1, y);
                draw(COLS, y);
            }

            drawApple();

            /* Le corps en premier, la tete par-dessus : elle est plus large
             * que le corps aux angles, et on veut la voir entiere. */
            /* CHAQUE anneau glisse vers celui qui le precede, du meme pas
             * et en meme temps. Deux anneaux voisins restent donc toujours
             * exactement a une case l'un de l'autre, quelle que soit la
             * phase - c'est ce qui fait que les carres se touchent sans
             * qu'on ait a fabriquer des pieces droites et des coudes. Un
             * virage devient un coude tout seul.
             *
             * L'etat reste une grille d'entiers : rien de tout ceci ne
             * remonte dans _body, et les collisions ne voient que des cases. */
            /* De la queue vers la tete : les anneaux epais passent par
             * dessus les fins, et le fuselage se lit dans le bon sens. */
            for (size_t i = _body.size(); i-- > 1; ) {
                graphic::IPolygon *ring = girth(i);
                const double shade = 1.0 - 0.25 * double(i) / _body.size();

                ring->setColor({uint8_t(110 * shade), uint8_t(205 * shade),
                                uint8_t(140 * shade), 255});
                ring->setPosition(slide(i));
                _host.window()->drawPoly(ring);
            }
            drawHead();

            _host.text()->setFont(_host.font());
            _host.text()->setFontSize(20);
            _host.text()->setTextColor({235, 235, 240, 255});
            _host.text()->setPosition({MARGIN, 18.f});
            _host.text()->setText(_dead
                ? "snake  " + std::to_string(_score) + "   perdu, espace pour rejouer"
                : "snake  " + std::to_string(_score) + "   fleches");
            _host.window()->drawText(_host.text());

            _host.window()->endDraw();
        }

        /* ---- ce que le jeu fabrique ----------------------------------- *
         *
         * UNE seule fonction, appelee quand la generation change ou quand la
         * case change de taille. Host a deja detruit les anciennes formes ;
         * on ne fait qu'ecraser des pointeurs devenus caducs.
         *
         * DES DISQUES, pas des carres. A une case d'ecart exactement, deux
         * disques de rayon 0.5 ne se touchent qu'en un point et le corps se
         * pince ; au-dela de 0.5 ils se chevauchent et forment un tube
         * continu. D'ou les rayons superieurs a la moitie.
         *
         * Trois epaisseurs : le serpent s'affine vers la queue. Le polygone
         * n'ayant pas d'echelle, une epaisseur EST un polygone - trois
         * suffisent sans en fabriquer un par anneau. */
        void rebuild() {
            _built = _host.generation();

            _cell = _host.polygon(disc(_side * 0.58, 10));
            _mid  = _host.polygon(disc(_side * 0.50, 10));
            _thin = _host.polygon(disc(_side * 0.40, 10));

            /* La tete est un octogone : assez proche du carre pour que le
             * corps reste lisible, assez arrondi pour qu'on voie ou elle
             * regarde. */
            _head  = _host.polygon(disc(_side * 0.60, 12));
            _eye   = _host.polygon(disc(_side * 0.15, 8));
            _pupil = _host.polygon(disc(_side * 0.075, 6));

            /* La pomme : un disque a douze cotes plutot qu'un carre, plus
             * une feuille et une queue. IPolygon ne connait que des points,
             * donc l'inclinaison de la feuille est cuite dans les siens. */
            _apple = _host.polygon(disc(_side * 0.36, 12));
            _leaf  = _host.polygon(leaf(_side * 0.30));
            _stem  = _host.polygon({{-_side * 0.03, -_side * 0.22},
                                    { _side * 0.03, -_side * 0.22},
                                    { _side * 0.05, -_side * 0.44},
                                    {-_side * 0.01, -_side * 0.44}});
        }

        /** @brief Un cercle approche par @p sides cotes, centre sur l'origine. */
        static std::vector<Vector2f> disc(double radius, int sides) {
            std::vector<Vector2f> points;

            for (int i = 0; i < sides; i++) {
                const double angle = i * 2.0 * 3.14159265 / sides;

                points.push_back({std::cos(angle) * radius, std::sin(angle) * radius});
            }
            return points;
        }

        /**
         * @brief Une feuille : deux arcs qui se rejoignent en pointe.
         *
         * L'inclinaison est cuite dans les points. IPolygon n'expose ni
         * rotation ni echelle - une forme est sa liste de sommets, un point
         * final - donc ce qui doit pencher penche a la fabrication.
         */
        static std::vector<Vector2f> leaf(double length) {
            std::vector<Vector2f> points;
            const int steps = 6;
            const double tilt = -0.45;   //radians, la pointe vers le haut

            for (int i = 0; i <= steps; i++) {
                const double u = double(i) / steps;

                points.push_back(turned(u * length,
                                        std::sin(u * 3.14159265) * length * 0.32, tilt));
            }
            for (int i = steps; i >= 0; i--) {
                const double u = double(i) / steps;

                points.push_back(turned(u * length,
                                        -std::sin(u * 3.14159265) * length * 0.32, tilt));
            }
            return points;
        }

        /** @brief Le point (x, y) tourne de @p angle radians autour de l'origine. */
        static Vector2f turned(double x, double y, double angle) {
            return {x * std::cos(angle) - y * std::sin(angle),
                    x * std::sin(angle) + y * std::cos(angle)};
        }

    private:
        struct Point { int x, y; };

        static constexpr int COLS = 12;
        static constexpr int ROWS = 7;
        /* Un pas tous les N ticks. Douze et non six : les cases ayant
         * double, un pas couvre deux fois plus de distance a l'ecran. A six
         * le serpent filait deux fois trop vite pour l'oeil. */
        /* Les pas par seconde. Cinq : c'est exactement ce que donnaient
         * les douze images d'attente sur un ecran a 60 Hz - la meme vitesse
         * qu'avant, mais la meme sur toutes les machines. */
        static constexpr double STEPS = 5.0;
        static constexpr const char *TITLE = "snake";
        static constexpr double MARGIN = 16.0;
        static constexpr double HEADER = 48.0;    ///< la ligne de score, au-dessus

        /* Le plafond doit suivre la grille : c'est LUI qui fixe la taille
         * reelle des cases des que la fenetre est assez grande. Le laisser
         * a 26 annulerait la reduction de COLS/ROWS - la grille tiendrait
         * dans un coin au lieu de grossir. */
        static constexpr double MAX_SIDE = 72.0;

        /**
         * @brief Recalcule la grille pour la fenetre courante.
         *
         * Le mur compte dans le calcul : il occupe une case de chaque cote,
         * sans quoi la bordure deborderait de l'ecran.
         *
         * IPolygon n'a ni echelle ni points modifiables, donc un cote
         * different oblige a refabriquer le carre. C'est le seul endroit ou
         * build() est rejoue sans changement de vendor.
         */
        void layout() {
            const Vector2f size = _host.window()->getSize();
            const double room = std::min((size.x - 2 * MARGIN) / (COLS + 2),
                                         (size.y - HEADER - 2 * MARGIN) / (ROWS + 2));
            const double side = std::clamp(room, 2.0, MAX_SIDE);

            _originX = (size.x - COLS * side) / 2.0 + side / 2.0;
            _originY = HEADER + (size.y - HEADER - ROWS * side) / 2.0 + side / 2.0;

            if (side == _side || !_host.visible())
                return;
            _side = side;

            /* Host jette les anciennes formes ; on en refait aussitot une
             * serie a la nouvelle taille. */
            _host.discard();
            rebuild();
        }

        /** @brief La pomme : un disque, une tige, une feuille. */
        void drawApple() {
            const Vector2f at{_originX + _food.x * _side, _originY + _food.y * _side};

            _stem->setColor({90, 60, 40, 255});
            _stem->setPosition({at.x, at.y - _side * 0.16});
            _host.window()->drawPoly(_stem);

            /* La feuille part de la tige et pointe vers la droite. */
            _leaf->setColor({90, 180, 95, 255});
            _leaf->setPosition({at.x + _side * 0.02, at.y - _side * 0.26});
            _host.window()->drawPoly(_leaf);

            _apple->setColor({215, 65, 75, 255});
            _apple->setPosition({at.x, at.y + _side * 0.06});
            _host.window()->drawPoly(_apple);
        }

        /**
         * @brief La tete, et ou elle regarde.
         *
         * Les yeux se placent dans le repere du deplacement : "devant" est
         * (_dx, _dy), "de cote" est sa perpendiculaire (-_dy, _dx). Un seul
         * calcul sert donc aux quatre directions, sans table de cas.
         */
        void drawHead() {
            const Vector2f at = slide(0);
            const double aheadX = _dx * _side * 0.17;
            const double aheadY = _dy * _side * 0.17;
            const double sideX = -_dy * _side * 0.20;
            const double sideY = _dx * _side * 0.20;

            _head->setColor(_dead ? Color{190, 110, 110, 255}
                                  : Color{140, 230, 165, 255});
            _head->setPosition(at);
            _host.window()->drawPoly(_head);

            for (int side = -1; side <= 1; side += 2) {
                const Vector2f eye{at.x + aheadX + side * sideX,
                                   at.y + aheadY + side * sideY};

                _eye->setColor({250, 250, 250, 255});
                _eye->setPosition(eye);
                _host.window()->drawPoly(_eye);

                /* La pupille avance un peu plus que l'oeil : c'est ce
                 * decalage qui donne le regard. */
                _pupil->setColor(_dead ? Color{200, 60, 60, 255}
                                       : Color{30, 40, 50, 255});
                _pupil->setPosition({eye.x + aheadX * 0.45, eye.y + aheadY * 0.45});
                _host.window()->drawPoly(_pupil);
            }
        }

        /**
         * @brief L'epaisseur de l'anneau @p i : le corps s'affine vers la queue.
         */
        graphic::IPolygon *girth(size_t i) const {
            const double along = double(i) / double(_body.size());

            if (along < 0.45)
                return _cell;
            return along < 0.75 ? _mid : _thin;
        }

        /**
         * @brief Ou en est le pas courant, entre 0 et 1.
         *
         * La cadence porte deja le reste de temps depuis le dernier pas :
         * la fluidite ne coute aucun etat de plus. Et comme ce reste vient
         * d'une horloge, le glissement est lisse a 240 images comme a 30,
         * la ou un compteur d'images le rendait saccade sur une machine
         * lente.
         */
        double phase() const {
            return _dead ? 1.0 : _host.cadence().phase();
        }

        /**
         * @brief La position DESSINEE de l'anneau @p i, entre deux cases.
         *
         * Il part de la case de son suivant et va vers la sienne. Le dernier
         * part de la case que la queue vient de liberer - la seule chose
         * qu'il a fallu retenir.
         */
        Vector2f slide(size_t i) const {
            const Point &to = _body[i];
            const Point &from = (i + 1 < _body.size()) ? _body[i + 1] : _vacated;
            const double t = phase();

            return {_originX + (from.x + (to.x - from.x) * t) * _side,
                    _originY + (from.y + (to.y - from.y) * t) * _side};
        }

        /** @brief Un seul polygone, repositionne pour chaque case. */
        void draw(int x, int y) {
            _cell->setPosition({_originX + x * _side, _originY + y * _side});
            _host.window()->drawPoly(_cell);
        }

        /**
         * @brief Empile un virage s'il est jouable.
         *
         * On se valide contre le DERNIER virage en file, ou contre le pas
         * courant si elle est vide : c'est la direction qu'aura le serpent
         * au moment ou celui-ci s'appliquera. Se valider contre _dx/_dy
         * seul laisserait passer ^< comme un demi-tour.
         */
        void turn(int dx, int dy) {
            const int lastX = _turns.empty() ? _dx : _turns.back().x;
            const int lastY = _turns.empty() ? _dy : _turns.back().y;

            if (dx == -lastX && dy == -lastY)
                return;   //demi-tour : dans sa propre nuque
            if (dx == lastX && dy == lastY)
                return;   //deja dans ce sens : rien a empiler
            if (_turns.size() < MAX_TURNS)
                _turns.push_back({dx, dy});
        }

        bool hits(int x, int y) const {
            for (const Point &p : _body)
                if (p.x == x && p.y == y)
                    return true;
            return false;
        }

        void dropFood() {
            do {
                _food = {std::rand() % COLS, std::rand() % ROWS};
            } while (hits(_food.x, _food.y));
        }

        void restart() {
            _body.assign({{COLS / 2, ROWS / 2}});
            _vacated = _body.back();
            _dx = 1;
            _dy = 0;
            _turns.clear();
            _score = 0;
            _dead = false;
            dropFood();
            /* Une partie neuve part sur un pas neuf : le temps passe sur
             * l'ecran de mort n'est pas du a la nouvelle. */
            _host.cadence().resync();
        }

        /* LA BIBLIOTHEQUE EN SERVICE, et tout ce qu'elle a fabrique.
         * Membre et pas classe de base : le compilateur le detruit, donc le
         * menage a vraiment lieu. */
        Host _host;

        /// La generation vue au dernier rebuild(). Differente = on refait.
        unsigned _built = 0;

        std::deque<Point> _body;
        Point _food{0, 0};
        /// La case que la queue vient de quitter, pour la faire glisser.
        Point _vacated{0, 0};
        /* _dx/_dy : le dernier pas effectue. _turns : ce qui a ete tape
         * depuis et attend son tour.
         *
         * DEUX, et il n'y a pas d'annulation. Trois raisons.
         *
         * A cinq pas par seconde, deux virages en attente bornent le retard
         * a 400 ms. Un troisieme se jouerait 600 ms apres la frappe, quand
         * le joueur a deja oublie l'avoir tapee.
         *
         * Annuler serait ambigu : dans >^>, le dernier > veut dire "apres
         * etre monte, repars a droite". C'est la meme frappe que "oublie
         * mon ^". Rien ne les distingue.
         *
         * Et la file ne se purge que par la queue : les virages y forment
         * une chaine validee, le < de ^< n'etant legal QUE parce que le ^
         * le precede. Retirer la tete rendrait le suivant illegal. */
        static constexpr size_t MAX_TURNS = 2;

        int _dx = 1, _dy = 0;
        std::deque<Point> _turns;
        int _score = 0;
        bool _dead = false;

        /* Deduits de la fenetre a chaque frame, jamais de l'etat du jeu. */
        double _side = MAX_SIDE;
        double _originX = MARGIN;
        double _originY = HEADER;

        graphic::IPolygon *_cell = nullptr;    ///< le corps, pres de la tete
        graphic::IPolygon *_mid = nullptr;     ///< le milieu
        graphic::IPolygon *_thin = nullptr;    ///< vers la queue
        graphic::IPolygon *_head = nullptr;    ///< la tete, orientee
        graphic::IPolygon *_eye = nullptr;
        graphic::IPolygon *_pupil = nullptr;
        graphic::IPolygon *_apple = nullptr;
        graphic::IPolygon *_leaf = nullptr;
        graphic::IPolygon *_stem = nullptr;
};

#endif /* !SNAKEAPP_HPP_ */
