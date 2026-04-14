/**
 * Copyright (c) 2017 Parrot Drones SAS
 * Copyright (c) 2017 Aurelien Barre
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>

#include "common.h"


/* To be used for all public API */
#ifdef RTSP_API_EXPORTS
#	ifdef _WIN32
#		define RTSP_API __declspec(dllexport)
#	else /* !_WIN32 */
#		define RTSP_API __attribute__((visibility("default")))
#	endif /* !_WIN32 */
#else /* !RTSP_API_EXPORTS */
#	define RTSP_API
#endif /* !RTSP_API_EXPORTS */


class RTSP_API RtspUrl {
public:
	static std::unique_ptr<RtspUrl> create(const std::string &url);

	using UniqueRtspUrl =
		std::unique_ptr<struct rtsp_url, decltype(&rtsp_url_free)>;

	enum rtsp_url_scheme getScheme() const noexcept;

	uint16_t getPort() const noexcept;

	const std::string &getOriginalUrl() const noexcept
	{
		return mOriginalUrl;
	}

	/**
	 * @brief Gets the full URL string using the ORIGINAL host name.
	 * This is the recommended URL for RTSP protocol requests.
	 */
	const std::string &getUrl() const noexcept
	{
		return mCache.url;
	}

	/**
	 * @brief Gets the full URL string with authentication, using the
	 * ORIGINAL host name.
	 */
	const std::string &getUrlWithAuth() const noexcept
	{
		return mCache.urlWithAuth;
	}

	/**
	 * @brief Gets the full URL string, prioritizing the RESOLVED IP
	 * address. The result is generated on-the-fly using
	 * rtsp_url_to_str_resolved().
	 */
	std::string getUrlResolved() const;

	/**
	 * @brief Gets the base URL string (without path), using the ORIGINAL
	 * host name.
	 */
	const std::string &getBaseUrl() const noexcept
	{
		return mCache.baseUrl;
	}

	/**
	 * @brief Gets the base URL string (without path) with authentication,
	 * using the ORIGINAL host name.
	 */
	const std::string &getBaseUrlWithAuth() const noexcept
	{
		return mCache.baseUrlWithAuth;
	}

	/**
	 * @brief Gets the base URL string (without path), prioritizing the
	 * RESOLVED IP address. The result is generated on-the-fly using
	 * rtsp_url_to_str_no_path_resolved().
	 */
	std::string getBaseUrlResolved() const;

	/**
	 * @brief Gets the host and port string, currently based on the ORIGINAL
	 * host name.
	 */
	const std::string &getAddr() const noexcept
	{
		return mCache.address;
	}

	const std::string &getHost() const noexcept
	{
		return mCache.host;
	}

	const std::string &getStreamName() const noexcept
	{
		return mCache.streamName;
	}

	const std::string &getUser() const noexcept
	{
		return mCache.user;
	}

	const std::string &getPass() const noexcept
	{
		return mCache.pass;
	}

	/**
	 * @brief Sets the resolved IP address for the URL.
	 * The internal caches (mUrlCache, mBaseUrlCache, etc.) are refreshed.
	 *
	 * @param ip The resolved IPv4 or IPv6 address.
	 * @return 0 on success, negative errno on failure (e.g., -ENOMEM).
	 */
	int setResolvedHost(const std::string &ip);

	/**
	 * @brief Returns the explicitly resolved host address (IP) if one was
	 * set. Corresponds directly to rtsp_url_get_resolved_host().
	 */
	const std::string &getResolvedHost() const noexcept
	{
		return mCache.resolvedHost;
	}

	/**
	 * @brief Checks if a resolved host address (IP) is available.
	 * Corresponds directly to rtsp_url_has_resolved_host().
	 */
	bool hasResolvedHost() const noexcept
	{
		return !mCache.resolvedHost.empty();
	}

	std::string toString() const
	{
		std::ostringstream oss;
		oss << mCache.url << " (user="
		    << (mCache.user.empty() ? "<none>" : mCache.user)
		    << ", pass=" << (mCache.pass.empty() ? "<none>" : "****")
		    << ", host=" << mCache.host << ", resolved="
		    << (mCache.resolvedHost.empty() ? "<none>"
						    : (mCache.resolvedHost))
		    << ", port=" << mCache.port
		    << ", stream=" << mCache.streamName << ")";
		return oss.str();
	}

	friend std::ostream &operator<<(std::ostream &os, const RtspUrl &url)
	{
		os << url.toString();
		return os;
	}

private:
	explicit RtspUrl(const std::string &url);

	void cacheData();

	/**
	 * @brief C Resource and Basic Fields (Parsed from the source URL)
	 */

	/* Underlying C structure pointer (RAII managed) */
	UniqueRtspUrl mParsedUrl;

	/* Original source URL */
	std::string mOriginalUrl{};

	/* true if port was provided in the source URL */
	bool mPortExplicit = false;

	/**
	 * @brief All derived, cached, and formatted URL components.
	 * These fields are computed and stored by the cacheData() method for
	 * fast access.
	 */
	struct {
		/** Cached Fields (Directly parsed from URL components) */

		/* Host */
		std::string host{};
		std::string resolvedHost{};

		/* Port */
		uint16_t port = 0;

		/* Authentication */
		std::string user{};
		std::string pass{};

		/* Path/stream */
		std::string streamName{};

		/** Generated Fields (Formatted for protocol requests) */

		/* e.g.: 'rtsp://127.0.0.1:8555/stream' */
		std::string url{};

		/* e.g.: 'rtsp://user:pass@127.0.0.1:8555/stream' */
		std::string urlWithAuth{};

		/* e.g.: 'rtsp://127.0.0.1:8555' */
		std::string baseUrl{};

		/* e.g.: 'rtsp://user:pass@127.0.0.1:8555' */
		std::string baseUrlWithAuth{};

		/* e.g.: '127.0.0.1:8555' */
		std::string address{};
	} mCache;
};
