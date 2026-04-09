package com.smartcam

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.Surface

/**
 * Wraps MediaCodec for H.264 hardware encoding.
 *
 * Uses Surface input (zero-copy from CameraX) and delivers encoded
 * NAL units via callback for streaming.
 */
class H264Encoder {

    companion object {
        private const val TAG = "H264Encoder"
        private const val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC
        private const val I_FRAME_INTERVAL = 1 // seconds
    }

    interface Callback {
        fun onEncodedData(data: ByteArray, offset: Int, size: Int, isKeyFrame: Boolean)
        fun onError(e: Exception)
    }

    private var codec: MediaCodec? = null
    private var inputSurface: Surface? = null
    private var callbackHandler: HandlerThread? = null
    private var callback: Callback? = null

    /**
     * Configure and start the encoder.
     * @return Surface that CameraX should render to.
     */
    fun start(
        width: Int,
        height: Int,
        fps: Int,
        bitrate: Int,
        callback: Callback
    ): Surface {
        this.callback = callback

        val format = MediaFormat.createVideoFormat(MIME_TYPE, width, height).apply {
            setInteger(MediaFormat.KEY_COLOR_FORMAT,
                MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
            setInteger(MediaFormat.KEY_BIT_RATE, bitrate)
            setInteger(MediaFormat.KEY_FRAME_RATE, fps)
            setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, I_FRAME_INTERVAL)
            setInteger(MediaFormat.KEY_PROFILE,
                MediaCodecInfo.CodecProfileLevel.AVCProfileBaseline)
            setInteger(MediaFormat.KEY_LEVEL,
                MediaCodecInfo.CodecProfileLevel.AVCLevel31)
        }

        val handlerThread = HandlerThread("H264Encoder").also {
            it.start()
            callbackHandler = it
        }
        val handler = Handler(handlerThread.looper)

        val encoder = MediaCodec.createEncoderByType(MIME_TYPE)
        encoder.setCallback(object : MediaCodec.Callback() {
            override fun onInputBufferAvailable(codec: MediaCodec, index: Int) {
                // Not used with Surface input
            }

            override fun onOutputBufferAvailable(
                codec: MediaCodec,
                index: Int,
                info: MediaCodec.BufferInfo
            ) {
                try {
                    if (info.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG != 0) {
                        // SPS/PPS — send immediately
                        val buffer = codec.getOutputBuffer(index) ?: return
                        val data = ByteArray(info.size)
                        buffer.get(data)
                        extractAndSendNalUnits(data, isKeyFrame = false)
                        codec.releaseOutputBuffer(index, false)
                        return
                    }

                    if (info.size > 0) {
                        val buffer = codec.getOutputBuffer(index) ?: return
                        val data = ByteArray(info.size)
                        buffer.get(data)
                        val isKey = info.flags and MediaCodec.BUFFER_FLAG_KEY_FRAME != 0
                        extractAndSendNalUnits(data, isKey)
                    }
                    codec.releaseOutputBuffer(index, false)
                } catch (e: Exception) {
                    Log.e(TAG, "Output buffer error", e)
                    callback.onError(e)
                }
            }

            override fun onError(codec: MediaCodec, e: MediaCodec.CodecException) {
                Log.e(TAG, "Codec error", e)
                callback.onError(e)
            }

            override fun onOutputFormatChanged(codec: MediaCodec, format: MediaFormat) {
                Log.i(TAG, "Output format changed: $format")
            }
        }, handler)

        encoder.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
        val surface = encoder.createInputSurface()
        inputSurface = surface
        codec = encoder
        encoder.start()

        Log.i(TAG, "Encoder started: ${width}x${height} @ ${fps}fps, ${bitrate / 1000}kbps")
        return surface
    }

    fun stop() {
        try {
            codec?.signalEndOfInputStream()
            codec?.stop()
            codec?.release()
        } catch (e: Exception) {
            Log.w(TAG, "Error stopping encoder", e)
        }
        codec = null
        inputSurface?.release()
        inputSurface = null
        callbackHandler?.quitSafely()
        callbackHandler = null
        Log.i(TAG, "Encoder stopped")
    }

    /**
     * Request an immediate I-frame (e.g., on new client connection).
     */
    fun requestKeyFrame() {
        try {
            val params = android.os.Bundle()
            params.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0)
            codec?.setParameters(params)
        } catch (e: Exception) {
            Log.w(TAG, "Failed to request key frame", e)
        }
    }

    /**
     * Extract individual NAL units from Annex-B byte stream and send them.
     * MediaCodec output may contain multiple NAL units in one buffer
     * (e.g., SPS + PPS, or AUD + slice).
     */
    private fun extractAndSendNalUnits(data: ByteArray, isKeyFrame: Boolean) {
        val cb = callback ?: return
        var i = 0
        val nalStarts = mutableListOf<Int>()

        // Find all NAL unit start codes (0x00000001 or 0x000001)
        while (i < data.size - 3) {
            if (data[i] == 0.toByte() && data[i + 1] == 0.toByte()) {
                if (data[i + 2] == 1.toByte()) {
                    nalStarts.add(i + 3)
                    i += 3
                    continue
                } else if (i < data.size - 4 && data[i + 2] == 0.toByte() && data[i + 3] == 1.toByte()) {
                    nalStarts.add(i + 4)
                    i += 4
                    continue
                }
            }
            i++
        }

        if (nalStarts.isEmpty()) {
            // No start codes found — send as single NAL unit
            cb.onEncodedData(data, 0, data.size, isKeyFrame)
            return
        }

        // Send each NAL unit individually
        for (j in nalStarts.indices) {
            val start = nalStarts[j]
            val end = if (j + 1 < nalStarts.size) {
                // Find the start code before the next NAL
                var k = nalStarts[j + 1] - 3
                if (k >= 1 && data[k - 1] == 0.toByte()) k-- // 4-byte start code
                k
            } else {
                data.size
            }
            val size = end - start
            if (size > 0) {
                cb.onEncodedData(data, start, size, isKeyFrame)
            }
        }
    }
}
