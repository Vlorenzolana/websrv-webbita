<?php
// Simple CGI script for custom web server
$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';
$uri = $_SERVER['REQUEST_URI'] ?? '/cgi-bin/script.php';
$query = $_SERVER['QUERY_STRING'] ?? '';
$server = $_SERVER['SERVER_NAME'] ?? 'localhost';
$port = $_SERVER['SERVER_PORT'] ?? '8080';
$client = $_SERVER['REMOTE_ADDR'] ?? 'unknown';
$timestamp = date('Y-m-d H:i:s');

// Parse GET manually from QUERY_STRING
$get_params = [];
if ($query) {
    parse_str($query, $get_params);
}

// Parse POST manually from body
$post_params = [];
$post_body = file_get_contents('php://input');
if ($method === 'POST' && $post_body) {
    parse_str($post_body, $post_params);
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PHP CGI - Custom Web Server</title>
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
        code {
            background: #e8eaf6;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: monospace;
        }
        form {
            background: #f9fafb;
            padding: 15px;
            border-radius: 8px;
            margin: 10px 0;
        }
        input {
            padding: 8px;
            border: 2px solid #ddd;
            border-radius: 5px;
            margin: 5px;
            font-size: 1em;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            background: linear-gradient(135deg, #667eea, #764ba2);
            color: white;
            border: none;
            padding: 8px 20px;
            border-radius: 5px;
            cursor: pointer;
            font-weight: bold;
            font-size: 1em;
        }
        button:hover {
            opacity: 0.9;
        }
        .php-icon {
            font-size: 3em;
            text-align: center;
            margin: 20px 0;
        }
        .empty {
            color: #999;
            font-style: italic;
            text-align: center;
            padding: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <div class="php-icon">🐘</div>
            <h1>PHP CGI Script</h1>
            <div class="status">
                <span class="status-dot"></span>
                CUSTOM WEB SERVER ACTIVE
            </div>
            <div class="info-box">
                <h3>✓ PHP CGI is Working!</h3>
                <p>This page is generated dynamically by PHP running on your custom web server.</p>
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
                    <td><code><?php echo htmlspecialchars($method); ?></code></td>
                </tr>
                <tr>
                    <td><strong>URI</strong></td>
                    <td><code><?php echo htmlspecialchars($uri); ?></code></td>
                </tr>
                <tr>
                    <td><strong>Query String</strong></td>
                    <td><code><?php echo htmlspecialchars($query ?: '(empty)'); ?></code></td>
                </tr>
                <tr>
                    <td><strong>Server</strong></td>
                    <td><code><?php echo htmlspecialchars($server . ':' . $port); ?></code></td>
                </tr>
                <tr>
                    <td><strong>Client IP</strong></td>
                    <td><code><?php echo htmlspecialchars($client); ?></code></td>
                </tr>
                <tr>
                    <td><strong>Timestamp</strong></td>
                    <td><code><?php echo htmlspecialchars($timestamp); ?></code></td>
                </tr>
                <tr>
                    <td><strong>PHP Version</strong></td>
                    <td><code><?php echo PHP_VERSION; ?></code></td>
                </tr>
            </table>
        </div>

        <div class="card">
            <h2 style="color: #667eea; margin-bottom: 15px;">🧪 Test Forms</h2>
            <form method="get" action="">
                <strong>🔍 Test GET:</strong><br>
                <input name="name" placeholder="Your name" value="">
                <input name="value" placeholder="Some value" value="">
                <button type="submit">Send GET</button>
            </form>
        </div>

        <div class="card">
            <h2 style="color: #667eea; margin-bottom: 15px;">📥 GET Parameters</h2>
            <?php if (!empty($get_params)): ?>
                <table>
                    <tr><th>Key</th><th>Value</th></tr>
                    <?php foreach ($get_params as $key => $value): ?>
                        <tr>
                            <td><strong><?php echo htmlspecialchars($key); ?></strong></td>
                            <td><?php echo htmlspecialchars($value); ?></td>
                        </tr>
                    <?php endforeach; ?>
                </table>
            <?php else: ?>
                <div class="empty">📭 No GET parameters received</div>
            <?php endif; ?>
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
                    <td><code><?php echo htmlspecialchars($_SERVER['GATEWAY_INTERFACE'] ?? 'not set'); ?></code></td>
                </tr>
                <tr>
                    <td><strong>SERVER_PROTOCOL</strong></td>
                    <td><code><?php echo htmlspecialchars($_SERVER['SERVER_PROTOCOL'] ?? 'not set'); ?></code></td>
                </tr>
                <tr>
                    <td><strong>SERVER_SOFTWARE</strong></td>
                    <td><code><?php echo htmlspecialchars($_SERVER['SERVER_SOFTWARE'] ?? 'Custom Web Server'); ?></code></td>
                </tr>
                <tr>
                    <td><strong>SCRIPT_NAME</strong></td>
                    <td><code><?php echo htmlspecialchars($_SERVER['SCRIPT_NAME'] ?? 'not set'); ?></code></td>
                </tr>
            </table>
        </div>

        <div class="card" style="text-align: center; background: linear-gradient(135deg, #f5f7fa 0%, #c3cfe2 100%);">
            <h3 style="color: #667eea;">🚀 Custom Web Server</h3>
            <p style="margin-top: 10px; color: #555;">This PHP CGI script is executed by your custom web server</p>
        </div>
    </div>
</body>
</html>
