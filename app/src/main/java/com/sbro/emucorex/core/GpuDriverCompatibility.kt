package com.sbro.emucorex.core

import android.os.Build
import java.io.File
import java.util.Locale

data class SnapdragonGpuProfile(
    val socName: String,
    val adrenoName: String,
    val family: AdrenoFamily
)

/**
 * GPU vendor a driver package targets. The catalog keeps this in the free-form
 * `gpu` string ("Adreno 7xx", "Mali PanVK (Valhall)", "Xclipse (Exynos 2400)"),
 * and driver libraries are identified by markers inside the ELF, because a
 * pack file name is not trustworthy (PanVK packs ship as
 * `libvulkan_freedreno.so` so adrenotools-style installers pick them up).
 */
enum class GpuDriverVendor(val displayName: String) {
    ADRENO("Adreno"),
    MALI("Mali"),
    XCLIPSE("Xclipse"),
    POWERVR("PowerVR"),
    UNKNOWN("Unknown");

    val supportsCustomDrivers: Boolean
        get() = this == ADRENO || this == MALI || this == XCLIPSE
}

/** SoC + GPU label used by the driver manager device card. */
data class GpuDriverDevice(
    val socName: String,
    val gpuName: String
)

/** Catalog driver package extension: which vendor the package targets. */
val RemoteGpuDriver.vendor: GpuDriverVendor
    get() = GpuDriverCompatibility.vendorOfCatalogGpu(gpu)

enum class AdrenoFamily(val catalogToken: String) {
    A6XX("6xx"),
    A7XX("7xx"),
    A8XX("8xx")
}

enum class GpuDriverMatch {
    COMPATIBLE,
    OTHER_FAMILY,
    UNKNOWN
}

/** Maps the detected Snapdragon SoC to the broad Adreno families used by driver packages. */
object GpuDriverRecommendations {
    fun currentDeviceProfile(): SnapdragonGpuProfile? =
        profileForSoc(MobileSocNameMapper.currentDeviceName())

    internal fun profileForSoc(socName: String): SnapdragonGpuProfile? {
        val normalized = socName.lowercase(Locale.US)
        if (!normalized.contains("snapdragon")) return null

        val mapping = SOC_GPU_MAPPINGS.firstOrNull { (socTokens, _) ->
            socTokens.any(normalized::contains)
        }?.second ?: return null
        return SnapdragonGpuProfile(
            socName = socName,
            adrenoName = "Adreno ${mapping.first}",
            family = mapping.second
        )
    }

    fun match(
        driver: RemoteGpuDriver,
        profile: SnapdragonGpuProfile?,
        deviceVendor: GpuDriverVendor? = null
    ): GpuDriverMatch {
        val driverVendor = driver.vendor
        if (driverVendor != GpuDriverVendor.UNKNOWN && driverVendor != GpuDriverVendor.ADRENO) {
            return when {
                deviceVendor == null -> GpuDriverMatch.UNKNOWN
                driverVendor == deviceVendor -> GpuDriverMatch.COMPATIBLE
                else -> GpuDriverMatch.OTHER_FAMILY
            }
        }
        if (driverVendor == GpuDriverVendor.ADRENO &&
            deviceVendor != null &&
            deviceVendor != GpuDriverVendor.ADRENO
        ) {
            return GpuDriverMatch.OTHER_FAMILY
        }

        profile ?: return GpuDriverMatch.UNKNOWN
        val supportedFamilies = supportedFamilies(driver.gpu)
        if (supportedFamilies.isEmpty()) return GpuDriverMatch.UNKNOWN
        if (profile.family !in supportedFamilies) return GpuDriverMatch.OTHER_FAMILY

        val explicitModels = EXPLICIT_ADRENO_MODEL.find(driver.gpu)
            ?.groupValues
            ?.getOrNull(1)
            ?.split('/', ',', ' ')
            ?.map(String::trim)
            ?.filter(String::isNotEmpty)
            .orEmpty()
        val deviceModel = profile.adrenoName.substringAfterLast(' ')
        return if (explicitModels.isNotEmpty() && deviceModel !in explicitModels) {
            GpuDriverMatch.OTHER_FAMILY
        } else {
            GpuDriverMatch.COMPATIBLE
        }
    }

    fun supportedFamilies(gpu: String): Set<AdrenoFamily> = buildSet {
        val normalized = gpu.lowercase(Locale.US)
        AdrenoFamily.entries.forEach { family ->
            if (family.catalogToken in normalized) add(family)
        }
    }

    private val SOC_GPU_MAPPINGS = listOf(
        listOf("8 elite gen 5") to ("840" to AdrenoFamily.A8XX),
        listOf("8 gen 5") to ("829" to AdrenoFamily.A8XX),
        listOf("8 elite") to ("830" to AdrenoFamily.A8XX),
        listOf("8s gen 4") to ("825" to AdrenoFamily.A8XX),
        listOf("8 gen 3") to ("750" to AdrenoFamily.A7XX),
        listOf("8s gen 3") to ("735" to AdrenoFamily.A7XX),
        listOf("8 gen 2") to ("740" to AdrenoFamily.A7XX),
        listOf("8+ gen 1", "8 gen 1") to ("730" to AdrenoFamily.A7XX),
        listOf("7 gen 4") to ("722" to AdrenoFamily.A7XX),
        listOf("7+ gen 3") to ("732" to AdrenoFamily.A7XX),
        listOf("7s gen 3") to ("710" to AdrenoFamily.A7XX),
        listOf("7 gen 3") to ("720" to AdrenoFamily.A7XX),
        listOf("7+ gen 2") to ("725" to AdrenoFamily.A7XX),
        listOf("7s gen 2") to ("710" to AdrenoFamily.A7XX),
        listOf("6 gen 4") to ("810" to AdrenoFamily.A8XX),
        listOf("6 gen 3", "6 gen 1") to ("710" to AdrenoFamily.A7XX),
        listOf("888") to ("660" to AdrenoFamily.A6XX),
        listOf("865") to ("650" to AdrenoFamily.A6XX),
        listOf("855") to ("640" to AdrenoFamily.A6XX),
        listOf("845") to ("630" to AdrenoFamily.A6XX),
        listOf("7 gen 1") to ("644" to AdrenoFamily.A6XX),
        listOf("780g") to ("642" to AdrenoFamily.A6XX),
        listOf("778g") to ("642L" to AdrenoFamily.A6XX),
        listOf("765") to ("620" to AdrenoFamily.A6XX),
        listOf("750g") to ("619" to AdrenoFamily.A6XX),
        listOf("730", "720g") to ("618" to AdrenoFamily.A6XX),
        listOf("4s gen 2", "4 gen 2") to ("613" to AdrenoFamily.A6XX),
        listOf("4 gen 1", "695") to ("619" to AdrenoFamily.A6XX),
        listOf("690") to ("619L" to AdrenoFamily.A6XX),
        listOf("480") to ("619" to AdrenoFamily.A6XX),
        listOf("460") to ("610" to AdrenoFamily.A6XX),
        listOf("685", "680", "665", "662") to ("610" to AdrenoFamily.A6XX),
        listOf("675", "678") to ("612" to AdrenoFamily.A6XX),
        listOf("670") to ("615" to AdrenoFamily.A6XX),
        listOf("660") to ("512" to AdrenoFamily.A6XX),
        listOf("636") to ("509" to AdrenoFamily.A6XX)
    )

    private val EXPLICIT_ADRENO_MODEL = Regex("8xx\\s*\\(([^)]+)\\)", RegexOption.IGNORE_CASE)
}

/**
 * Device and driver package vendor detection for the custom GPU driver manager.
 *
 * Detection is deliberately split into pure functions so unit tests can drive
 * them with arbitrary build strings: [deviceVendor] for the device, and
 * [vendorOfCatalogGpu] / [vendorOfDriverLibrary] for packages. Cross-vendor
 * installs and selections are refused so an Adreno Turnip pack can never be
 * applied to a Mali device and a PanVK pack can never reach an Adreno device.
 */
object GpuDriverCompatibility {
    private val QUALCOMM_SIGNALS = listOf(
        "adreno",
        "qcom",
        "qualcomm",
        "qti",
        "snapdragon",
        "msm",
        "sdm",
        "sm8",
        "sm7",
        "sm6",
        "kalama",
        "lahaina",
        "taro",
        "waipio"
    )

    private val XCLIPSE_SIGNALS = listOf(
        "xclipse",
        "exynos 2200",
        "exynos 2400",
        "exynos 2500",
        "exynos 2600"
    )

    private val MALI_SIGNALS = listOf(
        "mediatek",
        "mtk",
        "dimensity",
        "helio",
        "exynos",
        "mali",
        "kirin",
        "hisilicon",
        "tensor",
        "unisoc",
        "spreadtrum",
        "comtech"
    )

    private val POWERVR_SIGNALS = listOf("powervr", "imgtec", "imagination")

    private val SNAPDRAGON_SOC_PATTERN = Regex("""\bsm[0-9]{3,4}\b""")

    private const val DRIVER_VENDOR_METADATA = "driver_vendor.txt"

    fun deviceVendor(): GpuDriverVendor = deviceVendor(currentDeviceInfo())

    internal fun deviceVendor(deviceInfo: String): GpuDriverVendor {
        val info = deviceInfo.lowercase(Locale.US)
        if (QUALCOMM_SIGNALS.any { it in info }) return GpuDriverVendor.ADRENO
        if (XCLIPSE_SIGNALS.any { it in info }) return GpuDriverVendor.XCLIPSE
        if (POWERVR_SIGNALS.any { it in info }) return GpuDriverVendor.POWERVR
        if (MALI_SIGNALS.any { it in info }) return GpuDriverVendor.MALI
        if (SNAPDRAGON_SOC_PATTERN.containsMatchIn(info)) return GpuDriverVendor.ADRENO
        return GpuDriverVendor.UNKNOWN
    }

    fun supportsCustomDrivers(): Boolean = deviceVendor().supportsCustomDrivers

    /** Adreno-only helper kept for callers that explicitly need Turnip support. */
    fun supportsAdrenoToolsCustomDrivers(): Boolean = deviceVendor() == GpuDriverVendor.ADRENO

    fun currentDevice(): GpuDriverDevice {
        val vendor = deviceVendor()
        val socName = MobileSocNameMapper.currentDeviceName()
            .takeIf { it.isNotBlank() }
            ?: socLabel().ifBlank { vendor.displayName }
        val catalogModel = SocGpuCatalog.forSoc(socName)
        val gpuName = catalogModel?.displayName ?: when (vendor) {
            GpuDriverVendor.ADRENO ->
                GpuDriverRecommendations.currentDeviceProfile()?.adrenoName ?: vendor.displayName
            else -> vendor.displayName
        }
        return GpuDriverDevice(socName = socName, gpuName = gpuName)
    }

    fun currentDeviceInfo(): String = buildList {
        add(Build.BOARD)
        add(Build.BRAND)
        add(Build.DEVICE)
        add(Build.HARDWARE)
        add(Build.MANUFACTURER)
        add(Build.MODEL)
        add(Build.PRODUCT)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            add(Build.SOC_MANUFACTURER)
            add(Build.SOC_MODEL)
        }
    }
        .joinToString(" ")

    fun vendorOfCatalogGpu(gpu: String): GpuDriverVendor {
        val normalized = gpu.lowercase(Locale.US)
        return when {
            "xclipse" in normalized || "exynos" in normalized -> GpuDriverVendor.XCLIPSE
            "mali" in normalized || "panvk" in normalized || "panfrost" in normalized -> GpuDriverVendor.MALI
            "adreno" in normalized || "freedreno" in normalized || "turnip" in normalized -> GpuDriverVendor.ADRENO
            "powervr" in normalized || "imgtec" in normalized || "imagination" in normalized -> GpuDriverVendor.POWERVR
            else -> GpuDriverVendor.UNKNOWN
        }
    }

    /**
     * Which vendor a driver library targets, read from markers inside the ELF.
     * The file name cannot be trusted: PanVK packs for Mali are named
     * `libvulkan_freedreno.so` so adrenotools-style loaders accept them.
     */
    fun vendorOfDriverLibrary(file: File): GpuDriverVendor {
        if (!file.isFile || file.length() <= 0L) return GpuDriverVendor.UNKNOWN
        // Xclipse first: Samsung's Vortek packs are their own stack and must
        // not be caught by a generic Mali marker later.
        if (file.containsAsciiMarker("xclipse") || file.containsAsciiMarker("vortek")) {
            return GpuDriverVendor.XCLIPSE
        }
        if (file.containsAsciiMarker("panvk") ||
            file.containsAsciiMarker("panfrost") ||
            file.containsAsciiMarker("libgles_mali") ||
            file.containsAsciiMarker("libmali") ||
            file.containsAsciiMarker("mali_kbase")
        ) {
            return GpuDriverVendor.MALI
        }
        if (file.containsAsciiMarker("freedreno") ||
            file.containsAsciiMarker("turnip") ||
            file.containsAsciiMarker("adreno")
        ) {
            return GpuDriverVendor.ADRENO
        }
        return GpuDriverVendor.UNKNOWN
    }

    fun isVendorCompatible(
        driverVendor: GpuDriverVendor,
        deviceVendor: GpuDriverVendor = deviceVendor()
    ): Boolean {
        if (driverVendor == GpuDriverVendor.UNKNOWN || deviceVendor == GpuDriverVendor.UNKNOWN) return true
        return driverVendor == deviceVendor
    }

    /** Reads the vendor recorded next to an installed library, if any. */
    fun installedDriverVendor(libraryFile: File): GpuDriverVendor {
        val metadata = libraryFile.parentFile?.let { File(it, DRIVER_VENDOR_METADATA) } ?: return GpuDriverVendor.UNKNOWN
        if (!metadata.isFile) return GpuDriverVendor.UNKNOWN
        val stored = runCatching { metadata.readText().trim() }.getOrNull().orEmpty()
        return GpuDriverVendor.entries.firstOrNull { it.name.equals(stored, ignoreCase = true) }
            ?: GpuDriverVendor.UNKNOWN
    }

    fun writeInstalledDriverVendor(libraryFile: File, vendor: GpuDriverVendor) {
        if (vendor == GpuDriverVendor.UNKNOWN) return
        runCatching {
            libraryFile.parentFile?.let { File(it, DRIVER_VENDOR_METADATA).writeText(vendor.name) }
        }
    }

    private fun socLabel(): String = runCatching {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) Build.SOC_MODEL.orEmpty() else Build.HARDWARE
    }.getOrDefault(Build.HARDWARE).orEmpty()
}

private fun File.containsAsciiMarker(marker: String): Boolean {
    if (marker.isEmpty()) return true
    val needle = marker.lowercase(Locale.US).toByteArray(Charsets.US_ASCII)
    if (needle.isEmpty()) return true
    return try {
        inputStream().buffered(MARKER_SCAN_CHUNK).use { input ->
            val buffer = ByteArray(MARKER_SCAN_CHUNK)
            var carry = ByteArray(0)
            while (true) {
                val read = input.read(buffer)
                if (read <= 0) break
                val haystack = ByteArray(carry.size + read)
                carry.copyInto(haystack)
                buffer.copyInto(haystack, carry.size, 0, read)
                if (haystack.containsAsciiMarker(needle)) return true
                val keep = minOf(needle.size - 1, haystack.size)
                carry = haystack.copyOfRange(haystack.size - keep, haystack.size)
            }
            false
        }
    } catch (_: Exception) {
        false
    }
}

private fun ByteArray.containsAsciiMarker(needle: ByteArray): Boolean {
    if (needle.isEmpty()) return true
    if (size < needle.size) return false
    val last = size - needle.size
    var index = 0
    while (index <= last) {
        var matched = 0
        while (matched < needle.size &&
            ((this[index + matched].toInt() and 0xFF) or 0x20) == needle[matched].toInt()
        ) {
            matched++
        }
        if (matched == needle.size) return true
        index++
    }
    return false
}

private const val MARKER_SCAN_CHUNK = 1 shl 20
