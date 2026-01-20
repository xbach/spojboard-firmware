#ifndef WEB_TEMPLATES_H
#define WEB_TEMPLATES_H

#include <Arduino.h>

// Common HTML header with CSS styles
const char HTML_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>SpojBoard Configuration</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
               max-width: 600px; margin: 0 auto; padding: 20px; background: #1a1a2e; color: #eee; }
        h1 { color: #00d4ff; }
        h2 { color: #ff6b6b; margin-top: 30px; }
        .card { background: #16213e; border-radius: 10px; padding: 20px; margin: 15px 0; }
        label { display: block; margin: 10px 0 5px; color: #aaa; font-size: 0.9em; }
        input, select { width: 100%; padding: 12px; border: 1px solid #333; border-radius: 5px;
                        background: #0f0f23; color: #fff; box-sizing: border-box; font-size: 16px; }
        input:focus { border-color: #00d4ff; outline: none; }
        button { background: #00d4ff; color: #000; padding: 15px 30px; border: none;
                 border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 20px; width: 100%; }
        button:hover { background: #00a8cc; }
        button.danger { background: #ff4757; color: #fff; }
        button.danger:hover { background: #ff6b81; }
        .status { padding: 15px; border-radius: 5px; margin: 10px 0; }
        .status.ok { background: #2ed573; color: #000; }
        .status.error { background: #ff4757; }
        .status.warn { background: #ffa502; color: #000; }
        .info { color: #888; font-size: 0.85em; margin-top: 5px; }
        a { color: #00d4ff; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
        @media (max-width: 500px) { .grid { grid-template-columns: 1fr; } }
    </style>
</head>
<body>
)rawliteral";

// Common HTML footer
const char HTML_FOOTER[] PROGMEM = "</body></html>";

#endif // WEB_TEMPLATES_H
