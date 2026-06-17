#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <vector>

class Request
{
public:
    typedef std::map<std::string, std::string> HeaderMap;

    // Internal states describing the progress of the network parsing engine
    enum ParsingState
    {
        PARSE_REQUEST_LINE,
        PARSE_HEADERS,
        PARSE_BODY,
        PARSE_COMPLETE
    };

private:
    std::string _method;
    std::string _path;
    std::string _query_string;
    std::string _http_version;
    HeaderMap   _headers;
    std::string _body;
    bool        _is_parsed;
    int         _error_code;

    // Internal machinery tracking network state progression metrics
    ParsingState _parsing_state;
    size_t       _content_length;
    std::string  _raw_buffer;

public:
    Request();
    ~Request();

    // Core dynamic parsing driver: returns true only upon full packet compilation
    bool parse(const std::string& raw_request);

    // Structural read-only getters matching partner API interfaces
    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getQueryString() const;
    const std::string& getHttpVersion() const;
    const HeaderMap& getHeaders() const;
    const std::string& getBody() const;
    int getErrorCode() const;
    bool isParsed() const;

    // Safe direct map evaluation lookup utility
    std::string getHeaderValue(const std::string& key) const;

private:
    // Internal parsing pipeline worker units
    void _processRequestLine(const std::string& line);
    void _processHeaderLine(const std::string& line);
    void _extractQueryString();

    // Static cleaning utility
    static std::string _trim(const std::string& str);
};

#endif