#!/usr/bin/env python3
import os
import sys
import json
from datetime import datetime
from urllib.parse import parse_qs

# Get CGI environment information
method = os.environ.get('REQUEST_METHOD', 'GET')
query_string = os.environ.get('QUERY_STRING', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')
server_name = os.environ.get('SERVER_NAME', 'localhost')
server_port = os.environ.get('SERVER_PORT', '8080')
remote_addr = os.environ.get('REMOTE_ADDR', 'unknown')
request_uri = os.environ.get('REQUEST_URI', '/cgi-bin/test_python.py')
server_protocol = os.environ.get('SERVER_PROTOCOL', 'HTTP/1.1')
script_name = os.environ.get('SCRIPT_NAME', '/cgi-bin/test_python.py')
server_software = os.environ.get('SERVER_SOFTWARE', 'Custom Web Server')

# Parse GET parameters
get_params = parse_qs(query_string)

# Read POST data
post_data = {}
post_raw = ""
if method == 'POST' and content_length.isdigit() and int(content_length) > 0:
    post_raw = sys.stdin.read(int(content_length))
    try:
        post_data = parse_qs(post_raw)
    except:
        pass

# Get headers
headers = {}
for key, value in os.environ.items():
    if key.startswith('HTTP_'):
        header_name = key[5:].replace('_', '-').title()
        headers[header_name] = value

now = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

# Helper functions
def html_escape(text):
    return str(text).replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

def render_params_table(params):
    if not params:
        return '<div class="empty-state">📭 No parameters received</div>'
    
    html = '<table><tr><th>Key</th><th>Value</th></tr>'
    for key, values in params.items():
        value = ', '.join(values) if isinstance(values, list) else str(values)
        html += f'<tr><td><strong>{html_escape(key)}</strong></td><td>{html_escape(value)}</td></tr>'
    html += '</table>'
    return html

def render_headers_table(headers):
    if not headers:
        return '<div class="empty-state">📭 No headers detected</div>'
    
    html = '<table><tr><th>Header</th><th>Value</th></tr>'
    for key, value in sorted(headers.items()):
        html += f'<tr><td><strong>{html_escape(key)}</strong></td><td>{html_escape(value)}</td></tr>'
    html += '</table>'
    return html

# Print HTTP headers
print("Content-Type: text/html; charset=utf-8")
print()  # Blank line to separate headers from body

# HTML content
print(f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>🐍 Python CGI - Custom Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * {{ margin: 0; padding: 0; box-sizing: border-box; }}
    body {{
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #0ea5e9 0%, #2563eb 100%);
      min-height: 100vh;
      padding: 2rem;
      line-height: 1.6;
    }}
    .container {{
      max-width: 1400px;
      margin: 0 auto;
      animation: fadeIn 0.5s ease-in;
    }}
    @keyframes fadeIn {{
      from {{ opacity: 0; transform: translateY(-20px); }}
      to {{ opacity: 1; transform: translateY(0); }}
    }}
    @keyframes pulse {{
      0%, 100% {{ transform: scale(1); }}
      50% {{ transform: scale(1.05); }}
    }}
    @keyframes slideIn {{
      from {{ opacity: 0; transform: translateX(-30px); }}
      to {{ opacity: 1; transform: translateX(0); }}
    }}
    @keyframes rotate {{
      from {{ transform: rotate(0deg); }}
      to {{ transform: rotate(360deg); }}
    }}
    header {{
      background: white;
      border-radius: 16px;
      padding: 2rem;
      margin-bottom: 2rem;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      text-align: center;
      position: relative;
      overflow: hidden;
    }}
    header::before {{
      content: '';
      position: absolute;
      top: 0;
      left: -100%;
      width: 100%;
      height: 4px;
      background: linear-gradient(90deg, transparent, #0ea5e9, transparent);
      animation: scan 3s infinite;
    }}
    @keyframes scan {{
      0% {{ left: -100%; }}
      100% {{ left: 100%; }}
    }}
    h1 {{
      font-size: 2.5rem;
      color: #333;
      margin-bottom: 0.5rem;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 1rem;
    }}
    .python-logo {{
      display: inline-block;
      animation: rotate 10s linear infinite;
      font-size: 3rem;
    }}
    .server-badge {{
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      background: linear-gradient(135deg, #0ea5e9, #2563eb);
      color: white;
      padding: 0.75rem 1.5rem;
      border-radius: 50px;
      font-size: 1rem;
      font-weight: bold;
      margin-top: 1rem;
      animation: pulse 2s infinite;
      box-shadow: 0 4px 15px rgba(14, 165, 233, 0.4);
    }}
    .status-indicator {{
      width: 12px;
      height: 12px;
      background: #0f0;
      border-radius: 50%;
      display: inline-block;
      box-shadow: 0 0 10px #0f0;
      animation: blink 1s infinite;
    }}
    @keyframes blink {{
      0%, 100% {{ opacity: 1; }}
      50% {{ opacity: 0.3; }}
    }}
    .cgi-info {{
      background: rgba(255,255,255,0.95);
      padding: 1rem 2rem;
      border-radius: 12px;
      display: inline-block;
      margin-top: 1rem;
      border-left: 4px solid #0ea5e9;
    }}
    .cgi-info strong {{
      color: #0ea5e9;
      font-size: 1.1rem;
    }}
    .connection-indicator {{
      display: flex;
      align-items: center;
      gap: 0.5rem;
      justify-content: center;
      margin-top: 1rem;
      font-weight: 600;
      color: #10b981;
    }}
    .grid {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      gap: 1.5rem;
      margin-bottom: 1.5rem;
    }}
    .card {{
      background: white;
      border-radius: 12px;
      padding: 1.5rem;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
      transition: transform 0.3s ease, box-shadow 0.3s ease;
      animation: slideIn 0.5s ease-out;
    }}
    .card:hover {{
      transform: translateY(-5px);
      box-shadow: 0 15px 40px rgba(0,0,0,0.3);
    }}
    .card h2 {{
      color: #0ea5e9;
      margin-bottom: 1rem;
      font-size: 1.5rem;
      display: flex;
      align-items: center;
      gap: 0.5rem;
      border-bottom: 2px solid #0ea5e9;
      padding-bottom: 0.5rem;
    }}
    table {{
      width: 100%;
      border-collapse: collapse;
      margin-top: 1rem;
    }}
    th, td {{
      padding: 0.75rem;
      text-align: left;
      border-bottom: 1px solid #e5e7eb;
    }}
    th {{
      background: linear-gradient(135deg, #0ea5e9, #2563eb);
      color: white;
      font-weight: 600;
      text-transform: uppercase;
      font-size: 0.85rem;
      letter-spacing: 0.5px;
    }}
    tr:hover {{
      background: #f9fafb;
    }}
    .method-badge {{
      display: inline-block;
      padding: 0.25rem 0.75rem;
      border-radius: 20px;
      font-weight: bold;
      font-size: 0.9rem;
    }}
    .method-GET {{ background: #10b981; color: white; }}
    .method-POST {{ background: #f59e0b; color: white; }}
    .method-DELETE {{ background: #ef4444; color: white; }}
    .method-PUT {{ background: #3b82f6; color: white; }}
    pre {{
      background: #1e293b;
      color: #22d3ee;
      padding: 1rem;
      border-radius: 8px;
      overflow-x: auto;
      font-family: 'Courier New', monospace;
      margin-top: 0.5rem;
      border: 2px solid #334155;
      white-space: pre-wrap;
      word-wrap: break-word;
    }}
    .empty-state {{
      color: #9ca3af;
      font-style: italic;
      text-align: center;
      padding: 2rem;
      background: #f9fafb;
      border-radius: 8px;
      border: 2px dashed #d1d5db;
    }}
    .python-feature {{
      background: #eff6ff;
      border-left: 4px solid #0ea5e9;
      padding: 1rem;
      border-radius: 8px;
      margin: 1rem 0;
    }}
    .python-feature h3 {{
      color: #0ea5e9;
      margin-bottom: 0.5rem;
    }}
    footer {{
      background: white;
      border-radius: 12px;
      padding: 1.5rem;
      margin-top: 2rem;
      text-align: center;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
    }}
    .tech-stack {{
      display: flex;
      gap: 1rem;
      justify-content: center;
      flex-wrap: wrap;
      margin-top: 1rem;
    }}
    .tech-badge {{
      background: #f3f4f6;
      padding: 0.5rem 1rem;
      border-radius: 20px;
      font-size: 0.9rem;
      font-weight: 600;
      color: #0ea5e9;
      border: 2px solid #0ea5e9;
    }}
    form {{
      background: #f9fafb;
      padding: 1rem;
      border-radius: 8px;
      margin-top: 1rem;
      border: 2px solid #e5e7eb;
    }}
    input {{
      padding: 0.5rem;
      border: 2px solid #d1d5db;
      border-radius: 6px;
      margin: 0.5rem;
      font-size: 1rem;
      transition: border-color 0.3s;
    }}
    input:focus {{
      outline: none;
      border-color: #0ea5e9;
    }}
    button {{
      background: linear-gradient(135deg, #0ea5e9, #2563eb);
      color: white;
      border: none;
      padding: 0.5rem 1.5rem;
      border-radius: 6px;
      cursor: pointer;
      font-weight: bold;
      transition: transform 0.2s, box-shadow 0.2s;
      font-size: 1rem;
    }}
    button:hover {{
      transform: scale(1.05);
      box-shadow: 0 5px 15px rgba(14, 165, 233, 0.4);
    }}
    button:active {{
      transform: scale(0.98);
    }}
    .stats-grid {{
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 1rem;
      margin-top: 1rem;
    }}
    .stat-box {{
      background: linear-gradient(135deg, #eff6ff, #dbeafe);
      padding: 1rem;
      border-radius: 8px;
      text-align: center;
      border: 2px solid #0ea5e9;
    }}
    .stat-value {{
      font-size: 1.5rem;
      font-weight: bold;
      color: #0ea5e9;
    }}
    .stat-label {{
      font-size: 0.85rem;
      color: #64748b;
      margin-top: 0.25rem;
    }}
  </style>
</head>
<body>
<div class="container">
<header>
  <h1><span class="python-logo">🐍</span> Python CGI Script</h1>
  <div class="server-badge">
    <span class="status-indicator"></span>
    CUSTOM WEB SERVER ACTIVE
  </div>
  <div class="cgi-info">
    <strong>CGI Interface:</strong> Processing dynamic requests with Python 3<br>
    <small>Script executed on custom web server</small>
  </div>
  <div class="connection-indicator">
    ⚡ Connection established with custom server
  </div>
</header>

<section class="python-feature">
  <h3>🎯 Python CGI Capabilities</h3>
  <p><strong>✓</strong> Form data processing (GET/POST methods)</p>
  <p><strong>✓</strong> Environment variables manipulation</p>
  <p><strong>✓</strong> Dynamic HTML generation</p>
  <p><strong>✓</strong> Full integration with custom web server</p>
</section>

<section class="card">
  <h2>📊 Request Statistics</h2>
  <div class="stats-grid">
    <div class="stat-box">
      <div class="stat-value">{len(get_params)}</div>
      <div class="stat-label">GET Parameters</div>
    </div>
    <div class="stat-box">
      <div class="stat-value">{len(post_data)}</div>
      <div class="stat-label">POST Parameters</div>
    </div>
    <div class="stat-box">
      <div class="stat-value">{len(headers)}</div>
      <div class="stat-label">HTTP Headers</div>
    </div>
    <div class="stat-box">
      <div class="stat-value">{content_length}</div>
      <div class="stat-label">Content Length</div>
    </div>
  </div>
</section>

<section class="grid">
  <div class="card">
    <h2>📡 Request Information</h2>
    <table>
      <tr><th>Property</th><th>Value</th></tr>
      <tr><td><strong>Method</strong></td><td><span class="method-badge method-{method}">{method}</span></td></tr>
      <tr><td><strong>URI</strong></td><td><code>{html_escape(request_uri)}</code></td></tr>
      <tr><td><strong>Query String</strong></td><td><code>{html_escape(query_string or '(empty)')}</code></td></tr>
      <tr><td><strong>Protocol</strong></td><td>{html_escape(server_protocol)}</td></tr>
      <tr><td><strong>Host</strong></td><td>{html_escape(server_name)}:{html_escape(server_port)}</td></tr>
      <tr><td><strong>Client IP</strong></td><td>{html_escape(remote_addr)}</td></tr>
      <tr><td><strong>Timestamp</strong></td><td>{html_escape(now)}</td></tr>
      <tr><td><strong>Python Version</strong></td><td>{html_escape(sys.version.split()[0])}</td></tr>
    </table>
  </div>

  <div class="card">
    <h2>🧪 Interactive Testing</h2>
    <form method="get" action="">
      <label><strong>🔍 Test GET Method:</strong></label><br>
      <input name="test_param" placeholder="Test parameter" value="">
      <input name="value" placeholder="Value" value="">
      <button type="submit">Send GET</button>
    </form>
    <form method="post" action="" style="margin-top:1rem">
      <label><strong>📝 Test POST Method:</strong></label><br>
      <input name="python_data" placeholder="Python data" value="">
      <input name="message" placeholder="Message" value="">
      <button type="submit">Send POST</button>
    </form>
    <div style="margin-top:1rem; padding:1rem; background:#dbeafe; border-radius:8px; border-left:4px solid #0ea5e9;">
      <small><strong>💡 Tip:</strong> Use these forms to test how CGI processes different HTTP methods</small>
    </div>
  </div>
</section>

<section class="card">
  <h2>📥 GET Parameters Received</h2>
  {render_params_table(get_params)}
</section>

<section class="card">
  <h2>📤 POST Parameters Received</h2>
  {render_params_table(post_data)}
  {'<h3 style="margin-top:1rem; color:#0ea5e9;">📄 Raw POST Body</h3><pre>' + html_escape(post_raw) + '</pre>' if post_raw and method == 'POST' else ''}
</section>

<section class="card">
  <h2>📋 HTTP Headers</h2>
  {render_headers_table(headers)}
</section>

<section class="card">
  <h2>⚙️ CGI Environment Variables</h2>
  <table>
    <tr><th>Variable</th><th>Value</th></tr>""")

# Important CGI variables
cgi_vars = [
    'GATEWAY_INTERFACE', 'SERVER_PROTOCOL', 'SERVER_NAME', 'SERVER_PORT',
    'REQUEST_METHOD', 'SCRIPT_FILENAME', 'SCRIPT_NAME', 'PATH_INFO',
    'QUERY_STRING', 'REMOTE_ADDR', 'SERVER_SOFTWARE', 'CONTENT_TYPE',
    'CONTENT_LENGTH', 'HTTP_USER_AGENT', 'HTTP_ACCEPT'
]

for var in cgi_vars:
    value = os.environ.get(var, '(not defined)')
    print(f"    <tr><td><strong>{html_escape(var)}</strong></td><td>{html_escape(value)}</td></tr>")

print(f"""  </table>
</section>

<section class="card">
  <h2>🐍 Python System Information</h2>
  <div class="python-feature">
    <h3>📚 Imported Libraries</h3>
    <pre>import os                      # Environment variables
import sys                     # System information
import json                    # Data processing
from datetime import datetime  # Date/time handling
from urllib.parse import parse_qs  # URL parsing</pre>
  </div>
  <div class="python-feature">
    <h3>💻 System Details</h3>
    <table>
      <tr><th>Property</th><th>Value</th></tr>
      <tr><td><strong>Python Version</strong></td><td>{html_escape(sys.version)}</td></tr>
      <tr><td><strong>Platform</strong></td><td>{html_escape(sys.platform)}</td></tr>
      <tr><td><strong>Script Path</strong></td><td>{html_escape(os.path.abspath(__file__))}</td></tr>
      <tr><td><strong>Current Directory</strong></td><td>{html_escape(os.getcwd())}</td></tr>
      <tr><td><strong>Process ID</strong></td><td>{html_escape(str(os.getpid()))}</td></tr>
    </table>
  </div>
</section>

<footer>
  <h3 style="color:#0ea5e9; margin-bottom:1rem;">🚀 Custom Web Server</h3>
  <p style="color:#6b7280; margin-bottom:1rem;">This Python CGI script is being executed by your custom web server</p>
  <div class="tech-stack">
    <span class="tech-badge">Python {html_escape(sys.version.split()[0])}</span>
    <span class="tech-badge">CGI/1.1</span>
    <span class="tech-badge">HTTP/1.1</span>
    <span class="tech-badge">Custom Web Server</span>
  </div>
  <p style="margin-top:1.5rem; color:#9ca3af; font-size:0.9rem;">
    ⚡ Dynamically processed with Python • {html_escape(now)}
  </p>
</footer>
</div>
</body>
</html>""")