#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html\r\n\r\n")
print("<html><body>")
print("<h1>Python CGI Script Test</h1>")
print("<p>This script is running on the web server!</p>")
print("<h2>Environment Variables:</h2>")
print("<ul>")
print(f"<li>REQUEST_METHOD: {os.environ.get('REQUEST_METHOD', 'N/A')}</li>")
print(f"<li>PATH_INFO: {os.environ.get('PATH_INFO', 'N/A')}</li>")
print(f"<li>QUERY_STRING: {os.environ.get('QUERY_STRING', 'N/A')}</li>")
print(f"<li>SERVER_PROTOCOL: {os.environ.get('SERVER_PROTOCOL', 'N/A')}</li>")
print("</ul>")
print("<p>Script executed successfully!</p>")
print("</body></html>")
