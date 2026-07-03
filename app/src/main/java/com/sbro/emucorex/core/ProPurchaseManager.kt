package com.sbro.emucorex.core

import android.app.Activity
import android.content.Context
import android.util.Log
import androidx.annotation.StringRes
import com.android.billingclient.api.AcknowledgePurchaseParams
import com.android.billingclient.api.BillingClient
import com.android.billingclient.api.BillingClientStateListener
import com.android.billingclient.api.BillingFlowParams
import com.android.billingclient.api.BillingResult
import com.android.billingclient.api.PendingPurchasesParams
import com.android.billingclient.api.ProductDetails
import com.android.billingclient.api.Purchase
import com.android.billingclient.api.PurchasesUpdatedListener
import com.android.billingclient.api.QueryProductDetailsParams
import com.android.billingclient.api.QueryPurchasesParams
import com.sbro.emucorex.R
import com.sbro.emucorex.data.AppPreferences
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch

private const val PRO_PRODUCT_ID = "emucorex_pro"
private const val TAG = "ProPurchaseManager"

data class ProPurchaseState(
    val isProUnlocked: Boolean = false,
    val isBillingReady: Boolean = false,
    val isPurchaseInProgress: Boolean = false,
    val isProductLoading: Boolean = false,
    val isProductAvailable: Boolean = false,
    val productTitle: String? = null,
    val productPrice: String? = null,
    @StringRes val messageResId: Int? = null
)

class ProPurchaseManager private constructor(context: Context) : PurchasesUpdatedListener {

    private val appContext = context.applicationContext
    private val preferences = AppPreferences(appContext)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var productDetails: ProductDetails? = null

    private val billingClient = BillingClient.newBuilder(appContext)
        .setListener(this)
        .enablePendingPurchases(
            PendingPurchasesParams.newBuilder()
                .enableOneTimeProducts()
                .build()
        )
        .enableAutoServiceReconnection()
        .build()

    private val _state = MutableStateFlow(ProPurchaseState())
    val state: StateFlow<ProPurchaseState> = _state.asStateFlow()

    init {
        scope.launch {
            preferences.proUnlocked.distinctUntilChanged().collect { unlocked ->
                _state.value = _state.value.copy(isProUnlocked = unlocked)
            }
        }
        connect()
    }

    fun connect() {
        if (billingClient.isReady) {
            _state.value = _state.value.copy(isBillingReady = true)
            queryProductDetails(showMessage = false)
            restorePurchases(showMessage = false)
            return
        }

        billingClient.startConnection(object : BillingClientStateListener {
            override fun onBillingSetupFinished(billingResult: BillingResult) {
                val ready = billingResult.responseCode == BillingClient.BillingResponseCode.OK
                _state.value = _state.value.copy(
                    isBillingReady = ready,
                    messageResId = if (ready) null else R.string.pro_message_unavailable
                )
                if (ready) {
                    queryProductDetails(showMessage = false)
                    restorePurchases(showMessage = false)
                }
            }

            override fun onBillingServiceDisconnected() {
                _state.value = _state.value.copy(isBillingReady = false)
            }
        })
    }

    fun purchase(activity: Activity) {
        if (_state.value.isProUnlocked) {
            _state.value = _state.value.copy(messageResId = R.string.pro_message_already_active)
            return
        }
        if (!billingClient.isReady) {
            connect()
            _state.value = _state.value.copy(messageResId = R.string.pro_message_unavailable)
            return
        }

        val details = productDetails
        if (details == null) {
            queryProductDetails(showMessage = true)
            return
        }

        val productParamsBuilder = BillingFlowParams.ProductDetailsParams.newBuilder()
            .setProductDetails(details)
        details.oneTimePurchaseOfferDetailsList?.firstOrNull()?.offerToken?.takeIf { it.isNotBlank() }?.let {
            productParamsBuilder.setOfferToken(it)
        }

        val flowParams = BillingFlowParams.newBuilder()
            .setProductDetailsParamsList(listOf(productParamsBuilder.build()))
            .build()

        _state.value = _state.value.copy(isPurchaseInProgress = true, messageResId = null)
        val result = billingClient.launchBillingFlow(activity, flowParams)
        if (result.responseCode != BillingClient.BillingResponseCode.OK) {
            _state.value = _state.value.copy(
                isPurchaseInProgress = false,
                messageResId = R.string.pro_message_purchase_open_failed
            )
        }
    }

    fun restorePurchases(showMessage: Boolean = true) {
        if (!billingClient.isReady) {
            connect()
            if (showMessage) {
                _state.value = _state.value.copy(messageResId = R.string.pro_message_unavailable)
            }
            return
        }

        val params = QueryPurchasesParams.newBuilder()
            .setProductType(BillingClient.ProductType.INAPP)
            .build()
        billingClient.queryPurchasesAsync(params) { billingResult, purchases ->
            if (billingResult.responseCode == BillingClient.BillingResponseCode.OK) {
                val hasPro = purchases.any(::isActiveProPurchase)
                if (hasPro) {
                    handlePurchases(purchases)
                }
                if (showMessage) {
                    _state.value = _state.value.copy(
                        messageResId = if (hasPro) R.string.pro_message_active else R.string.pro_message_restore_missing
                    )
                }
            } else if (showMessage) {
                _state.value = _state.value.copy(messageResId = R.string.pro_message_restore_failed)
            }
        }
    }

    fun clearMessage() {
        _state.value = _state.value.copy(messageResId = null)
    }

    override fun onPurchasesUpdated(billingResult: BillingResult, purchases: MutableList<Purchase>?) {
        when (billingResult.responseCode) {
            BillingClient.BillingResponseCode.OK -> {
                if (!purchases.isNullOrEmpty()) {
                    handlePurchases(purchases)
                }
            }
            BillingClient.BillingResponseCode.USER_CANCELED -> {
                _state.value = _state.value.copy(isPurchaseInProgress = false)
            }
            else -> {
                _state.value = _state.value.copy(
                    isPurchaseInProgress = false,
                    messageResId = R.string.pro_message_purchase_failed
                )
            }
        }
    }

    private fun queryProductDetails(showMessage: Boolean) {
        _state.value = _state.value.copy(
            isProductLoading = true,
            isProductAvailable = false,
            productTitle = null,
            productPrice = null,
            messageResId = null
        )

        val product = QueryProductDetailsParams.Product.newBuilder()
            .setProductId(PRO_PRODUCT_ID)
            .setProductType(BillingClient.ProductType.INAPP)
            .build()
        val params = QueryProductDetailsParams.newBuilder()
            .setProductList(listOf(product))
            .build()

        billingClient.queryProductDetailsAsync(params) { billingResult, result ->
            if (billingResult.responseCode != BillingClient.BillingResponseCode.OK) {
                Log.w(TAG, "Product details query failed: code=${billingResult.responseCode}, message=${billingResult.debugMessage}")
                _state.value = _state.value.copy(
                    isProductLoading = false,
                    isProductAvailable = false,
                    messageResId = if (showMessage) R.string.pro_message_unavailable else null
                )
                return@queryProductDetailsAsync
            }

            val details = result.productDetailsList.firstOrNull { it.productId == PRO_PRODUCT_ID }
            if (details == null) {
                Log.w(TAG, "Product details missing for $PRO_PRODUCT_ID. Returned products=${result.productDetailsList.map { it.productId }}")
                productDetails = null
                _state.value = _state.value.copy(
                    isProductLoading = false,
                    isProductAvailable = false,
                    productTitle = null,
                    productPrice = null,
                    messageResId = if (showMessage) R.string.pro_message_unavailable else null
                )
                return@queryProductDetailsAsync
            }

            productDetails = details
            _state.value = _state.value.copy(
                isProductLoading = false,
                isProductAvailable = true,
                productTitle = details.title,
                productPrice = details.oneTimePurchaseOfferDetailsList?.firstOrNull()?.formattedPrice
                    ?: details.oneTimePurchaseOfferDetails?.formattedPrice,
                messageResId = null
            )
        }
    }

    private fun handlePurchases(purchases: List<Purchase>) {
        purchases.filter(::isActiveProPurchase).forEach { purchase ->
            if (purchase.isAcknowledged) {
                unlockPro()
            } else {
                val params = AcknowledgePurchaseParams.newBuilder()
                    .setPurchaseToken(purchase.purchaseToken)
                    .build()
                billingClient.acknowledgePurchase(params) { result ->
                    if (result.responseCode == BillingClient.BillingResponseCode.OK) {
                        unlockPro()
                    } else {
                        _state.value = _state.value.copy(
                            isPurchaseInProgress = false,
                            messageResId = R.string.pro_message_pending_confirmation
                        )
                    }
                }
            }
        }
    }

    private fun unlockPro() {
        scope.launch {
            preferences.setProUnlocked(true)
            _state.value = _state.value.copy(
                isProUnlocked = true,
                isPurchaseInProgress = false,
                messageResId = R.string.pro_message_active
            )
        }
    }

    private fun isActiveProPurchase(purchase: Purchase): Boolean {
        return purchase.purchaseState == Purchase.PurchaseState.PURCHASED &&
            purchase.products.contains(PRO_PRODUCT_ID)
    }

    companion object {
        @Volatile
        private var instance: ProPurchaseManager? = null

        fun getInstance(context: Context): ProPurchaseManager {
            return instance ?: synchronized(this) {
                instance ?: ProPurchaseManager(context).also { instance = it }
            }
        }
    }
}
