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

#include <ctype.h>
#include <openssl/evp.h>

#define ULOG_TAG rtsp_auth
#include <ulog.h>
ULOG_DECLARE_TAG(rtsp_auth);


struct {
	enum rtsp_auth_type type;
	const char *str;
} rtsp_auth_type_map[] = {
	{RTSP_AUTH_TYPE_BASIC, RTSP_KEY_AUTH_BASIC},
	{RTSP_AUTH_TYPE_DIGEST, RTSP_KEY_AUTH_DIGEST},
};


const char *rtsp_auth_type_str(enum rtsp_auth_type val)
{
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_auth_type_map); i++) {
		if (val == rtsp_auth_type_map[i].type)
			return rtsp_auth_type_map[i].str;
	}
	return NULL;
}


enum rtsp_auth_type rtsp_auth_type_from_str(const char *str)
{
	if (!str)
		return RTSP_AUTH_TYPE_UNKNOWN;
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_auth_type_map); i++) {
		if (!strcasecmp(str, rtsp_auth_type_map[i].str))
			return rtsp_auth_type_map[i].type;
	}
	return RTSP_AUTH_TYPE_UNKNOWN;
}


struct {
	enum rtsp_auth_algorithm algo;
	const char *str;
} rtsp_auth_algo_map[] = {
	{RTSP_AUTH_ALGORITHM_UNSPECIFIED, NULL},
	{RTSP_AUTH_ALGORITHM_MD5, RTSP_KEY_AUTH_ALGO_MD5},
	{RTSP_AUTH_ALGORITHM_MD5_SESS, RTSP_KEY_AUTH_ALGO_MD5_SESS},
};


const char *rtsp_auth_algorithm_str(enum rtsp_auth_algorithm val)
{
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_auth_algo_map); i++) {
		if (val == rtsp_auth_algo_map[i].algo)
			return rtsp_auth_algo_map[i].str;
	}
	return NULL;
}


enum rtsp_auth_algorithm rtsp_auth_algorithm_from_str(const char *str)
{
	if (!str || str[0] == '\0')
		return RTSP_AUTH_ALGORITHM_UNSPECIFIED;
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_auth_algo_map); i++) {
		if (rtsp_auth_algo_map[i].str == NULL)
			continue;
		if (!strcasecmp(str, rtsp_auth_algo_map[i].str))
			return rtsp_auth_algo_map[i].algo;
	}
	return RTSP_AUTH_ALGORITHM_UNKNOWN;
}


struct {
	enum rtsp_auth_qop qop;
	const char *str;
} rtsp_auth_qop_map[] = {
	{RTSP_AUTH_QOP_UNSPECIFIED, NULL},
	{RTSP_AUTH_QOP_AUTH, RTSP_KEY_AUTH_QOP_AUTH},
	{RTSP_AUTH_QOP_AUTH_INT, RTSP_KEY_AUTH_QOP_AUTH_INT},
};


const char *rtsp_auth_qop_str(enum rtsp_auth_qop val)
{
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_auth_qop_map); i++) {
		if (val == rtsp_auth_qop_map[i].qop)
			return rtsp_auth_qop_map[i].str;
	}
	return NULL;
}


enum rtsp_auth_qop rtsp_auth_qop_from_str(const char *str)
{
	if (!str || str[0] == '\0')
		return RTSP_AUTH_QOP_UNSPECIFIED;
	for (size_t i = 0; i < SIZEOF_ARRAY(rtsp_auth_qop_map); i++) {
		if (rtsp_auth_qop_map[i].str == NULL)
			continue;
		if (!strcasecmp(str, rtsp_auth_qop_map[i].str))
			return rtsp_auth_qop_map[i].qop;
	}
	return RTSP_AUTH_QOP_UNKNOWN;
}


/**
 * Generates a hexadecimal MD5 from a formatted string.
 */
/* clang-format off */
__attribute__((__format__(__printf__, 2, 3)))
static int md5_hex_fmt(char out[33], const char *fmt, ...)
/* clang-format on */
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (n < 0 || n >= (int)sizeof(buf))
		return -EINVAL;

	unsigned char digest[EVP_MAX_MD_SIZE];
	unsigned int digest_len = 0;
	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	if (!ctx)
		return -ENOMEM;

	if (EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1 ||
	    EVP_DigestUpdate(ctx, buf, (size_t)n) != 1 ||
	    EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
		EVP_MD_CTX_free(ctx);
		return -EPROTO;
	}
	EVP_MD_CTX_free(ctx);

	unsigned int limit = (digest_len > 16) ? 16 : digest_len;

	for (unsigned int i = 0; i < limit; i++)
		snprintf(&out[i * 2], 33 - (i * 2), "%02x", digest[i]);

	out[32] = '\0';
	return 0;
}


int rtsp_auth_nc_str(char *buffer, size_t len, unsigned int nc)
{
	if (!buffer || len < 9)
		return -EINVAL;

	int ret = snprintf(buffer, len, "%08x", nc);
	if (ret < 0)
		return -errno;

	/* snprintf returns the number of chars that would have been written */
	if ((size_t)ret >= len)
		return -ENOBUFS;

	return 0;
}


int rtsp_auth_generate_basic_response(struct rtsp_authorization_header *auth,
				      const char *password)
{
	int ret;
	char *auth_clear = NULL;

	ULOG_ERRNO_RETURN_ERR_IF(!auth || !password, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(auth->type != RTSP_AUTH_TYPE_BASIC, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!auth->username, EINVAL);

	ret = asprintf(&auth_clear,
		       "%s%s%s",
		       auth->username,
		       password ? ":" : "",
		       password ? password : "");
	if (ret <= 0) {
		ret = -ENOMEM;
		ULOG_ERRNO("asprintf", -ret);
		goto out;
	}

	ret = rtsp_base64_encode(auth_clear, (size_t)ret, &auth->credentials);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_base64_encode", -ret);
		goto out;
	}

#ifdef RTSP_AUTH_DBG
	ULOGI("[DBG] encoded: %s: %s (clear: %s)",
	      rtsp_auth_type_str(auth->type),
	      auth->credentials,
	      auth_clear);
#endif

	ret = 0;

out:
	free(auth_clear);
	return ret;
}


/**
 * Generates auth->response for Digest RTSP.
 * auth->username, auth->realm, auth->nonce, and auth->uri must be set.
 * method = "SETUP", "PLAY", etc.
 */
int rtsp_auth_generate_digest_response(struct rtsp_authorization_header *auth,
				       const char *password,
				       enum rtsp_method_type method_type)
{
	int ret;
	char nc_str[9]; /* 8 hex digits + '\0' */

	ULOG_ERRNO_RETURN_ERR_IF(!auth || !password, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(auth->type != RTSP_AUTH_TYPE_DIGEST, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!auth->username, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!auth->realm, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!auth->uri, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!auth->nonce, EINVAL);

	size_t nonce_len = strnlen(auth->nonce, 256);
	if ((nonce_len == 0) || (nonce_len > 256)) {
		ULOGE("invalid nonce length: %zu", nonce_len);
		return -EINVAL;
	}

	const char *method_str = rtsp_method_type_str(method_type);
	ULOG_ERRNO_RETURN_ERR_IF(!method_str, EINVAL);

	/* qop_str might be NULL (if UNSPECIFIED) */
	const char *qop_str = rtsp_auth_qop_str(auth->qop);

	ULOG_ERRNO_RETURN_ERR_IF(auth->algorithm == RTSP_AUTH_ALGORITHM_UNKNOWN,
				 EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(auth->qop == RTSP_AUTH_QOP_UNKNOWN, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(auth->qop == RTSP_AUTH_QOP_AUTH_INT, ENOSYS);

	/* Generate a client nonce ONLY if not already provided */
	if ((auth->cnonce == NULL) || (auth->cnonce[0] == '\0')) {
		char cnonce_tmp[17];
		ret = futils_random_base16(cnonce_tmp, sizeof(cnonce_tmp), 8);
		if (ret < 0) {
			ULOG_ERRNO("futils_random_base16", -ret);
			return ret;
		}
		dup_field(&auth->cnonce, cnonce_tmp);
	} else {
		size_t cnonce_len = strnlen(auth->cnonce, 64);
		if ((cnonce_len < 4) || (cnonce_len > 64)) {
			ULOGE("invalid cnonce length: %zu (expected 4-64)",
			      cnonce_len);
			return -EINVAL;
		}
	}

	auth->nc++;
	if (auth->nc == 0)
		auth->nc = 1;

	ret = rtsp_auth_nc_str(nc_str, sizeof(nc_str), auth->nc);
	if (ret < 0) {
		ULOG_ERRNO("rtsp_auth_nc_str", -ret);
		return ret;
	}


#ifdef RTSP_AUTH_DBG
	ULOGW("[DBG] DIGEST DUMP: user='%s' realm='%s' pass='%s' "
	      "method='%s' uri='%s' algo='%s' qop='%s' nonce='%s' "
	      "cnonce='%s' nc=%d nc_str='%s'",
	      auth->username,
	      auth->realm,
	      password,
	      method_str,
	      auth->uri,
	      rtsp_auth_algorithm_str(auth->algorithm),
	      qop_str,
	      auth->nonce,
	      auth->cnonce,
	      auth->nc,
	      nc_str);
#endif

	char ha1[33];
	char ha2[33];

	/* Format is:
	 * HA1 = MD5(username:realm:password) */
	ret = md5_hex_fmt(
		ha1, "%s:%s:%s", auth->username, auth->realm, password);
	if (ret < 0)
		return ret;
#ifdef RTSP_AUTH_DBG
	ULOGI("[DBG] HA1: %s:%s:%s", auth->username, auth->realm, password);
#endif

	if (auth->algorithm == RTSP_AUTH_ALGORITHM_MD5_SESS) {
		/* Format is:
		 * HA1 = MD5(MD5(username:realm:password):nonce:cnonce) */
		ret = md5_hex_fmt(
			ha1, "%s:%s:%s", ha1, auth->nonce, auth->cnonce);
		if (ret < 0)
			return ret;
		ULOGI("[DBG] HA1: %s:%s:%s", ha1, auth->nonce, auth->cnonce);
	}

	/* Format is:
	 * HA2 = MD5(method:uri) */
	ret = md5_hex_fmt(ha2, "%s:%s", method_str, auth->uri);
	if (ret < 0)
		return ret;
#ifdef RTSP_AUTH_DBG
	ULOGI("[DBG] HA2: %s:%s", method_str, auth->uri);
#endif

	/* Allocate response buffer if needed */
	if (auth->response == NULL) {
		auth->response = malloc(33);
		if (!auth->response)
			return -ENOMEM;
	}

	/* Compute final response depending on qop */
	switch (auth->qop) {
	case RTSP_AUTH_QOP_UNSPECIFIED:
		/* Format is:
		 * response = MD5(HA1:nonce:HA2) */
		ret = md5_hex_fmt(
			auth->response, "%s:%s:%s", ha1, auth->nonce, ha2);
		if (ret < 0)
			return ret;
#ifdef RTSP_AUTH_DBG
		ULOGI("[DBG] response: %s:%s:%s", ha1, auth->nonce, ha2);
#endif
		break;
	case RTSP_AUTH_QOP_AUTH:
		/* Format is:
		 * response = MD5(HA1:nonce:nonceCount:cnonce:qop:HA2) */
		ret = md5_hex_fmt(auth->response,
				  "%s:%s:%s:%s:%s:%s",
				  ha1,
				  auth->nonce,
				  nc_str,
				  auth->cnonce,
				  qop_str,
				  ha2);
		if (ret < 0)
			return ret;
#ifdef RTSP_AUTH_DBG
		ULOGI("[DBG] response: %s:%s:%s:%s:%s:%s",
		      ha1,
		      auth->nonce,
		      nc_str,
		      auth->cnonce,
		      qop_str,
		      ha2);
#endif
		break;
	case RTSP_AUTH_QOP_AUTH_INT:
	default:
		return -ENOSYS;
	}

	return 0;
}
