#pragma once
// FileSource ueber den von MainActivity.java verwalteten Spielordner (Storage Access Framework, ueber JNI).
#include <jni.h>

#include <memory>

#include "io/FileSource.h"

namespace fleet {

// activity: globale Referenz auf die MainActivity. Der aufrufende Thread muss an die JVM angehaengt sein.
std::unique_ptr<FileSource> OpenSafFolder(JavaVM* vm, jobject activity);

}  // namespace fleet
