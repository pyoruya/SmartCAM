package com.smartcam

import android.util.Log
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.IOException
import java.net.ServerSocket
import java.net.Socket
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean

/**
 * TCP server implementing the SmartCAM wire protocol v1.
 *
 * Listens on localhost for the PC host connection (via ADB reverse),
 * performs handshake, then streams H.264 NAL units.
 */
class StreamServer(
    private val port: Int = DEFAULT_PORT
) {
    companion object {
        private const val TAG = "StreamServer"
        const val DEFAULT_PORT = 38247

        // Protocol magic values
        private const val MAGIC_HELLO = 0x5343414D // "SCAM"
        private const val MAGIC_CAPS  = 0x53434150 // "SCAP"
        private const val MAGIC_SEL   = 0x5353454C // "SSEL"
        private const val MAGIC_START = 0x53535452 // "SSTR"

        private const val PROTOCOL_VERSION: Short = 1
    }

    interface Listener {
        fun onClientConnected()
        fun onFormatSelected(width: Int, height: Int, fps: Int, bitrate: Int)
        fun onClientDisconnected()
        fun onError(e: Exception)
    }

    private var serverSocket: ServerSocket? = null
    private var clientSocket: Socket? = null
    private var outputStream: DataOutputStream? = null
    private val running = AtomicBoolean(false)
    private val streaming = AtomicBoolean(false)
    private var listener: Listener? = null
    private var acceptThread: Thread? = null

    fun setListener(listener: Listener) {
        this.listener = listener
    }

    fun start() {
        if (running.getAndSet(true)) return

        acceptThread = Thread({
            try {
                serverSocket = ServerSocket(port).apply {
                    reuseAddress = true
                }
                Log.i(TAG, "Listening on port $port")

                while (running.get()) {
                    val socket = serverSocket?.accept() ?: break
                    handleClient(socket)
                }
            } catch (e: IOException) {
                if (running.get()) {
                    Log.e(TAG, "Server error", e)
                    listener?.onError(e)
                }
            }
        }, "StreamServer-Accept").also { it.start() }
    }

    fun stop() {
        running.set(false)
        streaming.set(false)
        try { clientSocket?.close() } catch (_: IOException) {}
        try { serverSocket?.close() } catch (_: IOException) {}
        acceptThread?.join(2000)
        acceptThread = null
        Log.i(TAG, "Server stopped")
    }

    /**
     * Send a single H.264 NAL unit to the connected host.
     * Called from the encoder output callback. Thread-safe.
     */
    fun sendNalUnit(data: ByteArray, offset: Int, size: Int) {
        if (!streaming.get()) return
        val out = outputStream ?: return
        try {
            synchronized(out) {
                out.writeInt(size)
                out.write(data, offset, size)
                out.flush()
            }
        } catch (e: IOException) {
            Log.w(TAG, "Send failed, disconnecting", e)
            streaming.set(false)
            listener?.onClientDisconnected()
        }
    }

    fun isStreaming(): Boolean = streaming.get()

    private fun handleClient(socket: Socket) {
        clientSocket = socket
        Log.i(TAG, "Client connected: ${socket.remoteSocketAddress}")

        try {
            socket.tcpNoDelay = true
            val input = DataInputStream(socket.getInputStream())
            val output = DataOutputStream(socket.getOutputStream())
            outputStream = output

            // 1. Read Hello from host
            val magic = input.readInt()
            if (magic != MAGIC_HELLO) {
                Log.w(TAG, "Invalid magic: 0x${Integer.toHexString(magic)}")
                socket.close()
                return
            }
            val version = input.readShort()
            input.readShort() // reserved
            Log.i(TAG, "Hello received, protocol v$version")

            // 2. Send capabilities
            val configs = DeviceCapabilities.getSupportedConfigs()
            output.writeInt(MAGIC_CAPS)
            output.writeByte(configs.size)
            for (config in configs) {
                output.writeShort(config.width)
                output.writeShort(config.height)
                output.writeShort(config.fps)
                output.writeShort(0) // reserved
            }
            output.flush()
            Log.i(TAG, "Capabilities sent: ${configs.size} resolutions")

            // 3. Read format selection from host
            val selMagic = input.readInt()
            if (selMagic != MAGIC_SEL) {
                Log.w(TAG, "Expected SSEL, got 0x${Integer.toHexString(selMagic)}")
                socket.close()
                return
            }
            val width = input.readUnsignedShort()
            val height = input.readUnsignedShort()
            val fps = input.readUnsignedShort()
            val bitrate = input.readInt()
            Log.i(TAG, "Format selected: ${width}x${height} @ ${fps}fps, ${bitrate}kbps")

            listener?.onClientConnected()
            listener?.onFormatSelected(width, height, fps, bitrate * 1000)

            // 4. Send stream start (actual params echoed back after encoder init)
            output.writeInt(MAGIC_START)
            output.writeShort(width)
            output.writeShort(height)
            output.writeShort(fps)
            output.flush()

            streaming.set(true)
            Log.i(TAG, "Streaming started")

            // Keep connection alive — read loop watches for disconnect
            while (streaming.get() && !socket.isClosed) {
                Thread.sleep(500)
            }

        } catch (e: IOException) {
            Log.w(TAG, "Client disconnected", e)
        } finally {
            streaming.set(false)
            outputStream = null
            clientSocket = null
            listener?.onClientDisconnected()
            try { socket.close() } catch (_: IOException) {}
        }
    }
}
