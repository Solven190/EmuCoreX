package com.sbro.emucorex.core.utils

import android.content.Context
import android.net.ConnectivityManager
import android.util.Log
import java.net.Inet6Address
import java.net.NetworkInterface
import java.security.SecureRandom
import java.util.Collections

object NetworkAdapterCollector {
    private const val TAG = "NetworkAdapterCollector"

    @JvmStatic
    private external fun onAdaptersCollected(adapters: Array<AdapterInfo>, stableMac: String)

    @JvmStatic
    @Suppress("DEPRECATION")
    fun collectAdapters(context: Context): List<AdapterInfo> {
        val result = runCatching {
            val connectivityManager = context.getSystemService(ConnectivityManager::class.java)
            val linkPropertiesByInterface = connectivityManager?.allNetworks.orEmpty()
                .mapNotNull(connectivityManager::getLinkProperties)
                .filter { !it.interfaceName.isNullOrBlank() }
                .associateBy { it.interfaceName!! }

            val interfaces = NetworkInterface.getNetworkInterfaces()
                ?.let(Collections::list)
                .orEmpty()
            interfaces.map { networkInterface ->
                val linkProperties = linkPropertiesByInterface[networkInterface.name]
                AdapterInfo(
                    name = networkInterface.name.orEmpty(),
                    displayName = friendlyName(networkInterface.name.orEmpty()),
                    isUp = runCatching(networkInterface::isUp).getOrDefault(false),
                    isLoopback = runCatching(networkInterface::isLoopback).getOrDefault(false),
                    isVirtual = runCatching(networkInterface::isVirtual).getOrDefault(false),
                    supportsMulticast = runCatching(networkInterface::supportsMulticast).getOrDefault(false),
                    mtu = linkProperties?.mtu ?: runCatching(networkInterface::getMTU).getOrDefault(0),
                    ipAddresses = Collections.list(networkInterface.inetAddresses)
                        .mapNotNull { it.hostAddress?.substringBefore('%') }
                        .toTypedArray(),
                    dnsServers = linkProperties?.dnsServers
                        ?.mapNotNull { it.hostAddress?.substringBefore('%') }
                        ?.toTypedArray()
                        ?: emptyArray(),
                    routes = linkProperties?.routes?.map { route ->
                        val destination = route.destination
                        val destinationAddress = destination.address
                        val gateway = route.gateway
                        val ipv6 = destinationAddress is Inet6Address
                        AdapterInfo.RouteInfo(
                            destination = destination.toString(),
                            address = destinationAddress.hostAddress.orEmpty().substringBefore('%'),
                            gateway = gateway?.hostAddress.orEmpty().substringBefore('%'),
                            prefix = destination.prefixLength,
                            isIPv6 = ipv6,
                            hasGateway = gateway != null && !gateway.isAnyLocalAddress,
                            isDefault = route.isDefaultRoute,
                            isHostRoute = destination.prefixLength == if (ipv6) 128 else 32,
                            isNetworkRoute = !route.isDefaultRoute && destination.prefixLength != if (ipv6) 128 else 32,
                            isDirect = gateway == null || gateway.isAnyLocalAddress,
                            isAnyLocal = destinationAddress.isAnyLocalAddress,
                            isSiteLocal = destinationAddress.isSiteLocalAddress,
                            isLoopback = destinationAddress.isLoopbackAddress,
                            isLinkLocal = destinationAddress.isLinkLocalAddress,
                            isMulticast = destinationAddress.isMulticastAddress
                        )
                    }?.toTypedArray() ?: emptyArray()
                )
            }
        }.onFailure { Log.e(TAG, "Unable to enumerate Android network adapters", it) }
            .getOrDefault(emptyList())

        runCatching { onAdaptersCollected(result.toTypedArray(), generateUniqueMac(context)) }
            .onFailure { Log.e(TAG, "Unable to publish network adapters to native DEV9", it) }
        return result
    }

    @JvmStatic
    @Synchronized
    fun generateUniqueMac(context: Context): String {
        val preferences = context.getSharedPreferences("dev9_network", Context.MODE_PRIVATE)
        preferences.getString("stable_mac", null)?.takeIf { STABLE_MAC.matches(it) }?.let { return it }
        val bytes = ByteArray(6).also(SecureRandom()::nextBytes)
        bytes[0] = ((bytes[0].toInt() and 0xFC) or 0x02).toByte()
        return bytes.joinToString(":") { "%02X".format(it.toInt() and 0xFF) }
            .also { preferences.edit().putString("stable_mac", it).apply() }
    }

    private fun friendlyName(name: String): String = when {
        name == "wlan0" || name.startsWith("wifi") -> "Wi-Fi"
        name.startsWith("tun") || name.startsWith("ppp") || name.startsWith("wg") -> "VPN"
        name.startsWith("rmnet") || name.startsWith("ccmni") || name.startsWith("pdp") -> "Mobile data"
        name.startsWith("eth") -> "Ethernet"
        else -> name
    }

    private val STABLE_MAC = Regex("^(?:[0-9A-F]{2}:){5}[0-9A-F]{2}$")

    data class AdapterInfo(
        @JvmField val name: String = "",
        @JvmField val displayName: String = "",
        @JvmField val isUp: Boolean = false,
        @JvmField val isLoopback: Boolean = false,
        @JvmField val isVirtual: Boolean = false,
        @JvmField val supportsMulticast: Boolean = false,
        @JvmField val mtu: Int = 0,
        @JvmField val ipAddresses: Array<String> = emptyArray(),
        @JvmField val dnsServers: Array<String> = emptyArray(),
        @JvmField val routes: Array<RouteInfo> = emptyArray()
    ) {
        data class RouteInfo(
            @JvmField val destination: String = "",
            @JvmField val address: String = "",
            @JvmField val gateway: String = "",
            @JvmField val prefix: Int = 0,
            @JvmField val isIPv6: Boolean = false,
            @JvmField val hasGateway: Boolean = false,
            @JvmField val isDefault: Boolean = false,
            @JvmField val isHostRoute: Boolean = false,
            @JvmField val isNetworkRoute: Boolean = false,
            @JvmField val isDirect: Boolean = false,
            @JvmField val isAnyLocal: Boolean = false,
            @JvmField val isSiteLocal: Boolean = false,
            @JvmField val isLoopback: Boolean = false,
            @JvmField val isLinkLocal: Boolean = false,
            @JvmField val isMulticast: Boolean = false
        )
    }
}
