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

#ifndef _RTSP_URL_H_
#define _RTSP_URL_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>

#include <libpomp.h>


/**
 * @brief Recognized transport schemes for RTSP URLs.
 */
enum rtsp_url_scheme {
	/* Unknown or unspecified scheme. */
	RTSP_URL_SCHEME_UNKNOWN = 0,
	/* Transport via TCP (e.g., rtsp://). */
	RTSP_URL_SCHEME_TCP,
	/* Transport via UDP (e.g., rtspu://). */
	RTSP_URL_SCHEME_UDP,
	/* Transport via TCP with TLS (e.g., rtsps://). */
	RTSP_URL_SCHEME_TCP_TLS,
};


struct rtsp_url;


/**
 * @brief Returns the string representation of an RTSP URL scheme.
 *
 * @param val: The scheme enumeration value.
 *
 * @return The corresponding string or NULL if unknown.
 */
RTSP_API const char *rtsp_url_scheme_str(enum rtsp_url_scheme val);


/**
 * @brief Tries to deduce an RTSP URL scheme from a string.
 *
 * @param str: The scheme string.
 *
 * @return The corresponding rtsp_url_scheme enumeration value.
 */
RTSP_API enum rtsp_url_scheme rtsp_url_scheme_from_str(const char *str);


/**
 * @brief Get the standard default port number for a given RTSP URL scheme.
 *
 * @param val: The scheme enumeration value.
 *
 * @return The default port number (e.g., 554 for TCP/UDP) or 0 if unknown.
 */
RTSP_API uint16_t rtsp_url_scheme_default_port(enum rtsp_url_scheme val);


/**
 * @brief Parses an RTSP URL string and populates an rtsp_url structure.
 *
 * The returned structure is allocated by the function and must be freed
 * by the caller using rtsp_url_free(). If parsing fails, *ret_obj will be NULL.
 *
 * @param url: The URL string to parse.
 * @param ret_obj: Pointer to the pointer for the new rtsp_url structure
 * (output).
 *
 * @return 0 on success, negative errno on failure.
 */
RTSP_API int rtsp_url_parse(const char *url, struct rtsp_url **ret_obj);


/**
 * @brief Frees the memory allocated for an rtsp_url structure.
 *
 * If the object is NULL, the function does nothing.
 *
 * @param url: The rtsp_url structure to free.
 *
 * @return 0 on success.
 */
RTSP_API int rtsp_url_free(struct rtsp_url *url);


/**
 * @brief Creates a deep copy of an rtsp_url structure.
 *
 * All string components are freshly allocated in the destination structure.
 *
 * @param src: Pointer to the source structure.
 * @param dst: Pointer to the pointer for the new copied structure (output).
 *
 * @return 0 on success, negative errno on error.
 */
RTSP_API int rtsp_url_copy(const struct rtsp_url *src, struct rtsp_url **dst);


/**
 * @brief Compares two rtsp_url structures.
 *
 * All string components (user, pass, host, path) are compared, handling NULL
 * pointers correctly (two NULLs are equal, one NULL is unequal).
 *
 * @param url1: Pointer to the first structure.
 * @param url2: Pointer to the second structure.
 *
 * @return true if the structures are identical, false otherwise.
 */
RTSP_API bool rtsp_url_cmp(const struct rtsp_url *url1,
			   const struct rtsp_url *url2);


/**
 * @brief Gets the transport scheme of the URL.
 */
RTSP_API enum rtsp_url_scheme rtsp_url_get_scheme(const struct rtsp_url *url);


/**
 * @brief Gets the username component.
 *
 * @return The username string (internal memory, DO NOT FREE) or NULL if not
 * present.
 */
RTSP_API const char *rtsp_url_get_user(const struct rtsp_url *url);


/**
 * @brief Gets the password component.
 *
 * @return The password string (internal memory, DO NOT FREE) or NULL if not
 * present.
 */
RTSP_API const char *rtsp_url_get_pass(const struct rtsp_url *url);


/**
 * @brief Sets the resolved host address (if resolved).
 *
 * @return 0 on success, negative errno in case of error.
 */
RTSP_API int rtsp_url_set_resolved_host(struct rtsp_url *url,
					const char *resolved_host);


/**
 * @brief Gets the unresolved host name component.
 *
 * @return The host string (internal memory, DO NOT FREE).
 */
RTSP_API const char *rtsp_url_get_host(const struct rtsp_url *url);


/**
 * @brief Gets the resolved host address (if resolved internally).
 *
 * @return The resolved host string (internal memory, DO NOT FREE) or NULL if
 * not resolved.
 */
RTSP_API const char *rtsp_url_get_resolved_host(const struct rtsp_url *url);


/**
 * @brief Checks if the URL structure holds a resolved host address.
 *
 * @return true if resolved_host is set, false otherwise.
 */
RTSP_API bool rtsp_url_has_resolved_host(const struct rtsp_url *url);


/**
 * @brief Gets the port number.
 */
RTSP_API uint16_t rtsp_url_get_port(const struct rtsp_url *url);


/**
 * @brief Checks if the port was explicitly included in the URL string.
 *
 * @return true if the port was explicitly set, false otherwise.
 */
RTSP_API bool rtsp_url_is_port_explicit(const struct rtsp_url *url);


/**
 * @brief Gets the path component.
 *
 * @return The path string (internal memory, DO NOT FREE) or NULL if not
 * present.
 */
RTSP_API const char *rtsp_url_get_path(const struct rtsp_url *url);


/**
 * @brief Converts an rtsp_url structure back into a full URL string.
 *
 * This function uses the ORIGINAL host name (url->host), regardless of whether
 * an IP has been resolved. This is typically used for generating URLs
 * inside RTSP protocol messages (SETUP, PLAY) as resource identifiers.
 *
 * The returned string is allocated by the function and must be freed
 * by the caller. The path component is included.
 *
 * @param url: The rtsp_url structure to convert.
 * @param str: Pointer to the generated URL string (output).
 *
 * @return 0 on success, negative errno on error (e.g., -ENOMEM).
 */
RTSP_API int rtsp_url_to_str(const struct rtsp_url *url, char **str);


/**
 * @brief Converts an rtsp_url structure back into a full URL string,
 * prioritizing the resolved IP address.
 *
 * This function uses the resolved IP address (url->resolved_host) if available,
 * otherwise it defaults to the original host name (url->host). This is intended
 * for logging, debugging, or displaying the 'active' address of the URL.
 *
 * The returned string is allocated by the function and must be freed
 * by the caller. The path component is included.
 *
 * @param url: The rtsp_url structure to convert.
 * @param str: Pointer to the generated URL string (output).
 *
 * @return 0 on success, negative errno on error (e.g., -ENOMEM).
 */
RTSP_API int rtsp_url_to_str_resolved(const struct rtsp_url *url, char **str);


/**
 * @brief Converts an rtsp_url structure back into a URL string without the
 * path, using the ORIGINAL host name.
 *
 * This function uses the ORIGINAL host name (url->host) for the authority part.
 * The returned string is allocated by the function and must be freed
 * by the caller. Only the authority part (scheme, user, host, port) is
 * included.
 *
 * @param url: The rtsp_url structure to convert.
 * @param str: Pointer to the generated URL string (output).
 *
 * @return 0 on success, negative errno on error (e.g., -ENOMEM).
 */
RTSP_API int rtsp_url_to_str_no_path(const struct rtsp_url *url, char **str);


/**
 * @brief Converts an rtsp_url structure back into a URL string without the
 * path, prioritizing the resolved IP address.
 *
 * This function prioritizes the resolved IP address (url->resolved_host)
 * for the authority part, otherwise it defaults to the original host name.
 * The returned string is allocated by the function and must be freed
 * by the caller. Only the authority part (scheme, user, host, port) is
 * included.
 *
 * @param url: The rtsp_url structure to convert.
 * @param str: Pointer to the generated URL string (output).
 *
 * @return 0 on success, negative errno on error (e.g., -ENOMEM).
 */
RTSP_API int rtsp_url_to_str_no_path_resolved(const struct rtsp_url *url,
					      char **str);


/**
 * @brief Removes the credentials (username and password) from an RTSP URL
 * string.
 *
 * The returned string is allocated by the function and must be freed by the
 * caller. If the URL does not contain credentials, a copy of the original URL
 * is returned.
 *
 * @param url: The input URL string.
 * @param ret_url: The URL without credentials (output).
 *
 * @return 0 on success, negative errno on error.
 */
RTSP_API int rtsp_url_strip_credentials(const char *url, char **ret_url);


/**
 * @brief Anonymize an RTSP URL.
 *
 * The returned string is allocated by the function and it is up to the caller
 * to free it when no longer needed. Credentials are replaced or omitted,
 * and the resolved host is masked.
 *
 * @param url: URL to anonymize
 * @param ret_url: the anonymized URL (output)
 *
 * @return 0 on success, negative errno on error.
 */
RTSP_API int rtsp_url_anonymize(const char *url, char **ret_url);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_RTSP_URL_H_ */
