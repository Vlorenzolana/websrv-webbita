#include "../includes/Request.hpp"
#include <sstream>
#include <iostream>
#include <stdexcept>

Request::Request() : 
    _is_parsed(false),
    _error_code(0),
    _parsing_state(PARSE_REQUEST_LINE),
    _content_length(0)
{
}

Request::~Request()
{
}

// Utility function to remove leading and trailing whitespace characters
std::string Request::_trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Safe look-up tool to fetch header values by key
std::string Request::getHeaderValue(const std::string& key) const
{
    HeaderMap::const_iterator it = _headers.find(key);
    if (it != _headers.end())
    {
        return it->second;
    }
    return "";
}

// Extracts method, path, and HTTP version from the initial Request Line
void Request::_processRequestLine(const std::string& line)
{
    std::stringstream ss(line);
    ss >> _method >> _path >> _http_version;

    // Check if the mandatory three elements were extracted successfully
    if (_method.empty() || _path.empty() || _http_version.empty())
    {
        _error_code = 400; // Bad Request
        throw std::runtime_error("Bad Request: Request line malformed.");
    }

    // Basic semantic validation for acceptable HTTP methods within the subject bounds
    if (_method != "GET" && _method != "POST" && _method != "DELETE")
    {
        _error_code = 405; // Method Not Allowed
        throw std::runtime_error("Method Not Allowed: " + _method);
    }

    _extractQueryString();
}

// Splits generic header lines into structured key-value pairs
void Request::_processHeaderLine(const std::string& line)
{
    size_t colon_pos = line.find(":");
    if (colon_pos == std::string::npos)
    {
        _error_code = 400; // Bad Request
        throw std::runtime_error("Bad Request: Header field missing colon delimiter.");
    }

    std::string key = line.substr(0, colon_pos);
    std::string value = _trim(line.substr(colon_pos + 1));

    // Map the header entry into the key-value container
    _headers[key] = value;
}

// Separates the target URL path from raw GET parameters query string
void Request::_extractQueryString()
{
    size_t question_mark_pos = _path.find("?");
    if (question_mark_pos != std::string::npos)
    {
        _query_string = _path.substr(question_mark_pos + 1);
        _path = _path.substr(0, question_mark_pos);
    }
}

// Incremental parsing coordinator managing the network buffer data stream
bool Request::parse(const std::string& raw_request)
{
    _raw_buffer += raw_request;
    try
    {
        while (_parsing_state == PARSE_REQUEST_LINE || _parsing_state == PARSE_HEADERS)
        {
            size_t eol = _raw_buffer.find("\n");
            if (eol == std::string::npos)
                return false;

            std::string line = _raw_buffer.substr(0, eol);
            _raw_buffer.erase(0, eol + 1);

            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);

            if (_parsing_state == PARSE_REQUEST_LINE)
            {
                if (line.empty()) continue;
                _processRequestLine(line);
                _parsing_state = PARSE_HEADERS;
            }
            else if (_parsing_state == PARSE_HEADERS)
            {
                if (line.empty())
                {
                    std::string len_str = getHeaderValue("Content-Length");
                    if (!len_str.empty())
                    {
                        std::stringstream ss(len_str);
                        ss >> _content_length;
                        _parsing_state = PARSE_BODY;
                    }
                    else
                    {
                        if (_method == "POST")
                        {
                            _error_code = 400;
                            throw std::runtime_error("Bad Request: POST method requires Content-Length header.");
                        }

                        _parsing_state = PARSE_COMPLETE;
                        _is_parsed = true;
                        return true;
                    }
                }
                else
                    _processHeaderLine(line);

            }
        }

        if (_parsing_state == PARSE_BODY)
        {
            if (_raw_buffer.size() >= _content_length)
            {
                _body = _raw_buffer.substr(0, _content_length);
                _raw_buffer.erase(0, _content_length);
                                
                _parsing_state = PARSE_COMPLETE;
                _is_parsed = true;
                return true;
            }
        }
    }
    catch (const std::exception& e)
    {
        _parsing_state = PARSE_COMPLETE;
        _is_parsed = true;
        if (_error_code == 0) 
            _error_code = 400;

        std::cerr << "[REQUEST ERROR] " << e.what() << std::endl;
        return true; 
    }
    return (_parsing_state == PARSE_COMPLETE);
}

// Getters
const std::string& Request::getMethod() const { return _method; }
const std::string& Request::getPath() const { return _path; }
const std::string& Request::getQueryString() const { return _query_string; }
const std::string& Request::getHttpVersion() const { return _http_version; }
const Request::HeaderMap& Request::getHeaders() const { return _headers; }
const std::string& Request::getBody() const { return _body; }
int Request::getErrorCode() const { return _error_code; }
bool Request::isParsed() const { return _is_parsed; }