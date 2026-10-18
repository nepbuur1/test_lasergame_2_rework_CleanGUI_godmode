#pragma once
void fakeLog(const char *tag, const char *format, ...);
#define ESP_LOGI(tag, ...) fakeLog(tag, __VA_ARGS__)
