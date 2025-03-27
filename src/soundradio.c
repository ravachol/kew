#include "miniaudio.h"
#include "soundradio.h"

/*

radio.c

 Radio playback and search functions.

*/

#define MAX_SERVERS 12
#define MAX_STATIONS 1000
#define NI_MAXHOST 1025
#define WAIT_TIMEOUT_SECONDS 3
#define MAX_RECONNECT_RETRIES 20
#define BUFFER_SIZE 8192

typedef struct
{
        char *memory;
        size_t size;
} Memory;

typedef struct
{
        bool isBroken;
        char url[2048];
} Server;

Server servers[MAX_SERVERS];
int serverCount = 0;
bool hasUpdatedServerList = false;
RadioSearchResult *currentlyPlayingRadioStation = NULL;
RadioPlayerContext radioContext = {0};
int reconnectCounter = 0;

typedef struct
{
        const char *name;
        const char *code;
} Country;

typedef enum
{
        STREAM_TYPE_UNKNOWN,
        STREAM_TYPE_HLS,
        STREAM_TYPE_MP3,
        STREAM_TYPE_AAC,
        STREAM_TYPE_MPEGTS,
        STREAM_TYPE_DASH,
        NUM_STREAM_TYPES
} StreamType;

Country countries[] = {
    {"Andorra", "AD"},
    {"The United Arab Emirates", "AE"},
    {"Afghanistan", "AF"},
    {"Antigua And Barbuda", "AG"},
    {"Anguilla", "AI"},
    {"Albania", "AL"},
    {"Armenia", "AM"},
    {"Angola", "AO"},
    {"Antarctica", "AQ"},
    {"Argentina", "AR"},
    {"American Samoa", "AS"},
    {"Austria", "AT"},
    {"Australia", "AU"},
    {"Aruba", "AW"},
    {"Aland Islands", "AX"},
    {"Azerbaijan", "AZ"},
    {"Bosnia And Herzegovina", "BA"},
    {"Barbados", "BB"},
    {"Bangladesh", "BD"},
    {"Belgium", "BE"},
    {"Burkina Faso", "BF"},
    {"Bulgaria", "BG"},
    {"Bahrain", "BH"},
    {"Burundi", "BI"},
    {"Benin", "BJ"},
    {"Bermuda", "BM"},
    {"Brunei Darussalam", "BN"},
    {"Bolivia", "BO"},
    {"Bonaire", "BQ"},
    {"Brazil", "BR"},
    {"The Bahamas", "BS"},
    {"Botswana", "BW"},
    {"Belarus", "BY"},
    {"Belize", "BZ"},
    {"Canada", "CA"},
    {"The Democratic Republic Of The Congo", "CD"},
    {"The Central African Republic", "CF"},
    {"The Congo", "CG"},
    {"Switzerland", "CH"},
    {"Coted Ivoire", "CI"},
    {"The Cook Islands", "CK"},
    {"Chile", "CL"},
    {"Cameroon", "CM"},
    {"China", "CN"},
    {"Colombia", "CO"},
    {"Costa Rica", "CR"},
    {"Cuba", "CU"},
    {"Cabo Verde", "CV"},
    {"Curacao", "CW"},
    {"Christmas Island", "CX"},
    {"Cyprus", "CY"},
    {"Czechia", "CZ"},
    {"Germany", "DE"},
    {"Denmark", "DK"},
    {"Dominica", "DM"},
    {"The Dominican Republic", "DO"},
    {"Algeria", "DZ"},
    {"Ecuador", "EC"},
    {"Estonia", "EE"},
    {"Egypt", "EG"},
    {"Eritrea", "ER"},
    {"Spain", "ES"},
    {"Ethiopia", "ET"},
    {"Finland", "FI"},
    {"Fiji", "FJ"},
    {"The Falkland Islands Malvinas", "FK"},
    {"The Faroe Islands", "FO"},
    {"France", "FR"},
    {"The United Kingdom Of Great Britain And Northern Ireland", "UK"},
    {"Grenada", "GD"},
    {"Georgia", "GE"},
    {"French Guiana", "GF"},
    {"Guernsey", "GG"},
    {"Ghana", "GH"},
    {"Gibraltar", "GI"},
    {"Greenland", "GL"},
    {"The Gambia", "GM"},
    {"Guadeloupe", "GP"},
    {"Equatorial Guinea", "GQ"},
    {"Greece", "GR"},
    {"Guatemala", "GT"},
    {"Guam", "GU"},
    {"Guinea Bissau", "GW"},
    {"Guyana", "GY"},
    {"Hong Kong", "HK"},
    {"Honduras", "HN"},
    {"Croatia", "HR"},
    {"Haiti", "HT"},
    {"Hungary", "HU"},
    {"Indonesia", "ID"},
    {"Ireland", "IE"},
    {"Israel", "IL"},
    {"Isle Of Man", "IM"},
    {"India", "IN"},
    {"British Indian Ocean Territory", "IO"},
    {"Iraq", "IQ"},
    {"Islamic Republic Of Iran", "IR"},
    {"Iceland", "IS"},
    {"Italy", "IT"},
    {"Jamaica", "JM"},
    {"Jordan", "JO"},
    {"Japan", "JP"},
    {"Kenya", "KE"},
    {"Kyrgyzstan", "KG"},
    {"Cambodia", "KH"},
    {"The Comoros", "KM"},
    {"Saint Kitts And Nevis", "KN"},
    {"The Democratic Peoples Republic Of Korea", "KP"},
    {"The Republic Of Korea", "KR"},
    {"Kuwait", "KW"},
    {"The Cayman Islands", "KY"},
    {"Kazakhstan", "KZ"},
    {"The Lao Peoples Democratic Republic", "LA"},
    {"Lebanon", "LB"},
    {"Saint Lucia", "LC"},
    {"Liechtenstein", "LI"},
    {"Sri Lanka", "LK"},
    {"Lesotho", "LS"},
    {"Lithuania", "LT"},
    {"Luxembourg", "LU"},
    {"Latvia", "LV"},
    {"Libya", "LY"},
    {"Morocco", "MA"},
    {"Monaco", "MC"},
    {"The Republic Of Moldova", "MD"},
    {"Montenegro", "ME"},
    {"Madagascar", "MG"},
    {"Republic Of North Macedonia", "MK"},
    {"Mali", "ML"},
    {"Myanmar", "MM"},
    {"Mongolia", "MN"},
    {"Macao", "MO"},
    {"Martinique", "MQ"},
    {"Montserrat", "MS"},
    {"Malta", "MT"},
    {"Mauritius", "MU"},
    {"Malawi", "MW"},
    {"Mexico", "MX"},
    {"Malaysia", "MY"},
    {"Mozambique", "MZ"},
    {"Namibia", "NA"},
    {"New Caledonia", "NC"},
    {"The Niger", "NE"},
    {"Nigeria", "NG"},
    {"Nicaragua", "NI"},
    {"The Netherlands", "NL"},
    {"Norway", "NO"},
    {"Nepal", "NP"},
    {"Niue", "NU"},
    {"New Zealand", "NZ"},
    {"Oman", "OM"},
    {"Panama", "PA"},
    {"Peru", "PE"},
    {"French Polynesia", "PF"},
    {"Papua New Guinea", "PG"},
    {"The Philippines", "PH"},
    {"Pakistan", "PK"},
    {"Poland", "PL"},
    {"Saint Pierre And Miquelon", "PM"},
    {"Puerto Rico", "PR"},
    {"State Of Palestine", "PS"},
    {"Portugal", "PT"},
    {"Palau", "PW"},
    {"Paraguay", "PY"},
    {"Qatar", "QA"},
    {"Reunion", "RE"},
    {"Romania", "RO"},
    {"Serbia", "RS"},
    {"The Russian Federation", "RU"},
    {"Rwanda", "RW"},
    {"Saudi Arabia", "SA"},
    {"Seychelles", "SC"},
    {"The Sudan", "SD"},
    {"Sweden", "SE"},
    {"Singapore", "SG"},
    {"Ascension And Tristan Da Cunha Saint Helena", "SH"},
    {"Slovenia", "SI"},
    {"Svalbard And Jan Mayen", "SJ"},
    {"Slovakia", "SK"},
    {"Sierra Leone", "SL"},
    {"San Marino", "SM"},
    {"Senegal", "SN"},
    {"Somalia", "SO"},
    {"Suriname", "SR"},
    {"South Sudan", "SS"},
    {"Sao Tome And Principe", "ST"},
    {"El Salvador", "SV"},
    {"Syrian Arab Republic", "SY"},
    {"Eswatini", "SZ"},
    {"The Turks And Caicos Islands", "TC"},
    {"Chad", "TD"},
    {"The French Southern Territories", "TF"},
    {"Togo", "TG"},
    {"Thailand", "TH"},
    {"Tajikistan", "TJ"},
    {"Timor Leste", "TL"},
    {"Turkmenistan", "TM"},
    {"Tunisia", "TN"},
    {"Türkiye", "TR"},
    {"Trinidad And Tobago", "TT"},
    {"Taiwan, Republic Of China", "TW"},
    {"United Republic Of Tanzania", "TZ"},
    {"Ukraine", "UA"},
    {"Uganda", "UG"},
    {"The United States Minor Outlying Islands", "UM"},
    {"The United States Of America", "US"},
    {"Uruguay", "UY"},
    {"Uzbekistan", "UZ"},
    {"The Holy See", "VA"},
    {"Saint Vincent And The Grenadines", "VC"},
    {"Bolivarian Republic Of Venezuela", "VE"},
    {"British Virgin Islands", "VG"},
    {"US Virgin Islands", "VI"},
    {"Vietnam", "VN"},
    {"Vanuatu", "VU"},
    {"Wallis And Futuna", "WF"},
    {"Kosovo", "XK"},
    {"Yemen", "YE"},
    {"Mayotte", "YT"},
    {"South Africa", "ZA"},
    {"Zambia", "ZM"},
    {"Zimbabwe", "ZW"}};

typedef struct
{
        char *searchTerm;
        void (*callback)(const char *, const char *, const char *, const char *, const int, const int);
        bool *stopFlag;
} SearchThreadArgs;

pthread_mutex_t server_list_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_t currentThread;

bool stopFlag = false;

const char *getCountryCode(const char *country_name)
{
        for (int i = 0; i < (int)sizeof(countries) / (int)sizeof(countries[0]); i++)
        {
                if (strcmp(countries[i].name, country_name) == 0)
                {
                        return countries[i].code;
                }
        }

        return "";
}

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
        size_t real_size = size * nmemb;
        Memory *mem = (Memory *)userdata;

        char *ptr_new = realloc(mem->memory, mem->size + real_size + 1);
        if (!ptr_new)
                return 0;

        mem->memory = ptr_new;
        memcpy(mem->memory + mem->size, ptr, real_size);
        mem->size += real_size;
        mem->memory[mem->size] = '\0';

        return real_size;
}

const char *json_get(const char *json, const char *key, char *dst, size_t dst_size)
{
        const char *pos = strstr(json, key);
        if (!pos)
                return NULL;
        pos = strchr(pos, ':');
        if (!pos)
                return NULL;
        pos++;

        // Skip whitespace and quotes
        while (*pos && (*pos == ' ' || *pos == '\"'))
                pos++;

        const char *end = pos;
        while (*end && *end != '\"' && *end != ',')
                end++;

        size_t len = end - pos;
        if (len >= dst_size)
                len = dst_size - 1;
        strncpy(dst, pos, len);
        dst[len] = '\0';

        return end;
}

#define GETF(k, v) json_get(ptr, k, v, sizeof(v))

bool isControlChar(char c)
{
        return (c >= 0 && c <= 31) || c == 127;
}

// Function to sanitize the URL by removing control characters and checking for malicious patterns
bool sanitizeUrl(const char *url)
{
        // Check for common unsafe schemes
        if (strstr(url, "javascript:") != NULL ||
            strstr(url, "data:") != NULL ||
            strstr(url, "file:") != NULL ||
            strstr(url, "vbscript:") != NULL)
        {
                fprintf(stderr, "Unsafe URL pattern detected: %s\n", url);
                return false;
        }

        // Check for control characters
        for (const char *p = url; *p; p++)
        {
                if (isControlChar(*p))
                {
                        fprintf(stderr, "URL contains control character: %s\n", url);
                        return false;
                }
        }

        return true;
}

bool isSafeURL(const char *url)
{
        // Sanitize the URL first
        if (!sanitizeUrl(url))
        {
                return false;
        }

        CURLUcode rc;
        CURLU *curlu = curl_url();
        if (!curlu)
        {
                fprintf(stderr, "Failed to initialize CURLU.\n");
                return false;
        }

        rc = curl_url_set(curlu, CURLUPART_URL, url, 0);
        if (rc != CURLUE_OK)
        {
                fprintf(stderr, "Invalid URL: %s\n", url);
                curl_url_cleanup(curlu);
                return false;
        }

        char *scheme = NULL;
        rc = curl_url_get(curlu, CURLUPART_SCHEME, &scheme, 0);
        if (rc != CURLUE_OK || !scheme)
        {
                fprintf(stderr, "Failed to get URL scheme.\n");
                curl_free(scheme);
                curl_url_cleanup(curlu);
                return false;
        }

        bool is_safe = strcmp(scheme, "http") == 0 || strcmp(scheme, "https") == 0;

        curl_free(scheme);
        curl_url_cleanup(curlu);

        if (!is_safe)
        {
                fprintf(stderr, "Unsafe URL scheme: %s\n", url);
        }

        return is_safe;
}

int updateServerList()
{
        struct addrinfo hints, *res, *p;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo("all.api.radio-browser.info", NULL, &hints, &res) != 0)
                return -1;

        serverCount = 0;
        for (p = res; p != NULL && serverCount < MAX_SERVERS; p = p->ai_next)
        {
                char host[NI_MAXHOST];
                if (getnameinfo(p->ai_addr, p->ai_addrlen, host, sizeof(host), NULL, 0, NI_NAMEREQD) == 0)
                {
                        snprintf(servers[serverCount].url, sizeof(servers[serverCount].url), "https://%s", host);
                        servers[serverCount].isBroken = false;
                        serverCount++;
                }
        }
        freeaddrinfo(res);
        return serverCount > 0 ? 0 : -1;
}

Server *pickServer()
{
        for (int attempts = 0; attempts < serverCount; attempts++)
        {
                int idx = rand() % serverCount;
                if (!servers[idx].isBroken && isSafeURL(servers[idx].url))
                        return &servers[idx];
        }
        return NULL;
}

void *searchThreadFunction(void *arg)
{
        SearchThreadArgs *args = (SearchThreadArgs *)arg;
        CURL *curl = NULL;
        Memory res = {.memory = NULL, .size = 0};
        Server *server = NULL;
        char *encodedTerm = NULL;

        pthread_mutex_lock(&server_list_mutex);
        if (!hasUpdatedServerList)
        {
                if (updateServerList() != 0)
                {
                        pthread_mutex_unlock(&server_list_mutex);
                        free(args->searchTerm);
                        free(args);
                        return NULL;
                }
                hasUpdatedServerList = true;
        }
        pthread_mutex_unlock(&server_list_mutex);

        while (true)
        {
                pthread_mutex_lock(&server_list_mutex);
                server = pickServer();
                pthread_mutex_unlock(&server_list_mutex);

                if (!server)
                {
                        break;
                }

                free(res.memory);
                res.memory = NULL;
                res.size = 0;

                curl = curl_easy_init();
                if (!curl)
                {
                        continue;
                }

                encodedTerm = curl_easy_escape(curl, args->searchTerm, 0);
                if (!encodedTerm)
                {
                        curl_easy_cleanup(curl);
                        curl = NULL;
                        continue;
                }

                char url[2070];
                snprintf(url, sizeof(url), "%s/json/stations/byname/%s", server->url, encodedTerm);
                curl_free(encodedTerm);
                encodedTerm = NULL;

                curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);

                char userAgent[64];
                snprintf(userAgent, sizeof(userAgent), "kew/%s", VERSION);
                curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

                CURLcode result = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
                curl = NULL;

                if (result == CURLE_OK && res.memory != NULL)
                {
                        char *ptr = res.memory;
                        int count = 0;

                        while ((ptr = strchr(ptr, '{')) != NULL && count < MAX_STATIONS)
                        {
                                char name[128] = {0}, resolved[4096] = {0}, country[64] = {0}, codec[32] = {0}, bitrate_str[16] = {0}, votes_str[16] = {0};
                                int br = 0, vo = 0;

                                if (!GETF("\"name\"", name))
                                {
                                        ptr++;
                                        continue;
                                }
                                if (!GETF("\"url_resolved\"", resolved))
                                {
                                        ptr++;
                                        continue;
                                }
                                GETF("\"country\"", country);
                                GETF("\"codec\"", codec);
                                if (GETF("\"bitrate\"", bitrate_str))
                                        br = atoi(bitrate_str);
                                if (GETF("\"votes\"", votes_str))
                                        vo = atoi(votes_str);

                                if (!isSafeURL(resolved))
                                {
                                        ptr++;
                                        continue;
                                }
                                args->callback(name, resolved, getCountryCode(country), codec, br, vo);
                                count++;
                                ptr++;

                                if (count % 10 == 0)
                                {
                                        refresh = true;
                                        c_sleep(100);
                                }
                        }

                        free(res.memory);
                        res.memory = NULL;
                        res.size = 0;

                        free(encodedTerm);
                        if (curl)
                                curl_easy_cleanup(curl);

                        free(args->searchTerm);
                        free(args);

                        return NULL;
                }
                else
                {

                        pthread_mutex_lock(&server_list_mutex);
                        server->isBroken = true;
                        pthread_mutex_unlock(&server_list_mutex);

                        free(res.memory);
                        res.memory = NULL;
                        res.size = 0;
                }
        }

        return NULL;
}

void stopCurrentThread() {
    if (currentThread) {
        stopFlag = true;
        pthread_cancel(currentThread);  // This cancels the thread if it is still running
        pthread_join(currentThread, NULL);  // Wait for the thread to join
        stopFlag = false;  // Reset the stop flag
    }
}

int internetRadioSearch(const char *searchTerm, void (*callback)(const char *, const char *, const char *, const char *, const int, const int)) {
    pthread_t threadId;
    SearchThreadArgs *args;

    if (!searchTerm || !callback) {
        return -1;
    }

    stopCurrentThread();

    args = malloc(sizeof(SearchThreadArgs));
    if (!args) {
        return -1;
    }

    args->searchTerm = strdup(searchTerm);
    if (!args->searchTerm) {
        free(args);
        return -1;
    }

    args->callback = callback;
    args->stopFlag = &stopFlag;


    if (pthread_create(&threadId, NULL, searchThreadFunction, args) != 0) {
        free(args->searchTerm);
        free(args);
        return -1;
    }

    pthread_detach(threadId);
    currentThread = threadId;

    return 0;
}

static bool radioIsPlaying = false;

bool isRadioPlaying()
{
        return radioIsPlaying;
}

static size_t curl_writefunc(void *ptr, size_t size, size_t nmemb, void *userdata)
{
        stream_buffer *buf = (stream_buffer *)userdata;
        size_t bytes = size * nmemb;
        size_t written = 0;

        pthread_mutex_lock(&buf->mutex);

        if (buf->eof)
        {
                pthread_mutex_unlock(&buf->mutex);
                return 0;
        }

        unsigned char *src = (unsigned char *)ptr;
        while (written < bytes)
        {
                size_t nextWritePos = (buf->write_pos + 1) % STREAM_BUFFER_SIZE;

                // If buffer is full, advance read pointer (drop oldest)
                if (nextWritePos == buf->read_pos)
                {
                        buf->read_pos = (buf->read_pos + 1) % STREAM_BUFFER_SIZE;
                }

                buf->buffer[buf->write_pos] = src[written];
                buf->write_pos = nextWritePos;
                written++;
        }

        buf->last_data_time = time(NULL);

        // Signal to the consumer that the data is ready
        pthread_cond_signal(&buf->cond);
        pthread_mutex_unlock(&buf->mutex);

        return bytes;
}

static ma_result decoder_read_callback(ma_decoder *decoder, void *pBufferOut, size_t bytesToRead, size_t *bytesRead)
{
        stream_buffer *buf = decoder->pUserData;
        size_t total_read = 0;
        unsigned char *out = (unsigned char *)pBufferOut;

        pthread_mutex_lock(&buf->mutex);

        while (total_read < bytesToRead)
        {
                // Wait until data is available OR EOF is flagged.
                while ((buf->read_pos == buf->write_pos) && !buf->eof)
                {
                        // Set up a timespec for timeout
                        struct timespec ts;
                        clock_gettime(CLOCK_REALTIME, &ts);
                        ts.tv_sec += WAIT_TIMEOUT_SECONDS;

                        // Use timed wait
                        int wait_result = pthread_cond_timedwait(&buf->cond, &buf->mutex, &ts);
                        if (wait_result == ETIMEDOUT)
                        {
                                // Check how long it's been since last data arrived
                                time_t now = time(NULL);
                                if ((now - buf->last_data_time) >= WAIT_TIMEOUT_SECONDS)
                                {
                                        // We haven't received data in too long, so signal error.
                                        pthread_mutex_unlock(&buf->mutex);
                                        *bytesRead = total_read;

                                        buf->stale = true;

                                        return MA_ERROR;
                                }
                        }
                }

                if ((buf->read_pos == buf->write_pos) && buf->eof)
                {
                        pthread_mutex_unlock(&buf->mutex);
                        *bytesRead = total_read;
                        return (total_read > 0) ? MA_SUCCESS : MA_AT_END;
                }

                size_t chunk = (buf->write_pos > buf->read_pos)
                                   ? (buf->write_pos - buf->read_pos)
                                   : (STREAM_BUFFER_SIZE - buf->read_pos);

                size_t to_copy = (chunk < (bytesToRead - total_read))
                                     ? chunk
                                     : (bytesToRead - total_read);

                memcpy(out + total_read, &buf->buffer[buf->read_pos], to_copy);
                buf->read_pos = (buf->read_pos + to_copy) % STREAM_BUFFER_SIZE;
                total_read += to_copy;
        }

        pthread_mutex_unlock(&buf->mutex);

        *bytesRead = total_read;
        return MA_SUCCESS;
}

static ma_result decoder_seek_callback(ma_decoder *decoder, ma_int64 offset, ma_seek_origin origin)
{
        (void)decoder;
        (void)offset;
        (void)origin;
        return MA_ERROR; // Can't seek in internet radio
}

static void audio_data_callback(ma_device *device, void *output, const void *input, ma_uint32 frameCount)
{
        (void)input;
        ma_decoder *decoder = (ma_decoder *)device->pUserData;
        ma_uint64 frames_read = 0;
        ma_decoder_read_pcm_frames(decoder, output, frameCount, &frames_read);
        if (frames_read < frameCount)
        {
                memset((char *)output + frames_read * device->playback.channels * sizeof(float), 0, (frameCount - frames_read) * device->playback.channels * sizeof(float));
        }
}

RadioSearchResult *copyRadioSearchResult(const RadioSearchResult *original)
{
        if (!original)
                return NULL;

        RadioSearchResult *copy = malloc(sizeof(RadioSearchResult));
        if (!copy)
        {
                fprintf(stderr, "Memory allocation failed.\n");
                return NULL;
        }

        memcpy(copy, original, sizeof(RadioSearchResult));
        return copy;
}

RadioSearchResult *getCurrentPlayingRadioStation(void)
{
        return currentlyPlayingRadioStation;
}

void setCurrentlyPlayingRadioStation(const RadioSearchResult *result)
{
        if (currentlyPlayingRadioStation != NULL)
                freeCurrentlyPlayingRadioStation();

        if (result != NULL)
                currentlyPlayingRadioStation = copyRadioSearchResult(result);
}

void freeCurrentlyPlayingRadioStation(void)
{
        if (currentlyPlayingRadioStation != NULL)
        {
                free(currentlyPlayingRadioStation);
                currentlyPlayingRadioStation = NULL;
        }
}

void stopRadio(void)
{
        pthread_mutex_lock(&radioContext.buf.mutex);
        radioContext.buf.eof = 1;
        radioContext.buf.read_pos = radioContext.buf.write_pos = 0;
        radioContext.buf.stale = false;
        pthread_cond_broadcast(&radioContext.buf.cond);
        pthread_mutex_unlock(&radioContext.buf.mutex);

        if (radioContext.curl_thread)
        {
                pthread_join(radioContext.curl_thread, NULL);
                radioContext.curl_thread = 0;
        }

        resetDevice();

        memset(&device, 0, sizeof(device));

        ma_decoder_uninit(&radioContext.decoder);

        if (radioContext.curl != NULL)
        {
                curl_easy_cleanup(radioContext.curl);
                radioContext.curl = NULL;
        }

        pthread_mutex_destroy(&radioContext.buf.mutex);
        pthread_cond_destroy(&radioContext.buf.cond);

        memset(&radioContext.decoder, 0, sizeof(ma_decoder));

        radioIsPlaying = false;

        freeCurrentlyPlayingRadioStation();
}

void reconnectRadioIfNeeded()
{
        if (radioContext.buf.stale && radioIsPlaying && reconnectCounter < MAX_RECONNECT_RETRIES)
        {
                RadioSearchResult *station = copyRadioSearchResult(getCurrentPlayingRadioStation());

                playRadioStation(station);

                reconnectCounter++;

                if (station)
                        free(station);
        }
}

void *curl_perform_wrapper(void *arg)
{
        CURL *curl = (CURL *)arg;
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK)
        {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        return NULL;
}

int playRadioStation(const RadioSearchResult *station)
{
        if (isRadioPlaying())
        {
                RadioSearchResult *radio = getCurrentPlayingRadioStation();

                // If it's not the same station, reset the reconnect counter
                if (strcmp(radio->url_resolved, station->url_resolved) != 0)
                        reconnectCounter = 0;
        }

        stopRadio();

        if (station == NULL)
                return -1;

        if (!(radioContext.curl = curl_easy_init()))
        {
                fprintf(stderr, "Curl init failed\n");
                return -1;
        }

        pthread_mutex_init(&radioContext.buf.mutex, NULL);
        pthread_cond_init(&radioContext.buf.cond, NULL);
        radioContext.buf.eof = 0;
        radioContext.buf.stale = false;

        curl_easy_setopt(radioContext.curl, CURLOPT_URL, station->url_resolved);
        curl_easy_setopt(radioContext.curl, CURLOPT_WRITEFUNCTION, curl_writefunc);
        curl_easy_setopt(radioContext.curl, CURLOPT_WRITEDATA, &radioContext.buf);
        curl_easy_setopt(radioContext.curl, CURLOPT_FOLLOWLOCATION, 1L);

        char userAgent[64];
        snprintf(userAgent, sizeof(userAgent), "kew/%s", VERSION);
        curl_easy_setopt(radioContext.curl, CURLOPT_USERAGENT, userAgent);

        pthread_create(&radioContext.curl_thread, NULL, curl_perform_wrapper, radioContext.curl);

        ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 44100);

        decoderConfig.encodingFormat = ma_encoding_format_mp3;

        if (ma_decoder_init(decoder_read_callback, decoder_seek_callback, &radioContext.buf, &decoderConfig, &radioContext.decoder) != MA_SUCCESS)
        {
                fprintf(stderr, "Decoder init failed\n");
                setErrorMessage("Radio station unsupported or unavailable.");

                return -1;
        }

        ma_device_config devConfig = ma_device_config_init(ma_device_type_playback);
        devConfig.playback.format = radioContext.decoder.outputFormat;
        devConfig.playback.channels = radioContext.decoder.outputChannels;
        devConfig.sampleRate = radioContext.decoder.outputSampleRate;
        devConfig.dataCallback = audio_data_callback;
        devConfig.pUserData = &radioContext.decoder;

        if (ma_device_init(NULL, &devConfig, &device) != MA_SUCCESS)
        {
                fprintf(stderr, "Device init failed\n");
                ma_decoder_uninit(&radioContext.decoder);
                return -1;
        }

        setVolume(getCurrentVolume());

        if (ma_device_start(&device) != MA_SUCCESS)
        {
                fprintf(stderr, "Failed to start device playback\n");
                ma_device_uninit(&device);
                ma_decoder_uninit(&radioContext.decoder);
                return -1;
        }

        stopped = false;
        paused = false;

        refresh = true;

        pthread_cond_signal(&radioContext.buf.cond);

        radioIsPlaying = true;

        setCurrentlyPlayingRadioStation(station);

        return 0;
}
