package com.smartcam

import android.app.Service
import android.content.Intent
import android.os.IBinder

/**
 * Foreground service that keeps camera streaming alive
 * even when the activity is in the background.
 */
class CameraStreamService : Service() {

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // TODO: Start foreground notification + camera capture + encoding + TCP server
        return START_STICKY
    }

    override fun onDestroy() {
        super.onDestroy()
        // TODO: Release camera, encoder, server socket
    }
}
