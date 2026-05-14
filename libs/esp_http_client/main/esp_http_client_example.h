#pragma once

#define CONFIG_EXAMPLE_HTTP_ENDPOINT "httpbin.org"


#ifdef __cplusplus
extern "C" {
#endif

// Declaratie van de C-functie
void http_client_setup();
void http_client_loop();

#ifdef __cplusplus
}
#endif