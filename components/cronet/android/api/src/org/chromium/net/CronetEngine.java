// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Constructor;
import java.net.IDN;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.util.Collection;
import java.util.Collections;
import java.util.Date;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executor;
import java.util.regex.Pattern;

/**
 * An engine to process {@link UrlRequest}s, which uses the best HTTP stack
 * available on the current platform.
 */
public abstract class CronetEngine {
    /**
     * A builder for {@link CronetEngine}s, which allows runtime configuration of
     * {@code CronetEngine}. Configuration options are set on the builder and
     * then {@link #build} is called to create the {@code CronetEngine}.
     */
    public static class Builder {
        // A hint that a host supports QUIC.
        static class QuicHint {
            // The host.
            final String mHost;
            // Port of the server that supports QUIC.
            final int mPort;
            // Alternate protocol port.
            final int mAlternatePort;

            QuicHint(String host, int port, int alternatePort) {
                mHost = host;
                mPort = port;
                mAlternatePort = alternatePort;
            }
        }

        // A public key pin.
        static class Pkp {
            // Host to pin for.
            final String mHost;
            // Array of SHA-256 hashes of keys.
            final byte[][] mHashes;
            // Should pin apply to subdomains?
            final boolean mIncludeSubdomains;
            // When the pin expires.
            final Date mExpirationDate;

            Pkp(String host, byte[][] hashes, boolean includeSubdomains, Date expirationDate) {
                mHost = host;
                mHashes = hashes;
                mIncludeSubdomains = includeSubdomains;
                mExpirationDate = expirationDate;
            }
        }

        private static final Pattern INVALID_PKP_HOST_NAME = Pattern.compile("^[0-9\\.]*$");

        // Private fields are simply storage of configuration for the resulting CronetEngine.
        // See setters below for verbose descriptions.
        private final Context mContext;
        private final List<QuicHint> mQuicHints = new LinkedList<QuicHint>();
        private final List<Pkp> mPkps = new LinkedList<Pkp>();
        private String mUserAgent;
        private String mStoragePath;
        private boolean mLegacyModeEnabled;
        private String mLibraryName;
        private boolean mQuicEnabled;
        private boolean mHttp2Enabled;
        private boolean mSdchEnabled;
        private String mDataReductionProxyKey;
        private String mDataReductionProxyPrimaryProxy;
        private String mDataReductionProxyFallbackProxy;
        private String mDataReductionProxySecureProxyCheckUrl;
        private boolean mDisableCache;
        private int mHttpCacheMode;
        private long mHttpCacheMaxSize;
        private String mExperimentalOptions;
        private long mMockCertVerifier;

        /**
         * Default config enables SPDY, disables QUIC, SDCH and HTTP cache.
         * @param context Android {@link Context} for engine to use.
         */
        public Builder(Context context) {
            mContext = context;
            setLibraryName("cronet");
            enableLegacyMode(false);
            enableQUIC(false);
            enableHTTP2(true);
            enableSDCH(false);
            enableHttpCache(HTTP_CACHE_DISABLED, 0);
        }

        /**
         * Constructs a User-Agent string including Cronet version, and
         * application name and version.
         *
         * @return User-Agent string.
         */
        public String getDefaultUserAgent() {
            return UserAgent.from(mContext);
        }

        /**
         * Overrides the User-Agent header for all requests.
         * @return the builder to facilitate chaining.
         */
        public Builder setUserAgent(String userAgent) {
            mUserAgent = userAgent;
            return this;
        }

        String getUserAgent() {
            return mUserAgent;
        }

        /**
         * Sets directory for HTTP Cache and Cookie Storage. The directory must
         * exist.
         * <p>
         * <b>NOTE:</b> Do not use the same storage directory with more than one
         * {@code CronetEngine} at a time. Access to the storage directory does
         * not support concurrent access by multiple {@code CronetEngine}s.
         *
         * @param value path to existing directory.
         * @return the builder to facilitate chaining.
         */
        public Builder setStoragePath(String value) {
            if (!new File(value).isDirectory()) {
                throw new IllegalArgumentException(
                        "Storage path must be set to existing directory");
            }
            mStoragePath = value;
            return this;
        }

        String storagePath() {
            return mStoragePath;
        }

        /**
         * Sets whether falling back to implementation based on system's
         * {@link java.net.HttpURLConnection} implementation is enabled.
         * Defaults to disabled.
         * @return the builder to facilitate chaining.
         * @deprecated Not supported by the new API.
         */
        @Deprecated
        public Builder enableLegacyMode(boolean value) {
            mLegacyModeEnabled = value;
            return this;
        }

        boolean legacyMode() {
            return mLegacyModeEnabled;
        }

        /**
         * Overrides the name of the native library backing Cronet.
         * @return the builder to facilitate chaining.
         */
        Builder setLibraryName(String libName) {
            mLibraryName = libName;
            return this;
        }

        String libraryName() {
            return mLibraryName;
        }

        /**
         * Sets whether <a href="https://www.chromium.org/quic">QUIC</a> protocol
         * is enabled. Defaults to disabled.
         * @return the builder to facilitate chaining.
         */
        public Builder enableQUIC(boolean value) {
            mQuicEnabled = value;
            return this;
        }

        boolean quicEnabled() {
            return mQuicEnabled;
        }

        /**
         * Sets whether <a href="https://tools.ietf.org/html/rfc7540">HTTP/2</a>
         * protocol is enabled. Defaults to enabled.
         * @return the builder to facilitate chaining.
         */
        public Builder enableHTTP2(boolean value) {
            mHttp2Enabled = value;
            return this;
        }

        boolean http2Enabled() {
            return mHttp2Enabled;
        }

        /**
         * Sets whether
         * <a
         * href="https://lists.w3.org/Archives/Public/ietf-http-wg/2008JulSep/att-0441/Shared_Dictionary_Compression_over_HTTP.pdf">
         * SDCH</a> compression is enabled. Defaults to disabled.
         * @return the builder to facilitate chaining.
         */
        public Builder enableSDCH(boolean value) {
            mSdchEnabled = value;
            return this;
        }

        boolean sdchEnabled() {
            return mSdchEnabled;
        }

        /**
         * Enables
         * <a href="https://developer.chrome.com/multidevice/data-compression">Data
         * Reduction Proxy</a>. Defaults to disabled.
         * @param key key to use when authenticating with the proxy.
         * @return the builder to facilitate chaining.
         */
        public Builder enableDataReductionProxy(String key) {
            mDataReductionProxyKey = key;
            return this;
        }

        String dataReductionProxyKey() {
            return mDataReductionProxyKey;
        }

        /**
         * Overrides
         * <a href="https://developer.chrome.com/multidevice/data-compression">
         * Data Reduction Proxy</a> configuration parameters with a primary
         * proxy name, fallback proxy name, and a secure proxy check URL. Proxies
         * are specified as [scheme://]host[:port]. Used for testing.
         * @param primaryProxy the primary data reduction proxy to use.
         * @param fallbackProxy a fallback data reduction proxy to use.
         * @param secureProxyCheckUrl a URL to fetch to determine if using a secure
         * proxy is allowed.
         * @return the builder to facilitate chaining.
         * @hide
         * @deprecated Marked as deprecated because @hide doesn't properly hide but
         *         javadocs are built with nodeprecated="yes".
         */
        @Deprecated
        @SuppressWarnings("DepAnn")
        public Builder setDataReductionProxyOptions(
                String primaryProxy, String fallbackProxy, String secureProxyCheckUrl) {
            if (primaryProxy.isEmpty() || fallbackProxy.isEmpty()
                    || secureProxyCheckUrl.isEmpty()) {
                throw new IllegalArgumentException(
                        "Primary and fallback proxies and check url must be set");
            }
            mDataReductionProxyPrimaryProxy = primaryProxy;
            mDataReductionProxyFallbackProxy = fallbackProxy;
            mDataReductionProxySecureProxyCheckUrl = secureProxyCheckUrl;
            return this;
        }

        String dataReductionProxyPrimaryProxy() {
            return mDataReductionProxyPrimaryProxy;
        }

        String dataReductionProxyFallbackProxy() {
            return mDataReductionProxyFallbackProxy;
        }

        String dataReductionProxySecureProxyCheckUrl() {
            return mDataReductionProxySecureProxyCheckUrl;
        }

        /** @deprecated not really deprecated but hidden. */
        @IntDef({
                HTTP_CACHE_DISABLED, HTTP_CACHE_IN_MEMORY, HTTP_CACHE_DISK_NO_HTTP, HTTP_CACHE_DISK,
        })
        @Retention(RetentionPolicy.SOURCE)
        @SuppressWarnings("DepAnn")
        public @interface HttpCacheSetting {}

        /**
         * Setting to disable HTTP cache. Some data may still be temporarily stored in memory.
         * Passed to {@link #enableHttpCache}.
         */
        public static final int HTTP_CACHE_DISABLED = 0;

        /**
         * Setting to enable in-memory HTTP cache, including HTTP data.
         * Passed to {@link #enableHttpCache}.
         */
        public static final int HTTP_CACHE_IN_MEMORY = 1;

        /**
         * Setting to enable on-disk cache, excluding HTTP data.
         * {@link #setStoragePath} must be called prior to passing this constant to
         * {@link #enableHttpCache}.
         */
        public static final int HTTP_CACHE_DISK_NO_HTTP = 2;

        /**
         * Setting to enable on-disk cache, including HTTP data.
         * {@link #setStoragePath} must be called prior to passing this constant to
         * {@link #enableHttpCache}.
         */
        public static final int HTTP_CACHE_DISK = 3;

        /**
         * Enables or disables caching of HTTP data and other information like QUIC
         * server information.
         * @param cacheMode control location and type of cached data. Must be one of
         *         {@link #HTTP_CACHE_DISABLED HTTP_CACHE_*}.
         * @param maxSize maximum size in bytes used to cache data (advisory and maybe
         * exceeded at times).
         * @return the builder to facilitate chaining.
         */
        public Builder enableHttpCache(@HttpCacheSetting int cacheMode, long maxSize) {
            if (cacheMode == HTTP_CACHE_DISK || cacheMode == HTTP_CACHE_DISK_NO_HTTP) {
                if (storagePath() == null) {
                    throw new IllegalArgumentException("Storage path must be set");
                }
            } else {
                if (storagePath() != null) {
                    throw new IllegalArgumentException("Storage path must not be set");
                }
            }
            mDisableCache =
                    (cacheMode == HTTP_CACHE_DISABLED || cacheMode == HTTP_CACHE_DISK_NO_HTTP);
            mHttpCacheMaxSize = maxSize;

            switch (cacheMode) {
                case HTTP_CACHE_DISABLED:
                    mHttpCacheMode = HttpCacheType.DISABLED;
                    break;
                case HTTP_CACHE_DISK_NO_HTTP:
                case HTTP_CACHE_DISK:
                    mHttpCacheMode = HttpCacheType.DISK;
                    break;
                case HTTP_CACHE_IN_MEMORY:
                    mHttpCacheMode = HttpCacheType.MEMORY;
                    break;
                default:
                    throw new IllegalArgumentException("Unknown cache mode");
            }
            return this;
        }

        boolean cacheDisabled() {
            return mDisableCache;
        }

        long httpCacheMaxSize() {
            return mHttpCacheMaxSize;
        }

        int httpCacheMode() {
            return mHttpCacheMode;
        }

        /**
         * Adds hint that {@code host} supports QUIC.
         * Note that {@link #enableHttpCache enableHttpCache}
         * ({@link #HTTP_CACHE_DISK}) is needed to take advantage of 0-RTT
         * connection establishment between sessions.
         *
         * @param host hostname of the server that supports QUIC.
         * @param port host of the server that supports QUIC.
         * @param alternatePort alternate port to use for QUIC.
         * @return the builder to facilitate chaining.
         */
        public Builder addQuicHint(String host, int port, int alternatePort) {
            if (host.contains("/")) {
                throw new IllegalArgumentException("Illegal QUIC Hint Host: " + host);
            }
            mQuicHints.add(new QuicHint(host, port, alternatePort));
            return this;
        }

        List<QuicHint> quicHints() {
            return mQuicHints;
        }

        /**
         * <p>
         * Pins a set of public keys for a given host. By pinning a set of public keys,
         * {@code pinsSha256}, communication with {@code hostName} is required to
         * authenticate with a certificate with a public key from the set of pinned ones.
         * An app can pin the public key of the root certificate, any of the intermediate
         * certificates or the end-entry certificate. Authentication will fail and secure
         * communication will not be established if none of the public keys is present in the
         * host's certificate chain, even if the host attempts to authenticate with a
         * certificate allowed by the device's trusted store of certificates.
         * </p>
         * <p>
         * Calling this method multiple times with the same host name overrides the previously
         * set pins for the host.
         * </p>
         * <p>
         * More information about the public key pinning can be found in
         * <a href="https://tools.ietf.org/html/rfc7469">RFC 7469</a>.
         * </p>
         *
         * @param hostName name of the host to which the public keys should be pinned. A host that
         *                 consists only of digits and the dot character is treated as invalid.
         * @param pinsSha256 a set of pins. Each pin is the SHA-256 cryptographic
         *                   hash of the DER-encoded ASN.1 representation of the Subject Public
         *                   Key Info (SPKI) of the host's X.509 certificate. Use
         *                   {@link java.security.cert.Certificate#getPublicKey()
         *                   Certificate.getPublicKey()} and
         *                   {@link java.security.Key#getEncoded() Key.getEncoded()}
         *                   to obtain DER-encoded ASN.1 representation of the SPKI.
         *                   Although, the method does not mandate the presence of the backup pin
         *                   that can be used if the control of the primary private key has been
         *                   lost, it is highly recommended to supply one.
         * @param includeSubdomains indicates whether the pinning policy should be applied to
         *                          subdomains of {@code hostName}.
         * @param expirationDate specifies the expiration date for the pins.
         * @return the builder to facilitate chaining.
         * @throws NullPointerException if any of the input parameters are {@code null}.
         * @throws IllegalArgumentException if the given host name is invalid or {@code pinsSha256}
         *                                  contains a byte array that does not represent a valid
         *                                  SHA-256 hash.
         */
        public Builder addPublicKeyPins(String hostName, Set<byte[]> pinsSha256,
                boolean includeSubdomains, Date expirationDate) {
            if (hostName == null) {
                throw new NullPointerException("The hostname cannot be null");
            }
            if (pinsSha256 == null) {
                throw new NullPointerException("The set of SHA256 pins cannot be null");
            }
            if (expirationDate == null) {
                throw new NullPointerException("The pin expiration date cannot be null");
            }
            String idnHostName = validateHostNameForPinningAndConvert(hostName);
            // Convert the pin to BASE64 encoding. The hash set will eliminate duplications.
            Set<byte[]> hashes = new HashSet<>(pinsSha256.size());
            for (byte[] pinSha256 : pinsSha256) {
                if (pinSha256 == null || pinSha256.length != 32) {
                    throw new IllegalArgumentException("Public key pin is invalid");
                }
                hashes.add(pinSha256);
            }
            // Add new element to PKP list.
            mPkps.add(new Pkp(idnHostName, hashes.toArray(new byte[hashes.size()][]),
                    includeSubdomains, expirationDate));
            return this;
        }

        /**
         * Returns list of public key pins.
         */
        List<Pkp> publicKeyPins() {
            return mPkps;
        }

        /**
         * Checks whether a given string represents a valid host name for PKP and converts it
         * to ASCII Compatible Encoding representation according to RFC 1122, RFC 1123 and
         * RFC 3490. This method is more restrictive than required by RFC 7469. Thus, a host
         * that contains digits and the dot character only is considered invalid.
         *
         * Note: Currently Cronet doesn't have native implementation of host name validation that
         *       can be used. There is code that parses a provided URL but doesn't ensure its
         *       correctness. The implementation relies on {@code getaddrinfo} function.
         *
         * @param hostName host name to check and convert.
         * @return true if the string is a valid host name.
         * @throws IllegalArgumentException if the the given string does not represent a valid
         *                                  hostname.
         */
        private static String validateHostNameForPinningAndConvert(String hostName)
                throws IllegalArgumentException {
            if (INVALID_PKP_HOST_NAME.matcher(hostName).matches()) {
                throw new IllegalArgumentException("Hostname " + hostName + " is illegal."
                        + " A hostname should not consist of digits and/or dots only.");
            }
            try {
                return IDN.toASCII(hostName, IDN.USE_STD3_ASCII_RULES);
            } catch (IllegalArgumentException ex) {
                throw new IllegalArgumentException("Hostname " + hostName + " is illegal."
                        + " The name of the host does not comply with RFC 1122 and RFC 1123.");
            }
        }

        /**
         * Sets experimental options to be used in Cronet.
         *
         * @param options JSON formatted experimental options.
         * @return the builder to facilitate chaining.
         */
        public Builder setExperimentalOptions(String options) {
            mExperimentalOptions = options;
            return this;
        }

        String experimentalOptions() {
            return mExperimentalOptions;
        }

        /**
         * Sets a native MockCertVerifier for testing.
         */
        Builder setMockCertVerifierForTesting(long mockCertVerifier) {
            mMockCertVerifier = mockCertVerifier;
            return this;
        }

        long mockCertVerifier() {
            return mMockCertVerifier;
        }

        /**
         * Returns {@link Context} for builder.
         *
         * @return {@link Context} for builder.
         */
        Context getContext() {
            return mContext;
        }

        /**
         * Build a {@link CronetEngine} using this builder's configuration.
         */
        public CronetEngine build() {
            CronetEngine engine = createContext(this);
            // Clear MOCK_CERT_VERIFIER reference if there is any, since
            // the ownership has been transferred to the engine.
            mMockCertVerifier = 0;
            return engine;
        }
    }

    private static final String TAG = "UrlRequestFactory";
    private static final String CRONET_URL_REQUEST_CONTEXT =
            "org.chromium.net.CronetUrlRequestContext";

    /**
     * Creates a {@link UrlRequest} object. All callbacks will
     * be called on {@code executor}'s thread. {@code executor} must not run
     * tasks on the current thread to prevent blocking networking operations
     * and causing exceptions during shutdown. Request is given medium priority,
     * see {@link UrlRequest.Builder#REQUEST_PRIORITY_MEDIUM}. To specify other
     * priorities see {@link #createRequest(String, UrlRequest.Callback,
     * Executor, int priority)}.
     *
     * @param url {@link URL} for the request.
     * @param callback callback object that gets invoked on different events.
     * @param executor {@link Executor} on which all callbacks will be invoked.
     * @return new request.
     * @deprecated Use {@link UrlRequest.Builder#build}.
     */
    @Deprecated
    public final UrlRequest createRequest(
            String url, UrlRequest.Callback callback, Executor executor) {
        return createRequest(url, callback, executor, UrlRequest.Builder.REQUEST_PRIORITY_MEDIUM);
    }

    /**
     * Creates a {@link UrlRequest} object. All callbacks will
     * be called on {@code executor}'s thread. {@code executor} must not run
     * tasks on the current thread to prevent blocking networking operations
     * and causing exceptions during shutdown.
     *
     * @param url {@link URL} for the request.
     * @param callback callback object that gets invoked on different events.
     * @param executor {@link Executor} on which all callbacks will be invoked.
     * @param priority priority of the request which should be one of the
     *         {@link UrlRequest.Builder#REQUEST_PRIORITY_IDLE REQUEST_PRIORITY_*}
     *         values.
     * @return new request.
     * @deprecated Use {@link UrlRequest.Builder#build}.
     */
    @Deprecated
    public final UrlRequest createRequest(String url, UrlRequest.Callback callback,
            Executor executor, @UrlRequest.Builder.RequestPriority int priority) {
        return createRequest(url, callback, executor, priority, Collections.emptyList());
    }

    /**
     * Creates a {@link UrlRequest} object. All callbacks will
     * be called on {@code executor}'s thread. {@code executor} must not run
     * tasks on the current thread to prevent blocking networking operations
     * and causing exceptions during shutdown.
     *
     * @param url {@link URL} for the request.
     * @param callback callback object that gets invoked on different events.
     * @param executor {@link Executor} on which all callbacks will be invoked.
     * @param priority priority of the request which should be one of the
     *         {@link UrlRequest.Builder#REQUEST_PRIORITY_IDLE REQUEST_PRIORITY_*}
     *         values.
     * @param requestAnnotations Objects to pass on to {@link CronetEngine.RequestFinishedListener}.
     * @return new request.
     */
    protected abstract UrlRequest createRequest(String url, UrlRequest.Callback callback,
            Executor executor, int priority, Collection<Object> requestAnnotations);

    /**
     * Creates a {@link BidirectionalStream} object. {@code callback} methods will
     * be invoked on {@code executor}. {@code executor} must not run
     * tasks on the current thread to prevent blocking networking operations
     * and causing exceptions during shutdown.
     *
     * @param url the URL for the stream
     * @param callback the object whose methods get invoked upon different events
     * @param executor the {@link Executor} on which all callbacks will be called
     * @param httpMethod the HTTP method to use for the stream
     * @param requestHeaders the list of request headers
     * @param priority priority of the stream which should be one of the
     *         {@link BidirectionalStream.Builder#STREAM_PRIORITY_IDLE STREAM_PRIORITY_*}
     *         values.
     * @return a new stream.
     */
    abstract BidirectionalStream createBidirectionalStream(String url,
            BidirectionalStream.Callback callback, Executor executor, String httpMethod,
            List<Map.Entry<String, String>> requestHeaders,
            @BidirectionalStream.Builder.StreamPriority int priority);

    /**
     * @return {@code true} if the engine is enabled.
     */
    abstract boolean isEnabled();

    /**
     * @return a human-readable version string of the engine.
     */
    public abstract String getVersionString();

    /**
     * Shuts down the {@link CronetEngine} if there are no active requests,
     * otherwise throws an exception.
     *
     * Cannot be called on network thread - the thread Cronet calls into
     * Executor on (which is different from the thread the Executor invokes
     * callbacks on). May block until all the {@code CronetEngine}'s
     * resources have been cleaned up.
     */
    public abstract void shutdown();

    /**
     * Starts NetLog logging to a file. The NetLog is useful for debugging.
     * The file can be viewed using a Chrome browser navigated to
     * chrome://net-internals/#import
     * @param fileName the complete file path. It must not be empty. If the file
     *            exists, it is truncated before starting. If actively logging,
     *            this method is ignored.
     * @param logAll {@code true} to include basic events, user cookies,
     *            credentials and all transferred bytes in the log.
     *            {@code false} to just include basic events.
     */
    public abstract void startNetLogToFile(String fileName, boolean logAll);

    /**
     * Stops NetLog logging and flushes file to disk. If a logging session is
     * not in progress, this call is ignored.
     */
    public abstract void stopNetLog();

    /**
     * Returns differences in metrics collected by Cronet since the last call to
     * {@link #getGlobalMetricsDeltas}.
     * <p>
     * Cronet collects these metrics globally. This means deltas returned by
     * {@code getGlobalMetricsDeltas()} will include measurements of requests
     * processed by other {@link CronetEngine} instances. Since this function
     * returns differences in metrics collected since the last call, and these
     * metrics are collected globally, a call to any {@code CronetEngine}
     * instance's {@code getGlobalMetricsDeltas()} method will affect the deltas
     * returned by any other {@code CronetEngine} instance's
     * {@code getGlobalMetricsDeltas()}.
     * <p>
     * Cronet starts collecting these metrics after the first call to
     * {@code getGlobalMetricsDeltras()}, so the first call returns no
     * useful data as no metrics have yet been collected.
     *
     * @return differences in metrics collected by Cronet, since the last call
     *         to {@code getGlobalMetricsDeltas()}, serialized as a
     *         <a href=https://developers.google.com/protocol-buffers>protobuf
     *         </a>.
     */
    public abstract byte[] getGlobalMetricsDeltas();

    /**
     * Enables the network quality estimator, which collects and reports
     * measurements of round trip time (RTT) and downstream throughput at
     * various layers of the network stack. After enabling the estimator,
     * listeners of RTT and throughput can be added with
     * {@link #addRttListener} and {@link #addThroughputListener} and
     * removed with {@link #removeRttListener} and
     * {@link #removeThroughputListener}. The estimator uses memory and CPU
     * only when enabled.
     * @param executor an executor that will be used to notified all
     *            added RTT and throughput listeners.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated public abstract void enableNetworkQualityEstimator(Executor executor);

    /**
     * Enables the network quality estimator for testing. This must be called
     * before round trip time and throughput listeners are added. Set both
     * boolean parameters to false for default behavior.
     * @param useLocalHostRequests include requests to localhost in estimates.
     * @param useSmallerResponses include small responses in throughput estimates.
     * @param executor an {@link java.util.concurrent.Executor} on which all
     *            listeners will be called.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    abstract void enableNetworkQualityEstimatorForTesting(
            boolean useLocalHostRequests, boolean useSmallerResponses, Executor executor);

    /**
     * Registers a listener that gets called whenever the network quality
     * estimator witnesses a sample round trip time. This must be called
     * after {@link #enableNetworkQualityEstimator}, and with throw an
     * exception otherwise. Round trip times may be recorded at various layers
     * of the network stack, including TCP, QUIC, and at the URL request layer.
     * The listener is called on the {@link java.util.concurrent.Executor} that
     * is passed to {@link #enableNetworkQualityEstimator}.
     * @param listener the listener of round trip times.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated public abstract void addRttListener(NetworkQualityRttListener listener);

    /**
     * Removes a listener of round trip times if previously registered with
     * {@link #addRttListener}. This should be called after a
     * {@link NetworkQualityRttListener} is added in order to stop receiving
     * observations.
     * @param listener the listener of round trip times.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated public abstract void removeRttListener(NetworkQualityRttListener listener);

    /**
     * Registers a listener that gets called whenever the network quality
     * estimator witnesses a sample throughput measurement. This must be called
     * after {@link #enableNetworkQualityEstimator}. Throughput observations
     * are computed by measuring bytes read over the active network interface
     * at times when at least one URL response is being received. The listener
     * is called on the {@link java.util.concurrent.Executor} that is passed to
     * {@link #enableNetworkQualityEstimator}.
     * @param listener the listener of throughput.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    public abstract void addThroughputListener(NetworkQualityThroughputListener listener);

    /**
     * Removes a listener of throughput. This should be called after a
     * {@link NetworkQualityThroughputListener} is added with
     * {@link #addThroughputListener} in order to stop receiving observations.
     * @param listener the listener of throughput.
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    public abstract void removeThroughputListener(NetworkQualityThroughputListener listener);

    /**
     * Establishes a new connection to the resource specified by the {@link URL} {@code url}.
     * <p>
     * <b>Note:</b> Cronet's {@link java.net.HttpURLConnection} implementation is subject to certain
     * limitations, see {@link #createURLStreamHandlerFactory} for details.
     *
     * @param url URL of resource to connect to.
     * @return an {@link java.net.HttpURLConnection} instance implemented by this CronetEngine.
     */
    public abstract URLConnection openConnection(URL url) throws IOException;

    /**
     * Establishes a new connection to the resource specified by the {@link URL} {@code url}
     * using the given proxy.
     * <p>
     * <b>Note:</b> Cronet's {@link java.net.HttpURLConnection} implementation is subject to certain
     * limitations, see {@link #createURLStreamHandlerFactory} for details.
     *
     * @param url URL of resource to connect to.
     * @param proxy proxy to use when establishing connection.
     * @return an {@link java.net.HttpURLConnection} instance implemented by this CronetEngine.
     * @hide
     * @deprecated Marked as deprecated because @hide doesn't properly hide but
     *         javadocs are built with nodeprecated="yes".
     *         TODO(pauljensen): Expose once implemented, http://crbug.com/418111
     */
    @Deprecated
    @SuppressWarnings("DepAnn")
    public abstract URLConnection openConnection(URL url, Proxy proxy) throws IOException;

    /**
     * Creates a {@link URLStreamHandlerFactory} to handle HTTP and HTTPS
     * traffic. An instance of this class can be installed via
     * {@link URL#setURLStreamHandlerFactory} thus using this CronetEngine by default for
     * all requests created via {@link URL#openConnection}.
     * <p>
     * Cronet does not use certain HTTP features provided via the system:
     * <ul>
     * <li>the HTTP cache installed via
     *     {@link android.net.http.HttpResponseCache#install(java.io.File, long)
     *            HttpResponseCache.install()}</li>
     * <li>the HTTP authentication method installed via
     *     {@link java.net.Authenticator#setDefault}</li>
     * <li>the HTTP cookie storage installed via {@link java.net.CookieHandler#setDefault}</li>
     * </ul>
     * <p>
     * While Cronet supports and encourages requests using the HTTPS protocol,
     * Cronet does not provide support for the
     * {@link javax.net.ssl.HttpsURLConnection} API. This lack of support also
     * includes not using certain HTTPS features provided via the system:
     * <ul>
     * <li>the HTTPS hostname verifier installed via {@link
     *   javax.net.ssl.HttpsURLConnection#setDefaultHostnameVerifier(javax.net.ssl.HostnameVerifier)
     *     HttpsURLConnection.setDefaultHostnameVerifier()}</li>
     * <li>the HTTPS socket factory installed via {@link
     *   javax.net.ssl.HttpsURLConnection#setDefaultSSLSocketFactory(javax.net.ssl.SSLSocketFactory)
     *     HttpsURLConnection.setDefaultSSLSocketFactory()}</li>
     * </ul>
     *
     * @return an {@link URLStreamHandlerFactory} instance implemented by this
     *         CronetEngine.
     */
    public abstract URLStreamHandlerFactory createURLStreamHandlerFactory();

    /**
     * Creates a {@link CronetEngine} with the given {@link Builder}.
     *
     * @param builder builder to used for creating the CronetEngine instance.
     * @return the created CronetEngine instance.
     * @deprecated Use {@link CronetEngine.Builder}.
     */
    @Deprecated
    public static CronetEngine createContext(Builder builder) {
        CronetEngine cronetEngine = null;
        if (builder.getUserAgent() == null) {
            builder.setUserAgent(builder.getDefaultUserAgent());
        }
        if (!builder.legacyMode()) {
            cronetEngine = createCronetEngine(builder);
        }
        if (cronetEngine == null) {
            cronetEngine = new JavaCronetEngine(builder.getUserAgent());
        }
        Log.i(TAG, "Using network stack: " + cronetEngine.getVersionString());
        return cronetEngine;
    }

    private static CronetEngine createCronetEngine(Builder builder) {
        CronetEngine cronetEngine = null;
        try {
            Class<? extends CronetEngine> engineClass =
                    CronetEngine.class.getClassLoader()
                            .loadClass(CRONET_URL_REQUEST_CONTEXT)
                            .asSubclass(CronetEngine.class);
            Constructor<? extends CronetEngine> constructor =
                    engineClass.getConstructor(Builder.class);
            CronetEngine possibleEngine = constructor.newInstance(builder);
            if (possibleEngine.isEnabled()) {
                cronetEngine = possibleEngine;
            }
        } catch (ClassNotFoundException e) {
            // Leave as null.
        } catch (Exception e) {
            throw new IllegalStateException("Cannot instantiate: " + CRONET_URL_REQUEST_CONTEXT, e);
        }
        return cronetEngine;
    }

    /**
     * Registers a listener that gets called after the end of each request with the request info.
     *
     * <p>This must be called after {@link #enableNetworkQualityEstimator} and will throw an
     * exception otherwise.
     *
     * <p>The listener is called on the {@link java.util.concurrent.Executor} that
     * is passed to {@link #enableNetworkQualityEstimator}.
     *
     * @param listener the listener for finished requests.
     *
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated public abstract void addRequestFinishedListener(RequestFinishedListener listener);

    /**
     * Removes a finished request listener.
     *
     * @param listener the listener to remove.
     *
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    public abstract void removeRequestFinishedListener(RequestFinishedListener listener);

    /**
     * Information about a finished request. Passed to {@link RequestFinishedListener}.
     *
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    public static final class UrlRequestInfo {
        private final String mUrl;
        private final Collection<Object> mAnnotations;
        private final UrlRequestMetrics mMetrics;
        @Nullable private final UrlResponseInfo mResponseInfo;

        UrlRequestInfo(String url, Collection<Object> annotations, UrlRequestMetrics metrics,
                @Nullable UrlResponseInfo responseInfo) {
            mUrl = url;
            mAnnotations = annotations;
            mMetrics = metrics;
            mResponseInfo = responseInfo;
        }

        /** Returns the request's original URL. */
        public String getUrl() {
            return mUrl;
        }

        /** Returns the objects that the caller has supplied when initiating the request. */
        public Collection<Object> getAnnotations() {
            return mAnnotations;
        }

        // TODO(klm): Collect and return a chain of Metrics objects for redirect responses.
        /**
         * Returns metrics collected for this request.
         *
         * <p>The reported times and bytes account for all redirects, i.e.
         * the TTFB is from the start of the original request to the ultimate response headers,
         * the TTLB is from the start of the original request to the end of the ultimate response,
         * the received byte count is for all redirects and the ultimate response combined.
         * These cumulative metric definitions are debatable, but are chosen to make sense
         * for user-facing latency analysis.
         *
         * <p>Must call {@link #enableNetworkQualityEstimator} to enable request metrics collection.
         */
        public UrlRequestMetrics getMetrics() {
            return mMetrics;
        }

        /** Returns a UrlResponseInfo for the request, if its response had started. */
        @Nullable
        public UrlResponseInfo getResponseInfo() {
            return mResponseInfo;
        }
    }

    /**
     * Metrics collected for a single request.
     *
     * <p>Must call {@link #enableNetworkQualityEstimator} to enable request metrics collection.
     *
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    public static final class UrlRequestMetrics {
        @Nullable private final Long mTtfbMs;
        @Nullable private final Long mTotalTimeMs;
        @Nullable private final Long mSentBytesCount;
        @Nullable private final Long mReceivedBytesCount;

        public UrlRequestMetrics(@Nullable Long ttfbMs, @Nullable Long totalTimeMs,
                @Nullable Long sentBytesCount, @Nullable Long receivedBytesCount) {
            mTtfbMs = ttfbMs;
            mTotalTimeMs = totalTimeMs;
            mSentBytesCount = sentBytesCount;
            mReceivedBytesCount = receivedBytesCount;
        }

        /**
         * Returns milliseconds between request initiation and first byte of response headers,
         * or null if not collected.
         */
        @Nullable
        public Long getTtfbMs() {
            return mTtfbMs;
        }

        /**
         * Returns milliseconds between request initiation and finish,
         * including a failure or cancellation, or null if not collected.
         */
        @Nullable
        public Long getTotalTimeMs() {
            return mTotalTimeMs;
        }

        /**
         * Returns total bytes sent over the network transport layer, or null if not collected.
         */
        @Nullable
        public Long getSentBytesCount() {
            return mSentBytesCount;
        }

        /**
         * Returns total bytes received over the network transport layer, or null if not collected.
         */
        @Nullable
        public Long getReceivedBytesCount() {
            return mReceivedBytesCount;
        }
    }

    /**
     * Interface to listen for finished requests that were created via this CronetEngine instance.
     *
     * @deprecated not really deprecated but hidden for now as it's a prototype.
     */
    @Deprecated
    public interface RequestFinishedListener { // TODO(klm): Add a convenience abstract class.
        /** Invoked with request info. */
        void onRequestFinished(UrlRequestInfo requestInfo);
    }
}
