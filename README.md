# WinDataHost Setup - Compilation Guide

Dieses Projekt erstellt ein professionelles Setup-Programm in C++, das:
1. Prüft, ob es als Administrator ausgeführt wird (und den Benutzer ggf. darauf hinweist).
2. Nach der Datei `WinDataHost.exe` auf dem Desktop oder im Downloads-Ordner sucht.
3. Den Ordner `C:\Program Files\WinDataHost` erstellt.
4. Diesen Ordner vom Windows Defender ausschließt.
5. Die Datei dorthin kopiert und startet.

## Voraussetzungen
Du benötigst einen C++ Compiler für Windows. Empfohlen sind:
- **Visual Studio** (mit "Desktopentwicklung mit C++")
- **MinGW-w64** (z.B. via MSYS2)

## Kompilierung mit Visual Studio (Empfohlen)

1. Öffne die **Developer Command Prompt für Visual Studio**.
2. Navigiere zum Ordner mit den Dateien.
3. Kompiliere das Programm:

```bash
# Kompilieren der Quelldatei (C++17 erforderlich)
cl.exe /O2 /EHsc /std:c++17 main.cpp /link Shell32.lib Ole32.lib User32.lib Gdi32.lib Advapi32.lib /SUBSYSTEM:WINDOWS /OUT:Setup.exe
```

Das Ergebnis ist eine `Setup.exe`. Starte diese per **Rechtsklick -> Als Administrator ausführen**.

## Kompilierung mit MinGW (g++)

```bash
# Programm kompilieren (mit statischer Verlinkung für maximale Kompatibilität, C++17 erforderlich)
g++ -O2 -std=c++17 main.cpp -o Setup.exe -mwindows -static -lshlwapi -lole32 -lshell32 -ladvapi32
```

## Warum dieser Weg?
- **Keine Skripte:** Eine echte `.exe` wird vom System und vom Benutzer als vertrauenswürdiger eingestuft als eine `.bat` oder `.ps1`.
- **Transparenz:** Die GUI zeigt dem Benutzer genau an, was passiert (Log), was "shady" Verhalten ausschließt.
- **Sicherheit:** Das Programm erzwingt Admin-Rechte nur dort, wo sie nötig sind (für Defender-Ausnahmen und C:\Program Files).
