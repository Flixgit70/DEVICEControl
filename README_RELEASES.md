# DEVICEControl – Release-Workflow

Diese Datei beschreibt den empfohlenen Ablauf für Versionsnummern, Git-Tags, GitHub-Releases und Binaries für DEVICEControl.

## 1. Versionsschema

DEVICEControl verwendet Semantic Versioning:

```text
0.6.9
│ │ └─ Patch: Bugfix oder kleine Änderung
│ └─── Minor: neue Funktion oder größere Erweiterung
└───── Major: große bzw. inkompatible Änderung
```

Solange DEVICEControl noch in aktiver Entwicklung ist, bleiben die Versionen im Bereich `0.x.x`.

Beispiele:

```text
0.6.9   Bugfix
0.6.10  weiterer Bugfix
0.7.11   neue größere Funktion
0.8.0   größere UI-/Architektur-Erweiterung
1.0.0   erste stabile Hauptversion
```

Git-Tags bekommen zusätzlich ein `v`:

```text
v0.6.9
v0.7.11
v1.0.0
```

---

## 2. Versionsnummer im Projekt

Die Versionsnummer sollte möglichst nur an einer zentralen Stelle gepflegt werden, z. B. in `CMakeLists.txt`:

```cmake
project(DEVICEControl VERSION 0.7.11)
```

Der About-Dialog sollte diese Versionsnummer automatisch übernehmen, damit sie nicht zusätzlich manuell gepflegt werden muss.

---

## 3. Normaler Entwicklungsablauf

Vor Beginn der Arbeit:

```bash
git pull
```

Während bzw. nach der Entwicklung:

```bash
git status
git add .
git commit -m "Kurze Beschreibung der Änderung"
git push
```

Beispiele für Commit-Nachrichten:

```text
Fix tree filtering with exclusions
Add multi-selection for page items
Improve Ember reconnect handling
Add DirectOut JSON provider
Fix Linux ARM build
```

`main` soll möglichst immer einen grundsätzlich lauffähigen Stand enthalten.

---

## 4. Neue Release-Version vorbereiten

Beispiel: Release `0.7.11`.

### Schritt 1 – Versionsnummer anpassen

In `CMakeLists.txt`:

```cmake
project(DEVICEControl VERSION 0.7.11)
```

### Schritt 2 – Änderungen committen

```bash
git add .
git commit -m "Release 0.7.11"
git push
```

### Schritt 3 – Git-Tag setzen

```bash
git tag -a v0.7.11 -m "DEVICEControl 0.7.11"
git push origin v0.7.11
```

Alle vorhandenen Tags anzeigen:

```bash
git tag
```

---

## 5. GitHub-Release anlegen

Mit der GitHub CLI:

```bash
gh release create v0.7.11 \
  --title "DEVICEControl 0.7.11" \
  --notes "DEVICEControl Release 0.7.11"
```

Alternativ kann das Release auch über die GitHub-Weboberfläche erstellt werden.

---

## 6. Binaries pro Version ablegen

Zu fast jeder Release-Version sollen die fertigen Binaries als Assets im GitHub-Release abgelegt werden.

Empfohlene Dateinamen:

```text
DEVICEControl_0.7.11_macOS_arm64.zip
DEVICEControl_0.7.11_Windows_x64.zip
DEVICEControl_0.7.11_Linux_x64.zip
DEVICEControl_0.7.11_Linux_arm64.zip
```

Optional können später weitere Varianten ergänzt werden.

Die Binaries sollten **nicht direkt ins Git-Repository committed werden**, sondern als Assets am jeweiligen GitHub-Release hängen.

### Binaries direkt beim Erstellen des Releases hochladen

Beispiel:

```bash
gh release create v0.7.11 \
  DEVICEControl_0.7.11_macOS_arm64.zip \
  DEVICEControl_0.7.11_Windows_x64.zip \
  DEVICEControl_0.7.11_Linux_x64.zip \
  DEVICEControl_0.7.11_Linux_arm64.zip \
  --title "DEVICEControl 0.7.11" \
  --notes "DEVICEControl Release 0.7.11"
```

### Binaries nachträglich zu einem bestehenden Release hinzufügen

```bash
gh release upload v0.7.11 DEVICEControl_0.7.11_macOS_arm64.zip
```

Mehrere Dateien gleichzeitig:

```bash
gh release upload v0.7.11 \
  DEVICEControl_0.7.11_macOS_arm64.zip \
  DEVICEControl_0.7.11_Windows_x64.zip \
  DEVICEControl_0.7.11_Linux_x64.zip \
  DEVICEControl_0.7.11_Linux_arm64.zip
```

---

## 7. Empfohlener kompletter Release-Ablauf

Beispiel für Version `0.7.11`:

```bash
# 1. Aktuellen Stand holen
git pull

# 2. Versionsnummer in CMakeLists.txt auf 0.7.11 setzen

# 3. Änderungen committen
git add .
git commit -m "Release 0.7.11"
git push

# 4. Tag setzen
git tag -a v0.7.11 -m "DEVICEControl 0.7.11"
git push origin v0.7.11

# 5. Binaries bauen und als ZIP-Dateien ablegen

# 6. GitHub-Release inkl. Binaries erstellen
gh release create v0.7.11 \
  DEVICEControl_0.7.11_macOS_arm64.zip \
  DEVICEControl_0.7.11_Windows_x64.zip \
  DEVICEControl_0.7.11_Linux_x64.zip \
  DEVICEControl_0.7.11_Linux_arm64.zip \
  --title "DEVICEControl 0.7.11" \
  --notes "DEVICEControl Release 0.7.11"
```

---

## 8. Größere neue Funktionen

Kleine Änderungen können direkt auf `main` entwickelt werden.

Für größere Funktionen kann ein eigener Branch verwendet werden:

```bash
git switch -c feature/directout
```

Änderungen committen:

```bash
git add .
git commit -m "Add initial DirectOut integration"
git push -u origin feature/directout
```

Nach erfolgreichem Test wird der Branch später wieder in `main` übernommen.

---

## 9. Repository auf einen neuen Rechner oder in einen neuen Ordner holen

Neue Arbeitskopie erstellen:

```bash
git clone git@github.com:Flixgit70/DEVICEControl.git
cd DEVICEControl
```

Status prüfen:

```bash
git status
```

Remote prüfen:

```bash
git remote -v
```

Vor jeder neuen Arbeit:

```bash
git pull
```

Nach Änderungen:

```bash
git add .
git commit -m "Beschreibung der Änderung"
git push
```

---

## Kurzfassung Release

```bash
git pull

# Versionsnummer ändern

git add .
git commit -m "Release 0.7.11"
git push

git tag -a v0.7.11 -m "DEVICEControl 0.7.11"
git push origin v0.7.11

gh release create v0.7.11 \
  DEVICEControl_0.7.11_macOS_arm64.zip \
  DEVICEControl_0.7.11_Windows_x64.zip \
  DEVICEControl_0.7.11_Linux_x64.zip \
  DEVICEControl_0.7.11_Linux_arm64.zip \
  --title "DEVICEControl 0.7.11" \
  --notes "DEVICEControl Release 0.7.11"
```

Damit ist jede veröffentlichte Version eindeutig über Git-Tag, Quellcode und passende Binaries reproduzierbar.
