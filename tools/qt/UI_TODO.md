# Qt UI port — état et reprise

Objectif : toute l'UI du launcher en QML, 1 pour 1 avec les 26 écrans de référence
(`-uishot` de la version C# Avalonia, 940×528) — valeurs reprises du C++ de Zection
(`src/MphRead.Native/Mods/Launcher/Gui`). Branche `qt-ui`, worktree `~/GIT/fp-qt`.

## Reprendre
- Build : `tools/qt/build-linux.sh`
- Captures Qt : `FP_QT_UISHOT=DIR [FP_QT_UISHOT_ONLY=a,b] ./FruityPrime -launcher`
  (dans `build/linux-qt-release`, avec `LIBGL_ALWAYS_SOFTWARE=1 QT_QPA_PLATFORM=wayland`)
- Erreurs QML : `build/linux-qt-release/logs/*-native.txt`
- Références + JSON de positions : `.qt-screenshots/avalonia-reference/` (copie des .json à régénérer
  avec `-uishot` si absents, cf. mémoire)
- Comparaison : script python (ref | qt | diff, % de pixels > 24) — cf. `tools/qt/cmp.py`

## Architecture
- `qml/Page.qml` = UiLayout::Page (fond, feuille, carte, bandeau/titre, corps, marques, note)
- `qml/Main.qml` = pile d'écrans (StartScreen/InGameMenu), `nav.push/pop`
- C++ : `ShellBridge` (contexte `shell`), `PlayModel`, `RowModel` (lignes des réglages),
  `HunterStandItem` (rectangle où le moteur dessine le VRAI modèle 3D — jamais de figurine en blocs),
  `ServerBadgeItem` (drapeaux)
- Tailles de police fractionnaires : `font.pointSize: Theme.pt(px)`

## Fait
- [x] Start, Pause (dynamique : vote, spectate, record), Confirm
- [x] Play : Online (liste serveurs, drapeaux), Offline (cartes + panneau latéral), Story, Clips, Vote
- [x] EndPanel (bulletin + chasseur + READY), Shell::TickEndPanel
- [x] Écarts actuels : confirm 0.9 %, pause 0.9 %, end 0.6 %, story 1.4 %, clips 1.5 %,
      offline 3.8 %, vote 3.9 %, online ~8 % (rendu du texte)

## Décisions utilisateur (2026-09-28)
- Police des libellés : **Inter** (`Assets/Fonts/Inter-Variable.ttf`, `Theme.pixel`) — intégrée.
  Pixelify ne reste que pour le logo (`Theme.wordmark`). JetBrains Mono pour champs/chiffres.
  Test d'une autre police sans rebuild : `FP_QT_FONT=<ttf>`.
- Texte : anticrénelage gris + hinting vertical (l'aliasé faisait trop pixelisé, retour utilisateur
  2026-09-28) ; jamais de sous-pixel. `FP_QT_TEXT_AA=0` remet l'aliasé.
  Captures à la taille d'un vrai écran : `FP_QT_UISHOT_SIZE=1920x1080`.
- Pas d'aide « Enter Select / Esc Back » en bas à gauche de l'écran d'accueil (inutile, retirée).
- Le flou ressenti vient surtout de la capture en 940×528 : vérifier en plein écran dans la vraie fenêtre.

## En cours / à faire
- [ ] Settings : SettingsModel (RowModel) + SettingsPage.qml ; Display, Audio, Controls
      (Keyboard/Gamepad/Stylus), Profile, Credits ; items peints KeyRow, PadRow, GamepadMonitor,
      CrosshairPreview ; GamepadSetupPanel, GamepadProfilePanel
- [ ] Setup (fichiers du jeu)
- [ ] CreateServer (+ dedicated, rotation de cartes, choix d'hôte)
- [ ] Lobby (LobbyScreen, LobbyPlayerRow)
- [ ] Variantes téléphone (start/play-online/pause phone, pausemenu-small)
- [ ] serverbrowser (page d'échantillon ServerBrowserSample.qml)
- [ ] Ligne de version cliquable (mise à jour), heart en coin (bar vertical)
- [ ] Navigation manette (ControllerNav/GamepadNavigation)
- [ ] Vérif fenêtre réelle (FP_QT_DEMO) + build Windows
