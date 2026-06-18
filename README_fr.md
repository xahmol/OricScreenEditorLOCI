# Oric Screen Editor
Éditeur d'écran pour l'Oric Atmos

## Sommaire

[Historique des versions et téléchargement](#historique-des-versions-et-téléchargement)

[Introduction](#introduction)

[Problèmes connus](#problèmes-connus)

[Démarrage du programme](#démarrage-du-programme)

[Mode principal](#mode-principal)

[Barre d'état](#barre-détat)

[Menu principal](#menu-principal)

[Éditeur de caractères](#éditeur-de-caractères)

[Mode palette](#mode-palette)

[Sélecteur de couleurs](#sélecteur-de-couleurs)

[Mode sélection](#mode-sélection)

[Mode déplacement](#mode-déplacement)

[Mode ligne et boîte](#mode-ligne-et-boîte)

[Mode écriture](#mode-écriture)

[Référence des valeurs de couleur](#référence-des-valeurs-de-couleur)

[Référence des codes d'attribut série](#référence-des-codes-dattribut-série)

[Référence du format de fichier](#référence-du-format-de-fichier)

[Crédits](#crédits)


![Écran-titre d'OSE](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Titlescreen.png?raw=true)

## Historique des versions et téléchargement
([Retour au sommaire](#sommaire))

Lien vers les dernières versions compilées :
- [Image disque .DSK](https://github.com/xahmol/OricScreenEditor/raw/main/BUILD/OSE.dsk)
- [Image disque .HFE](https://github.com/xahmol/OricScreenEditor/raw/main/BUILD/OSE.hfe)

Version v099-20220824-1345 :
- Ajout du sélecteur de fichiers
- Autres ajustements et corrections mineurs

Version v099-20220615-1454 :
- Première version bêta publiée, basée sur [VDCSE](https://github.com/xahmol/VDCScreenEdit) version v099-20220324-1527

## Introduction
([Retour au sommaire](#sommaire))

Oric Screen Editor est un éditeur permettant de créer des écrans textuels pour l'Oric Atmos. Il prend entièrement en charge l'utilisation de jeux de caractères personnalisés.

Principales fonctionnalités du programme :
- Prise en charge d'écrans plus grands que 40x25 caractères. Les écrans peuvent atteindre 8 Kio (8 192 octets) ; toutes les tailles tenant dans cette mémoire, avec une largeur minimale de 40 et une hauteur minimale de 27, sont prises en charge.
- Prend en charge le redimensionnement de la zone de dessin, son effacement ou son remplissage
- Prise en charge du chargement de jeux de caractères personnalisés (doivent être des jeux de caractères standards de 96 caractères de 6 bits de large et 8 bits de haut, des jeux alternatifs de 80 caractères, ou des jeux combinés de 176 caractères).
- Inclut un éditeur de caractères simple pour modifier les caractères à la volée et voir directement le résultat dans l'écran en cours de création.
- Prend en charge les attributs sériels de l'Oric pour l'encre, le papier et les modificateurs de caractère (jeu de caractères standard ou alternatif, double hauteur, clignotement).
- Mode écriture pour saisir librement des caractères au clavier
- Mode ligne et boîte pour dessiner des lignes et des boîtes
- Mode sélection pour couper, copier, effacer ou repeindre (couleur seule ou tous les attributs) la sélection.
- Mode déplacement pour faire défiler le contenu de l'écran (uniquement pour la fenêtre visible de 40x27, en raison de contraintes mémoire)
- Mode palette, incluant un mode de plan de caractères visuel, pour sélectionner visuellement les caractères et les couleurs
- Emplacements favoris pour sélectionner rapidement 10 caractères favoris
- **Ajout OSE-LOCI** : prend en charge un joystick compatible IJK
  (interface Raxiss IJK) comme alternative au clavier partout où les
  touches curseur et ENTER sont utilisées -- aucun mode ou réglage séparé
  n'est nécessaire, il est détecté automatiquement au démarrage et
  fonctionne simplement en parallèle du clavier.

## Problèmes connus
- La routine de sélection de fichiers ne fonctionne correctement qu'avec les disques SEDORIC3 créés par l'outil TAP2DSK de l'OSDK. Si vous souhaitez importer des écrans ou des jeux de caractères depuis d'autres disques, copiez-les d'abord sur une image créée avec TAP2DSK.

## Démarrage du programme
([Retour au sommaire](#sommaire))

Montez l'image disque OSE. Choisissez l'image .DSK ou .HFE selon ce que prend en charge votre matériel ou émulateur (en général : .HFE pour Cumana Reborn, .DSK pour un émulateur).

L'exécutable OSE démarre automatiquement au montage du disque. Si ce n'est pas le cas, tapez simplement OSE\<ENTER\>.

Description du contenu de l'image disque :

|Nom de fichier|Extension|Description|
|---|---|---|
|OSE|.COM|Exécutable principal
|OSEHS1|.BIN|Écran d'aide pour le mode principal
|OSEHS2|.BIN|Écran d'aide pour les modes édition de caractères et palette
|OSEHS3|.BIN|Écran d'aide pour les modes sélection, déplacement et ligne/boîte
|OSEHS4|.BIN|Écran d'aide pour les modes écriture et écriture colorée
|OSETSC|.BIN|Écran-titre


(Anecdote : tous ces écrans ont en fait été créés avec OSE comme éditeur)

Quittez l'écran-titre en appuyant sur n'importe quelle touche.

## Mode principal
([Retour au sommaire](#sommaire))

Après l'écran-titre, le programme démarre dans ce mode. Au démarrage, l'écran affiche ceci :

![Écran en mode principal](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20empty%20startscreen.png?raw=true)

Seul un curseur inversé affichant le code écran actuellement sélectionné est visible.

Appuyez sur ces touches en mode principal pour l'édition :

|Touche|Description
|---|---|
|**Touches curseur**|Déplacer le curseur
|**+** ou **=**|Caractère suivant (augmente le code écran)
|**-**|Caractère précédent (diminue le code écran)
|**0-9**|Sélectionner le caractère de l'emplacement favori correspondant
|**SHIFT + 0-9**|Enregistrer le caractère dans l'emplacement favori correspondant
|**,**|Encre précédente (diminue le numéro de couleur)
|**.**|Encre suivante (augmente le numéro de couleur)
|**;**|Diminuer le papier (diminue le numéro de couleur)
|**\'**|Augmenter le papier (augmente le numéro de couleur)
|**ESPACE**|Tracer avec le code écran et les attributs actuels
|**DEL**|Effacer la position actuelle du curseur (trace un espace blanc)
|**B**|Basculer l'attribut '**B**link' (clignotement)
|**A**|Basculer entre le jeu de caractères **A**lternatif et Standard
|**D**|Basculer l'attribut **D**ouble hauteur
|**I**|Tracer un attribut modificateur **I**nk (encre) pour la couleur d'encre actuellement sélectionnée
|**O**|Tracer un attribut modificateur Paper (papier) pour la couleur de papier actuellement sélectionnée
|**U**|Tracer un attribut modificateur de jeu de caractères pour la sélection de jeu, la taille double et le clignotement actuellement sélectionnés
|**E**|Aller au mode '**É**dition de caractère' avec le code écran actuel
|**G**|Récupérer (**g**rab) le caractère et l'attribut situés sous le curseur
|**W**|Aller au mode '**W**rite' (écriture)
|**L**|Aller au mode '**L**igne et boîte'
|**M**|Aller au mode '**M**ove' (déplacement)
|**S**|Aller au mode '**S**élection'
|**P**|Aller au mode '**P**alette'
|**C**|Aller au sélecteur de couleurs ('**C**olour picker')
|**T**|Mode d'essai (**T**ry)
|**R**|Basculer '**R**everse' (vidéo inversée) : bascule l'augmentation/diminution du code écran de 128
|**Z**|**OSE-LOCI uniquement :** Annuler la dernière modification de la zone de dessin
|**Y**|**OSE-LOCI uniquement :** Rétablir la dernière modification annulée
|**FUNCT+1**|Aller au menu principal
|**FUNCT+6**|Basculer la visibilité de la barre d'état
|**FUNCT+8**|Écran d'aide

**NB** : Dans Oricutron, avec la configuration clavier par défaut, la touche FUNCT est associée à la touche ALT du clavier PC.

*Déplacer le curseur*

Appuyez sur les **touches curseur** pour déplacer le curseur sur l'écran. Si la taille de la zone de dessin est plus grande que l'écran de 40x27, l'écran défile en atteignant les bords.

*Sélectionner le code écran à tracer*

La touche **+** ou **-** augmente, respectivement diminue, le code écran sélectionné de un. Le curseur se met à jour pour afficher le code écran actuellement sélectionné.

Appuyer sur **R** augmente le code écran de 128 si le code actuel est inférieur à 128, sinon le diminue de 128. Ceci active/désactive le bit d'inversion pour l'inversion matérielle du caractère.

*Sélectionner le code écran à tracer depuis un emplacement favori*

Dans OSE, 10 emplacements sont disponibles pour stocker vos caractères les plus utilisés. Appuyer sur une touche **0-9** sélectionne le favori avec le numéro correspondant.

*Enregistrer le code écran actuel dans un emplacement favori*

Appuyer sur **SHIFT** plus **0-9** enregistre le caractère actuellement sélectionné dans l'emplacement favori correspondant.

*Sélectionner les attributs à tracer*

Augmentez ou diminuez le [code couleur](#référence-des-valeurs-de-couleur) de un en appuyant sur la touche **.** resp. **,** pour la couleur d'encre, ou **;** resp. **\'** pour la couleur de papier. Appuyer sur **B**, **D** ou **A** bascule l'attribut **B**link (clignotement), **D**ouble taille ou **A**lternatif du jeu de caractères. Vous pouvez aussi appuyer sur **C** pour ouvrir le [sélecteur de couleurs](#sélecteur-de-couleurs) et choisir ensemble les couleurs d'Encre et de Papier depuis une grille visuelle.

*Tracer et effacer un caractère*

Appuyez sur **ESPACE** pour tracer le caractère actuellement sélectionné à la position actuelle du curseur. **DEL** efface le caractère ou la valeur d'attribut à la position actuelle et le remplace par un caractère ESPACE.
Appuyez sur **T** pour prévisualiser l'apparence du caractère sélectionné s'il était tracé sans le clignotement du curseur. Appuyez ensuite sur **ESPACE** pour confirmer le tracé du caractère, ou sur toute autre touche pour annuler.

*Tracer des attributs sériels*

En raison de la façon dont l'Oric gère les [changements de couleur et d'attribut](#référence-des-codes-dattribut-série), chaque position d'écran est soit un modificateur d'attribut, soit un caractère, mais jamais les deux à la fois. Pour cette raison, tracer un caractère ne trace aucun attribut. L'utilisateur d'OSE doit tracer les attributs sériels séparément, selon son propre choix.
Pour cela, appuyez sur **I** pour tracer un modificateur **I**nk (encre) pour la couleur d'encre actuelle, **P** pour tracer un modificateur **P**aper (papier) pour la couleur de papier actuelle, et **U** pour tracer un modificateur de jeu de caractères pour les attributs de caractère actuels (les bascules alternatif, double et clignotement).

*Récupérer un caractère*

Appuyer sur **G** permet de « récupérer » le caractère ou les attributs à la position actuelle du curseur et de remplacer le code écran ou l'attribut sélectionné par ces valeurs, pour utilisation dans toutes les autres fonctions d'édition.

*Mode édition de caractère*

Ceci ouvre le [mode édition de caractère](#éditeur-de-caractères) et commence l'édition du code écran actuellement sélectionné. Astuce : si vous souhaitez éditer un caractère précis de l'écran, récupérez-le d'abord en déplaçant le curseur sur ce caractère et en appuyant sur **G** pour récupérer.

*Entrer dans les modes d'édition*

Appuyez sur **S** ([Mode sélection](#mode-sélection)), **M** ([Mode déplacement](#mode-déplacement)), **L** ([Mode ligne et boîte](#mode-ligne-et-boîte)) ou **W** ([Mode écriture](#mode-écriture)) pour entrer dans le mode d'édition correspondant.
Référez-vous aux sections spécifiques de ce readme pour ces modes (cliquez sur les liens). Depuis tous les modes, revenez au mode principal en appuyant sur **ESC**.

*Annuler et rétablir (OSE-LOCI uniquement)*

Appuyez sur **Z** pour annuler la dernière modification de la zone de
dessin (tracé, Ligne/Boîte, remplissages/couper/copier de Sélection,
Déplacement, mode Écriture, ou Écran > Effacer/Remplir), et **Y** pour
rétablir la dernière modification annulée. Cette fonction nécessite un
périphérique LOCI connecté (l'historique d'annulation est stocké dans la
RAM d'extension de l'Oric, que seul un périphérique LOCI peut activer) --
sans périphérique, **Z**/**Y** ne font rien. Il n'existe pas d'équivalent
de cette fonction dans V1.

*Basculer la visibilité de la barre d'état*
Appuyez sur **FUNCT+6** pour basculer entre la barre d'état visible (par défaut) ou non.

*Écran d'aide*
Appuyez sur **FUNCT+8** pour afficher un écran d'aide avec toutes les commandes clavier de ce mode.

## Barre d'état
([Retour au sommaire](#sommaire))

Si elle est activée, la barre d'état est affichée ainsi sur la dernière ligne de l'écran :

![Barre d'état](https://github.com/xahmol/OricScreenEditor/raw/main/screenshots/OSE%20statusbar.png)

De gauche à droite, cette barre d'état affiche :

- Mode : le mode dans lequel se trouve le programme (tel que Principal, Sélection, Ligne/Boîte, Palette ou Éditeur de caractères).
- X,Y : coordonnées X et Y du curseur (coordonnées de l'écran complet, et pas seulement de la partie visible, si un écran plus grand que 40 par 27 caractères est sélectionné. Normalement affichées en décimal, mais en hexadécimal si la taille maximale de X ou Y dépasse 99)
- C : code écran (Screencode), le caractère actuellement sélectionné pour le tracé, d'abord sous forme de numéro de code écran en hexadécimal, puis sous forme du caractère visuel réel.
- S : le code écran en mémoire sous la position du curseur. Affiche soit le code de caractère (supérieur à $20), soit le code d'attribut (inférieur à $20), en hexadécimal
- I : Ink (encre), la couleur actuellement sélectionnée pour l'encre. D'abord sous forme de numéro 0-7, puis sous forme de couleur visuelle.
- P : Paper (papier), la couleur actuellement sélectionnée pour le papier. D'abord sous forme de numéro 0-7, puis sous forme de couleur visuelle.
- Les trois dernières positions affichent les attributs modificateurs de jeu de caractères activés : A si le jeu alternatif est activé, D pour la taille double et B pour le clignotement.

La barre d'état se masque automatiquement si le curseur est déplacé sur la dernière ligne visible de l'écran, et réapparaît (si elle était activée à l'origine) lorsque le curseur remonte.

Appuyer sur **FUNCT+6** bascule la visibilité de la barre d'état dans tous les modes.

## Menu principal
([Retour au sommaire](#sommaire))

Depuis le [mode principal](#mode-principal), appuyez sur **FUNCT+1** pour aller au menu principal. Le menu suivant apparaît :
![Menu principal d'OSE](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Main%20menu.png?raw=true)

(NB : si votre création utilise un jeu de caractères modifié, le programme charge la police système standard et votre création peut momentanément sembler incorrecte. Celle-ci sera restaurée à la sortie du menu principal. De plus, les menus déroulants affichent une ombre noire à droite en raison de limitations du système Oric).

La navigation dans ce menu se fait avec les touches suivantes :

|Touche|Description
|---|---|
|**Curseur GAUCHE / DROITE**|Se déplacer entre les options du menu principal
|**Curseur HAUT / BAS**|Se déplacer entre les options du menu déroulant
|**RETURN**|Sélectionner l'option de menu en surbrillance
|**ESC** |Quitter le menu et revenir en arrière

**_Menu Écran_**

![Menu Écran](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Screen%20menu.png?raw=true)

*Width : redimensionner la largeur*

Redimensionnez la largeur de la zone de dessin en saisissant la nouvelle largeur. Vous pouvez aussi bien réduire qu'agrandir la largeur. La largeur minimale est 40, la largeur maximale dépend de la hauteur de la zone de dessin et du fait que le résultat tienne dans l'allocation mémoire maximale de 8 Kio.

Notez qu'en réduisant la largeur vous risquez de perdre des données, car tous les caractères à droite de la nouvelle largeur seront perdus. C'est pourquoi, lors d'une réduction, un menu déroulant apparaît pour vous demander confirmation. Sélectionnez la réponse souhaitée (position en surbrillance jaune si vous utilisez un fond noir).

![Redimensionner la largeur](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Screen%20menu%20-%20width.png?raw=true)

*Height : redimensionner la hauteur*

Comme pour le redimensionnement de la largeur, cette option permet de redimensionner la hauteur de la même façon. La hauteur minimale est 27, le maximum dépend à nouveau de la largeur, compte tenu de l'allocation mémoire maximale de 8 Kio.

Là aussi : en réduisant, vous risquez de perdre des données, qui seront effectivement perdues si vous confirmez.

![Redimensionner la hauteur](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Screen%20menu%20-%20height.png?raw=true)

*Clear : effacer la zone de dessin*

Sélectionner cette option de menu efface la zone de dessin (c'est-à-dire la remplit d'espaces, avec les codes d'attribut pour la couleur d'encre et de papier sélectionnées tracés dans les deux premières colonnes de l'écran). Aucune confirmation n'est demandée.

*Fill : remplir la zone de dessin*

Comme pour Clear, mais cette option remplit la zone de dessin avec le code écran actuellement sélectionné (c'est-à-dire les valeurs affichées par le curseur).

**_Menu Fichier_**

![Menu Fichier](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20filemenu.png?raw=true)

**Note OSE-LOCI** : cette réécriture utilise le **périphérique de stockage
de masse LOCI** pour toutes les opérations sur fichiers, au lieu des
commandes cassette de V1. Les actions de **sauvegarde** demandent un nom
de fichier saisi (48 caractères maximum). Les actions de **chargement**
ouvrent à la place un navigateur de fichiers : une liste défilante du
répertoire (touches curseur pour se déplacer, ENTER pour entrer dans un
dossier ou sélectionner le fichier en surbrillance, GAUCHE pour remonter
au dossier parent, ESC pour annuler) n'affichant que les fichiers
pertinents pour l'action choisie (par exemple, Charger projet n'affiche
que les fichiers de projet). Si aucun périphérique LOCI n'est détecté,
chaque option des menus Fichier/Charset affiche un message "Aucun LOCI
detecte" au lieu de tenter l'opération, le reste de l'éditeur continuant
à fonctionner normalement sans périphérique connecté.

*Save screen / Load screen (sauvegarder/charger l'écran)*

Sauvegarde ou charge uniquement la zone de dessin (sans les jeux de
caractères) dans/depuis `<nomfichier>.BIN` sur le périphérique LOCI : un
petit en-tête enregistrant la largeur/hauteur de la zone de dessin, suivi
des données d'écran. Le chargement applique automatiquement la largeur/
hauteur sauvegardée -- aucune saisie séparée n'est nécessaire.

*Save project / Load project (sauvegarder/charger le projet)*

Sauvegarde ou charge la zone de dessin avec ses métadonnées (position du
curseur, fenêtre visible, sélection encre/papier/clignotement/double
hauteur/jeu alternatif) et, si vous les avez modifiés pendant la session,
les deux jeux de caractères -- sous la forme de jusqu'à quatre fichiers
partageant le nom saisi : `<nomfichier>PJ.BIN` (métadonnées),
`<nomfichier>SC.BIN` (écran), `<nomfichier>CS.BIN` (jeu standard, écrit/
attendu uniquement si vous l'avez modifié) et `<nomfichier>CA.BIN` (jeu
alternatif, même condition). Charger un projet sauvegardé avec un seul jeu
modifié laisse l'autre jeu inchangé (tel qu'il était avant le chargement).

*Save combined / Load combined (sauvegarder/charger combiné)*

Sauvegarde ou charge la zone de dessin avec le jeu de caractères standard
dans un seul fichier `<nomfichier>.BIN` (en-tête + les 768 octets du jeu
standard + les données d'écran).

**_Charset : charger et sauvegarder le jeu de caractères_**

![Menu Charset](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20charsetmenu.png?raw=true)

Chargez ou sauvegardez le jeu standard ou alternatif séparément (768
octets chacun, `<nomfichier>.BIN`), ou "combiné" : sauvegarder en combiné
est identique à sauvegarder le jeu standard ; charger en combiné charge le
fichier dans **les deux** jeux standard et alternatif, qui deviennent donc
identiques. (Ceci diffère de V1, qui utilisait un appel ROM pour régénérer
le jeu alternatif à partir du standard au chargement -- cet appel ROM ne
fonctionne pas dans cette réécriture, donc copier les mêmes données dans
les deux jeux est l'équivalent le plus proche disponible.)

**_Information : informations de version, quitter le programme_**


![Menu Information](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Screen%20Information.png?raw=true)


*Information*

**Note OSE-LOCI** : cette option affiche une fenêtre en 3 pages au lieu de
l'écran texte unique de V1 : une image de titre/logo plein écran, une page
avec les informations de version et les crédits, et une page avec un code
QR menant à la page GitHub de ce projet. Appuyez sur une touche pour
avancer entre les pages, puis pour revenir au menu principal.

*Exit program (quitter le programme)*

**Note OSE-LOCI** : comme cette réécriture bare-metal n'a pas de
chargeur cassette sous-jacent vers lequel revenir (contrairement à
l'OricScreenEditor original en CC65), cette option réinitialise la
machine à son état de démarrage à froid. NB : aucune confirmation ne sera
demandée et le travail non sauvegardé sera perdu.

## Éditeur de caractères
([Retour au sommaire](#sommaire))

Appuyer sur **E** depuis le mode principal fait apparaître l'éditeur de caractères, qui se présente ainsi :

![Éditeur de caractères](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Chaedit.png?raw=true)

Il affiche une grille agrandie des bits du caractère actuel. L'en-tête indique le code écran du caractère actuel (en hexadécimal) et si le jeu de caractères Standard (Std) ou Alternatif (Alt) est actif. À gauche de la grille, les valeurs en octets des 8 lignes du caractère sont affichées en hexadécimal.

Commandes clavier dans ce mode :

|Touche|Description
|---|---|
|**Touches curseur**|Déplacer le curseur
|**+**|Caractère suivant (augmente le code écran)
|**-**|Caractère précédent (diminue le code écran)
|**0-9**|Sélectionner le caractère de l'emplacement favori correspondant
|**SHIFT + 0-9**|Enregistrer le caractère dans l'emplacement favori correspondant
|**ESPACE**|Basculer le pixel à la position du curseur (trace/efface le pixel)
|**DEL**|Effacer le caractère (efface tous les pixels du caractère actuel)
|**I**|**I**nverser le caractère
|**Z**|Annuler (**U**ndo) : restaure le caractère actuel à son état d'origine
|**S**|Re**s**taurer le caractère depuis le jeu de caractères système (= jeu ROM système en minuscules)
|**C**|**C**opier le caractère actuel
|**V**|Coller le caractère actuel
|**X / Y**|Mettre en miroir sur l'axe **X** ou **Y**
|**L** / **R** / **U** / **D**|Faire défiler vers la gauche (**L**eft), la droite (**R**ight), le haut (**U**p) ou le bas (**D**own)
|**H**|Saisir une valeur **H**exadécimale pour la ligne à la position du curseur
|**ESC**|Quitter le mode caractère et revenir au mode principal
|**FUNCT+6**|Basculer la visibilité de la barre d'état
|**FUNCT+8**|Écran d'aide

*Déplacer le curseur*

Appuyez sur les **touches curseur** pour déplacer le curseur dans la grille 8 par 8.

*Sélectionner le code écran à éditer*

La touche **+** ou **-** augmente, respectivement diminue, le code écran sélectionné de un. Appuyer sur A bascule le jeu de caractères utilisé entre Standard et Alternatif.

*Sélectionner le code écran à éditer depuis un emplacement favori*

Dans OSE, 10 emplacements sont disponibles pour stocker vos caractères les plus utilisés. Appuyer sur une touche **0-9** sélectionne le favori avec le numéro correspondant.

*Enregistrer le code écran actuel dans un emplacement favori*

Appuyer sur **SHIFT** plus **0-9** enregistre le caractère actuellement sélectionné dans l'emplacement favori correspondant.

*Basculer les bits dans la grille*

Appuyez sur **ESPACE** pour basculer le bit à la position actuelle du curseur. **DEL** efface tous les bits de la grille, **I** inverse tous les bits de la grille.

*Annuler et restaurer*

**U** restaure le caractère actuel à ses valeurs d'origine. Notez qu'après être passé à un autre code écran à éditer, le code écran précédent ne peut plus être restauré.

**S** copie le code écran actuel depuis la police système.

*Copier et coller*

**C** copie le code écran actuel dans une mémoire tampon, à coller en appuyant sur **V** sur un autre code écran après avoir sélectionné ce dernier.

*Miroir et défilement*

Appuyez sur **X** ou **Y** pour mettre la grille en miroir sur l'axe X, respectivement Y. **L**, **R**, **U** et **D** font défiler la grille vers la gauche (**L**eft), la droite (**R**ight), le haut (**U**p) ou le bas (**D**own).

*Saisie hexadécimale*

Appuyez sur **H** pour éditer la ligne actuelle complète de la grille en saisissant la valeur hexadécimale de l'octet représentant les bits de cette ligne.

*Quitter le mode et aide*

Appuyer sur **ESC** quitte le mode caractère et revient au mode principal. **FUNCT+8** affiche un écran d'aide avec toutes les commandes clavier du mode caractère.

## Mode palette
([Retour au sommaire](#sommaire))

Appuyer sur **P** en mode principal démarre le mode Palette. Dans ce mode, un caractère à tracer peut être sélectionné depuis le jeu de caractères, la palette complète de 121 couleurs et les 10 emplacements favoris.

Une fenêtre comme celle-ci apparaît :

![Capture d'écran du mode Palette](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Palette.png?raw=true)

La fenêtre affiche les 10 emplacements favoris sur la première ligne, en dessous le jeu de caractères standard, et en dessous encore le jeu de caractères alternatif.

Commandes clavier dans ce mode :

|Touche|Description
|---|---|
|**Touches curseur**|Déplacer le curseur
|**ESPACE ou ENTER**|Sélectionner le caractère
|**0-9**|Enregistrer le caractère dans l'emplacement favori correspondant
|**V**|Basculer entre le mode normal et le mode plan de caractères **v**isuel
|**ESC** |Quitter le mode caractère et revenir au mode principal
|**FUNCT+6**|Basculer la visibilité de la barre d'état

*Déplacer le curseur*

Appuyez sur les **touches curseur** pour déplacer le curseur dans la grille. Vous pouvez passer d'une section à l'autre simplement en sortant d'une section vers l'autre.

*Sélectionner un caractère ou une couleur*

Appuyez sur **ESPACE** ou **ENTER** pour sélectionner le caractère ou la couleur en surbrillance comme nouveau caractère ou couleur de tracé. Ceci quitte le mode palette.

*Enregistrer dans un emplacement favori*

Appuyer sur **0-9** enregistre le caractère actuellement en surbrillance dans l'emplacement favori correspondant.

*Basculer le mode plan de caractères visuel*

Le mode plan de caractères visuel est un mode dans lequel la palette du jeu de caractères alternatif est réorganisée de manière à classer les caractères dans un ordre logique pour le dessin. Ce mode n'a de sens que pour des jeux de caractères alternatifs non modifiés.

Cela ressemble à ceci :

![Palette PETSCII visuelle](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20Palette%20Visual.png?raw=true)

Appuyer sur **V** bascule entre le mode normal et le mode visuel.

*Quitter le mode et aide*

Appuyer sur **ESC** quitte le mode palette et revient au mode principal. Il n'y a pas d'écran d'aide séparé pour le mode palette.

## Sélecteur de couleurs
([Retour au sommaire](#sommaire))

Appuyer sur **C** en mode principal démarre le sélecteur de couleurs. Il s'agit d'un ajout par rapport à l'OricScreenEditor original : une grille visuelle permettant de sélectionner ensemble les couleurs d'encre et de papier à tracer, avec un aperçu de la combinaison normale et inversée résultante.

Une fenêtre comme celle-ci apparaît, montrant une grille 8x8 de toutes les combinaisons Encre/Papier, ainsi que des lignes d'information indiquant l'Encre et le Papier actuellement en surbrillance et l'aperçu normal/inversé résultant :

```
Choisir encre et papier
<grille 8x8 de cellules encre x papier, chacune montrant l'echantillon normal et inverse pour cette combinaison>
Encre:  N <echantillon>
Papier: N <echantillon>
Result: <apercu normal/inverse>
```

Commandes clavier dans ce mode :

|Touche|Description
|---|---|
|**Touches curseur**|Déplacer le curseur (GAUCHE/DROITE font défiler l'Encre, HAUT/BAS font défiler le Papier, les deux bouclent de 0 à 7)
|**ESPACE ou ENTER**|Sélectionner la combinaison Encre/Papier en surbrillance
|**ESC**|Quitter le sélecteur de couleurs et revenir au mode principal, sans modifier les couleurs Encre/Papier sélectionnées

*Déplacer le curseur*

Appuyez sur les **touches curseur** pour déplacer le curseur dans la grille 8x8. **GAUCHE**/**DROITE** font défiler la couleur d'Encre, **HAUT**/**BAS** font défiler la couleur de Papier, les deux bouclant entre les couleurs 0 et 7.

*Sélectionner l'encre et le papier*

Appuyez sur **ESPACE** ou **ENTER** pour sélectionner la combinaison Encre/Papier en surbrillance comme nouvelles couleurs de tracé. Ceci quitte le sélecteur de couleurs et revient au mode principal ; les champs I et P de la barre d'état se mettent à jour en conséquence.

*Quitter le mode*

Appuyer sur **ESC** quitte le sélecteur de couleurs sans modifier les couleurs Encre/Papier sélectionnées, et revient au mode principal.

## Mode sélection
([Retour au sommaire](#sommaire))

Appuyer sur **S** en mode principal démarre le mode Sélection.

Si elle est activée, la barre d'état affiche ceci à l'entrée dans ce mode :

![Barre d'état en mode Sélection](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20statusbar%20Select.png?raw=true)

Dans ce mode, une sélection peut être faite, sur laquelle différentes opérations peuvent être effectuées comme décrit ci-dessous.

|Touche|Description
|---|---|
|**X**|Couper (Cut) : efface la sélection à son ancienne position et la colle à la nouvelle position
|**C**|**C**opier : copie la sélection à la nouvelle position, en laissant la sélection inchangée à l'ancienne position
|**D**|Effacer (**D**elete) la sélection (remplit d'espaces)
|**I**|Peindre avec l'attribut **I**nk (encre) : trace un modificateur d'encre dans la couleur actuellement sélectionnée
|**P**|Peindre avec l'attribut **P**aper (papier) : trace un modificateur de papier dans la couleur actuellement sélectionnée
|**M**|Peindre avec l'attribut **M**odificateur de caractère : trace le modificateur de caractère avec les attributs A, D et B actuellement sélectionnés.
|**RETURN**|Accepter la sélection / accepter la nouvelle position
|**ESC** |Annuler et revenir au mode principal
|**Touches curseur**|Agrandir/réduire dans la direction sélectionnée / déplacer le curseur pour sélectionner la position de destination
|**FUNCT+6**|Basculer la visibilité de la barre d'état
|**FUNCT+8**|Écran d'aide

*Créer la sélection*

Assurez-vous que le curseur est positionné sur un coin de la sélection à créer avant d'entrer en mode Sélection. À l'entrée du mode sélection, agrandissez la sélection en appuyant sur les **touches curseur** pour augmenter ou diminuer la largeur et la hauteur dans la direction souhaitée depuis l'origine. Ceci est similaire aux touches utilisées dans le [mode Ligne et boîte](#mode-ligne-et-boîte).

La sélection est affichée visuellement en utilisant le code écran et les attributs actuellement sélectionnés. Elle devrait ressembler à ceci :

![Mode Sélection](https://github.com/xahmol/OricScreenEditor/blob/main/screenshots/OSE%20select.png?raw=true)

Acceptez la sélection en appuyant sur **RETURN**, annulez-la en appuyant sur **ESC**.

*Choisir l'action à effectuer*

Après avoir accepté la sélection, appuyez sur **X**, **C**, **D**, **A** ou **P** pour choisir une action, ou appuyez sur **ESC** pour annuler.
La barre d'état (si activée) affiche ceci comme invite :

![Barre d'état - options de sélection](https://github.com/xahmol/OricScreenEditor/raw/main/screenshots/OSE%20statusbar%20Select%20choose%20option.png)

*Couper et copier*

Après avoir appuyé sur **X** pour couper ou **C** pour copier, déplacez le curseur jusqu'au coin supérieur gauche où la sélection doit être copiée. **C** ne fait qu'une copie, **X** efface la sélection à son ancienne position.

La barre d'état (si activée) affiche Cut ou Copy en conséquence, comme ceci :

![Barre d'état - Cut ou Copy](https://github.com/xahmol/OricScreenEditor/raw/main/screenshots/OSE%20statusbar%20Select%20Copy.png)

*Effacer*

Appuyer sur **D** efface la sélection actuelle (remplit la zone sélectionnée d'espaces).

*Peindre avec un attribut ou seulement une couleur*

Appuyer sur **I**, **P** ou **M** remplit la zone avec, respectivement, la couleur d'encre, la couleur de papier ou les modificateurs de jeu de caractères avec les valeurs actuellement sélectionnées. Ceci est surtout utile en sélectionnant une colonne verticale d'un caractère de large, pour changer les attributs de toutes les lignes à droite de cette colonne.

*Quitter le mode et aide*

Quittez le mode sélection en appuyant sur **ESC**. Appuyer sur **FUNCT+8** à tout moment dans ce mode affiche un écran d'aide avec les commandes clavier de ce mode (impossible si la sélection a été agrandie mais pas encore acceptée).

**Note OSE** : Couper (**X**) et Copier (**C**) déplacent la sélection
vers une nouvelle position choisie avec les touches curseur (**ENTER**
pour confirmer, **ESC** pour annuler) -- Copier laisse l'original en
place, Couper l'efface. Si la destination dépasse les limites de la zone
de dessin, un message "Selection hors limites." apparaît et rien n'est
modifié.

## Mode déplacement
([Retour au sommaire](#sommaire))

Appuyer sur **M** en mode principal démarre le mode Déplacement. Utilisez ce mode pour faire défiler la fenêtre visible actuelle dans la direction souhaitée en appuyant sur les **touches curseur**.

Notez que le déplacement ne s'effectue que sur la partie visible actuelle de 40x25 de l'écran ; sur des zones de dessin plus grandes, l'écran entier n'est donc pas déplacé. Ceci est dû à des contraintes mémoire.

Il est également important de noter que les caractères qui « sortent » de l'écran sont perdus si le déplacement est accepté.

Une alternative au mode déplacement consiste à utiliser le [mode sélection](#mode-sélection) et la fonction Couper pour déplacer une sélection vers une nouvelle position.

Acceptez avec **RETURN**, annulez avec **ESC**. Les deux quittent ce mode et reviennent au mode principal.

**Note OSE** : dans cette réécriture basée sur LOCI, chaque déplacement est
appliqué directement dès que vous appuyez sur une touche curseur (il n'y a
pas de copie de travail séparée pour annuler) -- **RETURN** et **ESC** se
comportent donc de façon identique ici, conservant les déplacements déjà
effectués pendant cette session.

|Touche|Description
|---|---|
|**Touches curseur**|Se déplacer dans la direction sélectionnée
|**RETURN**|Accepter la position déplacée
|**ESC**|Annuler et revenir au mode principal
|**FUNCT+6**|Basculer la visibilité de la barre d'état
|**FUNCT+8**|Écran d'aide

## Mode ligne et boîte
([Retour au sommaire](#sommaire))

Appuyer sur **L** en mode principal démarre le mode Ligne et boîte. Dans ce mode, des lignes et des boîtes peuvent être dessinées, tracées avec le code écran et la valeur d'attribut actuellement sélectionnés.

Assurez-vous que le curseur est positionné sur un coin de la sélection à créer avant d'entrer en mode Sélection. À l'entrée du mode sélection, agrandissez la sélection en appuyant sur les **touches curseur** pour augmenter ou diminuer la largeur et la hauteur dans la direction souhaitée depuis l'origine. Si la largeur ou la hauteur reste à un caractère, une ligne est dessinée, sinon une boîte est dessinée.

Acceptez avec **RETURN**, annulez avec **ESC**. Les deux quittent ce mode et reviennent au mode principal.

**FUNCT+8** affiche un écran d'aide avec toutes les commandes pour ce mode.

|Touche|Description
|---|---|
|**Touches curseur**|Agrandir/réduire dans la direction sélectionnée
|**RETURN**|Accepter la ligne ou la boîte
|**ESC**|Annuler et revenir au mode principal
|**FUNCT+6**|Basculer la visibilité de la barre d'état
|**FUNCT+8**|Écran d'aide

## Mode écriture
([Retour au sommaire](#sommaire))

Appuyer sur **W** en mode principal démarre le mode Écriture. Dans ce mode, du texte peut être saisi librement en utilisant tout le clavier, ce qui rend la saisie de texte bien plus facile que de sélectionner les codes écran appropriés un par un. Tout le clavier est pris en charge, à condition que les caractères saisis soient imprimables (codes écran supérieurs à 32).

Les couleurs et attributs peuvent être tracés en mode écriture en modifiant d'abord les attributs souhaités, puis en traçant un code d'attribut :
- Appuyer sur **CTRL+Z** ou **CTRL+X** diminue, respectivement augmente, la couleur d'encre sélectionnée
- Appuyer sur **CTRL+C** ou **CTRL+V** diminue, respectivement augmente, la couleur de papier sélectionnée
- Appuyer sur **CTRL+B**, **CTRL+A** ou **CTRL+D** bascule le jeu alternatif, le double ou le clignotement
- Appuyer sur **CTRL+R** bascule le mode inversé
- Appuyer sur **FUNCT+1** trace l'encre
- Appuyer sur **FUNCT+2** trace le papier
- Appuyer sur **FUNCT+3** trace le modificateur de caractère.

Quittez le mode Écriture en appuyant sur **ESC**. **FUNCT+8** affiche un écran d'aide avec les commandes clavier de ce mode.

|Touche|Description
|---|---|
|**Touches curseur**|Se déplacer dans la direction sélectionnée
|**DEL**|Effacer la position actuelle du curseur (trace un espace blanc)
|**CTRL+A**|Bascule l'attribut jeu de caractères alternatif
|**CTRL+B**|Bascule l'attribut clignotement
|**CTRL+D**|Bascule l'attribut double
|**CTRL+R**|Bascule l'inversion
|**CTRL+Z**|Diminue la couleur d'encre
|**CTRL+X**|Augmente la couleur d'encre
|**CTRL+C**|Diminue la couleur de papier
|**CTRL+V**|Augmente la couleur de papier
|**FUNCT+1**|Trace l'encre
|**FUNCT+2**|Trace le papier
|**FUNCT+3**|Trace le modificateur de caractère
|**ESC** |Revenir au mode principal
|**FUNCT+6**|Basculer la visibilité de la barre d'état
|**FUNCT+8**|Écran d'aide
|**Autres touches**|Trace le caractère correspondant (si imprimable)

## Référence des valeurs de couleur
([Retour au sommaire](#sommaire))

Vous trouverez ci-dessous la liste des 8 valeurs de couleur.

Les valeurs sont calculées à partir des 3 bits utilisés pour le rouge, le vert et le bleu (RGB) :
- +1 pour le rouge
- +2 pour le vert
- +4 pour le bleu

|Numéro|Couleur|B-V-R|
|---|---|---|
|0|Noir|0-0-0|
|1|Rouge|0-0-1|
|2|Vert|0-1-0|
|3|Jaune|0-1-1|
|4|Bleu|1-0-0|
|5|Magenta|1-0-1|
|6|Cyan|1-1-0|
|7|Blanc|1-1-1|

## Référence des codes d'attribut série
([Retour au sommaire](#sommaire))

L'Oric ne dispose pas d'un espace mémoire d'attributs séparé : changer un attribut se fait en traçant un code d'attribut à l'endroit où se trouverait normalement un caractère, l'effet de cet attribut étant valable pour le reste de la ligne jusqu'à ce qu'un autre code d'attribut le remplace plus loin sur la même ligne.
Également, un code d'attribut peut soit changer l'encre, soit changer le papier, soit changer les modificateurs de jeu de caractères, soit changer les attributs de contrôle vidéo, mais jamais une combinaison de ces quatre catégories à la fois. Si vous souhaitez changer deux ou plusieurs de ces quatre catégories, vous devez tracer le même nombre de codes d'attribut les uns après les autres. C'est pour cela qu'on les appelle des attributs sériels.
Cela complique assez la conception d'écrans multicolores, car chaque changement de couleur coûte un emplacement où aucun caractère normal ne peut être placé.

Dans Oric Screen Editor, tous les attributs sauf les attributs de contrôle vidéo sont pris en charge. Mais OSE n'a pas connaissance des attributs que vous avez placés dans la ligne, donc le placement correct des attributs est de la responsabilité de l'utilisateur lors de la conception.

Les codes d'attribut sont tous des codes de tracé de 0 à 31, les codes de 32 à 127 sont les caractères imprimables selon les codes ASCII standards, les codes à partir de 128 sont les mêmes mais en vidéo inversée.

Pour le contexte complet et la référence :
https://osdk.org/index.php?page=articles&ref=ART9

Vue d'ensemble des codes d'attribut possibles :

*Codes 0-7 : changer l'encre*

Pour changer la couleur de l'encre, les codes sont les numéros de couleur de base mentionnés ci-dessus dans la [Référence des valeurs de couleur](#référence-des-valeurs-de-couleur), c'est-à-dire simplement en positionnant les bits 0, 1 et 2 pour la valeur RGB, les bits 3 à 7 étant à zéro.

Motif de bits :

|Bit|7-6-5|4|3|2|1|0|
|---|---|---|---|---|---|---|
|Signification|0|0|0|Bleu|Vert|Rouge|


|Code|Hex|0-0-0-P-C-B-G-R|Couleur d'encre|
|---|---|---|---|
|00|00|0-0-0-0-0-0-0-0|Noir|
|01|01|0-0-0-0-0-0-0-1|Rouge|
|02|02|0-0-0-0-0-0-1-0|Vert|
|03|03|0-0-0-0-0-0-1-1|Jaune|
|04|04|0-0-0-0-0-1-0-0|Bleu|
|05|05|0-0-0-0-0-1-0-1|Magenta|
|06|06|0-0-0-0-0-1-1-0|Cyan|
|07|07|0-0-0-0-0-1-1-1|Blanc|

*Codes 8-15 : modificateur de jeu de caractères*

Avec le bit 3 activé (donc +8), les bits 0, 1 et 2 sont utilisés pour modifier le comportement du jeu de caractères. Les différents comportements du jeu de caractères peuvent être définis en une seule fois avec un seul code.

Motif de bits :

|Bit|7-6-5|4|3|2|1|0|
|---|---|---|---|---|---|---|
|Signification|0|0|Modificateur de jeu activé|Clignotement activé|Taille double activée|Alternatif activé|

|Code|Hex|0-0-0-P-C-B-D-A|Effet sur le jeu de caractères|
|---|---|---|---|
|08|08|0-0-0-1-0-0-0-0|Utilise le jeu de caractères standard|
|09|09|0-0-0-1-0-0-0-1|Utilise le jeu de caractères alternatif|
|10|0A|0-0-0-1-0-0-1-0|Utilise le jeu standard en taille double|
|11|0B|0-0-0-1-0-0-1-1|Utilise le jeu alternatif en taille double|
|12|0C|0-0-0-1-0-1-0-0|Utilise le jeu standard clignotant|
|13|0D|0-0-0-1-0-1-0-1|Utilise le jeu alternatif clignotant|
|14|0E|0-0-0-1-0-1-1-0|Utilise le jeu standard en taille double clignotant|
|15|0F|0-0-0-1-0-1-1-1|Utilise le jeu alternatif en taille double clignotant|

*Codes 16-23 : changer le papier*

Pour changer la couleur du papier, les codes sont les numéros de couleur de base mentionnés ci-dessus dans la [Référence des valeurs de couleur](#référence-des-valeurs-de-couleur), c'est-à-dire simplement en positionnant les bits 0, 1 et 2 pour la valeur RGB, en plus du bit 4 (donc +16). Le bit 3, et les bits 5, 6 et 7 doivent être à 0.

Motif de bits :

|Bit|7-6-5|4|3|2|1|0|
|---|---|---|---|---|---|---|
|Signification|0|Modificateur de papier activé|0|Bleu|Vert|Rouge|

|Code|Hex|0-0-0-P-C-B-G-R|Couleur de papier|
|---|---|---|---|
|16|10|0-0-0-1-0-0-0-0|Noir|
|17|11|0-0-0-1-0-0-0-1|Rouge|
|18|12|0-0-0-1-0-0-1-0|Vert|
|19|13|0-0-0-1-0-0-1-1|Jaune|
|20|14|0-0-0-1-0-1-0-0|Bleu|
|21|15|0-0-0-1-0-1-0-1|Magenta|
|22|16|0-0-0-1-0-1-1-0|Cyan|
|23|17|0-0-0-1-0-1-1-1|Blanc|

Notez que dans OSE, il n'est pas nécessaire de calculer ces codes d'attribut vous-même : le programme le fait pour vous en fonction des attributs et de la couleur sélectionnés. En mémoire, cependant, c'est ainsi que les codes sont stockés.

## Référence du format de fichier
([Retour au sommaire](#sommaire))

Comme l'Oric ne possède pas de mémoire d'attributs séparée, les données d'écran sont essentiellement un simple dump largeur*hauteur de codes écran. Le fichier d'écran est un fichier de données brutes contenant ces codes écran, sa longueur étant calculée comme largeur*hauteur.
Ainsi, un écran standard 40x27 ferait 1 080 octets.

## Crédits
([Retour au sommaire](#sommaire))

Oric Screen Editor

Éditeur d'écran pour l'Oric Atmos

Écrit en 2022 par Xander Mol

Basé sur VDC Screen Editor pour le C128

https://github.com/xahmol/OricScreenEditor

https://www.idreamtin8bits.com/

Code et ressources d'autres personnes utilisés :

-   Compilateur croisé CC65 :

    https://cc65.github.io/

-   6502.org : Practical Memory Move Routines : point de départ pour les routines de déplacement mémoire

    http://6502.org/source/general/memory_move.html

-   Code source de DraBrowse pour la commande DOS et la routine de saisie de texte

    DraBrowse (db*) est un simple navigateur de fichiers.
    Créé à l'origine en 2009 par Sascha Bader.
    Version utilisée adaptée par Dirk Jagdmann (doj)
    https://github.com/doj/dracopy

-   lib-sedoric de oricOpenLibrary (pour les opérations sur fichiers SEDORIC)
    Par Raxiss, (c) 2021
    https://github.com/iss000/oricOpenLibrary/blob/main/lib-sedoric/libsedoric.s

-   Bart van Leeuwen et forum.defence-force.org : pour l'inspiration et les conseils pendant le développement.

-   jab / Artline Designs (Jaakko Luoto) pour l'inspiration du mode Palette et du mode visuel PETSCII

-   Code original du système de fenêtrage sur Commodore 128, par un auteur inconnu.

-   Testé sur matériel réel Oric Atmos avec Cumana Reborn, et avec Oricutron pour Windows et Linux

Le code peut être utilisé librement à condition de conserver
une mention décrivant la source et l'auteur d'origine.

LES PROGRAMMES SONT DISTRIBUÉS DANS L'ESPOIR QU'ILS SERONT UTILES,
MAIS SANS AUCUNE GARANTIE. UTILISEZ-LES À VOS PROPRES RISQUES !

([Retour au sommaire](#sommaire))
