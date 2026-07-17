@file:Suppress("UnstableApiUsage", "DEPRECATION")

import java.util.Properties
import java.security.MessageDigest
import java.util.zip.ZipFile

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.kotlin.serialization)
    alias(libs.plugins.google.services)
}

val localProperties = Properties().apply {
    val propertiesFile = rootProject.file("local.properties")
    if (propertiesFile.isFile) {
        propertiesFile.inputStream().use(::load)
    }
}

fun localProperty(name: String): String? = localProperties.getProperty(name)?.takeIf { it.isNotBlank() }

fun buildConfigString(value: String): String = "\"" + value
    .replace("\\", "\\\\")
    .replace("\"", "\\\"") + "\""

val feedbackEndpoint = localProperty("emucorex.feedback.endpoint").orEmpty()
val feedbackApiKey = localProperty("emucorex.feedback.apiKey").orEmpty()

val releaseStoreFilePath = localProperty("emucorex.release.storeFile")
val releaseStorePassword = localProperty("emucorex.release.storePassword")
val releaseKeyAlias = localProperty("emucorex.release.keyAlias")
val releaseKeyPassword = localProperty("emucorex.release.keyPassword")
val releaseSigningConfigured = listOf(
    releaseStoreFilePath,
    releaseStorePassword,
    releaseKeyAlias,
    releaseKeyPassword
).all { it != null }

android {
    namespace = "com.sbro.emucorex"
    compileSdk = 37
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "com.sbro.emucorex"
        minSdk = 29
        targetSdk = 37
        versionCode = 119
        versionName = "0.2.6"

        buildConfigField("String", "FEEDBACK_ENDPOINT", buildConfigString(feedbackEndpoint))
        buildConfigField("String", "FEEDBACK_API_KEY", buildConfigString(feedbackApiKey))

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

    signingConfigs {
        if (releaseSigningConfigured) {
            create("release") {
                storeFile = rootProject.file(releaseStoreFilePath!!)
                storePassword = releaseStorePassword!!
                keyAlias = releaseKeyAlias!!
                keyPassword = releaseKeyPassword!!
            }
        }
    }

    buildTypes {
        release {
            if (releaseSigningConfigured) {
                signingConfig = signingConfigs.getByName("release")
            }
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
                "src/main/cpp/PCSX2/3rdparty/SDL3/android-project/app/proguard-rules.pro"
            )
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
        buildConfig = true
    }
    sourceSets {
        getByName("main") {
            assets.srcDir("src/main/cpp/PCSX2/bin")
            java.srcDir("src/main/cpp/PCSX2/3rdparty/SDL3/android-project/app/src/main/java")
        }
    }
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }
    bundle {
        language {
            enableSplit = false
        }
    }
    lint {
        lintConfig = file("lint.xml")
    }
}

val bundledPatchesArchive = layout.projectDirectory.file("src/main/cpp/PCSX2/bin/resources/patches.zip")
val bundledPatchesSha256 = "aa14a46908c1114e0c0f1c06accbfa7efc83c1768fb501699e043cbdb1af797f"

val verifyBundledPatches by tasks.registering {
    group = "verification"
    description = "Verifies the official PCSX2 widescreen/no-interlacing patch archive bundled in the APK."
    inputs.file(bundledPatchesArchive)
    doLast {
        val archive = bundledPatchesArchive.asFile
        check(archive.isFile) {
            "Missing ${archive.path}. Download patches.zip from the official PCSX2/pcsx2_patches latest release."
        }
        val actualSha256 = MessageDigest.getInstance("SHA-256")
            .digest(archive.readBytes())
            .joinToString("") { byte -> "%02x".format(byte.toInt() and 0xff) }
        check(actualSha256 == bundledPatchesSha256) {
            "Unexpected patches.zip digest: $actualSha256. Verify the official release and update the pinned digest."
        }
        ZipFile(archive).use { zip ->
            val patchEntries = zip.entries().asSequence()
                .count { entry -> !entry.isDirectory && entry.name.endsWith(".pnach", ignoreCase = true) }
            check(patchEntries >= 1_000) {
                "patches.zip is valid ZIP data but contains only $patchEntries PNACH files."
            }
        }
    }
}

tasks.matching { it.name == "preBuild" }.configureEach {
    dependsOn(verifyBundledPatches)
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
    implementation(libs.androidx.fragment)
    implementation(libs.google.play.billing)
    implementation(libs.google.play.review)
    implementation(libs.google.play.review.ktx)
    implementation(libs.androidx.work.runtime)
    implementation(libs.android.youtube.player.core)
    implementation(platform(libs.firebase.bom))
    implementation(libs.firebase.analytics)
    implementation(libs.firebase.firestore)
    implementation(libs.firebase.auth)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.compose.ui.test.junit4)
    debugImplementation(libs.androidx.compose.ui.tooling)
    debugImplementation(libs.androidx.compose.ui.test.manifest)
}
