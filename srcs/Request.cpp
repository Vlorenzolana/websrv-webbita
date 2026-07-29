#include "../includes/Request.hpp"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sstream>

Request::Request()
    : _isParsed(false), _headersComplete(false), _isChunkedBody(false),
      _errorCode(0), _parsingState(PARSE_REQUEST_LINE), _contentLength(0),
      _currentChunkSize(0), _maxBodySize(0)
{
}

Request::~Request() {}

std::string Request::_trim(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string Request::_toLower(const std::string& value)
{
    std::string result(value);
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] = static_cast<char>(result[i] - 'A' + 'a');
    }
    return result;
}

bool Request::_parseDecimalSize(const std::string& value, std::size_t& result)
{
    if (value.empty())
        return false;
    std::size_t parsed = 0;
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] < '0' || value[i] > '9')
            return false;
        const std::size_t digit = static_cast<std::size_t>(value[i] - '0');
        if (parsed > (static_cast<std::size_t>(-1) - digit) / 10)
            return false;
        parsed = parsed * 10 + digit;
    }
    result = parsed;
    return true;
}

bool Request::_parseHexSize(const std::string& value, std::size_t& result)
{
    if (value.empty())
        return false;
    std::size_t parsed = 0;
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        unsigned int digit = 0;
        const char c = value[i];
        if (c >= '0' && c <= '9')
            digit = static_cast<unsigned int>(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = static_cast<unsigned int>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            digit = static_cast<unsigned int>(c - 'A' + 10);
        else
            return false;
        if (parsed > (static_cast<std::size_t>(-1) - digit) / 16)
            return false;
        parsed = parsed * 16 + digit;
    }
    result = parsed;
    return true;
}

void Request::_setError(int code)
{
    if (_errorCode == 0)
        _errorCode = code;
    _parsingState = PARSE_COMPLETE;
    _isParsed = true;
}

void Request::_processRequestLine(const std::string& line)
{
    std::stringstream stream(line);
    std::string extra;
    if (!(stream >> _method >> _path >> _httpVersion) || (stream >> extra))
    {
        _setError(400);
        return;
    }

    if (_path.empty() || _path[0] != '/')
    {
        _setError(400);
        return;
    }
    if (_path.size() > 8192)
    {
        _setError(414);
        return;
    }
    if (_method != "GET" && _method != "POST" && _method != "DELETE")
    {
        _setError(405);
        return;
    }
    if (_httpVersion != "HTTP/1.1" && _httpVersion != "HTTP/1.0")
    {
        _setError(505);
        return;
    }
    _extractQueryString();
}

void Request::_processHeaderLine(const std::string& line)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0)
    {
        _setError(400);
        return;
    }

    const std::string key = _toLower(_trim(line.substr(0, colon)));
    const std::string value = _trim(line.substr(colon + 1));
    if (key.empty())
    {
        _setError(400);
        return;
    }

    HeaderMap::iterator existing = _headers.find(key);
    if (existing != _headers.end())
    {
        if (key == "content-length")
        {
            if (existing->second != value)
                _setError(400);
            return;
        }
        existing->second += ", " + value;
    }
    else
        _headers[key] = value;
}

void Request::_finishHeaders()
{
    _headersComplete = true;
    if (_httpVersion == "HTTP/1.1" && getHeaderValue("host").empty())
    {
        _setError(400);
        return;
    }

    const std::string transferEncoding = _toLower(getHeaderValue("transfer-encoding"));
    const std::string contentLength = getHeaderValue("content-length");
    if (!transferEncoding.empty() && !contentLength.empty())
    {
        _setError(400);
        return;
    }

    if (!transferEncoding.empty())
    {
        if (transferEncoding != "chunked")
        {
            _setError(501);
            return;
        }
        _isChunkedBody = true;
        _parsingState = PARSE_CHUNK_SIZE;
        return;
    }

    if (!contentLength.empty())
    {
        if (!_parseDecimalSize(contentLength, _contentLength))
        {
            _setError(400);
            return;
        }
        if (_maxBodySize > 0 && _contentLength > _maxBodySize)
        {
            _setError(413);
            return;
        }
        if (_contentLength == 0)
        {
            _parsingState = PARSE_COMPLETE;
            _isParsed = true;
        }
        else
            _parsingState = PARSE_BODY;
        return;
    }

    _parsingState = PARSE_COMPLETE;
    _isParsed = true;
}

void Request::_extractQueryString()
{
    const std::size_t question = _path.find('?');
    if (question != std::string::npos)
    {
        _queryString = _path.substr(question + 1);
        _path.erase(question);
    }
}

bool Request::_parseLine(std::string& line)
{
    const std::size_t newline = _rawBuffer.find('\n');
    if (newline == std::string::npos)
        return false;
    line = _rawBuffer.substr(0, newline);
    _rawBuffer.erase(0, newline + 1);
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    return true;
}

void Request::_checkBodyLimit(std::size_t additionalBytes)
{
    if (_maxBodySize == 0)
        return;
    if (additionalBytes > _maxBodySize || _body.size() > _maxBodySize - additionalBytes)
        _setError(413);
}

bool Request::_parseFixedBody()
{
    if (_rawBuffer.size() < _contentLength)
        return false;
    _checkBodyLimit(_contentLength);
    if (_errorCode != 0)
        return true;
    _body.assign(_rawBuffer, 0, _contentLength);
    _rawBuffer.erase(0, _contentLength);
    _parsingState = PARSE_COMPLETE;
    _isParsed = true;
    return true;
}

bool Request::_parseChunkedBody()
{
    while (_errorCode == 0 && !_isParsed)
    {
        if (_parsingState == PARSE_CHUNK_SIZE)
        {
            std::string line;
            if (!_parseLine(line))
                return false;
            const std::size_t semicolon = line.find(';');
            const std::string sizeText = _trim(line.substr(0, semicolon));
            if (!_parseHexSize(sizeText, _currentChunkSize))
            {
                _setError(400);
                return true;
            }
            if (_currentChunkSize == 0)
                _parsingState = PARSE_CHUNK_TRAILERS;
            else
            {
                _checkBodyLimit(_currentChunkSize);
                if (_errorCode != 0)
                    return true;
                _parsingState = PARSE_CHUNK_DATA;
            }
        }
        else if (_parsingState == PARSE_CHUNK_DATA)
        {
            if (_rawBuffer.size() < _currentChunkSize)
                return false;
            _body.append(_rawBuffer, 0, _currentChunkSize);
            _rawBuffer.erase(0, _currentChunkSize);
            _parsingState = PARSE_CHUNK_DATA_END;
        }
        else if (_parsingState == PARSE_CHUNK_DATA_END)
        {
            if (_rawBuffer.empty())
                return false;
            if (_rawBuffer[0] == '\r')
            {
                if (_rawBuffer.size() < 2)
                    return false;
                if (_rawBuffer[1] != '\n')
                {
                    _setError(400);
                    return true;
                }
                _rawBuffer.erase(0, 2);
            }
            else if (_rawBuffer[0] == '\n')
                _rawBuffer.erase(0, 1);
            else
            {
                _setError(400);
                return true;
            }
            _parsingState = PARSE_CHUNK_SIZE;
        }
        else if (_parsingState == PARSE_CHUNK_TRAILERS)
        {
            std::string line;
            if (!_parseLine(line))
                return false;
            if (line.empty())
            {
                _parsingState = PARSE_COMPLETE;
                _isParsed = true;
                return true;
            }
            if (line.find(':') == std::string::npos)
            {
                _setError(400);
                return true;
            }
        }
    }
    return _isParsed;
}

bool Request::parse(const std::string& rawData)
{
    if (_isParsed)
        return true;

    _rawBuffer.append(rawData);
    if (!_headersComplete && _rawBuffer.size() > 65536)
    {
        _setError(431);
        return true;
    }

    while (!_isParsed &&
           (_parsingState == PARSE_REQUEST_LINE || _parsingState == PARSE_HEADERS))
    {
        std::string line;
        if (!_parseLine(line))
            return false;

        if (_parsingState == PARSE_REQUEST_LINE)
        {
            if (line.empty())
                continue;
            _processRequestLine(line);
            if (_errorCode == 0)
                _parsingState = PARSE_HEADERS;
        }
        else if (line.empty())
            _finishHeaders();
        else
            _processHeaderLine(line);
    }

    if (_errorCode != 0 || _isParsed)
        return true;
    if (_parsingState == PARSE_BODY)
        return _parseFixedBody();
    return _parseChunkedBody();
}

void Request::setMaxBodySize(std::size_t maxBodySize)
{
    _maxBodySize = maxBodySize;
    if (_errorCode != 0)
        return;
    if (_headersComplete && !_isChunkedBody && _contentLength > _maxBodySize && _maxBodySize > 0)
        _setError(413);
    else if (_body.size() > _maxBodySize && _maxBodySize > 0)
        _setError(413);
}

const std::string& Request::getMethod() const { return _method; }
const std::string& Request::getPath() const { return _path; }
const std::string& Request::getQueryString() const { return _queryString; }
const std::string& Request::getHttpVersion() const { return _httpVersion; }
const Request::HeaderMap& Request::getHeaders() const { return _headers; }
const std::string& Request::getBody() const { return _body; }
int Request::getErrorCode() const { return _errorCode; }
bool Request::isParsed() const { return _isParsed; }
bool Request::headersComplete() const { return _headersComplete; }
bool Request::isChunked() const { return _isChunkedBody; }
std::size_t Request::getDeclaredContentLength() const { return _contentLength; }

std::string Request::getHeaderValue(const std::string& key) const
{
    const HeaderMap::const_iterator it = _headers.find(_toLower(key));
    if (it == _headers.end())
        return "";
    return it->second;
}
