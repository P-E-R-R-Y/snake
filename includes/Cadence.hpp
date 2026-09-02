/**
 * @file Cadence.hpp
 * @brief Le temps qui passe, pas les images qui defilent.
 *
 * Un jeu qui avance d'un pas par image tourne a la vitesse de la machine :
 * deux fois plus d'images, deux fois plus vite. La correction est de ne
 * plus compter les images du tout, mais les SECONDES ecoulees, et d'en
 * deduire combien de pas sont dus.
 *
 *   6 images/s   -> 166 ms d'ecart -> 8 pas d'un coup   (ca saccade)
 *  60 images/s   ->  16 ms d'ecart -> 1 pas             (le cas courant)
 * 240 images/s   ->   4 ms d'ecart -> 0 pas, puis 1     (ca glisse)
 *
 * Dans les trois cas, 50 pas par seconde de MONTRE. La machine lente saute
 * des images, elle ne ralentit pas le jeu.
 *
 * L'horloge est celle du systeme, pas celle du vendor. IWindow::getDelta()
 * existe, mais il disparait avec la fenetre : le jeu tourne sans ecran, et
 * pendant une bascule de bibliotheque il n'y a d'horloge nulle part. Une
 * source qui s'evapore au pire moment ne peut pas porter la simulation.
 */

#ifndef CADENCE_HPP_
#define CADENCE_HPP_

#include <chrono>

/**
 * @class Cadence
 * @brief Un pas fixe, un nombre variable de pas par image.
 *
 * Le pas est FIXE et c'est tout l'interet : la simulation reste
 * deterministe. Multiplier les deplacements par un delta variable donnerait
 * une trajectoire differente a chaque frequence d'images, et un serpent qui
 * traverse un mur quand la machine bafouille.
 */
class Cadence {

    public:
        /** @param perSecond les pas par seconde de montre. */
        explicit Cadence(double perSecond)
            : _step(1.0 / perSecond) { resync(); }

        /**
         * @brief Combien de pas sont dus depuis le dernier appel.
         *
         * A appeler UNE fois par image : elle consomme le temps ecoule.
         */
        int due() {
            const Instant now = Clock::now();
            double elapsed = std::chrono::duration<double>(now - _last).count();

            _last = now;

            /* LE GARDE-FOU. Sans lui, une image qui prend trois secondes
             * reclame cent cinquante pas, qui prennent plus d'une image a
             * calculer, qui en reclament d'autres : la spirale de la mort.
             *
             * Au-dela de cette borne on ADMET de perdre du temps de jeu.
             * C'est le seul cas ou la simulation ralentit vraiment, et c'est
             * preferable a une boucle qui ne rend jamais la main. */
            if (elapsed > CEILING)
                elapsed = CEILING;

            _carry += elapsed;

            int due = 0;

            while (_carry >= _step) {
                _carry -= _step;
                due++;
            }
            return due;
        }

        /**
         * @brief Ou en est le pas en cours, entre 0 et 1.
         *
         * De quoi interpoler : la logique est sur une grille, le dessin
         * glisse entre deux cases. Comme elle vient du temps et non d'un
         * compteur d'images, le glissement reste lisse a 30 comme a 240.
         */
        double phase() const { return _carry / _step; }

        /**
         * @brief Repartir de maintenant, sans rien devoir.
         *
         * INDISPENSABLE apres tout ce qui bloque : ouvrir une fenetre,
         * charger des textures, changer de vendor. Ces pauses-la ne sont pas
         * du temps de jeu ; sans cette remise a l'heure, le premier appel
         * suivant reclamerait d'un coup tous les pas de la pause, et le
         * serpent se tuerait dans le mur pendant que l'image apparait.
         */
        void resync() {
            _last = Clock::now();
            _carry = 0.0;
        }

    private:
        using Clock = std::chrono::steady_clock;
        using Instant = Clock::time_point;

        /* steady_clock et pas system_clock : celle-ci recule quand la
         * machine se met a l'heure, et un delta negatif n'a aucun sens ici. */

        /// Le plus grand ecart qu'on accepte de rattraper, en secondes.
        static constexpr double CEILING = 0.25;

        double _step;
        double _carry = 0.0;
        Instant _last;
};

#endif /* !CADENCE_HPP_ */
