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

#include "rtsp_priv.h"
#include <rtsp/rtsp_url.hpp>

#include <ctype.h>

#define ULOG_TAG rtsp_urlcpp
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_urlcpp);


std::unique_ptr<RtspUrl> RtspUrl::create(const std::string &url)
{
	try {
		return std::unique_ptr<RtspUrl>(new RtspUrl(url));
	} catch (const std::invalid_argument &e) {
		ULOGE("failed to create RtspUrl for '%s': %s",
		      url.c_str(),
		      e.what());
		return nullptr;
	}
}


RtspUrl::RtspUrl(const std::string &url) :
		mParsedUrl(nullptr, rtsp_url_free), mOriginalUrl(url)
{
	struct rtsp_url *parsed = nullptr;

	int res = rtsp_url_parse(mOriginalUrl.c_str(), &parsed);

	if (res < 0 || !parsed) {
		if (parsed)
			rtsp_url_free(parsed);
		throw std::invalid_argument("RtspUrl: failed to parse URL: " +
					    url);
	}

	mParsedUrl.reset(parsed);

	cacheData();
}


enum rtsp_url_scheme RtspUrl::getScheme() const noexcept
{
	return rtsp_url_get_scheme(mParsedUrl.get());
}


uint16_t RtspUrl::getPort() const noexcept
{
	return rtsp_url_get_port(mParsedUrl.get());
}


int RtspUrl::setResolvedHost(const std::string &ip)
{
	int ret = rtsp_url_set_resolved_host(mParsedUrl.get(), ip.c_str());
	if (ret < 0) {
		/* Error already logged by C API */
		return ret;
	}

	/* Dont' call cacheData() because mUrlCache must keep the host. */
	const char *tmpResolvedHost =
		rtsp_url_get_resolved_host(mParsedUrl.get());
	mCache.resolvedHost = tmpResolvedHost ? tmpResolvedHost : "";

	return 0;
}


std::string RtspUrl::getUrlResolved() const
{
	char *str = nullptr;
	if (rtsp_url_to_str_resolved(mParsedUrl.get(), &str) == 0 && str) {
		std::string result(str);
		free(str);
		return result;
	}
	return {};
}


std::string RtspUrl::getBaseUrlResolved() const
{
	char *str = nullptr;
	if (rtsp_url_to_str_no_path_resolved(mParsedUrl.get(), &str) == 0 &&
	    str) {
		std::string result(str);
		free(str);
		return result;
	}
	return {};
}


void RtspUrl::cacheData()
{
	const char *tmpUser = rtsp_url_get_user(mParsedUrl.get());
	const char *tmpPass = rtsp_url_get_pass(mParsedUrl.get());
	const char *tmpHost = rtsp_url_get_host(mParsedUrl.get());
	const char *tmpResolvedHost =
		rtsp_url_get_resolved_host(mParsedUrl.get());
	const char *tmpPath = rtsp_url_get_path(mParsedUrl.get());
	uint16_t port = rtsp_url_get_port(mParsedUrl.get());
	enum rtsp_url_scheme scheme = rtsp_url_get_scheme(mParsedUrl.get());
	const char *schemeStr = rtsp_url_scheme_str(scheme);

	mCache.user = tmpUser ? tmpUser : "";
	mCache.pass = tmpPass ? tmpPass : "";
	mCache.host = tmpHost ? tmpHost : "";
	mCache.resolvedHost = tmpResolvedHost ? tmpResolvedHost : "";
	mCache.port = port;

	mPortExplicit = rtsp_url_is_port_explicit(mParsedUrl.get());

	if (tmpPath) {
		mCache.streamName =
			(tmpPath[0] == '/') ? (tmpPath + 1) : tmpPath;
	} else {
		mCache.streamName = "";
	}

	if (!schemeStr)
		return;

	const std::string &activeHost =
		mCache.resolvedHost.empty() ? mCache.host : mCache.resolvedHost;

	mCache.address = activeHost;
	if (mCache.port != 0) {
		mCache.address += ":" + std::to_string(mCache.port);
	}

	std::string prefix = schemeStr;

	if (mPortExplicit)
		mCache.baseUrl = prefix + mCache.address;
	else
		mCache.baseUrl = prefix + activeHost;

	std::string auth_prefix = prefix;
	if (!mCache.user.empty()) {
		auth_prefix += mCache.user;
		if (!mCache.pass.empty())
			auth_prefix += ":" + mCache.pass;
		auth_prefix += "@";
	}

	if (mPortExplicit)
		mCache.baseUrlWithAuth = auth_prefix + mCache.address;
	else
		mCache.baseUrlWithAuth = auth_prefix + activeHost;

	if (!mCache.baseUrl.empty())
		mCache.url = mCache.baseUrl + "/" + mCache.streamName;

	if (!mCache.baseUrlWithAuth.empty())
		mCache.urlWithAuth =
			mCache.baseUrlWithAuth + "/" + mCache.streamName;
}
