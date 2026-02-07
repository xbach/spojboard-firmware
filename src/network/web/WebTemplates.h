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
        * { box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 0;
            background: #000;
            color: #f5f5f5;
        }

        /* Header */
        .header {
            position: sticky;
            top: 0;
            background: #000;
            z-index: 100;
            border-bottom: 1px solid #333;
        }
        .header-top {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 16px 20px 8px;
        }
        .header-title h1 {
            color: #67e8f9;
            margin: 0;
            font-size: 24px;
        }
        .header-subtitle {
            color: #666;
            font-size: 12px;
            margin: 4px 0 0;
        }
        .action-bar {
            display: flex;
            gap: 8px;
        }
        .action-btn {
            background: transparent;
            border: 1px solid #333;
            border-radius: 3px;
            color: #999;
            width: 36px;
            height: 36px;
            cursor: pointer;
            font-size: 16px;
            transition: all 150ms;
            padding: 0;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .action-btn:hover {
            border-color: #67e8f9;
            color: #67e8f9;
        }
        .action-btn.active {
            background: #67e8f920;
            border-color: #67e8f9;
            color: #67e8f9;
        }

        /* Tabs */
        .tabs {
            display: flex;
            border-bottom: 1px solid #333;
            background: #000;
        }
        .tab {
            flex: 1;
            padding: 16px 8px;
            background: transparent;
            border: none;
            color: #999;
            cursor: pointer;
            font-size: 14px;
            transition: all 150ms;
            text-align: center;
        }
        .tab:hover {
            color: #f5f5f5;
        }
        .tab.active {
            background: #1a1a1a;
            color: #67e8f9;
            border-bottom: 2px solid #67e8f9;
        }
        .tab-icon {
            display: block;
            font-size: 18px;
            margin-bottom: 4px;
        }
        .tab-label {
            display: block;
            font-size: 12px;
        }

        /* Content */
        .content {
            padding: 20px;
        }
        .tab-content {
            display: none;
            padding: 20px;
        }
        .tab-content.active {
            display: block;
            animation: fadeIn 200ms;
        }
        @keyframes fadeIn {
            from { opacity: 0; }
            to { opacity: 1; }
        }

        /* Status Banner */
        .banner {
            margin: 0 20px 20px;
            padding: 12px 16px;
            border-radius: 4px;
            display: flex;
            align-items: center;
            gap: 12px;
            font-size: 14px;
        }
        .banner-warn {
            background: #fcd34d20;
            color: #fcd34d;
        }
        .banner-error {
            background: #fb718520;
            color: #fb7185;
        }
        .banner-success {
            background: #86efac20;
            color: #86efac;
        }
        .banner-info {
            background: #67e8f920;
            color: #67e8f9;
        }
        .banner-warning {
            background: #c084fc20;
            color: #c084fc;
        }
        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: currentColor;
            flex-shrink: 0;
        }

        /* Cards */
        .card {
            background: #1a1a1a;
            border-radius: 8px;
            padding: 20px;
            margin: 16px 0;
        }

        /* Forms */
        .form-group {
            margin-top: 32px;
        }
        .form-group:first-child {
            margin-top: 0;
        }
        .form-group-title {
            font-size: 16px;
            font-weight: bold;
            color: #f5f5f5;
            margin-bottom: 16px;
        }
        label {
            display: block;
            margin: 16px 0 6px;
            color: #999;
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        input, select, textarea {
            width: 100%;
            padding: 12px;
            border: 1px solid #333;
            border-radius: 3px;
            background: #0a0a0a;
            color: #f5f5f5;
            font-size: 14px;
        }
        input:focus, select:focus, textarea:focus {
            border-color: #67e8f9;
            outline: none;
        }
        input::placeholder {
            color: #555;
        }
        input[type="checkbox"] {
            width: auto;
            margin-right: 8px;
        }
        input[type="range"] {
            height: 6px;
            background: #333;
            border-radius: 3px;
        }
        .help-text {
            color: #666;
            font-size: 11px;
            margin-top: 4px;
        }

        /* Grid */
        .grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 16px;
        }

        /* Buttons */
        button, .btn, .btn-primary {
            background: #67e8f9;
            color: #000;
            padding: 14px 24px;
            border: none;
            border-radius: 0;
            cursor: pointer;
            font-size: 14px;
            font-weight: bold;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            width: 100%;
            margin-top: 20px;
            transition: background 150ms;
        }
        button:hover, .btn:hover, .btn-primary:hover {
            background: #7dd3fc;
        }
        button:disabled, .btn:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        button.danger, .btn-danger {
            background: #fb7185;
        }
        button.danger:hover, .btn-danger:hover {
            background: #fc8c9e;
        }
        button.secondary, .btn-secondary {
            background: transparent;
            border: 1px dashed #333;
            color: #67e8f9;
            width: auto;
            padding: 10px 16px;
            margin-top: 0;
            margin-right: 8px;
        }
        button.secondary:hover, .btn-secondary:hover {
            border-style: solid;
            border-color: #67e8f9;
        }
        button.warning, .btn-warning {
            background: #fcd34d;
            color: #000;
        }
        button.warning:hover, .btn-warning:hover {
            background: #fde047;
        }
        .form-actions {
            margin-top: 32px;
            padding: 20px;
            border-top: 1px solid #333;
            background: #0a0a0a;
        }

        /* Tables */
        table {
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 16px;
        }
        thead {
            background: #0a0a0a;
        }
        th {
            text-align: left;
            padding: 12px 8px;
            font-size: 11px;
            color: #999;
            text-transform: uppercase;
            font-weight: bold;
            border-bottom: 2px solid #333;
        }
        th.center {
            text-align: center;
        }
        td {
            padding: 8px;
            border-bottom: 1px solid #222;
        }
        td.center {
            text-align: center;
        }
        tr:hover {
            background: #0a0a0a;
        }
        .delete-btn {
            background: #fb7185;
            color: #000;
            border: none;
            width: 28px;
            height: 28px;
            border-radius: 2px;
            cursor: pointer;
            font-size: 16px;
            font-weight: bold;
            padding: 0;
            margin: 0;
        }
        .delete-btn:hover {
            background: #fc8c9e;
        }

        /* System Info */
        .info-row {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid #222;
        }
        .info-row:last-child {
            border-bottom: none;
        }
        .info-label {
            color: #999;
            font-size: 12px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        .info-value {
            color: #f5f5f5;
            font-size: 14px;
            font-family: 'Courier New', monospace;
        }

        /* Responsive */
        @media (max-width: 768px) {
            body { padding: 0; }
            .header-top {
                flex-direction: column;
                align-items: flex-start;
                gap: 12px;
            }
            .action-bar {
                align-self: center;
            }
            .tab-label {
                display: none;
            }
            .tab-icon {
                margin-bottom: 0;
            }
            .grid {
                grid-template-columns: 1fr;
            }
            .content, .tab-content {
                padding: 16px;
            }
        }
        @media (max-width: 400px) {
            .content, .tab-content {
                padding: 12px;
            }
            .action-btn {
                width: 32px;
                height: 32px;
                font-size: 14px;
            }
        }

        /* Utility */
        .hidden { display: none !important; }
    </style>
</head>
<body>
)rawliteral";

// Common HTML footer
const char HTML_FOOTER[] PROGMEM = R"rawliteral(
</body>
</html>
)rawliteral";

#endif // WEB_TEMPLATES_H
