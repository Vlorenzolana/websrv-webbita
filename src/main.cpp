#include "../includes/Server.hpp"

int main()
{
    Server server(3000);
    server.init();
    server.run();
    return 0;
}
