# Swell Realistic guitar

Instrument VST3 stéréo réalisé avec JUCE 8.0.4. La version macOS fournie est compilée pour Apple Silicon.

## Jouer

Brancher le SSD et ouvrir **Swell Realistic guitar** sur une piste instrument. Le lecteur ouvre les exports WAV de `/Volumes/SSD/Swell Guitar`. Si le disque a changé de nom, le bouton de dossier en haut à droite permet de choisir le répertoire contenant **AG**, **NG**, **LP** et **TC**. Le chemin et les réglages sont enregistrés dans le projet de l’hôte.

La bibliothèque de gauche sélectionne la guitare. En mode Strum, jouer les notes d’un accord ensemble. Le sens et l’espacement règlent le coup de médiator. Le clavier de la façade permet d’écouter les sons à la souris. L’icône de clavier en haut à droite alterne clavier et manche.

Les neuf préréglages, les articulations et les six effets s’appliquent au moteur de lecture des WAV. Les vélocités sélectionnent les prises disponibles. La molette de pitch couvre deux demi-tons dans chaque sens. La pédale de sustain et la molette de modulation répondent aux contrôleurs MIDI 64 et 1.

## Bibliothèque et fonctionnement

Chaque instrument lit son dossier `AG Samples`, `AG Strum Samples`, etc. Le catalogue associe les prises identifiables à la corde, la frette, l’articulation et la couche. En mode Strum, les échantillons de relâchement, vibrato et glissando de la version standard de la même guitare complètent les articulations disponibles. Le vibrato de l’AG module la hauteur de ses prises Sustain. Les attaques sont préchargées et les suites des notes sont lues en arrière-plan. Les fichiers audio restent sur le SSD.

## Compiler

Prérequis : sources de JUCE 8.0.4, CMake 3.22 ou ultérieur, compilateur C++17. Sur le Mac utilisé ici, la cible de déploiement est macOS 12.

```sh
cmake -S . -B build -DJUCE_DIR=/chemin/vers/JUCE -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build build --target SwellGuitar_VST3 --config Release -j 4
```

Le catalogue des WAV et les quatre visuels sont inclus. Pour régénérer le catalogue après modification des exports :

```sh
python3 Tools/make_catalog.py "/Volumes/SSD/Swell Guitar"
```

Le test audio local est disponible avec `-DSWELL_TESTS=ON` et la cible `GuitarCheck`.
