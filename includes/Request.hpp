#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <cstddef>
#include <map>
#include <string>

class Request
{
public:
    typedef std::map<std::string, std::string> HeaderMap;

    // HTTP request parsing state machine
    enum ParsingState
    {
        PARSE_REQUEST_LINE,
        PARSE_HEADERS,
        PARSE_BODY,
        PARSE_CHUNK_SIZE,
        PARSE_CHUNK_DATA,
        PARSE_CHUNK_DATA_END,
        PARSE_CHUNK_TRAILERS,
        PARSE_COMPLETE
    };

    // Orthodox Canonical Form
    Request();
    ~Request();

    // Core parsing and configuration methods
    bool parse(const std::string& rawData);
    void setMaxBodySize(std::size_t maxBodySize);

    // Getters for request metadata
    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getQueryString() const;
    const std::string& getHttpVersion() const;
    const HeaderMap& getHeaders() const;
    const std::string& getBody() const;

    // State inspection getters
    int getErrorCode() const;
    bool isParsed() const;
    bool headersComplete() const;
    bool isChunked() const;
    std::size_t getDeclaredContentLength() const;
    std::string getHeaderValue(const std::string& key) const;

private:
    std::string _method;
    std::string _path;
    std::string _queryString;
    std::string _httpVersion;
    HeaderMap _headers;
    std::string _body;

    bool _isParsed;
    bool _headersComplete;
    bool _isChunkedBody;
    int _errorCode;
    ParsingState _parsingState;

    std::size_t _contentLength;
    std::size_t _currentChunkSize;
    std::size_t _maxBodySize;
    std::string _rawBuffer;

    // Parsing helpers
    static std::string _trim(const std::string& value);
    static std::string _toLower(const std::string& value);
    static bool _parseDecimalSize(const std::string& value, std::size_t& result);
    static bool _parseHexSize(const std::string& value, std::size_t& result);

    // Internal parsing stages
    void _setError(int code);
    void _processRequestLine(const std::string& line);
    void _processHeaderLine(const std::string& line);
    void _finishHeaders();
    void _extractQueryString();
    bool _parseLine(std::string& line);
    bool _parseFixedBody();
    bool _parseChunkedBody();
    void _checkBodyLimit(std::size_t additionalBytes);
};

#endif