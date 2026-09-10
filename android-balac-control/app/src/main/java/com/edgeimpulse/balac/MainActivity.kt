package com.edgeimpulse.balac

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.MotionEvent
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity(), BleController.Listener {

    private lateinit var ble: BleController
    private lateinit var statusText: TextView
    private lateinit var telemetryText: TextView
    private lateinit var connectButton: Button
    private var connected = false

    // Fixed drive magnitude (-100..100) applied while a direction button is held.
    private val speed = 70

    private val permLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        if (result.values.all { it }) onPermissionsGranted()
        else toast("Bluetooth permissions are required")
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.statusText)
        telemetryText = findViewById(R.id.telemetryText)
        connectButton = findViewById(R.id.connectButton)

        ble = BleController(this, this)

        connectButton.setOnClickListener {
            if (connected) ble.disconnect() else ensurePermissionsThenScan()
        }

        bindDrive(R.id.btnForward, speed, 0)
        bindDrive(R.id.btnBack, -speed, 0)
        bindDrive(R.id.btnLeft, 0, -speed)
        bindDrive(R.id.btnRight, 0, speed)
        findViewById<Button>(R.id.btnStop).setOnClickListener { ble.stop() }
        findViewById<Button>(R.id.btnMode).setOnClickListener { ble.toggleMode() }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun bindDrive(id: Int, throttle: Int, steer: Int) {
        findViewById<Button>(id).setOnTouchListener { v, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    ble.drive(throttle, steer)
                    v.isPressed = true
                    true
                }

                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    ble.stop()
                    v.isPressed = false
                    v.performClick()
                    true
                }

                else -> false
            }
        }
    }

    private fun ensurePermissionsThenScan() {
        val needed = requiredPermissions().filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (needed.isEmpty()) onPermissionsGranted()
        else permLauncher.launch(needed.toTypedArray())
    }

    private fun onPermissionsGranted() {
        if (!ble.isBluetoothOn()) {
            startActivity(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
            toast("Enable Bluetooth, then tap Connect again")
        } else {
            ble.startScan()
        }
    }

    private fun requiredPermissions(): List<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            listOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    override fun onStatus(text: String) {
        statusText.text = text
    }

    override fun onConnected(c: Boolean) {
        connected = c
        connectButton.text = getString(if (c) R.string.disconnect else R.string.connect)
    }

    override fun onTelemetry(text: String) {
        // Expected: "T,<batt>,<angle>,<standing>"
        val p = text.split(",")
        telemetryText.text = if (p.size >= 4 && p[0] == "T") {
            getString(R.string.telemetry_fmt, p[1], p[2], if (p[3] == "1") "yes" else "no")
        } else {
            text
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        ble.disconnect()
    }

    private fun toast(msg: String) = Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
}
