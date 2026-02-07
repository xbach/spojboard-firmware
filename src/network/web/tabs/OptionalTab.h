#ifndef OPTIONAL_TAB_H
#define OPTIONAL_TAB_H

#include "../../../config/AppConfig.h"
#include <WString.h>

/**
 * Build the Optional Features tab HTML content
 * @param config Pointer to configuration structure
 * @return HTML string for optional features tab
 */
String buildOptionalTab(const Config* config);

#endif // OPTIONAL_TAB_H
