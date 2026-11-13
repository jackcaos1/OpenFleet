#pragma once
// Einzige Stelle fuer die Build-Nummer. Bei jedem neuen APK hochzaehlen (android/app/build.gradle.kts versionCode/versionName mitziehen).
// Anzeige: "d<Build>" fuer Debug-Builds (z. B. d57), unten rechts im Hauptmenue.
#define FLEET_VERSION_MAJOR 0
#define FLEET_VERSION_MINOR 1
#define FLEET_BUILD 89
#define FLEET_BUILD_KIND "d"   // d = Debug, r = Release
