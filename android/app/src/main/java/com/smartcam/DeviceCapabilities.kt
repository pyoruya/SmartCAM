package com.smartcam

import android.media.MediaCodecInfo
import android.media.MediaCodecList
import android.media.MediaFormat
import android.util.Size

/**
 * Queries device MediaCodec capabilities to determine the best
 * supported H.264 encoding resolution. Implements the fallback chain:
 * 4K30 -> 1440p30 -> 1080p30 -> 720p30
 */
object DeviceCapabilities {

    data class StreamConfig(
        val width: Int,
        val height: Int,
        val fps: Int,
        val bitrate: Int // bps
    )

    private val FALLBACK_CHAIN = listOf(
        StreamConfig(3840, 2160, 30, 25_000_000),
        StreamConfig(2560, 1440, 30, 15_000_000),
        StreamConfig(1920, 1080, 30, 8_000_000),
        StreamConfig(1280, 720, 30, 4_000_000)
    )

    /**
     * Returns the best supported H.264 encoding config for this device,
     * walking down the fallback chain until a supported resolution is found.
     */
    fun getBestConfig(): StreamConfig {
        val codecInfo = findH264Encoder() ?: return FALLBACK_CHAIN.last()
        val caps = codecInfo.getCapabilitiesForType(MediaFormat.MIMETYPE_VIDEO_AVC)
        val videoCaps = caps.videoCapabilities

        for (config in FALLBACK_CHAIN) {
            if (videoCaps.isSizeSupported(config.width, config.height) &&
                videoCaps.areSizeAndRateSupported(config.width, config.height, config.fps.toDouble())
            ) {
                return config
            }
        }
        return FALLBACK_CHAIN.last()
    }

    /**
     * Returns all supported configs from the fallback chain for capability reporting.
     */
    fun getSupportedConfigs(): List<StreamConfig> {
        val codecInfo = findH264Encoder() ?: return listOf(FALLBACK_CHAIN.last())
        val caps = codecInfo.getCapabilitiesForType(MediaFormat.MIMETYPE_VIDEO_AVC)
        val videoCaps = caps.videoCapabilities

        return FALLBACK_CHAIN.filter { config ->
            videoCaps.isSizeSupported(config.width, config.height) &&
                videoCaps.areSizeAndRateSupported(config.width, config.height, config.fps.toDouble())
        }.ifEmpty { listOf(FALLBACK_CHAIN.last()) }
    }

    private fun findH264Encoder(): MediaCodecInfo? {
        val codecList = MediaCodecList(MediaCodecList.REGULAR_CODECS)
        return codecList.codecInfos.firstOrNull { info ->
            info.isEncoder && info.supportedTypes.any {
                it.equals(MediaFormat.MIMETYPE_VIDEO_AVC, ignoreCase = true)
            } && info.isHardwareAccelerated
        } ?: codecList.codecInfos.firstOrNull { info ->
            // Fallback to software encoder if no HW encoder found
            info.isEncoder && info.supportedTypes.any {
                it.equals(MediaFormat.MIMETYPE_VIDEO_AVC, ignoreCase = true)
            }
        }
    }
}
