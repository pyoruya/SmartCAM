package com.smartcam

import android.Manifest
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.camera.view.PreviewView
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private lateinit var statusText: TextView
    private lateinit var previewView: PreviewView
    private var service: CameraStreamService? = null
    private var bound = false

    private val requiredPermissions = buildList {
        add(Manifest.permission.CAMERA)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            add(Manifest.permission.POST_NOTIFICATIONS)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            add(Manifest.permission.FOREGROUND_SERVICE)
        }
    }.toTypedArray()

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        val allGranted = results.values.all { it }
        if (allGranted) {
            startStreamingService()
        } else {
            statusText.text = getString(R.string.status_no_permission)
        }
    }

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            val localBinder = binder as CameraStreamService.LocalBinder
            service = localBinder.service
            bound = true

            localBinder.service.setStateListener(object : CameraStreamService.StateListener {
                override fun onStateChanged(state: CameraStreamService.State, message: String) {
                    runOnUiThread { updateUI(state, message) }
                }
            })
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            service = null
            bound = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.status_text)
        previewView = findViewById(R.id.preview_view)

        val missingPermissions = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isEmpty()) {
            startStreamingService()
        } else {
            permissionLauncher.launch(missingPermissions.toTypedArray())
        }
    }

    override fun onDestroy() {
        if (bound) {
            service?.setStateListener(null)
            unbindService(serviceConnection)
            bound = false
        }
        super.onDestroy()
    }

    private fun startStreamingService() {
        val intent = Intent(this, CameraStreamService::class.java)
        ContextCompat.startForegroundService(this, intent)
        bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
        statusText.text = getString(R.string.status_waiting)
    }

    private fun updateUI(state: CameraStreamService.State, message: String) {
        statusText.text = when (state) {
            CameraStreamService.State.IDLE ->
                getString(R.string.status_initializing)
            CameraStreamService.State.WAITING_FOR_HOST ->
                getString(R.string.status_waiting)
            CameraStreamService.State.STREAMING ->
                getString(R.string.status_streaming)
            CameraStreamService.State.ERROR ->
                getString(R.string.status_error, message)
        }
    }
}
