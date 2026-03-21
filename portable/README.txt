OneClickRGB v1.0
================

INHALT
------
OneClickRGB.exe    - Hauptanwendung
hidapi.dll         - HID-Kommunikation (ASUS Aura, Tastatur)
PawnIOLib.dll      - SMBus-Treiber-Interface (G.Skill RAM)
SmbusI801.bin      - Intel SMBus Controller Modul
icon.png           - Anwendungsicon
PawnIO_setup.exe   - PawnIO Treiber-Installer

install.bat        - Automatische Installation
install_manual.bat - Interaktive Installation mit Optionen
uninstall.bat      - Deinstallation


INSTALLATION
------------
Option 1 - Automatisch:
  Rechtsklick auf "install.bat" -> "Als Administrator ausfuehren"

Option 2 - Manuell/Interaktiv:
  Rechtsklick auf "install_manual.bat" -> "Als Administrator ausfuehren"

Option 3 - Portabel:
  Ordner an beliebige Stelle kopieren
  PawnIO_setup.exe einmalig als Admin ausfuehren
  OneClickRGB.exe starten


DEINSTALLATION
--------------
  Rechtsklick auf "uninstall.bat" -> "Als Administrator ausfuehren"


UNTERSTUTZTE HARDWARE
---------------------
- ASUS Aura Mainboard RGB (USB HID)
- EVision Tastatur RGB (USB HID)
- G.Skill Trident Z5 RGB DDR5 (SMBus)


SYSTEMANFORDERUNGEN
-------------------
- Windows 10/11 (64-bit)
- Admin-Rechte (fuer volle Funktionalitaet)


EINSTELLUNGEN
-------------
Werden automatisch gespeichert in:
  %APPDATA%\OneClickRGB\app_settings.cfg
  %APPDATA%\OneClickRGB\profiles\


TASTENKURZEL
------------
Tab          - Navigation zwischen Controls
Enter/Space  - Button/Checkbox aktivieren
Pfeiltasten  - Slider anpassen


THEMES
------
Dark / Light / Colorblind
Umschalten mit Theme-Button (Sonnensymbol)


FEHLERBEHEBUNG
--------------
G.Skill RAM wird nicht erkannt:
  -> PawnIO_setup.exe als Admin ausfuehren
  -> PC neu starten

ASUS RGB funktioniert nicht:
  -> Als Administrator starten
  -> USB-Verbindung pruefen

Programm startet nicht:
  -> Alle DLL-Dateien im gleichen Ordner?
  -> Visual C++ Redistributable installiert?


---
(c) 2024-2025 OneClickRGB
https://github.com/yourusername/OneClickRGB
