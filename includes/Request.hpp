#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <vector>

class Request
{
public:
    typedef std::map<std::string, std::string> HeaderMap;

    // Fases del parser de request: línea inicial, headers, body o finalizado.
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

    // Estado interno del parser incremental.
    ParsingState _parsing_state;
    size_t       _content_length;
    std::string  _raw_buffer;

public:
    Request();
    ~Request();

    // Parser incremental: devuelve true cuando la request queda completa.
    bool parse(const std::string& raw_request);

    // Getters de acceso seguro a los campos ya parseados.
    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getQueryString() const;
    const std::string& getHttpVersion() const;
    const HeaderMap& getHeaders() const;
    const std::string& getBody() const;
    int getErrorCode() const;
    bool isParsed() const;

    // Busca un header concreto por nombre.
    std::string getHeaderValue(const std::string& key) const;

private:
    // Pasos internos del pipeline de parseo.
    void _processRequestLine(const std::string& line);
    void _processHeaderLine(const std::string& line);
    void _extractQueryString();

    // Limpieza básica de whitespace.
    static std::string _trim(const std::string& str);
};

#endif