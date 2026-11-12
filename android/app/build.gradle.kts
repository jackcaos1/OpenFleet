plugins {
    id("com.android.application")
}

android {
    namespace = "dev.openfleet.client"   // Platzhalter, kein Markenname
    compileSdk = 34
    ndkVersion = "26.1.10909125"

    defaultConfig {
        applicationId = "dev.openfleet.client"
        minSdk = 26
        targetSdk = 34
        versionCode = 72
        versionName = "0.1.72-debug"

        ndk {
            // arm64-v8a: echte Geraete, x86_64: Emulator
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}
