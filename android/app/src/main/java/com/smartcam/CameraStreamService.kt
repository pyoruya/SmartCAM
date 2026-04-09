package com.smartcam

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.util.Log
import android.util.Size
import androidx.camera.core.CameraSelector
import androidx.camera.core.Preview
import androidx.camera.core.SurfaceRequest
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleService

/**
 * Foreground service that manages the full pipeline:
 * Camera capture -> H.264 encoding -> TCP streaming to PC host.
 *
 * Runs as a foreground service so the camera keeps streaming
 * even when the user switches away from the app.
 */
class CameraStreamService : LifecycleService() {

    companion object {
        private const val TAG = "CameraStreamService"
        private const val CHANNEL_ID = "smartcam_streaming"
        private const val NOTIFICATION_ID = 1
    }

    enum class State {
        IDLE,
        WAITING_FOR_HOST,
        STREAMING,
        ERROR
    }

    interface StateListener {
        fun onStateChanged(state: State, message: String)
    }

    inner class LocalBinder : Binder() {
        val service: CameraStreamService get() = this@CameraStreamService
    }

    private val binder = LocalBinder()
    private val server = StreamServer()
    private var encoder: H264Encoder? = null
    private var cameraProvider: ProcessCameraProvider? = null
    private var stateListener: StateListener? = null
    private var currentState = State.IDLE

    fun setStateListener(listener: StateListener?) {
        stateListener = listener
        listener?.onStateChanged(currentState, "")
    }

    override fun onBind(intent: Intent): IBinder {
        super.onBind(intent)
        return binder
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()

        server.setListener(object : StreamServer.Listener {
            override fun onClientConnected() {
                updateState(State.STREAMING, "Host connected")
            }

            override fun onFormatSelected(width: Int, height: Int, fps: Int, bitrate: Int) {
                Log.i(TAG, "Format selected: ${width}x${height}@${fps}, ${bitrate}bps")
                startEncoder(width, height, fps, bitrate)
            }

            override fun onClientDisconnected() {
                stopEncoder()
                updateState(State.WAITING_FOR_HOST, "Host disconnected, waiting...")
            }

            override fun onError(e: Exception) {
                updateState(State.ERROR, e.message ?: "Unknown error")
            }
        })
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)
        startForeground()
        server.start()
        updateState(State.WAITING_FOR_HOST, "Waiting for PC connection on port ${StreamServer.DEFAULT_PORT}")
        return START_STICKY
    }

    override fun onDestroy() {
        server.stop()
        stopEncoder()
        releaseCameraProvider()
        super.onDestroy()
    }

    private fun startEncoder(width: Int, height: Int, fps: Int, bitrate: Int) {
        stopEncoder()

        val enc = H264Encoder()
        encoder = enc

        val encoderSurface = enc.start(width, height, fps, bitrate, object : H264Encoder.Callback {
            override fun onEncodedData(data: ByteArray, offset: Int, size: Int, isKeyFrame: Boolean) {
                server.sendNalUnit(data, offset, size)
            }

            override fun onError(e: Exception) {
                Log.e(TAG, "Encoder error", e)
                updateState(State.ERROR, "Encoder error: ${e.message}")
            }
        })

        // Start CameraX and bind to encoder surface
        startCamera(encoderSurface, Size(width, height))
    }

    private fun stopEncoder() {
        releaseCameraProvider()
        encoder?.stop()
        encoder = null
    }

    private fun startCamera(encoderSurface: android.view.Surface, targetSize: Size) {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(this)
        cameraProviderFuture.addListener({
            try {
                val provider = cameraProviderFuture.get()
                cameraProvider = provider

                // Preview use case — renders to encoder surface
                val preview = Preview.Builder()
                    .setTargetResolution(targetSize)
                    .build()

                preview.setSurfaceProvider { request ->
                    request.provideSurface(encoderSurface, ContextCompat.getMainExecutor(this)) {
                        // Surface released
                    }
                }

                provider.unbindAll()
                provider.bindToLifecycle(
                    this,
                    CameraSelector.DEFAULT_BACK_CAMERA,
                    preview
                )

                Log.i(TAG, "Camera bound: ${targetSize.width}x${targetSize.height}")
            } catch (e: Exception) {
                Log.e(TAG, "Camera bind failed", e)
                updateState(State.ERROR, "Camera error: ${e.message}")
            }
        }, ContextCompat.getMainExecutor(this))
    }

    private fun releaseCameraProvider() {
        try {
            cameraProvider?.unbindAll()
        } catch (e: Exception) {
            Log.w(TAG, "Error releasing camera", e)
        }
        cameraProvider = null
    }

    private fun updateState(state: State, message: String) {
        currentState = state
        Log.i(TAG, "State: $state — $message")
        stateListener?.onStateChanged(state, message)
        updateNotification(state, message)
    }

    private fun startForeground() {
        val notification = buildNotification("Initializing...")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun updateNotification(state: State, message: String) {
        val text = when (state) {
            State.IDLE -> "Idle"
            State.WAITING_FOR_HOST -> "Waiting for PC..."
            State.STREAMING -> "Streaming to PC"
            State.ERROR -> "Error: $message"
        }
        val nm = getSystemService(NotificationManager::class.java)
        nm.notify(NOTIFICATION_ID, buildNotification(text))
    }

    private fun buildNotification(text: String): Notification {
        val openIntent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            this, 0, openIntent, PendingIntent.FLAG_IMMUTABLE
        )

        return Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("SmartCAM")
            .setContentText(text)
            .setSmallIcon(android.R.drawable.ic_menu_camera)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()
    }

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Camera Streaming",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Shows when SmartCAM is streaming camera to PC"
        }
        getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
    }
}
