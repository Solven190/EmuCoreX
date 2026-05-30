@file:Suppress("UnstableApiUsage", "DEPRECATION")
plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.kotlin.serialization)
    alias(libs.plugins.google.services)
    alias(libs.plugins.firebase.crashlytics)
}

android {
    namespace = "com.sbro.emucorex"
    compileSdk = 37
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "com.sbro.emucorex"
        minSdk = 29
        targetSdk = 37
        versionCode = 89
        versionName = "0.1.7"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID=true",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DANDROID_STL=c++_static",
                    "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
                )
            }
        }
        ndk {
            //noinspection ChromeOsAbiSupport
            abiFilters += "arm64-v8a"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            configure<com.google.firebase.crashlytics.buildtools.gradle.CrashlyticsExtension> {
                nativeSymbolUploadEnabled = true
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
    buildFeatures {
        compose = true
    }
    sourceSets {
        getByName("main") {
            assets.srcDir("src/main/cpp/PCSX2/bin")
            java.srcDir("src/main/cpp/ARM_ANDROID/third_party/SDL/android-project/app/src/main/java")
        }
    }
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
}

val shadercBuildTask = tasks.register<Exec>("buildShadercArm64") {
    val sdkRoot = file(
        System.getenv("ANDROID_HOME")
            ?: System.getenv("ANDROID_SDK_ROOT")
            ?: "${System.getProperty("user.home")}/AppData/Local/Android/Sdk"
    )
    val ndkRoot = sdkRoot.resolve("ndk/${android.ndkVersion}")
    val shadercRoot = ndkRoot.resolve("sources/third_party/shaderc")
    val outDir = layout.buildDirectory.dir("shaderc/out")
    val libsDir = layout.buildDirectory.dir("shaderc/libs")

    inputs.dir(shadercRoot)
    outputs.files(
        outDir.map { it.file("local/arm64-v8a/libglslang.a") },
        outDir.map { it.file("local/arm64-v8a/libOGLCompiler.a") },
        outDir.map { it.file("local/arm64-v8a/libOSDependent.a") },
        outDir.map { it.file("local/arm64-v8a/libshaderc.a") },
        outDir.map { it.file("local/arm64-v8a/libshaderc_util.a") },
        outDir.map { it.file("local/arm64-v8a/libSPIRV.a") },
        outDir.map { it.file("local/arm64-v8a/libHLSL.a") },
        outDir.map { it.file("local/arm64-v8a/libSPIRV-Tools.a") },
        outDir.map { it.file("local/arm64-v8a/libSPIRV-Tools-opt.a") }
    )

    commandLine(
        ndkRoot.resolve("ndk-build.cmd").absolutePath,
        "NDK_PROJECT_PATH=${shadercRoot.absolutePath}",
        "APP_BUILD_SCRIPT=${shadercRoot.resolve("Android.mk").absolutePath}",
        "APP_ABI=arm64-v8a",
        "APP_PLATFORM=android-29",
        "APP_STL=c++_static",
        "NDK_OUT=${outDir.get().asFile.absolutePath}",
        "NDK_LIBS_OUT=${libsDir.get().asFile.absolutePath}",
        "glslang",
        "OGLCompiler",
        "OSDependent",
        "shaderc",
        "shaderc_util",
        "SPIRV",
        "HLSL",
        "SPIRV-Tools",
        "SPIRV-Tools-opt"
    )
}

tasks.configureEach {
    if (name.startsWith("configureCMake") ||
        name.startsWith("buildCMake") ||
        name.startsWith("externalNativeBuild")) {
        dependsOn(shadercBuildTask)
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.core.splashscreen)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.graphics)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.compose.material.icons.extended)
    implementation(libs.androidx.navigation.compose)
    implementation(libs.androidx.datastore.preferences)
    implementation(libs.kotlinx.serialization.json)
    implementation(libs.androidx.documentfile)
    implementation(libs.android.youtube.player.core)
    implementation(platform(libs.firebase.bom))
    implementation(libs.firebase.analytics)
    implementation(libs.firebase.crashlytics)
    implementation(libs.firebase.crashlytics.ndk)
    implementation(libs.firebase.firestore)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.compose.ui.test.junit4)
    debugImplementation(libs.androidx.compose.ui.tooling)
    debugImplementation(libs.androidx.compose.ui.test.manifest)
}
