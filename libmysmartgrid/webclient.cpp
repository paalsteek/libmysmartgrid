/**
 * This file is part of libmysmartgrid.
 *
 * (c) Fraunhofer ITWM - Mathias Dalheimer <dalheimer@itwm.fhg.de>,    2010
 *                       Ely de Oliveira   <ely.oliveira@itwm.fhg.de>, 2013
 *                       Stephan Platz     <platz@itwm.fhg.de>,        2014
 *
 * libklio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libklio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libklio. If not, see <http://www.gnu.org/licenses/>.
 */

#include "webclient.h"
#include "error.h"
#include <curl/curl.h>
#include <sstream>

using namespace libmsg;

/* Store curl responses in memory. Curl needs a custom callback for that. Otherwise it tries to write the response to a file. */
static size_t curlWriteCustomCallback(char *ptr, size_t size, size_t nmemb, void *data) {

    size_t realsize = size * nmemb;
		std::string *response = static_cast<std::string *> (data);;

		response->append(ptr, realsize);

    return realsize;
}

boost::shared_ptr<Json::Value> Webclient::performHttpRequest(const std::string& method, const std::string& url, const boost::shared_ptr<Json::Value>& body)
{
    long int httpCode = 0;
    CURLcode curlCode;
    std::string response = "";
    CURL *curl = nullptr;
    Json::Value *jsonValue = nullptr;
    curl_slist *headers = nullptr;

    try {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (!curl) {
            throw EnvironmentException("CURL could not be initiated.");
        }

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &response);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCustomCallback);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

        //Signal-handling is NOT thread-safe.
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

        //Required if next router has an ip-change.
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);

        headers = curl_slist_append(headers, "User-Agent: libklio");
        headers = curl_slist_append(headers, "X-Version: 1.0");
        headers = curl_slist_append(headers, "Accept: application/json,text/html");

				Json::FastWriter w;
        const std::string body = w.write(body);
        std::ostringstream oss;
        /*if (!key.empty())
            oss << "X-Digest: " << digest_message(body, key);
        if (!token.empty())
            oss << "X-Token: " << token;
        headers = curl_slist_append(headers, oss.str().c_str());*/

        if (method == "POST") {
            headers = curl_slist_append(headers, "Content-type: application/json");

            oss.str(std::string());
            oss << "Content-Length: " << body.length();
            headers = curl_slist_append(headers, oss.str().c_str());

            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curlCode = curl_easy_perform(curl);

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if (curlCode == CURLE_OK && httpCode == 200) {

						bool ok;
						Json::Reader r;
						ok = r.parse(response, *jsonValue, false);
						if (!ok) {
							throw GenericException("derp");
						}

            //Clean up
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            curl_global_cleanup();

            return boost::shared_ptr<Json::Value>(jsonValue);

        } else {
            oss.str(std::string());
            oss << "HTTPS request failed." <<
                    " cURL Error: " << curl_easy_strerror(curlCode) << ". " <<
                    " HTTPS code: " << httpCode;

            if (httpCode >= 400 && httpCode <= 499) {
                //throw DataFormatException(oss.str());
								throw GenericException(oss.str());

            } else if (httpCode >= 500 || httpCode == 0) {
                throw CommunicationException(oss.str());

            } else {
                //throw StoreException(oss.str());
								throw GenericException(oss.str());
            }
        }

    } catch (std::exception const& e) {

        //Clean up
        if (headers != nullptr) {
            curl_slist_free_all(headers);
        }
        if (curl != nullptr) {
            curl_easy_cleanup(curl);
        }
        curl_global_cleanup();
        throw;
    }
    //Some compilers require a return here
    throw GenericException("This point should never be reached.");
}

