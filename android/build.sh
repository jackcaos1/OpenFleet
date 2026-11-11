#!/usr/bin/env bash
# Baut die Debug-APK mit JDK 17 (Gradle 8.9 kommt mit neueren Java-Versionen nicht klar).
# Aufruf: android/build.sh [gradle-Ziel ...]   Standard: assembleDebug
# Optional: JAVA17_HOME, ANDROID_HOME vorher setzen.
set -euo pipefail
cd "$(dirname "$0")"

JDK="${JAVA17_HOME:-}"
if [ -z "$JDK" ]; then
  for c in /usr/lib/jvm/java-17-openjdk /usr/lib/jvm/java-17-openjdk-amd64 /usr/lib/jvm/jdk-17* /usr/lib/jvm/temurin-17*; do
    [ -x "$c/bin/java" ] && JDK="$c" && break
  done
fi
if [ -z "$JDK" ]; then
  echo "JDK 17 nicht gefunden. Arch: sudo pacman -S jdk17-openjdk, oder JAVA17_HOME setzen." >&2
  exit 1
fi
export JAVA_HOME="$JDK"
export PATH="$JAVA_HOME/bin:$PATH"

SDK="${ANDROID_HOME:-/opt/android-sdk}"
export ANDROID_HOME="$SDK"
[ -f local.properties ] || echo "sdk.dir=$SDK" > local.properties

if [ "$#" -eq 0 ]; then set -- assembleDebug; fi
./gradlew "$@"

APK=app/build/outputs/apk/debug/app-debug.apk
[ -f "$APK" ] && echo "APK: $(pwd)/$APK"
