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

/*
 * Throwaway self-signed test certificate/key (CN=localhost), embedded as PEM
 * string literals so tst-librtsp doesn't depend on being run from any
 * particular working directory. Reused verbatim from the identical fixture
 * already checked into packages/libtransport-tls/tests/certs/ttls_test_certs.h
 * (same non-secret, throwaway pair) rather than introducing a second one,
 * since this test build has no local means to generate a fresh certificate.
 */

#ifndef _RTSP_TEST_CERTS_H_
#define _RTSP_TEST_CERTS_H_

static const char RTSP_TEST_TLS_CERT_PEM[] =
	"-----BEGIN CERTIFICATE-----\n"
	"MIIC1zCCAb+gAwIBAgIJAPhhEqr7ElMxMA0GCSqGSIb3DQEBCwUAMBQxEjAQBgNV\n"
	"BAMMCWxvY2FsaG9zdDAeFw0yNjA4MTcxNTQ0MDRaFw0zNjA4MTUxNTQ0MDRaMBQx\n"
	"EjAQBgNVBAMMCWxvY2FsaG9zdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoC\n"
	"ggEBAJaViHIowwKrEgZWide9jDwE3zr2oJK2SxyHMYfJ7GNNUY8gszAnAOmO7Ysd\n"
	"+ToGTjhCF+p1gMg7RxSAk+ULCz/0uKYqQMNHBadflZyAGUKHHewv+hVSFyqq+AuX\n"
	"zTbcWrBc4KCYcKXKBjxzHfO29pYERSJbUqYwVSxbJ+qm6r+MB42tkzH2HYPMAvK1\n"
	"aCcGSgAr4ZXxSW5FOtVk4Wx0PnOj7SOolmDxOrWEo6i+0V58HD8ChEZi3KC7U3y3\n"
	"gIDxlrA7sd2E5+fS7Bq9mO5i+nN0/n+F9jtuK9cp3i0GJsKmwxwUP4/a+5cEg1+z\n"
	"C6hGfLRUSln/LDk31SVWPb/WBYcCAwEAAaMsMCowDAYDVR0TAQH/BAIwADAaBgNV\n"
	"HREEEzARgglsb2NhbGhvc3SHBH8AAAEwDQYJKoZIhvcNAQELBQADggEBAExR1ZT3\n"
	"C1BO7sXZgmvOc2pLjzDB+UkU0JDEdPZwvSOXvgVkkIaXjD3RqbCy2Fe3xiXjujis\n"
	"h8VIewKCc4qqnArt3zatO6LVaF8vsNTU5Zp8CEQFJ5hy4Wu7NMojNVyxuHD17QlH\n"
	"KAVSZAhTb5kjJgauPxOhMCGqUeY8PxsCuW7/g0X9LQijFn+7vS4KhDJGC+7J+KZ9\n"
	"Ey7fGJH4ZM76IpsfN2vghzhZRDqrce7fCR2jE6BIjFcyHLlqq7kTYgnGWA1WRMlU\n"
	"2D/gcjyX+DYUM5qqwvajH1OHymwkk2Zrl+0+tfFwh4T7HFMSswPHf98G+cF27jU1\n"
	"h0pCdOehi5P+AQw=\n"
	"-----END CERTIFICATE-----\n";

static const char RTSP_TEST_TLS_KEY_PEM[] =
	"-----BEGIN PRIVATE KEY-----\n"
	"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCWlYhyKMMCqxIG\n"
	"VonXvYw8BN869qCStkschzGHyexjTVGPILMwJwDpju2LHfk6Bk44QhfqdYDIO0cU\n"
	"gJPlCws/9LimKkDDRwWnX5WcgBlChx3sL/oVUhcqqvgLl8023FqwXOCgmHClygY8\n"
	"cx3ztvaWBEUiW1KmMFUsWyfqpuq/jAeNrZMx9h2DzALytWgnBkoAK+GV8UluRTrV\n"
	"ZOFsdD5zo+0jqJZg8Tq1hKOovtFefBw/AoRGYtygu1N8t4CA8ZawO7HdhOfn0uwa\n"
	"vZjuYvpzdP5/hfY7bivXKd4tBibCpsMcFD+P2vuXBINfswuoRny0VEpZ/yw5N9Ul\n"
	"Vj2/1gWHAgMBAAECggEAAJ6o719ZLQ6RlzkWW53jsxFn7pmHH+0O0Ddg5XnG4UrQ\n"
	"10B/mitEQqDCxHWLyfvhugA3Xx2qSmzy9yTnQ7mqccmb7b7iPZ62aYLfmdSg35Yo\n"
	"HLRYg0lrEv2/0O+ZRp/ce/WbO98LJYOJWSrgqIxDkFmzWUaVnOYGSy897Z22QVzk\n"
	"9mGViEmr5AuWervbOc0ePXgO/czkChOZeIf5eu5U139gZweGmoHoU1mxplzbxtyX\n"
	"jhRchkJk/MWkl7Yl/egPOJHMla4taf+QPZWEUu5lIGFBAD6fI22iLsneSb8Zuqnr\n"
	"uSKL2EH8+K2CETlArlSlpkF+pY+tPbJBx6+yzDHPgQKBgQDFIdSKVGDa7k4zrdbQ\n"
	"LNIFS84vZhYj+Jl38mbLFwI6BL2o2Vrm47GDgd754EIRFySA1rcPN0hNcbyJTgcf\n"
	"RhXNWfP7rhl1zOJuaJ3ByUUvkeBLJCKgUU2Q8aD1uH+cG45K9xQoaq9vRzQwKbTS\n"
	"CzMokouhDTuwAtb22fZtfJy3oQKBgQDDjT451VqgfvSETWixO00oj4LXxzBWJahk\n"
	"/jhJe/eccv6ogg0dzgFsnKkkmrOqZh9HYIc+8P/wyAW4Ucw/hFUVzknoCBmr5V58\n"
	"Gb3bBa8C1naqFseDCy6kHQoZyygBEbmzH++YeFeO1+aFxF5c73YNbB5cf1Pe4S8g\n"
	"h/pfG0qMJwKBgGOVknwK4h7W8dRzhigF7FbWk6MoLBkh1c1dPAZuaiaLDJCGtMAc\n"
	"2Cj5631Jh7aAfWVEkpdyMulEgpzOORTYX90sCu/iZGQ5C04BNWiW14ePZsIuz/sy\n"
	"9z84gTqAaJ6g4QIHzTwZoORTEQ8fkoPlOoPNfBHw0G07EgMD2tQ2dijhAoGBAJYf\n"
	"cI3lhNWBoSWQZ7bn16wzwDb269y2vDgMEhoX2Vd50JZ8gKVI6T8AZbl8KkG+dHTP\n"
	"XgSr71BhvkJqSWrZpJhA7ev7i/my9H5BJMdn7Zs4GZEqO7AWfY/v2zlCBUwM14r0\n"
	"EXPzrwQyKv62adre2rojLovEFBceuJc4zLUGtowxAoGBAIccYL1YQqKDjUwHaVoB\n"
	"kXYYYY9j2Kh7yxXYAj1E6m+oOo60hn6ZZAgG5qyDiePJGNKi8aHWw3SPspePoGiP\n"
	"ZpecDdswqQKuog7dCwNRAvuQkxijPs78weTTT09m0jwGuG6a/C+H2HgS52dMPO6l\n"
	"wpijzKMjaTOXeIkx2ov3l8DR\n"
	"-----END PRIVATE KEY-----\n";

#endif /* !_RTSP_TEST_CERTS_H_ */
