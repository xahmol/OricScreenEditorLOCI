# OricScreenEditorLOCI — Manuel de référence des bibliothèques

Bibliothèques Oscar64 bare-metal pour l'Oric Atmos.
Cible : 6502A, 1 MHz, sans appels ROM.

Adapté du `libmanual.md` de locifilemanager-v2 (référence locale
`/home/xahmol/git/locifilemanager-v2/libmanual.md`) — les §1 à 5 couvrent les
bibliothèques partagées textuellement entre les deux projets (`oric.h`,
`keyboard.h`, `charwin.h`, `ijk.h`, `loci.h`), avec des notes là où l'usage
d'OSE diffère. Le §6 (`include/charset.h`) est nouveau — une bibliothèque
générique de banques de jeux de caractères extraite pendant le développement
d'OSE. Le §7 (`menu.h`) est réécrit pour le système de menus d'OSE, plus
compact et basé sur la RAM principale. Le §8 (notes de compilation) est
adapté pour le Makefile d'OSE.

---

## Sommaire

1. [Vue d'ensemble matérielle (oric.h)](#1-vue-densemble-matérielle-orich)
2. [Lecteur de clavier (keyboard.h)](#2-lecteur-de-clavier-keyboardh)
3. [Bibliothèque de fenêtres (charwin.h)](#3-bibliothèque-de-fenêtres-charwinh)
4. [Manette IJK (ijk.h)](#4-manette-ijk-ijkh)
5. [API stockage de masse LOCI (loci.h)](#5-api-stockage-de-masse-loci-locih)
6. [Bibliothèque générique de jeux de caractères (include/charset.h)](#6-bibliothèque-générique-de-jeux-de-caractères-includecharseth)
7. [Système de menus (menu.h)](#7-système-de-menus-menuh)
8. [Notes de compilation](#8-notes-de-compilation)

---

## 1. Vue d'ensemble matérielle (oric.h)

Inclure `oric.h` dans tout fichier qui référence des registres matériels,
des constantes d'attributs ou des macros de disposition d'écran.

### Écran

| Symbole | Valeur | Signification |
|---|---|---|
| `TEXTVRAM` | `0xBB80` | Adresse de base de la RAM vidéo texte |
| `SCREEN_COLS` | `40` | Colonnes par ligne |
| `SCREEN_ROWS` | `28` | Lignes |
| `SCREEN_SIZE` | `1120` | Octets totaux (40 × 28) |

L'ULA de l'Oric traite la RAM vidéo de gauche à droite sur chaque ligne de
balayage. Un octet dont `(octet & 0x60) == 0` (valeurs `0x00–0x1F`) est un
**attribut série** qui modifie la couleur de l'encre, du papier ou le jeu de
caractères pour le reste de la ligne. Tous les autres octets sont des codes
de caractères. L'ULA remet l'encre en blanc et le papier en noir au début de
chaque ligne de balayage.

**Conséquence :** chaque attribut occupe une colonne et décale tous les
caractères suivants d'une position vers la droite.

### Constantes de couleur d'encre (avant-plan)

`A_FWBLACK` (0) · `A_FWRED` (1) · `A_FWGREEN` (2) · `A_FWYELLOW` (3)
`A_FWBLUE` (4) · `A_FWMAGENTA` (5) · `A_FWCYAN` (6) · `A_FWWHITE` (7)

**Attention :** `A_FWBLACK = 0x00` est le terminateur NUL en C. Il ne peut
pas être intégré dans un littéral de chaîne. Écrivez-le avec
`cwin_put_attr(&w, A_FWBLACK)`.

### Constantes de couleur de papier (arrière-plan)

`A_BGBLACK` (16) · `A_BGRED` (17) · `A_BGGREEN` (18) · `A_BGYELLOW` (19)
`A_BGBLUE` (20) · `A_BGMAGENTA` (21) · `A_BGCYAN` (22) · `A_BGWHITE` (23)

### Constantes de mode du jeu de caractères

| Constante | Valeur | Effet |
|---|---|---|
| `A_STD` | 8 | Jeu de caractères standard |
| `A_ALT` | 9 | Jeu alternatif (mosaïque/graphique) |
| `A_STD2H` | 10 | Double hauteur standard |
| `A_ALT2H` | 11 | Double hauteur alternatif |
| `A_BLINKSTD` | 12 | Clignotant standard |
| `A_BLINKALT` | 13 | Clignotant alternatif |
| `A_BLINK2H` | 14 | Double hauteur clignotant |
| `A_BLINK2HALT` | 15 | Double hauteur clignotant alternatif |

### Codes de caractères spéciaux

| Constante | Valeur | Caractère |
|---|---|---|
| `CH_SPACE` | `0x20` | Espace — sûr pour effacer |
| `CH_INVSPACE` | `0xA0` | Bloc plein couleur encre (curseur, animation) |
| `CH_POUND` | `0x5F` | Signe £ (la ROM map la position ASCII `_`) |
| `CH_COPYRIGHT` | `0x60` | Signe © (la ROM map la position ASCII `` ` ``) |

Évitez `_` et `` ` `` dans les chaînes affichées ; ils produisent £ et ©.

### Jeux de caractères en RAM (CHARSET_STD / CHARSET_ALT / CHARSETROM)

Ajout spécifique à OSE dans `oric.h` (absent de la copie de
locifilemanager-v2). L'ULA de l'Oric dessine les glyphes texte depuis de la
**RAM vive**, pas depuis la ROM — redéfinir les octets ci-dessous prend effet
dès la ligne de balayage suivante, sans traitement particulier.

| Symbole | Valeur | Signification |
|---|---|---|
| `CHARSET_STD` | `0xB400` | Base de la banque du jeu standard (codes `0x00`-`0x7F`) |
| `CHARSET_ALT` | `0xB800` | Base de la banque du jeu alternatif (codes `0x00`-`0x7F`) |
| `CHARSETROM` | `0xFC78` | Source ROM du jeu standard (96 codes imprimables seulement, voir ci-dessous) |

Chaque banque fait 1024 octets = 128 glyphes (codes `0x00`-`0x7F`) × 8 octets
(un par ligne de pixels) ; les bits 5..0 de chaque octet sont les pixels de
gauche à droite (bit5 = pixel le plus à gauche des 6 colonnes visibles — les
caractères Oric font 6×8).

**Deux conventions d'adressage différentes :**

- `CHARSET_STD`/`CHARSET_ALT` : banques complètes de 128 glyphes, adresse du
  glyphe = `base + code*8`, **sans décalage**. Les codes `0x00-0x1F` sont
  présents en RAM mais jamais affichés (ces valeurs d'octets en RAM écran
  déclenchent le mode attribut au lieu d'une recherche de glyphe).
- `CHARSETROM` : seulement les 96 codes imprimables `0x20-0x7F` (768 octets),
  adresse du glyphe = `CHARSETROM + (code-0x20)*8` — c'est-à-dire **avec** un
  décalage de `-0x20`. C'est la source pour la commande « restaurer le jeu
  standard » (jeu standard uniquement — voir le mécanisme d'échange de jeu de
  caractères au §6).

`include/charset.h` (§6 ci-dessous) fournit des primitives
d'adressage/copie et des transformations de bitmap de glyphes sur cette
disposition, utilisées par l'éditeur de caractères (`src/charsetedit.c`) et
le mécanisme d'échange de jeu de caractères des popups
(`src/charsetswap.c`).

### Macros d'attributs inline (ASTR_*)

Intégrez des changements de couleur ou de jeu de caractères directement dans
les littéraux de chaîne :

```c
cwin_putat_string(&w, 2, 5,
    ASTR_INK_RED "alerte" ASTR_INK_WHITE " normal");
```

Chaque macro `ASTR_*` se développe en une séquence d'un octet. Macros
disponibles :

- Encre : `ASTR_INK_RED` · `_GREEN` · `_YELLOW` · `_BLUE` · `_MAGENTA` · `_CYAN` · `_WHITE`
- Papier : `ASTR_PAPER_BLACK` · `_RED` · `_GREEN` · `_YELLOW` · `_BLUE` · `_MAGENTA` · `_CYAN` · `_WHITE`
- Jeu de caract. : `ASTR_CHARSET_STD` · `_ALT` · `_STD2H` · `_ALT2H` · `_BLKSTD` · `_BLKALT` · `_BLK2H` · `_BLK2HA`

**Contraintes :**
- `ASTR_INK_BLACK` (`\x00`) = NUL — ne peut pas figurer dans un littéral de chaîne C.
  Utiliser `cwin_put_attr(&w, A_FWBLACK)`.
- `ASTR_CHARSET_STD2H` (`\x0A`) = `\n` — provoque un défilement dans
  `cwin_console_put_string`. Utiliser `cwin_put_attr` en mode console.

### RAM overlay

La RAM overlay (`0xC000–0xFFFF`) est normalement la ROM Atmos. Lorsque LOCI
est connecté, écrire dans le registre compatible Microdisc en `0x0314`
active la RAM en dessous de la ROM.

```c
#define MICRODISCCFG    (*((volatile uint8_t *)0x0314))
#define OVERLAY_ON      0xFD   // activer la RAM overlay
#define OVERLAY_OFF     0xFF   // restaurer la ROM
#define OVERLAY_BASE    0xC000U
#define OVERLAY_SIZE    0x4000U
```

**Nécessite LOCI.** Non testable sous Oricutron. Les fonctions de haut niveau
`enable_overlay_ram()` et `disable_overlay_ram()` de `loci.h` (§5) sont
l'interface recommandée — actuellement inutilisées dans OSE (voir la note du
§5).

---

## 2. Lecteur de clavier (keyboard.h)

Lecture directe du matériel VIA/AY — sans appels ROM. Inclure `keyboard.h`.

### Codes de touches

| Constante | Valeur | Touche |
|---|---|---|
| `KEY_NONE` | `0x00` | Aucune touche |
| `KEY_ENTER` | `0x0D` | Entrée |
| `KEY_ESC` | `0x1B` | Échap |
| `KEY_DEL` | `0x7F` | Suppr / Retour arrière |
| `KEY_UP` | `0x0B` | Curseur haut |
| `KEY_DOWN` | `0x0A` | Curseur bas |
| `KEY_LEFT` | `0x08` | Curseur gauche |
| `KEY_RIGHT` | `0x09` | Curseur droite |
| `KEY_SPACE` | `0x20` | Espace |
| `KEY_F1`–`KEY_F9` | `0xB1`–`0xB9` | FUNCT + 1–9 |
| `KEY_F10` | `0xB0` | FUNCT + 0 |

CTRL + lettre produit `lettre & 0x1F`. Les constantes `KEY_CTRL_A` (1),
`KEY_CTRL_C` (3), `KEY_CTRL_X` (24), `KEY_CTRL_Z` (26) sont définies.

`KEY_TAB` (`0x09`) a la même valeur que `KEY_RIGHT` — dépendant du contexte.

### Drapeaux de modificateurs (keyb_modifiers)

| Constante | Bit | Signification |
|---|---|---|
| `MOD_SHIFT` | `0x01` | Shift enfoncé |
| `MOD_CTRL` | `0x02` | Ctrl enfoncé |
| `MOD_FUNCT` | `0x04` | FUNCT enfoncé (Atmos) |
| `MOD_CAPSLOCK` | `0x08` | Verrouillage majuscules actif |

`keyb_modifiers` est mis à jour par `keyb_poll()`.

### État global

```c
extern uint8_t  keyb_matrix[8];       // matrice brute 8×8
extern volatile uint8_t keyb_char;    // dernière touche décodée
extern uint8_t  keyb_modifiers;       // drapeaux modificateurs courants
```

### Fonctions

```c
void keyb_scan(void);
```
Lit la matrice complète 8×8 dans `keyb_matrix[]`. Durée ~1–2 ms à 1 MHz.
À appeler depuis une boucle de scrutation ou une IRQ de minuterie.

```c
uint8_t keyb_decode(void);
```
Décode `keyb_matrix[]` dans `keyb_char` et `keyb_modifiers`. Gère Shift,
Ctrl, FUNCT, verrouillage majuscules. Retourne le code ASCII décodé
(0 = aucune touche).

```c
uint8_t keyb_poll(void);
```
Lecture + décodage + répétition automatique. Met à jour `keyb_char`.
Retourne le code de touche (0 = aucune touche). La répétition se déclenche
après ~400 ms puis toutes les ~100 ms.
**Utiliser comme fonction principale de scrutation.**

```c
uint8_t keyb_getch(void);
```
Bloquant : boucle sur `keyb_poll()` jusqu'à ce qu'une touche soit pressée.
Retourne le code de touche.

```c
uint8_t keyb_check(void);
```
Vérifie si une touche est enfoncée sans la consommer. Retourne une valeur
non nulle si une touche est tenue.

### Délai d'animation sans sleep

`keyb_scan()` dure ~370 cycles à 1 MHz. À utiliser comme unité de délai :

```c
for (uint8_t d = 0; d < 80; d++) keyb_scan();   // ≈ 30 ms
```

Les accès aux registres matériels empêchent l'optimiseur Oscar64 de
supprimer la boucle.

---

## 3. Bibliothèque de fenêtres (charwin.h)

Bibliothèque d'affichage en fenêtres caractères pour l'écran texte Oric
40×28. Toutes les fonctions écrivent directement en RAM vidéo — sans appels
ROM.

Inclure `charwin.h`. La bibliothèque compile automatiquement `charwin.c`
via `#pragma compile`.

### Modèle de fenêtre

Chaque fenêtre est décrite par une structure `OricCharWin` :

```c
typedef struct {
    uint8_t sx, sy;   // coin supérieur gauche (sx >= 2)
    uint8_t wx, wy;   // largeur et hauteur en caractères
    uint8_t cx, cy;   // position du curseur dans la fenêtre (base 0)
    uint8_t ink;      // couleur de l'encre (constante A_FW*)
    uint8_t paper;    // couleur du papier (constante A_BG*)
} OricCharWin;
```

**Disposition des colonnes :** `cwin_clear` écrit l'attribut d'encre en
colonne 0 et l'attribut de papier en colonne 1 pour chaque ligne gérée.
Le contenu occupe les colonnes `sx` à `sx + wx - 1`. La valeur minimale
de `sx` est 2 pour laisser de la place aux deux colonnes d'attributs.

### Initialisation

```c
void charwin_init(void);
```
Construit la table de correspondance des adresses de lignes. **Appeler une
fois avant toute fonction `cwin_*`.**

```c
void cwin_init(OricCharWin *w,
               uint8_t sx, uint8_t sy,
               uint8_t wx, uint8_t wy,
               uint8_t ink, uint8_t paper);
```
Remplit une structure de fenêtre. Impose `sx >= 2`. N'affiche rien.

### Effacement et remplissage

```c
void cwin_clear(OricCharWin *w);
```
Remplit le contenu d'espaces (`0x20`). Écrit l'attribut d'encre en colonne 0
et l'attribut de papier en colonne 1 pour chaque ligne. Remet le curseur
en (0, 0).

```c
void cwin_fill_rect(OricCharWin *w,
                    uint8_t x, uint8_t y,
                    uint8_t bw, uint8_t bh,
                    uint8_t ch);
```
Remplit un rectangle `bw × bh` en position `(x, y)` relative à la fenêtre
avec le caractère `ch`. Clippe aux bords de la fenêtre.

### Écriture positionnelle (sans mise à jour du curseur)

```c
void cwin_putat_char(OricCharWin *w, uint8_t x, uint8_t y, uint8_t ch);
```
Écrit un caractère en position relative `(x, y)`.

```c
uint8_t cwin_getat_char(OricCharWin *w, uint8_t x, uint8_t y);
```
Lit le caractère en position relative `(x, y)` depuis la RAM vidéo.

```c
void cwin_putat_string(OricCharWin *w, uint8_t x, uint8_t y, const char *s);
```
Écrit une chaîne terminée par NUL à partir de `(x, y)`. Clippe au bord droit.
Les octets d'attributs intégrés via les macros `ASTR_*` sont écrits tels
quels (chacun occupe une colonne et décale le texte suivant d'une position).

```c
void cwin_putat_dblhi_string(OricCharWin *w, uint8_t x, uint8_t y, const char *s);
```
Écrit `s` en double hauteur en `(x, y)`. Émet `A_STD2H`, la chaîne, puis
`A_STD` sur **les deux** lignes `y` et `y+1` en un seul appel (le matériel
produit la moitié supérieure sur `y`, la moitié inférieure sur `y+1`).
Nécessite `y + 1 < w->wy`.

```c
void cwin_putat_printf(OricCharWin *w, uint8_t x, uint8_t y,
                       const char *fmt, ...);
```
Écriture formatée en `(x, y)`. Clippe au bord droit. Spécificateurs
supportés : `%d` (int16), `%u` (uint16), `%x` (hex uint16 majuscule),
`%s`, `%c`, `%%`, avec largeur et remplissage zéro (ex. `%04u`). Pas de
flottants (`-dNOFLOAT`).

### Écriture avec avancement du curseur

```c
void cwin_put_char(OricCharWin *w, uint8_t ch);
void cwin_put_string(OricCharWin *w, const char *s);
void cwin_put_attr(OricCharWin *w, uint8_t attr);
void cwin_printf(OricCharWin *w, const char *fmt, ...);
```
Écriture à la position du curseur avec avancement. Sans saut de ligne ni
défilement. `cwin_put_attr` est la méthode correcte pour écrire `A_FWBLACK`
(0x00) qui ne peut pas figurer dans un littéral de chaîne C.

### Mode console (saut de ligne + défilement)

```c
void cwin_console_put_char(OricCharWin *w, uint8_t ch);
void cwin_console_put_string(OricCharWin *w, const char *s);
```
Écriture en mode console : `\n` avance le curseur au début de la ligne
suivante en faisant défiler la fenêtre si le curseur est déjà sur la dernière
ligne. Les autres caractères se replient au bord droit.

**Attention :** `ASTR_CHARSET_STD2H` (`\x0A`) est égal à `\n` et provoque
un défilement. Utiliser `cwin_put_attr` en mode console.

### Défilement

```c
void cwin_scroll_up(OricCharWin *w);
```
Fait défiler le contenu d'une ligne vers le haut. La nouvelle ligne du bas
est remplie d'espaces et ses attributs sont rafraîchis.

```c
void cwin_scroll_down(OricCharWin *w);
```
Fait défiler le contenu d'une ligne vers le bas. La nouvelle ligne du haut
est remplie d'espaces et ses attributs sont rafraîchis.

### Insertion et suppression

```c
void cwin_insert_char(OricCharWin *w);
```
Insère un espace à la position du curseur en décalant le reste de la ligne
vers la droite. Le caractère en colonne `wx - 1` est perdu. Le curseur ne
bouge pas.

```c
void cwin_delete_char(OricCharWin *w);
```
Supprime le caractère à la position du curseur en décalant le reste de la
ligne vers la gauche. La colonne `wx - 1` est remplie d'un espace. Le
curseur ne bouge pas.

### Impression de ligne

```c
void cwin_printline(OricCharWin *w, const char *s);
```
Écrit `s` à la position du curseur, puis avance le curseur au début de la
ligne suivante. Fait défiler si on est sur la dernière ligne. À combiner
avec `cwin_putat_printf` pour afficher des lignes numérotées avec
défilement automatique :

```c
uint8_t cy = w.cy;   // variable temporaire — évite le gotcha Oscar64
cwin_putat_printf(&w, 0, cy, "Ligne %u", (uint16_t)i);
cwin_printline(&w, "");
```

### Curseur

```c
void cwin_cursor_show(OricCharWin *w, bool on);
```
Affiche (`on = true`) ou masque le curseur à la position courante par
inversion vidéo. L'appelant doit mémoriser l'état affiché/masqué pour éviter
le double basculement.

```c
bool cwin_cursor_left(OricCharWin *w);
bool cwin_cursor_right(OricCharWin *w);
bool cwin_cursor_up(OricCharWin *w);
bool cwin_cursor_down(OricCharWin *w);
```
Déplace le curseur d'une position. Retourne `true` si déplacé, `false` si
déjà au bord de la fenêtre.

### Widget de saisie de texte

```c
signed int cwin_textinput(OricCharWin *w,
                          uint8_t x, uint8_t y,
                          uint8_t vwidth,
                          char *str, uint8_t maxlen,
                          uint8_t validation);
```
Saisie de texte interactive sur une ligne en position relative `(x, y)`.

| Paramètre | Signification |
|---|---|
| `vwidth` | Largeur visible ; `vwidth < maxlen` active le défilement horizontal |
| `str` | Tampon pré-initialisé d'au moins `maxlen + 1` octets |
| `maxlen` | Longueur maximale de la chaîne (sans NUL) |
| `validation` | Filtre de saisie : `VINPUT_ALL` (0), `VINPUT_NUMS` (1), `VINPUT_ALPHA` (2), `VINPUT_WILD` (4) |

Retourne la longueur de la chaîne sur ENTRÉE, ou `-1` sur ÉCHAP.

Indicateurs de validation :

| Constante | Valeur | Caractères autorisés |
|---|---|---|
| `VINPUT_ALL` | `0` | Tous les caractères imprimables |
| `VINPUT_NUMS` | `1` | Chiffres 0–9 uniquement |
| `VINPUT_ALPHA` | `2` | Lettres + chiffres |
| `VINPUT_WILD` | `4` | Ajoute `*` et `?` à toute combinaison |

### Viewport — vue défilante dans une carte source

```c
typedef struct {
    uint8_t     *sourcebase;    // carte de caractères source (tableau plat)
    uint16_t     sourcewidth;   // octets par ligne dans la carte (>= win->wx)
    uint16_t     sourceheight;  // nombre total de lignes dans la carte
    uint16_t     viewx;         // décalage horizontal courant
    uint16_t     viewy;         // décalage vertical courant
    OricCharWin *win;           // fenêtre d'affichage cible
} OricViewport;
```

La carte source est un tableau plat : `carte[ligne * sourcewidth + col]`.
Seuls les octets de caractères sont stockés — les attributs proviennent des
champs `ink` et `paper` de la fenêtre.

**Important :** Les grandes cartes sources doivent être déclarées `static`.
Une carte de 60 × 20 = 1200 octets comme variable locale dépasserait la pile
6502 de seulement 256 octets.

```c
void cwin_viewport_init(OricViewport *vp,
                        uint8_t *sourcebase,
                        uint16_t sourcewidth, uint16_t sourceheight,
                        OricCharWin *win);
```
Initialise le viewport. Met `viewx = viewy = 0`. N'affiche rien.

```c
void cwin_viewport_blit(OricViewport *vp);
```
Copie la région de vue courante dans la fenêtre d'affichage, en incluant les
attributs d'encre/papier sur chaque ligne.

```c
void cwin_viewport_scroll(OricViewport *vp, uint8_t dir);
```
Fait défiler d'une unité dans la direction `dir` (`KEY_UP`, `KEY_DOWN`,
`KEY_LEFT`, `KEY_RIGHT`). Clampe aux bords de la carte, puis appelle
`cwin_viewport_blit`.

Exemple de boucle de défilement interactive :

```c
static uint8_t carte[20 * 60];   // static — 1200 octets, ne pas mettre sur la pile
// ... remplir la carte ...
OricCharWin vpwin;
OricViewport vp;
cwin_init(&vpwin, 2, 3, 34, 10, A_FWWHITE, A_BGBLUE);
cwin_viewport_init(&vp, carte, 60, 20, &vpwin);
cwin_viewport_blit(&vp);

uint8_t k;
while ((k = keyb_poll()) != KEY_ESC) {
    if (k == KEY_UP || k == KEY_DOWN || k == KEY_LEFT || k == KEY_RIGHT)
        cwin_viewport_scroll(&vp, k);
}
```

> **Note OSE :** le canevas 40×27 (`src/canvas.c`) n'utilise délibérément
> **pas** `OricViewport`/`cwin_viewport_blit` — voir `ARCHITECTURE.md` §2.6
> pour le pourquoi (cela écraserait les octets d'attribut série arbitraires
> que l'utilisateur place dans le canevas). `OricViewport` reste correct pour
> toute future sous-vue défilante (par ex. un navigateur de fichiers).

### Sauvegarde/restauration par RAM overlay

```c
void cwin_push(OricCharWin *w);
void cwin_pop(OricCharWin *w);
```
Sauvegarde (`push`) et restaure (`pop`) les lignes couvertes par la fenêtre
`w` en utilisant la RAM overlay. Jusqu'à 8 niveaux (pile LIFO). **Nécessite
le périphérique LOCI** — non testable sous Oricutron ; se dégrade
silencieusement en l'absence de LOCI.

> **Note OSE :** le système de menus d'OSE (§7) n'utilise **pas**
> `cwin_push`/`cwin_pop` — il a sa propre pile de sauvegarde de fenêtres en
> RAM principale (`menu_winsave`/`menu_winrestore`), si bien que les popups
> se referment proprement sous Oricutron sans LOCI. `cwin_push`/`cwin_pop`
> restent disponibles dans `charwin.h` pour un futur usage de la RAM overlay.

### Lecture de touche

```c
uint8_t cwin_getch(void);
```
Lecture bloquante (délègue à `keyb_getch`).

---

## 4. Manette IJK (ijk.h)

> **Présent dans `include/`, pas encore câblé dans OSE.** `ijk.h/c` ont été
> copiés textuellement depuis locifilemanager-v2 pour une phase ultérieure
> de méthodes de saisie ; aucun fichier de `src/` n'inclut actuellement
> `ijk.h`, donc ce code n'est pas encore compilé dans `build/oseloci*.tap`
> (voir `ARCHITECTURE.md` §2.4). Documenté ici pour le moment où il sera
> câblé.

Interface Raxiss IJK utilisant le Port A VIA (`$030F`) et le bit 4 du Port B.
Inclure `ijk.h` séparément de `charwin.h` — ce n'est pas une dépendance de
la bibliothèque de fenêtres.

L'interface IJK partage le Port A VIA avec le lecteur de clavier. `ijk_read`
encadre tous les accès au Port A avec SEI/CLI pour éviter les conflits.

### Masques de bits de direction

| Constante | Valeur | Direction |
|---|---|---|
| `IJK_JOY_RIGHT` | `0x01` | Droite |
| `IJK_JOY_LEFT` | `0x02` | Gauche |
| `IJK_JOY_FIRE` | `0x04` | Bouton tir |
| `IJK_JOY_DOWN` | `0x08` | Bas |
| `IJK_JOY_UP` | `0x10` | Haut |

Un bit est à **1** quand la direction est pressée (actif haut après inversion).

### État global

```c
extern uint8_t ijk_present;  // non nul si IJK détecté
extern uint8_t ijk_ljoy;     // état de la manette gauche
extern uint8_t ijk_rjoy;     // état de la manette droite
```

### Fonctions

```c
void ijk_detect(void);
```
Sonde le matériel pour détecter une interface IJK. Met `ijk_present` à une
valeur non nulle si trouvée. À appeler une fois au démarrage (ou déléguer à
`menu_init`, qui l'appelle dans locifilemanager-v2 — le `menu_init` d'OSE,
§7, ne le fait pas).

```c
void ijk_read(void);
```
Lit les deux manettes dans `ijk_ljoy` et `ijk_rjoy`. Ne fait rien si
`ijk_present` vaut zéro.

### Utilisation typique dans une boucle menu/saisie

```c
ijk_detect();
// ...
uint8_t k = keyb_poll();
if (ijk_present && k == KEY_NONE) {
    ijk_read();
    if      (ijk_ljoy & IJK_JOY_UP)    k = KEY_UP;
    else if (ijk_ljoy & IJK_JOY_DOWN)  k = KEY_DOWN;
    else if (ijk_ljoy & IJK_JOY_LEFT)  k = KEY_LEFT;
    else if (ijk_ljoy & IJK_JOY_RIGHT) k = KEY_RIGHT;
    else if (ijk_ljoy & IJK_JOY_FIRE)  k = KEY_ENTER;
}
```

---

## 5. API stockage de masse LOCI (loci.h)

> **Présent dans `include/`, pas encore câblé dans OSE.** `loci.h/c` ont été
> copiés textuellement depuis locifilemanager-v2 pour la phase d'E/S
> fichiers LOCI prévue ; aucun fichier de `src/` n'inclut actuellement
> `loci.h`, donc ce code n'est pas encore compilé dans `build/oseloci*.tap`
> (voir `ARCHITECTURE.md` §2.4). Les items Sauver/Charger Écran/Projet/Combiné
> du menu Fichier (index de menu déroulant 1 au §7) affichent actuellement
> une popup « pas encore implémenté » en attendant cette phase. Documenté ici
> pour le moment où il sera câblé.

API C de haut niveau pour le périphérique de stockage de masse LOCI. Inclure
`loci.h`.

> **Toutes les opérations LOCI nécessitent le matériel LOCI réel.** Oricutron
> n'émule pas les registres MIA ni TAP. `loci_present()` retourne `false`
> dans l'émulateur ; toutes les opérations sur fichiers/répertoires/montages
> se dégradent silencieusement.

### Registres matériels

#### MIA — Mass Interface Adapter en `$03A0`

```c
#define MIA (*(volatile struct __LOCI_MIA *)0x03A0)
```

Champs principaux utilisés par les fonctions de haut niveau :

| Champ | Adresse | Rôle |
|---|---|---|
| `MIA.ready` | `$03A0` | Bits RX/TX prêts (`MIA_READY_TX`, `MIA_READY_RX`) |
| `MIA.tx` | `$03A1` | Émet un octet vers le firmware |
| `MIA.rx` | `$03A2` | Reçoit un octet du firmware |
| `MIA.xstack` | `$03AC` | XSTACK — échange de paramètres/résultats |
| `MIA.errno_lo` | `$03AD` | Octet bas du code d'erreur |
| `MIA.op` | `$03AF` | Écrire un code d'opération pour l'invoquer |
| `MIA.busy` | `$03B2` | Bit 7 à 1 pendant une opération |
| `MIA.areg` | `$03B4` | Registre A (argument/résultat bas) |
| `MIA.xreg` | `$03B6` | Registre X (argument/résultat haut) |

#### TAP — Contrôleur de cassette en `$0315`

```c
#define TAP (*(volatile struct __LOCI_TAP *)0x0315)
```

| Champ | Adresse | Rôle |
|---|---|---|
| `TAP.cmd` | `$0315` | Commande (`TAP_CMD_PLAY/REC/REW/BIT/FFW`) |
| `TAP.status` | `$0316` | État |
| `TAP.data` | `$0317` | Données |

### État global

```c
extern uint8_t loci_errno;  // code d'erreur de la dernière opération échouée
extern LociCfg locicfg;     // configuration du périphérique (rempli par get_locicfg)

#define LOCI_EACCES 3        // "non vide" : valeur de loci_errno renseignée
                             // par MIA_OP_UNLINK sur un répertoire non vide
                             // (FatFs FR_DENIED / POSIX ENOTEMPTY ; confirmé
                             // avec loci_fs.c op_unlink de Phosphoric, à
                             // revérifier avec le micrologiciel LOCI réel
                             // si différent)
```

### Structures de données

```c
typedef struct { uint8_t major, minor, patch; } LociVersion;
```

```c
typedef struct {
    uint8_t     devnr;
    uint8_t     validdev[10];
    LociVersion version;
    LociUname   uname;
} LociCfg;
```

```c
typedef struct {
    int16_t  fd;
    uint16_t off;
    char     name[64];
} LociDir;   // descripteur de flux de répertoire
```

```c
typedef struct {
    int16_t  d_fd;
    char     d_name[64];
    uint8_t  d_attrib;     // DIR_ATTR_RDO, DIR_ATTR_SYS, DIR_ATTR_DIR
    uint8_t  reserved;
    uint32_t d_size;
} LociDirent;   // 72 octets
```

Indicateurs d'attribut de répertoire : `DIR_ATTR_RDO` (0x01 = lecture seule),
`DIR_ATTR_SYS` (0x04 = système), `DIR_ATTR_DIR` (0x10 = répertoire).

```c
typedef struct {
    uint8_t flag_int, flag_str, type, autorun;
    uint8_t end_addr_hi, end_addr_lo;
    uint8_t start_addr_hi, start_addr_lo;
    uint8_t reserved;
    uint8_t filename[16];
} LociTapHdr;   // en-tête de cassette Oric, 25 octets
```

Indicateurs d'ouverture : `O_RDONLY` (1) · `O_WRONLY` (2) · `O_RDWR` (3) ·
`O_CREAT` (0x10) · `O_TRUNC` (0x20) · `O_APPEND` (0x40) · `O_EXCL` (0x80).

Valeurs de positionnement : `SEEK_CUR` (0) · `SEEK_END` (1) · `SEEK_SET` (2).

### Détection et configuration

```c
bool loci_present(void);
```
Retourne `true` si un périphérique LOCI est actif. Vérifie la signature `'L'`
en `$0319`. **À appeler avant toute opération LOCI.**

```c
void get_locicfg(void);
```
Remplit `locicfg` : nombre de périphériques, version du firmware, informations
système via `MIA_OP_UNAME`.

```c
bool loci_check_fw(uint8_t major, uint8_t minor, uint8_t patch);
```
Retourne `true` si la version du firmware est ≥ `major.minor.patch`. Permet
de conditionner des fonctionnalités à la version.

```c
const char *get_loci_devname(uint8_t devid, uint8_t maxlength);
```
Retourne le nom du lecteur pour le périphérique `devid`.

```c
void loci_uname(LociUname *buf);
```
Remplit une structure `LociUname` via XSTACK.

### Utilitaires système

```c
int16_t phi2(void);
```
Retourne la fréquence d'horloge CPU en kHz (typiquement 1000 sur Oric Atmos).

```c
int32_t loci_lrand(void);
```
Retourne un entier aléatoire 32 bits depuis le générateur du firmware LOCI.

### Assistants RAM overlay

```c
void enable_overlay_ram(void);
void disable_overlay_ram(void);
```
Active/désactive la RAM overlay en écrivant dans `MICRODISCCFG` en `$0314`.
Toujours désactiver avant de retourner à l'exécution normale de la ROM.
Utilisées par le système de menus et `cwin_push`/`cwin_pop` dans
locifilemanager-v2 — actuellement inutilisées dans OSE (notes des §3/§7).

### Accès XRAM

La XRAM est une RAM étendue accessible uniquement via les canaux DMA de la
MIA. Adresse de base du tampon de copie : `COPYBUF_XRAM_ADDR` (`0x8000`),
taille `COPYBUF_XRAM_SIZE` (`0x0800` octets).

```c
void    xram_poke(uint16_t addr, uint8_t val);
uint8_t xram_peek(uint16_t addr);
void    xram_memcpy_to(uint16_t dest, const void *src, uint16_t count);
void    xram_memcpy_from(void *dest, uint16_t src, uint16_t count);
```

### Entrées/sorties de fichiers

```c
int16_t loci_open(const char *path, uint16_t flags);
```
Ouvre un fichier. Retourne un descripteur de fichier (≥ 0) en cas de succès,
négatif en cas d'erreur. `flags` est une combinaison de constantes `O_*`.

```c
int16_t loci_close(int16_t fd);
int16_t loci_read(int16_t fd, void *buf, uint16_t count);
int16_t loci_write(int16_t fd, const void *buf, uint16_t count);
int32_t loci_lseek(int16_t fd, int32_t offset, uint8_t whence);
int16_t loci_unlink(const char *path);   // supprimer un fichier
int16_t loci_rename(const char *oldpath, const char *newpath);
```

En cas d'erreur, ces fonctions renseignent `loci_errno` et retournent une
valeur négative.

### Opérations de fichiers de haut niveau

```c
bool file_exists(const char *path);
```
Retourne `true` si le fichier au chemin `path` existe.

```c
int16_t file_load(const char *path, void *dst, uint16_t count);
```
Ouvre, lit `count` octets dans `dst`, ferme. Retourne les octets lus ou
une valeur négative.

```c
int16_t file_save(const char *path, const void *src, uint16_t count);
```
Crée/écrase, écrit `count` octets depuis `src`, ferme. Retourne les octets
écrits ou une valeur négative.

```c
int16_t file_copy(const char *dst, const char *src);
```
Copie le fichier `src` vers `dst` en utilisant le tampon XRAM. Retourne les
octets copiés ou une valeur négative en cas d'erreur.

```c
int16_t file_copy_progress(const char *dst, const char *src,
                            uint8_t progx, uint8_t progy, uint8_t progl);
```
Comme `file_copy`, mais dessine une barre de progression directement dans la
RAM écran à la colonne `progx`, ligne `progy` (largeur `progl` cellules)
pendant la copie via le tampon XRAM. Vérifie `keyb_check()` à chaque bloc ;
si **ÉCHAP** est pressé, la copie s'arrête immédiatement, le fichier `dst`
partiellement écrit est supprimé via `loci_unlink`, et la fonction retourne
`-2`. Retourne `0` en cas de succès, ou un autre code d'erreur négatif (avec
`loci_errno` renseigné) en cas d'échec d'E/S.

### Opérations de répertoire

```c
LociDir    *loci_opendir(const char *path);
void        loci_closedir(LociDir *dir);
LociDirent *loci_readdir(LociDir *dir);
int16_t     loci_mkdir(const char *path);
void        loci_getcwd(char *buf, uint8_t len);
```

`loci_readdir` retourne un pointeur vers un tampon statique `LociDirent`, ou
`NULL` en fin de répertoire. Le tampon est écrasé à chaque appel.

`loci_getcwd` remplit `buf` (taille `len`) avec le chemin du répertoire
courant.

### Opérations de montage

```c
int16_t loci_mount(int16_t drive, const char *path, const char *filename);
int16_t loci_umount(int16_t drive);
```

Monte/démonte une image disque, cassette ou ROM sur le lecteur `drive`
(base 0).

### Opérations de cassette

```c
int32_t tap_seek(int32_t pos);
int32_t tap_tell(void);
int32_t tap_read_header(LociTapHdr *hdr);
```

`tap_seek` / `tap_tell` positionnent la cassette virtuelle.
`tap_read_header` lit l'en-tête de cassette Oric de 25 octets à la position
courante dans `*hdr`.

---

## 6. Bibliothèque générique de jeux de caractères (include/charset.h)

Nouveau dans OSE — absent de locifilemanager-v2. Purs assistants
d'adressage/copie pour le modèle de RAM de jeux de caractères de l'Oric
Atmos (`CHARSET_STD`/`CHARSET_ALT`/`CHARSETROM`, voir §1) — sans état
applicatif, réutilisable dans n'importe quel projet Oric Atmos qui redéfinit
la RAM de jeux de caractères à l'exécution. Inclure `charset.h` ; la
bibliothèque compile automatiquement `charset.c` via `#pragma compile`.

### Constantes

| Constante | Valeur | Signification |
|---|---|---|
| `CHARSET_GLYPH_AREA_OFFSET` | `0x100` | Décalage de la plage affichable (codes `0x20-0x7F`) dans une banque `CHARSET_STD`/`CHARSET_ALT` |
| `CHARSET_GLYPH_AREA_SIZE` | `768` | Taille de la plage affichable (96 glyphes × 8 octets) |
| `CHARSET_GLYPH_BYTES` | `8` | Octets par glyphe (un par ligne de pixels) |
| `CHARSET_GLYPH_WIDTH` | `6` | Colonnes de pixels visibles par ligne (bits 5..0, bit5 = la plus à gauche) |

### Primitives d'adressage et de copie

```c
uint16_t charset_address(uint8_t screencode, uint8_t altorstd);
```
Calcule l'adresse en RAM de jeux de caractères du glyphe de 8 octets de
`screencode` (`base + code*8`, codes `0x00-0x7F`). `altorstd` : `0` =
`CHARSET_STD`, `1` = `CHARSET_ALT`.

```c
void charset_save(uint16_t base, uint8_t *dest);
```
Copie la plage de glyphes affichables (`CHARSET_GLYPH_AREA_SIZE` octets) de
la banque `base` (`CHARSET_STD` ou `CHARSET_ALT`) dans `dest[]`.

```c
void charset_load(uint16_t base, const uint8_t *src);
```
Copie `CHARSET_GLYPH_AREA_SIZE` octets depuis `src[]` dans la plage de
glyphes affichables de la banque `base`. `src` peut être une autre banque
de jeux de caractères ou `CHARSETROM`.

```c
const uint8_t *charset_rom_glyph(uint8_t screencode);
```
Pointeur vers le glyphe de 8 octets de `screencode` dans `CHARSETROM` (codes
`0x20-0x7F` uniquement — adressé comme `CHARSETROM + (code-0x20)*8`, la
convention de décalage **opposée** à celle de `charset_address()`).

### Opérations sur le bitmap d'un glyphe

Transformations sur place d'un glyphe Oric de 8 octets (`CHARSET_GLYPH_BYTES`
lignes × `CHARSET_GLYPH_WIDTH` bits visibles/ligne, bit5 = le plus à gauche).
`glyph` est typiquement un pointeur dans la RAM de jeux de caractères vive
(voir `charset_address()`), donc `volatile`.

```c
void charset_glyph_invert(volatile uint8_t *glyph);
```
Inverse tous les pixels (XOR de chaque ligne avec le masque 6 bits `0x3F`).

```c
void charset_glyph_mirror_v(volatile uint8_t *glyph);
```
Miroir vertical : inverse l'ordre des 8 lignes.

```c
void charset_glyph_mirror_h(volatile uint8_t *glyph);
```
Miroir horizontal : inverse l'ordre des bits de chaque ligne.

```c
void charset_glyph_scroll_up(volatile uint8_t *glyph);
```
Fait défiler toutes les lignes vers le haut d'une position, la ligne 0
rebouclant en bas.

```c
void charset_glyph_scroll_down(volatile uint8_t *glyph);
```
Fait défiler toutes les lignes vers le bas d'une position, la dernière
ligne rebouclant en haut (ligne 0).

```c
void charset_glyph_rotate_left(volatile uint8_t *glyph);
```
Fait tourner les bits de chaque ligne vers la gauche d'une position (bit5
reboucle vers bit0).

```c
void charset_glyph_rotate_right(volatile uint8_t *glyph);
```
Fait tourner les bits de chaque ligne vers la droite d'une position (bit0
reboucle vers bit5).

### Appelants

`src/charsetedit.c` (éditeur de caractères — ESPACE/SUPPR basculent le pixel
sous le curseur ; `i`/`x`/`y`/`u`/`d`/`l`/`r` invoquent
`invert`/`mirror_h`/`mirror_v`/`scroll_up`/`scroll_down`/`rotate_left`/
`rotate_right` ; `s` restaure le glyphe courant via `charset_rom_glyph` +
`memcpy`) et `src/charsetswap.c` (ci-dessous) sont les seuls appelants
actuels.

### Mécanisme d'échange de jeu de caractères (src/charsetswap.h)

Logique spécifique à l'application construite sur `charset_save`/
`charset_load`/`charset_rom_glyph`, utilisée par le système de menus (§7)
pour que les éléments visuels des popups s'affichent toujours avec des
glyphes lisibles, standard ROM, même après que l'utilisateur a redéfini le
jeu standard :

```c
void charsetswap_mark_changed(void);  // à appeler une fois quand l'utilisateur édite un glyphe
void charsetswap_enter(void);         // opt-in : sauvegarde + restaure le CHARSET_STD standard ROM
void charsetswap_exit(void);          // opt-in : restaure le CHARSET_STD de l'utilisateur depuis la sauvegarde
```

- **Conditionné par `charset_changed`** : si l'utilisateur n'a jamais édité
  un glyphe, `charsetswap_enter`/`exit` sont des no-ops.
- **Compté par profondeur** : les popups imbriquées (par ex. un menu
  déroulant ouvrant une popup `areyousure`) ne font la sauvegarde/
  restauration qu'une fois, sur la paire enter/exit la plus externe.
- **Standard uniquement** : `CHARSET_ALT` n'est *pas* échangé (un test a
  confirmé que `jsr $F816`/`ROM_ALTCHARS` est un no-op pour `CHARSET_ALT`
  dans ce contexte bare-metal — voir la section échange de jeu de
  caractères de `CLAUDE.md` pour les détails). `CHARSET_ALT` conserve les
  éditions de l'utilisateur visibles pendant les popups.
- L'éditeur de caractères (`charsetedit_run()`) **n'opte pas** pour
  l'échange (via `swap_charset=0` de `menu_winsave`, §7), afin que les
  éditions de glyphe en direct restent visibles pendant l'édition.

---

## 7. Système de menus (menu.h)

Système de menus déroulants plein écran : barre de menus → menus déroulants
→ boîtes de dialogue, avec une pile LIFO de sauvegarde de fenêtres en RAM
principale. Inclure `menu.h`. Adapté du `menu.h`/`menu.c` de
locifilemanager-v2 (référence locale
`/home/xahmol/git/locifilemanager-v2/src/menu.h`) — **la version d'OSE
diffère substantiellement** : une barre à 4 items au lieu de 6, 5 menus
déroulants au lieu de 11, et la sauvegarde/restauration de fenêtres copie
dans un **tampon en RAM principale** au lieu de la RAM overlay (pas de LOCI
nécessaire — les menus se referment sans résidu sous Oricutron nu).

### Architecture

Trois niveaux :
- **Barre de menus** (ligne 0) — encre noire, papier vert, 4 items titrés
  (Écran/Fichier/Caractères/Information).
- **Menus déroulants** — items cyan avec surbrillance jaune ; s'ouvrent sous
  l'item de la barre (menus de premier niveau) ou à l'intérieur d'une popup
  (sous-menus, par ex. Oui/Non).
- **Boîtes de dialogue** — fond blanc (`menu_wininit`) ; utilisent le menu
  Oui/Non (`menu_areyousure`) ou un message + une frappe (`menu_messagepopup`).

### Constantes de capacité

| Constante | Valeur | Signification |
|---|---|---|
| `MENUBAR_MAXOPTIONS` | 4 | Nombre d'items de la barre |
| `MENUBAR_MAXLENGTH` | 12 | Longueur max par titre de barre (11 visibles + NUL) |
| `PULLDOWN_NUMBER` | 5 | Nombre total de menus déroulants (0–4) |
| `PULLDOWN_MAXOPTIONS` | 6 | Items max par menu déroulant |
| `PULLDOWN_MAXLENGTH` | 17 | 16 caractères visibles + NUL |
| `MENU_WIN_DEPTH` | 9 | Profondeur max de sauvegardes de fenêtres imbriquées |
| `MENU_WINBUF_SIZE` | 2048 | Taille du tampon de sauvegarde de fenêtres en RAM principale (octets) |

### Codes de retour

| Constante | Valeur | Signification |
|---|---|---|
| `MENU_CANCEL` | 0 | ÉCHAP pressé (menu déroulant échappable) |
| `MENU_LEFT_ARROW` | 18 | Flèche gauche → l'appelant ouvre l'item de barre précédent |
| `MENU_RIGHT_ARROW` | 19 | Flèche droite → l'appelant ouvre l'item de barre suivant |
| `MENU_YESNO` | 4 | Indice du menu déroulant Oui/Non |

### Carte des indices de menus déroulants

| Indice | Menu | Items (`pulldown_options[]`) |
|---|---|---|
| 0 | Écran | 4 — Largeur/Hauteur (en direct), Effacer, Remplir |
| 1 | Fichier | 6 — Sauver/Charger Écran, Sauver/Charger Projet, Sauver/Charger Combiné |
| 2 | Caractères | 6 — Charger/Sauver Standard, Charger/Sauver Alterné, Charger/Sauver Combiné |
| 3 | Information | 2 — Version/crédits, Quitter |
| 4 | Oui/Non | 2 — Oui, Non |

`pulldown_options[PULLDOWN_NUMBER] = { 4, 6, 6, 2, 2 }`.

`pulldown_titles[0][0]`/`[0][1]` (`"Width:%3u"`/`"Height:%3u"` en anglais,
`MSG_SCREEN_WIDTH_FMT`/`MSG_SCREEN_HEIGHT_FMT` localisés) sont des
emplacements réservés, réécrits à l'exécution par `update_size_titles()`
dans `src/menudata.c` chaque fois que la taille du canevas change. Les
items Fichier/Caractères (indices 1–2, et l'item 1 d'Information =
« Version/crédits ») affichent actuellement une popup « pas encore
implémenté » (`menu_messagepopup(MSG_MENU_NOTIMPL)`) en attendant la phase
d'E/S fichiers LOCI (§5).

### Structures de données

```c
typedef struct {
    uint16_t  offset;       // décalage dans menu_winbuf[] où les lignes sont sauvegardées
    uint8_t   ypos;         // première ligne d'écran sauvegardée
    uint8_t   height;       // nombre de lignes sauvegardées (chaque ligne = SCREEN_COLS octets)
    uint8_t   swap_charset; // 1 = charsetswap_enter() a été appelé pour cette sauvegarde
} MenuWinRecord;

typedef struct {
    char     titles[MENUBAR_MAXOPTIONS][MENUBAR_MAXLENGTH];
    uint8_t  xstart[MENUBAR_MAXOPTIONS];   // colonne d'écran de l'octet d'attribut de surbrillance
    uint8_t  ypos;                          // ligne d'écran où la barre est dessinée
} MenuBar;
```

### Données globales exportées

```c
extern MenuBar menubar;
extern char    pulldown_options[PULLDOWN_NUMBER];
extern char    pulldown_titles[PULLDOWN_NUMBER][PULLDOWN_MAXOPTIONS][PULLDOWN_MAXLENGTH];
```

Toutes définies dans `src/menudata.c`, qui possède aussi `menudata_init()`
et `menu_run()` — l'initialisation des tables spécifiques à l'application et
la boucle de dispatch, gardées séparées pour que `menu.c` reste un moteur de
menus générique, indépendant de l'application.

### Initialisation

```c
void menu_init(void);
```
Réinitialise la pile de sauvegarde de fenêtres (`menu_win_depth =
menu_win_ptr = 0`). À appeler une fois au démarrage. (Contrairement au
`menu_init` de locifilemanager-v2, celui-ci ne touche **pas**
`menubar`/`pulldown_titles` — ce sont des initialiseurs statiques dans
`menudata.c` — et n'appelle pas `ijk_detect()`, §4.)

### Affichage

```c
void menu_placebar(uint8_t y);
```
Dessine la barre de menus à la ligne d'écran `y` : encre noire / papier vert
sur toute la ligne, puis chaque entrée de `menubar.titles[]` de gauche à
droite avec un espace d'une colonne de chaque côté, en calculant
`menubar.xstart[]` à partir des longueurs des titres.

**Contrainte de disposition :** `somme(longueur(titles[0..3])) + 7 <= 39`,
c'est-à-dire `somme(longueur(titles)) <= 32`, sinon l'écriture de
surbrillance du dernier item dépasse la ligne de 40 colonnes et se confond
visuellement avec l'item précédent. Voir la section « Localisation » de
`CLAUDE.md` pour le bug de barre de menus en FR causé par cette contrainte
(et sa correction) — **tout nouveau titre de barre de menus, dans l'une ou
l'autre langue, doit respecter cette limite**.

### Sauvegarde/restauration de fenêtres (RAM principale, avec intégration de l'échange de jeu de caractères)

```c
void menu_winsave(uint8_t ypos, uint8_t height, uint8_t swap_charset);
```
Empile un enregistrement sur la pile LIFO de sauvegarde de fenêtres et copie
`height` lignes complètes de 40 octets à partir de la ligne d'écran `ypos`,
depuis la RAM vidéo vers `menu_winbuf[]`. Ne fait rien si la pile
(`MENU_WIN_DEPTH`) est pleine ou si le tampon (`MENU_WINBUF_SIZE`) est épuisé.

`swap_charset` : `1` = appelle aussi `charsetswap_enter()` (les éléments
visuels des popups optent pour le jeu de caractères standard ROM, §6, afin
que le texte des menus reste lisible même si l'utilisateur a redéfini des
glyphes) ; `0` = n'opte pas (l'éditeur de caractères, pour que les éditions
de glyphe en direct restent visibles).

```c
void menu_winrestore(void);
```
Dépile l'enregistrement le plus récent et recopie ses lignes sauvegardées
vers la RAM vidéo. Si son `swap_charset` était `1`, appelle aussi
`charsetswap_exit()` — l'appariement est automatique via la pile LIFO, les
appelants ne peuvent pas le désynchroniser. Ne fait rien si la pile est vide.

```c
void menu_wininit(uint8_t ypos, uint8_t height);
```
Peint un fond de popup à papier blanc pour les lignes `ypos..ypos+height-1`
(colonne 5 = `A_BGWHITE`, colonne 6 = `A_FWBLACK`, colonnes 7–39 = espace).
Les colonnes 0–4 conservent leur contenu existant. L'appelant est responsable
de `menu_winsave()`/`menu_winrestore()`.

**Remarque :** contrairement à `cwin_push`/`cwin_pop` en RAM overlay de
locifilemanager-v2 (§3) et sa pile de menus basée sur `MENU_OVERLAY_BASE`,
le `menu_winbuf[MENU_WINBUF_SIZE]` d'OSE est un simple tableau en RAM
principale — aucune RAM LOCI/overlay nécessaire. Le chemin imbriqué le plus
profond à ce jour (barre de menus + menu déroulant de premier niveau + popup
de redimensionnement + sa confirmation de réduction + son menu Oui/Non =
40+240+480+240+80 = 1080 octets) entre confortablement dans le tampon de
2048 octets, laissant de la marge pour les phases ultérieures.

### Menus déroulants

```c
uint8_t menu_pulldown(uint8_t xpos, uint8_t ypos,
                      uint8_t menunumber, uint8_t escapable);
```
Ouvre le menu déroulant `menunumber` à la position d'écran `(xpos, ypos)`, en
sauvegardant d'abord les lignes couvertes (`menu_winsave(ypos, height, 1)`)
et en les restaurant à la sortie. Met en surbrillance l'item courant ;
`KEY_UP`/`KEY_DOWN` déplacent la sélection, `KEY_ENTER` choisit. Pour les
menus déroulants de premier niveau (`menunumber < MENUBAR_MAXOPTIONS`),
`KEY_LEFT`/`KEY_RIGHT` demandent l'item de barre précédent/suivant.

| Paramètre | Signification |
|---|---|
| `xpos` | Colonne de l'octet d'encre de la barre de surbrillance (le texte de l'item commence à `xpos+2`) |
| `ypos` | Ligne d'écran du premier item |
| `menunumber` | Indice dans `pulldown_titles[]`/`pulldown_options[]` (0–4) |
| `escapable` | `1` = ÉCHAP annule (`MENU_CANCEL`) ; `0` = l'utilisateur doit choisir |

Retourne `1..pulldown_options[menunumber]` (item choisi), `MENU_CANCEL` (0),
ou `MENU_LEFT_ARROW`/`MENU_RIGHT_ARROW` pour les menus déroulants de premier
niveau.

```c
uint8_t menu_main(void);
```
Exécute la boucle de navigation de la barre de menus : `KEY_LEFT`/`KEY_RIGHT`
déplacent la surbrillance de la barre, `KEY_ENTER` ouvre `menu_pulldown()`
pour l'item en surbrillance. Si le menu déroulant retourne
`MENU_LEFT_ARROW`/`MENU_RIGHT_ARROW`, passe à l'item de barre adjacent et
rouvre son menu déroulant. `KEY_ESC` au niveau de la barre quitte.

Retourne `menubarchoice*10 + menuoptionchoice` (1..49 = un choix réel), ou
`menubarchoice*10 + 99` (≥ 99) sur ÉCHAP. **Les appelants doivent traiter
toute valeur de retour ≥ 99 comme « quitter le menu ».**

### Boîtes de dialogue

```c
uint8_t menu_areyousure(const char *message);
```
`menu_winsave(8, 6, 1)` + `menu_wininit(8, 6)`, affiche `message` et
`MSG_MENU_AREYOUSURE` (« Êtes-vous sûr ? »), puis `menu_pulldown(20, 11,
MENU_YESNO, 0)`. Retourne `1` (Oui) ou `2` (Non). Utilisé par la confirmation
de réduction du menu Écran (redimensionnement du canevas vers une taille
plus petite).

```c
void menu_messagepopup(const char *message);
```
`menu_winsave(8, 6, 1)` + `menu_wininit(8, 6)`, affiche `message` et
`MSG_MENU_PRESSAKEY` (« Appuyer sur une touche pour continuer »), attend une
frappe quelconque, puis restaure. Utilisé pour les stubs « pas encore
implémenté » (`MSG_MENU_NOTIMPL`).

### Abandonné par rapport à locifilemanager-v2

Absent du `menu.h`/`menu.c` d'OSE — pas nécessaire avant des phases
ultérieures : `menu_placeheader`/`menu_placetop` (pas de ligne d'en-tête
verte séparée — la ligne 0 d'OSE *est* la barre de menus),
`menu_fileerrormessage`/`menu_confirm_file`/`menu_option_select`/
`menu_popup_*` (boîtes de dialogue d'erreur/sélection de fichiers LOCI),
la saisie par manette IJK dans la boucle de menus déroulants
(`menu_pulldown`/`menu_main` appellent `cwin_getch()` directement), et le
partitionnement de la RAM overlay (le `menu_winbuf[]` en RAM principale
décrit ci-dessus le remplace entièrement).

---

## 8. Notes de compilation

### Compilateur

Oscar64 (`$OSCAR64_HOME/bin/oscar64`, par défaut `~/oscar64/`). Drapeaux
standard (issus du `Makefile`) :

```
-n -tf=bin -rt=include/oric_crt.c -i=include -i=src -O2 -dNOFLOAT \
  -dVERSION_MAJOR=$(VERSION_MAJOR) -dVERSION_MINOR=$(VERSION_MINOR) \
  -dVERSION_PATCH=$(VERSION_PATCH) $(LANGFLAG)
```

- `-i=include -i=src` : OSE ajoute `-i=src` (locifilemanager-v2 n'a que
  `-i=include`) afin que les chaînes `#pragma compile` puissent résoudre les
  fichiers `.c` spécifiques à l'application dans `src/` (par exemple
  `menu.c`, `menudata.c`, `charsetedit.c`, `charsetswap.c`) — voir « Chaîne
  #pragma compile » ci-dessous.
- `-dVERSION_MAJOR/MINOR/PATCH` : alimentent `MSG_SPLASH_BUILD_FMT` sur
  l'écran d'accueil (`src/main.c`).
- `$(LANGFLAG)` : `-dLANG_FR` pour `make LANG=FR`, vide pour `make`/`make
  LANG=EN` par défaut. Voir « Localisation » ci-dessous.

### Localisation

`make LANG=FR` (ou `make all-langs`) génère `build/oseloci_fr.tap` en plus de
`build/oseloci.tap` (`LANGSUFFIX=_fr`). Voir la section « Localisation » de
`CLAUDE.md` pour le mécanisme complet `strings.h`/`strings_en.h`/
`strings_fr.h`, la contrainte de longueur des titres de la barre de menus
(§7), et l'exigence de caractères non accentués en FR.

### Chaîne #pragma compile

Chaque en-tête de bibliothèque se termine par `#pragma compile("fichier.c")`.
Oscar64 résout ce chemin relativement aux répertoires d'inclusion
(`-i=include -i=src`, dans cet ordre).

- **Fichiers `.c` de bibliothèque** (génériques, réutilisables entre projets),
  dans `include/` : `oric_crt.c`, `crt_math.c`, `charwin.c`, `keyboard.c`,
  `charset.c`, `ijk.c`, `loci.c`.
- **Fichiers `.c` spécifiques à l'application** (propres à OSE), dans `src/` :
  `canvas.c`, `statusbar.c`, `editor.c`, `menu.c`, `menudata.c`,
  `charsetedit.c`, `charsetswap.c`.

### Gotchas connus d'Oscar64

| Symptôme | Cause | Correction |
|---|---|---|
| Crash de `va_arg` | `va_arg` ne fonctionne pas en mode natif (`-n`) | Ne pas utiliser `<stdarg.h>` |
| `#if MACRO` échoue avec `-d` | `-d` définit sans valeur | Utiliser `#ifdef MACRO` |
| Cast appliqué avant l'accès au membre | Oscar64 analyse `(type)struct.membre` incorrectement | Utiliser une variable temporaire |
| Ternaire `? ptr : 0` dans une fonction retournant un pointeur | Problème d'analyseur | Utiliser `if`/`return` |
| Crash sur grand tableau local | La pile 6502 ne fait que 256 octets | Déclarer les grands tableaux `static` |
| `snprintf` non trouvé | Absent de `<stdio.h>` Oscar64 | Utiliser `sprintf` à la place |
| Ensemble de sauvegarde `-O2` sous-évalué : parasites à l'écran sur un appel précis | L'allocateur de registres global sous-compte les registres actifs entre appels | Forcer un ensemble de sauvegarde plus grand avec un `sprintf` factice vers de la RAM scratch inutilisée (`0xA000`) — voir `menu_draw_item()` dans `src/menu.c` et la section « `-O2` whole-program register allocator: caller-save set can be under-counted » d'`oscar64manual.md` |
