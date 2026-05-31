#!/bin/bash

# Print HTTP headers
echo "Content-Type: text/html; charset=utf-8"
echo ""

# Get CGI environment variables
METHOD="${REQUEST_METHOD:-GET}"
URI="${REQUEST_URI:-/cgi-bin/test.sh}"
QUERY="${QUERY_STRING:-}"
SERVER="${SERVER_NAME:-localhost}"
PORT="${SERVER_PORT:-8080}"
CLIENT="${REMOTE_ADDR:-unknown}"
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

# HTML output
cat << EOF
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Bash CGI - Custom Web Server</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            padding: 20px;
            color: #333;
        }
        .container {
            max-width: 900px;
            margin: 0 auto;
        }
        .card {
            background: white;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        h1 {
            color: #667eea;
            text-align: center;
            margin-bottom: 10px;
            font-size: 2em;
        }
        .status {
            text-align: center;
            background: linear-gradient(135deg, #667eea, #764ba2);
            color: white;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            font-weight: bold;
            font-size: 1.1em;
        }
        .status-dot {
            display: inline-block;
            width: 10px;
            height: 10px;
            background: #00ff00;
            border-radius: 50%;
            margin-right: 8px;
            animation: blink 1s infinite;
        }
        @keyframes blink {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.3; }
        }
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th, td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        th {
            background: #667eea;
            color: white;
            font-weight: bold;
        }
        tr:hover {
            background: #f5f5f5;
        }
        .info-box {
            background: #f0f4ff;
            border-left: 4px solid #667eea;
            padding: 15px;
            margin: 15px 0;
            border-radius: 5px;
        }
        .info-box h3 {
            color: #667eea;
            margin-bottom: 10px;
        }
        code {
            background: #e8eaf6;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: monospace;
        }
        .bash-icon {
            font-size: 3em;
            text-align: center;
            margin: 20px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <div class="bash-icon">🐚</div>
            <h1>Bash CGI Script</h1>
            <div class="status">
                <span class="status-dot"></span>
                CUSTOM WEB SERVER ACTIVE
            </div>
            <div class="info-box">
                <h3>✓ Bash CGI is Working!</h3>
                <p>This page is generated dynamically by a Bash script running on your custom web server.</p>
            </div>
        </div>

        <div class="card">
            <h2 style="color: #667eea; margin-bottom: 15px;">📡 Request Information</h2>
            <table>
                <tr>
                    <th>Property</th>
                    <th>Value</th>
                </tr>
                <tr>
                    <td><strong>Method</strong></td>
                    <td><code>$METHOD</code></td>
                </tr>
                <tr>
                    <td><strong>URI</strong></td>
                    <td><code>$URI</code></td>
                </tr>
                <tr>
                    <td><strong>Query String</strong></td>
                    <td><code>${QUERY:-'(empty)'}</code></td>
                </tr>
                <tr>
                    <td><strong>Server</strong></td>
                    <td><code>$SERVER:$PORT</code></td>
                </tr>
                <tr>
                    <td><strong>Client IP</strong></td>
                    <td><code>$CLIENT</code></td>
                </tr>
                <tr>
                    <td><strong>Timestamp</strong></td>
                    <td><code>$TIMESTAMP</code></td>
                </tr>
            </table>
        </div>

        <div class="card">
            <h2 style="color: #667eea; margin-bottom: 15px;">⚙️ CGI Environment</h2>
            <table>
                <tr>
                    <th>Variable</th>
                    <th>Value</th>
                </tr>
                <tr>
                    <td><strong>GATEWAY_INTERFACE</strong></td>
                    <td><code>${GATEWAY_INTERFACE:-'not set'}</code></td>
                </tr>
                <tr>
                    <td><strong>SERVER_PROTOCOL</strong></td>
                    <td><code>${SERVER_PROTOCOL:-'not set'}</code></td>
                </tr>
                <tr>
                    <td><strong>SERVER_SOFTWARE</strong></td>
                    <td><code>${SERVER_SOFTWARE:-'Custom Web Server'}</code></td>
                </tr>
                <tr>
                    <td><strong>SCRIPT_NAME</strong></td>
                    <td><code>${SCRIPT_NAME:-'not set'}</code></td>
                </tr>
            </table>
        </div>

        <div class="card">
            <h2 style="color: #667eea; margin-bottom: 15px;">🔧 Bash Capabilities</h2>
            <div class="info-box">
                <h3>What Bash CGI can do:</h3>
                <p>✓ Process environment variables</p>
                <p>✓ Generate dynamic HTML content</p>
                <p>✓ Execute system commands</p>
                <p>✓ Handle HTTP requests</p>
            </div>
            <div style="background: #1e1e1e; color: #00ff00; padding: 15px; border-radius: 5px; font-family: monospace; margin-top: 15px;">
                <div>$ echo "Hello from Bash CGI!"</div>
                <div style="margin-top: 5px;">Hello from Bash CGI!</div>
                <div style="margin-top: 10px;">$ date</div>
                <div style="margin-top: 5px;">$TIMESTAMP</div>
            </div>
        </div>

        <div class="card" style="text-align: center; background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);">
            <h3 style="color: #667eea;">🚀 Custom Web Server</h3>
            <p style="margin-top: 10px; color: #555;">This Bash CGI script is executed by your custom web server</p>
        </div>
    </div>
</body>
</html>
EOF
