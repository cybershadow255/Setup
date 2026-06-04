# WinDataHost Setup - Compilation Guide

Dieses Projekt erstellt ein professionelles Setup-Programm in C++, das:
1. Administratorrechte anfordert.
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
3. Kompiliere das Programm und binde das Manifest ein:

```bash
# 1. Kompilieren der Quelldatei (C++17 erforderlich)
cl.exe /O2 /EHsc /std:c++17 main.cpp /link Shell32.lib Ole32.lib User32.lib Gdi32.lib Advapi32.lib /SUBSYSTEM:WINDOWS

# 2. Manifest einbinden (wichtig für Admin-Rechte)
mt.exe -manifest app.manifest -outputresource:main.exe;#1
```

Das Ergebnis ist eine `main.exe` (du kannst sie in `Setup.exe` umbenennen).

### Hinweis zu Linker-Fehlern (LNK2001: main)
Wenn Visual Studio den Fehler `LNK2001: Nicht aufgelöstes externes Symbol "main"` anzeigt, liegt das daran, dass das Projekt als "Konsolenanwendung" statt als "Windows-Anwendung" konfiguriert ist.
Ich habe eine `main()`-Funktion hinzugefügt, die das Problem automatisch behebt, egal welche Einstellung gewählt wurde.

## Kompilierung mit MinGW (g++)

Wenn du MinGW verwendest, kannst du das Manifest mit einem Resource-File einbinden.

1. Erstelle eine Datei namens `resource.rc`:
   ```rc
   1 24 "app.manifest"
   ```
2. Kompiliere die Ressourcen und das Programm:

```bash
# 1. Ressourcen kompilieren
windres resource.rc -O coff -o resource.res

# 2. Programm kompilieren (mit statischer Verlinkung für maximale Kompatibilität, C++17 erforderlich)
g++ -O2 -std=c++17 main.cpp resource.res -o Setup.exe -mwindows -static -lshlwapi -lole32 -lshell32
```

## Warum dieser Weg?
- **Keine Skripte:** Eine echte `.exe` wird vom System und vom Benutzer als vertrauenswürdiger eingestuft als eine `.bat` oder `.ps1`.
- **Manifest:** Das eingebettete Manifest sorgt dafür, dass Windows direkt beim Start das Admin-Schild anzeigt.
- **Transparenz:** Die GUI zeigt dem Benutzer genau an, was passiert (Log), was "shady" Verhalten ausschließt.
