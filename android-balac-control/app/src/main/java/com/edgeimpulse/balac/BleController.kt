package com.edgeimpulse.balac

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import java.util.UUID

/**
 * Minimal BLE client for the BalaC-Nesso balance robot.
 *
 * Uses the Nordic UART Service (NUS): the robot exposes an RX characteristic we write
 * text commands to, and a TX characteristic that notifies telemetry back.
 *
 * Caller must hold BLUETOOTH_SCAN / BLUETOOTH_CONNECT (API 31+) or location (<= API 30)
 * before invoking [startScan].
 */
@SuppressLint("MissingPermission")
class BleController(
    private val context: Context,
    private val listener: Listener
) {
    interface Listener {
        fun onStatus(text: String)
        fun onConnected(connected: Boolean)
        fun onTelemetry(text: String)
    }

    companion object {
        val SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val RX_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E") // app -> robot
        val TX_UUID: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E") // robot -> app
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val SCAN_TIMEOUT_MS = 12000L
    }

    private val main = Handler(Looper.getMainLooper())
    private val btManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val adapter: BluetoothAdapter? = btManager.adapter
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var rxChar: BluetoothGattCharacteristic? = null
    private var scanning = false

    fun isBluetoothOn(): Boolean = adapter?.isEnabled == true

    fun startScan() {
        val a = adapter
        if (a == null || !a.isEnabled) {
            post { listener.onStatus("Bluetooth is off") }
            return
        }
        if (scanning) return
        scanner = a.bluetoothLeScanner
        val filters = listOf(
            ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE_UUID)).build()
        )
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        scanning = true
        post { listener.onStatus("Scanning for BalaC-Nesso…") }
        scanner?.startScan(filters, settings, scanCallback)
        main.postDelayed({
            if (scanning) {
                stopScan()
                if (gatt == null) post { listener.onStatus("Not found. Tap Connect to retry.") }
            }
        }, SCAN_TIMEOUT_MS)
    }

    fun stopScan() {
        if (scanning) {
            scanning = false
            try {
                scanner?.stopScan(scanCallback)
            } catch (_: Exception) {
            }
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device ?: return
            stopScan()
            post { listener.onStatus("Found ${device.name ?: device.address}, connecting…") }
            connect(device)
        }

        override fun onScanFailed(errorCode: Int) {
            scanning = false
            post { listener.onStatus("Scan failed ($errorCode)") }
        }
    }

    private fun connect(device: BluetoothDevice) {
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    fun disconnect() {
        stopScan()
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        rxChar = null
        post { listener.onConnected(false) }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    post { listener.onStatus("Connected, discovering services…") }
                    g.discoverServices()
                }

                BluetoothProfile.STATE_DISCONNECTED -> {
                    rxChar = null
                    post {
                        listener.onStatus("Disconnected")
                        listener.onConnected(false)
                    }
                }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val service = g.getService(SERVICE_UUID)
            if (service == null) {
                post { listener.onStatus("UART service not found") }
                return
            }
            rxChar = service.getCharacteristic(RX_UUID)
            val txChar = service.getCharacteristic(TX_UUID)
            if (txChar != null) {
                g.setCharacteristicNotification(txChar, true)
                txChar.getDescriptor(CCCD_UUID)?.let { cccd ->
                    enableNotifications(g, cccd)
                }
            }
            post {
                listener.onStatus("Ready")
                listener.onConnected(true)
            }
        }

        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            ch: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleTelemetry(value)
        }

        @Deprecated("Kept for Android 12 and below")
        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION")
            handleTelemetry(ch.value ?: ByteArray(0))
        }
    }

    private fun enableNotifications(g: BluetoothGatt, cccd: BluetoothGattDescriptor) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        } else {
            @Suppress("DEPRECATION")
            cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            @Suppress("DEPRECATION")
            g.writeDescriptor(cccd)
        }
    }

    private fun handleTelemetry(value: ByteArray) {
        val text = String(value).trim()
        if (text.isNotEmpty()) post { listener.onTelemetry(text) }
    }

    /** Send a raw command line to the robot (e.g. "D,70,0\n"). */
    fun send(cmd: String) {
        val g = gatt ?: return
        val ch = rxChar ?: return
        val bytes = cmd.toByteArray()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeCharacteristic(ch, bytes, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE)
        } else {
            @Suppress("DEPRECATION")
            ch.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            @Suppress("DEPRECATION")
            ch.value = bytes
            @Suppress("DEPRECATION")
            g.writeCharacteristic(ch)
        }
    }

    fun drive(throttle: Int, steer: Int) = send("D,$throttle,$steer\n")
    fun stop() = send("S\n")
    fun toggleMode() = send("M\n")

    private fun post(block: () -> Unit) {
        main.post(block)
    }
}
